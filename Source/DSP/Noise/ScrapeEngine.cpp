#include "ScrapeEngine.h"

namespace luthier
{

//==============================================================================
namespace
{
    int lowStrings (int count, int numStrings) noexcept
    {
        // String 0 is the high E, so "the low three" are the last three.
        int mask = 0;

        for (int s = juce::jmax (0, numStrings - count); s < numStrings; ++s)
            mask |= 1 << s;

        return mask;
    }

    /*  How much a bend stretches the winding (5, microtonal bends). Tension
        goes as the square of the frequency ratio; a guitar string at pitch is
        strained about 0.3 %, so doubling the tension adds about that again. */
    double bendSpacingFactor (double bendCents) noexcept
    {
        if (bendCents == 0.0)
            return 1.0;

        const double r = centsToRatio (bendCents);
        return 1.0 / (1.0 + 0.003 * (r * r - 1.0));
    }
}

//==============================================================================
ScrapeSettings ScrapeSettings::fromPreset (ScrapePreset preset, int numStrings) noexcept
{
    ScrapeSettings s;
    s.armed = true;
    numStrings = juce::jlimit (1, kMaxStrings, numStrings);

    switch (preset)
    {
        case ScrapePreset::classicRock:
            s.tool = ScrapeTool::pick;
            s.pressure = 0.5;
            s.direction = ScrapeDirection::bridgeToNut;
            s.durationMs = 600.0;
            s.stringMask = lowStrings (3, numStrings);
            break;

        case ScrapePreset::metalZipper:
            s.tool = ScrapeTool::pick;
            s.pressure = 0.85;
            s.direction = ScrapeDirection::bridgeToNut;
            s.durationMs = 300.0;
            s.stringMask = lowStrings (4, numStrings);
            break;

        case ScrapePreset::slowRatchet:
            s.tool = ScrapeTool::pick;
            s.pressure = 0.25;
            s.durationMs = 2000.0;
            s.stringMask = lowStrings (1, numStrings);
            break;

        case ScrapePreset::nailScrape:
            s.tool = ScrapeTool::nail;
            s.pressure = 0.3;
            s.durationMs = 800.0;
            s.stringMask = lowStrings (3, numStrings);
            break;

        case ScrapePreset::modwheelSweep:
            s.direction = ScrapeDirection::holdAndSweep;
            s.sweepSource = ScrapeSweepSource::modWheel;
            break;

        case ScrapePreset::numPresets:
        default:
            break;
    }

    return s;
}

const char* ScrapeSettings::getPresetName (ScrapePreset preset) noexcept
{
    switch (preset)
    {
        case ScrapePreset::classicRock:   return "Classic Rock Scrape";
        case ScrapePreset::metalZipper:   return "Metal Zipper";
        case ScrapePreset::slowRatchet:   return "Slow Ratchet";
        case ScrapePreset::nailScrape:    return "Nail Scrape";
        case ScrapePreset::modwheelSweep: return "Modwheel-Sweep";
        case ScrapePreset::numPresets:
        default:                          return "";
    }
}

//==============================================================================
ScrapeEngine::ScrapeEngine()
{
    for (auto& p : uiPosition)
        p.store (-1.0f);
}

void ScrapeEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = juce::jmax (8000.0, sampleRate);
    maxBlock = juce::jmax (1, maxBlockSize);

    // The comb reaches back at most one period of the lowest string there is.
    const int longest = (int) std::ceil (sr / constants::kMinStringHz) + 4;
    const int ringSize = juce::nextPowerOfTwo (longest);
    ringMask = ringSize - 1;

    for (auto& r : rings)
        r.assign ((size_t) ringSize, 0.0);

    for (auto& e : excitation)
        e.assign ((size_t) maxBlock, 0.0);

    noiseOut.assign ((size_t) maxBlock, 0.0);

    // A 7-bit controller moves in 3-4 mm steps over a scrape's range: 4 ms of
    // smoothing turns each step into a run of catches rather than a burst.
    sweepCoefficient = std::exp (-1.0 / (0.004 * sr));

