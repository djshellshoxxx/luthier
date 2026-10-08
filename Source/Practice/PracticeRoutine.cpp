#include "PracticeRoutine.h"

#include <cmath>

namespace luthier
{

//==============================================================================
namespace
{
    const char* const kToolKeys[]   = { "metronome", "looper", "backing_track", "scale_trainer",
                                        "ear_trainer", "tab_reader", "progression", "session_recorder" };
    const char* const kToolLabels[] = { "METRO", "LOOP", "TRACK", "SCALE", "EAR", "TAB", "PROG", "SESSION" };

    const char* const kSubdivisionKeys[] = { "quarter", "eighth", "triplet", "sixteenth", "dotted_eighth" };
    const char* const kSoundKeys[]       = { "wood_block", "cowbell", "digital_blip", "side_stick", "shaker", "tap" };
    const char* const kAccentKeys[]      = { "silent", "ghost", "normal", "accent" };
    const char* const kScaleKeys[]       = { "ionian", "dorian", "phrygian", "lydian", "mixolydian", "aeolian",
                                             "locrian", "harmonic_minor", "melodic_minor", "major_pentatonic",
                                             "minor_pentatonic", "blues", "custom" };
    const char* const kScaleModeKeys[]   = { "explore", "quiz", "interval", "chord_tone" };
    const char* const kExerciseKeys[]    = { "interval", "chord_quality", "progression" };
    const char* const kLayerModeKeys[]   = { "overdub", "replace", "play_once" };

    template <typename Enum, size_t N>
    const char* keyOf (const char* const (&keys)[N], Enum value) noexcept
    {
        return keys[(size_t) juce::jlimit (0, (int) N - 1, (int) value)];
    }

    template <typename Enum, size_t N>
    bool parseKeyOf (const char* const (&keys)[N], const juce::String& text, Enum& result)
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (text == keys[i])
            {
                result = (Enum) (int) i;
                return true;
            }
        }

        return false;
    }

    /** A setting read off an entry, or nothing. */
    const juce::var* setting (const RoutineEntry& entry, const char* key)
    {
        if (auto* object = entry.settings.getDynamicObject())
            if (object->hasProperty (key))
                return &object->getProperty (key);

        return nullptr;
    }

    bool isNumber (const juce::var& v) noexcept
    {
        return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite ((double) v);
    }

    /** "7/8" -> 7, 8. */
    bool parseTimeSignature (const juce::String& text, int& numerator, int& denominator)
    {
        const auto n = text.upToFirstOccurrenceOf ("/", false, false).trim();
        const auto d = text.fromFirstOccurrenceOf ("/", false, false).trim();

        if (n.isEmpty() || d.isEmpty() || ! n.containsOnly ("0123456789") || ! d.containsOnly ("0123456789"))
            return false;

        numerator = n.getIntValue();
        denominator = d.getIntValue();

        // practice-tools 1: numerator 1-32, denominator 2/4/8/16.
        return numerator >= 1 && numerator <= 32
                 && (denominator == 2 || denominator == 4 || denominator == 8 || denominator == 16);
    }

    /** "A", "F#", "Bb" -> pitch class. */
    int parseNoteName (const juce::String& text)
    {
        static const int naturals[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G

        const auto t = text.trim();

        if (t.isEmpty() || t.length() > 2)
            return -1;

        const auto letter = juce::CharacterFunctions::toUpperCase (t[0]);

        if (letter < 'A' || letter > 'G')
            return -1;

        int pc = naturals[(int) (letter - 'A')];

        if (t.length() == 2)
        {
            if (t[1] == '#')      pc += 1;
            else if (t[1] == 'b') pc += 11;
            else return -1;
        }

        return pc % 12;
    }

    juce::NamedValueSet unknownFields (const juce::DynamicObject& object, std::initializer_list<const char*> known)
    {
        juce::NamedValueSet extra;

        for (const auto& property : object.getProperties())
        {
            bool isKnown = false;

            for (auto* k : known)
                isKnown = isKnown || property.name.toString() == k;

            if (! isKnown)
                extra.set (property.name, property.value);
        }

        return extra;
    }

    void appendExtras (juce::DynamicObject& object, const juce::NamedValueSet& extra)
    {
        for (const auto& field : extra)
            if (! object.hasProperty (field.name))
                object.setProperty (field.name, field.value);
    }

    /** Builds a settings object from key/value pairs, for the factory table. */
    juce::var makeSettings (std::initializer_list<std::pair<const char*, juce::var>> pairs)
    {
        auto* object = new juce::DynamicObject();

        for (const auto& p : pairs)
            object->setProperty (p.first, p.second);

        return juce::var (object);
    }

    juce::var accentList (std::initializer_list<const char*> accents)
    {
        juce::Array<juce::var> items;

        for (auto* a : accents)
            items.add (juce::String (a));

        return juce::var (items);
    }

    RoutineEntry makeEntry (PracticeTool tool, const char* title, const char* instructions,
                            double seconds, juce::var settings)
    {
        RoutineEntry e;
        e.tool = tool;
        e.title = title;
        e.instructions = instructions;
        e.durationSeconds = seconds;
        e.settings = std::move (settings);
        return e;
    }
}

