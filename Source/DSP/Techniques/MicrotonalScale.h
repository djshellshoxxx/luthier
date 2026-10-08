#pragma once

/*  A pitch grid for bend quantise (microtonal-bends.md 3).

    Equal divisions (quarter-tone / 24-EDO, semitone, 22-, 31-, 53-EDO) or a
    loaded scale: a Scala .scl file (degrees in cents or ratios, repeating at
    its period, rooted on middle C) or an AnaMark .tun file (an absolute pitch
    per MIDI note). Stored as a sorted list of absolute pitches in "MIDI
    cents" (MIDI note x 100, so A4 is 6900) in a fixed array: the audio thread
    searches it without allocating.
*/

#include "../Common/DspCommon.h"
#include <array>

namespace luthier
{

class MicrotonalScale
{
public:
    static constexpr int kMaxPoints = 2048;
    static constexpr double kRootMidiCents = 6000.0;   ///< a Scala scale's 1/1: middle C

    MicrotonalScale() { setEqual (12); }

    /** Every 1200/divisions cents; 12 and 24 line up with MIDI notes, others start on middle C. */
    void setEqual (int divisions) noexcept;

    /** Scala (.scl). Returns false and leaves the scale untouched on a malformed file, with a reason. */
    bool loadScala (const juce::String& text, juce::String& error);

    /** AnaMark (.tun): `note N=cents` under [Tuning] or [Exact Tuning], optional BaseFreq. */
    bool loadTun (const juce::String& text, juce::String& error);

    /** By extension. Message thread. */
    bool loadFile (const juce::File& file, juce::String& error);

    /** The grid point nearest `midiCents`. */
    double nearest (double midiCents) const noexcept;

    int getNumPoints() const noexcept { return numPoints; }
    double getPoint (int i) const noexcept { return points[(size_t) juce::jlimit (0, juce::jmax (0, numPoints - 1), i)]; }

    const juce::String& getName() const noexcept { return name; }

    /** The file the scale came from, verbatim, so a preset can carry it. */
    const juce::String& getSourceText() const noexcept { return sourceText; }
    bool isTun() const noexcept { return tun; }

    juce::var toVar() const;
    void fromVar (const juce::var& state);

private:
    void sortAndClamp() noexcept;

    std::array<double, kMaxPoints> points {};
    int numPoints = 0;
    juce::String name { "12-EDO" }, sourceText;
    bool tun = false;
};

} // namespace luthier
