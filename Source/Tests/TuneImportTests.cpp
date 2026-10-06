/*  MIDI import into the Tune Builder (midi-export.md 5; tune-builder.md 9.2
    and 15's "Export MIDI: Luthier profile export re-imported", test 15-08;
    error-recovery.md 1: a refused load changes nothing).
*/

#include "TestFramework.h"

#include "../Tune/TuneImport.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    ChordCell chord (const char* symbol, double beats = 4.0)
    {
        ChordCell c;
        ProgressionError error = ProgressionError::none;
        parseChordSymbol (symbol, c, error);
        c.durationBeats = beats;
        return c;
    }

    MelodyNote written (double start, double duration, int pitch, int velocity = 100)
    {
        auto n = MelodyNote::make (start, duration, pitch, velocity);
        n.locked = true;   // what the importer writes: it was played, not generated
        return n;
    }

    /** Verse (2 bars: Am, F, a melody, a manual bass, a countermelody and a
        pad) and Chorus (1 bar: C/E), played Verse x2, Chorus, in A minor. */
    Tune makeRoundTripTune()
    {
        Tune t;
        t.meta.title = "Round Trip";
        t.meta.artist = "Tests";
        t.meta.tempoBpm = 132.0;
        t.meta.keyTonic = 9;
        t.meta.mode = TuneMode::aeolian;

        TuneSection verse;
        verse.name = "Verse";
        verse.lengthBars = 2;
        verse.chords = { chord ("Am"), chord ("F") };
        verse.rhythmPatternId = "Folk Down Up";
        verse.genreKitId = "Folk Fingerstyle";

        MelodyTrack melody;
        melody.source = MelodySource::record;
        melody.notes = { written (0.0, 1.0, 72), written (1.5, 0.5, 74, 90), written (4.0, 2.0, 76, 110) };
        verse.melody = melody;

        verse.bass.mode = BassMode::manual;
        verse.bass.notes = { written (0.0, 2.0, 45, 96), written (4.0, 2.0, 41, 96) };

        // Layers in LayerType order, which is the order the importer writes
        // them back in (and so the order the re-export's events land in).
        TuneLayer pad;
        pad.type = LayerType::pad;
        pad.volume = 0.4;
        verse.layers.push_back (pad);

        TuneLayer counter;
        counter.type = LayerType::countermelody;
        counter.volume = 0.6;
        counter.pan = -0.5;
        counter.notes = { written (2.0, 1.0, 64, 80), written (6.0, 1.0, 65, 80) };
        verse.layers.push_back (counter);

        TuneSection chorus;
        chorus.name = "Chorus";
        chorus.lengthBars = 1;
        chorus.chords = { chord ("C/E") };
        chorus.rhythmPatternId = "Folk Down Up";
        chorus.genreKitId = "Folk Fingerstyle";

        t.addSection (verse);
        t.addSection (chorus);
        t.setSetlist ({ { "Verse", 2 }, { "Chorus", 1 } });
        return t;
    }

    juce::MemoryBlock toBytes (const juce::MidiFile& file)
    {
        juce::MemoryOutputStream out;
        file.writeTo (out, 1);
        return out.getMemoryBlock();
    }

    bool importBytes (const juce::MemoryBlock& bytes, const juce::String& name, Tune& out,
                      juce::String& error, juce::StringArray& warnings, TuneImportOptions options = {})
    {
        return importMidiData (bytes.getData(), bytes.getSize(), name, out, options, error, &warnings);
    }

    void addNote (juce::MidiMessageSequence& track, int channel, int pitch, double startBeat, double beats,
                  int tpq, int velocity = 100)
    {
        track.addEvent (juce::MidiMessage::noteOn (channel, pitch, (juce::uint8) velocity), startBeat * tpq);
        track.addEvent (juce::MidiMessage::noteOff (channel, pitch), (startBeat + beats) * tpq);
    }

    /** Layer of the type, or nullptr. */
    const TuneLayer* layerOf (const TuneSection& s, LayerType type)
    {
        return s.findLayer (type);
    }
}