//==============================================================================
const char* getPracticeToolKey (PracticeTool tool) noexcept       { return keyOf (kToolKeys, tool); }
const char* getPracticeToolTabLabel (PracticeTool tool) noexcept  { return keyOf (kToolLabels, tool); }
bool parsePracticeTool (const juce::String& key, PracticeTool& r) { return parseKeyOf (kToolKeys, key, r); }

namespace practicekeys
{
    const char* subdivision (ClickSubdivision v) noexcept     { return keyOf (kSubdivisionKeys, v); }
    const char* sound (ClickSound v) noexcept                 { return keyOf (kSoundKeys, v); }
    const char* accent (BeatAccent v) noexcept                { return keyOf (kAccentKeys, v); }
    const char* scale (ScaleType v) noexcept                  { return keyOf (kScaleKeys, v); }
    const char* scaleMode (ScaleTrainer::Mode v) noexcept     { return keyOf (kScaleModeKeys, v); }
    const char* earExercise (EarTrainer::Exercise v) noexcept { return keyOf (kExerciseKeys, v); }
    const char* layerMode (LayerMode v) noexcept              { return keyOf (kLayerModeKeys, v); }

    bool parse (const juce::String& t, ClickSubdivision& r)     { return parseKeyOf (kSubdivisionKeys, t, r); }
    bool parse (const juce::String& t, ClickSound& r)           { return parseKeyOf (kSoundKeys, t, r); }
    bool parse (const juce::String& t, BeatAccent& r)           { return parseKeyOf (kAccentKeys, t, r); }
    bool parse (const juce::String& t, ScaleType& r)            { return parseKeyOf (kScaleKeys, t, r); }
    bool parse (const juce::String& t, ScaleTrainer::Mode& r)   { return parseKeyOf (kScaleModeKeys, t, r); }
    bool parse (const juce::String& t, EarTrainer::Exercise& r) { return parseKeyOf (kExerciseKeys, t, r); }
    bool parse (const juce::String& t, LayerMode& r)            { return parseKeyOf (kLayerModeKeys, t, r); }
}

//==============================================================================
namespace practicefiles
{
    juce::File getPracticeDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Luthier")
                 .getChildFile ("Practice");
    }

    bool writeJson (const juce::File& file, const juce::var& data, juce::String& error)
    {
        const auto text = juce::JSON::toString (data, false).replace ("\r\n", "\n") + "\n";

        if (! file.getParentDirectory().createDirectory().wasOk())
        {
            error = file.getParentDirectory().getFullPathName() + ": could not create the folder";
            return false;
        }

        const auto temp = file.getSiblingFile (file.getFileName() + ".tmp");
        temp.deleteFile();

        bool written = false;

        {
            juce::FileOutputStream out (temp);

            if (out.openedOk())
            {
                written = out.write (text.toRawUTF8(), text.getNumBytesAsUTF8());
                out.flush();   // FlushFileBuffers / fsync: file-formats 13.2
                written = written && out.getStatus().wasOk();
            }
        }

        if (! written || ! temp.replaceFileIn (file))
        {
            temp.deleteFile();
            error = file.getFullPathName() + ": could not be written; the previous version is untouched";
            return false;
        }

        return true;
    }

    bool readJson (const juce::File& file, juce::var& result, juce::String& error)
    {
        if (! file.existsAsFile())
        {
            error = file.getFullPathName() + ": not found";
            return false;
        }

        juce::var parsed;
        const auto parse = juce::JSON::parse (file.loadFileAsString(), parsed);

        if (parse.failed())
        {
            error = file.getFileName() + ": " + parse.getErrorMessage();
            return false;
        }

        result = parsed;
        return true;
    }

    bool sameJson (const juce::var& a, const juce::var& b)
    {
        return juce::JSON::toString (a, true) == juce::JSON::toString (b, true);
    }

    bool sameExtras (const juce::NamedValueSet& a, const juce::NamedValueSet& b)
    {
        if (a.size() != b.size())
            return false;

        for (int i = 0; i < a.size(); ++i)
            if (a.getName (i) != b.getName (i) || ! sameJson (a.getValueAt (i), b.getValueAt (i)))
                return false;

        return true;
    }
}

//==============================================================================
bool RoutineEntry::operator== (const RoutineEntry& o) const
{
    return tool == o.tool && title == o.title && instructions == o.instructions
        && durationSeconds == o.durationSeconds && repetitions == o.repetitions
        && countInBars == o.countInBars && practicefiles::sameJson (settings, o.settings)
        && practicefiles::sameExtras (extra, o.extra);
}

