#include "TuneFile.h"
#include "../Support/ThreadProbe.h"

#include <cmath>

namespace luthier
{

using namespace tunetheory;

//==============================================================================
const char* getTuneLoadErrorName (TuneLoadError error) noexcept
{
    switch (error)
    {
        case TuneLoadError::none:              return "None";
        case TuneLoadError::fileNotFound:      return "FileNotFound";
        case TuneLoadError::unreadable:        return "Unreadable";
        case TuneLoadError::tooLarge:          return "TooLarge";
        case TuneLoadError::notUtf8:           return "NotUtf8";
        case TuneLoadError::malformedJson:     return "MalformedJson";
        case TuneLoadError::badMagic:          return "BadMagic";
        case TuneLoadError::unsupportedSchema: return "UnsupportedSchema";
        case TuneLoadError::missingField:      return "MissingField";
        case TuneLoadError::invalidField:      return "InvalidField";
    }

    return "Unknown";
}

//==============================================================================
// Writing
//==============================================================================
namespace
{
    /** Every double goes through here, which is what makes the text canonical. */
    juce::var num (double value)
    {
        return juce::var (canonical (value));
    }

    juce::var stringsToVar (const juce::StringArray& strings)
    {
        juce::Array<juce::var> items;

        for (const auto& s : strings)
            items.add (s);

        return juce::var (items);
    }

    /** Unknown fields go after the known ones, in the order they were read. */
    void appendExtras (juce::DynamicObject& object, const juce::NamedValueSet& extra)
    {
        for (const auto& field : extra)
            if (! object.hasProperty (field.name))
                object.setProperty (field.name, field.value);
    }

    juce::var noteToVar (const MelodyNote& n)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty ("start", num (n.startBeat));
        o->setProperty ("dur", num (n.durationBeats));
        o->setProperty ("pitch", n.pitch.toVar());
        o->setProperty ("vel", n.velocity);

        // Defaults are left out: a melody is most of a tune's bytes, and a
        // note is usually just where, how long, what and how hard.
        if (n.articulation != NoteArticulation::inherit)
            o->setProperty ("articulation", getNoteArticulationName (n.articulation));

        if (n.technique != NoteTechnique::none)
            o->setProperty ("technique", getNoteTechniqueName (n.technique));

        if (n.locked)
            o->setProperty ("locked", true);

        appendExtras (*o, n.extra);
        return juce::var (o);
    }

    juce::var notesToVar (const std::vector<MelodyNote>& notes)
    {
        juce::Array<juce::var> items;

        for (const auto& n : notes)
            items.add (noteToVar (n));

        return juce::var (items);
    }

    juce::var chordToVar (const ChordCell& c, bool flats)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty ("root", spellPitchClass (c.root, flats));
        o->setProperty ("quality", qualityToFileName (c.quality));
        o->setProperty ("beats", num (c.durationBeats));

        if (c.bass >= 0)
            o->setProperty ("bass", spellPitchClass (c.bass, flats));

        if (! c.extensions.isEmpty())
            o->setProperty ("extensions", stringsToVar (c.extensions));

        if (c.strumOverride.isNotEmpty())
            o->setProperty ("strum", c.strumOverride);

        if (c.emphasis != ChordEmphasis::normal)
            o->setProperty ("emphasis", getChordEmphasisName (c.emphasis));

        if (c.locked)
            o->setProperty ("locked", true);

