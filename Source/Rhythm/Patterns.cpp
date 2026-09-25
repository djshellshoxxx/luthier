#include "Patterns.h"

namespace luthier
{

//==============================================================================
const char* getStrumTypeName (StrumType type) noexcept
{
    switch (type)
    {
        case StrumType::rest:      return "Rest";
        case StrumType::down:      return "Down";
        case StrumType::up:        return "Up";
        case StrumType::downMute:  return "DownMute";
        case StrumType::upMute:    return "UpMute";
        case StrumType::rake:      return "Rake";
        case StrumType::rasgueado: return "Rasgueado";
        case StrumType::chuck:     return "Chuck";
        case StrumType::numTypes:
        default:                   return "Rest";
    }
}

const char* getFingerName (Finger finger) noexcept
{
    switch (finger)
    {
        case Finger::thumb:  return "p";
        case Finger::index:  return "i";
        case Finger::middle: return "m";
        case Finger::ring:   return "a";
        case Finger::little: return "e";
        case Finger::numFingers:
        default:             return "p";
    }
}

Finger fingerFromLetter (juce::juce_wchar letter) noexcept
{
    switch (letter)
    {
        case 'i': return Finger::index;
        case 'm': return Finger::middle;
        case 'a': return Finger::ring;
        case 'e': return Finger::little;
        case 'p':
        default:  return Finger::thumb;
    }
}

const char* getSubdivisionName (Subdivision s) noexcept
{
    switch (s)
    {
        case Subdivision::eighth:           return "8";
        case Subdivision::eighthTriplet:    return "8T";
        case Subdivision::sixteenth:        return "16";
        case Subdivision::sixteenthTriplet: return "16T";
        case Subdivision::thirtySecond:     return "32";
        case Subdivision::numSubdivisions:
        default:                            return "16";
    }
}

double subdivisionsPerBeat (Subdivision s) noexcept
{
    switch (s)
    {
        case Subdivision::eighth:           return 2.0;
        case Subdivision::eighthTriplet:    return 3.0;
        case Subdivision::sixteenth:        return 4.0;
        case Subdivision::sixteenthTriplet: return 6.0;
        case Subdivision::thirtySecond:     return 8.0;
        case Subdivision::numSubdivisions:
        default:                            return 4.0;
    }
}

//==============================================================================
RhythmPattern::RhythmPattern()
{
    clear();
}

void RhythmPattern::clear() noexcept
{
    for (auto& s : strumSteps)
        s = StrumStep {};

    for (auto& s : fingerpickSteps)
        s = FingerpickStep {};

    for (auto& s : muteSteps)
        s = MuteStep {};

    crossingSps = 0.0;
}

MuteStep RhythmPattern::getMuteStep (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, kMaxSteps) ? muteSteps[(size_t) index] : MuteStep {};
}

void RhythmPattern::setMuteStep (int index, const MuteStep& step) noexcept
{
    if (! juce::isPositiveAndBelow (index, kMaxSteps))
        return;

    auto& m = muteSteps[(size_t) index];
    m.type = juce::isPositiveAndBelow ((int) step.type, (int) MuteType::numTypes) ? step.type : MuteType::open;
    m.pressure = step.pressure < 0.0 ? -1.0 : juce::jlimit (0.0, 1.0, step.pressure);
    m.positionMm = step.positionMm < 0.0 ? -1.0
                                         : juce::jlimit (MuteSettings::kMinPositionMm, MuteSettings::kMaxPositionMm, step.positionMm);
}

StrumStep RhythmPattern::getStrumStep (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, kMaxSteps) ? strumSteps[(size_t) index] : StrumStep {};
}

