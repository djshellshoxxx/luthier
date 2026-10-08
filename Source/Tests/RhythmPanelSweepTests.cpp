/*  SPEC-SWEEP rhythm-engine UI checks (RE-34, RE-37, RE-38): the RHYTHM tab's
    editors and readouts, driven the way a click drives them. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/RhythmPanel.h"
#include "../Rhythm/Patterns.h"
#include "../Rhythm/RhythmEngine.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    juce::MouseEvent clickAt (juce::Component& target, juce::Point<float> p)
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        return juce::MouseEvent (source, p, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, juce::Time::getCurrentTime(), p,
                                 juce::Time::getCurrentTime(), 1, false);
    }
}

/*  RE-34: clicking a fingerpick cell assigns that step to that finger; clicking
    it again clears the step. */
LUTHIER_TEST (RhythmPanelUi, fingerpickGridTogglesAFingerStep)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& rhythm = processor.getEngine().getRhythmEngine();

    RhythmPattern pattern;
    pattern.setName ("Sweep pick");
    pattern.setKind (RhythmPattern::Kind::fingerpick);
    pattern.setSubdivision (Subdivision::sixteenth);
    pattern.setLength (16);
    rhythm.setPattern (pattern);

    FingerpickGrid grid (processor);
    grid.setSize (FingerpickGrid::headerWidth + 16 * 20, FingerpickGrid::preferredHeight);
    grid.refresh();

    const int step = 5, finger = 2;
    const juce::Point<float> cell ((float) (FingerpickGrid::headerWidth + step * 20 + 10),
                                   (float) (finger * FingerpickGrid::rowHeight + FingerpickGrid::rowHeight / 2));

    grid.mouseDown (clickAt (grid, cell));

    auto after = rhythm.getPattern().getFingerpickStep (step);
    CHECK (after.active);
    CHECK ((int) after.finger == finger);

    grid.mouseDown (clickAt (grid, cell));
    CHECK (! rhythm.getPattern().getFingerpickStep (step).active);
}

/*  RE-37 (rhythm-engine 8.7): with a chord held and the rhythm engine playing,
    the indicators name the chord and show its voicing as dots. */
LUTHIER_TEST (RhythmPanelUi, indicatorsShowChordVoicingAndNextStroke)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& rhythm = processor.getEngine().getRhythmEngine();
    PatternLibrary patterns;
    const auto strums = patterns.findByKind (RhythmPattern::Kind::strum);
    CHECK (! strums.isEmpty());

    if (strums.isEmpty())
        return;

    rhythm.setPattern (patterns.getPattern (strums[0]));
    rhythm.setFreeRun (true);
    rhythm.setEnabled (true);

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                 processor.getTotalNumOutputChannels()), kBlock);

    for (int block = 0; block < 20; ++block)
    {
        juce::MidiBuffer midi;

        if (block == 0)
            for (int n : { 48, 52, 55 })
                midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);

        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    RhythmIndicators indicators (processor);
    indicators.refresh();

    CHECK_MSG (indicators.getChordText().startsWith ("C"), "indicator chord was '" + indicators.getChordText() + "'");
    CHECK (indicators.getNumVoicedDots() >= 3);
}

/*  RE-5 (rhythm-engine 0.5): the instrument's Humanize macro is the one
    humanise control - it scales the rhythm engine's (kit) feel, which stays as
    written at the macro's default. */
LUTHIER_TEST (RhythmPatterns, macroHumanizeScalesTheRhythmFeel)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& rhythm = processor.getEngine().getRhythmEngine();
    RhythmHumanise kit;
    kit.amount = 0.8;
    rhythm.setHumanise (kit);
    rhythm.setEnabled (true);

    auto* macro = processor.getState().getParameter (ParamIDs::macroHumanize);
    CHECK (macro != nullptr);

    if (macro == nullptr)
        return;

    juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                 processor.getTotalNumOutputChannels()), kBlock);

    auto amountAt = [&] (float macroValue)
    {
        macro->setValueNotifyingHost (macro->convertTo0to1 (macroValue));

        for (int i = 0; i < 2; ++i)
        {
            juce::MidiBuffer midi;
            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        return rhythm.getBlockHumanise().amount;
    };

    CHECK_NEAR (amountAt (0.4f), 0.8, 1.0e-6);    // the default leaves the kit alone
    CHECK_NEAR (amountAt (0.0f), 0.0, 1.0e-6);    // a machine
    CHECK_NEAR (amountAt (0.8f), 1.6, 1.0e-6);    // twice as loose
    CHECK_NEAR (rhythm.getHumanise().amount, 0.8, 1.0e-9);   // the kit's own value is untouched
}

