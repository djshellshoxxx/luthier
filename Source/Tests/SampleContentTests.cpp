/*  onboarding.md 6's sample content and tune-builder.md 2.1's kit suggestions.
    TUNE-HELP-ONBOARDING workstream.

    The example tunes and MIDI clips are built in code (TuneExamples) and
    shipped as files; these tests hold the two in step. To regenerate the files
    after changing TuneExamples:

        LUTHIER_WRITE_SAMPLE_CONTENT=/path/to/repo/Resources LuthierTests SampleContent
*/

#include "TestFramework.h"

#include "../Live/Setlist.h"
#include "../PluginProcessor.h"
#include "../Practice/BackingTrackLibrary.h"
#include "../Presets/FactoryPresets.h"
#include "../Presets/PresetManager.h"
#include "../Rhythm/GenreKit.h"
#include "../Rhythm/Patterns.h"
#include "../Tune/TuneExamples.h"
#include "../Tune/TuneMelody.h"
#include "../Tune/TuneMidi.h"
#include "../Tune/TuneTemplates.h"
#include "../UI/TunePanel.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  The generator: writes the shipped files when asked to, and does nothing
    otherwise. Run first in the suite so the checks below see what it wrote. */
LUTHIER_TEST (SampleContent, aaGeneratorWritesTheShippedFilesWhenAsked)
{
    const auto target = juce::SystemStats::getEnvironmentVariable ("LUTHIER_WRITE_SAMPLE_CONTENT", {});

    if (target.isEmpty())
    {
        CHECK (true);
        return;
    }

    juce::String error;
    const int written = TuneExamples::writeShippedContent (juce::File (target), error);
    CHECK_MSG (error.isEmpty(), error);
    CHECK (written == TuneExamples::kNumExampleTunes + TuneExamples::kNumMidiClips);

    // The build's own Resources copy too, so the checks below pass in the same run.
    if (const auto build = TuneExamples::getExampleDirectory(); build != juce::File())
        TuneExamples::writeShippedContent (build.getParentDirectory().getParentDirectory(), error);
}

//==============================================================================
/*  onboarding 6: six example tunes, each showing a different capability. */
LUTHIER_TEST (SampleContent, theSixExampleTunesAreValidAndShipAsBuilt)
{
    const auto built = TuneExamples::buildExampleTunes();
    CHECK (built.size() == (size_t) TuneExamples::kNumExampleTunes);

    GenreKitLibrary kits;
    PatternLibrary patterns;

    for (const auto& e : built)
    {
        CHECK_MSG (e.tune.validate().isEmpty(), e.fileName + ": " + e.tune.validate().joinIntoString ("; "));
        CHECK (e.tune.getNumSections() >= 1);
        CHECK (e.tune.meta.author == "Factory");
        CHECK (e.tune.getTotalBeats() > 0.0);

        for (const auto& s : e.tune.arrangement.sections)
        {
            CHECK_MSG (kits.indexOf (s.genreKitId) >= 0, e.fileName + ": unknown kit " + s.genreKitId);
            CHECK_MSG (patterns.indexOf (s.rhythmPatternId) >= 0, e.fileName + ": unknown pattern " + s.rhythmPatternId);
            CHECK_MSG (! s.chords.empty(), e.fileName + ": a section with no chords");
        }
    }

    // What each one shows off.
    auto any = [&built] (int index, auto&& predicate)
    {
        for (const auto& s : built[(size_t) index].tune.arrangement.sections)
            if (predicate (s))
                return true;

        return false;
    };

    CHECK (any (0, [] (const TuneSection& s) { return s.style == MelodyStyle::classicalGuitar; }));      // fingerstyle etude
    CHECK (any (1, [] (const TuneSection& s) { return s.bass.mode == BassMode::walking; }));             // jazz standard
    CHECK (any (2, [] (const TuneSection& s) { return s.findLayer (LayerType::countermelody) != nullptr; })); // folk sketch
    CHECK (any (3, [] (const TuneSection& s) { return s.findLayer (LayerType::percussion) != nullptr; }));    // metal riff
    CHECK (any (4, [] (const TuneSection& s)                                                             // slide blues
    {
        return s.melody.has_value() && std::any_of (s.melody->notes.begin(), s.melody->notes.end(),
                                                    [] (const MelodyNote& n) { return n.technique == NoteTechnique::slide; });
    }));
    CHECK (any (5, [] (const TuneSection& s) { return s.bass.mode == BassMode::manual && ! s.bass.notes.empty(); })); // funk-slap bass

    // Deterministic, and the shipped files are exactly what this builds.
    const auto again = TuneExamples::buildExampleTunes();

    for (size_t i = 0; i < built.size(); ++i)
        CHECK (built[i].tune == again[i].tune);

    juce::StringArray errors;
    const auto shipped = TuneExamples::loadExamples (&errors);
    CHECK_MSG (errors.isEmpty(), errors.joinIntoString ("; "));
    CHECK_MSG (shipped.size() == built.size(), "shipped " + juce::String ((int) shipped.size()) + " example tunes");

    for (size_t i = 0; i < juce::jmin (shipped.size(), built.size()); ++i)
    {
        CHECK_MSG (shipped[i].file.getFileName() == built[i].fileName, shipped[i].file.getFileName());
        CHECK_MSG (TuneFile::toJson (shipped[i].tune) == TuneFile::toJson (built[i].tune),
                   built[i].fileName + " on disk is not what TuneExamples builds (regenerate it)");
    }
}