void RhythmPattern::setStrumStep (int index, const StrumStep& step) noexcept
{
    if (! juce::isPositiveAndBelow (index, kMaxSteps))
        return;

    auto& s = strumSteps[(size_t) index];
    s.type = step.type;
    s.dynamic = juce::jlimit (0.0, 1.0, step.dynamic);

    // A rest strums no strings, so it has no mask. Normalising here rather than
    // leaving whatever was set keeps the invariant true everywhere, including
    // after a round trip: the serialiser skips rests entirely, so a rest that
    // carried a mask would come back with the default one and not match.
    s.stringMask = step.isRest() ? 0x0FFF : step.stringMask;

    // ambiguity-resolutions 6: a step may name its own crossing velocity.
    s.crossingSps = step.isRest() ? 0.0
                                  : (step.crossingSps > 0.0 ? juce::jlimit (20.0, 800.0, step.crossingSps) : 0.0);
}

FingerpickStep RhythmPattern::getFingerpickStep (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, kMaxSteps) ? fingerpickSteps[(size_t) index]
                                                       : FingerpickStep {};
}

void RhythmPattern::setFingerpickStep (int index, const FingerpickStep& step) noexcept
{
    if (! juce::isPositiveAndBelow (index, kMaxSteps))
        return;

    auto& s = fingerpickSteps[(size_t) index];
    s.active = step.active;
    s.finger = step.finger;
    s.dynamic = juce::jlimit (0.0, 1.0, step.dynamic);
}

int RhythmPattern::getStringForFinger (Finger finger) const noexcept
{
    const auto i = (size_t) juce::jlimit (0, (int) Finger::numFingers - 1, (int) finger);
    return fingerStrings[i];
}

void RhythmPattern::setStringForFinger (Finger finger, int stringIndex) noexcept
{
    const auto i = (size_t) juce::jlimit (0, (int) Finger::numFingers - 1, (int) finger);
    fingerStrings[i] = juce::jlimit (0, kMaxStrings - 1, stringIndex);
}

bool RhythmPattern::isEmpty() const noexcept
{
    if (kind == Kind::strum)
    {
        for (int i = 0; i < length; ++i)
            if (! strumSteps[(size_t) i].isRest())
                return false;
    }
    else
    {
        for (int i = 0; i < length; ++i)
            if (fingerpickSteps[(size_t) i].active)
                return false;
    }

    return true;
}

//==============================================================================
juce::var RhythmPattern::toVar() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("name", name);
    root->setProperty ("type", kind == Kind::strum ? "strum" : "fingerpick");
    root->setProperty ("length_steps", length);
    root->setProperty ("subdivision", getSubdivisionName (subdivision));
    root->setProperty ("swing", swing);

    // ambiguity-resolutions 6: written only when the pattern names one.
    if (crossingSps > 0.0)
        root->setProperty ("crossing_sps", crossingSps);

    juce::Array<juce::var> stepArray;

    // muting-rhythm.md 2: a step's mute, written only when it is not open, so
    // a pattern without mutes reads exactly as it did before.
    auto writeMute = [this] (juce::DynamicObject* o, int i)
    {
        const auto& m = muteSteps[(size_t) i];

        if (m.isOpen())
            return;

        o->setProperty ("mute_type", getMuteTypeId (m.type));

        if (m.pressure >= 0.0)
            o->setProperty ("mute_pressure", m.pressure);

        if (m.positionMm >= 0.0)
            o->setProperty ("mute_position_mm", m.positionMm);
    };

    for (int i = 0; i < length; ++i)
    {
        if (kind == Kind::strum)
        {
            const auto& s = strumSteps[(size_t) i];

            // A rest with a mute is still written: the mute belongs to the step.
            if (s.isRest() && muteSteps[(size_t) i].isOpen())
                continue;

            auto* o = new juce::DynamicObject();
            o->setProperty ("step", i);
            o->setProperty ("event", getStrumTypeName (s.type));
            o->setProperty ("dynamic", s.dynamic);
            writeMute (o, i);

            if (s.crossingSps > 0.0)
                o->setProperty ("crossing_sps", s.crossingSps);

            // The mask is written low-index-first, as the spec's example shows.
            juce::String maskText;

            for (int bit = 0; bit < kMaxStrings; ++bit)
                maskText << (((s.stringMask >> bit) & 1u) != 0 ? "1" : "0");

            o->setProperty ("mask", maskText);
            stepArray.add (juce::var (o));
        }
        else
        {
            const auto& s = fingerpickSteps[(size_t) i];

            if (! s.active)
                continue;

            auto* o = new juce::DynamicObject();
            o->setProperty ("step", i);
            o->setProperty ("event", "Pluck");
            o->setProperty ("finger", getFingerName (s.finger));
            o->setProperty ("dynamic", s.dynamic);
            writeMute (o, i);
            stepArray.add (juce::var (o));
        }
    }

    root->setProperty ("steps", stepArray);

    if (kind == Kind::fingerpick)
    {
        juce::Array<juce::var> assignment;

        for (int f = 0; f < (int) Finger::numFingers; ++f)
            assignment.add (fingerStrings[(size_t) f]);

        root->setProperty ("finger_strings", assignment);
    }

    juce::Array<juce::var> tagArray;

    for (const auto& t : tags)
        tagArray.add (t);

    root->setProperty ("tags", tagArray);

    return juce::var (root);
}

