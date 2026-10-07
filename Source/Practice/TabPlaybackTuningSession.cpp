#include "TabPlaybackTuningSession.h"

#include <cmath>

namespace luthier
{

double TabPlaybackTuningSession::midiToHz (int midi) noexcept
{
    return 440.0 * std::pow (2.0, ((double) midi - 69.0) / 12.0);
}

void TabPlaybackTuningSession::clearPlaybackOffsets (TuningEngine::StringTuning& t) noexcept
{
    t.detuneCents = 0.0;
    t.realismDetuneCents = 0.0;
    t.driftCents = 0.0;
    t.characterDriftCents = 0.0;
    t.fineTuneCents = 0.0;
    t.stabilityCents = 0.0;
}

bool TabPlaybackTuningSession::begin (const ScoreTrack& track, juce::String* reason)
{
    if (active)
        end();

    auto& tuning = engine.getTuningEngine();
    const int currentStrings = tuning.getNumStrings();
    const int requestedStrings = juce::jlimit (0, kMaxStrings, track.numStrings);

    if (requestedStrings <= 0 || requestedStrings != currentStrings)
    {
        if (reason != nullptr)
            *reason = "Tab string count does not match the current instrument; using the existing adaptation path.";
        return false;
    }

    saved.numStrings = currentStrings;
    saved.capoFret = tuning.getCapoFret();
    saved.capoMask = tuning.getCapoStringMask();

    for (int s = 0; s < saved.numStrings; ++s)
        saved.strings[(size_t) s] = tuning.getStringTuning (s);

    {
        const LuthierEngine::ScopedStructuralChange change (engine);
        tuning.setNumStrings (requestedStrings);
        tuning.setCapoFret (juce::jmax (0, track.capoFret));
        tuning.setCapoStringMask (TuningEngine::kAllStrings);

        for (int s = 0; s < requestedStrings; ++s)
        {
            auto imported = saved.strings[(size_t) s];
            const int midi = track.tuning[(size_t) s];

            if (midi <= 0 || midi > 127)
            {
                if (reason != nullptr)
                    *reason = "Tab contains an invalid open-string tuning.";

                // Restore before returning so a partially-applied import can never leak.
                tuning.setCapoFret (saved.capoFret);
                tuning.setCapoStringMask (saved.capoMask);
                for (int i = 0; i < saved.numStrings; ++i)
                    tuning.setStringTuning (i, saved.strings[(size_t) i]);
                engine.refreshStringPhysics();
                return false;
            }

            imported.openFrequencyHz = midiToHz (midi);
            clearPlaybackOffsets (imported);
            tuning.setStringTuning (s, imported);
        }

        engine.refreshStringPhysics();

        // refreshStringPhysics writes each string's ageing detune into fineTuneCents,
        // which would leave the imported open pitches slightly off. Playback offsets
        // stay cleared, as the tab states them.
        for (int s = 0; s < requestedStrings; ++s)
            tuning.setFineTuneCents (s, 0.0);
    }

    active = true;
    if (reason != nullptr)
        reason->clear();
    return true;
}

void TabPlaybackTuningSession::end() noexcept
{
    if (! active)
        return;

    {
        const LuthierEngine::ScopedStructuralChange change (engine);
        auto& tuning = engine.getTuningEngine();
        tuning.setNumStrings (saved.numStrings);
        tuning.setCapoFret (saved.capoFret);
        tuning.setCapoStringMask (saved.capoMask);

        for (int s = 0; s < saved.numStrings; ++s)
            tuning.setStringTuning (s, saved.strings[(size_t) s]);

        engine.refreshStringPhysics();

        // refreshStringPhysics overwrote fineTuneCents with the ageing detune; put the
        // captured value back so stop restores every field the session captured.
        for (int s = 0; s < saved.numStrings; ++s)
            tuning.setFineTuneCents (s, saved.strings[(size_t) s].fineTuneCents);
    }

    active = false;
}

} // namespace luthier