double PracticeRoutine::getTotalSeconds() const
{
    double total = 0.0;

    for (const auto& e : entries)
    {
        if (e.isTimed())
            total += juce::jmax (0.0, e.durationSeconds);

        if (e.countInBars > 0)
        {
            const auto* tempo = setting (e, "tempo_bpm");
            const double bpm = (tempo != nullptr && isNumber (*tempo)) ? juce::jlimit (20.0, 300.0, (double) *tempo) : 120.0;
            total += (double) e.countInBars * 4.0 * 60.0 / bpm;   // a 4/4 bar, when the entry names none
        }
    }

    return total;
}

bool PracticeRoutine::operator== (const PracticeRoutine& o) const
{
    return name == o.name && author == o.author && notes == o.notes && factory == o.factory
        && entries == o.entries && practicefiles::sameExtras (extra, o.extra);
}

juce::var PracticeRoutine::toVar() const
{
    auto* root = new juce::DynamicObject();

    // file-formats 0.2 and 0.5: every file carries a schema and a magic.
    root->setProperty ("schema", kSchemaVersion);
    root->setProperty ("magic", juce::String (kMagic));

    auto* meta = new juce::DynamicObject();
    meta->setProperty ("name", name);
    meta->setProperty ("author", author);
    meta->setProperty ("notes", notes);
    root->setProperty ("meta", juce::var (meta));

    juce::Array<juce::var> items;

    for (const auto& e : entries)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("tool", getPracticeToolKey (e.tool));
        o->setProperty ("title", e.title);
        o->setProperty ("instructions", e.instructions);
        o->setProperty ("duration_s", e.durationSeconds);
        o->setProperty ("repetitions", e.repetitions);
        o->setProperty ("count_in_bars", e.countInBars);
        o->setProperty ("settings", e.settings.isObject() ? e.settings : juce::var (new juce::DynamicObject()));
        appendExtras (*o, e.extra);
        items.add (juce::var (o));
    }

    root->setProperty ("entries", juce::var (items));
    appendExtras (*root, extra);
    return juce::var (root);
}

bool PracticeRoutine::fromVar (const juce::var& state, PracticeRoutine& result, juce::String& error)
{
    const auto* root = state.getDynamicObject();

    if (root == nullptr || root->getProperty ("magic").toString() != kMagic)
    {
        error = "not a routine (magic is not \"" + juce::String (kMagic) + "\")";
        return false;
    }

    if (root->hasProperty ("schema"))
    {
        const auto& schema = root->getProperty ("schema");

        if (! isNumber (schema) || (int) schema < 1 || (int) schema > kSchemaVersion)
        {
            error = "schema " + schema.toString() + " is not one this version reads";
            return false;
        }
    }

    PracticeRoutine loaded;

    if (const auto* meta = root->getProperty ("meta").getDynamicObject())
    {
        loaded.name = meta->getProperty ("name").toString();
        loaded.author = meta->getProperty ("author").toString();
        loaded.notes = meta->getProperty ("notes").toString();
    }

    if (loaded.name.trim().isEmpty())
    {
        error = "meta.name: missing";
        return false;
    }

    if (root->hasProperty ("entries"))
    {
        const auto* items = root->getProperty ("entries").getArray();

        if (items == nullptr)
        {
            error = "entries: not an array";
            return false;
        }

        for (int i = 0; i < items->size(); ++i)
        {
            const auto item = (*items)[i];
            const auto* o = item.getDynamicObject();
            const auto path = "entries[" + juce::String (i) + "]";

            if (o == nullptr)
            {
                error = path + ": not an object";
                return false;
            }

            RoutineEntry e;

            if (! parsePracticeTool (o->getProperty ("tool").toString(), e.tool))
            {
                error = path + ".tool: unknown tool \"" + o->getProperty ("tool").toString() + "\"";
                return false;
            }

            e.title = o->getProperty ("title").toString();
            e.instructions = o->getProperty ("instructions").toString();

            const auto& duration = o->getProperty ("duration_s");
            const auto& reps = o->getProperty ("repetitions");
            const auto& countIn = o->getProperty ("count_in_bars");

            if ((o->hasProperty ("duration_s") && ! isNumber (duration))
                  || (o->hasProperty ("repetitions") && ! isNumber (reps))
                  || (o->hasProperty ("count_in_bars") && ! isNumber (countIn)))
            {
                error = path + ": duration_s, repetitions and count_in_bars must be numbers";
                return false;
            }

            e.durationSeconds = o->hasProperty ("duration_s") ? juce::jlimit (0.0, 24.0 * 3600.0, (double) duration) : 60.0;
            e.repetitions = o->hasProperty ("repetitions") ? juce::jlimit (0, 10000, (int) reps) : 0;
            e.countInBars = o->hasProperty ("count_in_bars") ? juce::jlimit (0, 16, (int) countIn) : 0;

            const auto& settings = o->getProperty ("settings");
            e.settings = settings.isObject() ? settings : juce::var (new juce::DynamicObject());

            e.extra = unknownFields (*o, { "tool", "title", "instructions", "duration_s", "repetitions",
                                           "count_in_bars", "settings" });
            loaded.entries.push_back (e);
        }
    }

    loaded.extra = unknownFields (*root, { "schema", "magic", "meta", "entries" });
    result = std::move (loaded);
    return true;
}