//==============================================================================
LUTHIER_TEST (TuneImport, aLuthierExportComesBackAsTheTuneItWas)
{
    const auto original = makeRoundTripTune();

    TuneMidiFileOptions exportOptions;
    const auto bytes = toBytes (buildTuneMidiFile (original, exportOptions));

    Tune imported;
    juce::String error;
    juce::StringArray warnings;
    CHECK_MSG (importBytes (bytes, "round-trip.mid", imported, error, warnings), error);

    // ---- meta
    CHECK (imported.meta.title == "Round Trip");
    CHECK (imported.meta.artist == "Tests");
    CHECK_NEAR (imported.meta.tempoBpm, 132.0, 0.01);
    CHECK (imported.meta.timeSigNumerator == 4 && imported.meta.timeSigDenominator == 4);
    CHECK (imported.meta.keyTonic == 9);
    CHECK (imported.meta.mode == TuneMode::aeolian);

    // ---- sections: the repeat folds back into a setlist
    CHECK (imported.getNumSections() == 2);

    if (imported.getNumSections() != 2)
        return;

    const auto& verse = imported.arrangement.sections[0];
    const auto& chorus = imported.arrangement.sections[1];

    CHECK (verse.name == "Verse");
    CHECK (chorus.name == "Chorus");
    CHECK (verse.lengthBars == 2);
    CHECK (chorus.lengthBars == 1);
    CHECK (imported.arrangement.setlist.size() == 2);
    CHECK (imported.arrangement.setlist.size() == 2 && imported.arrangement.setlist[0].section == "Verse"
             && imported.arrangement.setlist[0].repeats == 2);
    CHECK_NEAR (imported.getTotalBeats(), original.getTotalBeats(), 1.0e-6);

    // ---- chords, including the slash chord
    CHECK (verse.chords == original.arrangement.sections[0].chords);
    CHECK (chorus.chords == original.arrangement.sections[1].chords);

    // ---- melody: exact, locked, from Record
    CHECK (verse.melody.has_value());

    if (verse.melody.has_value())
    {
        CHECK (verse.melody->notes == original.arrangement.sections[0].melody->notes);
        CHECK (verse.melody->source == MelodySource::record);
    }

    // ---- bass: the manual line, as manual
    CHECK (verse.bass.mode == BassMode::manual);
    CHECK (verse.bass.notes == original.arrangement.sections[0].bass.notes);
    CHECK (! chorus.bass.isActive());

    // ---- layers: the countermelody verbatim with its level and pan, the pad
    //      as a pad again rather than as its generated notes
    const auto* counter = layerOf (verse, LayerType::countermelody);
    CHECK (counter != nullptr);

    if (counter != nullptr)
    {
        CHECK (counter->enabled);
        CHECK (counter->notes == original.arrangement.sections[0].layers[1].notes);
        CHECK_NEAR (counter->volume, 0.6, 0.01);
        CHECK_NEAR (counter->pan, -0.5, 0.02);
    }

    const auto* pad = layerOf (verse, LayerType::pad);
    CHECK (pad != nullptr);

    if (pad != nullptr)
    {
        CHECK (pad->enabled && pad->notes.empty());
        CHECK_NEAR (pad->volume, 0.4, 0.01);
    }

    CHECK (layerOf (chorus, LayerType::countermelody) == nullptr);

    // Nothing was guessed, so nothing to warn about beyond the file's own facts.
    for (const auto& w : warnings)
        CHECK_MSG (! w.contains ("sorted by what they play"), w);

    // And the tune it makes exports to the same MIDI again.
    CHECK (toBytes (buildTuneMidiFile (imported, exportOptions)) == bytes);
}

LUTHIER_TEST (TuneImport, aSingleTrackExportAndTechniquesRoundTripToo)
{
    auto original = makeRoundTripTune();

    auto& melody = original.arrangement.sections[0].melody->notes;
    melody[0].technique = NoteTechnique::slide;
    melody[1].technique = NoteTechnique::hammerOn;
    melody[2].articulation = NoteArticulation::palmMuted;

    TuneMidiFileOptions exportOptions;
    exportOptions.split = TuneMidiFileOptions::TrackSplit::single;

    Tune imported;
    juce::String error;
    juce::StringArray warnings;
    CHECK_MSG (importBytes (toBytes (buildTuneMidiFile (original, exportOptions)), "single.mid",
                            imported, error, warnings), error);

    CHECK (imported.getNumSections() == 2);

    if (imported.getNumSections() != 2 || ! imported.arrangement.sections[0].melody.has_value())
        return;

    const auto& notes = imported.arrangement.sections[0].melody->notes;
    CHECK (notes == melody);
    CHECK (imported.arrangement.sections[0].chords == original.arrangement.sections[0].chords);
    CHECK (imported.arrangement.sections[0].bass.notes == original.arrangement.sections[0].bass.notes);

    // A per-section split names its tracks after the sections; still ours.
    exportOptions.split = TuneMidiFileOptions::TrackSplit::perSection;
    Tune perSection;
    CHECK_MSG (importBytes (toBytes (buildTuneMidiFile (original, exportOptions)), "sections.mid",
                            perSection, error, warnings), error);
    CHECK (perSection.getNumSections() == 2);
    CHECK (perSection.getNumSections() == 2
             && perSection.arrangement.sections[1].chords == original.arrangement.sections[1].chords);
}

