#include "PluginEditor.h"
#include "Accessibility/Accessibility.h"

namespace luthier
{

LuthierAudioProcessorEditor::LuthierAudioProcessorEditor (LuthierAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      header (p),
      liveStrip (p),
      practicePanel (p),
      easyPanel (p),
      advancedPanel (p),
      helpPanel (p),
      debugPanel (p),
      optionsPanel (p),
      exportPanel (p),
      presetBrowser (p),
      saveAsPanel (p),
      chordPanel (p),
      secretPanel (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (header);
    addChildComponent (liveStrip);
    addAndMakeVisible (practicePanel);

    // practice-tools 9: the drawer changes the space the panels have, so the
    // window relays out when it opens or is dragged taller.
    practicePanel.onHeightChanged = [this] { resized(); };
    addChildComponent (easyPanel);
    addChildComponent (advancedPanel);

    addAndMakeVisible (chordButton);
    chordButton.setTooltip ("Chord library and the live tab display");
    chordButton.onClick = [this] { showOverlay (&chordPanel); };

    // The overlay host sits on top of everything and is invisible until used.
    addChildComponent (overlayHost);

    addChildComponent (midiLearnArmLayer);

    header.onMidiLearnArmChanged = [this] (bool armed) { setMidiLearnArmed (armed); };

    midiLearnArmLayer.onTargetPicked = [this] (juce::String parameterId)
    {
        const bool claimed = parameterId.isNotEmpty()
                               && processor.getMidiLearn().claimArmedLearn (parameterId);

        /*  A successful claim has already spent the arm and started the learn, so
            the overlay comes down directly. Going through setMidiLearnArmed(false)
            here would cancel the learn that was just started, because disarming
            cancels a learn in flight - which is right for Escape and wrong here.

            Clicking somewhere that is not a control disarms properly instead, so
            the mode can always be left with a click. */
        if (! claimed)
            processor.getMidiLearn().setArmed (false);

        midiLearnArmLayer.setVisible (false);
        header.setMidiLearnArmed (false);
    };

    // ---- header wiring -----------------------------------------------------------
    header.onModeChanged = [this] (bool advanced) { setAdvancedMode (advanced); };
    header.onOpenHelp = [this] { showOverlay (&helpPanel); };
    header.onOpenOptions = [this] { showOverlay (&optionsPanel); };
    header.onOpenExport = [this] { showOverlay (&exportPanel); };
    header.onOpenPresetBrowser = [this] { showOverlay (&presetBrowser); };
    header.onSaveAs = [this] { showOverlay (&saveAsPanel); };

    helpPanel.onOpenDebug = [this] { showOverlay (&debugPanel); };
    presetBrowser.saveAsPanelRequested = [this] { showOverlay (&saveAsPanel); };

    easyPanel.onOpenExport = [this] { showOverlay (&exportPanel); };

    // ---- window --------------------------------------------------------------------
    auto& ui = processor.getUiState();

    setResizable (true, true);

    // Aspect ratio is preserved, as the brief asks, with sane bounds either side.
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setSizeLimits (minimumWidth, minimumHeight, 2400, 1440);
        constrainer->setFixedAspectRatio ((double) defaultWidth / (double) defaultHeight);
    }

    setSize (juce::jmax (minimumWidth, ui.editorWidth),
             juce::jmax (minimumHeight, ui.editorHeight));

    setAdvancedMode (ui.advancedMode);
    header.setAdvancedMode (ui.advancedMode);

    tooltips.setLookAndFeel (&lookAndFeel);

    setWantsKeyboardFocus (true);
    startTimerHz (4);
}

LuthierAudioProcessorEditor::~LuthierAudioProcessorEditor()
{
    stopTimer();

    processor.getUiState().editorWidth = getWidth();
    processor.getUiState().editorHeight = getHeight();

    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==============================================================================
void LuthierAudioProcessorEditor::setAdvancedMode (bool advanced)
{
    advancedMode = advanced;
    processor.getUiState().advancedMode = advanced;

    easyPanel.setVisible (! advanced);
    advancedPanel.setVisible (advanced);

    if (advanced)
        advancedPanel.setSelectedString (processor.getUiState().selectedString);

    resized();
}

void LuthierAudioProcessorEditor::showOverlay (OverlayPanel* panel)
{
    overlayHost.show (panel);
}

//==============================================================================
juce::Rectangle<int> LuthierAudioProcessorEditor::getSecretPixelBounds() const
{
    // The signature notch runs from (6, 18) to (18, 6). The target is the single
    // point at its outer tip, with a couple of pixels of slack so it is findable
    // by someone who is looking but invisible to someone who is not.
    return { 16, 4, 4, 4 };
}

void LuthierAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    const bool over = getSecretPixelBounds().contains (e.getPosition());

    if (over != secretHovered)
    {
        secretHovered = over;
        repaint (getSecretPixelBounds().expanded (6));
    }
}

void LuthierAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (getSecretPixelBounds().contains (e.getPosition()))
    {
        processor.getUiState().easterEggFound = true;
        showOverlay (&secretPanel);
    }
}