RhythmPattern RhythmPattern::fromVar (const juce::var& state)
{
    RhythmPattern pattern;

    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return pattern;

    pattern.name = root->getProperty ("name").toString();

    if (pattern.name.isEmpty())
        pattern.name = "Untitled";

    pattern.kind = (root->getProperty ("type").toString() == "fingerpick")
                     ? Kind::fingerpick : Kind::strum;

    pattern.setLength ((int) root->getProperty ("length_steps"));

    const auto subdivisionText = root->getProperty ("subdivision").toString();

    for (int s = 0; s < (int) Subdivision::numSubdivisions; ++s)
    {
        if (subdivisionText == getSubdivisionName ((Subdivision) s))
        {
            pattern.subdivision = (Subdivision) s;
            break;
        }
    }

    if (root->hasProperty ("swing"))
        pattern.setSwing ((double) root->getProperty ("swing"));

    if (root->hasProperty ("crossing_sps"))
        pattern.setCrossingSps ((double) root->getProperty ("crossing_sps"));

    if (auto* assignment = root->getProperty ("finger_strings").getArray())
        for (int f = 0; f < juce::jmin ((int) Finger::numFingers, assignment->size()); ++f)
            pattern.setStringForFinger ((Finger) f, (int) assignment->getReference (f));

    if (auto* stepArray = root->getProperty ("steps").getArray())
    {
        for (const auto& entry : *stepArray)
        {
            auto* o = entry.getDynamicObject();

            if (o == nullptr)
                continue;

            const int index = (int) o->getProperty ("step");

            if (! juce::isPositiveAndBelow (index, kMaxSteps))
                continue;

            const auto eventName = o->getProperty ("event").toString();
            const double dynamic = o->hasProperty ("dynamic")
                                     ? (double) o->getProperty ("dynamic") : 1.0;

            // muting-rhythm.md 2 and 4: a missing mute_type is open; an unknown one too.
            if (o->hasProperty ("mute_type"))
            {
                MuteStep mute;
                mute.type = muteTypeFromId (o->getProperty ("mute_type").toString());
                mute.pressure = o->hasProperty ("mute_pressure") ? (double) o->getProperty ("mute_pressure") : -1.0;
                mute.positionMm = o->hasProperty ("mute_position_mm") ? (double) o->getProperty ("mute_position_mm") : -1.0;
                pattern.setMuteStep (index, mute);
            }

            if (pattern.kind == Kind::fingerpick || eventName == "Pluck")
            {
                FingerpickStep step;
                step.active = true;
                step.dynamic = dynamic;

                const auto fingerText = o->getProperty ("finger").toString();
                step.finger = fingerFromLetter (fingerText.isEmpty() ? 'p' : fingerText[0]);

                pattern.setFingerpickStep (index, step);
            }
            else
            {
                StrumStep step;
                step.dynamic = dynamic;

                if (o->hasProperty ("crossing_sps"))
                    step.crossingSps = (double) o->getProperty ("crossing_sps");

                for (int t = 0; t < (int) StrumType::numTypes; ++t)
                {
                    if (eventName == getStrumTypeName ((StrumType) t))
                    {
                        step.type = (StrumType) t;
                        break;
                    }
                }

                const auto maskText = o->getProperty ("mask").toString();

                if (maskText.isNotEmpty())
                {
                    uint16_t mask = 0;

                    for (int bit = 0; bit < juce::jmin (kMaxStrings, maskText.length()); ++bit)
                        if (maskText[bit] == '1')
                            mask = (uint16_t) (mask | (uint16_t) (1u << bit));

                    step.stringMask = mask;
                }

                pattern.setStrumStep (index, step);
            }
        }
    }

    juce::StringArray loadedTags;

    if (auto* tagArray = root->getProperty ("tags").getArray())
        for (const auto& t : *tagArray)
            loadedTags.add (t.toString());

    pattern.setTags (loadedTags);

    return pattern;
}