LUTHIER_TEST (TuneImport, aGeneratedBassLineImportsAsAManualOne)
{
    auto original = makeRoundTripTune();
    original.arrangement.sections[0].bass.mode = BassMode::root;
    original.arrangement.sections[0].bass.notes.clear();

    Tune imported;
    juce::String error;
    juce::StringArray warnings;
    CHECK_MSG (importBytes (toBytes (buildTuneMidiFile (original)), "bass.mid", imported, error, warnings), error);

    CHECK (imported.getNumSections() == 2);

    if (imported.getNumSections() != 2)
        return;

    const auto& bass = imported.arrangement.sections[0].bass;
    CHECK (bass.mode == BassMode::manual);
    CHECK (! bass.notes.empty());

    for (const auto& n : bass.notes)
        CHECK (n.locked && n.pitch.isAbsolute() && n.pitch.value < 52);
}

//==============================================================================
LUTHIER_TEST (TuneImport, aGenericFileIsSortedByWhatItsTracksDo)
{
    constexpr int tpq = 480;
    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);

    // Track 0: 100 bpm, 3/4, G major, two marked sections of four bars each.
    juce::MidiMessageSequence meta;
    meta.addEvent (juce::MidiMessage::textMetaEvent (3, "Waltz"), 0.0);
    meta.addEvent (juce::MidiMessage::tempoMetaEvent (600000), 0.0);
    meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (3, 4), 0.0);
    meta.addEvent (juce::MidiMessage::keySignatureMetaEvent (1, false), 0.0);
    meta.addEvent (juce::MidiMessage::textMetaEvent (6, "A"), 0.0);
    meta.addEvent (juce::MidiMessage::textMetaEvent (6, "B"), 12.0 * tpq);
    file.addTrack (meta);

    // Drums on channel 10: skipped.
    juce::MidiMessageSequence drums;
    drums.addEvent (juce::MidiMessage::textMetaEvent (3, "Drums"), 0.0);

    for (int beat = 0; beat < 24; ++beat)
        addNote (drums, 10, 36 + (beat % 3 == 0 ? 0 : 6), (double) beat, 0.25, tpq);

    file.addTrack (drums);

    // Piano on channel 1: block chords, six beats each (two bars of 3/4),
    // strummed a few ticks apart the way a real file is.
    juce::MidiMessageSequence piano;
    piano.addEvent (juce::MidiMessage::textMetaEvent (3, "Piano"), 0.0);

    auto block = [&] (double at, std::initializer_list<int> pitches)
    {
        int k = 0;

        for (int pitch : pitches)
        {
            const double late = 0.01 * (double) k++;
            addNote (piano, 1, pitch, at + late, 6.0 - late - 0.01, tpq, 90);
        }
    };

    block (0.0,  { 55, 59, 62 });   // G
    block (6.0,  { 50, 54, 57 });   // D
    block (12.0, { 48, 52, 55 });   // C
    block (18.0, { 55, 59, 62 });   // G
    file.addTrack (piano);

    // A lead on channel 2: one note per beat, and one held across the section
    // boundary at beat 12.
    juce::MidiMessageSequence lead;
    lead.addEvent (juce::MidiMessage::textMetaEvent (3, "Lead"), 0.0);

    for (int beat = 0; beat < 11; ++beat)
        addNote (lead, 2, 79 + (beat % 4), (double) beat, 0.5, tpq);

    addNote (lead, 2, 74, 11.0, 2.0, tpq);   // 11 .. 13: cut at 12
    file.addTrack (lead);

    // An unnamed low part on channel 3: the bass, by register.
    juce::MidiMessageSequence low;

    for (int beat = 0; beat < 24; beat += 3)
        addNote (low, 3, 31 + (beat % 6), (double) beat, 3.0, tpq, 100);

    file.addTrack (low);

    Tune tune;
    juce::String error;
    juce::StringArray warnings;
    CHECK_MSG (importBytes (toBytes (file), "waltz.mid", tune, error, warnings), error);

    CHECK (tune.meta.title == "Waltz");
    CHECK_NEAR (tune.meta.tempoBpm, 100.0, 0.01);
    CHECK (tune.meta.timeSigNumerator == 3 && tune.meta.timeSigDenominator == 4);
    CHECK (tune.meta.keyTonic == 7 && tune.meta.mode == TuneMode::ionian);
    CHECK (tune.getNumSections() == 2);

    if (tune.getNumSections() != 2)
        return;

    const auto& a = tune.arrangement.sections[0];
    const auto& b = tune.arrangement.sections[1];
    CHECK (a.name == "A" && b.name == "B");
    CHECK (a.lengthBars == 4 && b.lengthBars == 4);
    CHECK (tune.arrangement.setlist.empty());

    // The piano became chords, six beats each.
    CHECK ((a.chords == std::vector<ChordCell> { chord ("G", 6.0), chord ("D", 6.0) }));
    CHECK ((b.chords == std::vector<ChordCell> { chord ("C", 6.0), chord ("G", 6.0) }));

    // ... and is kept, muted, as the countermelody layer.
    const auto* kept = layerOf (a, LayerType::countermelody);
    CHECK (kept != nullptr);

    if (kept != nullptr)
    {
        CHECK (! kept->enabled);
        CHECK (kept->notes.size() == 6);
    }

    // The lead is the melody, with the held note cut at the boundary.
    CHECK (a.melody.has_value() && b.melody.has_value());

    if (a.melody.has_value() && b.melody.has_value())
    {
        CHECK (a.melody->notes.size() == 12);
        CHECK (a.melody->notes.back().pitch.value == 74);
        CHECK_NEAR (a.melody->notes.back().startBeat, 11.0, 1.0e-6);
        CHECK_NEAR (a.melody->notes.back().durationBeats, 1.0, 1.0e-6);
        CHECK (b.melody->notes.size() == 1);
        CHECK_NEAR (b.melody->notes.front().startBeat, 0.0, 1.0e-6);
        CHECK_NEAR (b.melody->notes.front().durationBeats, 1.0, 1.0e-6);
        CHECK (a.melody->notes.front().locked);
    }

    // The low part is the bass line, manual.
    CHECK (a.bass.mode == BassMode::manual);
    CHECK (a.bass.notes.size() == 4);

    // Drums were skipped, and the file said so.
    bool saidDrums = false, saidStrummed = false;

    for (const auto& w : warnings)
    {
        saidDrums = saidDrums || (w.contains ("Drums") && w.contains ("skipped"));
        saidStrummed = saidStrummed || w.contains ("strummed");
    }

    CHECK (saidDrums);
    CHECK (saidStrummed);
    CHECK (layerOf (a, LayerType::percussion) == nullptr);
}

