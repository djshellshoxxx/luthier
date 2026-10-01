#include "TuneExamples.h"
#include "TuneHarmony.h"
#include "TuneMelody.h"
#include "TuneMidi.h"
#include "../Live/Setlist.h"
#include "../Presets/PresetManager.h"
#include "../Rhythm/GenreKit.h"
#include "../Support/IrLibrary.h"

namespace luthier
{

//==============================================================================
// Kit suggestions (tune-builder 2.1)
//==============================================================================
namespace
{
    struct KitRow
    {
        const char* kit;
        double tempo;
        double swing;
        double feel;
        const char* palette;   // space-separated roman numerals
    };

    /*  One row per factory kit (rhythm-engine 7). Tempi are where each style
        usually sits; swing is the tune's shuffle; the palette is the chords the
        style leans on, in the order a progression usually visits them. */
    const KitRow kKitRows[] =
    {
        { "Nashville Country",        110, 0,  0.50, "I IV V I vi IV V7" },
        { "Modern Country",           118, 0,  0.45, "I V vi IV ii" },
        { "Bluegrass Flatpick",       140, 0,  0.40, "I IV I V7 I" },
        { "Delta Blues",               80, 60, 0.65, "I7 IV7 I7 V7" },
        { "Chicago Blues",             96, 55, 0.55, "I7 IV7 I7 V7 IV7" },
        { "Modern Blues Rhythm",      100, 30, 0.50, "i7 iv7 i7 bVI7 V7" },
        { "Folk Fingerstyle",          92, 0,  0.55, "I V vi IV" },
        { "Contemporary Fingerstyle",  84, 0,  0.60, "Imaj7 vi7 IVmaj7 V" },
        { "Piedmont",                 104, 20, 0.55, "I VI7 II7 V7" },
        { "Bossa Nova",               132, 0,  0.45, "Imaj7 vi7 ii7 V7" },
        { "Samba",                    150, 0,  0.40, "ii7 V7 Imaj7 VI7" },
        { "Latin Ballad",              76, 0,  0.55, "i iv V7 i" },
        { "Reggae Skank",              76, 20, 0.45, "I IV V IV" },
        { "Rocksteady",                84, 25, 0.45, "I vi IV V" },
        { "Dub",                       70, 20, 0.50, "i iv i v" },
        { "Funk 16th",                100, 15, 0.40, "I7 IV7 I7 bVII7" },
        { "Funk Wah Rhythm",           96, 15, 0.45, "i7 IV7 i7 IV7" },
        { "Punk Downstroke",          180, 0,  0.30, "I IV V IV" },
        { "Metal Chug",               150, 0,  0.25, "i bVI bVII i" },
        { "Djent Palm-Mute Grid",     130, 0,  0.20, "i bII i bVII" },
        { "Jazz Comping",             120, 60, 0.55, "ii7 V7 Imaj7 vi7" },
        { "Gypsy Jazz La Pompe",      180, 55, 0.45, "i6 iv6 V7 i6" },
        { "Flamenco Rasgueado",       120, 0,  0.50, "iv bIII bII I" },
        { "Flamenco Alzapua",         110, 0,  0.50, "i bVII bVI V" },
        { "Classical Etude",           72, 0,  0.60, "i iv V7 i VI" },
        { "Classical Arpeggio",        66, 0,  0.60, "I vi ii V7" },
        { "Ambient Swell",             60, 0,  0.70, "Imaj7 IVmaj7 vi7 IVmaj7" },
        { "Post-Rock Arpeggio",        88, 0,  0.60, "I V vi IV" }
    };