//==============================================================================
/*  onboarding 6: twelve example MIDI clips in Resources/Examples. */
LUTHIER_TEST (SampleContent, theTwelveMidiClipsShipAndPlay)
{
    const auto clips = TuneExamples::buildMidiClips();
    CHECK (clips.size() == (size_t) TuneExamples::kNumMidiClips);

    const auto folder = TuneExamples::getMidiClipDirectory();
    juce::StringArray kitsSeen;

    for (const auto& clip : clips)
    {
        CHECK (! kitsSeen.contains (clip.genreKit));
        kitsSeen.add (clip.genreKit);
        CHECK (clip.tune.getSection (0) != nullptr && clip.tune.getSection (0)->chords.size() >= 3);

        const auto file = folder.getChildFile (clip.fileName);
        CHECK_MSG (file.existsAsFile(), clip.fileName + " is missing");

        juce::MemoryBlock onDisk;
        file.loadFileAsData (onDisk);
        CHECK_MSG (onDisk == TuneExamples::renderMidiClip (clip), clip.fileName + " is not what TuneExamples builds");

        juce::MidiFile midi;
        juce::MemoryInputStream in (onDisk, false);
        CHECK (midi.readFrom (in));

        int noteOns = 0;

        for (int t = 0; t < midi.getNumTracks(); ++t)
            for (const auto* event : *midi.getTrack (t))
                noteOns += event->message.isNoteOn() ? 1 : 0;

        CHECK_MSG (noteOns >= 8, clip.fileName + " has only " + juce::String (noteOns) + " notes");
    }
}

//==============================================================================
/*  onboarding 6: ten example setlists, installed without overwriting. */
LUTHIER_TEST (SampleContent, theTenExampleSetlistsInstallOnceOverTheFactoryBank)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& presets = processor->getPresetManager();

    const auto lists = TuneExamples::buildExampleSetlists (presets);
    CHECK (lists.size() == (size_t) TuneExamples::kNumExampleSetlists);

    for (const auto& s : lists)
        CHECK_MSG (s.getNumEntries() >= 3, s.getName() + " found only " + juce::String (s.getNumEntries()) + " presets");

    juce::TemporaryFile folderHandle;
    const auto folder = folderHandle.getFile();
    folder.createDirectory();

    CHECK (TuneExamples::installExampleSetlists (presets, folder) == TuneExamples::kNumExampleSetlists);
    CHECK_MSG (TuneExamples::installExampleSetlists (presets, folder) == 0, "a second install overwrote the setlists");

    const auto files = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + Setlist::kFileExtension);
    CHECK (files.size() == TuneExamples::kNumExampleSetlists);

    for (const auto& f : files)
    {
        Setlist s;
        CHECK (s.loadFrom (f));
        CHECK (s.getNumEntries() >= 3);
        CHECK (juce::File (s.getEntry (0).presetPath).existsAsFile());
    }

    folder.deleteRecursively();
}

//==============================================================================
/*  tune-builder 2.1: "Every genre kit ships with a suggested tempo, feel, and a
    chord palette." */
LUTHIER_TEST (SampleContent, everyGenreKitSuggestsATempoAFeelAndAPalette)
{
    GenreKitLibrary kits;

    for (int i = 0; i < kits.getNumKits(); ++i)
    {
        const auto name = kits.getKit (i).name;
        const auto s = TuneKits::getSuggestion (name);

        CHECK_MSG (s.tempoBpm >= 40.0 && s.tempoBpm <= 240.0, name + " tempo " + juce::String (s.tempoBpm));
        CHECK (s.feel >= 0.0 && s.feel <= 1.0);
        CHECK (s.swingPercent >= 0.0 && s.swingPercent <= 100.0);

        const auto cells = TuneKits::resolvePalette (s.palette, 0, TuneMode::ionian, 4.0);
        CHECK_MSG ((int) cells.size() == s.palette.size() && cells.size() >= 3,
                   name + ": its palette resolves to " + juce::String ((int) cells.size()) + " chords");
    }

    // The numerals mean what they say.
    const auto c = TuneKits::resolvePalette ({ "I", "vi", "ii7", "V7", "bVII", "Imaj7" }, 0, TuneMode::ionian, 4.0);
    CHECK (c.size() == 6);

    if (c.size() == 6)
    {
        CHECK (getChordSymbol (c[0], false) == "C");
        CHECK (getChordSymbol (c[1], false) == "Am");
        CHECK (getChordSymbol (c[2], false) == "Dm7");
        CHECK (getChordSymbol (c[3], false) == "G7");
        CHECK (getChordSymbol (c[4], true) == "Bb");
        CHECK (getChordSymbol (c[5], false) == "Cmaj7");
    }
}

