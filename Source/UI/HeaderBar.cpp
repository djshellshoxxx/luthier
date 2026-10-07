#include "HeaderBar.h"
#include "UndoHistoryPanel.h"
#include "MidiOutPanel.h"
#include "MidiExportDefaults.h"
#include "NotationPanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

HeaderBar::HeaderBar (LuthierAudioProcessor& p)
    : processor (p)
{
    addAndMakeVisible (led);

    // output-normalization.md 5.1: visible only while normalization is on.
    addChildComponent (normalizationBadge);
    normalizationBadge.onVisibilityChanged = [this] { resized(); };
    led.setSource (&processor);

    addAndMakeVisible (guitarSelector);
    guitarSelector.setLabelVisible (false);
    guitarSelector.attachTo (processor, ParamIDs::guitarType,
                             "The instrument. Changing this loads its body, woods, pickups, "
                             "strings, tuning and default rig.");

    addAndMakeVisible (tuningSelector);
    tuningSelector.setLabelVisible (false);
    tuningSelector.attachTo (processor, ParamIDs::tuningPreset,
                             "Open-string tuning. Custom per-string tunings live in Advanced.");

    // ---- preset ---------------------------------------------------------------
    addAndMakeVisible (presetPrev);
    presetPrev.setTooltip ("Previous preset");
    presetPrev.onClick = [this] { processor.stepPresetAsUserAction (false); };   // action-and-undo.md 3.8

    addAndMakeVisible (presetNext);
    presetNext.setTooltip ("Next preset");
    presetNext.onClick = [this] { processor.stepPresetAsUserAction (true); };    // action-and-undo.md 3.8

    addAndMakeVisible (presetName);
    presetName.setTooltip ("Click to browse the preset bank");
    presetName.onClick = [this] { if (onOpenPresetBrowser) onOpenPresetBrowser(); };

    addChildComponent (rangePadlock);
    rangePadlock.onClick = [this] { if (onOpenRanges) onOpenRanges(); };

    addAndMakeVisible (fileMenuButton);
    fileMenuButton.setTooltip ("Save, open, import, export and options");
    fileMenuButton.onClick = [this] { showFileMenu(); };

    // ---- A/B ------------------------------------------------------------------
    addAndMakeVisible (compareA);
    compareA.setTooltip ("Compare slot A");
    compareA.setClickingTogglesState (true);
    compareA.setRadioGroupId (1001);
    compareA.setToggleState (true, juce::dontSendNotification);
    compareA.onClick = [this] { processor.setSlotBActive (false); refreshPresetDisplay(); };

    addAndMakeVisible (compareB);
    compareB.setTooltip ("Compare slot B");
    compareB.setClickingTogglesState (true);
    compareB.setRadioGroupId (1001);
    compareB.onClick = [this] { processor.setSlotBActive (true); refreshPresetDisplay(); };

    addAndMakeVisible (copyAB);
    copyAB.setTooltip ("Copy the current slot to the other one");
    copyAB.onClick = [this] { processor.copyAtoB(); };

    // ---- undo / redo ------------------------------------------------------------
    addAndMakeVisible (undoButton);
    undoButton.onClick = [this] { processor.undo(); updateUndoRedoState(); };

    addAndMakeVisible (redoButton);
    redoButton.onClick = [this] { processor.redo(); updateUndoRedoState(); };

    // ---- panic --------------------------------------------------------------------
    addAndMakeVisible (panicButton);
    panicButton.setTooltip ("Stop every string immediately, clear all held notes, and stop the tune, looper, backing track, metronome and progression");
    panicButton.setColour (juce::TextButton::textColourOffId, Palette::warning);
    panicButton.onClick = [this] { processor.panic(); };

    addAndMakeVisible (midiLearnButton);
    midiLearnButton.setTooltip ("Arm MIDI Learn, then click a control to assign it "
                                "to the next CC you move (Ctrl+L)");
    midiLearnButton.setClickingTogglesState (true);
    midiLearnButton.onClick = [this]
    {
        if (onMidiLearnArmChanged)
            onMidiLearnArmChanged (midiLearnButton.getToggleState());
    };

    addAndMakeVisible (helpButton);
    helpButton.setTooltip ("Help, troubleshooting and debug tools");
    helpButton.onClick = [this] { if (onOpenHelp) onOpenHelp(); };

    // global-search.md 6.1 (FEAT-SEARCH): the magnifier opens the palette.
    addAndMakeVisible (searchButton);
    searchButton.onClick = [this] { if (onOpenSearch) onOpenSearch(); };

    // ---- mode ----------------------------------------------------------------------
    addAndMakeVisible (modeButton);
    modeButton.setClickingTogglesState (true);
    modeButton.setTooltip ("Switch between Easy and Advanced");
    modeButton.onClick = [this]
    {
        advancedMode = modeButton.getToggleState();
        modeButton.setButtonText (advancedMode ? "Easy" : "Advanced");

        if (onModeChanged)
            onModeChanged (advancedMode);
    };

    // ---- live mode ---------------------------------------------------------------
    // live-performance 10: Live Mode is a header switch, and while it is on the
    // Advanced toggle is locked so that a mis-hit on stage cannot swap the whole
    // window out from under the player.
    addAndMakeVisible (workshopButton);
    workshopButton.setTooltip ("The Workshop: take the guitar apart, swap parts, move pickups");
    workshopButton.setTitle ("Workshop");
    workshopButton.onClick = [this] { if (onOpenWorkshop) onOpenWorkshop(); };

    addAndMakeVisible (slideButton);
    slideButton.setClickingTogglesState (true);
    slideButton.setTooltip ("Slide Mode: play with a bar instead of frets (S)");
    slideButton.setTitle ("Slide Mode");
    slideButton.onClick = [this]
    {
        auto* p = processor.getState().getParameter (ParamIDs::slideGuitar);

        if (p != nullptr && (p->getValue() > 0.5f) != slideButton.getToggleState())
            toggleSlideMode (processor);
    };

    addAndMakeVisible (liveButton);
    liveButton.setClickingTogglesState (true);
    liveButton.setToggleState (processor.isLiveMode(), juce::dontSendNotification);
    liveButton.setTooltip ("Live Mode: shows the snapshot, setlist, tap, morph, "
                           "kill and monitor strip, and grows every control to a "
                           "size that can be hit without looking.");

    liveButton.onClick = [this]
    {
        processor.setLiveMode (liveButton.getToggleState());
        updateModeButtonEnablement();
    };

    updateModeButtonEnablement();

    processor.getPresetManager().addChangeListener (this);
    processor.getMidiLearn().addChangeListener (this);

    refreshPresetDisplay();
    updateUndoRedoState();

    setOpaque (true);   // paint() fills every pixel: spares the editor's paint under it
    motion.startTimerHz (*this, 6);
}

