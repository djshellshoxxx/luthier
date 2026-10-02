#pragma once

/*  Where the acoustic mic landmarks are on a body (mic-placement.md 3).

    A pure function of the guitar: the tail, the centre of the lower bout,
    the saddle, the soundhole, the upper bout and the 12th fret, along the
    centreline, in the scene millimetres of guitar-illustration.md 1 (origin at
    the saddle, +X toward the headstock). Mics are placed in landmark units
    (`ac_mic_along` 0 to 4), so a body swap in the Workshop moves the
    landmarks under the mic and "12th fret" stays at the 12th fret.

    The body geometry comes from the illustration's outline data
    (BodyOutlines.h), so the plot, the drawing and the sound agree.
*/

#include "GuitarLibrary.h"
#include "../../UI/Guitar/BodyOutlines.h"
#include <cmath>

namespace luthier
{

struct AcousticLandmarks
{
    /** X in mm of along = 0 (tail), 1 (lower bout), 2 (bridge), 3 (soundhole),
        4 (12th fret). Strictly increasing. */
    double alongMm[5] = { -330.0, -60.0, 0.0, 180.0, 324.0 };

    double upperBoutMm = 250.0;       ///< along = 3.5, labelled
    double lowerBoutHalfWidthMm = 200.0;
    bool hasSoundhole = true;
    const char* styleId = "dreadnought";

    /** along in [0, 4] -> X mm, linear in mm between landmarks. */
    double alongToMm (double along) const noexcept
    {
        const double a = juce::jlimit (0.0, 4.0, along);
        const int i = juce::jmin (3, (int) a);
        const double t = a - (double) i;
        return alongMm[i] * (1.0 - t) + alongMm[i + 1] * t;
    }

    /** across in [-1, 1] (+ treble side) -> Y mm. */
    double acrossToMm (double across) const noexcept
    {
        return juce::jlimit (-1.0, 1.0, across) * lowerBoutHalfWidthMm;
    }
};

//==============================================================================
/** The illustration's body style for an engine body shape. */
inline const char* acousticStyleIdFor (BodyShape shape) noexcept
{
    switch (shape)
    {
        case BodyShape::Parlor:            return "parlor";
        case BodyShape::Concert:           return "om";
        case BodyShape::Auditorium:        return "grand_auditorium";
        case BodyShape::Dreadnought:       return "dreadnought";
        case BodyShape::Jumbo:             return "jumbo";
        case BodyShape::Classical:         return "classical";
        case BodyShape::Flamenco:          return "flamenco";
        case BodyShape::Resonator:         return "resonator";
        case BodyShape::TwelveStringDread: return "dreadnought";
        default:                           return "dreadnought";
    }
}

/** Landmarks for a body shape and scale (mic-placement.md 3). */
inline AcousticLandmarks computeAcousticLandmarks (BodyShape shape, double scaleLengthMm) noexcept
{
    AcousticLandmarks lm;
    lm.styleId = acousticStyleIdFor (shape);

    const auto* style = outlines::findBodyStyle (lm.styleId);

    if (style == nullptr)
        style = outlines::findBodyStyle ("dreadnought");

    const auto& props = BodyModels::getShape (shape);
    lm.hasSoundhole = props.soundHoleMm > 0.0;

    const double L = style->lengthMm;
    const double saddleU = style->saddleU;
    const auto toX = [L, saddleU] (double u) { return (saddleU - u) * L; };

    // Widest point behind the waist (lower bout), widest ahead of it (upper
    // bout), and the narrowest between them (the waist), from the outline.
    double lowerU = 0.75, lowerW = 0.0, upperU = 0.3, upperW = 0.0;

    for (int i = 0; i < style->numOutline; ++i)
    {
        const auto& p = style->outline[i];
        const double w = std::abs ((double) p.v);

        if (p.u > 0.5 && w > lowerW)       { lowerW = w; lowerU = p.u; }
        if (p.u < 0.5 && p.u > style->neckU && w > upperW) { upperW = w; upperU = p.u; }
    }

    lm.lowerBoutHalfWidthMm = juce::jmax (50.0, (double) style->physWidthMm * 0.5);

    const double tail = toX (1.0);
    double lower = toX (lowerU);
    const double fret12 = scaleLengthMm * 0.5;
    double hole = 0.0;

    if (lm.hasSoundhole && style->hole != outlines::Hole::none
        && style->hole != outlines::Hole::resonatorCover)
        hole = toX (style->holeCentre.u);
    else
        hole = 0.5 * (toX (lowerU) + toX (upperU));   // geometric centre of the bouts

    // Keep the landmarks in order whatever the outline data says.
    lower = juce::jlimit (tail + 20.0, -10.0, lower);
    hole = juce::jlimit (10.0, fret12 - 10.0, hole);

    lm.alongMm[0] = tail;
    lm.alongMm[1] = lower;
    lm.alongMm[2] = 0.0;
    lm.alongMm[3] = hole;
    lm.alongMm[4] = fret12;
    lm.upperBoutMm = 0.5 * (hole + fret12);
    return lm;
}

inline AcousticLandmarks computeAcousticLandmarks (const GuitarSpec& spec) noexcept
{
    return computeAcousticLandmarks (spec.bodyShape, spec.scaleLengthMm);
}

} // namespace luthier