        appendExtras (*o, c.extra);
        return juce::var (o);
    }

    juce::var melodyTrackToVar (const MelodyTrack& t)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty ("string_hint", t.stringHint <= 0 ? juce::var ("auto") : juce::var (t.stringHint));
        o->setProperty ("articulation", getNoteArticulationName (t.articulationDefault));
        o->setProperty ("source", getMelodySourceName (t.source));
        o->setProperty ("seed", t.seed);
        o->setProperty ("density", num (t.density));
        o->setProperty ("range_low", t.rangeLow);
        o->setProperty ("range_high", t.rangeHigh);
        o->setProperty ("follow_chords", t.followChords);

        appendExtras (*o, t.extra);
        return juce::var (o);
    }

    juce::var bassToVar (const BassTrack& b)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty ("mode", getBassModeName (b.mode));

        if (! b.notes.empty())
            o->setProperty ("notes", notesToVar (b.notes));

        appendExtras (*o, b.extra);
        return juce::var (o);
    }

    juce::var layerToVar (const TuneLayer& l)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty ("type", getLayerTypeName (l.type));
        o->setProperty ("on", l.enabled);
        o->setProperty ("volume", num (l.volume));
        o->setProperty ("pan", num (l.pan));
        o->setProperty ("pattern", l.patternId);
        o->setProperty ("seed", l.seed);

        if (! l.notes.empty())
            o->setProperty ("notes", notesToVar (l.notes));

        appendExtras (*o, l.extra);
        return juce::var (o);
    }

    juce::var sectionToVar (const TuneSection& s, bool flats)
    {
        auto* o = new juce::DynamicObject();

        // tune-builder 11's order first; everything the example abridges after.
        o->setProperty ("name", s.name);
        o->setProperty ("length_bars", s.lengthBars);
        o->setProperty ("rhythm_pattern", s.rhythmPatternId);
        o->setProperty ("genre_kit", s.genreKitId);

        juce::Array<juce::var> chords;

        for (const auto& c : s.chords)
            chords.add (chordToVar (c, flats));

        o->setProperty ("chords", juce::var (chords));

        if (s.melody.has_value())
        {
            o->setProperty ("melody", notesToVar (s.melody->notes));
            o->setProperty ("melody_track", melodyTrackToVar (*s.melody));
        }

        if (s.bass.mode != BassMode::off || ! s.bass.notes.empty() || s.bass.extra.size() > 0)
            o->setProperty ("bass", bassToVar (s.bass));

        if (! s.layers.empty())
        {
            juce::Array<juce::var> layers;

            for (const auto& l : s.layers)
                layers.add (layerToVar (l));

            o->setProperty ("layers", juce::var (layers));
        }

        o->setProperty ("role", getSectionRoleName (s.role));
        o->setProperty ("rhythm_on", s.rhythmOn);
        o->setProperty ("rhythm_link", s.rhythmLinkedTo);
        o->setProperty ("feel", num (s.feel));
        o->setProperty ("strum", num (s.strum));
        o->setProperty ("state_boundary", s.stateBoundary);
        o->setProperty ("style", getMelodyStyleName (s.style));

        appendExtras (*o, s.extra);
        return juce::var (o);
    }

    juce::var setlistToVar (const std::vector<TuneSetlistEntry>& setlist)
    {
        juce::Array<juce::var> items;

        for (const auto& e : setlist)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("section", e.section);
            o->setProperty ("repeats", e.repeats);
            appendExtras (*o, e.extra);
            items.add (juce::var (o));
        }

        return juce::var (items);
    }

    juce::var sectionsToVar (const std::vector<TuneSection>& sections, bool flats)
    {
        juce::Array<juce::var> items;

        for (const auto& s : sections)
            items.add (sectionToVar (s, flats));

        return juce::var (items);
    }

    juce::var metaToVar (const TuneMeta& m)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty ("title", m.title);
        o->setProperty ("artist", m.artist);
        o->setProperty ("author", m.author);
        o->setProperty ("tempo_bpm", num (m.tempoBpm));
        o->setProperty ("time_sig", juce::String (m.timeSigNumerator) + "/" + juce::String (m.timeSigDenominator));
        o->setProperty ("key", formatKey (m.keyTonic, m.mode));
        o->setProperty ("mode", getTuneModeName (m.mode));
        o->setProperty ("swing", num (m.swingPercent));
        o->setProperty ("feel_pct", num (m.feelPercent));
        o->setProperty ("tags", stringsToVar (m.tags));
        o->setProperty ("notes", m.notes);
        o->setProperty ("created", m.created);
        o->setProperty ("modified", m.modified);

        appendExtras (*o, m.extra);
        return juce::var (o);
    }
}

juce::var TuneFile::toVar (const Tune& tune)
{
    const bool flats = tune.preferFlats();

    auto* root = new juce::DynamicObject();

    root->setProperty ("schema", kSchemaVersion);
    root->setProperty ("magic", juce::String (kMagic));
    root->setProperty ("meta", metaToVar (tune.meta));
    root->setProperty ("sections", sectionsToVar (tune.arrangement.sections, flats));
    root->setProperty ("setlist", setlistToVar (tune.arrangement.setlist));

    if (! tune.variations.empty())
    {
        juce::Array<juce::var> items;

        for (const auto& v : tune.variations)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", v.name);
            o->setProperty ("sections", sectionsToVar (v.arrangement.sections, flats));
            o->setProperty ("setlist", setlistToVar (v.arrangement.setlist));
            appendExtras (*o, v.extra);
            items.add (juce::var (o));
        }

        root->setProperty ("variations", juce::var (items));
    }

    appendExtras (*root, tune.extra);
    return juce::var (root);
}

juce::String TuneFile::toJson (const Tune& tune)
{
    // JUCE writes CRLF; the file is LF on every platform so that the same tune
    // is the same bytes wherever it was saved.
    return juce::JSON::toString (toVar (tune), false).replace ("\r\n", "\n") + "\n";
}

//==============================================================================
// Reading
//==============================================================================
namespace
{
    struct Reader
    {
        TuneLoadResult result;

        bool failed() const noexcept { return result.error != TuneLoadError::none; }

        bool fail (TuneLoadError error, const juce::String& path, const juce::String& why)
        {
            if (! failed())
            {
                result.error = error;
                result.message = path + ": " + why;
            }

            return false;
        }

        void warn (const juce::String& text) { result.warnings.add (text); }
    };

    juce::String field (const juce::String& path, const char* key)
    {
        return path.isEmpty() ? juce::String (key) : path + "." + key;
    }