HeaderBar::~HeaderBar()
{
    motion.stopTimer();
    processor.getPresetManager().removeChangeListener (this);
    processor.getMidiLearn().removeChangeListener (this);
}

//==============================================================================
void HeaderBar::setMidiLearnArmed (bool armed)
{
    midiLearnButton.setToggleState (armed, juce::dontSendNotification);

    // Armed is a mode, so it gets the accent the other mode pills use.
    midiLearnButton.setColour (juce::TextButton::textColourOffId,
                               armed ? Palette::accent : Palette::textMuted);
    midiLearnButton.repaint();
}

void HeaderBar::setAdvancedMode (bool advanced)
{
    advancedMode = advanced;
    modeButton.setToggleState (advanced, juce::dontSendNotification);
    modeButton.setButtonText (advanced ? "Easy" : "Advanced");
    updateRangePadlock();
}

void HeaderBar::toggleSlideMode (LuthierAudioProcessor& processor)
{
    if (auto* p = processor.getState().getParameter (ParamIDs::slideGuitar))
    {
        const bool on = p->getValue() > 0.5f;
        const LuthierAudioProcessor::ScopedUndoAction undo (processor, on ? "Slide Mode off" : "Slide Mode on");

        p->beginChangeGesture();
        p->setValueNotifyingHost (on ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
}

void HeaderBar::updateRangePadlock()
{
    // gui-integration 3.6: Easy Mode leaves the padlock out.
    const bool show = advancedMode && processor.getRanges().isAnythingAdvanced();

    if (show != rangePadlock.isVisible())
    {
        rangePadlock.setVisible (show);
        resized();
    }
}

void HeaderBar::setAdvancedModeAvailable (bool available)
{
    if (available == advancedAvailable)
        return;

    advancedAvailable = available;
    updateModeButtonEnablement();
}

void HeaderBar::setLiveMode (bool live)
{
    if (live == liveButton.getToggleState())
        return;

    liveButton.setToggleState (live, juce::dontSendNotification);
    updateModeButtonEnablement();
}

void HeaderBar::updateModeButtonEnablement()
{
    const bool live = liveButton.getToggleState();

    modeButton.setEnabled (advancedAvailable && ! live);

    /*  A disabled control that does not say why is worse than an enabled one that
        refuses: the tooltip is the only place the reason fits in the header. */
    modeButton.setTooltip (live        ? "Locked while Live Mode is on"
                           : ! advancedAvailable
                                       ? "Advanced Mode needs a wider window"
                                       : "Switch between Easy and Advanced");
}

void HeaderBar::refreshPresetDisplay()
{
    auto& manager = processor.getPresetManager();

    juce::String name = manager.getCurrentPresetName();

    if (name.isEmpty())
        name = "Init";

    if (manager.isCurrentPresetModified())
        name += " *";

    presetName.setButtonText (name);

    compareA.setToggleState (! processor.isSlotBActive(), juce::dontSendNotification);
    compareB.setToggleState (processor.isSlotBActive(), juce::dontSendNotification);
}

void HeaderBar::updateUndoRedoState()
{
    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());

    undoButton.setTooltip (processor.canUndo() ? "Undo " + processor.getUndoDescription() : "Nothing to undo");
    redoButton.setTooltip (processor.canRedo() ? "Redo " + processor.getRedoDescription() : "Nothing to redo");
}

void HeaderBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshPresetDisplay();
    repaint();
}

