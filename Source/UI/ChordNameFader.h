#pragma once

/*  piano-roll-chord-display.md 4: when the chord or note name on the guitar
    shows, and how strongly. Pure timing with the clock passed in, so the
    fades are testable (CD-02, CD-03, CD-05); ChordNameOverlay draws it.

    - Fade in over 60 ms to 0.35 from the first note of a chord.
    - Notes within the 30 ms strum window update the one name, not restart it.
    - Held at peak while notes sound, for at most 1.2 s; then 0.8 s to nothing.
    - A new chord crossfades from the old name over 60 ms.
    - A bend, slide or legato change (no new onset) updates the text in place.
    - Reduced motion: no fades - peak, then nothing after the hold.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

class ChordNameFader
{
public:
    static constexpr double kPeak = 0.35;
    static constexpr double kFadeInMs = 60.0;
    static constexpr double kBurstMs = 30.0;
    static constexpr double kHoldMs = 1200.0;
    static constexpr double kFadeOutMs = 800.0;
    static constexpr double kCrossfadeMs = 60.0;

    /** What started since the last observation. The caller decides a strum's
        membership from the audio's own sample times (a 30 Hz UI cannot see a
        30 ms window): `sameStrum` is an onset within kBurstMs of the chord's
        first one, `newChord` any later onset. */
    enum class Onset { none, sameStrum, newChord };

    /** One observation: `sounding` - any note sounds; `name` - the name for
        what sounds now (ignored when nothing does). */
    void update (double nowMs, bool sounding, Onset onset, const juce::String& name);

    /** The name shown and its opacity (0 - kPeak) at `nowMs`. */
    juce::String getText() const { return text; }
    float getOpacity (double nowMs) const noexcept;

    /** The previous name while a crossfade runs, else empty. */
    juce::String getOutgoingText (double nowMs) const;
    float getOutgoingOpacity (double nowMs) const noexcept;

    bool isVisible (double nowMs) const noexcept { return getOpacity (nowMs) > 0.0f || getOutgoingOpacity (nowMs) > 0.0f; }

    void setReducedMotion (bool reduced) noexcept { reducedMotion = reduced; }
    void clear() noexcept;

private:
    juce::String text, outgoing;
    double chordStartMs = -1.0e9;     ///< the first onset of the chord shown
    double releaseMs = -1.0;          ///< when the notes stopped (-1 while they sound)
    double outgoingFromMs = -1.0e9;   ///< the crossfade's start
    float outgoingFrom = 0.0f;        ///< the old name's opacity when the new one came
    bool reducedMotion = false;

    float envelope (double nowMs) const noexcept;
};

} // namespace luthier
