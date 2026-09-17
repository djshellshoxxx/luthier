#include "PluginEditor.h"

namespace luthier
{

LuthierAudioProcessorEditor::LuthierAudioProcessorEditor (LuthierAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      header (p),
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
    addChildComponent (easyPanel);
    addChildComponent (advancedPanel);

    addAndMakeVisible (chordButton);
    chordButton.setTooltip ("Chord library and the live tab display");
    chordButton.onClick = [this] { showOverlay (&chordPanel); };

    // The overlay host sits on top of everything and is invisible until used.
    addChildComponent (overlayHost);

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

    auto footer = bounds.removeFromBottom (Metrics::footerHeight);
    chordButton.setBounds (footer.withSizeKeepingCentre (110, Metrics::footerHeight - 2));

    easyPanel.setBounds (bounds);
    advancedPanel.setBounds (bounds);

    overlayHost.setBounds (getLocalBounds());
}

//==============================================================================
void LuthierAudioProcessorEditor::timerCallback()
{
    // Tooltips are a user preference, so the window is created or torn down to
    // match rather than the tips being silently empty.
    tooltips.setMillisecondsBeforeTipAppears (
        processor.getUiState().tooltipsEnabled ? Metrics::tooltipDelayMs : 0x7fffffff);

    repaint (getLocalBounds().removeFromBottom (Metrics::footerHeight));
}

//==============================================================================
bool LuthierAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const bool command = key.getModifiers().isCommandDown();
    const bool shift = key.getModifiers().isShiftDown();

    // Escape always closes whatever is open. An overlay handles this itself when
    // it has focus; this is the backstop for when it does not.
    if (key == juce::KeyPress::escapeKey)
    {
        if (overlayHost.isShowingOverlay())
        {
            overlayHost.dismiss();
            return true;
        }

        return false;
    }

    if (key == juce::KeyPress::F1Key)
    {
        showOverlay (&helpPanel);
        return true;
    }

    if (key == juce::KeyPress::spaceKey)
    {
        if (processor.isAuditioning())
            processor.stopAudition();
        else
            processor.startAudition (processor.getUiState().auditionType);

        return true;
    }

    if (key == juce::KeyPress::tabKey)
    {
        setAdvancedMode (! advancedMode);
        header.setAdvancedMode (advancedMode);
        return true;
    }

    if (key.getTextCharacter() == '0')
    {
        processor.panic();
        return true;
    }

    if (key.getTextCharacter() == '[')
    {
        processor.pushUndoState ("Load preset");
        processor.getPresetManager().loadPrevious();
        processor.getParameterBridge().applyAllNow();
        return true;
    }

    if (key.getTextCharacter() == ']')
    {
        processor.pushUndoState ("Load preset");
        processor.getPresetManager().loadNext();
        processor.getParameterBridge().applyAllNow();
        return true;
    }

    if (! command)
        return false;

    switch (key.getKeyCode())
    {
        case 'Z':
            if (shift) processor.redo();
            else       processor.undo();
            return true;

        case 'Y':
            processor.redo();
            return true;

        case 'S':
            if (shift)
                showOverlay (&saveAsPanel);
            else if (! processor.getPresetManager().saveCurrent())
                showOverlay (&saveAsPanel);
            return true;

        case 'R':
            processor.randomiseParameters();
            return true;

        case 'E':
            showOverlay (&exportPanel);
            return true;

        case 'P':
            showOverlay (&presetBrowser);
            return true;

        case 'D':
            showOverlay (&debugPanel);
            return true;

        case ',':
            showOverlay (&optionsPanel);
            return true;

        default:
            break;
    }

    return false;
}

} // namespace luthier
