#include "HeaderBar.h"
#include "MidiOutPanel.h"
#include "MidiExportDefaults.h"
#include "NotationPanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Localisation.h"

namespace luthier
{

//==============================================================================
void TwoLineTextButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    const bool on = getToggleState();

    getLookAndFeel().drawButtonBackground (g, *this,
                                           findColour (on ? buttonOnColourId : buttonColourId),
                                           isHighlighted, isDown);

    auto colour = findColour (on ? textColourOnId : textColourOffId);

    if (! isEnabled())
        colour = Palette::textDisabled;
    else if (isHighlighted && ! on)
        colour = colour.brighter (0.25f);

    const auto text = getButtonText().toUpperCase();
    const int split = text.indexOf (" & ");
    const auto first = split >= 0 ? text.substring (0, split) : text;
    const auto second = split >= 0 ? text.substring (split + 1) : juce::String();

    g.setColour (colour);
    g.setFont (Fonts::ui (juce::jlimit (8.0f, 11.0f, (float) getHeight() * 0.28f), true));

    auto area = getLocalBounds();

    if (second.isEmpty())
    {
        Fonts::drawTrackedText (g, first, area, juce::Justification::centred);
        return;
    }

    const int half = area.getHeight() / 2;
    Fonts::drawTrackedText (g, first, area.removeFromTop (half).withTrimmedTop (3),
                            juce::Justification::centredBottom);
    Fonts::drawTrackedText (g, second, area.withTrimmedBottom (3),
                            juce::Justification::centredTop);
}

//==============================================================================
HeaderBar::HeaderBar (LuthierAudioProcessor& p)
    : processor (p)
{
    addAndMakeVisible (led);
    led.setSource (&processor);

    addAndMakeVisible (guitarSelector);
    guitarSelector.setLabelVisible (false);
    guitarSelector.attachTo (processor, ParamIDs::guitarType,
                             "The instrument. Changing this loads its body, woods, pickups, "
                             "strings, tuning and default rig.");
    processor.addListener (this);   // the selector's gesture: see audioProcessorParameterChangeGestureEnd

    addAndMakeVisible (tuningSelector);
    tuningSelector.setLabelVisible (false);
    tuningSelector.attachTo (processor, ParamIDs::tuningPreset,
                             "Open-string tuning. Custom per-string tunings live in Advanced.");

    // ---- preset ---------------------------------------------------------------
    addAndMakeVisible (presetPrev);
    presetPrev.setTooltip ("Previous preset");
    presetPrev.onClick = [this]
    {
        // action-and-undo.md 5: a preset load is a state boundary.
        processor.pushUndoBoundary ("Load preset");
        processor.getPresetManager().loadPrevious();
        processor.getParameterBridge().applyAllNow();
    };

    addAndMakeVisible (presetNext);
    presetNext.setTooltip ("Next preset");
    presetNext.onClick = [this]
    {
        processor.pushUndoBoundary ("Load preset");
        processor.getPresetManager().loadNext();
        processor.getParameterBridge().applyAllNow();
    };

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
    panicButton.setTooltip ("Stop every string immediately and clear all held notes");
    panicButton.setColour (juce::TextButton::textColourOffId, Palette::warning);
    panicButton.onClick = [this] { processor.panic(); };

    /*  Beside Panic, in the same warning colour: Panic ends the notes; this ends
        the loop that would start them again. A tune left looping, a looper, a
        free-running rhythm engine and a kill switch all survived Reset, and a
        user with a runaway loop hit Reset and Panic "a bunch of times". */
    addAndMakeVisible (resetStopButton);
    resetStopButton.setButtonText (tr ("header.resetStop"));
    resetStopButton.setTitle (tr ("header.resetStop.title"));
    resetStopButton.setTooltip (tr ("header.resetStop.tooltip"));
    resetStopButton.setColour (juce::TextButton::textColourOffId, Palette::warning);
    resetStopButton.onClick = [this] { processor.resetAndStop(); refreshPresetDisplay(); };

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

    startTimerHz (6);
}

HeaderBar::~HeaderBar()
{
    stopTimer();
    processor.removeListener (this);
    processor.getPresetManager().removeChangeListener (this);
    processor.getMidiLearn().removeChangeListener (this);
}

void HeaderBar::audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int parameterIndex)
{
    auto* param = processor.getState().getParameter (ParamIDs::guitarType);

    if (param == nullptr || param->getParameterIndex() != parameterIndex)
        return;

    // The guitar already loaded picked again is no pick: the pass that would
    // consume the flag never runs, and it must not linger for a later change.
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        if (choice->getIndex() != (int) processor.getEngine().getGuitarType())
            processor.getParameterBridge().followGuitarHandOnNextLoad();
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
    updateUndoRedoState();
    updateRangePadlock();

    if (auto* p = processor.getState().getParameter (ParamIDs::slideGuitar))
        slideButton.setToggleState (p->getValue() > 0.5f, juce::dontSendNotification);

    // The MIDI-in indicator blinks when notes arrive.
    if (processor.getEngine().consumeMidiActivity())
        repaint();
}