    juce::String element (const juce::String& path, int index)
    {
        return path + "[" + juce::String (index) + "]";
    }

    bool isNumber (const juce::var& v) noexcept
    {
        return v.isInt() || v.isInt64() || v.isDouble();
    }

    /** Strict UTF-8 (file-formats 14.2). CharPointer_UTF8::isValidString is
        not enough on its own: it accepts a lead byte followed by a byte that
        is not a continuation byte (0xC3 0x28), and stops at a NUL. */
    bool isValidUtf8 (const unsigned char* bytes, size_t numBytes) noexcept
    {
        size_t i = 0;

        while (i < numBytes)
        {
            const unsigned char lead = bytes[i];

            if (lead == 0)
                return false;

            if (lead < 0x80)
            {
                ++i;
                continue;
            }

            size_t extra = 0;
            uint32_t codePoint = 0;

            if (lead >= 0xC2 && lead <= 0xDF)      { extra = 1; codePoint = lead & 0x1Fu; }
            else if (lead >= 0xE0 && lead <= 0xEF) { extra = 2; codePoint = lead & 0x0Fu; }
            else if (lead >= 0xF0 && lead <= 0xF4) { extra = 3; codePoint = lead & 0x07u; }
            else return false;

            if (numBytes - i <= extra)
                return false;

            for (size_t k = 1; k <= extra; ++k)
            {
                const unsigned char next = bytes[i + k];

                if ((next & 0xC0u) != 0x80u)
                    return false;

                codePoint = (codePoint << 6) | (uint32_t) (next & 0x3Fu);
            }

            const bool overlong = (extra == 2 && codePoint < 0x800u) || (extra == 3 && codePoint < 0x10000u);
            const bool surrogate = codePoint >= 0xD800u && codePoint <= 0xDFFFu;

            if (overlong || surrogate || codePoint > 0x10FFFFu)
                return false;

            i += extra + 1;
        }

        return true;
    }

    /** The fields of `object` not in `known`, in file order (file-formats 0.3). */
    juce::NamedValueSet unknownFields (const juce::DynamicObject& object, std::initializer_list<const char*> known)
    {
        juce::NamedValueSet extra;

        for (const auto& property : object.getProperties())
        {
            bool isKnown = false;

            for (auto* k : known)
            {
                if (property.name.toString() == k)
                {
                    isKnown = true;
                    break;
                }
            }

            if (! isKnown)
                extra.set (property.name, property.value);
        }

        return extra;
    }

    //==========================================================================
    // A missing field keeps its default and succeeds; a present field of the
    // wrong type fails the load. A number out of range is clamped and warned
    // about, because a tempo of 900 is a typo, not a corrupt file.

    bool readString (Reader& r, const juce::DynamicObject& o, const char* key, const juce::String& path,
                     juce::String& out, bool required = false)
    {
        if (! o.hasProperty (key))
            return required ? r.fail (TuneLoadError::missingField, field (path, key), "missing") : true;

        const auto& v = o.getProperty (key);

        if (! v.isString())
            return r.fail (TuneLoadError::invalidField, field (path, key), "not a string");

        out = v.toString();
        return true;
    }

    bool readDouble (Reader& r, const juce::DynamicObject& o, const char* key, const juce::String& path,
                     double& out, double low, double high)
    {
        if (! o.hasProperty (key))
            return true;

        const auto& v = o.getProperty (key);

        if (! isNumber (v) || ! std::isfinite ((double) v))
            return r.fail (TuneLoadError::invalidField, field (path, key), "not a number");

        const double value = (double) v;
        const double clamped = juce::jlimit (low, high, value);

        if (clamped != value)
            r.warn (field (path, key) + " was " + juce::String (value) + "; using " + juce::String (clamped));

        out = canonical (clamped);
        return true;
    }

    bool readInt (Reader& r, const juce::DynamicObject& o, const char* key, const juce::String& path,
                  int& out, int low, int high)
    {
        if (! o.hasProperty (key))
            return true;

        const auto& v = o.getProperty (key);
        const double d = isNumber (v) ? (double) v : 0.0;

        if (! isNumber (v) || ! std::isfinite (d) || d != std::floor (d) || std::abs (d) > 2147483647.0)
            return r.fail (TuneLoadError::invalidField, field (path, key), "not a whole number");

        const int value = (int) d;
        const int clamped = juce::jlimit (low, high, value);

        if (clamped != value)
            r.warn (field (path, key) + " was " + juce::String (value) + "; using " + juce::String (clamped));

        out = clamped;
        return true;
    }

    bool readBool (Reader& r, const juce::DynamicObject& o, const char* key, const juce::String& path, bool& out)
    {
        if (! o.hasProperty (key))
            return true;

        const auto& v = o.getProperty (key);

        if (! v.isBool())
            return r.fail (TuneLoadError::invalidField, field (path, key), "not true or false");

        out = (bool) v;
        return true;
    }

