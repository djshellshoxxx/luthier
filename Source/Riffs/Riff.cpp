#include "Riff.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace luthier
{

//==============================================================================
namespace RiffVocabulary
{
    const std::vector<Genre>& genres()
    {
        static const std::vector<Genre> list {
            { "rock",    "Rock",    "Rock" },
            { "blues",   "Blues",   "Blues" },
            { "metal",   "Metal",   "Metal" },
            { "funk",    "Funk",    "Funk" },
            { "country", "Country", "Country" },
            { "jazz",    "Jazz",    "Jazz" },
            { "folk",    "Folk",    "Folk / Acoustic" },
            { "reggae",  "Reggae",  "Reggae / Ska" },
            { "latin",   "Latin",   "Latin" },
            { "pop",     "Pop",     "Pop" },
            { "punk",    "Punk",    "Punk / Indie" },
            { "soul",    "Soul",    "Soul / R&B" },
        };
        return list;
    }

    int indexOfGenre (const juce::String& id)
    {
        const auto& list = genres();

        for (size_t i = 0; i < list.size(); ++i)
            if (id == list[i].id)
                return (int) i;

        return -1;
    }

    const juce::StringArray& types()
    {
        static const juce::StringArray list { "riff", "lick", "strum", "bass" };
        return list;
    }

    const juce::StringArray& instruments()
    {
        static const juce::StringArray list { "guitar6", "guitar7", "guitar12", "bass4", "bass5", "bass6" };
        return list;
    }

    int stringsForInstrument (const juce::String& instrument)
    {
        if (instrument == "guitar7") return 7;
        if (instrument == "bass4")   return 4;
        if (instrument == "bass5")   return 5;
        return 6;
    }

    bool isBassInstrument (const juce::String& instrument)
    {
        return instrument.startsWith ("bass");
    }

    namespace
    {
        using Type = ScoreTechnique::Type;

        struct TokenInfo { Type type; const char* token; };

        // MidiPerformance.cpp's kTechniques, in the same order.
        constexpr TokenInfo kTokens[] =
        {
            { Type::bend, "bend" }, { Type::bendRelease, "bendrelease" }, { Type::preBend, "prebend" },
            { Type::slideUp, "slideup" }, { Type::slideDown, "slidedown" }, { Type::slideLegato, "slidelegato" },
            { Type::slideShift, "slideshift" }, { Type::slideIn, "slidein" }, { Type::slideOut, "slideout" },
            { Type::hammerOn, "hammer" }, { Type::pullOff, "pull" }, { Type::palmMute, "pm" },
            { Type::deadNote, "dead" }, { Type::naturalHarmonic, "natural" }, { Type::pinchHarmonic, "pinch" },
            { Type::artificialHarmonic, "artificial" }, { Type::tapHarmonic, "tapharm" }, { Type::tap, "tap" },
            { Type::vibrato, "vibrato" }, { Type::trill, "trill" }, { Type::whammy, "whammy" },
            { Type::ghostNote, "ghost" }, { Type::accent, "accent" }, { Type::staccato, "staccato" },
            { Type::letRing, "letring" },
        };

        static_assert (sizeof (kTokens) / sizeof (kTokens[0]) == (size_t) Type::numTypes,
                       "every notation technique needs a riff token");
    }

    const juce::StringArray& noteTechniques()
    {
        static const juce::StringArray list = []
        {
            juce::StringArray l;

            for (const auto& t : kTokens)
                l.add (t.token);

            return l;
        }();

        return list;
    }

    const juce::StringArray& allTechniques()
    {
        static const juce::StringArray list = []
        {
            auto l = noteTechniques();
            l.addArray (juce::StringArray { "strum", "slap", "pop", "thump", "lhslap" });
            return l;
        }();

        return list;
    }

    bool typeForToken (const juce::String& token, ScoreTechnique::Type& type)
    {
        for (const auto& t : kTokens)
        {
            if (token == t.token)
            {
                type = t.type;
                return true;
            }
        }

        return false;
    }

    const char* tokenForType (ScoreTechnique::Type type)
    {
        for (const auto& t : kTokens)
            if (t.type == type)
                return t.token;

        return "bend";
    }

    juce::String techniqueDisplayName (const juce::String& token)
    {
        ScoreTechnique::Type type;

        if (typeForToken (token, type))
            return getTechniqueName (type);

        if (token == "strum")  return "Strum";
        if (token == "slap")   return "Slap";
        if (token == "pop")    return "Pop";
        if (token == "thump")  return "Double Thump";
        if (token == "lhslap") return "Left-Hand Slap";
        return token;
    }

    const juce::StringArray& scales()
    {
        static const juce::StringArray list {
            "ionian", "dorian", "phrygian", "lydian", "mixolydian", "aeolian", "locrian",
            "harmonic_minor", "melodic_minor", "major_pentatonic", "minor_pentatonic", "blues",
            "chromatic"
        };
        return list;
    }

    juce::String scaleDisplayName (const juce::String& scale)
    {
        if (scale == "ionian")  return "major";
        if (scale == "aeolian") return "minor";
        return scale.replaceCharacter ('_', ' ');
    }

    const juce::StringArray& feels()
    {
        static const juce::StringArray list { "straight", "shuffle", "swing", "half_time", "laid_back", "driving" };
        return list;
    }

    int pitchClassOfRoot (const juce::String& root)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const char* flats[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

        for (int i = 0; i < 12; ++i)
            if (root == names[i] || root == flats[i])
                return i;

        return -1;
    }

    juce::String rootName (int pitchClass)
    {
        static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
        return names[((pitchClass % 12) + 12) % 12];
    }
}

//==============================================================================
namespace RiffJson
{
    juce::String formatReal (double value)
    {
        if (! std::isfinite (value))
            value = 0.0;

        char buffer[64];
        std::snprintf (buffer, sizeof (buffer), "%.6f", value);
        juce::String s (buffer);

        if (s.containsChar ('.'))
            s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

        if (s == "-0" || s.isEmpty())
            s = "0";

        return s;
    }

    juce::String quote (const juce::String& text)
    {
        // json.dumps (ensure_ascii=True), exactly.
        juce::String out ("\"");
        auto p = text.getCharPointer();

        for (;;)
        {
            const auto c = (juce::uint32) p.getAndAdvance();

            if (c == 0)
                break;

            auto hex4 = [&out] (juce::uint32 v)
            {
                out << "\\u" << juce::String::toHexString ((int) v).paddedLeft ('0', 4);
            };

            switch (c)
            {
                case '"':  out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                case '\b': out << "\\b"; break;
                case '\f': out << "\\f"; break;
                default:
                    if (c < 0x20 || c > 0x7e)
                    {
                        if (c == 0x7f)
                        {
                            out << juce::String::charToString ((juce::juce_wchar) c);
                        }
                        else if (c > 0xffff)
                        {
                            const auto v = c - 0x10000;
                            hex4 (0xd800 + (v >> 10));
                            hex4 (0xdc00 + (v & 0x3ff));
                        }
                        else
                        {
                            hex4 (c);
                        }
                    }
                    else
                    {
                        out << juce::String::charToString ((juce::juce_wchar) c);
                    }
                    break;
            }
        }

        return out + "\"";
    }

    namespace
    {
        bool isScalar (const juce::var& v)
        {
            return ! v.isArray() && v.getDynamicObject() == nullptr;
        }

        juce::String number (const juce::var& v)
        {
            if (v.isBool())
                return (bool) v ? "true" : "false";

            if (v.isInt() || v.isInt64())
                return juce::String ((juce::int64) v);

            return formatReal ((double) v);
        }

        juce::String emit (const juce::var& v, int indent, bool inlined)
        {
            if (auto* object = v.getDynamicObject())
            {
                std::vector<juce::String> keys;

                for (const auto& property : object->getProperties())
                    keys.push_back (property.name.toString());

                std::sort (keys.begin(), keys.end(),
                           [] (const juce::String& a, const juce::String& b) { return a.compare (b) < 0; });

                juce::String out;

                if (inlined || keys.empty())
                {
                    out << "{";

                    for (size_t i = 0; i < keys.size(); ++i)
                        out << (i > 0 ? "," : "") << quote (keys[i]) << ":"
                            << emit (object->getProperty (keys[i]), 0, true);

                    return out + "}";
                }

                const juce::String pad = juce::String::repeatedString (" ", indent + 1);
                out << "{\n";

                for (size_t i = 0; i < keys.size(); ++i)
                    out << (i > 0 ? ",\n" : "") << pad << quote (keys[i]) << ": "
                        << emit (object->getProperty (keys[i]), indent + 1, false);

                return out + "\n" + juce::String::repeatedString (" ", indent) + "}";
            }

            if (auto* array = v.getArray())
            {
                if (array->isEmpty())
                    return "[]";

                bool flat = true;

                for (const auto& e : *array)
                    if (! (isScalar (e) || e.isArray()))
                        flat = false;

                juce::String out;

                if (inlined || flat)
                {
                    out << "[";

                    for (int i = 0; i < array->size(); ++i)
                        out << (i > 0 ? "," : "") << emit (array->getReference (i), 0, true);

                    return out + "]";
                }

                const juce::String pad = juce::String::repeatedString (" ", indent + 1);
                out << "[\n";

                for (int i = 0; i < array->size(); ++i)
                    out << (i > 0 ? ",\n" : "") << pad << emit (array->getReference (i), indent + 1, true);

                return out + "\n" + juce::String::repeatedString (" ", indent) + "]";
            }

            if (v.isString())
                return quote (v.toString());

            if (v.isVoid() || v.isUndefined())
                return "null";

            return number (v);
        }
    }

    juce::String write (const juce::var& value)
    {
        return emit (value, 0, false) + "\n";
    }
}

//==============================================================================
bool RiffStrum::operator== (const RiffStrum& o) const noexcept
{
    return juce::exactlyEqual (beat, o.beat) && down == o.down && juce::exactlyEqual (cv, o.cv)
        && mask == o.mask && striker == o.striker && juce::exactlyEqual (mute, o.mute);
}

bool RiffBassTech::operator== (const RiffBassTech& o) const noexcept
{
    return juce::exactlyEqual (beat, o.beat) && str == o.str && tech == o.tech
        && juce::exactlyEqual (pos, o.pos) && juce::exactlyEqual (force, o.force);
}

//==============================================================================
double Riff::getBeatsPerBar() const noexcept
{
    return (double) juce::jmax (1, meterNumerator) * 4.0 / (double) juce::jmax (1, meterDenominator);
}

int Riff::getNumBars() const noexcept
{
    return juce::jmax (1, (int) std::ceil (lengthBeats / getBeatsPerBar() - 1.0e-6));
}

int Riff::pitchOf (int stringIndex, int fret) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, (int) tuning.size()))
        return 60;

    return tuning[(size_t) stringIndex] + capo + fret;
}