    reset();
}

void ScrapeEngine::reset() noexcept
{
    for (auto& v : voices)
        v = Voice();

    activeVoices = 0;

    for (auto& r : rings)
        std::fill (r.begin(), r.end(), 0.0);

    ringWrite = 0;

    for (auto& e : excitation)
        std::fill (e.begin(), e.end(), 0.0);

    std::fill (noiseOut.begin(), noiseOut.end(), 0.0);
    renderedThisBlock = false;

    loadCents.fill (0.0);
    loadSettling = false;
    sweepValue = sweepSmoothed = 0.0;
    numEvents = 0;
    numPendingTriggers = 0;
    uiRequests.store (0);
    rakeWanted = false;
    triggerCcDown = false;

    clock = 0;
    lastTriggerAt = std::numeric_limits<juce::int64>::min() / 2;
    droppedTriggers = 0;
    catchTotals.fill (0);
    numRecords = 0;

    for (auto& p : uiPosition)
        p.store (-1.0f, std::memory_order_relaxed);
}

void ScrapeEngine::setSettings (const ScrapeSettings& s) noexcept
{
    settings = s;
    settings.pressure = juce::jlimit (0.0, 1.0, s.pressure);
    settings.durationMs = juce::jlimit (10.0, 10000.0, s.durationMs);
    settings.angleDegrees = juce::jlimit (-60.0, 60.0, s.angleDegrees);
    settings.retriggerMs = juce::jmax (0.0, s.retriggerMs);
    settings.triggerCc = juce::jlimit (0, 127, s.triggerCc);
    settings.sweepCc = juce::jlimit (0, 127, s.sweepCc);
    settings.stringMask = s.stringMask & ((1 << kMaxStrings) - 1);
    settings.level = juce::jlimit (0.0, 16.0, s.level);
}

void ScrapeEngine::setString (int stringIndex, const StringNoiseInfo& info, double frequencyHz,
                              double fret, double bendCents) noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    auto& st = strings[(size_t) stringIndex];
    st.info = info;
    st.hz = std::isfinite (frequencyHz) ? juce::jmax (constants::kMinStringHz, frequencyHz) : 110.0;
    st.fret = std::isfinite (fret) ? juce::jlimit (0.0, 36.0, fret) : 0.0;
    st.bendCents = std::isfinite (bendCents) ? juce::jlimit (-2400.0, 2400.0, bendCents) : 0.0;
}

void ScrapeEngine::setStringBlocked (int stringIndex, bool blocked) noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    voices[(size_t) stringIndex].blocked = blocked;

    if (blocked)
        preempt (stringIndex);
}

//==============================================================================
double ScrapeEngine::toolLevel (ScrapeTool t) noexcept
{
    // Flesh barely catches a winding; a nail catches, but rounder than a pick's edge.
    switch (t)
    {
        case ScrapeTool::nail:  return 0.70;
        case ScrapeTool::thumb: return 0.35;
        case ScrapeTool::pick:
        case ScrapeTool::numTools:
        default:                return 1.0;
    }
}

double ScrapeEngine::toolWidthMs (ScrapeTool t) noexcept
{
    // How long the tool takes to slip off one winding: the catch's width.
    switch (t)
    {
        case ScrapeTool::nail:  return 0.09;
        case ScrapeTool::thumb: return 0.30;
        case ScrapeTool::pick:
        case ScrapeTool::numTools:
        default:                return 0.05;
    }
}

double ScrapeEngine::angleFactor (double degrees) noexcept
{
    // Tilted toward the travel the edge rides over the windings; against it,
    // the edge digs in.
    const double a = juce::degreesToRadians (juce::jlimit (-60.0, 60.0, degrees));
    return 1.0 - 0.5 * std::sin (a);
}

