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
#include "../Presets/PresetManager.h"
#include "../Rhythm/GenreKit.h"
#include "../Rhythm/Patterns.h"
#include "../Tune/TuneExamples.h"
#include "../Tune/TuneMelody.h"
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