//==============================================================================
void LuthierAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // ---- window shape: rounded, with a cutaway on the right ---------------------
    // The brief asks for a guitar-body cutaway in the window's right edge. Plugin
    // windows are rectangular in every host, so the shape is drawn rather than
    // clipped: the background fills the rectangle and the cutaway is carved out of
    // the panel surface, which reads as the intended silhouette without the
    // host-dependent behaviour of an actually non-rectangular window.
    g.fillAll (Palette::backgroundDeep);

    juce::Path shape;
    const float corner = Metrics::windowCorner;
    const float w = (float) bounds.getWidth();
    const float h = (float) bounds.getHeight();

    const float cutawayDepth = juce::jmin (w * 0.055f, 64.0f);
    const float cutawayTop = h * 0.44f;
    const float cutawayBottom = h * 0.86f;

    shape.startNewSubPath (corner, 0.0f);
    shape.lineTo (w - corner, 0.0f);
    shape.quadraticTo (w, 0.0f, w, corner);
    shape.lineTo (w, cutawayTop);

    // The cutaway: a smooth scoop, like the horn of a double-cut body.
    shape.cubicTo (w, cutawayTop + h * 0.06f,
                   w - cutawayDepth, cutawayTop + h * 0.10f,
                   w - cutawayDepth, (cutawayTop + cutawayBottom) * 0.5f);
    shape.cubicTo (w - cutawayDepth, cutawayBottom - h * 0.10f,
                   w, cutawayBottom - h * 0.06f,
                   w, cutawayBottom);

    shape.lineTo (w, h - corner);
    shape.quadraticTo (w, h, w - corner, h);
    shape.lineTo (corner, h);
    shape.quadraticTo (0.0f, h, 0.0f, h - corner);
    shape.lineTo (0.0f, corner);
    shape.quadraticTo (0.0f, 0.0f, corner, 0.0f);
    shape.closeSubPath();

    g.setColour (Palette::background);
    g.fillPath (shape);

    g.setColour (Palette::edge);
    g.strokePath (shape, juce::PathStrokeType (1.0f));

    // ---- signature notch, top-left ------------------------------------------------
    LuthierLookAndFeel::drawSignatureNotch (g, bounds);

    // The hidden target lives at the tip of the notch. Once found it stays faintly
    // marked, so it can be got back to; before that it is invisible.
    if (secretHovered || processor.getUiState().easterEggFound)
    {
        g.setColour (Palette::secondary.withAlpha (secretHovered ? 0.85f : 0.30f));
        g.fillEllipse (getSecretPixelBounds().toFloat().reduced (0.5f));
    }

    // ---- footer ----------------------------------------------------------------------
    auto footer = bounds.removeFromBottom (Metrics::footerHeight);

    g.setColour (Palette::textDisabled);
    g.setFont (Fonts::mono (9.0f));
    g.drawText ("v" JucePlugin_VersionString,
                footer.reduced (Metrics::windowPadding, 0),
                juce::Justification::centredRight, false);

    // CPU and latency, where a player can see them without opening anything.
    g.drawText ("CPU " + juce::String (processor.getEngine().getCpuEstimate(), 1) + "%"
                + "    latency " + juce::String (processor.getLatencySamples()) + " smp",
                footer.reduced (Metrics::windowPadding, 0),
                juce::Justification::centredLeft, false);
}

void LuthierAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    header.setBounds (bounds.removeFromTop (Metrics::headerHeight));

    // live-performance 10: the live strip attaches under the header when Live
    // Mode is on, and takes no space at all when it is off.
    if (liveStrip.isVisible())
        liveStrip.setBounds (bounds.removeFromTop (LiveStrip::preferredHeight));

    auto footer = bounds.removeFromBottom (Metrics::footerHeight);
    chordButton.setBounds (footer.withSizeKeepingCentre (110, Metrics::footerHeight - 2));

    // practice-tools 9: the drawer sits above the footer.
    practicePanel.setBounds (bounds.removeFromBottom (practicePanel.preferredHeight()));

    easyPanel.setBounds (bounds);
    advancedPanel.setBounds (bounds);

    overlayHost.setBounds (getLocalBounds());
    midiLearnArmLayer.setBounds (getLocalBounds());
}

//==============================================================================
void LuthierAudioProcessorEditor::setMidiLearnArmed (bool armed)
{
    processor.getMidiLearn().setArmed (armed);

    midiLearnArmLayer.setVisible (armed);

    if (armed)
        midiLearnArmLayer.toFront (false);

    header.setMidiLearnArmed (armed);
}

void LuthierAudioProcessorEditor::updateLiveStripVisibility()
{
    const bool live = processor.isLiveMode();

    if (live == liveModeShown)
        return;

    liveModeShown = live;
    liveStrip.setVisible (live);

    // live-performance 10: Live Mode locks the Advanced toggle, so that a
    // mis-hit on stage cannot swap the whole window out from under the player.
    if (live && advancedMode)
    {
        setAdvancedMode (false);
        header.setAdvancedMode (false);
    }

    resized();
}

//==============================================================================
void LuthierAudioProcessorEditor::timerCallback()
{
    updateLiveStripVisibility();

    // Tooltips are a user preference, so the window is created or torn down to
    // match rather than the tips being silently empty.
    tooltips.setMillisecondsBeforeTipAppears (
        processor.getUiState().tooltipsEnabled ? Metrics::tooltipDelayMs : 0x7fffffff);

    repaint (getLocalBounds().removeFromBottom (Metrics::footerHeight));
}