double ScrapeEngine::windingsPerMm (int stringIndex) const noexcept
{
    const auto& st = strings[(size_t) stringIndex];

    if (! st.info.wound || st.info.windingPitchPerMm <= 0.0)
        return 0.0;

    return st.info.windingPitchPerMm * bendSpacingFactor (st.bendCents);
}

double ScrapeEngine::vibratingLengthMm (int stringIndex) const noexcept
{
    return scaleLengthMm * std::pow (2.0, -strings[(size_t) stringIndex].fret / 12.0);
}

double ScrapeEngine::clampPosition (double mm) const noexcept
{
    return juce::jlimit (kEdgeMm, scaleLengthMm - kEdgeMm, mm);
}

double ScrapeEngine::catchLevelFor (const ScrapeGesture& g, int stringIndex) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return 0.0;

    const auto& info = strings[(size_t) stringIndex].info;
    const double depth = info.wound ? info.windingDepth : 0.0;

    // 1: amplitude proportional to pressure. Everything else is a fixed
    // property of the tool, the winding and the hand, and the level trim.
    return kCatchReference * settings.level * juce::jlimit (0.0, 1.0, g.pressure) * depth * toolLevel (g.tool)
             * angleFactor (g.angleDegrees) * (1.0 - 0.35 * muteAmount);
}

int ScrapeEngine::effectiveMask() const noexcept
{
    if (settings.stringMask != 0)
        return settings.stringMask;

    // 2's default: the wound strings, however many the set has.
    int mask = 0;

    for (int s = 0; s < numStrings; ++s)
        if (strings[(size_t) s].info.wound)
            mask |= 1 << s;

    return mask;
}

ScrapeGesture ScrapeEngine::gestureFor (int stringIndex) const noexcept
{
    ScrapeGesture g;
    g.stringIndex = stringIndex;
    g.durationMs = settings.durationMs;
    g.pressure = settings.pressure;
    g.tool = settings.tool;
    g.angleDegrees = settings.angleDegrees;

    const double nearMm = juce::jmin (settings.startPositionMm, settings.endPositionMm);
    const double farMm = juce::jmax (settings.startPositionMm, settings.endPositionMm);
    const bool towardBridge = settings.direction == ScrapeDirection::nutToBridge;

    g.startPositionMm = towardBridge ? farMm : nearMm;
    g.endPositionMm = towardBridge ? nearMm : farMm;
    g.controlled = settings.direction == ScrapeDirection::holdAndSweep
                     || settings.sweepSource != ScrapeSweepSource::automatic;
    return g;
}

double ScrapeEngine::controlledTarget (const Voice& v) const noexcept
{
    const double span = v.farMm - v.nearMm;
    return v.reversed ? v.farMm - sweepSmoothed * span : v.nearMm + sweepSmoothed * span;
}

ScrapeSweepSource ScrapeEngine::effectiveSweepSource() const noexcept
{
    // 2: hold-and-sweep is "a modwheel-style hold and manual sweep", so with
    // nothing else chosen the mod wheel is what sweeps.
    if (settings.sweepSource == ScrapeSweepSource::automatic
        && settings.direction == ScrapeDirection::holdAndSweep)
        return ScrapeSweepSource::modWheel;

    return settings.sweepSource;
}

bool ScrapeEngine::ownsVibratoControllers() const noexcept
{
    const auto source = effectiveSweepSource();

    if (source != ScrapeSweepSource::modWheel && source != ScrapeSweepSource::aftertouch)
        return false;

    for (int s = 0; s < numStrings; ++s)
        if (voices[(size_t) s].moving && voices[(size_t) s].controlled)
            return true;

    return false;
}

//==============================================================================
void ScrapeEngine::pushEvent (const Event& e) noexcept
{
    if (numEvents < kMaxEventsPerBlock)
    {
        events[(size_t) numEvents++] = e;
        return;
    }

    // Full: a controller sweep keeps its latest value rather than its first.
    if (e.type == Event::Type::sweep && events[(size_t) numEvents - 1].type == Event::Type::sweep)
        events[(size_t) numEvents - 1] = e;
}

