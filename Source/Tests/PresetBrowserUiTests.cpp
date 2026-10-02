/*  preset-browser-previews.md 16: the GUI tests (PB-28 to PB-34, PB-37), in the
    style of EditorTests.cpp - a real editor, under xvfb, driven through its
    own keys and components.
*/

#include "PresetBrowserTestHelpers.h"
#include "../PluginEditor.h"
#include "../UI/PresetBrowser/PresetBrowserPanel.h"
#include "../UI/PresetBrowser/PresetRow.h"
#include "../UI/PresetBrowser/PresetFilterSidebar.h"
#include "../UI/PresetBrowser/PresetDetailPane.h"
#include "../UI/PresetBrowser/PresetBrowserOptions.h"
#include "../UI/UiPreferences.h"
#include "../Presets/Preview/PreviewCache.h"

using namespace luthier;
using namespace luthier::tests;
using namespace luthier::tests::browser;

namespace
{
    template <typename T>
    T* findIn (juce::Component& root)
    {
        if (auto* t = dynamic_cast<T*> (&root))
            return t;

        for (auto* child : root.getChildren())
            if (auto* found = findIn<T> (*child))
                return found;

        return nullptr;
    }

    juce::KeyPress shortcutFor (const char* action)
    {
        const auto* binding = AccessibilitySettings::get().findShortcut (action);
        return binding != nullptr ? binding->key : juce::KeyPress();
    }

    /** Keeps the user's real UiPreferences keys and restores them afterwards. */
    struct PrefsGuard
    {
        PrefsGuard()
        {
            for (const auto& k : keys)
                if (UiPreferences::get().has (k))
                    saved.set (k, UiPreferences::get().getString (k, {}));
        }

        ~PrefsGuard()
        {
            // Restored with their original types (the file stores JSON values).
            for (const auto& k : keys)
                UiPreferences::get().remove (k);

            for (const auto& entry : saved)
            {
                const auto v = entry.value.toString();

                if (v == "true" || v == "false")      UiPreferences::get().setBool (entry.name.toString(), v == "true");
                else if (v.containsOnly ("-0123456789") && v.isNotEmpty())
                                                      UiPreferences::get().setInt (entry.name.toString(), v.getIntValue());
                else                                  UiPreferences::get().setString (entry.name.toString(), v);
            }
        }

        const juce::StringArray keys { "presetPreview.enabled", "presetPreview.trigger", "presetPreview.onSelect",
                                       "presetPreview.volumeDb", "presetPreview.whileTransport",
                                       "presetBrowser.sort", "presetBrowser.filters", "presetBrowser.viewSimilar" };
        juce::NamedValueSet saved;
    };

    /** A processor, an editor and the browser open, its caches in scratch. */
    struct Ui
    {
        explicit Ui (bool advanced = false, int width = LuthierAudioProcessorEditor::defaultWidth,
                     int height = LuthierAudioProcessorEditor::defaultHeight)
        {
            PreviewCache::setDefaultFolderOverride (scratch.folder.getChildFile ("cache"));
            PreviewRenderService::setShippedFolderOverride (scratch.folder.getChildFile ("no-shipped-previews"));
            PresetLibraryPrefs::get().setFile (scratch.folder.getChildFile ("preset-library.json"));

            for (const auto& k : guard.keys)
                UiPreferences::get().remove (k);
            processor.getUiState().advancedMode = advanced;
            processor.prepareToPlay (48000.0, 256);

            editor.reset (processor.createEditor());
            editor->setVisible (true);
            editor->setSize (width, height);
            host = findIn<OverlayHost> (*editor);
        }

        ~Ui()
        {
            editor.reset();
            PreviewCache::setDefaultFolderOverride ({});
            PreviewRenderService::setShippedFolderOverride ({});
            PresetLibraryPrefs::get().setFile ({});
        }

        PresetBrowserPanel* open()
        {
            editor->keyPressed (shortcutFor ("presetBrowser"));
            panel = host != nullptr ? dynamic_cast<PresetBrowserPanel*> (host->getCurrentOverlay()) : nullptr;

            if (panel != nullptr)
                processor.getPresetLibrary().refreshSynchronously();

            return panel;
        }