bool RhythmPattern::loadFrom (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.getDynamicObject() == nullptr)
        return false;

    *this = fromVar (parsed);
    return true;
}

bool RhythmPattern::saveTo (const juce::File& file) const
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (toVar(), false));
}

//==============================================================================
namespace
{
    /** Builds a strum pattern from a compact description, so the factory table
        below reads like the patterns it describes rather than like code.

        `events` is one entry per step: '.' is a rest, and the letters are the
        first letter of the strum type, with muted strokes in upper case. */
    RhythmPattern makeStrum (const char* name, const char* events,
                             Subdivision subdivision, double swing,
                             const char* tags,
                             const char* masks = nullptr)
    {
        RhythmPattern p;
        p.setName (name);
        p.setKind (RhythmPattern::Kind::strum);
        p.setSubdivision (subdivision);
        p.setSwing (swing);
        p.setTags (juce::StringArray::fromTokens (juce::String (tags), " ", ""));

        const int length = juce::jmin (RhythmPattern::kMaxSteps, (int) std::strlen (events));
        p.setLength (length);

        for (int i = 0; i < length; ++i)
        {
            StrumStep step;

            switch (events[i])
            {
                case 'd': step.type = StrumType::down;      step.dynamic = 1.0;  break;
                case 'u': step.type = StrumType::up;        step.dynamic = 0.82; break;
                case 'D': step.type = StrumType::downMute;  step.dynamic = 0.8;  break;
                case 'U': step.type = StrumType::upMute;    step.dynamic = 0.7;  break;
                case 'r': step.type = StrumType::rake;      step.dynamic = 0.75; break;
                case 'R': step.type = StrumType::rasgueado; step.dynamic = 0.9;  break;
                case 'c': step.type = StrumType::chuck;     step.dynamic = 0.8;  break;
                case '.':
                default:  step.type = StrumType::rest;      break;
            }

            // Masks are given as one character per step: 'a' is all strings,
            // 'l' the lower half, 'h' the upper half.
            //
            // String index 0 is the high E, so the low strings carry the *high*
            // indices, and the low-string masks run upward from index 3 rather
            // than downward from index 5. They are deliberately open-ended at the
            // top so that a seven- or eight-string guitar's extra bass strings -
            // which sit at indices 6 and 7 - are included too. Bits that no
            // string occupies cost nothing: the scheduler intersects the mask
            // with the voicing, so only strings that are actually being fretted
            // ever sound.
            if (masks != nullptr && i < (int) std::strlen (masks))
            {
                switch (masks[i])
                {
                    case 'l': step.stringMask = 0x0FF8; break;   // string 3 and below
                    case 'h': step.stringMask = 0x0007; break;   // strings 0..2
                    case 'm': step.stringMask = 0x001C; break;   // strings 2..4
                    case 'b': step.stringMask = 0x0FF0; break;   // string 4 and below
                    case 'a':
                    default:  step.stringMask = 0x0FFF; break;
                }
            }

            p.setStrumStep (i, step);
        }

        return p;
    }