void Riff::updatePitches()
{
    for (auto& n : notes)
    {
        n.midiNote = juce::jlimit (0, 127, pitchOf (n.stringIndex, n.fret));
        n.pitchHz = 440.0 * std::pow (2.0, (n.midiNote - 69) / 12.0);
    }
}

juce::StringArray Riff::computeTechniques() const
{
    std::vector<bool> used ((size_t) ScoreTechnique::Type::numTypes, false);

    for (const auto& n : notes)
        for (const auto& t : n.techniques)
            used[(size_t) t.type] = true;

    juce::StringArray result;

    for (const auto& token : RiffVocabulary::noteTechniques())
    {
        ScoreTechnique::Type type;

        if (RiffVocabulary::typeForToken (token, type) && used[(size_t) type])
            result.add (token);
    }

    if (! strums.empty())
        result.add ("strum");

    for (const char* bass : { "slap", "pop", "thump", "lhslap" })
        for (const auto& b : bassTech)
            if (b.tech == bass)
            {
                result.add (bass);
                break;
            }

    return result;
}

PerformanceScore Riff::toScore (const juce::String& title) const
{
    PerformanceScore score;
    auto& m = score.getMeta();
    m.title = title.isNotEmpty() ? title : meta.name;
    m.tempoBpm = tempoBpm;
    m.timeSignatureNumerator = meterNumerator;
    m.timeSignatureDenominator = meterDenominator;
    m.key = keyRoot + " " + RiffVocabulary::scaleDisplayName (scale);
    m.tuningName = tuningName;

    // A new score already has its first track; the riff is that track (part 0).
    auto& track = score.getNumTracks() > 0 ? score.getTrack (0) : score.addTrack();
    track.name = isBass() ? "Bass" : "Guitar";
    track.capoFret = capo;
    track.numStrings = juce::jlimit (1, kMaxStrings, getNumStrings());
    track.tuning.fill (0);

    for (int s = 0; s < track.numStrings; ++s)
        track.tuning[(size_t) s] = tuning[(size_t) s];

    const double bar = getBeatsPerBar();
    const int bars = getNumBars();

    for (int b = 0; b < bars; ++b)
    {
        ScoreMeasure measure;
        measure.timeSignatureNumerator = meterNumerator;
        measure.timeSignatureDenominator = meterDenominator;
        measure.voices.resize (1);
        track.measures.push_back (std::move (measure));
    }

    for (const auto& n : notes)
    {
        const int b = juce::jlimit (0, bars - 1, (int) std::floor (n.startBeat / bar + 1.0e-9));
        auto copy = n;
        copy.startBeat = n.startBeat - b * bar;
        track.measures[(size_t) b].voices[0].notes.push_back (std::move (copy));
    }

    for (const auto& c : chords)
    {
        const int b = juce::jlimit (0, bars - 1, (int) std::floor (c.beat / bar + 1.0e-9));
        track.measures[(size_t) b].chordSymbols.emplace_back (c.beat - b * bar, c.symbol);
    }

    return score;
}