    template <typename Enum>
    bool readEnum (Reader& r, const juce::DynamicObject& o, const char* key, const juce::String& path,
                   Enum& out, bool (*parse) (const juce::String&, Enum&))
    {
        juce::String text;

        if (! o.hasProperty (key))
            return true;

        if (! readString (r, o, key, path, text))
            return false;

        if (! parse (text, out))
            return r.fail (TuneLoadError::invalidField, field (path, key), "unknown value \"" + text + "\"");

        return true;
    }

    bool readStringArray (Reader& r, const juce::DynamicObject& o, const char* key, const juce::String& path,
                          juce::StringArray& out)
    {
        if (! o.hasProperty (key))
            return true;

        const auto* items = o.getProperty (key).getArray();

        if (items == nullptr)
            return r.fail (TuneLoadError::invalidField, field (path, key), "not an array");

        juce::StringArray strings;

        for (int i = 0; i < items->size(); ++i)
        {
            const auto item = (*items)[i];

            if (! item.isString())
                return r.fail (TuneLoadError::invalidField, element (field (path, key), i), "not a string");

            strings.add (item.toString());
        }

        out = strings;
        return true;
    }

    const juce::Array<juce::var>* readArray (Reader& r, const juce::DynamicObject& o, const char* key,
                                             const juce::String& path)
    {
        const auto* items = o.getProperty (key).getArray();

        if (items == nullptr)
            r.fail (TuneLoadError::invalidField, field (path, key), "not an array");

        return items;
    }

    bool readPitchClass (Reader& r, const juce::String& text, const juce::String& path, int& out)
    {
        int used = 0;
        const int pc = parsePitchClass (text, 0, used);

        if (pc < 0 || used != text.length())
            return r.fail (TuneLoadError::invalidField, path, "\"" + text + "\" is not a note name");

        out = pc;
        return true;
    }

    //==========================================================================
    bool readNotes (Reader& r, const juce::var& v, const juce::String& path, std::vector<MelodyNote>& out)
    {
        const auto* items = v.getArray();

        if (items == nullptr)
            return r.fail (TuneLoadError::invalidField, path, "not an array");

        std::vector<MelodyNote> notes;

        for (int i = 0; i < items->size(); ++i)
        {
            const auto p = element (path, i);
            const auto item = (*items)[i];
            const auto* o = item.getDynamicObject();

            if (o == nullptr)
                return r.fail (TuneLoadError::invalidField, p, "not an object");

            if (! o->hasProperty ("start"))
                return r.fail (TuneLoadError::missingField, field (p, "start"), "missing");

            if (! o->hasProperty ("pitch"))
                return r.fail (TuneLoadError::missingField, field (p, "pitch"), "missing");

            MelodyNote n;

            if (! readDouble (r, *o, "start", p, n.startBeat, 0.0, 65536.0)
                  || ! readDouble (r, *o, "dur", p, n.durationBeats, kBeatResolution, 65536.0)
                  || ! readInt (r, *o, "vel", p, n.velocity, 1, 127)
                  || ! readEnum (r, *o, "articulation", p, n.articulation, &parseNoteArticulation)
                  || ! readEnum (r, *o, "technique", p, n.technique, &parseNoteTechnique)
                  || ! readBool (r, *o, "locked", p, n.locked))
                return false;

            if (! MelodyPitch::fromVar (o->getProperty ("pitch"), n.pitch))
                return r.fail (TuneLoadError::invalidField, field (p, "pitch"),
                               "not a MIDI note, root+N or chord_tone_N");

            n.extra = unknownFields (*o, { "start", "dur", "pitch", "vel", "articulation", "technique", "locked" });
            notes.push_back (n);
        }

        out = std::move (notes);
        return true;
    }

    bool readChord (Reader& r, const juce::var& v, const juce::String& p, double beatsPerBar, ChordCell& out)
    {
        const auto* o = v.getDynamicObject();

        if (o == nullptr)
            return r.fail (TuneLoadError::invalidField, p, "not an object");

        ChordCell c;
        c.durationBeats = canonical (beatsPerBar);   // 1.1: a cell defaults to one bar

        juce::String root, quality ("maj"), bass;

        if (! readString (r, *o, "root", p, root, true)
              || ! readPitchClass (r, root, field (p, "root"), c.root)
              || ! readString (r, *o, "quality", p, quality)
              || ! readDouble (r, *o, "beats", p, c.durationBeats, 0.0, 4096.0)
              || ! readString (r, *o, "bass", p, bass)
              || ! readStringArray (r, *o, "extensions", p, c.extensions)
              || ! readString (r, *o, "strum", p, c.strumOverride)
              || ! readEnum (r, *o, "emphasis", p, c.emphasis, &parseChordEmphasis)
              || ! readBool (r, *o, "locked", p, c.locked))
            return false;

        if (! resolveQuality (quality, c.quality))
            return r.fail (TuneLoadError::invalidField, field (p, "quality"), "unknown chord quality \"" + quality + "\"");

        if (bass.isNotEmpty() && ! readPitchClass (r, bass, field (p, "bass"), c.bass))
            return false;

        for (int i = 0; i < c.extensions.size(); ++i)
            if (getExtensionSemitones (c.extensions[i]) < 0)
                return r.fail (TuneLoadError::invalidField, element (field (p, "extensions"), i),
                               "unknown extension \"" + c.extensions[i] + "\"");

        c.extra = unknownFields (*o, { "root", "quality", "beats", "bass", "extensions", "strum", "emphasis", "locked" });
        out = c;
        return true;
    }