LUTHIER_TEST (TuneImport, aFileWithoutMarkersIsCutIntoPartsAndNotesSplitAtTheCuts)
{
    constexpr int tpq = 96;
    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);

    juce::MidiMessageSequence track;

    // 40 bars of 4/4, one note a bar, and one note across bar 8's line.
    for (int bar = 0; bar < 40; ++bar)
        addNote (track, 1, 60 + (bar % 12), 4.0 * bar, 1.0, tpq);

    addNote (track, 1, 84, 31.0, 2.0, tpq);   // beats 31..33: the cut is at 32
    file.addTrack (track);

    Tune tune;
    juce::String error;
    juce::StringArray warnings;
    CHECK_MSG (importBytes (toBytes (file), "long.mid", tune, error, warnings), error);

    CHECK (tune.getNumSections() == 5);

    for (const auto& s : tune.arrangement.sections)
        CHECK (s.lengthBars == 8 && s.chords.empty());

    if (tune.getNumSections() != 5)
        return;

    CHECK (tune.arrangement.sections[0].name == "Part 1");
    CHECK (tune.arrangement.sections[4].name == "Part 5");

    const auto& first = tune.arrangement.sections[0];
    const auto& second = tune.arrangement.sections[1];
    CHECK (first.melody.has_value() && second.melody.has_value());

    if (first.melody.has_value() && second.melody.has_value())
    {
        CHECK (first.melody->notes.size() == 9);
        CHECK (second.melody->notes.size() == 9);

        // The cut note's second half starts the next part.
        const MelodyNote* rest = nullptr;

        for (const auto& n : second.melody->notes)
            if (n.pitch.value == 84)
                rest = &n;

        CHECK (rest != nullptr);

        if (rest != nullptr)
        {
            CHECK_NEAR (rest->startBeat, 0.0, 1.0e-6);
            CHECK_NEAR (rest->durationBeats, 1.0, 1.0e-6);
        }
    }

    bool saidDefaults = false;

    for (const auto& w : warnings)
        saidDefaults = saidDefaults || w.contains ("No tempo");

    CHECK (saidDefaults);
    CHECK_NEAR (tune.meta.tempoBpm, 120.0, 1.0e-9);

    // A short file is one section.
    juce::MidiFile shortFile;
    shortFile.setTicksPerQuarterNote (tpq);
    juce::MidiMessageSequence few;
    addNote (few, 1, 60, 0.0, 1.0, tpq);
    addNote (few, 1, 62, 9.0, 1.0, tpq);
    shortFile.addTrack (few);

    Tune shortTune;
    CHECK_MSG (importBytes (toBytes (shortFile), "short.mid", shortTune, error, warnings), error);
    CHECK (shortTune.getNumSections() == 1);
    CHECK (shortTune.getNumSections() == 1 && shortTune.arrangement.sections[0].lengthBars == 3);
    CHECK (shortTune.meta.title == "short");
}

