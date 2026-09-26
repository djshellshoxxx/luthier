#pragma once

/*  harmonic-realism.md 1 and 4: where the nodes are.

    The geometry every harmonic shares: which partial a touch at a given place
    selects (the analytic node search that replaces the old fret table), how
    far a fret is along the string, and - for keyboard players - which string
    and partial sound a requested pitch (HarmonicLocator, 4.2).

    Pure functions of the geometry: no state, no allocation, safe anywhere.
*/

#include "../Common/DspCommon.h"

namespace luthier::harmonics
{

/** The partials the node search considers (harmonic-realism.md 1). */
inline constexpr int kMinPartial = 2;
inline constexpr int kMaxPartial = 8;

/** Below this efficiency a touch is off-node: a dead thud (1, 3).

    DECISION (REALISM-B): 0.1 rather than the draft's 0.05. With the pad
    profile below, a node whose e is under 0.1 loses over 5 dB per round trip
    at the default pressure - it dies with everything else, which is a miss.
    Fret 6.0 (4.7 mm from the 7th partial's node at 5.83) is e = 0.09. */
inline constexpr double kOffNodeEfficiency = 0.1;

/*  Node efficiency of a finger pad of contact width `widthMm` whose centre is
    `distanceMm` from a node.

    DECISION (REALISM-B): a super-Gaussian, exp(-(d / 1.5w)^4), in place of
    the spec draft's exp(-(d/w)^2). A pad is flat across its contact width, so
    a node anywhere under it is touched squarely (e stays above 0.8 out to
    d = w); once the node leaves the pad its edge sits on moving string and
    the efficiency falls steeply (0.04 at d = 2w). The Gaussian took 31 dB off
    a low-E harmonic touched 3 mm off the 12th-fret node at 2.5 mm width,
    where HR-07 (and players) expect a few dB. */
inline double nodeEfficiency (double distanceMm, double widthMm) noexcept
{
    const double w = juce::jmax (0.01, widthMm);
    const double u = std::abs (distanceMm) / (1.5 * w);
    const double u2 = u * u;
    return std::exp (-u2 * u2);
}

struct NodeChoice
{
    int partial = 0;          ///< 0 = off-node (uniform damping)
    double efficiency = 0.0;  ///< e of the chosen partial (the best e even when off-node)
    double distanceMm = 0.0;  ///< from the touch to that partial's nearest node
};

/** 1: `n` in 2..8 maximising e / sqrt(n). `positionFraction` is the touch
    as a fraction of the vibrating length (either end: nodes are symmetric). */
inline NodeChoice findNode (double positionFraction, double vibratingLengthMm, double widthMm) noexcept
{
    NodeChoice best;
    double bestScore = -1.0;
    const double x = juce::jlimit (0.0, 1.0, positionFraction);
    const double lengthMm = juce::jmax (1.0, vibratingLengthMm);

    for (int n = kMinPartial; n <= kMaxPartial; ++n)
    {
        // Nearest node m/n of partial n (m = 1..n-1).
        const int m = juce::jlimit (1, n - 1, (int) std::lround (x * (double) n));
        const double d = std::abs (x - (double) m / (double) n) * lengthMm;
        const double e = nodeEfficiency (d, widthMm);
        const double score = e / std::sqrt ((double) n);

        if (score > bestScore)
        {
            bestScore = score;
            best.partial = n;
            best.efficiency = e;
            best.distanceMm = d;
        }
    }

    if (best.efficiency < kOffNodeEfficiency)
        best.partial = 0;

    return best;
}

/** A touch at fret `touchFret` on a string stopped at `stoppedFret`, as a
    fraction of the vibrating length measured from the bridge. */
inline double touchFractionFromBridge (double touchFret, double stoppedFret) noexcept
{
    return juce::jlimit (0.0, 1.0, std::pow (2.0, -(touchFret - stoppedFret) / 12.0));
}

/** The vibrating length of a string stopped at a fret. */
inline double vibratingLengthMm (double scaleLengthMm, double stoppedFret) noexcept
{
    return scaleLengthMm * std::pow (2.0, -juce::jmax (0.0, stoppedFret) / 12.0);
}

/** The partial a touch at `fret` on an open string selects, 0 if none - the
    analytic replacement for the old node table (4.1). */
inline int partialForFret (double fret, double scaleLengthMm = 648.0, double widthMm = 2.5) noexcept
{
    if (fret <= 0.0)
        return 0;

    return findNode (touchFractionFromBridge (fret, 0.0), scaleLengthMm, widthMm).partial;
}

/*  The exact fret offset of the artificial / tapped offset choices (5):
    12 -> 2, 7 -> 3, 5 -> 4, 4 (3.86) -> 5, 19 -> 3 (the 2/3 node), 24 -> 4. */
inline double offsetFretsForChoice (int choiceIndex) noexcept
{
    switch (choiceIndex)
    {
        case 1:  return 12.0 * std::log2 (3.0 / 2.0);   // 7.02
        case 2:  return 12.0 * std::log2 (4.0 / 3.0);   // 4.98
        case 3:  return 12.0 * std::log2 (5.0 / 4.0);   // 3.86
        case 4:  return 12.0 * std::log2 (3.0);         // 19.02
        case 5:  return 24.0;
        case 0:
        default: return 12.0;
    }
}

/*  Tab reading for a MIDI-voiced (integer) touch fret.

    DECISION (REALISM-B): tab writes the 5th-partial harmonics as <4>, <9> and
    <16>, and a player puts the finger on the node (3.86, 8.84, 15.87) rather
    than over the wire. A voiced note is an integer fret, so the interpreter
    reads it as tab: an integer fret within 0.35 of a node of partials 2-5
    lands on that node. A fractional fret (a guitar controller's bend, the
    API) is used exactly, which is how HR-04..07 reach 7.02, 3.86, 6.0. */
inline double tabTouchFret (double fret) noexcept
{
    if (std::abs (fret - std::round (fret)) > 1.0e-9 || fret <= 0.0)
        return fret;

    double best = fret, bestDistance = 0.35;

    for (int n = 2; n <= 5; ++n)
    {
        for (int m = 1; m < n; ++m)
        {
            // A node m/n from the nut, as a fret.
            const double nodeFret = -12.0 * std::log2 (1.0 - (double) m / (double) n);
            const double d = std::abs (nodeFret - fret);

            if (d < bestDistance)
            {
                bestDistance = d;
                best = nodeFret;
            }
        }
    }

    return best;
}

//==============================================================================
/** 4.2: the sounding-pitch mapping's search. */
struct LocatedHarmonic
{
    bool found = false;
    int stringIndex = -1;
    int partial = 0;
    double touchFret = 0.0;
};

/** For n = 2..7 and each string, accepts when n f_open sqrt(1 + B n^2) is
    within 30 cents of the target and the node fret is playable; prefers the
    lowest n, then the string nearest the hand. The node taken is the one
    nearest the nut (1/n), the one players reach for. */
inline LocatedHarmonic locate (double targetHz, const double* openHz, const double* inharmonicityB,
                               int numStrings, int maxFrets, double handFret) noexcept
{
    LocatedHarmonic result;

    if (targetHz <= 0.0 || openHz == nullptr)
        return result;

    for (int n = 2; n <= 7 && ! result.found; ++n)
    {
        double bestHandDistance = 1.0e9;

        for (int s = 0; s < numStrings; ++s)
        {
            const double b = inharmonicityB != nullptr ? inharmonicityB[s] : 0.0;
            const double f = (double) n * openHz[s] * std::sqrt (1.0 + b * (double) (n * n));

            if (f <= 0.0 || std::abs (1200.0 * std::log2 (f / targetHz)) > 30.0)
                continue;

            const double nodeFret = -12.0 * std::log2 (1.0 - 1.0 / (double) n);

            if (nodeFret > (double) maxFrets)
                continue;

            const double handDistance = std::abs (nodeFret - handFret);

            if (handDistance < bestHandDistance)
            {
                bestHandDistance = handDistance;
                result.found = true;
                result.stringIndex = s;
                result.partial = n;
                result.touchFret = nodeFret;
            }
        }
    }

    return result;
}

} // namespace luthier::harmonics

namespace luthier
{
/** harmonic-realism.md 5: the touch parameters the engine builds contacts from. */
struct HarmonicTouchSettings
{
    double pressure = 0.6;         ///< harmonic_touch_pressure, g
    double fingerWidthMm = 2.5;    ///< harmonic_finger_width, w
    double touchSeconds = 0.070;   ///< harmonic_touch_time
    double briefSeconds = 0.008;   ///< harmonic_brief_touch (pinch graze; a tap is 1.5x)
    double thumbOffsetMm = 6.0;    ///< pinch_thumb_offset_mm
};
} // namespace luthier
