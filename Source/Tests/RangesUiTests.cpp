/*  The UI half of advanced-ranges.md: the right-click unlock and restrict, the
    Options RANGES page, marking, re-attaching a control whose range was swapped,
    and randomise staying inside stock.

    Each of these goes through the real processor, because the thing most likely
    to go wrong is not the drawing but the plumbing: a range that widens in the
    APVTS and not on the slider, or a lock that clamps and cannot be undone.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/HeaderBar.h"
#include "../UI/OptionsPages.h"
#include "../UI/RangesUi.h"
#include "../UI/UiPreferences.h"
#include "../UI/Widgets.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    float plainOf (LuthierAudioProcessor& processor, const juce::String& id)
    {
        auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (processor.getState().getParameter (id));
        return parameter != nullptr ? parameter->get() : -1.0f;
    }

    void setPlain (LuthierAudioProcessor& processor, const juce::String& id, float plain)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (processor.getState().getParameter (id)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    }

    juce::Array<int> menuIds (const juce::PopupMenu& menu)
    {
        juce::Array<int> ids;
        juce::PopupMenu::MenuItemIterator it (menu, true);

        while (it.next())
            ids.add (it.getItem().itemID);

        return ids;
    }

    /*  UiPreferences writes through to the user's real config file. Every test
        here that touches a preference puts the file back as it found it, so a
        test run does not decide what the user's next session looks like. */
    struct PreservedPreferences
    {
        PreservedPreferences()
            : file (UiPreferences::getConfigFile()),
              existed (file.existsAsFile()),
              contents (existed ? file.loadFileAsString() : juce::String())
        {
        }

        ~PreservedPreferences()
        {
            if (existed)
                file.replaceWithText (contents);
            else
                file.deleteFile();

            UiPreferences::get().reset();
            UiPreferences::get().load();
        }

        juce::File file;
        bool existed;
        juce::String contents;
    };
}

//==============================================================================
LUTHIER_TEST (RangesUi, rightClickUnlocksAndRestrictsOneControl)
{
    const PreservedPreferences preserved;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    juce::Component owner;
    const juce::String gain (ParamIDs::ampGain);

    // A locked physical control offers unlock and not restrict.
    auto ids = menuIds (buildParameterContextMenu (processor, gain));
    CHECK_MSG (ids.contains (kUnlockRangeMenuId), "no unlock item on a locked physical control");
    CHECK (! ids.contains (kRestrictRangeMenuId));

    // A control with no physical range offers neither.
    ids = menuIds (buildParameterContextMenu (processor, ParamIDs::masterGain));
    CHECK_MSG (! ids.contains (kUnlockRangeMenuId) && ! ids.contains (kRestrictRangeMenuId),
               "a non-physical control offered a range item");

    applyParameterMenuResult (kUnlockRangeMenuId, owner, processor, gain);

    CHECK (processor.getRanges().isUnlockedIndividually (gain));
    CHECK_MSG (! processor.getRanges().isFamilyAdvanced (RangeFamily::amp),
               "unlocking one control unlocked its whole family");
    CHECK (juce::approximatelyEqual (processor.getState().getParameterRange (gain).end, 2.0f));

    // Now it offers restrict and not unlock.
    ids = menuIds (buildParameterContextMenu (processor, gain));
    CHECK (ids.contains (kRestrictRangeMenuId));
    CHECK (! ids.contains (kUnlockRangeMenuId));

    setPlain (processor, gain, 1.8f);
    CHECK (std::abs (plainOf (processor, gain) - 1.8f) < 1.0e-3f);

    applyParameterMenuResult (kRestrictRangeMenuId, owner, processor, gain);

    CHECK (! processor.getRanges().isUnlockedIndividually (gain));
    CHECK_MSG (std::abs (plainOf (processor, gain) - 1.0f) < 1.0e-3f,
               "restricting did not clamp to stockMax, got " + juce::String (plainOf (processor, gain)));

    // advanced-ranges.md 7: undoing a lock restores the value it clamped.
    processor.undo();

    CHECK_MSG (processor.getRanges().isUnlockedIndividually (gain),
               "undo did not bring the unlock back");
    CHECK_MSG (std::abs (plainOf (processor, gain) - 1.8f) < 1.0e-3f,
               "undo did not bring the clamped value back, got " + juce::String (plainOf (processor, gain)));
}

//==============================================================================
LUTHIER_TEST (RangesUi, theRangesPageListsLocksAndClamps)
{
    const PreservedPreferences preserved;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    RangesPage page (processor);
    page.setSize (700, 500);

    CHECK_MSG (page.getListedParameters().isEmpty(),
               "a stock preset listed values outside stock");

    CHECK (page.setAllFamilies (true) == 0);

    for (int i = 0; i < (int) RangeFamily::numFamilies; ++i)
        CHECK (processor.getRanges().isFamilyAdvanced ((RangeFamily) i));

    const juce::String gain (ParamIDs::ampGain);
    const juce::String master (ParamIDs::ampMaster);

    setPlain (processor, gain, 1.5f);
    setPlain (processor, master, 1.9f);
    page.refresh();

    CHECK_MSG (page.getListedParameters().contains (gain) && page.getListedParameters().contains (master),
               "the summary did not list both advanced values: "
                 + page.getListedParameters().joinIntoString (", "));
    CHECK (page.getListedParameters().size() == 2);

    // Locking clamps both, reports it, and undo restores both.
    CHECK_MSG (page.setAllFamilies (false) == 2, "the lock did not report two clamps");
    CHECK (std::abs (plainOf (processor, gain) - 1.0f) < 1.0e-3f);
    CHECK (page.getListedParameters().isEmpty());

    processor.undo();

    CHECK (std::abs (plainOf (processor, gain) - 1.5f) < 1.0e-3f);
    CHECK (std::abs (plainOf (processor, master) - 1.9f) < 1.0e-3f);
}

