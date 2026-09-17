#include "HeaderBar.h"
#include "../PluginProcessor.h"

namespace luthier
{

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

    addAndMakeVisible (tuningSelector);
    tuningSelector.setLabelVisible (false);
    tuningSelector.attachTo (processor, ParamIDs::tuningPreset,
                             "Open-string tuning. Custom per-string tunings live in Advanced.");

    // ---- preset ---------------------------------------------------------------
    addAndMakeVisible (presetPrev);
    presetPrev.setTooltip ("Previous preset");
    presetPrev.onClick = [this]
    {
        processor.pushUndoState ("Load preset");
        processor.getPresetManager().loadPrevious();
        processor.getParameterBridge().applyAllNow();
    };

    addAndMakeVisible (presetNext);
    presetNext.setTooltip ("Next preset");
    presetNext.onClick = [this]
    {
        processor.pushUndoState ("Load preset");
        processor.getPresetManager().loadNext();
        processor.getParameterBridge().applyAllNow();
    };

    addAndMakeVisible (presetName);
    presetName.setTooltip ("Click to browse the preset bank");
    presetName.onClick = [this] { if (onOpenPresetBrowser) onOpenPresetBrowser(); };

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

    processor.getPresetManager().addChangeListener (this);
    processor.getMidiLearn().addChangeListener (this);

    refreshPresetDisplay();
    updateUndoRedoState();

    startTimerHz (6);
}

HeaderBar::~HeaderBar()
{
    stopTimer();
    processor.getPresetManager().removeChangeListener (this);
    processor.getMidiLearn().removeChangeListener (this);
}

//==============================================================================
void HeaderBar::setAdvancedMode (bool advanced)
{
    advancedMode = advanced;
    modeButton.setToggleState (advanced, juce::dontSendNotification);
    modeButton.setButtonText (advanced ? "Easy" : "Advanced");
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
    menu.addItem (4, "Import preset...");
    menu.addItem (5, "Export preset...");
    menu.addSeparator();
    menu.addItem (6, "Export audio...");
    menu.addItem (7, "Save last MIDI take...",
                  processor.getMidiCapture().getCapturedSeconds() > 0.05);
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

                    processor.pushUndoState (isImport ? "Import preset" : "Open preset");

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

                    const bool ok = processor.getMidiCapture().writeToFile (file, processor.getHostTempo());

                    juce::NativeMessageBox::showAsync (
                        juce::MessageBoxOptions()
                            .withIconType (ok ? juce::MessageBoxIconType::InfoIcon
                                              : juce::MessageBoxIconType::WarningIcon)
                            .withTitle (ok ? "MIDI saved" : "Could not save")
                            .withMessage (ok ? "Saved to\n" + file.getFullPathName()
                                             : "There was nothing in the capture buffer to save.")
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
    auto logoArea = getLocalBounds().withTrimmedLeft (28).withWidth (96);

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (16.0f, true));
    Fonts::drawTrackedText (g, "LUTHIER", logoArea, juce::Justification::centredLeft, 0.14f);

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
    modeButton.setBounds (bounds.removeFromRight (84).reduced (2, 0));
    bounds.removeFromRight (Metrics::gridHalf);

    helpButton.setBounds (bounds.removeFromRight (30).reduced (2, 0));
    panicButton.setBounds (bounds.removeFromRight (56).reduced (2, 0));

    bounds.removeFromRight (Metrics::gridHalf);

    redoButton.setBounds (bounds.removeFromRight (50).reduced (2, 0));
    undoButton.setBounds (bounds.removeFromRight (50).reduced (2, 0));

    bounds.removeFromRight (Metrics::gridHalf);

    copyAB.setBounds (bounds.removeFromRight (40).reduced (2, 0));
    compareB.setBounds (bounds.removeFromRight (28).reduced (2, 0));
    compareA.setBounds (bounds.removeFromRight (28).reduced (2, 0));

    bounds.removeFromRight (Metrics::grid);

    // ---- left-hand cluster -------------------------------------------------------------
    guitarSelector.setBounds (bounds.removeFromLeft (150).reduced (2, 3));
    bounds.removeFromLeft (Metrics::gridHalf);

    tuningSelector.setBounds (bounds.removeFromLeft (128).reduced (2, 3));
    bounds.removeFromLeft (Metrics::grid);

    // ---- preset, filling whatever is left -----------------------------------------------
    fileMenuButton.setBounds (bounds.removeFromRight (56).reduced (2, 0));
    bounds.removeFromRight (Metrics::gridHalf);

    presetPrev.setBounds (bounds.removeFromLeft (24).reduced (1, 3));
    presetNext.setBounds (bounds.removeFromRight (24).reduced (1, 3));
    presetName.setBounds (bounds.reduced (2, 3));
}

} // namespace luthier