    /** A roman numeral as a chord symbol in the key: "bVII7" in C is "Bb7". */
    bool numeralToCell (juce::String numeral, int tonic, TuneMode mode, double beats, ChordCell& cell)
    {
        numeral = numeral.trim();
        int offset = 0;

        while (numeral.startsWithChar ('b') || numeral.startsWithChar ('#'))
        {
            offset += numeral.startsWithChar ('b') ? -1 : 1;
            numeral = numeral.substring (1);
        }

        static const char* const romans[] = { "vii", "iii", "vi", "iv", "ii", "v", "i" };
        static const int degrees[] = { 7, 3, 6, 4, 2, 5, 1 };

        int degree = 0;
        bool upper = false;
        juce::String rest;

        for (int i = 0; i < 7; ++i)
        {
            if (numeral.startsWithIgnoreCase (romans[i]))
            {
                degree = degrees[i];
                upper = numeral.substring (0, 1) == numeral.substring (0, 1).toUpperCase();
                rest = numeral.substring ((int) std::strlen (romans[i]));
                break;
            }
        }

        if (degree == 0)
            return false;

        // Numerals are counted on the major scale; the flats and sharps move them.
        static const int major[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int root = tunetheory::wrapPitchClass (tonic + major[degree - 1] + offset);

        juce::String quality;

        if (rest == "7")          quality = upper ? "7" : "m7";
        else if (rest == "maj7")  quality = "maj7";
        else if (rest == "6")     quality = upper ? "6" : "m6";
        else if (rest == "o" || rest == "dim") quality = "dim";
        else if (rest.isEmpty())  quality = upper ? "" : "m";
        else                      return false;

        ProgressionError error;
        const auto symbol = tunetheory::spellPitchClass (root, tunetheory::keyPrefersFlats (tonic, mode)) + quality;

        if (! parseChordSymbol (symbol, cell, error))
            return false;

        cell.durationBeats = beats;
        return true;
    }

    /** The kit's first strum pattern, else its first fingerpick pattern. */
    juce::String firstPatternOf (const juce::String& kitName)
    {
        static const GenreKitLibrary kits;
        const int index = kits.indexOf (kitName);

        if (index < 0)
            return {};

        const auto& kit = kits.getKit (index);
        return ! kit.strumPatterns.isEmpty() ? kit.strumPatterns[0]
             : (! kit.fingerpickPatterns.isEmpty() ? kit.fingerpickPatterns[0] : juce::String());
    }

    void setKit (TuneSection& section, const juce::String& kit)
    {
        section.genreKitId = kit;
        section.rhythmPatternId = firstPatternOf (kit);
        section.feel = TuneKits::getSuggestion (kit).feel;
    }

    constexpr const char* kStamp = "2026-09-24T00:00:00Z";

    Tune makeTune (const juce::String& title, double tempo, int tonic, TuneMode mode, const juce::String& notes,
                   const juce::String& tag)
    {
        Tune t;
        t.meta.title = title;
        t.meta.author = "Factory";
        t.meta.tempoBpm = tempo;
        t.meta.keyTonic = tonic;
        t.meta.mode = mode;
        t.meta.notes = notes;
        t.meta.tags = { "example", tag };
        t.meta.created = kStamp;
        t.meta.modified = kStamp;
        return t;
    }

    void setlistOf (Tune& t, std::initializer_list<std::pair<const char*, int>> steps)
    {
        std::vector<TuneSetlistEntry> entries;

        for (const auto& [name, repeats] : steps)
        {
            TuneSetlistEntry e;
            e.section = name;
            e.repeats = repeats;
            entries.push_back (e);
        }

        t.setSetlist (entries);
    }

    void melodyFor (Tune& t, int section, int seed, MelodyStyle style = MelodyStyle::none)
    {
        t.setMelodyEnabled (section, true);
        t.getSection (section)->melody->seed = seed;
        t.getSection (section)->melody->density = getKitMelodyDensity (t.getSection (section)->genreKitId);
        generateMelody (t, section);
        t.getSection (section)->melody->source = MelodySource::autoGenerate;
        t.getSection (section)->style = style;
    }
}

TuneKits::Suggestion TuneKits::getSuggestion (const juce::String& genreKitName)
{
    Suggestion s;

    for (const auto& row : kKitRows)
    {
        if (genreKitName.equalsIgnoreCase (row.kit))
        {
            s.tempoBpm = row.tempo;
            s.swingPercent = row.swing;
            s.feel = row.feel;
            s.palette = juce::StringArray::fromTokens (row.palette, " ", {});
            return s;
        }
    }

    s.palette = { "I", "IV", "V", "vi" };
    return s;
}

std::vector<ChordCell> TuneKits::resolvePalette (const juce::StringArray& numerals, int tonic, TuneMode mode, double beats)
{
    std::vector<ChordCell> cells;

    for (const auto& n : numerals)
    {
        ChordCell cell;

        if (numeralToCell (n, tonic, mode, beats, cell))
            cells.push_back (cell);
    }

    return cells;
}

//==============================================================================
// Example tunes (onboarding 6)
//==============================================================================
std::vector<TuneExamples::Example> TuneExamples::buildExampleTunes()
{
    std::vector<Example> examples;

    // 1. A fingerstyle etude: fingerpicked arpeggios, a classical-guitar phrasing.
    {
        auto t = makeTune ("Fingerstyle Etude", 72.0, 4, TuneMode::aeolian,
                           "Two eight-bar halves over a fingerpicked arpeggio, the melody phrased like a classical "
                           "guitarist. Try it on Nylon Classical.", "fingerstyle");
        applyProgressionText (t, 0, "[A] Em Am D G C Am B7 Em [B] C G Am Em Am D B7 Em");

        for (int i = 0; i < t.getNumSections(); ++i)
            setKit (*t.getSection (i), "Classical Arpeggio");

        t.setSectionRole (0, SectionRole::verse);
        t.setSectionRole (1, SectionRole::bridge);
        melodyFor (t, 0, 3, MelodyStyle::classicalGuitar);
        melodyFor (t, 1, 5, MelodyStyle::classicalGuitar);
        t.setBassMode (0, BassMode::root);
        t.setBassMode (1, BassMode::root);
        setlistOf (t, { { "A", 2 }, { "B", 1 }, { "A", 1 } });
        examples.push_back ({ "01-fingerstyle-etude.luthiertune", t });
    }

    // 2. A jazz standard: AABA with a walking bass and a sax-phrased head.
    {
        auto t = makeTune ("Late Night Standard", 132.0, 10, TuneMode::ionian,
                           "An AABA head over ii-V changes: comping, a walking bass and a melody phrased like a "
                           "tenor sax. Try it on Jazz Hollowbody.", "jazz");
        t.meta.swingPercent = 60.0;
        applyProgressionText (t, 0, "[A] Bbmaj7 Gm7 Cm7 F7 Dm7 G7 Cm7 F7 [B] Fm7 Bb7 Ebmaj7 Ebmaj7 Am7 D7 Cm7 F7");

        for (int i = 0; i < t.getNumSections(); ++i)
        {
            setKit (*t.getSection (i), "Jazz Comping");
            t.setBassMode (i, BassMode::walking);
        }

        t.setSectionRole (0, SectionRole::verse);
        t.setSectionRole (1, SectionRole::bridge);
        melodyFor (t, 0, 11, MelodyStyle::jazzSax);
        melodyFor (t, 1, 12, MelodyStyle::jazzSax);
        setlistOf (t, { { "A", 2 }, { "B", 1 }, { "A", 1 } });
        examples.push_back ({ "02-late-night-standard.luthiertune", t });
    }

    // 3. A folk sketch: verse and chorus with a pad and a countermelody layer.
    {
        auto t = makeTune ("Morning Folk Sketch", 92.0, 7, TuneMode::ionian,
                           "Verse and chorus, strummed, with a soft pad under the chorus and a countermelody "
                           "answering the tune. Try it on Strummed Dreadnought.", "folk");
        applyProgressionText (t, 0, "[Verse] G D Em C G D C G [Chorus] C G D G C G D D");

        for (int i = 0; i < t.getNumSections(); ++i)
        {
            setKit (*t.getSection (i), "Folk Fingerstyle");
            t.setBassMode (i, BassMode::rootFifth);
            melodyFor (t, i, 21 + i);
        }

        t.setSectionRole (0, SectionRole::verse);
        t.setSectionRole (1, SectionRole::chorus);

        TuneLayer pad;
        pad.type = LayerType::pad;
        pad.volume = 0.5;
        pad.pan = -0.3;
        t.setLayer (1, pad);

        TuneLayer counter;
        counter.type = LayerType::countermelody;
        counter.volume = 0.6;
        counter.pan = 0.3;
        counter.seed = 4;
        counter.notes = generateCountermelody (t, 1, 4);
        t.setLayer (1, counter);

        setlistOf (t, { { "Verse", 1 }, { "Chorus", 1 }, { "Verse", 1 }, { "Chorus", 2 } });
        examples.push_back ({ "03-morning-folk-sketch.luthiertune", t });
    }

    // 4. A metal riff: palm-muted chugs with the percussion layer.
    {
        auto t = makeTune ("Iron Riff", 150.0, 4, TuneMode::aeolian,
                           "A palm-muted chug with a driving bass and the percussion layer (chucks and mutes, no "
                           "drums). Try it on Modern Metal Chug or 8-String Djent.", "metal");
        applyProgressionText (t, 0, "[Riff] Em Em C D Em Em C B [Break] Am C D Em");

        for (int i = 0; i < t.getNumSections(); ++i)
        {
            setKit (*t.getSection (i), "Metal Chug");
            t.setBassMode (i, BassMode::genre);

            TuneLayer percussion;
            percussion.type = LayerType::percussion;
            percussion.volume = 0.7;
            t.setLayer (i, percussion);
        }

        t.setSectionRole (0, SectionRole::verse);
        t.setSectionRole (1, SectionRole::bridge);
        melodyFor (t, 1, 31);
        t.getSection (1)->stateBoundary = true;
        setlistOf (t, { { "Riff", 2 }, { "Break", 1 }, { "Riff", 1 } });
        examples.push_back ({ "04-iron-riff.luthiertune", t });
    }

    // 5. A slide blues: twelve bars in A, the melody slid into on a bottleneck.
    {
        auto t = makeTune ("Bottleneck Blues", 80.0, 9, TuneMode::ionian,
                           "Twelve-bar blues in A with a shuffle. The melody's notes are marked as slides: turn "
                           "Slide Mode on (the header's Slide, or S) and play it on Blues Slide.", "blues");
        t.meta.swingPercent = 60.0;
        applyProgressionText (t, 0, "[Blues] A7 A7 A7 A7 D7 D7 A7 A7 E7 D7 A7 E7");
        setKit (*t.getSection (0), "Delta Blues");
        t.setSectionRole (0, SectionRole::verse);
        melodyFor (t, 0, 41, MelodyStyle::bluesGuitar);

        auto& notes = t.getSection (0)->melody->notes;

        for (size_t i = 0; i < notes.size(); ++i)
            if (i % 2 == 0)
                notes[i].technique = NoteTechnique::slide;
            else if (i % 5 == 1)
                notes[i].technique = NoteTechnique::vibrato;

        t.setBassMode (0, BassMode::genre);
        setlistOf (t, { { "Blues", 3 } });
        examples.push_back ({ "05-bottleneck-blues.luthiertune", t });
    }

    // 6. A funk-slap bass line: a manual bass part of thumbed roots and popped octaves.
    {
        auto t = makeTune ("Slap Street", 100.0, 4, TuneMode::mixolydian,
                           "A sixteenth-note funk groove whose bass line is written out: thumbed roots, popped "
                           "octaves and ghosted dead notes. Play it on a bass (P-Bass Flatwound or J-Style "
                           "Fingerstyle) with slap on; on a guitar it goes out as MIDI.", "funk");
        applyProgressionText (t, 0, "[Groove] E7 E7 A7 E7 [Turn] A7 A7 B7 B7");

        for (int i = 0; i < t.getNumSections(); ++i)
        {
            setKit (*t.getSection (i), "Funk 16th");
            t.setBassMode (i, BassMode::manual);
        }

        // One bar per chord: thumb (root), ghost, pop (octave), thumb, pop, ghost ...
        for (int i = 0; i < t.getNumSections(); ++i)
        {
            std::vector<MelodyNote> bass;
            const auto spans = resolveChordSpans (*t.getSection (i), t.getBeatsPerBar());

            for (const auto& span : spans)
            {
                const int root = 28 + tunetheory::wrapPitchClass (t.getSection (i)->chords[(size_t) span.cellIndex].root - 4);
                const struct { double at; int octave; int velocity; NoteArticulation art; } hits[] =
                {
                    { 0.00, 0, 112, NoteArticulation::natural },
                    { 0.50, 0,  40, NoteArticulation::palmMuted },
                    { 0.75, 1, 105, NoteArticulation::staccato },
                    { 1.50, 0, 100, NoteArticulation::natural },
                    { 2.00, 1, 108, NoteArticulation::staccato },
                    { 2.50, 0,  38, NoteArticulation::palmMuted },
                    { 3.00, 0, 104, NoteArticulation::natural },
                    { 3.50, 1, 110, NoteArticulation::staccato }
                };

                for (const auto& h : hits)
                {
                    auto note = MelodyNote::make (span.startBeat + h.at, 0.25, root + 12 * h.octave, h.velocity);
                    note.articulation = h.art;
                    note.locked = true;
                    bass.push_back (note);
                }
            }

            t.setBassNotes (i, bass);
        }

        t.setSectionRole (0, SectionRole::verse);
        t.setSectionRole (1, SectionRole::bridge);
        setlistOf (t, { { "Groove", 2 }, { "Turn", 1 }, { "Groove", 1 } });
        examples.push_back ({ "06-slap-street.luthiertune", t });
    }

    return examples;
}

juce::File TuneExamples::getExampleDirectory()
{
    const auto resources = IrLibrary::getResourcesFolder();
    return resources == juce::File() ? juce::File() : resources.getChildFile ("Tunes").getChildFile ("Examples");
}

std::vector<TuneTemplate> TuneExamples::loadExamples (juce::StringArray* errors)
{
    return TuneTemplateLibrary::loadDirectory (getExampleDirectory(), errors);
}

//==============================================================================
// MIDI clips (onboarding 6)
//==============================================================================
std::vector<TuneExamples::Clip> TuneExamples::buildMidiClips()
{
    // Twelve kits across the styles the factory bank covers, each in a key its
    // players reach for.
    const struct { const char* kit; int tonic; TuneMode mode; } picks[] =
    {
        { "Nashville Country",  7, TuneMode::ionian },
        { "Chicago Blues",      4, TuneMode::ionian },
        { "Folk Fingerstyle",   2, TuneMode::ionian },
        { "Bossa Nova",         5, TuneMode::ionian },
        { "Reggae Skank",       9, TuneMode::ionian },
        { "Funk 16th",          4, TuneMode::mixolydian },
        { "Punk Downstroke",    9, TuneMode::ionian },
        { "Metal Chug",         4, TuneMode::aeolian },
        { "Jazz Comping",       0, TuneMode::ionian },
        { "Flamenco Rasgueado", 4, TuneMode::phrygian },
        { "Classical Arpeggio", 0, TuneMode::ionian },
        { "Ambient Swell",      2, TuneMode::ionian }
    };

    std::vector<Clip> clips;
    int n = 1;

    for (const auto& p : picks)
    {
        const auto suggestion = TuneKits::getSuggestion (p.kit);
        auto t = makeTune (juce::String (p.kit) + " clip", suggestion.tempoBpm, p.tonic, p.mode,
                           "Four bars in the " + juce::String (p.kit) + " kit.", "clip");
        t.meta.swingPercent = suggestion.swingPercent;

        TuneSection section;
        section.name = "Clip";
        section.lengthBars = 4;
        auto cells = TuneKits::resolvePalette (suggestion.palette, p.tonic, p.mode, t.getBeatsPerBar());

        if (cells.size() > 4)
            cells.resize (4);

        section.chords = cells;
        setKit (section, p.kit);
        t.addSection (section);
        t.setBassMode (0, BassMode::genre);
        melodyFor (t, 0, n);

        const auto slug = juce::String (p.kit).toLowerCase().replaceCharacters (" -", "__").replace ("_", "-");
        clips.push_back ({ juce::String (n).paddedLeft ('0', 2) + "-" + slug + ".mid", p.kit, t });
        ++n;
    }

    return clips;
}

juce::File TuneExamples::getMidiClipDirectory()
{
    const auto resources = IrLibrary::getResourcesFolder();
    return resources == juce::File() ? juce::File() : resources.getChildFile ("Examples");
}

juce::MemoryBlock TuneExamples::renderMidiClip (const Clip& clip)
{
    const auto file = buildTuneMidiFile (clip.tune);
    juce::MemoryBlock block;
    juce::MemoryOutputStream out (block, false);
    file.writeTo (out);
    out.flush();
    return block;
}

int TuneExamples::writeShippedContent (const juce::File& resources, juce::String& error)
{
    int written = 0;
    const auto tunes = resources.getChildFile ("Tunes").getChildFile ("Examples");
    const auto midi = resources.getChildFile ("Examples");
    tunes.createDirectory();
    midi.createDirectory();

    for (const auto& e : buildExampleTunes())
    {
        if (! TuneFile::save (e.tune, tunes.getChildFile (e.fileName), error, false))
            return written;

        ++written;
    }

    for (const auto& c : buildMidiClips())
    {
        const auto bytes = renderMidiClip (c);

        if (! midi.getChildFile (c.fileName).replaceWithData (bytes.getData(), bytes.getSize()))
        {
            error = "Could not write " + c.fileName;
            return written;
        }

        ++written;
    }

    return written;
}

//==============================================================================
// Example setlists (onboarding 6)
//==============================================================================
std::vector<Setlist> TuneExamples::buildExampleSetlists (const PresetManager& presets)
{
    const struct { const char* name; double bpm; const char* notes; std::initializer_list<const char*> entries; } lists[] =
    {
        { "Example - Rock Night", 120, "A club set: crunch, lead, drop tuning, fuzz, and surf to close.",
          { "Single-Cut Crunch", "Shred Lead", "Drop C Riff", "Germanium Fuzz Lead", "Surf Reverb" } },
        { "Example - Acoustic Set", 92, "Fingerpicked to strummed to open tunings.",
          { "Fingerstyle Folk", "Strummed Dreadnought", "12-String Jangle", "Parlor Blues", "DADGAD Drone" } },
        { "Example - Blues Club", 88, "Slide, parlor, crunch and chime.",
          { "Blues Slide", "Parlor Blues", "Single-Cut Crunch", "Semi-Hollow Chime" } },
        { "Example - Country Hour", 110, "Twang, high-strung and bluegrass.",
          { "T-Style Country Twang", "Nashville High-Strung", "Jumbo Bluegrass", "Strummed Dreadnought" } },
        { "Example - Jazz Trio", 132, "Hollowbody comping, a chiming semi, and a nylon ballad.",
          { "Jazz Hollowbody", "Semi-Hollow Chime", "Nylon Classical" } },
        { "Example - Metal Set", 150, "Chugs, extended range and a solo spot.",
          { "Modern Metal Chug", "8-String Djent", "Drop C Riff", "Shred Lead" } },
        { "Example - Funk and Soul", 100, "Clean funk, wah, a bass feature and a slap-back number.",
          { "Clean Double-Cut Funk", "Wah Funk Rhythm", "J-Style Fingerstyle", "Rockabilly Slap" } },
        { "Example - Classical Recital", 72, "Nylon, flamenco, and a steel-string encore.",
          { "Nylon Classical", "Flamenco Rasgueado", "Fingerstyle Folk" } },
        { "Example - Bass Player", 96, "Every bass in the bank.",
          { "P-Bass Flatwound", "J-Style Fingerstyle", "Fretless Mwah", "Violin Bass Grind", "5-String Low B" } },
        { "Example - Ambient and Experimental", 70, "Swells, fuzz, tapping and the physics showcase.",
          { "Ambient Swell", "Octave Fuzz Stoner", "Physics Showcase", "Tapping Etude", "Transposing Trem Chords" } }
    };

    std::vector<Setlist> result;

    for (const auto& list : lists)
    {
        Setlist s;
        s.setName (list.name);
        s.setNotes (list.notes);
        s.setDefaultBpm (list.bpm);

        for (const auto* presetName : list.entries)
        {
            const int index = presets.indexOfPreset (presetName);

            if (const auto* info = presets.getPreset (index); info != nullptr && info->file.existsAsFile())
            {
                SetlistEntry entry;
                entry.presetPath = info->file.getFullPathName();
                entry.snapshotIndex = 0;
                s.addEntry (entry);
            }
        }

        result.push_back (s);
    }

    return result;
}

int TuneExamples::installExampleSetlists (const PresetManager& presets, const juce::File& directory)
{
    const auto folder = directory != juce::File() ? directory : Setlist::getUserDirectory();
    int written = 0;

    for (const auto& s : buildExampleSetlists (presets))
    {
        const auto file = folder.getChildFile (juce::File::createLegalFileName (s.getName()) + Setlist::kFileExtension);

        if (file.existsAsFile() || s.getNumEntries() == 0)
            continue;

        if (s.saveTo (file))
            ++written;
    }

    return written;
}

} // namespace luthier