/*  RE-33 (8.3): a 32-step strum pattern lays out as two rows of sixteen, a
    left-click on the second row cycles that step, and the right-click menu
    offers the dynamics, the string masks and delete, each doing what it says. */
LUTHIER_TEST (RhythmPanelUi, strumGridThirtyTwoStepsAndItsMenu)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& rhythm = processor.getEngine().getRhythmEngine();

    RhythmPattern pattern;
    pattern.setName ("Sweep strum 32");
    pattern.setKind (RhythmPattern::Kind::strum);
    pattern.setSubdivision (Subdivision::sixteenth);
    pattern.setLength (32);
    rhythm.setPattern (pattern);

    StrumGrid grid (processor);
    grid.setSize (16 * 20, StrumGrid::preferredHeight);
    grid.refresh();

    // Step 20 is the fifth cell of the second row.
    const int step = 20;
    const auto before = rhythm.getPattern().getStrumStep (step).type;
    grid.mouseDown (clickAt (grid, { 4.0f * 20.0f + 10.0f, (float) StrumGrid::rowHeight * 1.5f }));
    CHECK ((int) rhythm.getPattern().getStrumStep (step).type == ((int) before + 1) % (int) StrumType::numTypes);
    CHECK (rhythm.getPattern().getStrumStep (4).type == pattern.getStrumStep (4).type);   // not the first row's

    juce::Array<int> ids;
    const auto menu = grid.buildStepMenu (step);
    juce::PopupMenu::MenuItemIterator it (menu, true);

    while (it.next())
        if (it.getItem().itemID != 0)
            ids.add (it.getItem().itemID);

    for (int id : { 100, 101, 102, 103, 104, 200, 201, 202, 203, 204, 300 })
        CHECK_MSG (ids.contains (id), "the step menu has no item " + juce::String (id));

    grid.applyStepMenuResult (step, 102);   // 70 %
    CHECK_NEAR (rhythm.getPattern().getStrumStep (step).dynamic, 0.70, 1.0e-9);

    grid.applyStepMenuResult (step, 201);   // top three
    CHECK (rhythm.getPattern().getStrumStep (step).stringMask == 0x0007);

    grid.applyStepMenuResult (step, 300);   // delete
    CHECK (rhythm.getPattern().getStrumStep (step).isRest());
}

/*  RE-36 (8.6): the browser's tag filter narrows the list to that tag, LOAD
    puts the selected pattern in the engine, and SAVE writes the current
    pattern to the user folder and lists it. (EXPORT opens a file chooser.) */
LUTHIER_TEST (RhythmPanelUi, patternBrowserFiltersLoadsAndSaves)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& rhythm = processor.getEngine().getRhythmEngine();
    auto& library = processor.getPatternLibrary();

    RhythmPanel panel (processor);
    panel.setSize (1200, 900);

    auto& filter = panel.getTagFilterBox();
    CHECK (filter.getNumItems() > 1);

    if (filter.getNumItems() <= 1)
        return;

    filter.setSelectedItemIndex (1, juce::sendNotificationSync);
    const auto tag = filter.getText();
    const auto tagged = library.findByTag (tag);

    auto& list = panel.getPatternList();
    CHECK (tagged.size() > 0);
    CHECK_MSG (list.getListBoxModel()->getNumRows() == tagged.size(),
               "tag '" + tag + "' lists " + juce::String (list.getListBoxModel()->getNumRows())
                 + " rows for " + juce::String (tagged.size()) + " patterns");
    CHECK (tagged.size() < library.getNumPatterns());

    list.selectRow (tagged.size() - 1);
    panel.getLoadButton().onClick();
    CHECK (rhythm.getPattern().getName() == library.getPattern (tagged.getLast()).getName());

    // SAVE: a pattern with a name nobody has, removed again afterwards.
    auto mine = rhythm.getPattern();
    mine.setName ("Sweep RE36 " + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));
    rhythm.setPattern (mine);

    const auto file = PatternLibrary::getUserDirectory()
                        .getChildFile (juce::File::createLegalFileName (mine.getName()) + ".luthierpattern");

    panel.getSaveButton().onClick();
    CHECK_MSG (file.existsAsFile(), "SAVE wrote no " + file.getFullPathName());

    filter.setSelectedItemIndex (0, juce::sendNotificationSync);   // all patterns
    bool listed = false;

    for (int i = 0; i < library.getNumPatterns(); ++i)
        listed = listed || library.getPattern (i).getName() == mine.getName();

    CHECK (listed);
    CHECK (list.getListBoxModel()->getNumRows() == library.getNumPatterns());

    CHECK (panel.getExportButton().onClick != nullptr);

    file.deleteFile();
    library.refresh();
}
