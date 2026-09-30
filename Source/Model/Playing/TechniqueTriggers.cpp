#include "TechniqueTriggers.h"

namespace luthier
{

//==============================================================================
void TechniqueTriggers::configure (TechniqueId technique, const TechniqueTriggerConfig& config) noexcept
{
    const auto t = (size_t) juce::jlimit (0, kTechniques - 1, (int) technique);
    const bool wasArmed = configs[t].armed;

    configs[t] = config;
    configs[t].triggerCc = juce::jlimit (-1, 127, config.triggerCc);
    configs[t].auxCc = juce::jlimit (-1, 127, config.auxCc);
    configs[t].continuousCc = juce::jlimit (-1, 127, config.continuousCc);
    configs[t].zoneChannel = juce::jlimit (1, 16, config.zoneChannel);

    // Disarming lets go of everything that technique was holding, so nothing
    // stays held down behind a switch that is off.
    if (wasArmed && ! config.armed)
    {
        held[t].fill (false);
        triggerCcDown[t] = auxCcDown[t] = false;
        captured[t].fill (false);
    }
}

const TechniqueTriggerConfig& TechniqueTriggers::getConfig (TechniqueId technique) const noexcept
{
    return configs[(size_t) juce::jlimit (0, kTechniques - 1, (int) technique)];
}

void TechniqueTriggers::reset() noexcept
{
    for (auto& h : held)
        h.fill (false);

    triggerCcDown.fill (false);
    auxCcDown.fill (false);

    for (auto& c : captured)
        c.fill (false);

    for (auto& r : requests)
        r.store (0);

    numEvents = 0;
}

void TechniqueTriggers::request (TechniqueId technique, int role, bool on) noexcept
{
    if (! juce::isPositiveAndBelow (role, kMaxRoles))
        return;

    const auto t = (size_t) juce::jlimit (0, kTechniques - 1, (int) technique);
    requests[t].fetch_or (1u << (role + (on ? 0 : 8)));
}

bool TechniqueTriggers::isHeld (TechniqueId technique, int role) const noexcept
{
    if (! juce::isPositiveAndBelow (role, kMaxRoles))
        return false;

    return held[(size_t) juce::jlimit (0, kTechniques - 1, (int) technique)][(size_t) role];
}

void TechniqueTriggers::push (const GestureEvent& e) noexcept
{
    if (juce::isPositiveAndBelow (e.role, kMaxRoles))
        held[(size_t) e.technique][(size_t) e.role] = e.on;

    if (numEvents < kMaxEvents)
    {
        events[(size_t) numEvents++] = e;
        return;
    }

    // Full: a controller keeps its latest value rather than its first.
    if (e.role == kContinuousRole && events[(size_t) numEvents - 1].role == kContinuousRole
        && events[(size_t) numEvents - 1].technique == e.technique)
        events[(size_t) numEvents - 1] = e;
}

//==============================================================================
bool TechniqueTriggers::takes (const juce::uint8* d, int n) const noexcept
{
    // Only notes are ever taken: a CC is harmless to pass on, a note would sound.
    if (n < 3 || d[0] >= 0xf0)
        return false;

    const int type = d[0] & 0xf0;

    if (type != 0x90 && type != 0x80)
        return false;

    const int channel = (d[0] & 0x0f) + 1;

    for (const auto& c : configs)
    {
        if (! c.armed)
            continue;

        for (size_t role = 0; role < c.keyswitches.size(); ++role)
            if (c.keyswitches[role] >= 0 && d[1] == c.keyswitches[role]
                && (role > 0 || c.source == TriggerSource::keyswitch))
                return true;

        if (c.source == TriggerSource::mpeZone && c.consumeZoneNotes && channel == c.zoneChannel)
            return true;
    }

    return capturingTechnique (d, n) >= 0;
}

int TechniqueTriggers::capturingTechnique (const juce::uint8* d, int n) const noexcept
{
    // TECHNIQUES (two-hand-tapping.md 3): a note captured under a held keyswitch.
    if (n < 3 || d[0] >= 0xf0 || ((d[0] & 0xf0) != 0x90 && (d[0] & 0xf0) != 0x80))
        return -1;

    const int index = (d[0] & 0x0f) * 128 + (d[1] & 0x7f);

    for (int t = 0; t < kTechniques; ++t)
        if (configs[(size_t) t].armed && captured[(size_t) t][(size_t) index])
            return t;

    return -1;
}

const juce::MidiBuffer& TechniqueTriggers::process (const juce::MidiBuffer& in, juce::MidiBuffer& filtered) noexcept
{
    numEvents = 0;

    // ---- the buttons, at the top of the block --------------------------------------
    for (int t = 0; t < kTechniques; ++t)
    {
        const juce::uint32 bits = requests[(size_t) t].exchange (0);

        if (bits == 0 || ! configs[(size_t) t].armed)
            continue;

        for (int role = 0; role < kMaxRoles; ++role)
        {
            if ((bits & (1u << role)) != 0)
                push ({ (TechniqueId) t, role, true, 0, 1.0, -1 });

            if ((bits & (1u << (role + 8))) != 0)
                push ({ (TechniqueId) t, role, false, 0, 0.0, -1 });
        }
    }

    bool anyArmed = false;

    for (const auto& c : configs)
        anyArmed = anyArmed || c.armed;

    // Idle: one pass over a handful of flags.
    if (! anyArmed)
        return in;

    bool anyTaken = false;

    for (const auto m : in)
    {
        const auto* d = m.data;
        const int n = m.numBytes;

        if (n < 2 || d[0] >= 0xf0)
            continue;

        const int type = d[0] & 0xf0;
        const int channel = (d[0] & 0x0f) + 1;
        const int offset = juce::jmax (0, m.samplePosition);
        const bool noteOn = type == 0x90 && n >= 3 && d[2] > 0;
        const bool noteOff = (type == 0x80 || (type == 0x90 && d[2] == 0)) && n >= 3;

        anyTaken = anyTaken || takes (d, n);

        for (int t = 0; t < kTechniques; ++t)
        {
            const auto& c = configs[(size_t) t];

            if (! c.armed)
                continue;

            const auto technique = (TechniqueId) t;

            if (noteOn || noteOff)
            {
                const double velocity = noteOn ? d[2] / 127.0 : 0.0;

                // TECHNIQUES: notes under a held keyswitch belong to the technique.
                if (c.captureNotesWhileHeld && c.source == TriggerSource::keyswitch)
                {
                    bool isKeyswitch = false;

                    for (const auto& other : configs)
                        for (auto ks : other.keyswitches)
                            isKeyswitch = isKeyswitch || (other.armed && ks >= 0 && d[1] == ks);

                    const auto index = (size_t) ((channel - 1) * 128 + (d[1] & 0x7f));

                    if (! isKeyswitch && noteOn && held[(size_t) t][0])
                    {
                        captured[(size_t) t][index] = true;
                        push ({ technique, kCaptureRole, true, offset, velocity, d[1] });
                        anyTaken = true;
                        continue;
                    }

                    if (! isKeyswitch && noteOff && captured[(size_t) t][index])
                    {
                        push ({ technique, kCaptureRole, false, offset, 0.0, d[1] });
                        anyTaken = true;
                        continue;
                    }
                }

                for (int role = 0; role < (int) c.keyswitches.size(); ++role)
                    if (c.keyswitches[(size_t) role] >= 0 && d[1] == c.keyswitches[(size_t) role]
                        && (role > 0 || c.source == TriggerSource::keyswitch))
                        push ({ technique, role, noteOn, offset, velocity, d[1] });

                if (c.source == TriggerSource::mpeZone && channel == c.zoneChannel)
                    push ({ technique, 0, noteOn, offset, velocity, d[1] });
            }
            else if (type == 0xb0 && n >= 3)
            {
                const int cc = d[1];
                const double value = d[2] / 127.0;
                const bool down = d[2] >= 64;

                // A trigger CC is a switch: on at 64 and up, off below, edges only.
                if (c.source == TriggerSource::controller && cc == c.triggerCc && down != triggerCcDown[(size_t) t])
                {
                    triggerCcDown[(size_t) t] = down;
                    push ({ technique, 0, down, offset, value, cc });
                }

                if (cc == c.auxCc && down != auxCcDown[(size_t) t])
                {
                    auxCcDown[(size_t) t] = down;
                    push ({ technique, 1, down, offset, value, cc });
                }

                if (cc == c.continuousCc)
                    push ({ technique, kContinuousRole, true, offset, value, cc });
            }
            else if ((type == 0xd0 || type == 0xa0) && c.continuousAftertouch)
            {
                const int raw = type == 0xd0 ? d[1] : (n >= 3 ? d[2] : 0);
                push ({ technique, kContinuousRole, true, offset, raw / 127.0, -1 });
            }
        }
    }

    if (! anyTaken)
        return in;

    filtered.clear();

    for (const auto m : in)
        if (! takes (m.data, m.numBytes))
            filtered.addEvent (m.data, m.numBytes, m.samplePosition);

    // A captured note's note-off has now been taken; let it go.
    for (const auto m : in)
    {
        const auto* d = m.data;

        if (m.numBytes >= 3 && d[0] < 0xf0
            && ((d[0] & 0xf0) == 0x80 || ((d[0] & 0xf0) == 0x90 && d[2] == 0)))
            for (auto& c : captured)
                c[(size_t) ((d[0] & 0x0f) * 128 + (d[1] & 0x7f))] = false;
    }

    return filtered;
}

} // namespace luthier