void HeaderBar::timerCallback()
{
    // SPEC-SWEEP (GD-30): the armed MIDI Learn button pulses at 1 Hz.
    if (midiLearnButton.getToggleState())
    {
        const bool on = LearnPulse::isOnNow();
        midiLearnButton.setColour (juce::TextButton::buttonOnColourId,
                                   Palette::accent.withAlpha (on ? 0.45f : 0.15f));
    }

    updateUndoRedoState();
    updateRangePadlock();

    if (auto* p = processor.getState().getParameter (ParamIDs::slideGuitar))
        slideButton.setToggleState (p->getValue() > 0.5f, juce::dontSendNotification);

    // The MIDI-in indicator blinks when notes arrive: only the dot repaints,
    // not the whole strip with every button on it (OPTIMISATION_LOG).
    if (processor.getEngine().consumeMidiActivity())
        repaint (midiDotArea().getSmallestIntegerContainer());
}

//==============================================================================
juce::PopupMenu HeaderBar::buildUndoHistoryMenu (const LuthierAudioProcessor& processor)
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Undo back to before...");

    for (const auto& item : processor.getUndoHistory (20))
    {
        // action-and-undo.md 5: boundaries drawn as rules with a subtitle.
        if (item.boundary)
        {
            menu.addSeparator();
            menu.addSectionHeader (item.description);
        }

        menu.addItem (1 + item.stepsBack, item.description);
    }

    return menu;
}

void HeaderBar::applyUndoHistoryChoice (LuthierAudioProcessor& processor, int result)
{
    if (result > 1)
        processor.undoSteps (result - 1);
}

void HeaderBar::showUndoHistory()
{
    // action-and-undo.md 9: the list with its search box, in a callout.
    auto panel = std::make_unique<UndoHistoryPanel> (processor);
    auto* raw = panel.get();

    auto& box = juce::CallOutBox::launchAsynchronously (std::move (panel), fileMenuButton.getScreenBounds(), nullptr);
    raw->onChosen = [&box] { box.dismiss(); };
}