bool PracticeRoutine::saveTo (const juce::File& file, juce::String& error) const
{
    return practicefiles::writeJson (file, toVar(), error);
}

bool PracticeRoutine::loadFrom (const juce::File& file, PracticeRoutine& result, juce::String& error)
{
    juce::var parsed;

    if (! practicefiles::readJson (file, parsed, error))
        return false;

    if (! fromVar (parsed, result, error))
    {
        error = file.getFileName() + ": " + error;
        return false;
    }

    return true;
}

//==============================================================================
std::vector<PracticeRoutine> PracticeRoutineLibrary::createFactoryRoutines()
{
    using T = PracticeTool;
    std::vector<PracticeRoutine> routines;

    // ---- Warm-up, 10 minutes: 120 + 120 + 180 + 120 + 60 s --------------------------------
    {
        PracticeRoutine r;
        r.name = "Warm-up, 10 minutes";
        r.author = "Factory";
        r.notes = "Loosen both hands before anything fast.";

        r.entries.push_back (makeEntry (T::metronome, "Chromatic, one note per click",
            "Frets 1-2-3-4 on every string and back, one finger per fret.", 120.0,
            makeSettings ({ { "tempo_bpm", 60.0 }, { "subdivision", "quarter" } })));

        r.entries.push_back (makeEntry (T::metronome, "Chromatic, two notes per click",
            "The same pattern in eighths. Alternate picking throughout.", 120.0,
            makeSettings ({ { "tempo_bpm", 72.0 }, { "subdivision", "eighth" } })));

        r.entries.push_back (makeEntry (T::metronome, "Spider walk, speeding up",
            "Fingers 1-3 then 2-4 across the strings. The click climbs from 80 to 100.", 180.0,
            makeSettings ({ { "tempo_bpm", 80.0 }, { "subdivision", "eighth" },
                            { "progressive", makeSettings ({ { "from_bpm", 80.0 }, { "to_bpm", 100.0 },
                                                             { "bars", 32 } }) } })));

        r.entries.push_back (makeEntry (T::scaleTrainer, "The major scale in two positions",
            "C major from the 8th fret, then from the 3rd.", 120.0,
            makeSettings ({ { "key", "C" }, { "scale", "ionian" }, { "mode", "explore" } })));

        r.entries.push_back (makeEntry (T::progression, "Open-chord changes",
            "Change on the one; keep the strumming hand moving.", 60.0,
            makeSettings ({ { "text", "G - C - D - Em x4" }, { "tempo_bpm", 90.0 } })));

        routines.push_back (r);
    }

    // ---- Scales and modes, 20 minutes: 180 + 240 + 180 + 180 + 120 + 180 + 120 s ---------------
    {
        PracticeRoutine r;
        r.name = "Scales and modes, 20 minutes";
        r.author = "Factory";
        r.notes = "Hear each mode against its own chord.";

        r.entries.push_back (makeEntry (T::scaleTrainer, "A minor across the neck",
            "Play A Aeolian in every position you know. Say the degree as you play it.", 180.0,
            makeSettings ({ { "key", "A" }, { "scale", "aeolian" }, { "mode", "explore" } })));

        r.entries.push_back (makeEntry (T::scaleTrainer, "A minor quiz",
            "Find each degree without looking.", 240.0,
            makeSettings ({ { "key", "A" }, { "scale", "aeolian" }, { "mode", "quiz" } })));

        r.entries.push_back (makeEntry (T::scaleTrainer, "D Dorian",
            "The same notes as A minor, heard from D. Lean on the natural 6th.", 180.0,
            makeSettings ({ { "key", "D" }, { "scale", "dorian" }, { "mode", "explore" } })));

        r.entries.push_back (makeEntry (T::scaleTrainer, "D Dorian quiz",
            "Find each degree without looking.", 180.0,
            makeSettings ({ { "key", "D" }, { "scale", "dorian" }, { "mode", "quiz" } })));

        r.entries.push_back (makeEntry (T::scaleTrainer, "A minor pentatonic",
            "All five shapes, joined up the neck.", 120.0,
            makeSettings ({ { "key", "A" }, { "scale", "minor_pentatonic" }, { "mode", "explore" } })));

        r.entries.push_back (makeEntry (T::earTrainer, "Intervals by ear",
            "Name each interval before you check.", 180.0,
            makeSettings ({ { "exercise", "interval" }, { "adaptive", true } })));

        r.entries.push_back (makeEntry (T::progression, "Dorian vamp",
            "Improvise in D Dorian over the vamp.", 120.0,
            makeSettings ({ { "text", "Dm7 - G7 x4" }, { "tempo_bpm", 96.0 } })));

        routines.push_back (r);
    }

    // ---- Timing and feel, 15 minutes: 120 + 180 + 120 + 180 + 120 + 180 s ----------------------
    {
        PracticeRoutine r;
        r.name = "Timing and feel, 15 minutes";
        r.author = "Factory";
        r.notes = "Play with the click, then without it.";

        r.entries.push_back (makeEntry (T::metronome, "Quarter notes, dead on the click",
            "Single muted strums. Bury the click.", 120.0,
            makeSettings ({ { "tempo_bpm", 80.0 }, { "subdivision", "quarter" } })));

        r.entries.push_back (makeEntry (T::metronome, "The click on 2 and 4",
            "Hear the click as a backbeat and feel 1 and 3 yourself.", 180.0,
            makeSettings ({ { "tempo_bpm", 80.0 }, { "subdivision", "quarter" },
                            { "accents", accentList ({ "silent", "accent", "silent", "accent" }) } })));

        r.entries.push_back (makeEntry (T::metronome, "Triplets",
            "Three even strokes per click; accent the first.", 120.0,
            makeSettings ({ { "tempo_bpm", 90.0 }, { "subdivision", "triplet" } })));

        r.entries.push_back (makeEntry (T::metronome, "Silent bars",
            "The click drops out every other bar. Land the next one together.", 180.0,
            makeSettings ({ { "tempo_bpm", 90.0 }, { "subdivision", "quarter" }, { "silent_bar_period", 2 } })));

        r.entries.push_back (makeEntry (T::metronome, "Sixteenths",
            "Down-up-down-up, even and relaxed.", 120.0,
            makeSettings ({ { "tempo_bpm", 100.0 }, { "subdivision", "sixteenth" } })));

        r.entries.push_back (makeEntry (T::looper, "Loop a rhythm, then play over it",
            "Record four bars of rhythm, then take a solo over the loop.", 180.0,
            makeSettings ({ { "layer_mode", "overdub" }, { "tempo_bpm", 90.0 }, { "click", true } })));

        routines.push_back (r);
    }

    for (auto& r : routines)
        r.factory = true;

    return routines;
}

