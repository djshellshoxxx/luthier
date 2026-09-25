/*  The practice drawer running a routine (practice-tools 10-12): the drawer
    follows the routine's tool, counts the minutes, pauses the routine when it
    closes, and a START on the PRACTICE tab reaches it through the processor. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/PracticePanel.h"
#include "../UI/PracticeSetupPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    RoutineEntry timed (PracticeTool tool, double seconds)
    {
        RoutineEntry e;
        e.tool = tool;
        e.title = getPracticeToolTabLabel (tool);
        e.durationSeconds = seconds;
        e.settings = juce::var (new juce::DynamicObject());
        return e;
    }

    /** A processor whose practice history saves to a temporary file, never
        the user's own. */
    std::unique_ptr<LuthierAudioProcessor> practising (juce::File& statsFile)
    {
        statsFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("luthier-drawer-tests").getChildFile ("stats.json");
        statsFile.getParentDirectory().createDirectory();
        statsFile.deleteFile();

        auto processor = std::make_unique<LuthierAudioProcessor>();
        processor->setPracticeStatsFile (statsFile);
        processor->getPracticeStats().clear();
        return processor;
    }
}

LUTHIER_TEST (PracticeDrawer, theDrawerFollowsTheRoutineAndCountsItsMinutes)
{
    juce::File statsFile;
    auto processor = practising (statsFile);

    PracticePanel drawer (*processor);
    drawer.setSize (900, PracticePanel::defaultOpenHeight);
    drawer.setOpen (true);

    PracticeRoutine routine;
    routine.name = "Warm-up";
    routine.entries.push_back (timed (PracticeTool::metronome, 10.0));
    routine.entries.push_back (timed (PracticeTool::scaleTrainer, 10.0));

    auto& runner = processor->getPracticeRoutineRunner();
    CHECK (runner.start (routine));

    // The routine's first tool, and minutes against it while it runs.
    drawer.tick (0.05);
    CHECK (drawer.getCurrentTab() == (int) PracticeTool::metronome);
    CHECK (processor->getMetronome().isEnabled());

    for (int i = 0; i < 100; ++i)
        drawer.tick (0.05);

    const auto today = PracticeStats::today();
    const double metronomeSeconds = processor->getPracticeStats().getSeconds (today, PracticeTool::metronome);
    CHECK_MSG (metronomeSeconds > 4.0 && metronomeSeconds < 6.0,
               "5 s with the metronome running counted " + juce::String (metronomeSeconds, 2) + " s");

    // Past the first entry's ten seconds, the drawer moves to the next tool.
    for (int i = 0; i < 120; ++i)
        drawer.tick (0.05);

    CHECK (runner.getEntryIndex() == 1);
    CHECK (drawer.getCurrentTab() == (int) PracticeTool::scaleTrainer);

    // 0.1: closing the drawer pauses the routine and saves the history;
    // opening it carries on.
    drawer.setOpen (false);
    CHECK (runner.getPhase() == PracticeRoutineRunner::Phase::paused);
    CHECK (statsFile.existsAsFile());

    drawer.setOpen (true);
    CHECK (runner.getPhase() == PracticeRoutineRunner::Phase::running);

    // A pause the player chose survives a close and reopen.
    runner.pause();
    drawer.setOpen (false);
    drawer.setOpen (true);
    CHECK (runner.getPhase() == PracticeRoutineRunner::Phase::paused);

    runner.stop();
    drawer.setOpen (false);
    statsFile.getParentDirectory().deleteRecursively();
}

/*  11.2: START on the PRACTICE tab asks for the drawer on the routine's tool;
    the editor's timer takes the request once. */
LUTHIER_TEST (PracticeDrawer, aRoutineStartAsksForTheDrawerOnce)
{
    juce::File statsFile;
    auto processor = practising (statsFile);

    CHECK (processor->takePracticeDrawerRequest() < 0);

    processor->requestPracticeDrawer (PracticeTool::looper);
    CHECK (processor->takePracticeDrawerRequest() == (int) PracticeTool::looper);
    CHECK (processor->takePracticeDrawerRequest() < 0);

    statsFile.getParentDirectory().deleteRecursively();
}