juce::PopupMenu HeaderBar::buildFileMenu()
{
    // SPEC-SWEEP (USER_MANUAL UM-7): built and handled apart from showing, so a
    // test can check every documented item and drive the results.
    auto& manager = processor.getPresetManager();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    menu.addItem (1, "Save", ! manager.getCurrentPresetName().isEmpty());
    menu.addItem (2, "Save As...");
    menu.addItem (3, "Open preset file...");
    menu.addSeparator();
    menu.addItem (4, "Import preset...");
    menu.addItem (14, "Import MIDI...");   // midi-export 5 (MODEL-GAPS)
    menu.addItem (5, "Export preset...");
    menu.addSeparator();
    menu.addItem (6, "Export audio...");
    menu.addItem (7, "Save last MIDI take...",
                  processor.getMidiCapture().getCapturedSeconds() > 0.05);
    // notation-export 5: "Export to Notation" beside the MIDI save.
    menu.addItem (13, "Export notation...", ! processor.getPerformanceCapture().getNotes().empty());
    menu.addSeparator();
    menu.addItem (8, "Open user preset folder");
    menu.addItem (9, "Open render folder");
    menu.addSeparator();
    {
        // global-search.md 6.1 (FEAT-SEARCH): always here, the only route below 1280.
        const auto* binding = AccessibilitySettings::get().findShortcut ("search");
        menu.addItem (40, "Search..." + (binding != nullptr && binding->key.isValid()
                                          ? "  " + binding->key.getTextDescription() : juce::String()));
    }
    menu.addItem (10, "Options...");
    menu.addSeparator();
    menu.addItem (15, "Undo history...", processor.getNumUndoSteps() > 0);   // action-and-undo.md 9
    menu.addSeparator();
    menu.addItem (11, "Randomise");
    menu.addItem (12, "Reset all settings to default");

    return menu;
}

void HeaderBar::showFileMenu()
{
    buildFileMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileMenuButton),
                                   [this] (int result) { handleFileMenuResult (result); });
}

void HeaderBar::handleFileMenuResult (int result)
{
    auto& presetManager = processor.getPresetManager();

    switch (result)
    {
        case 1:
            if (! presetManager.saveCurrent())
                if (onSaveAs) onSaveAs();
            break;

        case 2:
            if (onSaveAs)
                onSaveAs();
            break;

        case 40:   // FEAT-SEARCH (an id clear of the menu's 1-15)
            if (onOpenSearch)
                onOpenSearch();
            break;

        case 3:
        case 4:
        {
            const bool isImport = (result == 4);

            // Owned by the header: closing the window cancels it (MIDI chooser lifetime).
            fileChooser.launch (*this, "Open a Luthier preset",
                                PresetManager::getUserPresetFolder(),
                                "*" + juce::String (PresetManager::kFileExtension),
                                juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectFiles,
                                [this, isImport] (const juce::File& file)
            {
                processor.pushUndoBoundary ((isImport ? "Import preset " : "Load preset ") + file.getFileNameWithoutExtension());   // action-and-undo.md 5

                if (isImport)
                    processor.getPresetManager().importPreset (file);
                else
                    processor.getPresetManager().loadPreset (file);

                processor.getParameterBridge().applyAllNow();
            });
            break;
        }

        case 5:
        {
            fileChooser.launch (*this, "Export the current preset",
                                PresetManager::getUserPresetFolder()
                                    .getChildFile (processor.getPresetManager().getCurrentPresetName()
                                                   + PresetManager::kFileExtension),
                                "*" + juce::String (PresetManager::kFileExtension),
                                juce::FileBrowserComponent::saveMode
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
                                [this] (const juce::File& file)
            {
                processor.getPresetManager().exportPreset (file);
            });
            break;
        }

        case 6:
            if (onOpenExport)
                onOpenExport();
            break;

        case 7:
        {
            fileChooser.launch (*this, "Save the last take as MIDI",
                                PresetManager::getRenderFolder().getChildFile (MidiCapture::makeDefaultFileName()),
                                "*.mid",
                                juce::FileBrowserComponent::saveMode
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
                                [this] (const juce::File& file)
            {
                // midi-export 8: the take goes out in the Options -> MIDI profile.
                juce::String error;
                const bool ok = MidiTakeExport::exportCapture (processor, file, MidiExportDefaults::load(),
                                                               0.0, &error);

                juce::NativeMessageBox::showAsync (
                    juce::MessageBoxOptions()
                        .withIconType (ok ? juce::MessageBoxIconType::InfoIcon
                                          : juce::MessageBoxIconType::WarningIcon)
                        .withTitle (ok ? "MIDI saved" : "Could not save")
                        .withMessage (ok ? "Saved to\n" + file.getFullPathName()
                                         : error)
                        .withButton ("OK"),
                    nullptr);
            });
            break;
        }

        case 8:
            PresetManager::getUserPresetFolder().revealToUser();
            break;

        case 9:
            PresetManager::getRenderFolder().revealToUser();
            break;

        case 10:
            if (onOpenOptions)
                onOpenOptions();
            break;

        case 11:
            processor.randomiseParameters();
            break;

        case 13:
        {
            // The format follows the extension chosen; the NOTATION tab has the options.
            fileChooser.launch (*this, "Export the take as notation",
                                PresetManager::getRenderFolder().getChildFile ("Luthier Take.musicxml"),
                                "*.musicxml;*.gp;*.txt;*.mid",
                                juce::FileBrowserComponent::saveMode
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
                                [this] (const juce::File& file)
            {
                auto report = [file] (bool ok, const juce::String& error)
                {
                    juce::NativeMessageBox::showAsync (
                        juce::MessageBoxOptions()
                            .withIconType (ok ? juce::MessageBoxIconType::InfoIcon
                                              : juce::MessageBoxIconType::WarningIcon)
                            .withTitle (ok ? "Notation exported" : "Could not export")
                            .withMessage (ok ? "Saved to\n" + file.getFullPathName() : error)
                            .withButton ("OK"),
                        nullptr);
                };

                // notation-export 0.1: written on the export worker (MODEL-GAPS).
                juce::String error;

                if (! NotationTakeExport::writeAsync (processor, NotationTakeExport::formatForFile (file),
                                                      file, {}, {}, report, &error))
                    report (false, error);
            });
            break;
        }

        case 12:
            processor.resetEverything();
            break;

        case 14:
        {
            // midi-export 5 (MODEL-GAPS): the window asks where it goes.
            // CODEX_COMPLETENESS_LEDGER (MIDI import chooser lifetime): owned by the
            // header, so a window closed while it is open cancels it.
            fileChooser.launch (*this, "Import MIDI",
                                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mid;*.midi",
                                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                [this] (const juce::File& file)
            {
                if (onImportMidi)
                    onImportMidi (file);
            });
            break;
        }

        case 15:   // action-and-undo.md 9
            showUndoHistory();
            break;

        default:
            break;
    }

    refreshPresetDisplay();
}