juce::File PracticeRoutineLibrary::getUserDirectory()
{
    return practicefiles::getPracticeDirectory().getChildFile ("Routines");
}

PracticeRoutineLibrary::PracticeRoutineLibrary (const juce::File& directory)
    : userDirectory (directory)
{
    refresh();
}

void PracticeRoutineLibrary::refresh()
{
    routines = createFactoryRoutines();
    loadErrors.clear();

    if (! userDirectory.isDirectory())
        return;

    auto files = userDirectory.findChildFiles (juce::File::findFiles, false,
                                               juce::String ("*") + PracticeRoutine::kFileExtension);
    files.sort();

    for (const auto& file : files)
    {
        PracticeRoutine routine;
        juce::String error;

        if (! PracticeRoutine::loadFrom (file, routine, error))
        {
            loadErrors.add (error);
            continue;
        }

        // A user file may not shadow a factory routine: 11.4 promises the
        // three are always there as shipped.
        if (indexOf (routine.name) >= 0)
        {
            loadErrors.add (file.getFileName() + ": a routine named \"" + routine.name + "\" already exists");
            continue;
        }

        routines.push_back (std::move (routine));
    }
}

const PracticeRoutine& PracticeRoutineLibrary::getRoutine (int index) const
{
    static const PracticeRoutine empty;
    return juce::isPositiveAndBelow (index, (int) routines.size()) ? routines[(size_t) index] : empty;
}

int PracticeRoutineLibrary::indexOf (const juce::String& name) const
{
    for (size_t i = 0; i < routines.size(); ++i)
        if (routines[i].name == name)
            return (int) i;

    return -1;
}

int PracticeRoutineLibrary::getNumUserRoutines() const noexcept
{
    int count = 0;

    for (const auto& r : routines)
        count += r.factory ? 0 : 1;

    return count;
}

juce::File PracticeRoutineLibrary::getFileFor (const juce::String& routineName) const
{
    return userDirectory.getChildFile (juce::File::createLegalFileName (routineName) + PracticeRoutine::kFileExtension);
}