//==============================================================================
namespace
{
    juce::var object() { return juce::var (new juce::DynamicObject()); }

    void set (juce::var& o, const char* key, const juce::var& value)
    {
        o.getDynamicObject()->setProperty (key, value);
    }

    juce::var real (double v)
    {
        // What the file holds: six decimals. An integral value is an int, as
        // the generator's r6() makes it.
        const double r = std::round (v * 1.0e6) / 1.0e6;

        if (std::abs (r) < 1.0e15 && juce::exactlyEqual (r, std::round (r)))
            return juce::var ((juce::int64) std::llround (r));

        return juce::var (r);
    }

    juce::var strings (const juce::StringArray& list)
    {
        juce::Array<juce::var> a;

        for (const auto& s : list)
            a.add (s);

        return a;
    }

    int tokenIndex (ScoreTechnique::Type type)
    {
        return RiffVocabulary::noteTechniques().indexOf (RiffVocabulary::tokenForType (type));
    }
}

juce::String Riff::toJson() const
{
    auto metaObject = object();
    set (metaObject, "id", meta.id);
    set (metaObject, "name", meta.name);
    set (metaObject, "author", meta.author);
    set (metaObject, "origin", meta.origin);
    set (metaObject, "tags", strings (meta.tags));
    set (metaObject, "created", meta.created);
    set (metaObject, "modified", meta.modified);
    set (metaObject, "version_created", meta.versionCreated);
    set (metaObject, "version_modified", meta.versionModified);
    set (metaObject, "notes", meta.notes);

    auto riffObject = object();
    set (riffObject, "type", type);
    set (riffObject, "genre", genre);
    set (riffObject, "instrument", instrument);

    juce::Array<juce::var> tuningArray;
    for (auto t : tuning)
        tuningArray.add (t);

    set (riffObject, "tuning", tuningArray);
    set (riffObject, "tuning_name", tuningName);
    set (riffObject, "capo", capo);

    auto keyObject = object();
    set (keyObject, "root", keyRoot);
    set (keyObject, "scale", scale);
    set (riffObject, "key", keyObject);

    set (riffObject, "tempo_bpm", real (tempoBpm));
    set (riffObject, "meter", juce::Array<juce::var> { meterNumerator, meterDenominator });
    set (riffObject, "length_beats", real (lengthBeats));
    set (riffObject, "feel", feel);
    set (riffObject, "difficulty", difficulty);
    set (riffObject, "techniques", strings (techniques));

    juce::Array<juce::var> chordArray;
    for (const auto& c : chords)
    {
        auto o = object();
        set (o, "beat", real (c.beat));
        set (o, "symbol", c.symbol);
        chordArray.add (o);
    }
    set (riffObject, "chords", chordArray);

    // Notes as the generator orders them: by start, then string.
    std::vector<const ScoreNote*> ordered;
    for (const auto& n : notes)
        ordered.push_back (&n);

    std::stable_sort (ordered.begin(), ordered.end(), [] (const ScoreNote* a, const ScoreNote* b)
    {
        const auto ra = std::round (a->startBeat * 1.0e6), rb = std::round (b->startBeat * 1.0e6);
        return ra != rb ? ra < rb : a->stringIndex < b->stringIndex;
    });

    juce::Array<juce::var> noteArray;

    for (const auto* n : ordered)
    {
        auto o = object();
        set (o, "beat", real (n->startBeat));
        set (o, "dur", real (n->durationBeats));
        set (o, "str", n->stringIndex);
        set (o, "fret", n->fret);
        set (o, "vel", real (n->velocity));

        std::vector<const ScoreTechnique*> techs;
        for (const auto& t : n->techniques)
            techs.push_back (&t);

        std::stable_sort (techs.begin(), techs.end(), [] (const ScoreTechnique* a, const ScoreTechnique* b)
        {
            return tokenIndex (a->type) < tokenIndex (b->type);
        });

        juce::Array<juce::var> techArray;

        for (const auto* t : techs)
        {
            auto to = object();
            set (to, "type", RiffVocabulary::tokenForType (t->type));
            set (to, "value", real (t->value));
            set (to, "second", real (t->secondValue));

            if (! t->curve.empty())
            {
                juce::Array<juce::var> curve;

                for (const auto& [position, semitones] : t->curve)
                    curve.add (juce::Array<juce::var> { real (position), real (semitones) });

                set (to, "curve", curve);
            }

            techArray.add (to);
        }

        set (o, "tech", techArray);
        noteArray.add (o);
    }

    juce::Array<juce::var> strumArray;
    for (const auto& s : strums)
    {
        auto o = object();
        set (o, "beat", real (s.beat));
        set (o, "dir", s.down ? "down" : "up");
        set (o, "cv", real (s.cv));
        set (o, "mask", s.mask);
        set (o, "striker", s.striker);
        set (o, "mute", real (s.mute));
        strumArray.add (o);
    }

    juce::Array<juce::var> bassArray;
    for (const auto& b : bassTech)
    {
        auto o = object();
        set (o, "beat", real (b.beat));
        set (o, "str", b.str);
        set (o, "tech", b.tech);
        set (o, "pos", real (b.pos));
        set (o, "force", real (b.force));
        bassArray.add (o);
    }

    auto root = object();
    set (root, "schema", 1);
    set (root, "magic", "luthier.riff");
    set (root, "meta", metaObject);
    set (root, "riff", riffObject);
    set (root, "notes", noteArray);
    set (root, "strums", strumArray);
    set (root, "bass_tech", bassArray);
    set (root, "source", source);

    return RiffJson::write (root);
}

