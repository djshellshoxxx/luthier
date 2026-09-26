#pragma once

/*  part-acoustics.md 2.1's chambering, as the realism specs that key on it
    (environment.md 2.4, body-coupling.md 2.2) need it for any guitar - a parts
    guitar or a compiled one, whose body shape is all there is to go on.
*/

#include "BodyModels.h"

namespace luthier
{

enum class Chambering { solid = 0, chambered, semiHollow, hollow, acoustic, numChamberings };

inline Chambering chamberingFor (BodyShape shape) noexcept
{
    switch (shape)
    {
        case BodyShape::Parlor:
        case BodyShape::Concert:
        case BodyShape::Auditorium:
        case BodyShape::Dreadnought:
        case BodyShape::Jumbo:
        case BodyShape::Classical:
        case BodyShape::Flamenco:
        case BodyShape::Resonator:
        case BodyShape::TwelveStringDread:  return Chambering::acoustic;

        case BodyShape::Hollow:
        case BodyShape::BassHollow:         return Chambering::hollow;

        case BodyShape::SemiHollow:         return Chambering::semiHollow;
        case BodyShape::Chambered:          return Chambering::chambered;

        case BodyShape::SolidThin:
        case BodyShape::SolidStandard:
        case BodyShape::SolidHeavy:
        case BodyShape::Offset:
        case BodyShape::BassSolid:
        case BodyShape::NumShapes:
        default:                            return Chambering::solid;
    }
}

inline const char* getChamberingName (Chambering c) noexcept
{
    switch (c)
    {
        case Chambering::chambered:  return "chambered";
        case Chambering::semiHollow: return "semi_hollow";
        case Chambering::hollow:     return "hollow";
        case Chambering::acoustic:   return "acoustic";
        case Chambering::solid:
        case Chambering::numChamberings:
        default:                     return "solid";
    }
}

} // namespace luthier