//==============================================================================
juce::Rectangle<float> HeaderBar::midiDotArea() const noexcept
{
    const auto logoArea = getLocalBounds().withTrimmedLeft (22).withWidth (102);
    return { (float) logoArea.getRight() + 2.0f, (float) getLocalBounds().withTrimmedBottom (1).getCentreY() - 3.0f, 6.0f, 6.0f };
}

void HeaderBar::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

    // The strip is static while notes play; PaintCache keeps the logo's tracked
    // text and the notch out of the LED's and the dot's small repaints.
    backgroundCache.draw (g, getLocalBounds(), 0, [this] (juce::Graphics& g)
    {
        auto bounds = getLocalBounds();

        g.setColour (Palette::panel);
        g.fillRect (bounds);

        // The 1 px separator below the header.
        g.setColour (Palette::edge);
        g.fillRect (bounds.removeFromBottom (1));

        // ---- logo: the brass headstock mark and the name in the display face ---------
        // (visual-polish.md 6.2 and 6.4, TODO V). The window's own notch sits under
        // this strip, so the mark is drawn here, beside the name.
        auto logoArea = getLocalBounds().withTrimmedLeft (22).withWidth (102);

        LuthierLookAndFeel::drawSignatureNotch (g, { logoArea.getX() - 4, (getHeight() - 28) / 2, 20, 28 }, Palette::accent);
        logoArea.removeFromLeft (18);

        g.setColour (Palette::textPrimary);
        g.setFont (Fonts::display (24.0f));
        Fonts::drawTrackedText (g, "LUTHIER", logoArea, juce::Justification::centredLeft, 0.12f);
    }, true);

    // ---- MIDI activity indicator ---------------------------------------------------
    const bool active = processor.getEngine().getMidiInterpreter().getActiveNoteCount() > 0;

    g.setColour (active ? Palette::success : Palette::textDisabled);
    g.fillEllipse (midiDotArea());
}