//==============================================================================
void HeaderBar::showFileMenu()
{
    auto& manager = processor.getPresetManager();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    menu.addItem (1, "Save", ! manager.getCurrentPresetName().isEmpty());
    menu.addItem (2, "Save As...");
    menu.addItem (3, "Open preset file...");
    menu.addSeparator();
    // gui-integration 19: the Tune Builder's entries in the File menu.
    menu.addItem (14, "New Tune...", onNewTune != nullptr);
    menu.addItem (15, "Import MIDI...", onImportMidi != nullptr);
    menu.addSeparator();
    menu.addItem (4, "Import preset...");
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
    menu.addItem (10, "Options...");
    menu.addSeparator();
    menu.addItem (11, "Randomise");
    menu.addItem (12, "Reset all settings to default");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileMenuButton),
                        [this] (int result)
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

            case 3:
            case 4:
            {
                auto chooser = std::make_shared<juce::FileChooser> (
                    "Open a Luthier preset",
                    PresetManager::getUserPresetFolder(),
                    "*" + juce::String (PresetManager::kFileExtension));

                const bool isImport = (result == 4);

                chooser->launchAsync (juce::FileBrowserComponent::openMode
                                        | juce::FileBrowserComponent::canSelectFiles,
                                      [this, chooser, isImport] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();

                    if (file == juce::File())
                        return;

                    processor.pushUndoBoundary ((isImport ? "Import preset " : "Open preset ")
                                                  + file.getFileNameWithoutExtension());

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
                auto chooser = std::make_shared<juce::FileChooser> (
                    "Export the current preset",
                    PresetManager::getUserPresetFolder()
                        .getChildFile (processor.getPresetManager().getCurrentPresetName()
                                       + PresetManager::kFileExtension),
                    "*" + juce::String (PresetManager::kFileExtension));

                chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                        | juce::FileBrowserComponent::warnAboutOverwriting,
                                      [this, chooser] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();

                    if (file != juce::File())
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
                auto chooser = std::make_shared<juce::FileChooser> (
                    "Save the last take as MIDI",
                    PresetManager::getRenderFolder().getChildFile (MidiCapture::makeDefaultFileName()),
                    "*.mid");

                chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                        | juce::FileBrowserComponent::warnAboutOverwriting,
                                      [this, chooser] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();

                    if (file == juce::File())
                        return;

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

            case 14:
                if (onNewTune)
                    onNewTune();
                break;

            case 15:
            {
                // midi-export 5: File -> Import MIDI loads the file as a new
                // tune in the Tune Builder; the editor shows the tab.
                auto chooser = std::make_shared<juce::FileChooser> (
                    "Import a MIDI file into the Tune Builder",
                    PresetManager::getRenderFolder(),
                    "*.mid;*.midi");

                chooser->launchAsync (juce::FileBrowserComponent::openMode
                                        | juce::FileBrowserComponent::canSelectFiles,
                                      [this, chooser] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();

                    if (file != juce::File() && onImportMidi)
                        onImportMidi (file);
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
                auto chooser = std::make_shared<juce::FileChooser> (
                    "Export the take as notation",
                    PresetManager::getRenderFolder().getChildFile ("Luthier Take.musicxml"),
                    "*.musicxml;*.gp;*.txt;*.mid");

                chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                        | juce::FileBrowserComponent::warnAboutOverwriting,
                                      [this, chooser] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();

                    if (file == juce::File())
                        return;

                    juce::String error;
                    const bool ok = NotationTakeExport::write (processor, NotationTakeExport::formatForFile (file),
                                                               file, {}, {}, &error);

                    juce::NativeMessageBox::showAsync (
                        juce::MessageBoxOptions()
                            .withIconType (ok ? juce::MessageBoxIconType::InfoIcon
                                              : juce::MessageBoxIconType::WarningIcon)
                            .withTitle (ok ? "Notation exported" : "Could not export")
                            .withMessage (ok ? "Saved to\n" + file.getFullPathName() : error)
                            .withButton ("OK"),
                        nullptr);
                });
                break;
            }

            case 12:
                processor.resetEverything();
                break;

            default:
                break;
        }

        refreshPresetDisplay();
    });
}