        /** Makes an entry's preview "decoded", as if its render had arrived. */
        void makeReady (int entry)
        {
            auto& library = processor.getPresetLibrary();
            auto audio = std::make_shared<juce::AudioBuffer<float>> (2, 48000 * 2);

            for (int i = 0; i < audio->getNumSamples(); ++i)
            {
                const float v = 0.2f * (float) std::sin (i * 0.05);
                audio->setSample (0, i, v);
                audio->setSample (1, i, v);
            }

            library.getService().injectDecodedForTesting (library.getIndex()[entry].soundHash, audio);
        }

        int entryNamed (const juce::String& name)
        {
            return processor.getPresetLibrary().getIndex().indexOfName (name);
        }

        PrefsGuard guard;
        ScratchFolder scratch;
        LuthierAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        OverlayHost* host = nullptr;
        PresetBrowserPanel* panel = nullptr;
    };
}

//==============================================================================
/*  PB-28: Easy mode, by keyboard. */
LUTHIER_TEST (PresetBrowserUi, PB28_easyModeKeyboard)
{
    Ui ui;
    auto* panel = ui.open();
    CHECK_MSG (panel != nullptr, "Ctrl+O did not open the preset browser");

    if (panel == nullptr)
        return;

    runAudio (ui.processor);

    // Typing focuses search; Down moves into the list.
    CHECK (panel->keyPressed (juce::KeyPress ('j', 0, 'j')));
    CHECK (panel->getQuery() == "j");
    panel->setQuery ("jazz hollow");
    CHECK (panel->getResults().size() >= 1);

    CHECK (panel->keyPressed (juce::KeyPress (juce::KeyPress::downKey), &panel->getSearchBox()));
    const int entry = panel->getSelectedEntry();
    CHECK (entry >= 0);

    // Space starts a preview; the player reports it active.
    ui.makeReady (entry);
    CHECK (panel->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey), &panel->getList()));
    runAudio (ui.processor);
    CHECK (ui.processor.getPreviewPlayer().isActive());

    // Escape stops it; a second Escape closes the browser.
    CHECK (panel->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (! ui.processor.getPreviewPlayer().isActive());
    CHECK (ui.host->isShowingOverlay());
    CHECK (panel->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (! ui.host->isShowingOverlay());

    // Enter loads, with a "Load preset" undo boundary.
    panel = ui.open();
    CHECK (panel != nullptr);

    if (panel == nullptr)
        return;

    panel->setQuery ("jazz hollow");
    panel->selectEntry (ui.entryNamed ("Jazz Hollowbody"));
    const int undoBefore = ui.processor.getNumUndoSteps();
    CHECK (panel->keyPressed (juce::KeyPress (juce::KeyPress::returnKey), &panel->getList()));
    CHECK (ui.processor.getPresetManager().getCurrentPresetName() == "Jazz Hollowbody");
    CHECK (ui.processor.getNumUndoSteps() == undoBefore + 1);
    CHECK (ui.processor.getUndoDescription().contains ("Load preset"));
}

/*  PB-29: the Advanced layout has no overlaps at 1280 x 800 and 2560 x 1600;
    Easy has the chip row and "More like this" instead. */
LUTHIER_TEST (PresetBrowserUi, PB29_layouts)
{
    for (auto size : { juce::Point<int> { 1280, 800 }, juce::Point<int> { 2560, 1600 } })
    {
        Ui ui (true, size.x, size.y);
        auto* panel = ui.open();
        CHECK (panel != nullptr);

        if (panel == nullptr)
            continue;

        panel->resized();

        juce::Array<juce::Component*> parts { &panel->getSearchBox(), &panel->getSortBox(), &panel->getList(),
                                              &panel->getDetailPane(), &panel->getSpeakerButton(),
                                              &panel->getVolumeSlider() };
        parts.add (panel->getSidebar().getParentComponent()->getParentComponent());   // the viewport

        for (auto* c : parts)
            CHECK_MSG (c->isVisible() && ! c->getBounds().isEmpty(), c->getTitle() + " is not laid out");

        for (int i = 0; i < parts.size(); ++i)
            for (int j = i + 1; j < parts.size(); ++j)
                CHECK_MSG (! parts[i]->getBounds().intersects (parts[j]->getBounds()),
                           parts[i]->getTitle() + " overlaps " + parts[j]->getTitle() + " at "
                             + juce::String (size.x) + " x " + juce::String (size.y));

        CHECK (! panel->getChipRow().isVisible());
        CHECK (! panel->getMoreLikeThisButton().isVisible());
        CHECK (panel->getWidth() <= 1040 && panel->getHeight() <= 680);
    }

    Ui easy;
    auto* panel = easy.open();
    CHECK (panel != nullptr);

    if (panel != nullptr)
    {
        CHECK (panel->getChipRow().isVisible());
        CHECK (panel->findChip ("Uses Techniques") != nullptr);
        CHECK (panel->getMoreLikeThisButton().isVisible());
        CHECK (! panel->getSortBox().isVisible());
        CHECK (! panel->getSidebar().getParentComponent()->getParentComponent()->isVisible());
    }
}

/*  PB-30: 300 ms of rest starts a preview; leaving stops it within 30 ms.
    With "Click only", hovering never starts one. */
LUTHIER_TEST (PresetBrowserUi, PB30_hover)
{
    Ui ui;
    auto* panel = ui.open();
    CHECK (panel != nullptr);

    if (panel == nullptr)
        return;

    const int entry = ui.entryNamed ("Surf Reverb");
    ui.makeReady (entry);
    runAudio (ui.processor);

    panel->rowHovered (entry);
    pumpMessages (150);
    runAudio (ui.processor, 1);
    CHECK (! ui.processor.getPreviewPlayer().isActive());   // not before 300 ms

    CHECK (waitFor ([&] { runAudio (ui.processor, 1); return ui.processor.getPreviewPlayer().isActive(); }, 1500));

    panel->rowUnhovered (entry);
    CHECK (! ui.processor.getPreviewPlayer().isActive());

    // Within 30 ms of audio the player has let go of the clip.
    runAudio (ui.processor, (int) std::ceil (0.030 * 48000.0 / 256.0) + 1);
    CHECK (ui.processor.getPreviewPlayer().getInUse (0) == nullptr && ui.processor.getPreviewPlayer().getInUse (1) == nullptr);

    // "Click only": hover never starts one.
    UiPreferences::get().setString ("presetPreview.trigger", "click");
    panel->syncPreviewSettings();
    panel->rowHovered (entry);
    pumpMessages (600);
    runAudio (ui.processor, 2);
    CHECK (! ui.processor.getPreviewPlayer().isActive());
    panel->rowUnhovered (entry);
}

/*  PB-31: the focused browser consumes digits and Space; closed, they reach
    the snapshot recall and the audition. */
LUTHIER_TEST (PresetBrowserUi, PB31_keysDoNotLeak)
{
    Ui ui;
    ui.processor.captureSnapshot (0, "one");
    ui.processor.captureSnapshot (1, "two");
    auto* panel = ui.open();
    CHECK (panel != nullptr);

    if (panel == nullptr)
        return;

    const int snapshotBefore = ui.processor.getSnapshots().getCurrentSnapshot();

    CHECK (panel->keyPressed (juce::KeyPress ('2', 0, '2')));
    CHECK (panel->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    ui.processor.getSnapshots().advancePending();
    CHECK (ui.processor.getSnapshots().getCurrentSnapshot() == snapshotBefore);
    CHECK (! ui.processor.getSnapshots().isRecalling());
    CHECK (! ui.processor.isAuditioning());

    // Closed: the same keys act globally.
    ui.host->dismiss();
    CHECK (ui.editor->keyPressed (juce::KeyPress ('2', 0, '2')));
    CHECK (ui.processor.getSnapshots().isRecalling() || ui.processor.getSnapshots().getCurrentSnapshot() == 1);
    CHECK (ui.editor->keyPressed (shortcutFor ("audition")));
    CHECK (ui.processor.isAuditioning());
    ui.processor.stopAudition();
}

/*  PB-32: accessibility. */
LUTHIER_TEST (PresetBrowserUi, PB32_accessibility)
{
    Ui ui;
    auto* panel = ui.open();
    CHECK (panel != nullptr);

    if (panel == nullptr)
        return;

    const int entry = ui.entryNamed ("Jazz Hollowbody");
    panel->selectEntry (entry);
    panel->getList().updateContent();
    panel->resized();

    // Every row, glyph, heart, star control and chip has a name.
    auto* row = panel->getRowComponentFor (entry);
    CHECK (row != nullptr);

    if (row != nullptr)
    {
        CHECK (row->getTitle().startsWith ("Jazz Hollowbody, Electric"));
        CHECK (row->getTitle().contains ("factory"));
        CHECK (row->getGlyph().getTitle() == "Preview Jazz Hollowbody");
        CHECK (row->getHeart().getTitle().isNotEmpty());
        CHECK (row->getStars().getTitle().isNotEmpty());

        auto handler = row->getStars().createAccessibilityHandler();
        CHECK (handler != nullptr && handler->getValueInterface() != nullptr);

        if (handler != nullptr && handler->getValueInterface() != nullptr)
        {
            const auto range = handler->getValueInterface()->getRange();
            CHECK (range.getMinimumValue() == 0.0 && range.getMaximumValue() == 5.0);
        }
    }

    for (auto* chipText : { "All", "Electric", "Fav", "Recent", "Uses Techniques" })
        if (auto* chip = panel->findChip (chipText))
            CHECK (chip->getTitle().isNotEmpty());

    for (auto* chip : panel->getDetailPane().getChips())
        CHECK (chip->getTitle().isNotEmpty());

    // Tab order: search, chips or sidebar, list, detail pane, footer.
    const int search = panel->getSearchBox().getExplicitFocusOrder();
    const int chips = panel->getChipRow().getExplicitFocusOrder();
    const int list = panel->getList().getExplicitFocusOrder();
    const int detail = panel->getDetailPane().getExplicitFocusOrder();
    const int footer = panel->getSpeakerButton().getExplicitFocusOrder();
    CHECK (search < chips && chips < list && list < detail && detail < footer);

    // "Previewing <name>" is announced.
    ui.makeReady (entry);
    runAudio (ui.processor);
    panel->startPreview (entry, true);
    CHECK (panel->getLastAnnouncement() == "Previewing Jazz Hollowbody");

    // Reduced motion: frames are pixel-stable while it plays.
    auto& settings = AccessibilitySettings::get();
    const bool wasReduced = settings.isReducedMotion();
    settings.setReducedMotion (true);
    panel->getList().updateContent();
    runAudio (ui.processor, 4);
    pumpMessages (80);
    const auto first = panel->createComponentSnapshot (panel->getLocalBounds());
    runAudio (ui.processor, 20);
    pumpMessages (80);
    const auto second = panel->createComponentSnapshot (panel->getLocalBounds());
    CHECK (ui.processor.getPreviewPlayer().isActive());

    bool identical = first.getWidth() == second.getWidth() && first.getHeight() == second.getHeight();

    for (int y = 0; identical && y < first.getHeight(); ++y)
        for (int x = 0; identical && x < first.getWidth(); ++x)
            identical = first.getPixelAt (x, y) == second.getPixelAt (x, y);

    CHECK_MSG (identical, "the browser moved under reduced motion");
    settings.setReducedMotion (wasReduced);
    panel->stopPreview();
}

/*  PB-33: the five keys persist; the footer mirrors them; off hides and blocks. */
LUTHIER_TEST (PresetBrowserUi, PB33_previewSettings)
{
    PrefsGuard outer;

    {
        LuthierAudioProcessor p;
        PresetBrowserAppearanceGroup group (p);
        group.enabledToggle.setToggleState (true, juce::sendNotificationSync);
        group.triggerBox.setSelectedId (2, juce::sendNotificationSync);
        group.onSelectToggle.setToggleState (false, juce::sendNotificationSync);
        group.volumeSlider.setValue (-12.0, juce::sendNotificationSync);
        group.whileTransportToggle.setToggleState (true, juce::sendNotificationSync);
    }

    // A new instance reads them back (the file is re-read).
    UiPreferences::get().load();

    {
        LuthierAudioProcessor p;
        PresetBrowserAppearanceGroup group (p);
        CHECK (group.enabledToggle.getToggleState());
        CHECK (group.triggerBox.getSelectedId() == 2);
        CHECK (! group.onSelectToggle.getToggleState());
        CHECK_NEAR (group.volumeSlider.getValue(), -12.0, 0.01);
        CHECK (group.whileTransportToggle.getToggleState());
    }

    Ui ui;
    UiPreferences::get().setInt ("presetPreview.volumeDb", -12);
    auto* panel = ui.open();
    CHECK (panel != nullptr);

    if (panel == nullptr)
        return;

    // The footer mirrors the keys, both ways.
    CHECK_NEAR (panel->getVolumeSlider().getValue(), -12.0, 0.01);
    panel->getVolumeSlider().setValue (-20.0, juce::sendNotificationSync);
    CHECK (UiPreferences::get().getInt ("presetPreview.volumeDb", 0) == -20);
    CHECK_NEAR (ui.processor.getPreviewPlayer().getVolumeDb(), -20.0, 0.01);

    PresetBrowserAppearanceGroup group (ui.processor);
    CHECK_NEAR (group.volumeSlider.getValue(), -20.0, 0.01);

    // Off: glyphs hidden, every trigger blocked.
    panel->getSpeakerButton().setToggleState (false, juce::sendNotificationSync);
    CHECK (! UiPreferences::get().getBool ("presetPreview.enabled", true));
    CHECK (panel->getFooterText() == "Previews are off - Options -> Appearance");

    const int entry = ui.entryNamed ("Init");
    float progress = 0.0f;
    juce::String failure;
    CHECK (panel->glyphStateFor (entry, progress, failure) == PlayGlyph::State::hidden);

    ui.makeReady (entry);
    runAudio (ui.processor);
    panel->startPreview (entry, true);
    runAudio (ui.processor);
    CHECK (! ui.processor.getPreviewPlayer().isActive());

    panel->getSpeakerButton().setToggleState (true, juce::sendNotificationSync);
}

/*  PB-34: the empty states. */
LUTHIER_TEST (PresetBrowserUi, PB34_emptyStates)
{
    Ui ui;
    auto* panel = ui.open();
    CHECK (panel != nullptr);

    if (panel == nullptr)
        return;

    panel->setQuery ("zzqxv wobblefrob");
    CHECK (panel->getResults().empty());
    CHECK (panel->getEmptyText() == "No presets match \"zzqxv wobblefrob\". Try fewer words or clear filters.");

    panel->clearSearchAndFilters();
    CHECK (! panel->getResults().empty());

    panel->getFilters().favourites = true;
    panel->filtersChanged();
    CHECK (panel->getEmptyText() == "Tap the heart on a preset to keep it here.");

    panel->getFilters().favourites = false;
    panel->getFilters().recent = true;
    panel->filtersChanged();
    CHECK (panel->getEmptyText() == "Presets you load appear here.");
    panel->clearSearchAndFilters();
}

/*  PB-37: the browser, its Options group and the cache buttons are each within
    3 interactions of the header, in both modes (gui-integration.md 0.2). */
LUTHIER_TEST (PresetBrowserUi, PB37_reachability)
{
    for (bool advanced : { false, true })
    {
        Ui ui (advanced, 1280, 800);

        // 1: the header's preset name (or Ctrl+O) opens the browser.
        auto* header = findIn<HeaderBar> (*ui.editor);
        CHECK (header != nullptr && header->onOpenPresetBrowser != nullptr);

        if (header != nullptr && header->onOpenPresetBrowser)
            header->onOpenPresetBrowser();

        CHECK (dynamic_cast<PresetBrowserPanel*> (ui.host->getCurrentOverlay()) != nullptr);
        ui.host->dismiss();

        // 1: Options; 2: its tab; 3: the control is on the page.
        CHECK (ui.editor->keyPressed (shortcutFor ("options")));
        auto* options = dynamic_cast<OptionsPanel*> (ui.host->getCurrentOverlay());
        CHECK (options != nullptr);

        if (options == nullptr)
            continue;

        CHECK (options->showPageNamed ("APPEARANCE"));
        auto* group = findIn<PresetBrowserAppearanceGroup> (*options);
        CHECK (group != nullptr && group->isVisible() && group->getHeight() > 0 && group->enabledToggle.isVisible());

        CHECK (options->showPageNamed ("FILE LOCATIONS"));
        auto* cache = findIn<PresetCacheGroup> (*options);
        CHECK (cache != nullptr && cache->isVisible() && cache->openButton.isVisible() && cache->clearButton.isVisible()
               && cache->getHeight() > 0);

        CHECK (options->showPageNamed ("ACCESSIBILITY"));
        auto* keys = findIn<PresetBrowserKeysGroup> (*options);
        CHECK (keys != nullptr && keys->isVisible() && keys->getHeight() > 0);
        ui.host->dismiss();
    }
}