bool PracticeRoutineLibrary::save (const PracticeRoutine& routine, juce::String& error)
{
    const auto name = routine.name.trim();

    if (name.isEmpty())
    {
        error = "A routine needs a name.";
        return false;
    }

    const int existing = indexOf (name);

    if (existing >= 0 && routines[(size_t) existing].factory)
    {
        error = "\"" + name + "\" is a factory routine; save your version under another name.";
        return false;
    }

    auto copy = routine;
    copy.name = name;
    copy.factory = false;

    if (! copy.saveTo (getFileFor (name), error))
        return false;

    if (existing >= 0)
        routines[(size_t) existing] = copy;
    else
        routines.push_back (copy);

    return true;
}

bool PracticeRoutineLibrary::remove (const juce::String& name)
{
    const int index = indexOf (name);

    if (index < 0 || routines[(size_t) index].factory)
        return false;

    getFileFor (name).deleteFile();
    routines.erase (routines.begin() + index);
    return true;
}

//==============================================================================
juce::StringArray applyRoutineSettings (const RoutineEntry& entry, const PracticeTargets& targets)
{
    juce::StringArray problems;
    const auto tool = juce::String (getPracticeToolKey (entry.tool));

    auto need = [&problems, &tool] (void* target, const char* what)
    {
        if (target == nullptr)
            problems.add (tool + ": no " + what + " to apply settings to");

        return target != nullptr;
    };

    auto bad = [&problems, &tool] (const char* key, const juce::var& value)
    {
        problems.add (tool + "." + key + ": \"" + value.toString() + "\" is not a value it takes");
    };

    // The practice tempo, which any entry may name.
    if (const auto* tempo = setting (entry, "tempo_bpm"))
    {
        if (! isNumber (*tempo))
            bad ("tempo_bpm", *tempo);
        else if (targets.metronome != nullptr)
        {
            targets.metronome->setFollowsTempo (false);   // SPEC-SWEEP PT-6: the routine's tempo stays
            targets.metronome->setTempo (juce::jlimit (20.0, 300.0, (double) *tempo));
        }
    }

    switch (entry.tool)
    {
        case PracticeTool::metronome:
        {
            if (! need (targets.metronome, "metronome"))
                break;

            auto& m = *targets.metronome;

            if (const auto* v = setting (entry, "time_sig"))
            {
                int n = 4, d = 4;

                if (parseTimeSignature (v->toString(), n, d))
                    m.setTimeSignature (n, d);
                else
                    bad ("time_sig", *v);
            }

            if (const auto* v = setting (entry, "subdivision"))
            {
                ClickSubdivision s;

                if (practicekeys::parse (v->toString(), s))
                    m.setSubdivision (s);
                else
                    bad ("subdivision", *v);
            }

            if (const auto* v = setting (entry, "sound"))
            {
                ClickSound s;

                if (practicekeys::parse (v->toString(), s))
                    m.setSound (s);
                else
                    bad ("sound", *v);
            }

            // An entry without an accent pattern gets the default one, so a
            // pattern from the previous entry never leaks into this one.
            m.resetAccents();

            if (const auto* v = setting (entry, "accents"))
            {
                if (const auto* list = v->getArray())
                {
                    for (int beat = 0; beat < juce::jmin (list->size(), Metronome::kMaxBeatsPerBar); ++beat)
                    {
                        BeatAccent a;

                        if (practicekeys::parse ((*list)[beat].toString(), a))
                            m.setBeatAccent (beat, a);
                        else
                            bad ("accents", (*list)[beat]);
                    }
                }
                else
                {
                    bad ("accents", *v);
                }
            }

            {
                const auto* v = setting (entry, "silent_bar_period");
                m.setSilentBarPeriod (v != nullptr && isNumber (*v) ? (int) *v : 0);
            }

            if (const auto* v = setting (entry, "level_db"))
            {
                if (isNumber (*v))
                    m.setLevelDb ((double) *v);
                else
                    bad ("level_db", *v);
            }

            m.stopProgressiveTempo();

            if (const auto* v = setting (entry, "progressive"))
            {
                const auto* p = v->getDynamicObject();

                if (p != nullptr && isNumber (p->getProperty ("from_bpm")) && isNumber (p->getProperty ("to_bpm"))
                      && isNumber (p->getProperty ("bars")))
                    m.startProgressiveTempo ((double) p->getProperty ("from_bpm"), (double) p->getProperty ("to_bpm"),
                                             juce::jmax (1, (int) p->getProperty ("bars")));
                else
                    bad ("progressive", *v);
            }

            m.setEnabled (true);
            break;
        }

        case PracticeTool::looper:
        {
            if (! need (targets.looper, "looper"))
                break;

            if (const auto* v = setting (entry, "layer_mode"))
            {
                LayerMode mode;

                if (practicekeys::parse (v->toString(), mode))
                    targets.looper->getLayer (targets.looper->getActiveLayer()).setMode (mode);
                else
                    bad ("layer_mode", *v);
            }

            break;
        }

        case PracticeTool::backingTrack:
        {
            if (! need (targets.backingTrack, "backing track"))
                break;

            auto& b = *targets.backingTrack;

            if (const auto* v = setting (entry, "file"))
            {
                const juce::File file (v->toString());

                if (file != b.getFile() && ! b.load (file))
                    problems.add (tool + ".file: could not load " + file.getFullPathName());
            }

            if (const auto* v = setting (entry, "level_db"))       { if (isNumber (*v)) b.setLevelDb ((double) *v); else bad ("level_db", *v); }
            if (const auto* v = setting (entry, "tempo_ratio"))    { if (isNumber (*v)) b.setTempoRatio ((double) *v); else bad ("tempo_ratio", *v); }
            if (const auto* v = setting (entry, "pitch_semitones")) { if (isNumber (*v)) b.setPitchShiftSemitones ((double) *v); else bad ("pitch_semitones", *v); }

            const auto* start = setting (entry, "loop_start_s");
            const auto* end = setting (entry, "loop_end_s");

            if (start != nullptr && end != nullptr && isNumber (*start) && isNumber (*end)
                  && (double) *end > (double) *start)
                b.setLoopSeconds ((double) *start, (double) *end);

            if (const auto* v = setting (entry, "loop"))
                b.setLoopEnabled ((bool) *v);

            if (const auto* v = setting (entry, "play"))
                if ((bool) *v && b.isLoaded())
                    b.play();

            break;
        }

        case PracticeTool::scaleTrainer:
        {
            if (! need (targets.scaleTrainer, "scale trainer"))
                break;

            auto& s = *targets.scaleTrainer;

            if (const auto* v = setting (entry, "key"))
            {
                const int pc = parseNoteName (v->toString());

                if (pc >= 0)
                    s.setKey (pc);
                else
                    bad ("key", *v);
            }

            if (const auto* v = setting (entry, "scale"))
            {
                ScaleType type;

                if (practicekeys::parse (v->toString(), type))
                    s.setScale (type);
                else
                    bad ("scale", *v);
            }

            if (const auto* v = setting (entry, "mode"))
            {
                ScaleTrainer::Mode mode;

                if (practicekeys::parse (v->toString(), mode))
                    s.setMode (mode);
                else
                    bad ("mode", *v);
            }

            break;
        }

        case PracticeTool::earTrainer:
        {
            if (! need (targets.earTrainer, "ear trainer"))
                break;

            auto& e = *targets.earTrainer;

            if (const auto* v = setting (entry, "exercise"))
            {
                EarTrainer::Exercise exercise;

                if (practicekeys::parse (v->toString(), exercise))
                    e.setExercise (exercise);
                else
                    bad ("exercise", *v);
            }

            if (const auto* v = setting (entry, "difficulty"))
            {
                if (isNumber (*v))
                    e.setDifficulty ((int) *v);
                else
                    bad ("difficulty", *v);
            }

            if (const auto* v = setting (entry, "adaptive"))
                e.setAdaptive ((bool) *v);

            break;
        }

        case PracticeTool::progression:
        {
            if (! need (targets.progression, "progression looper"))
                break;

            if (const auto* v = setting (entry, "text"))
                if (! targets.progression->parse (v->toString()))
                    bad ("text", *v);

            break;
        }

        case PracticeTool::tabReader:        // settings are read by the drawer's TAB tab
        case PracticeTool::sessionRecorder:  // its setup is not a transport (11.3)
        case PracticeTool::numTools:
            break;
    }

    return problems;
}