void HeaderBar::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::grid, Metrics::gridHalf);

    // The LED sits in the very top-left corner of the window, outside the row flow.
    led.setBounds (4, 4, 14, 14);

    bounds.removeFromLeft (20);            // clear the LED
    bounds.removeFromLeft (96 + 14);       // logo and the MIDI dot

    // output-normalization.md 5.1: the badge, only while normalization is on.
    if (normalizationBadge.isVisible())
        normalizationBadge.setBounds (bounds.removeFromLeft (NormalizationBadge::preferredWidth + 4).withTrimmedRight (4));

    /*  gui-integration.md 2: the header collapses gracefully below 1280. Every
        control keeps its place; below 1280 each takes a little less room so the
        preset name keeps at least a readable width (at 1200 it was 24 points and
        read "INIT" in a box barely wider than the word - TODO V screenshots). */
    const bool compact = getWidth() < 1280;
    auto w = [compact] (int full, int small) { return compact ? small : full; };

    // ---- right-hand cluster ---------------------------------------------------------
    modeButton.setBounds (bounds.removeFromRight (w (84, 78)).reduced (2, 0));
    bounds.removeFromRight (Metrics::gridHalf);

    liveButton.setBounds (bounds.removeFromRight (w (52, 44)).reduced (2, 0));
    slideButton.setBounds (bounds.removeFromRight (w (52, 46)).reduced (2, 0));
    workshopButton.setBounds (bounds.removeFromRight (w (82, 74)).reduced (2, 0));
    bounds.removeFromRight (Metrics::gridHalf);

    helpButton.setBounds (bounds.removeFromRight (w (30, 26)).reduced (2, 0));

    // FEAT-SEARCH: the magnifier sits beside Help; below 1280 it is File -> Search.
    {
        const auto* binding = AccessibilitySettings::get().findShortcut ("search");
        const auto key = binding != nullptr && binding->key.isValid() ? binding->key.getTextDescription() : juce::String();
        searchButton.setTooltip ("Search everything" + (key.isNotEmpty() ? " (" + key + ")" : juce::String()));
        searchButton.setVisible (getWidth() >= searchButtonMinWidth);

        if (searchButton.isVisible())
            searchButton.setBounds (bounds.removeFromRight (w (30, 26)).reduced (2, 0));
    }
    panicButton.setBounds (bounds.removeFromRight (w (56, 50)).reduced (2, 0));
    midiLearnButton.setBounds (bounds.removeFromRight (w (54, 48)).reduced (2, 0));

    bounds.removeFromRight (Metrics::gridHalf);

    redoButton.setBounds (bounds.removeFromRight (w (50, 44)).reduced (2, 0));
    undoButton.setBounds (bounds.removeFromRight (w (50, 44)).reduced (2, 0));

    bounds.removeFromRight (Metrics::gridHalf);

    copyAB.setBounds (bounds.removeFromRight (w (40, 36)).reduced (2, 0));
    compareB.setBounds (bounds.removeFromRight (w (28, 26)).reduced (2, 0));
    compareA.setBounds (bounds.removeFromRight (w (28, 26)).reduced (2, 0));

    bounds.removeFromRight (Metrics::grid);

    // ---- left-hand cluster -------------------------------------------------------------
    guitarSelector.setBounds (bounds.removeFromLeft (w (150, 124)).reduced (2, 3));
    bounds.removeFromLeft (Metrics::gridHalf);

    tuningSelector.setBounds (bounds.removeFromLeft (w (128, 104)).reduced (2, 3));
    bounds.removeFromLeft (Metrics::grid);

    // ---- preset, filling whatever is left -----------------------------------------------
    fileMenuButton.setBounds (bounds.removeFromRight (w (56, 46)).reduced (2, 0));
    bounds.removeFromRight (Metrics::gridHalf);

    presetPrev.setBounds (bounds.removeFromLeft (24).reduced (1, 3));
    presetNext.setBounds (bounds.removeFromRight (24).reduced (1, 3));

    if (rangePadlock.isVisible())
        rangePadlock.setBounds (bounds.removeFromRight (22).reduced (1, 3));

    presetName.setBounds (bounds.reduced (2, 3));
}

} // namespace luthier
