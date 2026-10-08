#pragma once

#include "../LuthierEngine.h"
#include "../Notation/PerformanceScore.h"

namespace luthier
{

/** Temporary, non-destructive tuning used only while an imported tab is played.

    This is intentionally outside the UI: Practice asks for a session, this
    object performs the structural engine change and restores the exact prior
    TuningEngine state on stop. It never writes host parameters or preset state.

    Only same-string-count imports are exact-retuned. A 7-string tab on a
    6-string instrument is an instrument change, not a tuning change, and is
    left to the existing RiffTransposer adaptation path. */
class TabPlaybackTuningSession
{
public:
    explicit TabPlaybackTuningSession (LuthierEngine& engineToUse) noexcept
        : engine (engineToUse) {}

    ~TabPlaybackTuningSession() { end(); }

    bool begin (const ScoreTrack& track, juce::String* reason = nullptr);
    void end() noexcept;

    bool isActive() const noexcept { return active; }

private:
    struct SavedState
    {
        int numStrings = 0;
        int capoFret = 0;
        juce::uint32 capoMask = TuningEngine::kAllStrings;
        std::array<TuningEngine::StringTuning, kMaxStrings> strings {};
    };

    static double midiToHz (int midi) noexcept;
    static void clearPlaybackOffsets (TuningEngine::StringTuning& tuning) noexcept;

    LuthierEngine& engine;
    SavedState saved;
    bool active = false;

    JUCE_DECLARE_NON_COPYABLE (TabPlaybackTuningSession)
};

} // namespace luthier
