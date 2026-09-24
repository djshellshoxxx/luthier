/*  The TUNE tab in the plugin (tune-builder.md 2.1, 12, 14, 15; gui-integration
    17; action-and-undo 3.9). TUNE-HELP-ONBOARDING workstream. */

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Tune/TuneExamples.h"
#include "../Tune/TuneHarmony.h"
#include "../UI/TunePanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    Tune verseTune (const juce::String& kit = "Folk Fingerstyle")
    {
        Tune t;
        t.meta.title = "Integration";
        TuneSection verse;
        verse.name = "Verse";
        verse.lengthBars = 4;
        verse.genreKitId = kit;
        verse.rhythmPatternId = "Folk Down Up";
        t.addSection (verse);
        applyProgressionText (t, 0, "C Am F G");
        return t;
    }
}

//==============================================================================
/*  2.1: a kit brings its suggested tempo, swing and feel while the tempo is
    still the default or the old kit's, never over one the player chose; its
    chord palette adds chords; KIT sets the tempo on request. */
LUTHIER_TEST (TuneIntegration, aGenreKitBringsItsTempoFeelAndPalette)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& session = processor->getTuneSession();
    session.newTune (verseTune (""));
    TunePanel panel (*processor, processor->getTunePlayer(), session);

    auto chooseKit = [&panel, &processor] (const juce::String& name)
    {
        panel.getKitBox().setSelectedId (processor->getGenreKits().indexOf (name) + 1, juce::dontSendNotification);
        panel.getKitBox().onChange();
    };

    chooseKit ("Reggae Skank");
    const auto reggae = TuneKits::getSuggestion ("Reggae Skank");
    CHECK_NEAR (session.getTune().meta.tempoBpm, reggae.tempoBpm, 1.0e-9);
    CHECK_NEAR (session.getTune().meta.swingPercent, reggae.swingPercent, 1.0e-9);
    CHECK_NEAR (session.getTune().getSection (0)->feel, reggae.feel, 1.0e-9);

    // Still on the old kit's tempo: the next kit's comes with it.
    chooseKit ("Metal Chug");
    CHECK_NEAR (session.getTune().meta.tempoBpm, TuneKits::getSuggestion ("Metal Chug").tempoBpm, 1.0e-9);

    // A tempo the player chose stays.
    session.edit (TuneEditClass::other, "tempo", [] (Tune& t) { return t.setTempo (97.0); });
    chooseKit ("Bossa Nova");
    CHECK_NEAR (session.getTune().meta.tempoBpm, 97.0, 1.0e-9);
    CHECK (panel.getKitTempoButton().getButtonText() == "KIT 132");
    CHECK (panel.applyKitTempo());
    CHECK_NEAR (session.getTune().meta.tempoBpm, 132.0, 1.0e-9);

    // The palette, in the key, appends.
    const auto palette = panel.getPaletteChords();
    CHECK (palette.size() == 4);
    CHECK (getChordSymbol (palette[0], false) == "Cmaj7");
    const auto before = session.getTune().getSection (0)->chords.size();
    CHECK (panel.appendPaletteChord (1));
    CHECK (session.getTune().getSection (0)->chords.size() == before + 1);
    CHECK (getChordSymbol (session.getTune().getSection (0)->chords.back(), false) == "Am7");
}

//==============================================================================
/*  gui-integration 17: "New tune  Ctrl+T", rebindable, from anywhere. */
LUTHIER_TEST (TuneIntegration, ctrlTIsInTheShortcutRegistryAndOpensTheTuneTab)
{
    const auto* binding = AccessibilitySettings::get().findShortcut ("newTune");
    CHECK (binding != nullptr);

    if (binding == nullptr)
        return;

    CHECK (binding->key == juce::KeyPress ('t', juce::ModifierKeys::commandModifier, 0));

    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto editor = std::make_unique<LuthierAudioProcessorEditor> (*processor);
    editor->setSize (1400, 840);

    CHECK (editor->keyPressed (binding->key));
    CHECK_MSG (processor->getUiState().advancedMode, "Ctrl+T did not open Advanced mode");
    CHECK (editor->openNewTune() != nullptr);

    // Too narrow for Advanced: it says why rather than doing nothing.
    editor->setSize (940, 560);
    CHECK (editor->openNewTune() == nullptr);
    CHECK (editor->getNotifications().contains ("new-tune"));
}

//==============================================================================
/*  action-and-undo 3.9 / DECISIONS "TUNE in the plugin": the tune keeps its own
    undo stack. A tune edit is not a plugin undo step; the plugin's undo does
    not undo the tune; the tab's Ctrl+Z undoes the tune and not the plugin. */
LUTHIER_TEST (TuneIntegration, theTunesUndoStackIsSeparateFromThePlugins)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& session = processor->getTuneSession();
    session.newTune (verseTune());
    TunePanel panel (*processor, processor->getTunePlayer(), session);

    processor->pushUndoState ("A plugin edit");
    const int pluginSteps = processor->getNumUndoSteps();

    panel.getChordPills().performMenuItem (0, TuneChordPills::deleteItem);
    CHECK (session.getNumUndoSteps() == 1);
    CHECK_MSG (processor->getNumUndoSteps() == pluginSteps, "a tune edit became a plugin undo step");

    processor->undo();
    CHECK_MSG (session.getTune().getSection (0)->chords.size() == 3, "the plugin's undo undid the tune");

    processor->pushUndoState ("Another plugin edit");
    const int stepsNow = processor->getNumUndoSteps();

    CHECK (panel.keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)));
    CHECK (session.getTune().getSection (0)->chords.size() == 4);
    CHECK_MSG (processor->getNumUndoSteps() == stepsNow, "the tune's Ctrl+Z undid a plugin step");

    CHECK (panel.keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)));
    CHECK (session.getTune().getSection (0)->chords.size() == 3);
}