    /** `fingers` is one letter per step, '.' for a rest. */
    RhythmPattern makeFingerpick (const char* name, const char* fingers,
                                  Subdivision subdivision, double swing,
                                  const char* tags,
                                  std::array<int, 5> fingerStrings)
    {
        RhythmPattern p;
        p.setName (name);
        p.setKind (RhythmPattern::Kind::fingerpick);
        p.setSubdivision (subdivision);
        p.setSwing (swing);
        p.setTags (juce::StringArray::fromTokens (juce::String (tags), " ", ""));

        const int length = juce::jmin (RhythmPattern::kMaxSteps, (int) std::strlen (fingers));
        p.setLength (length);

        for (int f = 0; f < 5; ++f)
            p.setStringForFinger ((Finger) f, fingerStrings[(size_t) f]);

        for (int i = 0; i < length; ++i)
        {
            FingerpickStep step;

            // A space reads as a rest as well as a dot, so a pattern string can be
            // grouped for legibility without silently becoming thumb strokes.
            if (fingers[i] != '.' && fingers[i] != ' ')
            {
                step.active = true;
                step.finger = fingerFromLetter ((juce::juce_wchar) fingers[i]);

                // The thumb carries the beat and is played a little harder, the
                // way a real hand does it.
                step.dynamic = (step.finger == Finger::thumb) ? 1.0 : 0.78;
            }

            p.setFingerpickStep (i, step);
        }

        return p;
    }
}

//==============================================================================
PatternLibrary::PatternLibrary()
{
    addFactoryPatterns();
}

