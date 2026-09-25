#include "PluginEditor.h"
#include "UI/NewFeatureDots.h"
#include "UI/CpuReliefUi.h"
#include "UI/RangesUi.h"
#include "UI/UiPreferences.h"
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
      workshopOverlay (p),
      secretPanel (p)
{
    setLookAndFeel (&lookAndFeel);

    shownPalette = Palette::current();
    AccessibilitySettings::get().addChangeListener (this);

    // accessibility.md 4: the UI scale (75-200 %) was stored and offered but
    // never applied; the host is told through the editor's scale factor.
    setScaleFactor ((float) AccessibilitySettings::get().getUiScale());

    // gui-integration 20's NEW dots: the first launch of this version starts the week.
    NewFeatureDots::noteLaunch (JucePlugin_VersionString, juce::Time::getCurrentTime());

    addAndMakeVisible (header);
    addChildComponent (liveStrip);
    addAndMakeVisible (practicePanel);

    // practice-tools 9: the drawer changes the space the panels have, so the
    // window relays out when it opens or is dragged taller.
    practicePanel.onHeightChanged = [this] { resized(); };
    addChildComponent (easyPanel);
    addChildComponent (advancedPanel);

    addAndMakeVisible (chordButton);

    // gui-integration 12: the footer's data stream, one line of the internals.
    addChildComponent (dataStream);
    dataStream.setNumLines (1);
    dataStream.setSource (&processor);
    dataStream.setVisible (DataStreamDisplay::isEnabledByUser());
    chordButton.setTooltip ("Chord library and the live tab display");
    chordButton.onClick = [this] { showOverlay (&chordPanel); };

    /*  The notice is in the layout rather than over it, so when it takes itself
        away the window has to give the space back. It goes in before the overlay
        host deliberately: an overlay is the thing in front, and a status strip
        that painted over an open dialog would be a worse bug than the silent
        mode switch it exists to explain. */
    addChildComponent (inlineNotice);
    inlineNotice.onVisibilityChanged = [this] { resized(); };

    // gui-integration 15: same arrangement, at the top of the window instead of
    // the bottom. It reclaims its 32 px the moment the queue empties.
    addChildComponent (notifications);
    notifications.onVisibilityChanged = [this] { resized(); };

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
    header.onOpenHelp = [this] { openHelp (getHelpContext()); };

    // gui-integration.md 6: the wrench opens the WORKSHOP tab in Advanced mode
    // and the same bench as an overlay in Easy mode.
    header.onOpenWorkshop = [this]
    {
        if (advancedMode)
            advancedPanel.setWorkspaceTabNamed ("WORKSHOP");
        else
            showOverlay (&workshopOverlay);
    };

    workshopOverlay.getPanel().onSaveAsGuitar = [this] { showSaveGuitarDialog(); };

    if (auto* bench = advancedPanel.getWorkshopPanel())
        bench->onSaveAsGuitar = [this] { showSaveGuitarDialog(); };
    header.onOpenOptions = [this] { showOverlay (&optionsPanel); };

    // gui-integration 20: a panel's `?` opens Help pinned to it.
    openHelpForPanel = [safe = juce::Component::SafePointer<LuthierAudioProcessorEditor> (this)] (const juce::String& panel)
    {
        if (safe != nullptr)
            safe->openHelp (panel);
    };

    // gui-integration 16 item 13: a control's "Show in Options -> Shortcuts".
    showShortcutInOptions = [safe = juce::Component::SafePointer<LuthierAudioProcessorEditor> (this)] (const juce::String& action)
    {
        if (safe == nullptr)
            return;

        juce::String description;
        if (const auto* binding = AccessibilitySettings::get().findShortcut (action))
            description = tr (binding->descriptionKey);

        safe->optionsPanel.showShortcutTable (description);
        safe->showOverlay (&safe->optionsPanel);
    };
    header.onOpenRanges = [this] { showOptionsPage ("RANGES"); };
    header.onOpenExport = [this] { showOverlay (&exportPanel); };
    header.onOpenPresetBrowser = [this] { showOverlay (&presetBrowser); };
    header.onSaveAs = [this] { showOverlay (&saveAsPanel); };

    // The overlay and the HELP tab are one HelpTab in two places; both reach
    // the debug tools and the shortcut table the same way.
    auto wireHelp = [this] (HelpTab& help)
    {
        help.onOpenDebug = [this] { showOverlay (&debugPanel); };
        help.onOpenShortcutTable = [this]
        {
            showOverlay (&optionsPanel);
            optionsPanel.showShortcutTable();
        };
    };

    wireHelp (helpPanel.getView());

    if (auto* helpTab = advancedPanel.getHelpTab())
        wireHelp (*helpTab);

    // Options -> Diagnostics offers the same window. An overlay cannot show
    // another overlay, so the request comes out to here.
    optionsPanel.onShowDebugWindow = [this] { showOverlay (&debugPanel); };
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

    /*  setAdvancedMode refuses if the restored size is too narrow for it, and
        header.setAdvancedMode is given what it actually decided rather than what
        was asked for - otherwise the switch reads "Advanced" over an Easy panel. */
    setAdvancedMode (ui.advancedMode);
    header.setAdvancedMode (advancedMode);

    /*  On the way in, the refusal is silent. A window restored below 1000 points
        is a window that was already this size last session, and a notice about a
        mode the user has not touched yet is noise; the disabled toggle in the
        header carries it instead. */
    inlineNotice.dismiss();

    tooltips.setLookAndFeel (&lookAndFeel);

    setWantsKeyboardFocus (true);

    // performance-budget.md 8: relief 7's opt-out is a user preference.
    CpuReliefUi::applySavedChoice (processor);

    // The processor cannot read UiPreferences, so it is told (advanced-ranges.md 5).
    processor.setRandomiseRespectsStock (RangesUi::randomiseRespectsStock());

    // Controls attached during construction already see the live ranges.
    seenRangeGeneration = RangeState::getGeneration();
    startTimerHz (4);

    /*  gui-integration 15. Last in the constructor, because a banner posting
        itself makes the strip visible and calls resized(), and everything it
        lays out has to exist by then. */
    postStartupNotifications();
}

