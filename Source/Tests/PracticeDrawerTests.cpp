/*  The practice drawer running a routine (practice-tools 10-12): the drawer
    follows the routine's tool, counts the minutes, pauses the routine when it
    closes, and a START on the PRACTICE tab reaches it through the processor. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/PracticePanel.h"

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