    bool readMelodyTrack (Reader& r, const juce::var& v, const juce::String& p, MelodyTrack& t)
    {
        const auto* o = v.getDynamicObject();

        if (o == nullptr)
            return r.fail (TuneLoadError::invalidField, p, "not an object");

        if (o->hasProperty ("string_hint"))
        {
            const auto& hint = o->getProperty ("string_hint");

            if (hint.isString() && hint.toString() == "auto")
                t.stringHint = 0;
            else if (! readInt (r, *o, "string_hint", p, t.stringHint, 0, kMaxStrings))
                return false;
        }

        if (! readEnum (r, *o, "articulation", p, t.articulationDefault, &parseNoteArticulation)
              || ! readEnum (r, *o, "source", p, t.source, &parseMelodySource)
              || ! readInt (r, *o, "seed", p, t.seed, 0, 0x7fffffff)
              || ! readDouble (r, *o, "density", p, t.density, 0.25, 16.0)
              || ! readInt (r, *o, "range_low", p, t.rangeLow, 0, 127)
              || ! readInt (r, *o, "range_high", p, t.rangeHigh, 0, 127)
              || ! readBool (r, *o, "follow_chords", p, t.followChords))
            return false;

        // A track default of "inherit" would inherit from nothing.
        if (t.articulationDefault == NoteArticulation::inherit)
        {
            r.warn (field (p, "articulation") + " cannot be \"inherit\"; using \"natural\"");
            t.articulationDefault = NoteArticulation::natural;
        }

        t.extra = unknownFields (*o, { "string_hint", "articulation", "source", "seed", "density",
                                       "range_low", "range_high", "follow_chords" });
        return true;
    }

    bool readLayer (Reader& r, const juce::var& v, const juce::String& p, TuneLayer& l)
    {
        const auto* o = v.getDynamicObject();

        if (o == nullptr)
            return r.fail (TuneLoadError::invalidField, p, "not an object");

        if (! o->hasProperty ("type"))
            return r.fail (TuneLoadError::missingField, field (p, "type"), "missing");

        if (! readEnum (r, *o, "type", p, l.type, &parseLayerType)
              || ! readBool (r, *o, "on", p, l.enabled)
              || ! readDouble (r, *o, "volume", p, l.volume, 0.0, 1.0)
              || ! readDouble (r, *o, "pan", p, l.pan, -1.0, 1.0)
              || ! readString (r, *o, "pattern", p, l.patternId)
              || ! readInt (r, *o, "seed", p, l.seed, 0, 0x7fffffff))
            return false;

        if (o->hasProperty ("notes") && ! readNotes (r, o->getProperty ("notes"), field (p, "notes"), l.notes))
            return false;

        l.extra = unknownFields (*o, { "type", "on", "volume", "pan", "pattern", "seed", "notes" });
        return true;
    }

