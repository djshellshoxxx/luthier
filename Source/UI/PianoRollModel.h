#pragma once

/*  piano-roll-chord-display.md 1-2: what the piano roll shows, apart from how
    it is drawn - the range, the lit keys and the roll's bars. Pure, with the
    clock passed in, so PR-01, PR-02 and PR-06 test it without a window.

    - Range: from the lowest open string (after tuning and capo) to the highest
      string's last fret; drawn padded to whole octaves (C to B), the keys
      outside the playable range dimmed.
    - A key lit by a string takes that string's colour (StringColours) at 85 %;
      held while the string sounds, then 150 ms to nothing.
    - The roll: the last 4 s, one bar per note from its start to its note-off,
      in the string's colour at 60 %, marked when bent more than 20 cents.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Support/SoundingNotes.h"

namespace luthier
{

class TuningEngine;

/** One colour per string (engine index, 0 = the high E), the same wherever a
    string is coloured by index. Evenly spaced hues, readable on every palette. */
namespace StringColours
{
    juce::Colour forString (int stringIndex, int numStrings);
}

class PianoRollModel
{
public:
    struct Range
    {
        int lowestPlayable = 40, highestPlayable = 88;   ///< E2 - E6
        int drawLow = 36, drawHigh = 95;                 ///< whole octaves around it

        bool isPlayable (int note) const noexcept { return note >= lowestPlayable && note <= highestPlayable; }
        int numKeys() const noexcept { return drawHigh - drawLow + 1; }
    };

    /** The instrument's range now: tuning, capo and each string's fret count. */
    static Range rangeFor (const TuningEngine& tuning, int numStrings);

    struct Key
    {
        float alpha = 0.0f;           ///< 0 - 0.85
        juce::Colour colour;
        int string = -1;              ///< the string sounding it, -1 when not
        double releasedMs = -1.0;     ///< when it stopped sounding
    };

    struct Bar
    {
        int note = 0, string = 0;
        double startMs = 0.0, endMs = -1.0;   ///< -1 while it sounds
        bool bent = false;                    ///< bent more than 20 cents at some point
    };

    static constexpr float kLitAlpha = 0.85f;
    static constexpr float kBarAlpha = 0.60f;
    static constexpr double kReleaseMs = 150.0;
    static constexpr double kRollSeconds = 4.0;
    static constexpr int kBendTickCents = 20;
    static constexpr double kStaleMs = 250.0;

    /** One UI frame: the strings as the audio thread last published them. A
        frame that has not changed for 250 ms (`stale`) lights nothing. */
    void update (double nowMs, const SoundingNotes::Frame& frame, bool stale);

    const Key& getKey (int note) const noexcept { return keys[(size_t) juce::jlimit (0, 127, note)]; }
    float keyAlpha (int note, double nowMs) const noexcept;
    bool isLit (int note, double nowMs) const noexcept { return keyAlpha (note, nowMs) > 0.0f; }
    int countLit (double nowMs) const noexcept;

    const std::vector<Bar>& getBars() const noexcept { return bars; }

    int numStrings = 6;

private:
    std::array<Key, 128> keys {};
    std::vector<Bar> bars;
    std::array<int, SoundingNotes::kMaxStrings> openBar {};   ///< index into bars, -1 none
    std::array<std::int64_t, SoundingNotes::kMaxStrings> barStart {};
    bool initialised = false;
};

} // namespace luthier
