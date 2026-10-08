#pragma once

/*  Key detection for imported tabs (universal tab player, item 1).

    A page's own "Key:" header wins. Otherwise the chord symbols vote (a chord's
    root, third and fifth are weighted by a beat each, the first chord's root a
    little more) and the parsed notes are matched to the Krumhansl-Kessler
    major/minor pitch-class profiles, duration-weighted. The result is spelled
    the way chord symbols are ("Am", "Bb", "F#m") and stored in Meta::key.

    Offline, any non-audio thread.
*/

#include "PerformanceScore.h"

namespace luthier
{

struct TabKeyEstimate
{
    int rootPitchClass = -1;      ///< -1 when there was nothing to go on
    bool minor = false;
    double confidence = 0.0;      ///< 0..1: the gap between the best and the runner-up profile
    juce::String name;            ///< "Am", "G"; empty when unknown
};

class TabKeyDetector
{
public:
    /** Estimates the key of a track from its notes and chord symbols. */
    static TabKeyEstimate estimate (const PerformanceScore& score, int trackIndex = 0);

    /** Estimates from a bare pitch-class weight vector (12 entries). */
    static TabKeyEstimate estimateFromProfile (const double (&weights)[12]);

    /** Fills Meta::key when it is empty (a "Key:" header already there wins). */
    static void apply (PerformanceScore& score);

    /** "Am" -> (9, true); "Bb" -> (10, false). False for anything else. */
    static bool parseKeyName (const juce::String& name, int& rootPitchClass, bool& minor);
};

} // namespace luthier
