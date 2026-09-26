/*  SPEC-SWEEP: GI-100 (gui-integration.md 17) - every action in the shortcut
    registry reaches the editor's dispatcher and does something observable. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/Overlays.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    OverlayHost* findHost (juce::Component& root)
    {
        for (auto* c : root.getChildren())
        {
            if (auto* h = dynamic_cast<OverlayHost*> (c))
                return h;

            if (auto* h = findHost (*c))
                return h;
        }

        return nullptr;
    }
}

LUTHIER_TEST (Editor, everyActionShortcutDispatches)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
        return;

    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);
    auto* host = findHost (*editor);
    CHECK (host != nullptr);

    /*  Left out on purpose: the ones that touch the user's files or the OS
        file browser (save, reveal*), and the setlist steps, which return false
        by design when no setlist is loaded (checked below). */
    const juce::StringArray skipped { "save", "revealPreset", "revealGuitar", "saveGuitarAs",
                                      "setlistPrevious", "setlistNext",
                                      "previousWorkspaceTab", "nextWorkspaceTab" };   // Advanced only (below)

    const juce::StringArray overlays { "help", "showShortcuts", "presetBrowser", "saveAs", "export",
                                       "options", "debugPanel" };

    juce::StringArray dead;

    for (const auto& binding : AccessibilitySettings::get().getShortcuts())
    {
        const auto id = binding.id;

        if (skipped.contains (id) || ! binding.key.isValid())
            continue;

        const bool liveBefore = processor.isLiveMode();
        const bool killBefore = processor.getKillSwitch().isActive();
        const bool slotBBefore = processor.isSlotBActive();

        if (! editor->keyPressed (binding.key))
        {
            dead.add (id);
            continue;
        }

        if (id == "toggleLiveMode")
            CHECK (processor.isLiveMode() != liveBefore);

        if (id == "killSwitch")
            CHECK (processor.getKillSwitch().isActive() != killBefore);

        if (id == "abCompare")
            CHECK (processor.isSlotBActive() != slotBBefore);

        if (host != nullptr && overlays.contains (id))
        {
            CHECK_MSG (host->isShowingOverlay(), "\"" + id + "\" opened no overlay");
            host->dismiss();
        }

        // Put the toggles back so the next action runs in the default state.
        if (id == "toggleLiveMode" || id == "toggleSlideMode" || id == "togglePractice"
            || id == "killSwitch" || id == "toggleAdvanced")
            editor->keyPressed (binding.key);

        if (host != nullptr && host->isShowingOverlay())
            host->dismiss();
    }

    CHECK_MSG (dead.isEmpty(), "shortcuts that did nothing: " + dead.joinIntoString (", "));

    // The Column 4 tab steps act in Advanced, where the tabs are.
    if (const auto* advanced = AccessibilitySettings::get().findShortcut ("toggleAdvanced"))
    {
        processor.setLiveMode (false);

        if (! processor.getUiState().advancedMode)
            CHECK (editor->keyPressed (advanced->key));

        CHECK (processor.getUiState().advancedMode);

        for (const char* id : { "nextWorkspaceTab", "previousWorkspaceTab" })
            if (const auto* b = AccessibilitySettings::get().findShortcut (id))
                CHECK_MSG (editor->keyPressed (b->key), juce::String (id) + " did nothing in Advanced");
    }

    // The setlist steps say "nothing to step" with no setlist.
    if (const auto* next = AccessibilitySettings::get().findShortcut ("setlistNext"))
        CHECK (! editor->keyPressed (next->key));
}