void ScrapeEngine::trigger (const ScrapeGesture& gesture, int sampleOffset) noexcept
{
    if (numPendingTriggers >= kMaxStrings)
        return;

    pendingGestures[(size_t) numPendingTriggers] = gesture;
    pendingOffsets[(size_t) numPendingTriggers] = juce::jmax (0, sampleOffset);
    ++numPendingTriggers;
}

void ScrapeEngine::triggerFromSettings (int sampleOffset) noexcept
{
    pushEvent ({ Event::Type::triggerOn, juce::jmax (0, sampleOffset), 1.0 });
}

void ScrapeEngine::releaseControlled (int sampleOffset) noexcept
{
    pushEvent ({ Event::Type::triggerOff, juce::jmax (0, sampleOffset), 0.0 });
}

void ScrapeEngine::setSweepValue (double value, int sampleOffset) noexcept
{
    pushEvent ({ Event::Type::sweep, juce::jmax (0, sampleOffset), juce::jlimit (0.0, 1.0, value) });
}

void ScrapeEngine::preempt (int stringIndex) noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    auto& v = voices[(size_t) stringIndex];

    // What was queued behind it goes too: the string belongs to something else now.
    v.hasQueued = false;

    if (v.active && v.fadeStep == 0.0)
        v.fadeStep = 1.0 / juce::jmax (1.0, kPreemptFadeSeconds * sr);
}

void ScrapeEngine::stopAll() noexcept
{
    for (int s = 0; s < kMaxStrings; ++s)
    {
        voices[(size_t) s] = Voice();
        uiPosition[(size_t) s].store (-1.0f, std::memory_order_relaxed);
    }

    activeVoices = 0;
    numPendingTriggers = 0;
    numEvents = 0;
    loadCents.fill (0.0);
    loadSettling = false;
}

bool ScrapeEngine::takeRake (bool& downward, double& seconds) noexcept
{
    if (! rakeWanted)
        return false;

    rakeWanted = false;
    downward = rakeDownward;
    seconds = juce::jlimit (0.1, 3.0, settings.durationMs * 0.001);   // pick-noise.md 5: 100 ms - 3 s
    return true;
}