void PatternLibrary::addFactoryPatterns()
{
    patterns.clear();

    // ---- strum patterns ---------------------------------------------------------
    patterns.push_back (makeStrum ("Country Boom Chick", "d...U.d.D...u...",
                                   Subdivision::sixteenth, 0.55, "country acoustic medium",
                                   "aaaahhhhllllhhhh"));

    patterns.push_back (makeStrum ("Folk Down Up", "d.u.d.u.d.u.d.u.",
                                   Subdivision::sixteenth, 0.5, "folk acoustic easy"));

    patterns.push_back (makeStrum ("Classic Strum", "d...d.u.u.d.u...",
                                   Subdivision::sixteenth, 0.5, "pop acoustic easy"));

    patterns.push_back (makeStrum ("Punk Downstroke", "d.d.d.d.d.d.d.d.",
                                   Subdivision::sixteenth, 0.5, "punk electric fast"));

    patterns.push_back (makeStrum ("Metal Chug", "DDDDDDDDDDDDDDDD",
                                   Subdivision::sixteenth, 0.5, "metal electric palm-mute",
                                   "llllllllllllllll"));

    patterns.push_back (makeStrum ("Djent Grid", "D.DD..D.DD..D.D.",
                                   Subdivision::sixteenth, 0.5, "metal djent palm-mute",
                                   "llllllllllllllll"));

    patterns.push_back (makeStrum ("Funk Sixteenth", "d.UuD.UuD.UuD.Uu",
                                   Subdivision::sixteenth, 0.5, "funk electric tight",
                                   "hhhhhhhhhhhhhhhh"));

    patterns.push_back (makeStrum ("Reggae Skank", "..u...u...u...u.",
                                   Subdivision::sixteenth, 0.5, "reggae offbeat",
                                   "..h...h...h...h."));

    patterns.push_back (makeStrum ("Freddie Green", "d...d...d...d...",
                                   Subdivision::sixteenth, 0.58, "jazz comping four-to-the-bar",
                                   "mmmmmmmmmmmmmmmm"));

    patterns.push_back (makeStrum ("La Pompe", "..D...d...D...d.",
                                   Subdivision::sixteenth, 0.5, "jazz gypsy"));

    patterns.push_back (makeStrum ("Bossa Comp", "d...u..d..u.d...",
                                   Subdivision::sixteenth, 0.5, "latin bossa soft"));

    patterns.push_back (makeStrum ("Blues Shuffle", "d.u.d.u.d.u.d.u.",
                                   Subdivision::sixteenth, 0.66, "blues shuffle swung"));

    patterns.push_back (makeStrum ("Country Rake", "r...d...r...d...",
                                   Subdivision::sixteenth, 0.55, "country blues rake"));

    patterns.push_back (makeStrum ("Flamenco Rasgueado", "R...R...R...R...",
                                   Subdivision::sixteenth, 0.5, "flamenco spanish"));

    patterns.push_back (makeStrum ("Bluegrass Flatpick", "d.udu.dud.udu.du",
                                   Subdivision::sixteenth, 0.5, "bluegrass acoustic fast"));

    patterns.push_back (makeStrum ("Ambient Swell", "d...............",
                                   Subdivision::sixteenth, 0.5, "ambient slow"));

    // The remaining strum patterns exist so that every genre kit in GenreKit.cpp
    // resolves to something the library actually holds.
    patterns.push_back (makeStrum ("Nashville Strum", "d...d.u.d.u.d.u.",
                                   Subdivision::sixteenth, 0.54, "country nashville acoustic"));

    patterns.push_back (makeStrum ("Modern Country", "d.u.D.u.d.u.D.u.",
                                   Subdivision::sixteenth, 0.5, "country modern electric"));

    patterns.push_back (makeStrum ("Chicago Shuffle", "d.u.D.u.d.u.D.u.",
                                   Subdivision::sixteenth, 0.66, "blues chicago shuffle swung"));

    patterns.push_back (makeStrum ("Modern Blues Rhythm", "d...D.u.d...D.u.",
                                   Subdivision::sixteenth, 0.58, "blues modern electric"));

    patterns.push_back (makeStrum ("Samba Comp", "d.u.du.ud.u.du.u",
                                   Subdivision::sixteenth, 0.5, "latin samba brazilian"));

    patterns.push_back (makeStrum ("Latin Ballad", "d.......u...d...",
                                   Subdivision::sixteenth, 0.5, "latin ballad soft"));

    patterns.push_back (makeStrum ("Rocksteady Skank", "..u...u...u...u.",
                                   Subdivision::sixteenth, 0.56, "reggae rocksteady offbeat",
                                   "..h...h...h...h."));

    patterns.push_back (makeStrum ("Dub Skank", "..u.......u.....",
                                   Subdivision::sixteenth, 0.5, "reggae dub sparse",
                                   "..h.......h....."));

    patterns.push_back (makeStrum ("Funk Wah Rhythm", "D.UuD.UuD.UuD.Uu",
                                   Subdivision::sixteenth, 0.5, "funk wah tight",
                                   "hhhhhhhhhhhhhhhh"));

    patterns.push_back (makeStrum ("Flamenco Alzapua", "d.ud.ud.ud.ud.ud",
                                   Subdivision::sixteenth, 0.5, "flamenco spanish alzapua",
                                   "llllllllllllllll"));


    // ---- fingerpick patterns -------------------------------------------------------
    // rhythm-engine 5 names these seven as the minimum ship set.
    patterns.push_back (makeFingerpick ("Travis Picking", "p.i.pmi.p.i.pmi.",
                                        Subdivision::sixteenth, 0.5,
                                        "folk travis alternating-bass",
                                        { 5, 2, 1, 0, 0 }));

    patterns.push_back (makeFingerpick ("Classical Ascending", "pima.pima.pima..",
                                        Subdivision::sixteenth, 0.5,
                                        "classical arpeggio ascending",
                                        { 5, 2, 1, 0, 0 }));

    patterns.push_back (makeFingerpick ("Classical Return", "pimami..pimami..",
                                        Subdivision::sixteenth, 0.5,
                                        "classical arpeggio return",
                                        { 5, 2, 1, 0, 0 }));

    patterns.push_back (makeFingerpick ("Boom Chick", "p.imp.imp.imp.im",
                                        Subdivision::sixteenth, 0.55,
                                        "country boom-chick thumb-brush",
                                        { 5, 2, 1, 0, 0 }));

    patterns.push_back (makeFingerpick ("Piedmont Blues", "p.impi.mp.impi.m",
                                        Subdivision::sixteenth, 0.62,
                                        "blues piedmont syncopated",
                                        { 5, 2, 1, 0, 0 }));

    patterns.push_back (makeFingerpick ("Modern Folk", "pimaimpaimpaimpa",
                                        Subdivision::sixteenth, 0.5,
                                        "folk modern sixteenth",
                                        { 5, 2, 1, 0, 0 }));

    patterns.push_back (makeFingerpick ("Fingerstyle Folk", "pm.i.ma.pm.i.ma.",
                                        Subdivision::sixteenth, 0.5,
                                        "folk fingerstyle",
                                        { 4, 2, 1, 0, 0 }));

    patterns.push_back (makeFingerpick ("Post-Rock Arpeggio", "pimaimpapimaimpa",
                                        Subdivision::sixteenth, 0.5,
                                        "ambient post-rock arpeggio",
                                        { 5, 2, 1, 0, 0 }));

    // These three exist so that every genre kit in GenreKit.cpp resolves.
    patterns.push_back (makeFingerpick ("Delta Blues Thumb", "p.imp.i.p.imp.i.",
                                        Subdivision::sixteenth, 0.66,
                                        "blues delta alternating-bass swung",
                                        { 5, 2, 1, 0, 0 }));

    patterns.push_back (makeFingerpick ("Contemporary Fingerstyle", "pimaimapimaimap.",
                                        Subdivision::sixteenth, 0.5,
                                        "folk contemporary fingerstyle",
                                        { 5, 2, 1, 0, 0 }));

    patterns.push_back (makeFingerpick ("Classical Etude", "pimimapipimimapi",
                                        Subdivision::sixteenth, 0.5,
                                        "classical etude study",
                                        { 5, 2, 1, 0, 0 }));
}