//==============================================================================
namespace
{
    bool isNumber (const juce::var& v) { return v.isInt() || v.isInt64() || v.isDouble(); }

    double num (const juce::var& v, double fallback = 0.0)
    {
        if (! isNumber (v))
            return fallback;

        const double d = (double) v;
        return std::isfinite (d) ? d : fallback;
    }

    juce::String str (const juce::var& v, const juce::String& fallback = {})
    {
        return v.isString() ? v.toString() : fallback;
    }

    juce::StringArray stringList (const juce::var& v)
    {
        juce::StringArray out;

        if (auto* a = v.getArray())
            for (const auto& e : *a)
                if (e.isString())
                    out.add (e.toString());

        return out;
    }
}

juce::Result Riff::fromJson (const juce::String& text, Riff& out, juce::StringArray* warnings)
{
    juce::var root;
    const auto parsed = juce::JSON::parse (text, root);

    if (parsed.failed())
        return juce::Result::fail ("not JSON: " + parsed.getErrorMessage());

    if (root.getDynamicObject() == nullptr)
        return juce::Result::fail ("not a riff");

    if (str (root["magic"]) != "luthier.riff")
        return juce::Result::fail ("wrong magic");

    if (! isNumber (root["schema"]) || num (root["schema"]) < 1.0)
        return juce::Result::fail ("no schema");

    const auto& metaVar = root["meta"];
    const auto& riffVar = root["riff"];

    if (metaVar.getDynamicObject() == nullptr || riffVar.getDynamicObject() == nullptr)
        return juce::Result::fail ("missing meta or riff");

    Riff r;
    r.meta.id = str (metaVar["id"]);
    r.meta.name = str (metaVar["name"]);
    r.meta.author = str (metaVar["author"]);
    r.meta.origin = str (metaVar["origin"]);
    r.meta.tags = stringList (metaVar["tags"]);
    r.meta.created = str (metaVar["created"]);
    r.meta.modified = str (metaVar["modified"]);
    r.meta.versionCreated = str (metaVar["version_created"]);
    r.meta.versionModified = str (metaVar["version_modified"]);
    r.meta.notes = str (metaVar["notes"]);

    if (r.meta.id.isEmpty() || r.meta.name.isEmpty())
        return juce::Result::fail ("missing id or name");

    r.type = str (riffVar["type"], "lick");
    r.genre = str (riffVar["genre"]);
    r.instrument = str (riffVar["instrument"], "guitar6");

    if (! RiffVocabulary::types().contains (r.type))
        return juce::Result::fail ("unknown type " + r.type);

    if (! RiffVocabulary::instruments().contains (r.instrument))
        return juce::Result::fail ("unknown instrument " + r.instrument);

    r.tuning.clear();

    if (auto* t = riffVar["tuning"].getArray())
        for (const auto& e : *t)
            if (isNumber (e))
                r.tuning.push_back (juce::jlimit (0, 127, (int) num (e)));

    if (r.tuning.empty() || (int) r.tuning.size() > kMaxStrings)
        return juce::Result::fail ("bad tuning");

    r.tuningName = str (riffVar["tuning_name"], "Standard");
    r.capo = juce::jlimit (0, 24, (int) num (riffVar["capo"]));

    r.keyRoot = str (riffVar["key"]["root"], "C");
    r.scale = str (riffVar["key"]["scale"], "chromatic");

    if (RiffVocabulary::pitchClassOfRoot (r.keyRoot) < 0)
        return juce::Result::fail ("unknown key root " + r.keyRoot);

    if (! RiffVocabulary::scales().contains (r.scale))
        return juce::Result::fail ("unknown scale " + r.scale);

    r.tempoBpm = num (riffVar["tempo_bpm"], -1.0);

    if (r.tempoBpm < kMinTempo || r.tempoBpm > kMaxTempo)
        return juce::Result::fail ("tempo out of range");

    if (auto* m = riffVar["meter"].getArray(); m != nullptr && m->size() == 2)
    {
        r.meterNumerator = (int) num (m->getReference (0), 4.0);
        r.meterDenominator = (int) num (m->getReference (1), 4.0);
    }

    if (r.meterNumerator < 1 || r.meterNumerator > 32
          || ! (r.meterDenominator == 1 || r.meterDenominator == 2 || r.meterDenominator == 4
                || r.meterDenominator == 8 || r.meterDenominator == 16))
        return juce::Result::fail ("bad meter");

    r.lengthBeats = num (riffVar["length_beats"], -1.0);

    if (r.lengthBeats <= 0.0 || r.lengthBeats > kMaxBeats)
        return juce::Result::fail ("length out of range");

    r.feel = str (riffVar["feel"], "straight");
    r.difficulty = juce::jlimit (1, 5, (int) num (riffVar["difficulty"], 1.0));
    r.techniques = stringList (riffVar["techniques"]);

    if (auto* c = riffVar["chords"].getArray())
        for (const auto& e : *c)
            if (e.getDynamicObject() != nullptr)
                r.chords.push_back ({ num (e["beat"]), str (e["symbol"]) });

    const auto* noteArray = root["notes"].getArray();

    if (noteArray != nullptr && noteArray->size() > kMaxNotes)
        return juce::Result::fail ("too many notes");

    const int numStrings = r.getNumStrings();

    if (noteArray != nullptr)
    {
        r.notes.reserve ((size_t) noteArray->size());

        for (const auto& e : *noteArray)
        {
            if (e.getDynamicObject() == nullptr)
                return juce::Result::fail ("bad note");

            ScoreNote n;
            n.startBeat = num (e["beat"], -1.0);
            n.durationBeats = num (e["dur"], -1.0);
            n.stringIndex = (int) num (e["str"], -1.0);
            n.fret = (int) num (e["fret"], -1.0);
            n.velocity = juce::jlimit (0.0, 1.0, num (e["vel"], 0.8));

            if (n.startBeat < 0.0 || n.startBeat > kMaxBeats || n.durationBeats < 0.0 || n.durationBeats > kMaxBeats)
                return juce::Result::fail ("note out of range");

            if (! juce::isPositiveAndBelow (n.stringIndex, numStrings))
                return juce::Result::fail ("note on a string the instrument does not have");

            if (n.fret < 0 || n.fret > kMaxFret)
                return juce::Result::fail ("fret out of range");

            if (auto* techs = e["tech"].getArray())
            {
                for (const auto& t : *techs)
                {
                    ScoreTechnique technique;

                    if (! RiffVocabulary::typeForToken (str (t["type"]), technique.type))
                    {
                        if (warnings != nullptr)
                            warnings->addIfNotAlreadyThere ("unknown technique " + str (t["type"]));
                        continue;
                    }

                    technique.value = num (t["value"]);
                    technique.secondValue = num (t["second"]);

                    if (auto* curve = t["curve"].getArray())
                        for (const auto& point : *curve)
                            if (auto* p = point.getArray(); p != nullptr && p->size() == 2)
                                technique.curve.emplace_back (num (p->getReference (0)), num (p->getReference (1)));

                    n.techniques.push_back (std::move (technique));
                }
            }

            r.notes.push_back (std::move (n));
        }
    }

    if (auto* s = root["strums"].getArray())
    {
        for (const auto& e : *s)
        {
            RiffStrum strum;
            strum.beat = num (e["beat"]);
            strum.down = str (e["dir"], "down") != "up";
            strum.cv = juce::jlimit (1.0, 10000.0, num (e["cv"], 200.0));
            strum.mask = (int) num (e["mask"]);
            strum.striker = str (e["striker"], "pick");
            strum.mute = num (e["mute"]);
            r.strums.push_back (strum);
        }
    }

    if (auto* b = root["bass_tech"].getArray())
    {
        for (const auto& e : *b)
        {
            RiffBassTech bass;
            bass.beat = num (e["beat"]);
            bass.str = juce::jlimit (0, numStrings - 1, (int) num (e["str"]));
            bass.tech = str (e["tech"], "slap");
            bass.pos = num (e["pos"], 0.5);
            bass.force = num (e["force"], 0.8);
            r.bassTech.push_back (bass);
        }
    }

    r.source = str (root["source"]);
    r.updatePitches();

    const auto recomputed = r.computeTechniques();

    if (recomputed != r.techniques && warnings != nullptr)
        warnings->add (r.meta.id + ": stored techniques differ from the notes; using the notes");

    out = std::move (r);
    return juce::Result::ok();
}

juce::Result Riff::loadFromFile (const juce::File& file, Riff& out, juce::StringArray* warnings)
{
    // file-formats 14: nothing is read past a sane size (64 KB of notes is
    // far beyond the 4096-note limit anyway).
    if (! file.existsAsFile())
        return juce::Result::fail ("missing " + file.getFullPathName());

    if (file.getSize() > 4 * 1024 * 1024)
        return juce::Result::fail ("oversized " + file.getFileName());

    return fromJson (file.loadFileAsString(), out, warnings);
}

juce::Result Riff::saveToFile (const juce::File& file) const
{
    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail ("cannot create " + file.getParentDirectory().getFullPathName());

    juce::TemporaryFile temp (file);

    if (! temp.getFile().replaceWithText (toJson(), false, false, "\n"))
        return juce::Result::fail ("cannot write " + temp.getFile().getFullPathName());

    if (! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("cannot replace " + file.getFullPathName());

    return juce::Result::ok();
}

} // namespace luthier