LUTHIER_TEST (TuneImport, aChordChartInTextEventsIsUsedWhenNoTrackPlaysChords)
{
    constexpr int tpq = 240;
    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::textMetaEvent (1, "Am7"), 0.0);
    track.addEvent (juce::MidiMessage::textMetaEvent (1, "D7"), 4.0 * tpq);
    track.addEvent (juce::MidiMessage::textMetaEvent (1, "Gmaj7"), 8.0 * tpq);

    for (int beat = 0; beat < 12; ++beat)
        addNote (track, 1, 67 + (beat % 5), (double) beat, 0.5, tpq);

    file.addTrack (track);

    Tune tune;
    juce::String error;
    juce::StringArray warnings;
    CHECK_MSG (importBytes (toBytes (file), "chart.mid", tune, error, warnings), error);
    CHECK (tune.getNumSections() == 1);

    if (tune.getNumSections() != 1)
        return;

    CHECK ((tune.arrangement.sections[0].chords
             == std::vector<ChordCell> { chord ("Am7"), chord ("D7"), chord ("Gmaj7") }));
    CHECK (tune.arrangement.sections[0].melody.has_value());
}

//==============================================================================
LUTHIER_TEST (TuneImport, aCorruptOrMissingFileIsRefusedAndTheTuneIsUntouched)
{
    const auto before = makeRoundTripTune();
    auto tune = before;

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuthierTuneImportTest");
    folder.createDirectory();

    const auto corrupt = folder.getChildFile ("corrupt.mid");
    corrupt.replaceWithText ("MThd this is not a midi file at all, just some text that starts like one");

    juce::String error;
    juce::StringArray warnings;
    CHECK (! importMidiFile (corrupt, tune, {}, error, &warnings));
    CHECK (error.contains ("corrupt.mid"));
    CHECK (tune == before);

    error.clear();
    CHECK (! importMidiFile (folder.getChildFile ("missing.mid"), tune, {}, error, &warnings));
    CHECK (error.contains ("missing.mid") && error.contains ("not found"));
    CHECK (tune == before);

    // Well-formed but empty: refused too, by name.
    juce::MidiFile silent;
    silent.setTicksPerQuarterNote (96);
    juce::MidiMessageSequence meta;
    meta.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);
    silent.addTrack (meta);

    error.clear();
    CHECK (! importBytes (toBytes (silent), "silent.mid", tune, error, warnings));
    CHECK (error.contains ("silent.mid") && error.contains ("no notes"));
    CHECK (tune == before);

    // Garbage from memory.
    const char junk[] = "not midi";
    error.clear();
    CHECK (! importMidiData (junk, sizeof (junk), "junk.mid", tune, {}, error, &warnings));
    CHECK (error.contains ("junk.mid"));
    CHECK (tune == before);

    // Unlike a refusal, a good file does write it.
    const auto good = folder.getChildFile ("good.mid");
    CHECK (writeTuneMidiFile (before, good, {}, error));
    CHECK_MSG (importMidiFile (good, tune, {}, error, &warnings), error);
    CHECK (tune.meta.title == "Round Trip");

    folder.deleteRecursively();
}