//==============================================================================
const juce::MidiBuffer& ScrapeEngine::handleMidi (const juce::MidiBuffer& in, juce::MidiBuffer& filtered) noexcept
{
    if (! settings.armed)
        return in;

    const auto trigger = settings.trigger;
    const auto source = effectiveSweepSource();

    // Raw bytes, not MidiMessage: a SysEx in the buffer would allocate.
    auto consumed = [trigger] (const juce::uint8* d, int n) noexcept
    {
        if (n < 2 || d[0] >= 0xf0)
            return false;

        const int type = d[0] & 0xf0;
        const int channel = (d[0] & 0x0f) + 1;
        const bool noteMessage = (type == 0x90 || type == 0x80) && n >= 3;

        switch (trigger)
        {
            case ScrapeTriggerSource::keyswitch:
                return noteMessage && (d[1] == kScrapeKeyswitch || d[1] == kRakeDownKeyswitch
                                         || d[1] == kRakeUpKeyswitch);

            case ScrapeTriggerSource::mpeZone:
                return noteMessage && channel == kZoneChannel;

            case ScrapeTriggerSource::controller:
            case ScrapeTriggerSource::buttonOnly:
            case ScrapeTriggerSource::numSources:
            default:
                return false;
        }
    };

    bool anyConsumed = false;

    for (const auto m : in)
    {
        const auto* d = m.data;
        const int n = m.numBytes;

        if (n < 2 || d[0] >= 0xf0)
            continue;

        const int type = d[0] & 0xf0;
        const int offset = juce::jmax (0, m.samplePosition);
        const bool noteOn = type == 0x90 && n >= 3 && d[2] > 0;
        const bool noteOff = (type == 0x80 || (type == 0x90 && d[2] == 0)) && n >= 3;

        // ---- triggers -----------------------------------------------------------
        if (consumed (d, n))
        {
            anyConsumed = true;

            if (trigger == ScrapeTriggerSource::keyswitch && d[1] != kScrapeKeyswitch)
            {
                if (noteOn)
                    pushEvent ({ d[1] == kRakeDownKeyswitch ? Event::Type::rakeDown : Event::Type::rakeUp, offset, 0.0 });
            }
            else if (noteOn)
            {
                pushEvent ({ Event::Type::triggerOn, offset, 1.0 });
            }
            else if (noteOff)
            {
                pushEvent ({ Event::Type::triggerOff, offset, 0.0 });
            }

            continue;
        }

        if (type == 0xb0 && n >= 3)
        {
            const int cc = d[1];
            const double value = d[2] / 127.0;

            // The trigger CC is a switch: on at 64 and up, off below, edges only.
            if (trigger == ScrapeTriggerSource::controller && cc == settings.triggerCc)
            {
                const bool down = d[2] >= 64;

                if (down != triggerCcDown)
                    pushEvent ({ down ? Event::Type::triggerOn : Event::Type::triggerOff, offset, value });

                triggerCcDown = down;
            }

            const bool sweep = (source == ScrapeSweepSource::modWheel && cc == 1)
                            || (source == ScrapeSweepSource::expression && cc == 4)
                            || (source == ScrapeSweepSource::customCc && cc == settings.sweepCc);

            if (sweep)
                pushEvent ({ Event::Type::sweep, offset, value });
        }
        else if ((type == 0xd0 || type == 0xa0) && source == ScrapeSweepSource::aftertouch)
        {
            const int raw = type == 0xd0 ? d[1] : (n >= 3 ? d[2] : 0);
            pushEvent ({ Event::Type::sweep, offset, raw / 127.0 });
        }
    }

    if (! anyConsumed)
        return in;

    // Everything the scrape did not take goes on, at its own sample. The caller
    // sized `filtered` in prepare, so this does not allocate.
    filtered.clear();

    for (const auto m : in)
        if (! consumed (m.data, m.numBytes))
            filtered.addEvent (m.data, m.numBytes, m.samplePosition);

    return filtered;
}

//==============================================================================
void ScrapeEngine::applyEvent (const Event& e, int sampleOffset) noexcept
{
    switch (e.type)
    {
        case Event::Type::triggerOn:  fireFromSettings (sampleOffset); break;
        case Event::Type::triggerOff:
            for (auto& v : voices)
                if (v.controlled)
                    v.moving = false;   // the pick lifts; its last catch and the comb's tail play out
            break;

        case Event::Type::sweep:      sweepValue = e.value; break;
        case Event::Type::rakeDown:   rakeWanted = true; rakeDownward = true;  break;
        case Event::Type::rakeUp:     rakeWanted = true; rakeDownward = false; break;
        default: break;
    }
}

void ScrapeEngine::fireFromSettings (int offset) noexcept
{
    // 2 and 6: a second trigger inside the threshold is dropped, silently.
    const juce::int64 now = clock + offset;
    const auto threshold = (juce::int64) (settings.retriggerMs * 0.001 * sr);

    if (now - lastTriggerAt < threshold)
    {
        ++droppedTriggers;
        return;
    }

    lastTriggerAt = now;

    const int mask = effectiveMask();

    for (int s = 0; s < numStrings; ++s)
        if ((mask & (1 << s)) != 0)
            startVoice (gestureFor (s), offset);
}