//==============================================================================
/*  onboarding 10 path C: "load one of the example tunes, hit play" - from the
    TUNE tab's New menu, as a new tune that Save never writes over the factory file. */
LUTHIER_TEST (SampleContent, anExampleTuneOpensFromTheTuneTabAndPlays)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    TunePanel panel (*processor, processor->getTunePlayer(), processor->getTuneSession());

    CHECK (panel.openExample (2));
    const auto& tune = processor->getTuneSession().getTune();
    CHECK (tune.meta.title == "Morning Folk Sketch");
    CHECK (processor->getTuneSession().getFile() == juce::File());
    CHECK (! processor->getTuneSession().isDirty());
    CHECK (! panel.openExample (99));

    panel.getPlayButton().onClick();
    CHECK (processor->getTunePlayer().isPlaying());
    processor->getTunePlayer().stop();
}

//==============================================================================
/*  onboarding 6: twelve tune templates, every one parseable and musically sound. */
LUTHIER_TEST (SampleContent, theTwelveTuneTemplatesShipParseAndPlay)
{
    CHECK (TuneTemplateLibrary::kNumFactoryTemplates == 12);

    juce::StringArray errors;
    const auto templates = TuneTemplateLibrary::loadFactory (&errors);
    CHECK_MSG (errors.isEmpty(), errors.joinIntoString ("; "));
    CHECK_MSG (templates.size() == 12, "shipped " + juce::String ((int) templates.size()) + " templates");

    GenreKitLibrary kits;
    PatternLibrary patterns;
    juce::StringArray titles;

    for (const auto& t : templates)
    {
        CHECK (! titles.contains (t.name));
        titles.add (t.name);
        CHECK_MSG (t.tune.validate().isEmpty(), t.name + ": " + t.tune.validate().joinIntoString ("; "));
        CHECK (t.tune.meta.tempoBpm >= 40.0 && t.tune.meta.tempoBpm <= 240.0);

        for (const auto& s : t.tune.arrangement.sections)
        {
            CHECK_MSG (s.genreKitId.isEmpty() || kits.indexOf (s.genreKitId) >= 0, t.name + ": kit " + s.genreKitId);
            CHECK_MSG (s.rhythmPatternId.isEmpty() || patterns.indexOf (s.rhythmPatternId) >= 0,
                       t.name + ": pattern " + s.rhythmPatternId);
        }
    }

    if (templates.size() < 12)
        return;

    // The two added to reach onboarding 6's twelve.
    const auto& pop = templates[10].tune;
    CHECK (pop.meta.title == "Pop four-chord in G" && pop.meta.keyTonic == 7 && pop.getNumSections() == 2);
    CHECK (formatProgression (pop.arrangement.sections[0].chords, 4.0, false) == "G D Em C");

    const auto& funk = templates[11].tune;
    CHECK (funk.meta.title == "Funk groove in E" && funk.meta.keyTonic == 4 && funk.getNumSections() == 1);
    CHECK (funk.arrangement.sections[0].rhythmPatternId == "Funk Sixteenth");
    CHECK (TuneTimeline::build (funk).getLengthPpq() == funk.getTotalBeats());
    CHECK (funk.getTotalBeats() > 0.0);
}

//==============================================================================
/*  onboarding 6: six royalty-free backing tracks, that open, are the right
    length and are not silent. */
