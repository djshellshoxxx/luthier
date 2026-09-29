#include "BassStepGrid.h"

namespace luthier
{

const char* getBassStepTypeName (BassStepType t) noexcept
{
    switch (t)
    {
        case BassStepType::thumb:  return "Thumb";
        case BassStepType::pop:    return "Pop";
        case BassStepType::ghost:  return "Ghost";
        case BassStepType::finger: return "Finger";
        case BassStepType::dead:   return "Dead";
        case BassStepType::rest:
        case BassStepType::numTypes:
        default:                   return "Rest";
    }
}

const char* getBassStepNoteName (BassStepNote n) noexcept
{
    switch (n)
    {
        case BassStepNote::fifth:  return "5th";
        case BassStepNote::octave: return "8va";
        case BassStepNote::root:
        case BassStepNote::numNotes:
        default:                   return "Root";
    }
}

//==============================================================================
void BassStepGrid::setStep (int index, const BassStep& step) noexcept
{
    if (! juce::isPositiveAndBelow (index, kMaxSteps))
        return;

    auto s = step;
    s.level = juce::jlimit (0.0, 1.0, std::isfinite (s.level) ? s.level : 0.8);
    s.type = (BassStepType) juce::jlimit (0, (int) BassStepType::numTypes - 1, (int) s.type);
    s.note = (BassStepNote) juce::jlimit (0, (int) BassStepNote::numNotes - 1, (int) s.note);
    steps[(size_t) index] = s;
}

BassStepType BassStepGrid::nextType (BassStepType t) noexcept
{
    return (BassStepType) (((int) t + 1) % (int) BassStepType::numTypes);
}

bool BassStepGrid::isEmpty() const noexcept
{
    for (int i = 0; i < length; ++i)
        if (! steps[(size_t) i].isRest())
            return false;

    return true;
}

void BassStepGrid::clear() noexcept
{
    steps.fill (BassStep {});
}

bool BassStepGrid::operator== (const BassStepGrid& o) const noexcept
{
    if (length != o.length || subdivision != o.subdivision || gate != o.gate)
        return false;

    for (int i = 0; i < kMaxSteps; ++i)
        if (! (steps[(size_t) i] == o.steps[(size_t) i]))
            return false;

    return true;
}

//==============================================================================
juce::var BassStepGrid::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("name", name);
    root->setProperty ("length", length);
    root->setProperty ("subdivision", (int) subdivision);
    root->setProperty ("gate", gate);

    juce::Array<juce::var> list;

    for (int i = 0; i < length; ++i)
    {
        const auto& s = steps[(size_t) i];
        auto* o = new juce::DynamicObject();
        o->setProperty ("type", (int) s.type);
        o->setProperty ("level", s.level);
        o->setProperty ("note", (int) s.note);
        list.add (juce::var (o));
    }

    root->setProperty ("steps", list);
    return juce::var (root);
}

BassStepGrid BassStepGrid::fromVar (const juce::var& v)
{
    BassStepGrid g;

    if (! v.isObject())
        return g;

    g.name = v.getProperty ("name", "").toString();
    g.setLength ((int) v.getProperty ("length", kDefaultSteps));
    g.subdivision = (Subdivision) juce::jlimit (0, (int) Subdivision::numSubdivisions - 1,
                                                (int) v.getProperty ("subdivision", (int) Subdivision::sixteenth));
    g.setGate ((double) v.getProperty ("gate", 0.8));

    if (auto* list = v.getProperty ("steps", {}).getArray())
    {
        for (int i = 0; i < juce::jmin (list->size(), kMaxSteps); ++i)
        {
            const auto& o = list->getReference (i);
            BassStep s;
            s.type = (BassStepType) (int) o.getProperty ("type", 0);
            s.level = (double) o.getProperty ("level", 0.8);
            s.note = (BassStepNote) (int) o.getProperty ("note", 0);
            g.setStep (i, s);
        }
    }

    return g;
}

//==============================================================================
const char* BassStepGrid::getFactoryName (Factory which) noexcept
{
    switch (which)
    {
        case Factory::slapFunk:          return "Slap Funk";
        case Factory::fingerstyleGroove: return "Fingerstyle Groove";
        case Factory::motownThumb:       return "Motown Thumb";
        case Factory::rootFifthWalk:     return "Root-Fifth Walk";
        case Factory::numFactory:
        default:                         return "Bass Grid";
    }
}

BassStepGrid BassStepGrid::factory (Factory which)
{
    BassStepGrid g;
    g.setName (getFactoryName (which));

    // One character per sixteenth: T thumb, P pop, g ghost, F finger, x dead,
    // . rest. Upper case for the root, 5 marks the fifth, O the octave.
    auto fill = [&g] (const char* types, const char* notes)
    {
        for (int i = 0; types[i] != 0 && i < kMaxSteps; ++i)
        {
            BassStep s;

            switch (types[i])
            {
                case 'T': s.type = BassStepType::thumb;  s.level = 0.85; break;
                case 'P': s.type = BassStepType::pop;    s.level = 0.80; break;
                case 'g': s.type = BassStepType::ghost;  s.level = 0.50; break;
                case 'F': s.type = BassStepType::finger; s.level = 0.80; break;
                case 'f': s.type = BassStepType::finger; s.level = 0.55; break;
                case 'x': s.type = BassStepType::dead;   s.level = 0.60; break;
                default:  s.type = BassStepType::rest;   break;
            }

            if (notes != nullptr && notes[i] == '5') s.note = BassStepNote::fifth;
            if (notes != nullptr && notes[i] == 'O') s.note = BassStepNote::octave;

            g.setStep (i, s);
        }
    };

    switch (which)
    {
        case Factory::slapFunk:
            // The classic: thumb on the one, ghosts between, a pop on the octave.
            fill ("T.gTg.PgT.gTg.Pg", "......O.......O.");
            break;

        case Factory::fingerstyleGroove:
            fill ("F.fF.fF.F.fF.xf.", "......5.........");
            break;

        case Factory::motownThumb:
            // slap_fret_contact low makes this a thumb thump; the grid just says "thumb".
            g.setSubdivision (Subdivision::eighth);
            g.setLength (8);
            fill ("T.TT.T.T", "...5...5");
            break;

        case Factory::rootFifthWalk:
            g.setSubdivision (Subdivision::eighth);
            g.setLength (8);
            fill ("F.F.F.F.", "..5.O.5.");
            break;

        case Factory::numFactory:
        default:
            break;
    }

    return g;
}

} // namespace luthier