//==============================================================================
PracticeRoutineRunner::PracticeRoutineRunner (PracticeTargets t) : targets (t) {}

const RoutineEntry* PracticeRoutineRunner::getCurrentEntry() const noexcept
{
    return juce::isPositiveAndBelow (entryIndex, (int) routine.entries.size())
             ? &routine.entries[(size_t) entryIndex] : nullptr;
}

PracticeTool PracticeRoutineRunner::getActiveTool() const noexcept
{
    const auto* e = getCurrentEntry();
    return e != nullptr ? e->tool : PracticeTool::metronome;
}

double PracticeRoutineRunner::getEntryRemainingSeconds() const noexcept
{
    const auto* e = getCurrentEntry();

    if (e == nullptr || ! e->isTimed())
        return 0.0;

    return juce::jmax (0.0, e->durationSeconds - entryElapsed);
}

double PracticeRoutineRunner::countInSecondsFor (const RoutineEntry& entry) const
{
    if (entry.countInBars <= 0 || targets.metronome == nullptr)
        return 0.0;

    // The metronome has already been given the entry's tempo and signature.
    const auto sig = targets.metronome->getTimeSignature();
    return (double) entry.countInBars * sig.beatsPerBar() * 60.0 / juce::jmax (20.0, targets.metronome->getTempo());
}

bool PracticeRoutineRunner::start (const PracticeRoutine& r)
{
    stop();

    if (r.entries.empty())
        return false;

    routine = r;
    beginEntry (0);
    return true;
}