//==============================================================================
/*  practice-tools 8 through the processor: the session recorder takes what the
    plugin produced and the MIDI that produced it, drawer open or not, and the
    stop with auto-save on writes a WAV and a Luthier-profile MIDI file whose
    notes are the ones played, at the samples they were played. */
LUTHIER_TEST (PracticeDrawer, theSessionRecorderSavesWhatTheProcessorPlayed)
{
    juce::File statsFile;
    auto processor = practising (statsFile);

    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    processor->prepareToPlay (kSr, kBlock);

    const auto sessions = statsFile.getParentDirectory().getChildFile ("Sessions");
    sessions.deleteRecursively();

    auto& recorder = processor->getSessionRecorder();
    CHECK (recorder.prepare (kSr, 1.0));
    recorder.setSaveDirectory (sessions);
    recorder.setRecordAudio (true);
    recorder.setRecordMidi (true);
    recorder.setAutoSaveOnStop (true);
    recorder.setTempoBpm (120.0);
    recorder.setEnabled (true);

    // The drawer stays closed: the recorder is switched on in Options as well
    // as in the drawer, so it must not depend on the drawer being open.
    CHECK (! processor->isPracticePanelOpen());

    struct Played { int block; int offset; int note; bool on; };

    const Played played[] =
    {
        { 2, 37, 52, true }, { 20, 0, 52, false }, { 24, 100, 57, true }, { 40, 0, 57, false }
    };

    constexpr int kBlocks = 48;

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor->getTotalNumOutputChannels()), kBlock);
    double peak = 0.0;

    for (int b = 0; b < kBlocks; ++b)
    {
        juce::MidiBuffer midi;

        for (const auto& p : played)
            if (p.block == b)
                midi.addEvent (p.on ? juce::MidiMessage::noteOn (1, p.note, (juce::uint8) 100)
                                    : juce::MidiMessage::noteOff (1, p.note),
                               p.offset);

        buffer.clear();
        processor->processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            peak = juce::jmax (peak, (double) buffer.getMagnitude (ch, 0, kBlock));
    }

    CHECK_MSG (peak > 0.01, "the guitar did not sound: peak " + juce::String (peak, 6));
    CHECK_MSG (recorder.getRecordedSamples() == kBlocks * kBlock,
               "the recorder holds " + juce::String (recorder.getRecordedSamples()) + " samples");

    SessionRecorder::SavedTake saved;
    CHECK_MSG (recorder.stop (&saved), "the stop did not auto-save");
    CHECK (! recorder.isEnabled());
    CHECK_MSG (saved.wav.existsAsFile(), "no WAV was written");
    CHECK_MSG (saved.midi.existsAsFile(), "no MIDI file was written");
    CHECK (saved.wav.getParentDirectory() == sessions);
    CHECK (saved.midi.getFileNameWithoutExtension() == saved.wav.getFileNameWithoutExtension());

    // ---- the WAV holds what the plugin produced --------------------------------------
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (saved.wav.createInputStream().release(), true));
        CHECK (reader != nullptr);

        if (reader != nullptr)
        {
            CHECK (reader->lengthInSamples == kBlocks * kBlock);

            juce::AudioBuffer<float> back (2, (int) reader->lengthInSamples);
            reader->read (&back, 0, (int) reader->lengthInSamples, 0, true, true);

            const double wavPeak = juce::jmax (back.getMagnitude (0, 0, back.getNumSamples()),
                                               back.getMagnitude (1, 0, back.getNumSamples()));
            CHECK_MSG (wavPeak > 0.01, "the saved WAV is silent: peak " + juce::String (wavPeak, 6));
        }
    }

    // ---- the MIDI file is the notes played, Luthier profile, at their samples ----------
    {
        MidiPerformance back (kSr);
        const auto result = MidiProfiles::importFromFile (saved.midi, back, kSr);

        CHECK_MSG (result.ok, result.error);
        CHECK (result.detectedProfile == MidiProfile::luthier);

        std::vector<PerformanceMessage> notes;

        for (const auto& m : back.getMessages())
            if (m.message.isNoteOnOrOff())
                notes.push_back (m);

        CHECK_MSG (notes.size() == 4, juce::String ((int) notes.size()) + " note messages came back, not 4");

        for (size_t i = 0; i < notes.size() && i < 4; ++i)
        {
            const auto& p = played[i];
            const juce::int64 sample = (juce::int64) p.block * kBlock + p.offset;

            CHECK_MSG (notes[i].message.getNoteNumber() == p.note && notes[i].message.isNoteOn() == p.on,
                       "note " + juce::String ((int) i) + " is " + notes[i].message.getDescription());
            CHECK_MSG (notes[i].sample == sample,
                       "note " + juce::String ((int) i) + " at sample " + juce::String (notes[i].sample)
                         + ", played at " + juce::String (sample));
        }
    }

    // Off, nothing more goes in.
    {
        juce::MidiBuffer midi;
        buffer.clear();
        processor->processBlock (buffer, midi);
        CHECK (recorder.getRecordedSamples() == kBlocks * kBlock);
    }

    statsFile.getParentDirectory().deleteRecursively();
}