LUTHIER_TEST (TuneImport, smpteTimingIsRefused)
{
    juce::MidiFile file;
    file.setSmpteTimeFormat (25, 40);

    juce::MidiMessageSequence track;
    addNote (track, 1, 60, 0.0, 1.0, 1000);
    file.addTrack (track);

    const auto before = makeRoundTripTune();
    auto tune = before;

    juce::String error;
    juce::StringArray warnings;
    CHECK (! importBytes (toBytes (file), "smpte.mid", tune, error, warnings));
    CHECK (error.contains ("SMPTE"));
    CHECK (error.contains ("smpte.mid"));
    CHECK (tune == before);
}

LUTHIER_TEST (TuneImport, quantiseSnapsMelodyAndBassWhenAsked)
{
    constexpr int tpq = 960;
    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);

    juce::MidiMessageSequence track;
    addNote (track, 1, 72, 0.03, 0.47, tpq);
    addNote (track, 1, 74, 1.02, 0.52, tpq);
    addNote (track, 1, 76, 2.26, 0.5, tpq);
    file.addTrack (track);

    TuneImportOptions options;
    options.quantise = true;
    options.grid = QuantiseGrid::eighth;

    Tune tune;
    juce::String error;
    juce::StringArray warnings;
    CHECK_MSG (importBytes (toBytes (file), "loose.mid", tune, error, warnings, options), error);
    CHECK (tune.getNumSections() == 1 && tune.arrangement.sections[0].melody.has_value());

    if (! (tune.getNumSections() == 1 && tune.arrangement.sections[0].melody.has_value()))
        return;

    const auto& notes = tune.arrangement.sections[0].melody->notes;
    CHECK (notes.size() == 3);

    for (const auto& n : notes)
    {
        CHECK (n.locked);
        CHECK_NEAR (std::fmod (n.startBeat, 0.5), 0.0, 1.0e-6);
    }

    if (notes.size() == 3)
        CHECK_NEAR (notes[2].startBeat, 2.5, 1.0e-6);
}

//==============================================================================
/*  A marker section rounded to whole bars keeps the notes of its own span:
    rounded up, it does not take the next section's first notes as well;
    rounded down with notes in the cut-off tail, it grows a bar rather than
    dropping them. And a chord chart is read in time order whatever order its
    tracks came in. */
LUTHIER_TEST (TuneImport, aSectionRoundedUpDoesNotTakeTheNextSectionsNotes)
{
    constexpr int tpq = 480;   // 14.4 and 17.6 beats land on whole ticks
    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::textMetaEvent (6, "A"), 0.0);
    track.addEvent (juce::MidiMessage::textMetaEvent (6, "B"), 14.4 * tpq);   // 3.6 bars: rounds to 4

    for (double beat : { 0.0, 4.0, 8.0, 12.0 })
        addNote (track, 1, 60, beat, 1.0, tpq);

    for (double beat : { 15.0, 16.5, 18.0 })   // B's; 15.0 lies inside A's rounded fourth bar
        addNote (track, 1, 72, beat, 0.5, tpq);

    file.addTrack (track);

    Tune tune;
    juce::String error;
    juce::StringArray warnings;
    CHECK_MSG (importBytes (toBytes (file), "up.mid", tune, error, warnings), error);
    CHECK (tune.getNumSections() == 2);

    if (tune.getNumSections() != 2)
        return;

    const auto& a = tune.arrangement.sections[0];
    const auto& b = tune.arrangement.sections[1];
    CHECK (a.lengthBars == 4);
    CHECK (a.melody.has_value() && b.melody.has_value());

    if (a.melody.has_value() && b.melody.has_value())
    {
        CHECK_MSG (a.melody->notes.size() == 4, "A holds " + juce::String ((int) a.melody->notes.size()) + " notes, not its own 4");

        for (const auto& n : a.melody->notes)
            CHECK_MSG (n.pitch.value == 60, "A took one of B's notes");

        CHECK (b.melody->notes.size() == 3);

        if (! b.melody->notes.empty())
            CHECK_NEAR (b.melody->notes.front().startBeat, 0.6, 1.0e-6);
    }
}

