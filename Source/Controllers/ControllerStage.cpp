#include "ControllerStage.h"

namespace luthier
{

//==============================================================================
ControllerRtSettings ControllerRtSettings::fromProfile (const ControllerProfile& profile) noexcept
{
    ControllerRtSettings s;
    s.mode = profile.mode;

    for (int i = 0; i < kMaxStrings; ++i)
    {
        s.channelForString[(size_t) i] = profile.perString[(size_t) i].channel;
        s.stringBendSemis[(size_t) i] = profile.perString[(size_t) i].pitchBendSemis;
    }

    s.bendSemis = (profile.mode == ControllerMode::mpe) ? profile.memberPitchBendSemis
                                                        : profile.pitchBendSemis;
    s.ccMap = profile.ccMap;
    s.pitchDeadZoneCents = profile.pitchDeadZoneCents;
    s.minimumNoteDurationMs = profile.minimumNoteDurationMs;
    s.mpeMasterChannel = profile.mpeMasterChannel;
    s.latencyMs = juce::jmax (0.0, profile.getEffectiveLatencyMs());

    // CT-10: the curve, resampled into a fixed table the interpreter can copy.
    if (profile.pitchCurve.size() >= 2)
    {
        s.numPitchCurvePoints = MidiInterpreter::kPitchCurvePoints;

        for (int i = 0; i < s.numPitchCurvePoints; ++i)
            s.pitchCurve[(size_t) i] = (float) profile.applyPitchCurve ((double) i / (double) (s.numPitchCurvePoints - 1));
    }

    return s;
}

void ControllerRtSettings::applyTo (MidiInterpreter& interpreter) const noexcept
{
    switch (mode)
    {
        case ControllerMode::mpe:
            interpreter.setMpeEnabled (true);
            interpreter.setPlayingMode (PlayingMode::GuitarController);
            interpreter.setPitchBendRange (bendSemis);
            interpreter.resetChannelMap();
            break;

        case ControllerMode::perChannel:
            interpreter.setMpeEnabled (false);
            interpreter.setPlayingMode (PlayingMode::GuitarController);
            interpreter.setPitchBendRange (bendSemis);

            for (int s = 0; s < kMaxStrings; ++s)
            {
                interpreter.setChannelForString (s, channelForString[(size_t) s]);

                if (channelForString[(size_t) s] > 0)
                    interpreter.setStringBendRange (s, stringBendSemis[(size_t) s]);
            }
            break;

        case ControllerMode::standard:
        case ControllerMode::numModes:
        default:
            interpreter.setMpeEnabled (false);
            interpreter.setPitchBendRange (bendSemis);
            interpreter.resetChannelMap();

            // A standard controller says nothing about how the player wants to
            // play, so the playing mode is deliberately left alone here.
            break;
    }

    // The CC map is replaced wholesale rather than merged, so that switching
    // profiles cannot leave a mapping behind from the previous one.
    interpreter.resetCcMapToDefaults();

    for (int number = 0; number < 128; ++number)
        if (ccMap[(size_t) number] != MidiTarget::None)
            interpreter.setCcTarget (number, ccMap[(size_t) number]);

    interpreter.setPitchDeadZoneCents (pitchDeadZoneCents);
    interpreter.setMinimumNoteDurationMs (minimumNoteDurationMs);
    interpreter.setMpeMasterChannel (mode == ControllerMode::mpe ? mpeMasterChannel : 0);   // CT-17
    interpreter.setPitchCurve (numPitchCurvePoints >= 2 ? pitchCurve.data() : nullptr,
                               numPitchCurvePoints);                                           // CT-10
}

//==============================================================================
ControllerStage::ControllerStage()
{
    settings.resetAll (ControllerRtSettings {});
}

void ControllerStage::setProfile (const ControllerProfile& profile)
{
    const juce::ScopedLock sl (writeLock);

    profileId = profile.id;
    const auto rt = ControllerRtSettings::fromProfile (profile);
    latencyMs.store (rt.latencyMs, std::memory_order_relaxed);
    settings.write (rt);
    publishedGeneration.fetch_add (1, std::memory_order_release);
}

juce::String ControllerStage::getProfileId() const
{
    const juce::ScopedLock sl (writeLock);
    return profileId;
}

void ControllerStage::applyPending (MidiInterpreter& interpreter) noexcept
{
    const auto generation = publishedGeneration.load (std::memory_order_acquire);

    if (generation == appliedGeneration)
        return;

    appliedGeneration = generation;
    settings.acquire().applyTo (interpreter);
}

void ControllerStage::compensateLatency (juce::MidiBuffer& midi, int numSamples, double sampleRate,
                                         juce::MidiBuffer& scratch) const noexcept
{
    if (! compensate.load (std::memory_order_relaxed) || midi.isEmpty())
        return;

    const int shift = (int) std::lround (getLatencyMs() * 0.001 * juce::jmax (1.0, sampleRate));

    if (shift <= 0)
        return;

    // Every event moves by the same amount, so a sustain pedal or a bend keeps
    // its place relative to the notes it shapes; ties keep their order.
    scratch.clear();

    for (const auto metadata : midi)
        scratch.addEvent (metadata.data, metadata.numBytes,
                          juce::jlimit (0, juce::jmax (0, numSamples - 1), metadata.samplePosition - shift));

    midi.clear();

    for (const auto metadata : scratch)
        midi.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);

    scratch.clear();
}

} // namespace luthier