void ScrapeEngine::startVoice (const ScrapeGesture& g, int offset) noexcept
{
    const int s = juce::jlimit (0, numStrings - 1, g.stringIndex);
    auto& v = voices[(size_t) s];

    // technique-cascade.md 3.4: the slide has this string.
    if (v.blocked)
        return;

    // 0.2: nothing to catch on a plain string. Not an event at all.
    const double w = windingsPerMm (s);
    const double level = catchLevelFor (g, s);

    if (w <= 0.0 || level <= 0.0)
        return;

    // Scrape on scrape is "queue" (technique-cascade.md 2): the new one
    // starts when the pick in motion leaves the string. The latest wins the
    // queue's one place.
    if (v.active && v.moving)
    {
        v.hasQueued = true;
        v.queued = g;
        v.queued.stringIndex = s;
        return;
    }

    if (! v.active)
    {
        // The comb only ever reads back what this voice wrote.
        std::fill (rings[(size_t) s].begin(), rings[(size_t) s].end(), 0.0);
        v.pulseIndex = v.pulseLength = 0;
        v.tail = 0;
        v.active = true;
        ++activeVoices;
    }

    v.fade = 1.0;
    v.fadeStep = 0.0;
    v.hasQueued = false;
    v.moving = true;
    v.controlled = g.controlled;
    v.level = level;
    v.pressure = juce::jlimit (0.0, 1.0, g.pressure);
    v.depth = strings[(size_t) s].info.windingDepth;
    v.angleDegrees = g.angleDegrees;

    // Duller and thumpier through a mute (5); a tool tilted with the travel
    // slips off each winding more slowly.
    const double tilt = juce::jmax (0.0, std::sin (juce::degreesToRadians (juce::jlimit (-60.0, 60.0, g.angleDegrees))));
    v.widthMs = toolWidthMs (g.tool) * (1.0 + 1.5 * muteAmount) * (1.0 + 0.5 * tilt);

    v.fromMm = clampPosition (g.startPositionMm);
    v.toMm = clampPosition (g.endPositionMm);
    v.nearMm = juce::jmin (v.fromMm, v.toMm);
    v.farMm = juce::jmax (v.fromMm, v.toMm);
    v.reversed = v.fromMm > v.toMm;

    double durationMs = g.durationMs;

    if (v.controlled)
    {
        // The sweep takes over from wherever the controller already is, unless
        // another string's sweep is using the smoothed value right now.
        bool sweeping = false;

        for (int o = 0; o < numStrings; ++o)
            if (o != s && voices[(size_t) o].moving && voices[(size_t) o].controlled)
                sweeping = true;

        if (! sweeping)
            sweepSmoothed = sweepValue;

        v.positionMm = controlledTarget (v);
        v.samplesLeft = (int) (kMaxHoldSeconds * sr);
        durationMs = kMaxHoldSeconds * 1000.0;
    }
    else
    {
        const int samples = juce::jmax (1, (int) std::round (g.durationMs * 0.001 * sr));
        v.positionMm = v.fromMm;
        v.mmPerSample = (v.toMm - v.fromMm) / samples;
        v.samplesLeft = samples;
    }

    v.winding = (juce::int64) std::floor (v.positionMm * w);

    if (numRecords < kMaxRecordsPerBlock)
        records[(size_t) numRecords++] = { s, offset, (float) level, (float) durationMs };

    firedCount.fetch_add (1, std::memory_order_relaxed);
}

void ScrapeEngine::finishVoice (int stringIndex, int sampleOffset) noexcept
{
    auto& v = voices[(size_t) stringIndex];

    if (v.hasQueued && v.fadeStep == 0.0)
    {
        // The queued scrape starts now, where the last one let go.
        v.hasQueued = false;
        const auto next = v.queued;
        v.moving = false;
        startVoice (next, sampleOffset);

        if (v.moving)
            return;
    }

    const bool blocked = v.blocked;
    v = Voice();
    v.blocked = blocked;
}