LUTHIER_TEST (TuneImport, aSectionRoundedDownKeepsTheNotesInItsTail)
{
    constexpr int tpq = 480;   // 14.4 and 17.6 beats land on whole ticks
    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::textMetaEvent (6, "A"), 0.0);
    track.addEvent (juce::MidiMessage::textMetaEvent (6, "B"), 17.6 * tpq);   // 4.4 bars: would round to 4

    for (double beat : { 0.0, 4.0, 8.0, 12.0, 16.5 })   // 16.5 starts in the tail past bar 4
        addNote (track, 1, 60, beat, 1.0, tpq);

    for (double beat : { 18.0, 20.0 })
        addNote (track, 1, 72, beat, 0.5, tpq);

    file.addTrack (track);

    Tune tune;
    juce::String error;
    juce::StringArray warnings;
    CHECK_MSG (importBytes (toBytes (file), "down.mid", tune, error, warnings), error);
    CHECK (tune.getNumSections() == 2);

    if (tune.getNumSections() != 2)
        return;

    const auto& a = tune.arrangement.sections[0];
    const auto& b = tune.arrangement.sections[1];
    CHECK_MSG (a.lengthBars == 5, "A is " + juce::String (a.lengthBars) + " bars; its fifth holds a note");
    CHECK (a.melody.has_value() && b.melody.has_value());

    if (a.melody.has_value() && b.melody.has_value())
    {
        CHECK_MSG (a.melody->notes.size() == 5, "A holds " + juce::String ((int) a.melody->notes.size()) + " notes; the tail note was dropped");
        CHECK (b.melody->notes.size() == 2);

        if (! b.melody->notes.empty())
            CHECK_NEAR (b.melody->notes.front().startBeat, 0.4, 1.0e-6);
    }

    // A section rounded down with nothing in its tail is still rounded down.
    juce::MidiFile plain;
    plain.setTicksPerQuarterNote (tpq);
    juce::MidiMessageSequence one;
    one.addEvent (juce::MidiMessage::textMetaEvent (6, "A"), 0.0);
    one.addEvent (juce::MidiMessage::textMetaEvent (6, "B"), 17.6 * tpq);
    addNote (one, 1, 60, 0.0, 1.0, tpq);
    addNote (one, 1, 72, 18.0, 1.0, tpq);
    plain.addTrack (one);

    Tune plainTune;
    CHECK_MSG (importBytes (toBytes (plain), "plain.mid", plainTune, error, warnings), error);
    CHECK (plainTune.getNumSections() == 2 && plainTune.arrangement.sections[0].lengthBars == 4);
}

LUTHIER_TEST (TuneImport, aChordChartSpreadOverTracksIsReadInTimeOrder)
{
    constexpr int tpq = 240;
    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);

    // The later chords on the first track, the first chord on the second:
    // track order is not time order.
    juce::MidiMessageSequence later, first;
    later.addEvent (juce::MidiMessage::textMetaEvent (1, "D7"), 4.0 * tpq);
    later.addEvent (juce::MidiMessage::textMetaEvent (1, "Gmaj7"), 8.0 * tpq);
    first.addEvent (juce::MidiMessage::textMetaEvent (1, "Am7"), 0.0);

    for (int beat = 0; beat < 12; ++beat)
        addNote (first, 1, 67 + (beat % 5), (double) beat, 0.5, tpq);

    file.addTrack (later);
    file.addTrack (first);

    Tune tune;
    juce::String error;
    juce::StringArray warnings;
    CHECK_MSG (importBytes (toBytes (file), "chart.mid", tune, error, warnings), error);
    CHECK (tune.getNumSections() == 1);

    if (tune.getNumSections() != 1)
        return;

    CHECK ((tune.arrangement.sections[0].chords
             == std::vector<ChordCell> { chord ("Am7"), chord ("D7"), chord ("Gmaj7") }));
}