    bool readSection (Reader& r, const juce::var& v, const juce::String& p, double beatsPerBar, TuneSection& s)
    {
        const auto* o = v.getDynamicObject();

        if (o == nullptr)
            return r.fail (TuneLoadError::invalidField, p, "not an object");

        // The name is required: the setlist finds sections by it.
        if (! readString (r, *o, "name", p, s.name, true)
              || ! readInt (r, *o, "length_bars", p, s.lengthBars, 1, Tune::kMaxBars)
              || ! readString (r, *o, "rhythm_pattern", p, s.rhythmPatternId)
              || ! readString (r, *o, "genre_kit", p, s.genreKitId)
              || ! readEnum (r, *o, "role", p, s.role, &parseSectionRole)
              || ! readBool (r, *o, "rhythm_on", p, s.rhythmOn)
              || ! readString (r, *o, "rhythm_link", p, s.rhythmLinkedTo)
              || ! readDouble (r, *o, "feel", p, s.feel, 0.0, 1.0)
              || ! readDouble (r, *o, "strum", p, s.strum, 0.0, 1.0)
              || ! readBool (r, *o, "state_boundary", p, s.stateBoundary)
              || ! readEnum (r, *o, "style", p, s.style, &parseMelodyStyle))
            return false;

        if (o->hasProperty ("chords"))
        {
            const auto* chords = readArray (r, *o, "chords", p);

            if (chords == nullptr)
                return false;

            for (int i = 0; i < chords->size(); ++i)
            {
                ChordCell c;

                if (! readChord (r, (*chords)[i], element (field (p, "chords"), i), beatsPerBar, c))
                    return false;

                s.chords.push_back (c);
            }
        }

        // tune-builder 1: `melody_track: MelodyTrack | null`. Either key
        // present means there is a track; `melody` holds its notes, as in
        // the spec's example, and `melody_track` its settings.
        if (o->hasProperty ("melody") || o->hasProperty ("melody_track"))
        {
            MelodyTrack track;

            if (o->hasProperty ("melody_track")
                  && ! readMelodyTrack (r, o->getProperty ("melody_track"), field (p, "melody_track"), track))
                return false;

            if (o->hasProperty ("melody")
                  && ! readNotes (r, o->getProperty ("melody"), field (p, "melody"), track.notes))
                return false;

            s.melody = std::move (track);
        }

        if (o->hasProperty ("bass"))
        {
            const auto p2 = field (p, "bass");
            const auto* b = o->getProperty ("bass").getDynamicObject();

            if (b == nullptr)
                return r.fail (TuneLoadError::invalidField, p2, "not an object");

            if (! readEnum (r, *b, "mode", p2, s.bass.mode, &parseBassMode))
                return false;

            if (b->hasProperty ("notes") && ! readNotes (r, b->getProperty ("notes"), field (p2, "notes"), s.bass.notes))
                return false;

            s.bass.extra = unknownFields (*b, { "mode", "notes" });
        }

        if (o->hasProperty ("layers"))
        {
            const auto* layers = readArray (r, *o, "layers", p);

            if (layers == nullptr)
                return false;

            for (int i = 0; i < layers->size(); ++i)
            {
                TuneLayer l;

                if (! readLayer (r, (*layers)[i], element (field (p, "layers"), i), l))
                    return false;

                s.layers.push_back (l);
            }
        }

        s.extra = unknownFields (*o, { "name", "length_bars", "rhythm_pattern", "genre_kit", "chords",
                                       "melody", "melody_track", "bass", "layers", "role", "rhythm_on",
                                       "rhythm_link", "feel", "strum", "state_boundary", "style" });
        return true;
    }

    bool readSections (Reader& r, const juce::DynamicObject& o, const juce::String& path, double beatsPerBar,
                       std::vector<TuneSection>& out)
    {
        if (! o.hasProperty ("sections"))
            return true;

        const auto* items = readArray (r, o, "sections", path);

        if (items == nullptr)
            return false;

        if (items->size() > Tune::kMaxSections)
            return r.fail (TuneLoadError::invalidField, field (path, "sections"),
                           "more than " + juce::String (Tune::kMaxSections) + " sections");

        for (int i = 0; i < items->size(); ++i)
        {
            TuneSection s;

            if (! readSection (r, (*items)[i], element (field (path, "sections"), i), beatsPerBar, s))
                return false;

            for (const auto& existing : out)
                if (existing.name == s.name)
                    r.warn ("Two sections are named \"" + s.name + "\"; the setlist plays the first.");

            out.push_back (std::move (s));
        }

        return true;
    }

    bool readSetlist (Reader& r, const juce::DynamicObject& o, const juce::String& path,
                      std::vector<TuneSetlistEntry>& out)
    {
        if (! o.hasProperty ("setlist"))
            return true;

        const auto* items = readArray (r, o, "setlist", path);

        if (items == nullptr)
            return false;

        for (int i = 0; i < items->size(); ++i)
        {
            const auto p = element (field (path, "setlist"), i);
            const auto item = (*items)[i];
            const auto* e = item.getDynamicObject();

            if (e == nullptr)
                return r.fail (TuneLoadError::invalidField, p, "not an object");

            TuneSetlistEntry entry;

            if (! readString (r, *e, "section", p, entry.section, true)
                  || ! readInt (r, *e, "repeats", p, entry.repeats, 1, Tune::kMaxRepeats))
                return false;

            entry.extra = unknownFields (*e, { "section", "repeats" });
            out.push_back (entry);
        }

        return true;
    }