void PracticeRoutineRunner::stop()
{
    if (isActive())
        leaveEntry();

    phase = Phase::idle;
    entryIndex = -1;
    entryElapsed = 0.0;
    countInRemaining = 0.0;
    repetitionsDone = 0;
}

void PracticeRoutineRunner::pause()
{
    if (phase == Phase::countIn || phase == Phase::running)
    {
        phaseBeforePause = phase;
        phase = Phase::paused;
    }
}

void PracticeRoutineRunner::resume()
{
    if (phase == Phase::paused)
        phase = phaseBeforePause;
}

void PracticeRoutineRunner::leaveEntry()
{
    const auto* e = getCurrentEntry();

    if (e == nullptr)
        return;

    // Stop the transport this entry started, so the next entry begins from
    // silence. The next entry turns the metronome back on if it wants it.
    if (targets.metronome != nullptr)
    {
        targets.metronome->stopProgressiveTempo();
        targets.metronome->setEnabled (false);
    }

    if (e->tool == PracticeTool::looper && targets.looper != nullptr)
        targets.looper->stop();

    if (e->tool == PracticeTool::backingTrack && targets.backingTrack != nullptr)
        targets.backingTrack->pause();
}

void PracticeRoutineRunner::beginEntry (int index)
{
    entryIndex = index;
    entryElapsed = 0.0;
    repetitionsDone = 0;

    const auto& entry = routine.entries[(size_t) index];
    lastProblems = applyRoutineSettings (entry, targets);

    // The metronome runs under a metronome entry, under an entry that asks for
    // the click, and through any count-in.
    const auto* click = setting (entry, "click");
    const bool wantsClick = entry.tool == PracticeTool::metronome || (click != nullptr && (bool) *click);

    countInRemaining = countInSecondsFor (entry);

    if (targets.metronome != nullptr)
        targets.metronome->setEnabled (wantsClick || countInRemaining > 0.0);

    phase = countInRemaining > 0.0 ? Phase::countIn : Phase::running;

    if (onEntryStarted != nullptr)
        onEntryStarted (index);

    if (phase == Phase::running)
        startTimer();
}

void PracticeRoutineRunner::startTimer()
{
    const auto* entry = getCurrentEntry();

    if (entry == nullptr)
        return;

    // After a count-in the click stays only where the entry wants it.
    if (targets.metronome != nullptr)
    {
        const auto* click = setting (*entry, "click");
        const bool wantsClick = entry->tool == PracticeTool::metronome || (click != nullptr && (bool) *click);
        targets.metronome->setEnabled (wantsClick);
    }

    if (onTimerStarted != nullptr)
        onTimerStarted (entryIndex);
}

void PracticeRoutineRunner::finish()
{
    leaveEntry();
    phase = Phase::finished;
    entryIndex = (int) routine.entries.size() - 1;

    if (onFinished != nullptr)
        onFinished();
}

void PracticeRoutineRunner::advance (double elapsedSeconds)
{
    double remaining = juce::jmax (0.0, elapsedSeconds);

    // Bounded, so a zero-length entry cannot spin.
    for (int guard = 0; remaining > 0.0 && guard < 10000; ++guard)
    {
        if (phase == Phase::countIn)
        {
            const double used = juce::jmin (remaining, countInRemaining);
            countInRemaining -= used;
            remaining -= used;

            if (countInRemaining <= 1.0e-9)
            {
                countInRemaining = 0.0;
                phase = Phase::running;
                startTimer();
            }

            continue;
        }

        if (phase != Phase::running)
            return;

        const auto* entry = getCurrentEntry();

        if (entry == nullptr)
            return;

        if (! entry->isTimed())
        {
            // A repetition entry waits for the drawer; time only accumulates.
            entryElapsed += remaining;
            return;
        }

        const double left = juce::jmax (0.0, entry->durationSeconds - entryElapsed);
        const double used = juce::jmin (remaining, left);
        entryElapsed += used;
        remaining -= used;

        if (entryElapsed >= entry->durationSeconds - 1.0e-9)
        {
            if (! next())
                return;
        }
    }
}

void PracticeRoutineRunner::completeRepetition()
{
    const auto* entry = getCurrentEntry();

    if (phase != Phase::running || entry == nullptr || entry->isTimed())
        return;

    if (++repetitionsDone >= entry->repetitions)
        next();
}

bool PracticeRoutineRunner::next()
{
    if (! isActive())
        return false;

    if (entryIndex + 1 >= (int) routine.entries.size())
    {
        finish();
        return false;
    }

    leaveEntry();
    beginEntry (entryIndex + 1);
    return true;
}

bool PracticeRoutineRunner::previous()
{
    if (! isActive() || entryIndex <= 0)
        return false;

    leaveEntry();
    beginEntry (entryIndex - 1);
    return true;
}

} // namespace luthier