//==============================================================================
bool LuthierAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    /*  Every binding is looked up in AccessibilitySettings rather than compared
        against a key code here.

        gui-integration.md section 17 says all shortcuts are rebindable, and
        accessibility.md section 2 puts a rebind table in Options. Both were true
        of the table and false of the plugin: this function used to hard-code its
        keys, so rebinding a shortcut changed the row in the table and nothing
        else. Going through the registry is what connects them.
    */
    auto& shortcuts = AccessibilitySettings::get();

    auto is = [&shortcuts, &key] (const char* actionId)
    {
        const auto* binding = shortcuts.findShortcut (actionId);
        return binding != nullptr && binding->key == key;
    };

    // Escape always closes whatever is open, and is deliberately not rebindable:
    // accessibility 2 makes it the way out of a dialog, so it cannot be lost to a
    // clumsy rebind. An overlay handles it when focused; this is the backstop.
    if (key == juce::KeyPress::escapeKey)
    {
        if (processor.getMidiLearn().isArmed())
        {
            setMidiLearnArmed (false);
            return true;
        }

        if (overlayHost.isShowingOverlay())
        {
            overlayHost.dismiss();
            return true;
        }

        return false;
    }

    if (is ("help"))            { showOverlay (&helpPanel);     return true; }
    if (is ("options"))         { showOverlay (&optionsPanel);  return true; }
    if (is ("presetBrowser"))   { showOverlay (&presetBrowser); return true; }
    if (is ("export"))          { showOverlay (&exportPanel);   return true; }
    if (is ("debugPanel"))      { showOverlay (&debugPanel);    return true; }

    if (is ("showShortcuts"))
    {
        // accessibility 2's "show all shortcuts" surface is the rebind table
        // itself, so this opens Options on the page that holds it.
        showOverlay (&optionsPanel);
        optionsPanel.showShortcutTable();
        return true;
    }

    if (is ("audition"))
    {
        if (processor.isAuditioning())
            processor.stopAudition();
        else
            processor.startAudition (processor.getUiState().auditionType);

        return true;
    }

    if (is ("toggleAdvanced"))
    {
        setAdvancedMode (! advancedMode);
        header.setAdvancedMode (advancedMode);
        return true;
    }

    if (is ("toggleLiveMode"))
    {
        processor.setLiveMode (! processor.isLiveMode());
        updateLiveStripVisibility();
        return true;
    }

    if (is ("togglePractice"))
    {
        practicePanel.setOpen (! practicePanel.isOpen());
        resized();
        return true;
    }

    if (is ("midiLearnArm"))
    {
        setMidiLearnArmed (! processor.getMidiLearn().isArmed());
        return true;
    }

    if (is ("panic"))     { processor.panic();       return true; }
    if (is ("tapTempo"))  { processor.tapTempoNow(); return true; }

    if (is ("killSwitch"))
    {
        // live-performance 6: a keyboard cannot express "held", so from the
        // keyboard this toggles. The on-screen pill and a MIDI footswitch are the
        // momentary routes.
        auto& kill = processor.getKillSwitch();
        kill.setActive (! kill.isActive());
        return true;
    }

    /*  live-performance 2 asks for [ and ] to step snapshots, and the plugin
        already used them to step presets.

        Live Mode decides which. Off, they keep doing what they always did; on,
        they step snapshots, which is what a player with a preset open and eight
        snapshots inside it means by "next". */
    if (is ("previousItem") || is ("nextItem"))
    {
        const bool forward = is ("nextItem");

        if (processor.isLiveMode() && processor.getSnapshots().getNumSnapshots() > 0)
        {
            if (forward) processor.nextSnapshot();
            else         processor.previousSnapshot();

            return true;
        }

        processor.pushUndoState ("Load preset");

        if (forward) processor.getPresetManager().loadNext();
        else         processor.getPresetManager().loadPrevious();

        processor.getParameterBridge().applyAllNow();
        return true;
    }

    if (is ("setlistPrevious") || is ("setlistNext"))
    {
        const bool forward = is ("setlistNext");

        if (forward ? processor.getSetlist().next() : processor.getSetlist().previous())
        {
            processor.applyCurrentSetlistEntry();
            return true;
        }

        return false;
    }

    if (is ("undo")) { processor.undo(); return true; }
    if (is ("redo")) { processor.redo(); return true; }

    if (is ("save"))
    {
        if (! processor.getPresetManager().saveCurrent())
            showOverlay (&saveAsPanel);

        return true;
    }

    if (is ("saveAs")) { showOverlay (&saveAsPanel); return true; }

    if (is ("randomise")) { processor.randomiseParameters(); return true; }

    if (is ("resetAll"))
    {
        processor.pushUndoState ("Reset everything");
        processor.resetEverything();
        return true;
    }

    if (is ("abCompare"))
    {
        processor.setSlotBActive (! processor.isSlotBActive());
        return true;
    }

    /*  live-performance 2: digits recall snapshots directly, shifted for the
        second bank of nine.

        These are not in the rebind registry. Eighteen rows for eighteen digits
        would bury the table section 17 wants a user to be able to read, and the
        binding is positional rather than nominal - digit n recalls snapshot n, so
        there is nothing meaningful to rebind it to. GAPS.md records the
        deviation. */
    if (const auto character = key.getTextCharacter();
        character >= '1' && character <= '9')
    {
        const int index = (character - '1')
                            + (key.getModifiers().isShiftDown() ? 9 : 0);

        if (index < processor.getSnapshots().getNumSnapshots())
        {
            processor.recallSnapshot (index);
            return true;
        }
    }

    return false;
}

} // namespace luthier