//==============================================================================
LUTHIER_TEST (RangesUi, controlsFollowASwappedRangeAndMarkTheValue)
{
    const PreservedPreferences preserved;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);

    juce::Component root;
    auto* knob = new LuthierKnob ("Gain");
    root.addAndMakeVisible (knob);
    std::unique_ptr<LuthierKnob> owned (knob);
    knob->attachTo (processor, gain);

    auto& slider = knob->getSlider();

    CHECK_MSG (slider.getProperties().contains (RangesUi::kStockMaxProperty),
               "a physical control's slider was not tagged with its stock range");
    CHECK (juce::approximatelyEqual (slider.getMaximum(), 1.0));

    RangeState unlocked;
    unlocked.setFamilyAdvanced (RangeFamily::amp, true);
    processor.changeRanges (unlocked, "test");

    // The attachment copied the old range; until it is resynced the slider is stale.
    RangesUi::resyncControls (root);
    CHECK_MSG (juce::approximatelyEqual (slider.getMaximum(), 2.0),
               "the knob did not pick up the widened range, max is " + juce::String (slider.getMaximum()));

    setPlain (processor, gain, 0.8f);
    CHECK_MSG (! RangesUi::markReadout (processor, gain, "x").endsWith ("*"),
               "an unlocked value inside stock was marked - marking follows the value");

    setPlain (processor, gain, 1.4f);
    CHECK (RangesUi::markReadout (processor, gain, "x") == "x*");
    CHECK (std::abs (slider.getValue() - 1.4) < 1.0e-3);

    // Non-physical controls are never marked and never tagged.
    CHECK (RangesUi::markReadout (processor, ParamIDs::masterGain, "y") == "y");

    root.removeChildComponent (knob);
}

//==============================================================================
LUTHIER_TEST (RangesUi, randomiseStaysInStockUnlessToldOtherwise)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    RangeState unlocked;
    unlocked.setFamilyAdvanced (RangeFamily::amp, true);
    processor.changeRanges (unlocked, "test");

    const auto ampIds = RangeRegistry::idsInFamily (RangeFamily::amp);

    auto countOutside = [&] (bool respect)
    {
        int outside = 0;

        for (uint64_t seed = 1; seed <= 40; ++seed)
        {
            processor.getPresetManager().randomise (seed * 104729, {}, respect);

            for (const auto& id : ampIds)
                if (RangeRegistry::find (id)->isOutsideStock (plainOf (processor, id)))
                    ++outside;
        }

        return outside;
    };

    CHECK_MSG (countOutside (true) == 0, "randomise left stock with the preference on");
    CHECK_MSG (countOutside (false) > 0,
               "randomise never left stock with the preference off - the test proves nothing");
}

//==============================================================================
/*  The processor's undo stack, one step at a time in both directions.

    Written when the ranges tests above found that a single action could not be
    undone at all and that the first undo after two actions reverted both.
*/
LUTHIER_TEST (Undo, stepsOneActionAtATimeBothWays)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const juce::String gain (ParamIDs::ampGain);
    const float original = plainOf (processor, gain);

    CHECK_MSG (! processor.canUndo(), "a fresh processor has something to undo");

    processor.pushUndoState ("first");
    setPlain (processor, gain, 0.2f);

    CHECK_MSG (processor.canUndo(), "one action cannot be undone");
    CHECK (processor.getUndoDescription() == "first");

    processor.pushUndoState ("second");
    setPlain (processor, gain, 0.9f);

    CHECK (processor.getUndoDescription() == "second");

    processor.undo();
    CHECK_MSG (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f,
               "the first undo did not stop after one action, got " + juce::String (plainOf (processor, gain)));
    CHECK (processor.getUndoDescription() == "first");
    CHECK (processor.getRedoDescription() == "second");

    processor.undo();
    CHECK (std::abs (plainOf (processor, gain) - original) < 1.0e-3f);
    CHECK (! processor.canUndo());

    processor.redo();
    CHECK (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f);

    processor.redo();
    CHECK_MSG (std::abs (plainOf (processor, gain) - 0.9f) < 1.0e-3f,
               "redo did not come back to the latest state, got " + juce::String (plainOf (processor, gain)));
    CHECK (! processor.canRedo());

    // A new action after an undo drops the redo tail.
    processor.undo();
    processor.pushUndoState ("branch");
    setPlain (processor, gain, 0.5f);
    CHECK (! processor.canRedo());

    processor.undo();
    CHECK (std::abs (plainOf (processor, gain) - 0.2f) < 1.0e-3f);
}

//==============================================================================
/*  gui-integration 2: the header padlock shows while the preset has anything
    unlocked, only in Advanced Mode (3.6), and opens Options -> Ranges. */
LUTHIER_TEST (RangesUi, theHeaderPadlockShowsOnlyWhenSomethingIsUnlocked)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    HeaderBar header (processor);
    header.setSize (1600, 48);
    header.setAdvancedMode (true);

    CHECK_MSG (! header.isRangePadlockShowing(), "the padlock showed on a stock preset");

    RangeState unlocked;
    unlocked.setUnlockedIndividually (ParamIDs::ampGain, true);
    processor.changeRanges (unlocked, "test");

    header.setAdvancedMode (true);
    CHECK_MSG (header.isRangePadlockShowing(), "one unlocked control did not show the padlock");

    header.setAdvancedMode (false);
    CHECK_MSG (! header.isRangePadlockShowing(), "the padlock showed in Easy Mode");
}