//==============================================================================
void ScrapeEngine::renderSample (int s, int i) noexcept
{
    auto& v = voices[(size_t) s];
    const auto& st = strings[(size_t) s];

    const double lengthMm = vibratingLengthMm (s);

    // ---- the pick moves, and catches every winding boundary it crosses ------------
    if (v.moving)
    {
        double next;

        if (v.controlled)
        {
            next = controlledTarget (v);
        }
        else
        {
            next = v.positionMm + v.mmPerSample;

            if (v.samplesLeft <= 1)
                next = v.toMm;
        }

        const double w = windingsPerMm (s);
        const auto k = (juce::int64) std::floor (next * w);

        if (k != v.winding)
        {
            /*  Going up the string the boundary crossed is the new winding's
                lower edge; going down, the old one's. Either way it is the
                same boundary index for the same place on the string, so the
                winding's irregularity is a property of the winding and a
                reversed scrape is the forward one mirrored (6). */
            const juce::int64 boundary = juce::jmax (k, v.winding);
            catchTotals[(size_t) s] += std::abs (k - v.winding);
            v.winding = k;

            // One catch sounds per sample however many were crossed in it; the
            // tool is still slipping off the first.
            const double mmThisSample = juce::jmax (1.0e-9, std::abs (next - v.positionMm));
            const double samplesPerCatch = 1.0 / (mmThisSample * w);
            const double width = juce::jmin (v.widthMs * 0.001 * sr, 0.45 * samplesPerCatch);
            const int half = juce::jmax (1, (int) std::round ((width - 1.0) * 0.5));

            const double irregular = 1.0 + 0.2 * (noiseUniform (seed32 ^ (0x9e3779b9u * (juce::uint32) (s + 1)),
                                                                (juce::uint32) boundary) - 0.5);

            // Past the fretting finger the pick is on string that is not vibrating.
            const double reach = next <= lengthMm ? 1.0 : 0.15;

            v.pulseLength = 2 * half + 1;
            v.pulseIndex = 0;
            v.pulseAmp = v.level * irregular * reach;
        }

        v.positionMm = next;

        if (--v.samplesLeft <= 0)
            v.moving = false;   // arrived, or a held sweep nobody released
    }

    // ---- the catch's impulse ------------------------------------------------------
    double pulse = 0.0;

    if (v.pulseIndex < v.pulseLength)
    {
        const double phase = (double) (v.pulseIndex + 1) / (double) (v.pulseLength + 1);
        pulse = v.pulseAmp * 0.5 * (1.0 - std::cos (constants::kTwoPi * phase));
        ++v.pulseIndex;
    }

    // ---- at the pick's position: the impulse, less itself b of a period later ------
    auto& ring = rings[(size_t) s];
    ring[(size_t) ringWrite] = pulse;

    const double beta = juce::jlimit (0.0, 1.0, v.positionMm / juce::jmax (1.0, lengthMm));
    const double delay = beta * sr / st.hz;
    const double readAt = (double) ringWrite - delay + (double) (ringMask + 1);
    const int i0 = (int) std::floor (readAt);
    const double frac = readAt - (double) i0;
    const double delayed = ring[(size_t) (i0 & ringMask)] * (1.0 - frac)
                         + ring[(size_t) ((i0 + 1) & ringMask)] * frac;

    if (pulse != 0.0)
        v.tail = (int) delay + 2;
    else if (v.tail > 0)
        --v.tail;

    const double gain = v.fade;
    excitation[(size_t) s][(size_t) i] += gain * (pulse - delayed);
    noiseOut[(size_t) i] += gain * pulse;

    // ---- fades and endings --------------------------------------------------------
    if (v.fadeStep > 0.0)
    {
        v.fade -= v.fadeStep;

        if (v.fade <= 0.0)
        {
            // Preempted: gone, with nothing queued behind it.
            v.hasQueued = false;
            v.moving = false;
            v.pulseIndex = v.pulseLength;
            v.tail = 0;
        }
    }

    if (! v.moving && v.pulseIndex >= v.pulseLength && v.tail <= 0)
    {
        finishVoice (s, i);

        if (! voices[(size_t) s].active)
            --activeVoices;
    }
}