    bool readMeta (Reader& r, const juce::var& v, TuneMeta& m)
    {
        const auto* o = v.getDynamicObject();

        if (o == nullptr)
            return r.fail (TuneLoadError::invalidField, "meta", "not an object");

        const juce::String p ("meta");
        juce::String timeSig, key, modeName;

        if (! readString (r, *o, "title", p, m.title)
              || ! readString (r, *o, "artist", p, m.artist)
              || ! readString (r, *o, "author", p, m.author)
              || ! readDouble (r, *o, "tempo_bpm", p, m.tempoBpm, Tune::kMinTempo, Tune::kMaxTempo)
              || ! readString (r, *o, "time_sig", p, timeSig)
              || ! readString (r, *o, "key", p, key)
              || ! readString (r, *o, "mode", p, modeName)
              || ! readDouble (r, *o, "swing", p, m.swingPercent, 0.0, 100.0)
              || ! readDouble (r, *o, "feel_pct", p, m.feelPercent, 0.0, 100.0)
              || ! readStringArray (r, *o, "tags", p, m.tags)
              || ! readString (r, *o, "notes", p, m.notes)
              || ! readString (r, *o, "created", p, m.created)
              || ! readString (r, *o, "modified", p, m.modified))
            return false;

        if (timeSig.isNotEmpty())
        {
            const auto num = timeSig.upToFirstOccurrenceOf ("/", false, false);
            const auto den = timeSig.fromFirstOccurrenceOf ("/", false, false);

            const bool digits = num.isNotEmpty() && den.isNotEmpty() && num.length() <= 2 && den.length() <= 2
                                  && num.containsOnly ("0123456789") && den.containsOnly ("0123456789");

            const int n = digits ? num.getIntValue() : 0;
            const int d = digits ? den.getIntValue() : 0;
            const bool validDenominator = d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32;

            if (! digits || n < 1 || n > 32 || ! validDenominator)
                return r.fail (TuneLoadError::invalidField, "meta.time_sig", "\"" + timeSig + "\" is not a time signature");

            m.timeSigNumerator = n;
            m.timeSigDenominator = d;
        }

        if (key.isNotEmpty() && ! parseKey (key, m.keyTonic, m.mode))
            return r.fail (TuneLoadError::invalidField, "meta.key", "\"" + key + "\" is not a key");

        // `mode` refines `key`: "Am" alone is aeolian, "Am" with "dorian" is dorian.
        if (modeName.isNotEmpty() && ! parseTuneMode (modeName, m.mode))
            return r.fail (TuneLoadError::invalidField, "meta.mode", "\"" + modeName + "\" is not a mode");

        m.extra = unknownFields (*o, { "title", "artist", "author", "tempo_bpm", "time_sig", "key", "mode",
                                       "swing", "feel_pct", "tags", "notes", "created", "modified" });
        return true;
    }
}

//==============================================================================
TuneLoadResult TuneFile::fromVar (const juce::var& rootVar, Tune& destination)
{
    Reader r;
    const auto* root = rootVar.getDynamicObject();

    if (root == nullptr)
    {
        r.fail (TuneLoadError::badMagic, "file", "not a JSON object, so not a .luthiertune");
        return r.result;
    }

    // file-formats 14.2 and 14.3: the magic, then a schema this build knows.
    if (! root->hasProperty ("magic") || root->getProperty ("magic").toString() != kMagic
          || ! root->getProperty ("magic").isString())
    {
        r.fail (TuneLoadError::badMagic, "magic", "expected \"" + juce::String (kMagic) + "\"");
        return r.result;
    }

    int schema = 1;   // file-formats 0.2: a missing schema is schema 1

    if (root->hasProperty ("schema"))
    {
        const auto& v = root->getProperty ("schema");
        const double d = isNumber (v) ? (double) v : -1.0;

        if (! isNumber (v) || d != std::floor (d) || d < 1.0 || d > (double) kSchemaVersion)
        {
            r.fail (TuneLoadError::unsupportedSchema, "schema",
                    juce::String ("schema ") + v.toString() + " is not one this version reads (1 to "
                      + juce::String (kSchemaVersion) + ")");
            return r.result;
        }

        schema = (int) d;
    }

    juce::ignoreUnused (schema);   // One schema so far: no migrations to run (file-formats 14.4).

    Tune loaded;

    if (root->hasProperty ("meta") && ! readMeta (r, root->getProperty ("meta"), loaded.meta))
        return r.result;

    const double beatsPerBar = loaded.getBeatsPerBar();

    if (! readSections (r, *root, {}, beatsPerBar, loaded.arrangement.sections)
          || ! readSetlist (r, *root, {}, loaded.arrangement.setlist))
        return r.result;

    if (root->hasProperty ("variations"))
    {
        const auto* items = readArray (r, *root, "variations", {});

        if (items == nullptr)
            return r.result;

        for (int i = 0; i < items->size(); ++i)
        {
            const auto p = element ("variations", i);
            const auto item = (*items)[i];
            const auto* o = item.getDynamicObject();

            if (o == nullptr)
            {
                r.fail (TuneLoadError::invalidField, p, "not an object");
                return r.result;
            }

            TuneVariation variation;

            if (! readString (r, *o, "name", p, variation.name)
                  || ! readSections (r, *o, p, beatsPerBar, variation.arrangement.sections)
                  || ! readSetlist (r, *o, p, variation.arrangement.setlist))
                return r.result;

            variation.extra = unknownFields (*o, { "name", "sections", "setlist" });
            loaded.variations.push_back (std::move (variation));
        }
    }

    loaded.extra = unknownFields (*root, { "schema", "magic", "meta", "sections", "setlist", "variations" });

    for (const auto& problem : loaded.validate())
        r.warn (problem);

    destination = std::move (loaded);
    return r.result;
}