/*  practice-tools 11.2 "Tab files opened recently": a tab opened in the
    drawer's TAB tab reaches the PRACTICE tab's recent list; one that could not
    be read does not. */
LUTHIER_TEST (PracticeDrawer, aTabOpenedInTheDrawerReachesTheRecentList)
{
    juce::File statsFile;
    auto processor = practising (statsFile);

    const auto root = statsFile.getParentDirectory().getChildFile ("library");
    root.deleteRecursively();
    root.createDirectory();

    const auto locations = PracticeSetupLocations::inside (root);

    PracticePanel drawer (*processor);
    drawer.setSize (900, PracticePanel::defaultOpenHeight);
    drawer.setLibraryFile (locations.libraryFile);

    // A tab file, as the exporter writes one.
    PerformanceScore score;
    score.beginCapture (120.0, 4, 4);

    const int open[6] = { 64, 59, 55, 50, 45, 40 };
    const struct { int string, fret; double beat; } riff[] = { { 5, 0, 0.0 }, { 5, 3, 1.0 }, { 4, 2, 2.0 }, { 4, 0, 3.0 } };

    for (const auto& note : riff)
    {
        score.noteStarted (note.string, note.fret, open[note.string] + note.fret, 440.0, 0.8, note.beat);
        score.noteEnded (note.string, note.beat + 0.5);
    }

    score.endCapture (4.0);

    NotationExporter exporter;
    NotationExportOptions options;
    options.lineWidth = 200;
    options.chordSymbols = false;

    const auto tabFile = root.getChildFile ("riff.txt");
    CHECK (tabFile.replaceWithText (exporter.renderAsciiTab (score, options)));

    CHECK_MSG (drawer.openTabFile (tabFile), "the drawer could not open the tab file");

    PracticeLibrary library;
    CHECK_MSG (library.load (locations.libraryFile), "the drawer did not write the library");
    CHECK (library.getRecentTabs().size() == 1);
    CHECK (library.getRecentTabs().size() == 1 && library.getRecentTabs()[0] == tabFile);

    // Not a tab: refused, and not "recent".
    const auto notATab = root.getChildFile ("notes.txt");
    notATab.replaceWithText ("Remember to change the strings.");
    CHECK (! drawer.openTabFile (notATab));

    PracticeLibrary again;
    CHECK (again.load (locations.libraryFile));
    CHECK (again.getRecentTabs().size() == 1);

    // Opening it again moves it to the top rather than listing it twice.
    CHECK (drawer.openTabFile (tabFile));
    CHECK (again.load (locations.libraryFile) && again.getRecentTabs().size() == 1);

    // And the PRACTICE tab, reading the same library, lists it.
    PracticeSetupContext context;
    context.targets = processor->getPracticeTargets();
    context.runner = &processor->getPracticeRoutineRunner();
    context.stats = &processor->getPracticeStats();
    context.sampleRate = [] { return 48000.0; };
    context.locations = locations;
    context.showInDrawer = [] (PracticeTool) {};

    PracticeSetupPanel tab (context);
    CHECK_MSG (tab.getRecentTabsText().isEmpty(), "the tab still shows its empty state: " + tab.getRecentTabsText());
    CHECK (tab.getRecentTabList().getListBoxModel() != nullptr
           && tab.getRecentTabList().getListBoxModel()->getNumRows() == 1);

    statsFile.getParentDirectory().deleteRecursively();
}