void ScrapeEngine::processBlock (int numSamples) noexcept
{
    numRecords = 0;
    renderedThisBlock = false;

    // 3: zero cost when idle - this test, and the clock.
    if (! isBusy())
    {
        clock += numSamples;
        return;
    }

    const int n = juce::jlimit (0, maxBlock, numSamples);

    // ---- the UI's requests, at the top of the block ---------------------------------
    const int requests = uiRequests.exchange (0);

    if ((requests & requestRakeDownBit) != 0) { rakeWanted = true; rakeDownward = true; }
    if ((requests & requestRakeUpBit) != 0)   { rakeWanted = true; rakeDownward = false; }

    // The button is a trigger like the keyswitch: it needs the technique armed.
    if (settings.armed && (requests & requestTriggerBit) != 0)
        fireFromSettings (0);

    if ((requests & requestReleaseBit) != 0)
        applyEvent ({ Event::Type::triggerOff, 0, 0.0 }, 0);

    for (int s = 0; s < numStrings; ++s)
        std::fill (excitation[(size_t) s].begin(), excitation[(size_t) s].begin() + n, 0.0);

    std::fill (noiseOut.begin(), noiseOut.begin() + n, 0.0);

    int nextEvent = 0;
    int pendingLeft = numPendingTriggers;

    for (int i = 0; i < n; ++i)
    {
        // Events land on their own sample; anything past the block's end lands
        // on its last sample rather than being lost.
        while (nextEvent < numEvents
               && (events[(size_t) nextEvent].offset <= i || i == n - 1))
        {
            applyEvent (events[(size_t) nextEvent], i);
            ++nextEvent;
        }

        if (pendingLeft > 0)
        {
            for (int p = 0; p < numPendingTriggers; ++p)
            {
                if (pendingOffsets[(size_t) p] == i || (i == n - 1 && pendingOffsets[(size_t) p] >= i))
                {
                    startVoice (pendingGestures[(size_t) p], i);
                    --pendingLeft;
                }
            }
        }

        sweepSmoothed = sweepValue + (sweepSmoothed - sweepValue) * sweepCoefficient;

        if (activeVoices > 0)
            for (int s = 0; s < numStrings; ++s)
                if (voices[(size_t) s].active)
                    renderSample (s, i);

        ringWrite = (ringWrite + 1) & ringMask;
    }

    numEvents = 0;
    numPendingTriggers = 0;
    renderedThisBlock = true;

    // ---- block-rate: the pick's load on each string (1) --------------------------------
    const double blockCoefficient = 1.0 - std::exp (-(double) n / (0.005 * sr));
    loadSettling = false;

    for (int s = 0; s < kMaxStrings; ++s)
    {
        const auto& v = voices[(size_t) s];
        double target = 0.0;

        if (s < numStrings && v.moving && v.fadeStep == 0.0)
        {
            // Deeper into the winding at higher pressure, and each winding
            // loads it a little differently.
            const double wobble = 1.0 + 0.3 * (noiseUniform (seed32 ^ 0x10adu, (juce::uint32) v.winding) - 0.5);
            target = kLoadCentsAtFullPressure * v.pressure * v.depth * wobble;
        }

        loadCents[(size_t) s] += (target - loadCents[(size_t) s]) * blockCoefficient;

        if (std::abs (loadCents[(size_t) s] - target) < 1.0e-6)
            loadCents[(size_t) s] = target;

        loadSettling = loadSettling || loadCents[(size_t) s] != target;

        uiPosition[(size_t) s].store (v.moving ? (float) v.positionMm : -1.0f, std::memory_order_relaxed);
    }

    clock += numSamples;
}

//==============================================================================
bool ScrapeEngine::isStringActive (int stringIndex) const noexcept
{
    return juce::isPositiveAndBelow (stringIndex, kMaxStrings) && voices[(size_t) stringIndex].active;
}

bool ScrapeEngine::isStringMoving (int stringIndex) const noexcept
{
    return juce::isPositiveAndBelow (stringIndex, kMaxStrings) && voices[(size_t) stringIndex].moving;
}

juce::int64 ScrapeEngine::getCatchCount (int stringIndex) const noexcept
{
    return juce::isPositiveAndBelow (stringIndex, kMaxStrings) ? catchTotals[(size_t) stringIndex] : 0;
}

} // namespace luthier