//==============================================================================
void HeaderBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (Palette::panel);
    g.fillRect (bounds);

    // The 1 px separator below the header.
    g.setColour (Palette::edge);
    g.fillRect (bounds.removeFromBottom (1));

    // ---- logo -------------------------------------------------------------------
    // visual-polish.md 6.2 / 6.4: the plugin's name is set in the display face,
    // the condensed vintage lettering of a 1950s amp badge, beside the brass
    // headstock mark. High contrast keeps the face; only textures are turned off.
    auto logoArea = getLocalBounds().withTrimmedLeft (28).withWidth (96);

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::display (22.0f));
    Fonts::drawTrackedText (g, "LUTHIER", logoArea, juce::Justification::centredLeft, 0.16f);

    // ---- MIDI activity indicator ---------------------------------------------------
    const bool active = processor.getEngine().getMidiInterpreter().getActiveNoteCount() > 0;

    auto midiDot = juce::Rectangle<float> ((float) logoArea.getRight() + 2.0f,
                                           (float) bounds.getCentreY() - 3.0f, 6.0f, 6.0f);

    g.setColour (active ? Palette::success : Palette::textDisabled);
    g.fillEllipse (midiDot);
}

void HeaderBar::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::grid, Metrics::gridHalf);

    // The LED sits in the very top-left corner of the window, outside the row flow.
    led.setBounds (4, 4, 14, 14);

    bounds.removeFromLeft (20);            // clear the LED
    bounds.removeFromLeft (96 + 14);       // logo and the MIDI dot

    // ---- right-hand cluster ---------------------------------------------------------
    // Each button is its uppercase label plus a few pixels: the row has to fit
    // RESET & STOP at gui-integration's 1200 px minimum with a preset name
    // still readable in the middle.
    modeButton.setBounds (bounds.removeFromRight (78).reduced (2, 0));
    bounds.removeFromRight (Metrics::gridHalf);

    liveButton.setBounds (bounds.removeFromRight (46).reduced (2, 0));
    slideButton.setBounds (bounds.removeFromRight (48).reduced (2, 0));
    workshopButton.setBounds (bounds.removeFromRight (76).reduced (2, 0));
    bounds.removeFromRight (Metrics::gridHalf);

    helpButton.setBounds (bounds.removeFromRight (28).reduced (2, 0));
    panicButton.setBounds (bounds.removeFromRight (52).reduced (2, 0));
    resetStopButton.setBounds (bounds.removeFromRight (64).reduced (2, 0));
    midiLearnButton.setBounds (bounds.removeFromRight (50).reduced (2, 0));

    bounds.removeFromRight (Metrics::gridHalf);

    redoButton.setBounds (bounds.removeFromRight (46).reduced (2, 0));
    undoButton.setBounds (bounds.removeFromRight (46).reduced (2, 0));

    bounds.removeFromRight (Metrics::gridHalf);

    copyAB.setBounds (bounds.removeFromRight (38).reduced (2, 0));
    compareB.setBounds (bounds.removeFromRight (28).reduced (2, 0));
    compareA.setBounds (bounds.removeFromRight (28).reduced (2, 0));

    bounds.removeFromRight (Metrics::grid);

    // ---- left-hand cluster -------------------------------------------------------------
    /*  The selectors give way before the preset name does: below about 1280
        the row is short, and a guitar name losing its last letters is better
        than the preset name losing all of them (gui-integration 2, "collapses
        gracefully"). Each has a floor its usual names still fit. */
    int guitarWidth = 144, tuningWidth = 120;

    {
        const int presetFixed = 56 + Metrics::gridHalf + 24 + 24 + (rangePadlock.isVisible() ? 22 : 0);
        const int wantedName = 96;
        const int spare = bounds.getWidth() - (guitarWidth + Metrics::gridHalf + tuningWidth + Metrics::grid)
                          - presetFixed - wantedName;

        if (spare < 0)
        {
            const int fromGuitar = juce::jmin (-spare, guitarWidth - 112);
            guitarWidth -= fromGuitar;
            tuningWidth -= juce::jmin (-spare - fromGuitar, tuningWidth - 96);
        }
    }

    guitarSelector.setBounds (bounds.removeFromLeft (guitarWidth).reduced (2, 3));
    bounds.removeFromLeft (Metrics::gridHalf);

    tuningSelector.setBounds (bounds.removeFromLeft (tuningWidth).reduced (2, 3));
    bounds.removeFromLeft (Metrics::grid);

    // ---- preset, filling whatever is left -----------------------------------------------
    fileMenuButton.setBounds (bounds.removeFromRight (56).reduced (2, 0));
    bounds.removeFromRight (Metrics::gridHalf);

    presetPrev.setBounds (bounds.removeFromLeft (24).reduced (1, 3));
    presetNext.setBounds (bounds.removeFromRight (24).reduced (1, 3));

    if (rangePadlock.isVisible())
        rangePadlock.setBounds (bounds.removeFromRight (22).reduced (1, 3));

    presetName.setBounds (bounds.reduced (2, 3));
}

} // namespace luthier