//==============================================================================
juce::File PatternLibrary::getFactoryDirectory()
{
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile)
             .getParentDirectory()
             .getChildFile ("Resources")
             .getChildFile ("Rhythm");
}

juce::File PatternLibrary::getUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("Rhythm");
}

void PatternLibrary::refresh()
{
    addFactoryPatterns();

    scanDirectory (getFactoryDirectory());
    scanDirectory (getUserDirectory());
}

void PatternLibrary::scanDirectory (const juce::File& directory)
{
    if (! directory.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.luthierpattern"))
    {
        RhythmPattern pattern;

        if (! pattern.loadFrom (entry.getFile()))
            continue;

        // A user pattern with the same name as a factory one replaces it, which
        // is how someone edits a factory pattern without losing the original.
        const int existing = indexOf (pattern.getName());

        if (existing >= 0)
            patterns[(size_t) existing] = pattern;
        else
            patterns.push_back (pattern);
    }
}

const RhythmPattern& PatternLibrary::getPattern (int index) const noexcept
{
    static const RhythmPattern empty;

    return juce::isPositiveAndBelow (index, (int) patterns.size())
             ? patterns[(size_t) index] : empty;
}

int PatternLibrary::indexOf (const juce::String& patternName) const
{
    for (size_t i = 0; i < patterns.size(); ++i)
        if (patterns[i].getName() == patternName)
            return (int) i;

    return -1;
}

juce::Array<int> PatternLibrary::findByTag (const juce::String& tag) const
{
    juce::Array<int> result;

    for (size_t i = 0; i < patterns.size(); ++i)
        if (patterns[i].getTags().contains (tag, true))
            result.add ((int) i);

    return result;
}

juce::Array<int> PatternLibrary::findByKind (RhythmPattern::Kind kind) const
{
    juce::Array<int> result;

    for (size_t i = 0; i < patterns.size(); ++i)
        if (patterns[i].getKind() == kind)
            result.add ((int) i);

    return result;
}

bool PatternLibrary::save (const RhythmPattern& pattern)
{
    const int existing = indexOf (pattern.getName());

    if (existing >= 0)
        patterns[(size_t) existing] = pattern;
    else
        patterns.push_back (pattern);

    const auto file = getUserDirectory()
                        .getChildFile (juce::File::createLegalFileName (pattern.getName())
                                         + ".luthierpattern");

    return pattern.saveTo (file);
}

} // namespace luthier