TuneLoadResult TuneFile::fromJson (const juce::String& text, Tune& destination)
{
    juce::var parsed;
    const auto parse = juce::JSON::parse (text, parsed);

    if (parse.failed())
    {
        TuneLoadResult result;
        result.error = TuneLoadError::malformedJson;
        result.message = "file: " + parse.getErrorMessage();
        return result;
    }

    return fromVar (parsed, destination);
}

TuneLoadResult TuneFile::fromBytes (const void* data, size_t numBytes, Tune& destination)
{
    TuneLoadResult result;

    if (numBytes > kMaxFileBytes)
    {
        result.error = TuneLoadError::tooLarge;
        result.message = "file: " + juce::String ((juce::int64) numBytes) + " bytes is too large for a tune";
        return result;
    }

    const auto* bytes = static_cast<const char*> (data);

    if (numBytes == 0 || bytes == nullptr)
    {
        result.error = TuneLoadError::malformedJson;
        result.message = "file: empty";
        return result;
    }

    // A UTF-8 byte-order mark is allowed and skipped; it is still UTF-8.
    if (numBytes >= 3 && (unsigned char) bytes[0] == 0xEF && (unsigned char) bytes[1] == 0xBB
          && (unsigned char) bytes[2] == 0xBF)
    {
        bytes += 3;
        numBytes -= 3;
    }

    // A NUL fails too: no tune contains one, and one would hide the rest of
    // the file from the parser.
    if (! isValidUtf8 (reinterpret_cast<const unsigned char*> (bytes), numBytes))
    {
        result.error = TuneLoadError::notUtf8;
        result.message = "file: not UTF-8 text";
        return result;
    }

    return fromJson (juce::String::fromUTF8 (bytes, (int) numBytes), destination);
}

TuneLoadResult TuneFile::load (const juce::File& file, Tune& destination)
{
    ThreadProbe::noteFileAccess();

    TuneLoadResult result;

    if (! file.existsAsFile())
    {
        result.error = TuneLoadError::fileNotFound;
        result.message = file.getFullPathName() + ": not found";
        return result;
    }

    if ((size_t) juce::jmax ((juce::int64) 0, file.getSize()) > kMaxFileBytes)
    {
        result.error = TuneLoadError::tooLarge;
        result.message = file.getFileName() + ": too large for a tune";
        return result;
    }

    juce::MemoryBlock data;

    if (! file.loadFileAsData (data))
    {
        result.error = TuneLoadError::unreadable;
        result.message = file.getFullPathName() + ": could not be read";
        return result;
    }

    result = fromBytes (data.getData(), data.getSize(), destination);

    if (! result.ok())
        result.message = file.getFileName() + ": " + result.message;

    return result;
}

//==============================================================================
juce::File TuneFile::getUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("Tunes");
}

juce::File TuneFile::getBackupFolder (const juce::File& target)
{
    return target.getParentDirectory()
             .getChildFile (".backup")
             .getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d"));
}

bool TuneFile::save (const Tune& tune, const juce::File& file, juce::String& error, bool keepBackup)
{
    ThreadProbe::noteFileAccess();

    const auto text = toJson (tune);
    const auto folder = file.getParentDirectory();

    if (! folder.createDirectory().wasOk())
    {
        error = folder.getFullPathName() + ": could not create the folder";
        return false;
    }

    // 13.1: written beside the target under the target's own name plus .tmp.
    const auto temp = file.getSiblingFile (file.getFileName() + ".tmp");
    temp.deleteFile();

    bool written = false;

    {
        juce::FileOutputStream out (temp);

        if (out.openedOk())
        {
            out.setPosition (0);
            out.truncate();

            written = out.write (text.toRawUTF8(), text.getNumBytesAsUTF8());

            // 13.2: FileOutputStream::flush reaches the disk (FlushFileBuffers
            // on Windows, fsync elsewhere), which is the fsync the spec asks for.
            out.flush();
            written = written && out.getStatus().wasOk();
        }
    }

    if (! written)
    {
        temp.deleteFile();
        error = file.getFullPathName() + ": could not write (disk full or not writable); the previous file is untouched";
        return false;
    }

    // 13.4: the version being replaced is kept, filed by the day.
    if (keepBackup && file.existsAsFile())
    {
        const auto backups = getBackupFolder (file);

        if (backups.createDirectory().wasOk())
        {
            auto destination = backups.getChildFile (file.getFileName());

            // Several saves in a day keep several versions, not one.
            for (int i = 2; destination.existsAsFile() && i < 1000; ++i)
                destination = backups.getChildFile (file.getFileNameWithoutExtension() + "-" + juce::String (i)
                                                      + file.getFileExtension());

            file.copyFileTo (destination);
        }
    }

    // 13.3: one rename over the target. replaceFileIn is an atomic replace on
    // Windows (ReplaceFile) and a rename(2) elsewhere.
    if (! temp.replaceFileIn (file))
    {
        temp.deleteFile();
        error = file.getFullPathName() + ": could not replace the file; the previous version is intact";
        return false;
    }

    return true;
}

} // namespace luthier