LuthierAudioProcessorEditor::~LuthierAudioProcessorEditor()
{
    stopTimer();
    AccessibilitySettings::get().removeChangeListener (this);

    processor.getUiState().editorWidth = getWidth();
    processor.getUiState().editorHeight = getHeight();

    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==============================================================================
bool LuthierAudioProcessorEditor::isAdvancedModeAvailable() const noexcept
{
    return getWidth() >= AdvancedPanel::minimumUsableWidth;
}

juce::String LuthierAudioProcessorEditor::advancedUnavailableMessage()
{
    return "Advanced Mode needs a window at least "
             + juce::String (AdvancedPanel::minimumUsableWidth)
             + " points wide. Widen the window to use it.";
}

void LuthierAudioProcessorEditor::setAdvancedMode (bool advanced)
{
    /*  4.5 again: the toggle forces Easy with an inline notice rather than
        switching to a panel whose four columns cannot fit. Saying why matters -
        a toggle that silently does nothing reads as a broken toggle, and ground
        rule 0.2 is that degradation is never silent. */
    if (advanced && ! isAdvancedModeAvailable())
    {
        inlineNotice.show (advancedUnavailableMessage(), InlineNotice::Level::warning);
        advanced = false;
    }

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

void LuthierAudioProcessorEditor::toggleWorkshop()
{
    if (advancedMode)
    {
        if (advancedPanel.isWorkshopShowing())
        {
            advancedPanel.setWorkspaceTab (juce::jmax (1, tabBeforeWorkshop));
        }
        else
        {
            tabBeforeWorkshop = advancedPanel.getWorkspaceTab();
            advancedPanel.setWorkspaceTabNamed ("WORKSHOP");
        }
        return;
    }

    if (overlayHost.getCurrentOverlay() == &workshopOverlay)
        overlayHost.dismiss();
    else
        showOverlay (&workshopOverlay);
}

void LuthierAudioProcessorEditor::showSaveGuitarDialog()
{
    auto* dialog = new juce::AlertWindow (tr ("workshop.saveGuitar.title"),
                                          tr ("workshop.saveGuitar.prompt"),
                                          juce::MessageBoxIconType::NoIcon, this);

    const auto& current = processor.getCurrentGuitar();
    dialog->addTextEditor ("name", current.name, tr ("workshop.saveGuitar.name"));

    // 6: "Bundle parts" is the sharing option, so it is a second way to save.
    dialog->addButton (tr ("workshop.saveGuitar.save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton (tr ("workshop.saveGuitar.bundle"), 2);
    dialog->addButton (tr ("common.cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<LuthierAudioProcessorEditor> (this), dialog] (int result)
        {
            const auto name = dialog->getTextEditorContents ("name").trim();
            const bool bundleParts = result == 2;

            if (safeThis == nullptr || result == 0 || name.isEmpty())
                return;

            safeThis->processor.pushUndoState ("Save As Guitar");
            const auto file = safeThis->processor.saveGuitarAs (name, bundleParts);

            if (file.existsAsFile())
                safeThis->notifications.post ({ "save-guitar", tr ("workshop.saveGuitar.saved", { { "name", name } }),
                                                Notification::Level::info });
            else
                safeThis->notifications.post ({ "save-guitar", tr ("workshop.saveGuitar.failed"),
                                                Notification::Level::warning });
        }), true);
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

    // action-and-undo.md 12: Options -> Diagnostics "Show undo depth".
    if (UiPreferences::get().getBool (UndoHistory::kShowDepthPreference, false))
        g.drawText (UndoHistory::describeDepth (processor.getNumUndoSteps(), processor.getNumRedoSteps()),
                    footer.reduced (Metrics::windowPadding, 0).withTrimmedRight (70),
                    juce::Justification::centredRight, false);
}

void LuthierAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    /*  4.5: a window dragged below the Advanced minimum forces Easy.

        The flags are set here rather than by calling setAdvancedMode, which ends
        in resized() and would re-enter this function. The layout below is the
        same pass, so the panels land in the right place without a second one. */
    const bool advancedAvailable = isAdvancedModeAvailable();

    header.setAdvancedModeAvailable (advancedAvailable);

    if (advancedMode && ! advancedAvailable)
    {
        advancedMode = false;
        processor.getUiState().advancedMode = false;

        easyPanel.setVisible (true);
        advancedPanel.setVisible (false);
        header.setAdvancedMode (false);

        inlineNotice.show (advancedUnavailableMessage(), InlineNotice::Level::warning);
    }

    header.setBounds (bounds.removeFromTop (Metrics::headerHeight));

    /*  gui-integration 15: "under the header strip", and above the live strip.
        A banner that pushed the live controls down every time one arrived would
        move the buttons under a player's hand in the middle of a set. */
    if (notifications.isVisible())
        notifications.setBounds (bounds.removeFromTop (NotificationCentre::preferredHeight)
                                   .reduced (Metrics::windowPadding, 2));

    // live-performance 10: the live strip attaches under the header when Live
    // Mode is on, and takes no space at all when it is off.
    if (liveStrip.isVisible())
        liveStrip.setBounds (bounds.removeFromTop (LiveStrip::preferredHeight));

    auto footer = bounds.removeFromBottom (Metrics::footerHeight);
    chordButton.setBounds (footer.withSizeKeepingCentre (110, Metrics::footerHeight - 2));

    // 12: the data stream runs between the CPU readout and the chords button.
    dataStream.setBounds (footer.withTrimmedLeft (230).withRight (chordButton.getX() - Metrics::grid));

    // practice-tools 9: the drawer sits above the footer.
    practicePanel.setBounds (bounds.removeFromBottom (practicePanel.preferredHeight()));

    // The notice takes space from the panel rather than floating over it, so it
    // never covers a control the user is reaching for.
    if (inlineNotice.isVisible())
        inlineNotice.setBounds (bounds.removeFromBottom (InlineNotice::preferredHeight)
                                  .reduced (Metrics::windowPadding, 2));

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

    /*  The header carries the state as well as setting it. Live Mode can be
        turned on from the shortcut as well as from the pill, and this used to
        leave the pill dark and the Advanced toggle unlocked - the mode was on and
        the header said it was off. */
    header.setLiveMode (live);

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
void LuthierAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    auto& settings = AccessibilitySettings::get();
    const auto& wanted = settings.getColours();

    Palette::apply (wanted, settings.getPalette() != PaletteId::highContrast);
    Palette::remap (*this, shownPalette, wanted);
    shownPalette = wanted;

    if (std::abs (getTransform().getScaleFactor() - (float) settings.getUiScale()) > 1.0e-3f)
        setScaleFactor ((float) settings.getUiScale());   // accessibility.md 4

    lookAndFeel.refreshColours();
    sendLookAndFeelChange();
    repaint();
}

void LuthierAudioProcessorEditor::timerCallback()
{
    updateLiveStripVisibility();
    pollForNotifications();

    // gui-integration 20: mark this version's new entry points, once the window is built.
    if (! newDotsApplied)
    {
        newDotsApplied = true;
        NewFeatureDots::apply (*this, JucePlugin_VersionString, juce::Time::getCurrentTime());
    }

    // visual-polish.md 5: "Follow the guitar" takes the accent from the finish.
    AccessibilitySettings::get().setGuitarAccentSource (
        juce::Colour::fromString ("ff" + processor.getCurrentGuitar().finish.colourA.trimCharactersAtStart ("#")));

    // practice-tools 11.2: the PRACTICE tab's START opens the drawer on the
    // routine's first tool.
    if (const int tool = processor.takePracticeDrawerRequest(); tool >= 0)
    {
        practicePanel.setOpen (true);
        practicePanel.showTool ((PracticeTool) tool);
    }

    if (const auto generation = RangeState::getGeneration(); generation != seenRangeGeneration)
    {
        seenRangeGeneration = generation;
        RangesUi::resyncControls (*this);
    }

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

    if (is ("help"))            { openHelp (getHelpContext());  return true; }
    if (is ("options"))         { showOverlay (&optionsPanel);  return true; }
    if (is ("presetBrowser"))   { showOverlay (&presetBrowser); return true; }
    if (is ("export"))          { showOverlay (&exportPanel);   return true; }
    if (is ("debugPanel"))      { showOverlay (&debugPanel);    return true; }

    /*  Section 17's "New preset": load Init, which is the factory preset whose own
        description calls it the place to start when building your own. Undoable,
        because losing an unsaved sound to a mistyped Ctrl+N would be the worst
        thing a shortcut in this window can do. */
    if (is ("newPreset"))
    {
        processor.pushUndoBoundary ("New preset");   // action-and-undo.md 5

        auto& presets = processor.getPresetManager();
        const int init = presets.indexOfPreset ("Init");

        if (init >= 0)
            presets.loadPreset (init);

        processor.getParameterBridge().applyAllNow();
        return true;
    }

    /*  Section 17's "Reveal preset file". A factory preset has a file too, so this
        works for both; a session that has not loaded one has nothing to show and
        says so rather than opening the wrong folder. */
    if (is ("revealPreset"))
    {
        const auto file = processor.getPresetManager().getCurrentPresetFile();

        if (file.existsAsFile())
            file.revealToUser();
        else
            notifications.post ({ "reveal-preset",
                                  "This sound has not been saved yet, so there is no file to show.",
                                  Notification::Level::info });

        return true;
    }

    if (is ("saveGuitarAs"))
    {
        showSaveGuitarDialog();
        return true;
    }

    if (is ("revealGuitar"))
    {
        // A factory guitar is a file too; an edited one is not until it is saved.
        const auto file = processor.getGuitarFile();

        if (file.existsAsFile() && ! processor.isGuitarEdited())
            file.revealToUser();
        else
            notifications.post ({ "reveal-guitar", tr ("workshop.revealGuitar.none"),
                                  Notification::Level::info });

        return true;
    }

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

    /*  Section 17's Column 4 tab steps, which were blocked until the workspace
        had tabs to step. They are answered only in Advanced Mode: in Easy there
        is no column 4, and swallowing the key there would make Ctrl+] look
        broken rather than inapplicable. */
    if (is ("previousWorkspaceTab") || is ("nextWorkspaceTab"))
    {
        if (! advancedMode)
            return false;

        advancedPanel.stepWorkspaceTab (is ("nextWorkspaceTab") ? 1 : -1);
        return true;
    }

    // gui-integration 17: W toggles the Workshop - the WORKSHOP tab in
    // Advanced, the bench overlay in Easy (VISUAL-WORKSHOP-QA).
    if (is ("toggleWorkshop"))
    {
        toggleWorkshop();
        return true;
    }

    if (is ("toggleSlideMode"))
    {
        HeaderBar::toggleSlideMode (processor);
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

        processor.stepPresetAsUserAction (forward);   // action-and-undo.md 3.8
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

    if (is ("undo"))
    {
        // action-and-undo.md 5: plain undo stops at a boundary and says how to cross it.
        if (processor.isUndoStoppedAtBoundary())
            notifications.post ({ "undo-boundary",
                                  "Undo stopped at a preset or guitar load. Ctrl+Alt+Z goes further back.",
                                  Notification::Level::info });
        else
            processor.undo();

        return true;
    }

    if (is ("undoAcrossBoundary"))
    {
        // action-and-undo.md 9: crossing a boundary asks first, in a banner.
        if (! processor.isUndoStoppedAtBoundary())
        {
            processor.undo();
            return true;
        }

        Notification n;
        n.id = "undo-boundary";
        n.message = "Undo past the load, back to \"" + processor.getUndoHistory (1)[0].description + "\"?";
        n.level = Notification::Level::warning;
        n.actionText = "Undo";
        n.action = [safeThis = juce::Component::SafePointer<LuthierAudioProcessorEditor> (this)]
        {
            if (safeThis != nullptr)
                safeThis->processor.undoAcrossBoundary();
        };
        notifications.post (n);
        return true;
    }

    if (is ("redo") || is ("redoAlt")) { processor.redo(); return true; }   // action-and-undo.md 9

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
        // action-and-undo.md 0.1: resetEverything pushes its own entry.
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


//==============================================================================
bool LuthierAudioProcessorEditor::showOptionsPage (const juce::String& tabName)
{
    if (! optionsPanel.showPageNamed (tabName))
        return false;

    showOverlay (&optionsPanel);
    return true;
}

//==============================================================================
/*  gui-integration 15's triggers, as far as the build can raise them.

    Section 15 lists nine. Four of them are things the plugin discovers about
    itself with no network and no user action, and those are checked here, once,
    when the window opens:

      - Crash on last session       Telemetry::hasPendingCrashReport
      - License grace countdown     License::State::grace
      - Managed by policy           Telemetry::isManagedByPolicy
      - Update available            only when the user has opted in, below

    The other five are not skipped, they are unreachable. "Preset load error",
    "missing IR", "missing guitar" and "missing part" need the loader to report
    what it fell back to, and it currently swallows that. "Advanced-range clamped
    on save" needs advanced ranges, which advanced-ranges.md has not specified.
    "Sample-rate change" is the one that is merely awkward: prepareToPlay knows,
    but it runs on the audio thread and the editor may not exist at the time, so
    it needs somewhere to leave the message. GAPS.md has all five.

    Ordered deliberately. A crash report is the only one of the four with
    something to do about it, so it is posted last and therefore shown first -
    the queue is first-in-first-out and the actionable banner is the one that
    should not be behind a countdown the user has to clear.
*/
void LuthierAudioProcessorEditor::postStartupNotifications()
{
    auto& telemetry = processor.getTelemetry();
    auto& license = processor.getLicense();

    // ---- managed by policy ---------------------------------------------------
    if (telemetry.isManagedByPolicy())
    {
        Notification n;
        n.id = "policy";
        n.message = "Some settings are managed by your organisation's policy file.";
        n.level = Notification::Level::info;

        notifications.post (std::move (n));
    }

    // ---- licence grace period ------------------------------------------------
    if (license.getState() == License::State::grace)
    {
        const int days = license.getDaysUntilRevalidation();

        Notification n;
        n.id = "licence-grace";
        n.level = Notification::Level::warning;

        /*  The wording changes at one day because "1 days" is the kind of thing
            that makes a user distrust everything else the plugin tells them, and
            at zero because a countdown that reads "0 days left" is worse than
            saying what actually happens next. */
        n.message = days <= 0
                      ? "Licence revalidation is due. Luthier keeps working; connect to "
                        "revalidate."
                      : "Licence revalidates in " + juce::String (days)
                          + (days == 1 ? " day." : " days.");

        notifications.post (std::move (n));
    }

    // ---- an update, if the user asked us to look ------------------------------
    /*  updates-telemetry 1: the check is opt-in and off by default, and a policy
        can switch it off but never on. Checking here without that opt-in would
        make opening the window a network call the user declined.

        checkForUpdate is throttled to once every 24 hours, so this is at most one
        request a day however many times the window is opened. It blocks on the
        network, which is why it goes to a background thread and posts back.
    */
    if (telemetry.isUpdateCheckEnabled())
    {
        const auto running = Version::parse (JucePlugin_VersionString);

        /*  Same shape as UpdatesPage::checkForUpdate - launch, then back to the
            message thread to touch the UI. Deliberately the same: a second
            threading idiom for the same job, in the same file set, would be a
            worse thing to maintain than the one this codebase already uses.

            Unthrottled here, unlike the Options page's button, which forces. If
            the 24-hour window has not elapsed this returns without a request. */
        juce::Thread::launch ([this, running]
        {
            const auto result = processor.getTelemetry().checkForUpdate (running);

            if (! result.updateAvailable)
                return;

            juce::MessageManager::callAsync ([this, result]
            {
                Notification n;
                n.id = "update";
                n.message = "Luthier " + result.available.toString() + " is available.";
                n.level = Notification::Level::info;
                n.actionText = "Details";
                n.action = [this] { showOptionsPage ("UPDATES"); };

                notifications.post (std::move (n));
            });
        });
    }

    // ---- a crash dump from last time -----------------------------------------
    if (telemetry.hasPendingCrashReport())
    {
        Notification n;
        n.id = "crash";
        n.message = "Luthier did not shut down cleanly last time.";
        n.level = Notification::Level::warning;
        n.actionText = "Review";

        /*  Review rather than Send. updates-telemetry 4 asks for a diff viewer
            showing exactly what would be uploaded, and the Privacy page is where
            that lives - so the button opens it rather than uploading on one
            click. A crash dump is the most sensitive thing this plugin ever
            offers to transmit, and a single button that sent it would be the
            opt-in equivalent of a dark pattern. */
        n.action = [this] { showOptionsPage ("PRIVACY"); };

        notifications.post (std::move (n));
    }
}


//==============================================================================
/*  gui-integration 15's two triggers that can arrive at any moment.

    Both are conditions rather than events - a string that is set or is not - so
    this reads them on the timer and posts when the message *changes*. Posting on
    every tick would be harmless to the queue, because a repeated id replaces
    itself rather than stacking, but it would make the banner undismissable: the
    user clicks the cross and it is back a quarter of a second later.

    The IR case was anticipated by the code it reads. IrSlot::fromVar falls back
    to the built-in model when a preset names an IR that is no longer on disk, and
    leaves lastError set with a comment saying it is there "so the header can show
    the banner the spec asks for". The banner did not exist at the time. It does
    now, and this is the thing that reads it.
*/
void LuthierAudioProcessorEditor::pollForNotifications()
{
    /*  ---- the host moved the sample rate --------------------------------------

        Claimed rather than compared, because the claim is what clears it: this is
        an event that happened once, not a condition that is still true, so the
        "post only when the message changes" rule the other two use does not
        apply. Asking twice must not say it twice. */
    if (const double changedTo = processor.claimSampleRateChange(); changedTo > 0.0)
    {
        Notification n;
        n.id = "sample-rate";
        n.level = Notification::Level::info;

        /*  Section 15's own wording, and the second half of it is the part that
            matters: the user needs to know the IRs and filters were rebuilt, not
            merely that a number changed, because that is what explains the gap in
            the audio they just heard. */
        /*  One decimal, with a trailing ".0" dropped: 44.1 and 88.2 need it, 48
            and 96 read wrong with it. */
        auto khz = juce::String (changedTo / 1000.0, 1);

        if (khz.endsWith (".0"))
            khz = khz.dropLastCharacters (2);

        n.message = "Sample rate changed to " + khz
                      + " kHz. IRs and circuit filters re-resampled.";

        notifications.post (std::move (n));
    }

    // ---- CPU limit (performance-budget.md 8, relief 7) ------------------------
    {
        const bool due = processor.getEngine().getCpuRelief().isCpuLimitBannerDue();

        switch (CpuReliefUi::bannerAction (due, cpuLimitEpisode))
        {
            case CpuReliefUi::BannerAction::post:
            {
                Notification n;
                n.id = CpuReliefUi::kBannerId;
                n.message = CpuReliefUi::bannerMessage();
                n.level = Notification::Level::warning;
                notifications.post (std::move (n));
                break;
            }

            case CpuReliefUi::BannerAction::withdraw:
                if (notifications.getCurrentId() == CpuReliefUi::kBannerId)
                    notifications.dismissCurrent();
                break;

            case CpuReliefUi::BannerAction::none:
                break;
        }

        cpuLimitEpisode = due;
    }

    // ---- a part a guitar asked for and could not have (gui-integration 15) -----
    for (const auto& message : processor.takeGuitarNotices())
    {
        Notification n;
        n.id = "missing-part";
        n.message = message;
        n.level = Notification::Level::warning;
        notifications.post (std::move (n));
    }

    // ---- a preset that would not load ----------------------------------------
    const auto presetError = processor.getPresetManager().getLastLoadError();

    if (presetError != reportedPresetError)
    {
        reportedPresetError = presetError;

        if (presetError.isNotEmpty())
        {
            Notification n;
            n.id = "preset-load";
            n.message = presetError;
            n.level = Notification::Level::warning;

            notifications.post (std::move (n));
        }
    }

    // ---- installer.md 8: a load that migrated an old file ----------------------
    /*  "A subtle info banner on the first affected load": one per window, not
        one per migrated preset, however many old presets are browsed. */
    if (const auto generation = processor.getPresetManager().getMigrationGeneration();
        generation != seenMigrationGeneration)
    {
        seenMigrationGeneration = generation;

        if (! migrationBannerShown)
        {
            migrationBannerShown = true;

            Notification n;
            n.id = "migrated";
            n.message = "This preset was made with an older version of Luthier and has been "
                        "updated (" + processor.getPresetManager().getLastMigration()
                        + "). Saving it keeps the original in Presets/Backup.";
            n.level = Notification::Level::info;
            notifications.post (std::move (n));
        }
    }

    // ---- an IR the preset asked for and could not have ------------------------
    /*  Three slots, one banner. A preset that names three missing IRs has one
        thing wrong with it - the folder moved - and three banners saying so in
        turn would be three dismissals for one problem. The first slot with
        something to say speaks for all of them; the error log has the detail. */
    juce::String irError = processor.getBodyIrSlot().getLastError();

    for (int slot = 0; slot < 2 && irError.isEmpty(); ++slot)
        irError = processor.getCabIrSlot (slot).getLastError();

    if (irError != reportedIrError)
    {
        reportedIrError = irError;

        if (irError.isNotEmpty())
        {
            Notification n;
            n.id = "ir-missing";
            n.message = irError;
            n.level = Notification::Level::warning;
            n.actionText = "Tone Match";

            /*  Somewhere to fix it. The IR slots live on the TONE MATCH tab of
                column 4, which only exists in Advanced Mode - so this switches
                mode on the way, and does nothing at all if the window is too
                narrow for Advanced, which setAdvancedMode already refuses with
                its own notice rather than laying out columns that do not fit. */
            n.action = [this]
            {
                setAdvancedMode (true);
                header.setAdvancedMode (advancedMode);

                if (advancedMode)
                    advancedPanel.setWorkspaceTabNamed ("TONE MATCH");
            };

            notifications.post (std::move (n));
        }
    }
}

//==============================================================================
juce::String LuthierAudioProcessorEditor::getHelpContext() const
{
    return advancedMode ? advancedPanel.getHelpContextFor (juce::Component::getCurrentlyFocusedComponent())
                        : juce::String();
}

void LuthierAudioProcessorEditor::openHelp (const juce::String& topic)
{
    // Advanced: the HELP tab, with any overlay out of the way (it would sit on
    // top of the tab). Easy: the same help as an overlay.
    if (advancedMode)
    {
        if (overlayHost.isShowingOverlay())
            overlayHost.dismiss();

        advancedPanel.showHelp (topic);
        return;
    }

    helpPanel.showTopicFor (topic);
    showOverlay (&helpPanel);
}

} // namespace luthier