LUTHIER_TEST (SampleContent, theSixBackingTracksShipAndPlay)
{
    CHECK (BackingTrackLibrary::kNumFactoryTracks == 6);

    const auto tracks = BackingTrackLibrary::findFactoryTracks();
    CHECK_MSG (tracks.size() == 6, "found " + juce::String (tracks.size()) + " backing tracks in "
                                       + BackingTrackLibrary::getFactoryDirectory().getFullPathName());

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    juce::int64 totalBytes = 0;
    juce::StringArray names;

    for (const auto& file : tracks)
    {
        totalBytes += file.getSize();
        CHECK (! names.contains (BackingTrackLibrary::getDisplayName (file)));
        names.add (BackingTrackLibrary::getDisplayName (file));

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        CHECK_MSG (reader != nullptr, file.getFileName() + " has no reader");

        if (reader == nullptr)
            continue;

        const double seconds = (double) reader->lengthInSamples / reader->sampleRate;
        CHECK_MSG (seconds >= 20.0 && seconds <= 90.0, file.getFileName() + " is " + juce::String (seconds) + " s");
        CHECK (reader->sampleRate >= 32000.0);
        CHECK (reader->numChannels >= 1 && reader->numChannels <= 2);

        // Audible, not clipping, and not just one instant of sound: measure the
        // level of one-second windows across the file.
        juce::AudioBuffer<float> audio ((int) reader->numChannels, (int) reader->sampleRate);
        int loudWindows = 0, windows = 0;
        float peak = 0.0f;

        for (juce::int64 pos = 0; pos + (juce::int64) reader->sampleRate <= reader->lengthInSamples;
             pos += (juce::int64) reader->sampleRate)
        {
            reader->read (&audio, 0, audio.getNumSamples(), pos, true, true);
            ++windows;

            const float rms = audio.getRMSLevel (0, 0, audio.getNumSamples());
            peak = juce::jmax (peak, audio.getMagnitude (0, audio.getNumSamples()));
            loudWindows += rms > 0.01f ? 1 : 0;
        }

        CHECK_MSG (peak > 0.1f && peak <= 1.0f, file.getFileName() + " peak " + juce::String (peak));
        CHECK_MSG (loudWindows >= windows - 1, file.getFileName() + " has silent stretches");

        // And the player the Practice drawer uses can take it and stream it.
        BackingTrackPlayer player;
        player.prepare (48000.0, 512);
        CHECK_MSG (player.load (file), file.getFileName() + " did not load");
        CHECK_NEAR (player.getLengthSeconds(), seconds, 0.01);

        juce::Thread::sleep (400);   // the file streams on its own thread
        player.play();

        juce::AudioBuffer<float> block (2, 512);
        float played = 0.0f;

        for (int i = 0; i < 200; ++i)
        {
            player.processBlock (block, 512);
            played = juce::jmax (played, block.getMagnitude (0, 512));
        }

        CHECK_MSG (played > 0.001f, file.getFileName() + " played silence");
        player.unload();
    }

    CHECK_MSG (totalBytes < 15 * 1024 * 1024, "backing tracks total " + juce::String (totalBytes) + " bytes");
}

//==============================================================================
/*  The factory catalogue mismatch (CODEX_COMPLETENESS_LEDGER, first-run audit):
    the docs advertised a bank the code never built. The bank is now the
    documented one; this pins it so the spec and the code cannot drift again. */
LUTHIER_TEST (SampleContent, theFactoryBankIsTheDocumentedThirtySix)
{
    const char* const expected[] =
    {
        "Clean Double-Cut Funk", "T-Style Country Twang", "Single-Cut Crunch", "Modern Metal Chug",
        "Jazz Hollowbody", "Blues Slide", "Shred Lead", "Germanium Fuzz Lead", "Surf Reverb",
        "Semi-Hollow Chime", "Drop C Riff", "Wah Funk Rhythm", "Octave Fuzz Stoner", "Ambient Swell",
        "8-String Djent", "Rockabilly Slap", "Tapping Etude", "Fingerstyle Folk", "Strummed Dreadnought",
        "Parlor Blues", "12-String Jangle", "Nylon Classical", "Flamenco Rasgueado", "Nashville High-Strung",
        "DADGAD Drone", "Jumbo Bluegrass", "P-Bass Flatwound", "J-Style Fingerstyle", "Fretless Mwah",
        "Violin Bass Grind", "5-String Low B", "Init", "Dry Instrument", "Physics Showcase",
        "Transposing Trem Chords", "Microtonal Just"
    };

    CHECK (FactoryPresets::getNumPresets() == 36);

    if (FactoryPresets::getNumPresets() != 36)
        return;

    int electric = 0, acoustic = 0, classical = 0, bass = 0, utility = 0;

    for (int i = 0; i < 36; ++i)
    {
        const auto& def = FactoryPresets::getPreset (i);
        CHECK_MSG (juce::String (def.name) == expected[i], juce::String (def.name) + " where " + expected[i] + " was expected");

        const juce::String category (def.category);
        electric += category == "Electric";
        acoustic += category == "Acoustic";
        classical += category == "Classical";
        bass += category == "Bass";
        utility += category == "Utility";
    }

    CHECK (electric == 17 && acoustic + classical == 9 && bass == 5 && utility == 5);

    // The first-run preset the onboarding spec names is in the bank.
    CHECK (juce::String (FactoryPresets::getPreset (2).name) == "Single-Cut Crunch");
}
