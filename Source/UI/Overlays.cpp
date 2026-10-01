#include "NormalizationOptions.h"   // output-normalization.md 5.4
#include "Overlays.h"
#include "OptionsPages.h"
#include "../Presets/TechniquePresets.h"   // TECHNIQUES
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"   // SPEC-SWEEP: A11Y-10
#include "../Support/SupportLinks.h"
#include "QualityOptions.h"   // cpu-quality-modes

namespace luthier
{

//==============================================================================
//  OverlayPanel
//==============================================================================
OverlayPanel::OverlayPanel (const juce::String& t)
    : title (t)
{
    addAndMakeVisible (closeButton);
    closeButton.setTooltip ("Close (Escape)");
    closeButton.onClick = [this] { if (onDismiss) onDismiss(); };

    setWantsKeyboardFocus (true);

    // Clicks must not fall through to the scrim behind, or clicking inside the
    // panel would dismiss it.
    setInterceptsMouseClicks (true, true);
}

OverlayPanel::~OverlayPanel() = default;

juce::Rectangle<int> OverlayPanel::getContentBounds() const
{
    return getLocalBounds().withTrimmedTop (titleBarHeight).reduced (Metrics::windowPadding);
}

void OverlayPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::DropShadow (juce::Colour (0xcc000000), 24, { 0, 6 }).drawForRectangle (g, getLocalBounds());

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, Metrics::windowCorner);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), Metrics::windowCorner, 1.0f);

    auto titleBar = getLocalBounds().removeFromTop (titleBarHeight);

    LuthierLookAndFeel::drawSectionHeader (g, titleBar.reduced (Metrics::windowPadding, 0), title);
    LuthierLookAndFeel::drawSeparator (g, { Metrics::windowPadding, titleBarHeight - 1,
                                            getWidth() - Metrics::windowPadding * 2, 1 });
}

void OverlayPanel::resized()
{
    closeButton.setBounds (getWidth() - Metrics::windowPadding - 72,
                           (titleBarHeight - Metrics::buttonHeight) / 2, 72, Metrics::buttonHeight);

    layoutContent (getContentBounds());
}

bool OverlayPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onDismiss)
            onDismiss();

        return true;
    }

    return false;
}

//==============================================================================
//  OverlayHost
//==============================================================================
OverlayHost::OverlayHost()
{
    setInterceptsMouseClicks (true, true);
    setVisible (false);
}

OverlayHost::~OverlayHost() = default;

void OverlayHost::show (OverlayPanel* panel)
{
    if (panel == nullptr)
        return;

    // Only one overlay at a time, structurally.
    if (current != nullptr && current != panel)
        dismiss();

    // SPEC-SWEEP: A11Y-11 - remember who opened it, unless that is inside an
    // overlay itself (one overlay replacing another keeps the first launcher).
    if (current == nullptr)
    {
        auto* focused = juce::Component::getCurrentlyFocusedComponent();
        launcher = (focused != nullptr && ! isParentOf (focused)) ? focused : nullptr;
    }

    current = panel;
    current->onDismiss = [this] { dismiss(); };

    // JUCE tells a component its look and feel on a change, not on joining a
    // parent: a panel first shown here would keep JUCE's default slider value
    // boxes (white text, unreadable on the Light palette).
    const bool joining = current->getParentComponent() != this;
    addAndMakeVisible (current);

    if (joining)
        current->sendLookAndFeelChange();

    setVisible (true);
    toFront (false);
    resized();

    current->overlayShown();
    current->grabKeyboardFocus();

    // SPEC-SWEEP: A11Y-10 - announce it and put focus on its first control, so
    // tabbing starts inside the dialog. Escape still reaches the panel: an
    // unhandled key travels up to it.
    AccessibleSetup::announceOverlayOpened (*current, current->getName().isNotEmpty() ? current->getName()
                                                                                    : juce::String ("Dialog"));
}

void OverlayHost::dismiss()
{
    if (current == nullptr)
    {
        setVisible (false);
        return;
    }

    auto* panel = current;
    current = nullptr;

    panel->overlayHidden();
    removeChildComponent (panel);

    setVisible (false);

    // SPEC-SWEEP: A11Y-11 - focus goes back where it came from.
    if (auto* back = launcher.getComponent(); back != nullptr && back->isShowing() && back->getWantsKeyboardFocus())
        back->grabKeyboardFocus();
    else if (auto* parent = getParentComponent())
        parent->grabKeyboardFocus();

    launcher = nullptr;
}

void OverlayHost::paint (juce::Graphics& g)
{
    // The scrim: dark enough to focus attention, light enough that the plugin is
    // still visibly there behind it.
    g.fillAll (juce::Colour (0xb0000000));
}

void OverlayHost::resized()
{
    if (current == nullptr)
        return;

    const auto preferred = current->getPreferredSize();

    const int w = juce::jmin (preferred.x, getWidth() - Metrics::windowPadding * 2);
    const int h = juce::jmin (preferred.y, getHeight() - Metrics::windowPadding * 2);

    current->setBounds (juce::Rectangle<int> (w, h).withCentre (getLocalBounds().getCentre()));
}

void OverlayHost::mouseDown (const juce::MouseEvent& e)
{
    // A click on the scrim, outside the panel, dismisses.
    if (current == nullptr || ! current->getBounds().contains (e.getPosition()))
        dismiss();
}

//==============================================================================
//  HelpPanel
//==============================================================================
/*  The Easy-mode overlay is the same HelpTab the Advanced window shows in its
    column-4 HELP tab (gui-integration 4.4), so the two can never disagree. */
HelpPanel::HelpPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Help"), view (p)
{
    addAndMakeVisible (view);
}

//==============================================================================
//  DebugPanel
//==============================================================================
DebugPanel::DebugPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Debug Tools"), processor (p)
{
    addAndMakeVisible (explanation);
    explanation.setFont (Fonts::ui (11.5f));
    explanation.setColour (juce::Label::textColourId, Palette::textMuted);
    explanation.setJustificationType (juce::Justification::topLeft);
    explanation.setText (
        "Two different files, for two different jobs.\n\n"
        "The TROUBLESHOOTING FILE is a one-off snapshot: your settings, your audio and MIDI "
        "configuration, your host, the version, and a short self-test. It is what to send "
        "first for anything that is merely not working as expected. It is safe to share and "
        "contains no audio.\n\n"
        "The CRASH LOG is only useful if Luthier is actually crashing. Tick the box below, "
        "reproduce the crash, and a timestamped log is written with a copy of the "
        "troubleshooting report at the top. It is off every time the plugin loads, on purpose.\n\n"
        "If Luthier is hard-crashing, send BOTH files to support with a description of what "
        "you were doing. Both are written to Documents/Luthier/Diagnostics.",
        juce::dontSendNotification);

    addAndMakeVisible (crashLogToggle);
    crashLogToggle.setTooltip ("Off on every load. Turning it on also turns on the live data stream.");
    crashLogToggle.onClick = [this]
    {
        processor.getDiagnostics().setCrashLogEnabled (crashLogToggle.getToggleState());
        refreshState();
    };

    addAndMakeVisible (troubleshootButton);
    troubleshootButton.setTooltip ("Write a troubleshooting file you can send to support");
    troubleshootButton.onClick = [this]
    {
        processor.getPresetManager().captureExtraState();

        const auto file = processor.getDiagnostics().writeTroubleshootingReport (
            juce::JSON::toString (processor.getPresetManager().toVar(), false),
            processor.getEngine().getValidator().getSummary());

        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions()
                .withIconType (file != juce::File() ? juce::MessageBoxIconType::InfoIcon
                                                    : juce::MessageBoxIconType::WarningIcon)
                .withTitle (file != juce::File() ? "Troubleshooting file written" : "Could not write the file")
                .withMessage (file != juce::File()
                                ? "Written to\n" + file.getFullPathName()
                                  + "\n\nSend this to " + juce::String (SupportLinks::supportEmail)
                                  + " with a description of the problem."   // SupportLinks.h (TUNE-HELP-ONBOARDING)
                                : "The diagnostics folder could not be written to. Check the folder "
                                  "permissions for Documents/Luthier.")
                .withButton ("OK"),
            nullptr);
    };

    addAndMakeVisible (openFolderButton);
    openFolderButton.onClick = [] { Diagnostics::getDiagnosticsFolder().revealToUser(); };

    addAndMakeVisible (hardResetButton);
    hardResetButton.setColour (juce::TextButton::textColourOffId, Palette::clip);
    hardResetButton.setTooltip ("Destructive: resets every setting, clears the MIDI map, deletes "
                                "diagnostic files and cached data, and reinstalls the factory bank.");
    hardResetButton.onClick = [this]
    {
        // Taken here: MSVC resolves `this` inside a nested lambda's init-capture
        // to the enclosing lambda, not the component.
        juce::Component::SafePointer<DebugPanel> self (this);

        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle ("Reset everything?")
                .withMessage ("This resets every setting, clears your MIDI mappings, deletes "
                              "diagnostic files and cached data, and reinstalls the factory "
                              "preset bank.\n\nYour own saved presets are NOT deleted.\n\n"
                              "This cannot be undone.")
                .withButton ("Reset everything")
                .withButton ("Cancel"),
            [safe = self] (int result)
            {
                // NativeMessageBox::showAsync reports the plain button index:
                // 0 is "Reset everything", 1 is Cancel (and Escape).
                if (safe != nullptr && result == 0)
                {
                    safe->processor.hardResetAndClearCaches();
                    safe->refreshState();
                }
            });
    };

    addAndMakeVisible (clearButton);
    clearButton.onClick = [this]
    {
        processor.getDiagnostics().reset();
        streamView.clear();
        lastStreamCount = 0;
    };

    auto setupView = [this] (juce::TextEditor& view)
    {
        addAndMakeVisible (view);
        view.setMultiLine (true);
        view.setReadOnly (true);
        view.setScrollbarsShown (true);
        view.setCaretVisible (false);
        view.setFont (Fonts::mono (10.5f));
        view.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    };

    setupView (stateView);
    setupView (streamView);
}

DebugPanel::~DebugPanel()
{
    motion.stopTimer();
}

void DebugPanel::overlayShown()
{
    processor.getDiagnostics().setEnabled (true);
    crashLogToggle.setToggleState (processor.getDiagnostics().isCrashLogEnabled(),
                                   juce::dontSendNotification);
    refreshState();
    motion.startTimerHz (*this, 8);
}

void DebugPanel::overlayHidden()
{
    motion.stopTimer();
}

void DebugPanel::refreshState()
{
    auto& engine = processor.getEngine();
    const auto& validator = engine.getValidator();

    juce::String text;

    text << "ENGINE\n"
         << "  Sample rate        " << juce::String (engine.getSampleRate(), 1) << " Hz\n"
         << "  Reported latency   " << processor.getLatencySamples() << " samples\n"
         << "  CPU (this plugin)  " << juce::String (engine.getCpuEstimate(), 1) << " %\n"
         << "  Oversampling       " << engine.getOversamplingFactor() << "x\n"
         << "  Host tempo         " << juce::String (processor.getHostTempo(), 1) << " BPM\n"
         << QualityDiagnostics::describe (processor)   // cpu-quality-modes 5
         << "\nINSTRUMENT\n"
         << "  Guitar             " << engine.getGuitarSpec().name << "\n"
         << "  Strings            " << engine.getNumStrings() << "\n"
         << "  Scale length       " << juce::String (engine.getGuitarSpec().scaleLengthMm, 1) << " mm\n"
         << "  Fretless           " << (engine.isFretless() ? "yes" : "no") << "\n"
         << "  E-Bow              " << (engine.isEBowing() ? "yes" : "no") << "\n"
         << "  Freeze             " << (engine.getFreezeOverlay().isHolding() ? "holding"
                                          : engine.getFreezeOverlay().isEnabled() ? "capturing"
                                                                                  : "off") << "\n"
         << "\nNORMALIZATION\n";

    // output-normalization.md 5.4: normalization gain and true-peak GR.
    for (const auto& line : NormalizationUi::diagnosticsLines (processor))
        text << "  " << line << "\n";

    text << "\nSTRINGS\n";

    for (int s = 0; s < engine.getNumStrings(); ++s)
    {
        const auto& spec = engine.getStringSpec (s);

        text << "  " << juce::String (s + 1).paddedLeft (' ', 2) << "  "
             << TuningEngine::describeFrequency (engine.getStringFrequency (s)).paddedRight (' ', 12)
             << "fret " << juce::String (engine.getStringFret (s), 2).paddedLeft (' ', 6) << "   "
             << "level " << juce::String (engine.getStringLevel (s), 5).paddedLeft (' ', 8) << "   "
             << "tension " << juce::String (spec.tensionNewtons, 1).paddedLeft (' ', 6) << " N"
             << (StringMaterials::isTensionPlayable (spec.tensionNewtons) ? "" : "  <-- out of range")
             << "\n";
    }

    text << "\nSIGNAL\n"
         << "  Peak               " << juce::String (engine.getMasterBus().getPeakDb(), 2) << " dBFS\n"
         << "  LUFS (short)       " << juce::String (engine.getMasterBus().getLufs(), 1) << "\n"
         << "  Limiter reduction  " << juce::String (engine.getMasterBus().getGainReductionDb(), 2) << " dB\n"
         << "  Body mode          " << (int) engine.getBodyEngine().getMode()
         << "  (" << engine.getBodyEngine().getNumModes() << " modes)\n"
         << "  Pickups silent     " << (engine.getPickupEngine().isSilent() ? "YES" : "no") << "\n"
         << "\nVALIDATOR\n  " << validator.getSummary() << "\n";

    ValidationRecord records[12];
    const int count = validator.getRecentRecords (records, 12);

    for (int i = 0; i < count; ++i)
    {
        text << "  " << juce::String (getValidationCheckName (records[i].check)).paddedRight (' ', 20)
             << " string " << records[i].stringIndex
             << "  " << juce::String (records[i].offendingValue, 4)
             << " -> " << juce::String (records[i].correctedValue, 4)
             << (records[i].rejected ? "   (rejected)" : "") << "\n";
    }

    const auto selfTest = Diagnostics::runSelfTest();

    text << "\nSELF TEST\n";

    for (const auto& line : selfTest.passed)   text << "  [ok]      " << line << "\n";
    for (const auto& line : selfTest.warnings) text << "  [warning] " << line << "\n";
    for (const auto& line : selfTest.failed)   text << "  [FAILED]  " << line << "\n";

    const auto caret = stateView.getCaretPosition();
    stateView.setText (text, false);
    stateView.setCaretPosition (caret);
}

void DebugPanel::timerCallback()
{
    refreshState();

    auto& diagnostics = processor.getDiagnostics();
    const int total = diagnostics.getTotalRecords();

    if (total == lastStreamCount)
        return;

    const int newRecords = juce::jmin (total - lastStreamCount, Diagnostics::kRingSize);
    lastStreamCount = total;

    std::vector<Diagnostics::Record> records ((size_t) juce::jmax (1, newRecords));
    const int count = diagnostics.getRecords (records.data(), newRecords);

    juce::String appended;

    for (int i = 0; i < count; ++i)
        appended << Diagnostics::formatRecord (records[(size_t) i], processor.getEngine().getSampleRate()) << "\n";

    streamView.moveCaretToEnd (false);
    streamView.insertTextAtCaret (appended);

    // Keep the view bounded, or a long session eats memory in the UI.
    if (streamView.getTotalNumChars() > 200000)
        streamView.setText (streamView.getText().fromLastOccurrenceOf ("\n", false, false)
                                                .paddedLeft (' ', 0), false);
}

void DebugPanel::layoutContent (juce::Rectangle<int> content)
{
    auto left = content.removeFromLeft (content.getWidth() / 2);
    content.removeFromLeft (Metrics::grid);

    auto buttons = left.removeFromBottom (Metrics::buttonHeight * 2 + Metrics::gridHalf);

    auto topRow = buttons.removeFromTop (Metrics::buttonHeight);
    troubleshootButton.setBounds (topRow.removeFromLeft (200));
    topRow.removeFromLeft (Metrics::gridHalf);
    openFolderButton.setBounds (topRow.removeFromLeft (170));

    buttons.removeFromTop (Metrics::gridHalf);
    hardResetButton.setBounds (buttons.removeFromLeft (300));

    left.removeFromBottom (Metrics::grid);
    crashLogToggle.setBounds (left.removeFromBottom (Metrics::buttonHeight));
    left.removeFromBottom (Metrics::gridHalf);

    explanation.setBounds (left.removeFromBottom (juce::jmin (190, left.getHeight() / 2)));
    left.removeFromBottom (Metrics::grid);

    stateView.setBounds (left);

    auto streamHeader = content.removeFromBottom (Metrics::buttonHeight);
    clearButton.setBounds (streamHeader.removeFromLeft (120));

    content.removeFromBottom (Metrics::gridHalf);
    streamView.setBounds (content);
}

//==============================================================================
//  OptionsPanel
//==============================================================================
OptionsPanel::OptionsPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Options"), processor (p)
{
    /*  gui-integration.md section 5's tab list, all eleven, in its order.

        CONTROLLERS used to sit in the slot RANGES now takes, because section 19
        puts controller setup in the Advanced column 4 tab strip and that strip
        did not exist. It does now, and the page has moved there - see
        AdvancedPanel::buildWorkspace. Every tab below is one section 5 names,
        in the order it names them. */
    auto add = [this] (const juce::String& name, OptionsPage* page)
    {
        pages.add (page);
        addChildComponent (*page);

        auto* button = pageButtons.add (new juce::TextButton (name));

        button->setClickingTogglesState (true);
        button->setRadioGroupId (0x20);

        const int index = pageButtons.size() - 1;
        button->onClick = [this, index] { showPage (index); };

        addAndMakeVisible (*button);
    };

    add ("AUDIO",          new AudioPage (processor));
    add ("MIDI",           new MidiPage (processor));
    add ("APPEARANCE",     new AppearancePage (processor));
    add ("ACCESSIBILITY",  new AccessibilityPage (processor));
    add ("LOCALIZATION",   new LocalizationPage (processor));
    add ("EXPRESSION",     new ExpressionPage (processor));
    add ("RANGES",         new RangesPage (processor));
    add ("UPDATES",        new UpdatesPage (processor));
    add ("PRIVACY",        new PrivacyPage (processor));
    add ("DIAGNOSTICS",    new DiagnosticsPage (processor));
    add ("FILE LOCATIONS", new FileLocationsPage (processor));

    for (auto* page : pages)
        if (auto* diagnostics = dynamic_cast<DiagnosticsPage*> (page))
            diagnostics->onShowDebugWindow = [this]
            {
                if (onShowDebugWindow != nullptr)
                    onShowDebugWindow();
            };

    showPage (0);
}

//==============================================================================
//  MidiLearnArmLayer
//==============================================================================
MidiLearnArmLayer::MidiLearnArmLayer()
{
    setAlwaysOnTop (true);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setInterceptsMouseClicks (true, false);
}

void MidiLearnArmLayer::paint (juce::Graphics& g)
{
    /*  Barely there on purpose.

        The user has to be able to see and aim at the control they are about to
        assign, so this cannot be a scrim. It is just enough tint to say the
        plugin is in a mode, plus a border, which is the same language the kill
        switch and record arm use elsewhere. */
    g.fillAll (Palette::accent.withAlpha (0.06f));

    g.setColour (Palette::accent.withAlpha (0.65f));
    g.drawRect (getLocalBounds(), 2);

    auto banner = getLocalBounds().removeFromTop (Metrics::buttonHeight + Metrics::grid)
                                  .reduced (Metrics::grid, Metrics::gridHalf);

    g.setColour (Palette::panel.withAlpha (0.92f));
    g.fillRoundedRectangle (banner.toFloat(), Metrics::controlCorner);

    g.setColour (Palette::accent);
    g.setFont (Fonts::ui (12.0f));
    g.drawText ("MIDI Learn armed - click a control to assign it, or press Escape",
                banner, juce::Justification::centred, true);
}

void MidiLearnArmLayer::mouseDown (const juce::MouseEvent& e)
{
    juce::String parameterId;

    /*  Hide to hit-test, because getComponentAt would otherwise find this layer.
        The visibility flicker never reaches the screen: nothing repaints between
        here and the setVisible below. */
    setVisible (false);

    if (auto* parent = getParentComponent())
    {
        auto* hit = parent->getComponentAt (e.getEventRelativeTo (parent).getPosition());

        // A control's clickable part is usually a child - a Slider inside a knob -
        // so walk up until something says what parameter it edits.
        for (auto* c = hit; c != nullptr && parameterId.isEmpty(); c = c->getParentComponent())
            if (auto* target = dynamic_cast<LearnTarget*> (c))
                parameterId = target->getLearnParameterId();
    }

    setVisible (true);

    if (onTargetPicked != nullptr)
        onTargetPicked (parameterId);
}

void OptionsPanel::showShortcutTable (const juce::String& filter)
{
    // The shortcut table lives on the Accessibility page. Found by name rather
    // than by a hard-coded index, so that reordering the tabs cannot silently
    // point this at the wrong page.
    for (int i = 0; i < pageButtons.size(); ++i)
        if (pageButtons[i]->getButtonText().containsIgnoreCase ("ACCESSIBILITY"))
        {
            showPage (i);

            // gui-integration 16 item 13: straight to the control's row.
            for (auto* child : getChildren())
                if (auto* page = dynamic_cast<AccessibilityPage*> (child))
                    page->filterShortcuts (filter);

            return;
        }
}

juce::StringArray OptionsPanel::getPageNames() const
{
    juce::StringArray names;

    for (const auto* button : pageButtons)
        names.add (button->getButtonText());

    return names;
}

bool OptionsPanel::showPageNamed (const juce::String& tabName)
{
    for (int i = 0; i < pageButtons.size(); ++i)
    {
        if (pageButtons[i]->getButtonText().equalsIgnoreCase (tabName))
        {
            showPage (i);
            return true;
        }
    }

    return false;
}

void OptionsPanel::showPage (int index)
{
    if (pages.isEmpty())
        return;

    currentPage = juce::jlimit (0, pages.size() - 1, index);

    for (int i = 0; i < pages.size(); ++i)
        pages[i]->setVisible (i == currentPage);

    for (int i = 0; i < pageButtons.size(); ++i)
        pageButtons[i]->setToggleState (i == currentPage, juce::dontSendNotification);

    pages[currentPage]->refresh();

    resized();
    repaint();
}

void OptionsPanel::overlayShown()
{
    // A page can be stale by the time the overlay comes back: a controller may
    // have been unplugged, or telemetry consent changed from the header.
    if (! pages.isEmpty())
        pages[currentPage]->refresh();
}

void OptionsPanel::layoutContent (juce::Rectangle<int> content)
{
    /*  The tab strip wraps.

        Section 5 asks for eleven tabs, ten of which are built, and ten across
        820 points gives each one 74 points - not enough for "ACCESSIBILITY" or
        "FILE LOCATIONS", which would both come out as an ellipsis. So the strip
        takes as many rows as it needs to keep every tab wide enough to read, and
        the page gets what is left. The arithmetic is done against the buttons
        that exist rather than against the spec's count, so RANGES arriving does
        not need this rewritten. */
    if (! pageButtons.isEmpty())
    {
        const int gap = Metrics::gridHalf;
        const int minimumTabWidth = 112;

        const int perRow = juce::jlimit (1, pageButtons.size(),
                                         (content.getWidth() + gap) / (minimumTabWidth + gap));

        const int rows = (pageButtons.size() + perRow - 1) / perRow;

        // Spread them evenly rather than filling the first row and leaving the
        // last one short.
        const int perRowBalanced = (pageButtons.size() + rows - 1) / rows;

        for (int row = 0, done = 0; row < rows; ++row)
        {
            auto strip = content.removeFromTop (Metrics::buttonHeight);
            content.removeFromTop (gap);

            const int count = juce::jmin (perRowBalanced, pageButtons.size() - done);

            if (count <= 0)
                break;

            const int width = (strip.getWidth() - gap * (count - 1)) / count;

            for (int i = 0; i < count; ++i)
            {
                pageButtons[done + i]->setBounds (strip.removeFromLeft (width));
                strip.removeFromLeft (gap);
            }

            done += count;
        }
    }

    content.removeFromTop (Metrics::gridHalf);

    if (! pages.isEmpty())
        pages[currentPage]->setBounds (content);
}

//==============================================================================
//  ExportPanel
//==============================================================================
ExportPanel::ExportPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Export Audio"), processor (p)
{
    destinationFolder = PresetManager::getRenderFolder();

    auto addLabelled = [this] (juce::Component& c, const juce::String& tooltip)
    {
        addAndMakeVisible (c);

        if (auto* combo = dynamic_cast<juce::ComboBox*> (&c))
            combo->setTooltip (tooltip);
    };

    addLabelled (sourceBox, "What to render");
    sourceBox.addItem ("Audition phrase", 1);
    sourceBox.addItem ("MIDI file...", 2);
    sourceBox.setSelectedId (1);
    sourceBox.onChange = [this]
    {
        if (sourceBox.getSelectedId() != 2)
        {
            updateEstimate();
            return;
        }

        // Owned by the panel, so closing it cancels the dialog (chooser lifetime).
        fileChooser.launch (*this, "Choose a MIDI file to render",
                            PresetManager::getRenderFolder(), "*.mid;*.midi",
                            juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                            [this] (const juce::File& file)
        {
            importedMidiFile = file;
            updateEstimate();
        },
        [this]
        {
            importedMidiFile = juce::File();
            sourceBox.setSelectedId (1, juce::dontSendNotification);
            updateEstimate();
        });
    };

    addLabelled (phraseBox, "Which phrase to render");

    for (int i = 0; i < (int) AuditionPhrase::Type::NumTypes; ++i)
        phraseBox.addItem (AuditionPhrase::getName ((AuditionPhrase::Type) i), i + 1);

    phraseBox.setSelectedItemIndex (0);
    phraseBox.onChange = [this] { updateEstimate(); };

    addLabelled (formatBox, "File format");
    formatBox.addItem ("WAV", 1);
    formatBox.addItem ("AIFF", 2);
    formatBox.addItem ("FLAC", 3);
    formatBox.setSelectedId (1);
    formatBox.onChange = [this]
    {
        const auto format = (AudioExporter::Format) (formatBox.getSelectedId() - 1);
        const auto depths = AudioExporter::getSupportedBitDepths (format);

        const int previous = bitDepthBox.getSelectedId();
        bitDepthBox.clear (juce::dontSendNotification);

        for (int d : depths)
            bitDepthBox.addItem (juce::String (d) + "-bit", d);

        bitDepthBox.setSelectedId (depths.contains (previous) ? previous : depths.getLast(),
                                   juce::dontSendNotification);
        updateEstimate();
    };

    addLabelled (bitDepthBox, "Bit depth");
    bitDepthBox.addItem ("16-bit", 16);
    bitDepthBox.addItem ("24-bit", 24);
    bitDepthBox.addItem ("32-bit", 32);
    bitDepthBox.setSelectedId (24);
    bitDepthBox.onChange = [this] { updateEstimate(); };

    addLabelled (sampleRateBox, "Sample rate");

    for (int rate : { 44100, 48000, 88200, 96000, 176400, 192000 })
        sampleRateBox.addItem (juce::String (rate / 1000.0, 1) + " kHz", rate);

    sampleRateBox.setSelectedId (48000);
    sampleRateBox.onChange = [this] { updateEstimate(); };

    addAndMakeVisible (tailSlider);
    tailSlider.setRange (0.0, 20.0, 0.1);
    tailSlider.setValue (4.0);
    tailSlider.setTextValueSuffix (" s tail");
    tailSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tailSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 20);
    tailSlider.setTooltip ("Extra time recorded after the last note, so the reverb and the "
                           "ringing strings are not cut off");
    tailSlider.onValueChange = [this] { updateEstimate(); };

    addAndMakeVisible (normaliseToggle);
    normaliseToggle.setTooltip ("Scale the whole render so its loudest peak hits the target");
    normaliseToggle.onClick = [this] { normaliseTarget.setEnabled (normaliseToggle.getToggleState()); };

    addAndMakeVisible (normaliseTarget);
    normaliseTarget.setRange (-12.0, 0.0, 0.1);
    normaliseTarget.setValue (-1.0);
    normaliseTarget.setTextValueSuffix (" dBFS");
    normaliseTarget.setSliderStyle (juce::Slider::LinearHorizontal);
    normaliseTarget.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 20);
    normaliseTarget.setEnabled (false);

    addAndMakeVisible (fileNameEditor);
    fileNameEditor.setText ("Luthier Render", false);
    fileNameEditor.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);

    addAndMakeVisible (chooseFolderButton);
    chooseFolderButton.onClick = [this]
    {
        fileChooser.launch (*this, "Choose where to save the render", destinationFolder, {},
                            juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories,
                            [this] (const juce::File& folder)
        {
            if (folder.isDirectory())
            {
                destinationFolder = folder;
                updateEstimate();
            }
        });
    };

    addAndMakeVisible (exportButton);
    exportButton.setColour (juce::TextButton::textColourOffId, Palette::accent);
    exportButton.onClick = [this] { startExport(); };

    addAndMakeVisible (cancelButton);
    cancelButton.setEnabled (false);
    cancelButton.onClick = [this] { processor.getExporter().cancelExport(); };

    addAndMakeVisible (statusLabel);
    statusLabel.setFont (Fonts::ui (11.5f));
    statusLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    statusLabel.setJustificationType (juce::Justification::topLeft);

    addAndMakeVisible (estimateLabel);
    estimateLabel.setFont (Fonts::mono (11.0f));
    estimateLabel.setColour (juce::Label::textColourId, Palette::textMuted);

    addAndMakeVisible (progressBar);
    progressBar.setVisible (false);

    motion.startTimerHz (*this, 10);
}

ExportPanel::~ExportPanel()
{
    motion.stopTimer();
}

void ExportPanel::overlayShown()
{
    phraseBox.setSelectedItemIndex ((int) processor.getUiState().auditionType,
                                    juce::dontSendNotification);
    updateEstimate();
}

void ExportPanel::updateEstimate()
{
    const double rate = (double) juce::jmax (8000, sampleRateBox.getSelectedId());
    const int depth = juce::jmax (16, bitDepthBox.getSelectedId());

    double musicSeconds = 4.0;

    if (sourceBox.getSelectedId() == 2 && importedMidiFile.existsAsFile())
    {
        juce::FileInputStream stream (importedMidiFile);
        juce::MidiFile file;

        if (stream.openedOk() && file.readFrom (stream))
        {
            file.convertTimestampTicksToSeconds();
            musicSeconds = juce::jmax (0.5, file.getLastTimestamp());
        }
    }
    else
    {
        musicSeconds = AuditionPhrase::getDurationSeconds (
            (AuditionPhrase::Type) juce::jmax (0, phraseBox.getSelectedItemIndex()),
            processor.getHostTempo());
    }

    const double total = musicSeconds + tailSlider.getValue();
    const double bytes = total * rate * (depth / 8.0) * 2.0;

    estimateLabel.setText (juce::String (total, 1) + " s   ~"
                           + juce::File::descriptionOfSizeInBytes ((juce::int64) bytes)
                           + "   ->   " + destinationFolder.getFullPathName(),
                           juce::dontSendNotification);
}

void ExportPanel::startExport()
{
    if (processor.getExporter().isExporting())
        return;

    AudioExporter::Options options;
    options.format = (AudioExporter::Format) juce::jmax (0, formatBox.getSelectedId() - 1);
    options.bitDepth = juce::jmax (16, bitDepthBox.getSelectedId());
    options.sampleRate = (double) juce::jmax (8000, sampleRateBox.getSelectedId());
    options.tailSeconds = tailSlider.getValue();
    options.normalise = normaliseToggle.getToggleState();
    options.normaliseTargetDb = normaliseTarget.getValue();
    options.tempoBpm = processor.getHostTempo();
    options.numChannels = 2;

    auto name = fileNameEditor.getText().trim();

    if (name.isEmpty())
        name = "Luthier Render";

    options.outputFile = destinationFolder.getNonexistentChildFile (
        juce::File::createLegalFileName (name),
        AudioExporter::getExtension (options.format));

    // ---- what to render -------------------------------------------------------
    juce::MidiMessageSequence sequence;

    if (sourceBox.getSelectedId() == 2 && importedMidiFile.existsAsFile())
    {
        juce::FileInputStream stream (importedMidiFile);
        juce::MidiFile file;

        if (stream.openedOk() && file.readFrom (stream))
        {
            file.convertTimestampTicksToSeconds();

            for (int t = 0; t < file.getNumTracks(); ++t)
                sequence.addSequence (*file.getTrack (t), 0.0);

            sequence.updateMatchedPairs();
            sequence.sort();
        }
    }

    if (sequence.getNumEvents() == 0)
        sequence = AuditionPhrase::build (
            (AuditionPhrase::Type) juce::jmax (0, phraseBox.getSelectedItemIndex()),
            processor.getHostTempo());

    if (sequence.getNumEvents() == 0)
    {
        statusLabel.setText ("There is nothing to render.", juce::dontSendNotification);
        return;
    }

    const auto state = processor.captureStateBlock();

    statusLabel.setText ("Rendering...", juce::dontSendNotification);
    progressBar.setVisible (true);
    exportButton.setEnabled (false);
    cancelButton.setEnabled (true);

    processor.getExporter().startExport (
        options, sequence, state,
        [] { return LuthierAudioProcessor::createOfflineInstance(); },
        [safe = juce::Component::SafePointer<ExportPanel> (this)] (const AudioExporter::Result& result)
        {
            // The exporter belongs to the processor and outlives this panel: the
            // window may have been closed while it rendered.
            if (safe != nullptr)
            {
                safe->progressBar.setVisible (false);
                safe->exportButton.setEnabled (true);
                safe->cancelButton.setEnabled (false);
                safe->statusLabel.setText (result.message, juce::dontSendNotification);
            }

            // On success the user is told everything the brief asks for: that it
            // worked, where it went, how long it is and at what quality.
            juce::NativeMessageBox::showAsync (
                juce::MessageBoxOptions()
                    .withIconType (result.success ? juce::MessageBoxIconType::InfoIcon
                                                  : juce::MessageBoxIconType::WarningIcon)
                    .withTitle (result.success ? "Export finished" : "Export failed")
                    .withMessage (AudioExporter::describeResult (result))   // SPEC-SWEEP INC-16
                    .withButton ("OK"),
                nullptr);
        });
}

void ExportPanel::timerCallback()
{
    progress = processor.getExporter().getProgress();

    if (processor.getExporter().isExporting())
        statusLabel.setText ("Rendering... " + juce::String (juce::roundToInt (progress * 100.0)) + "%",
                             juce::dontSendNotification);
}

void ExportPanel::layoutContent (juce::Rectangle<int> content)
{
    auto row = [&content] (int height)
    {
        auto r = content.removeFromTop (height);
        content.removeFromTop (Metrics::gridHalf);
        return r;
    };

    auto sourceRow = row (26);
    sourceBox.setBounds (sourceRow.removeFromLeft (170));
    sourceRow.removeFromLeft (Metrics::gridHalf);
    phraseBox.setBounds (sourceRow);

    auto formatRow = row (26);
    formatBox.setBounds (formatRow.removeFromLeft (110));
    formatRow.removeFromLeft (Metrics::gridHalf);
    bitDepthBox.setBounds (formatRow.removeFromLeft (110));
    formatRow.removeFromLeft (Metrics::gridHalf);
    sampleRateBox.setBounds (formatRow.removeFromLeft (120));

    tailSlider.setBounds (row (24));

    auto normRow = row (24);
    normaliseToggle.setBounds (normRow.removeFromLeft (110));
    normaliseTarget.setBounds (normRow);

    auto fileRow = row (26);
    chooseFolderButton.setBounds (fileRow.removeFromRight (100));
    fileRow.removeFromRight (Metrics::gridHalf);
    fileNameEditor.setBounds (fileRow);

    estimateLabel.setBounds (row (20));

    auto buttons = content.removeFromBottom (Metrics::buttonHeight);
    exportButton.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (Metrics::gridHalf);
    cancelButton.setBounds (buttons.removeFromRight (100));

    content.removeFromBottom (Metrics::gridHalf);
    progressBar.setBounds (content.removeFromBottom (18));
    content.removeFromBottom (Metrics::gridHalf);

    statusLabel.setBounds (content);
}

//==============================================================================
//  SaveAsPanel
//==============================================================================
SaveAsPanel::SaveAsPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Save Preset As"), processor (p)
{
    auto setupEditor = [this] (juce::TextEditor& e, const juce::String& placeholder, bool multiline)
    {
        addAndMakeVisible (e);
        e.setMultiLine (multiline);
        e.setReturnKeyStartsNewLine (multiline);
        e.setTextToShowWhenEmpty (placeholder, Palette::textDisabled);
        e.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    };

    setupEditor (nameEditor, "Preset name", false);
    setupEditor (descriptionEditor, "What this sound is for (optional)", true);
    setupEditor (tagsEditor, "Tags, separated by commas (optional)", false);

    addAndMakeVisible (categoryBox);
    categoryBox.setEditableText (true);
    categoryBox.setTooltip ("The folder the preset is saved into, and the category it is "
                            "listed under. Type a new one to create it.");

    addAndMakeVisible (saveButton);
    saveButton.setColour (juce::TextButton::textColourOffId, Palette::accent);
    saveButton.onClick = [this]
    {
        const auto name = nameEditor.getText().trim();

        if (name.isEmpty())
        {
            status.setText ("Give the preset a name first.", juce::dontSendNotification);
            return;
        }

        auto category = categoryBox.getText().trim();

        if (category.isEmpty())
            category = "User";

        const auto tags = juce::StringArray::fromTokens (tagsEditor.getText(), ",", "");

        if (processor.getPresetManager().saveAs (name, category, descriptionEditor.getText(), tags))
        {
            status.setText ("Saved.", juce::dontSendNotification);

            if (onDismiss)
                onDismiss();
        }
        else
        {
            status.setText ("Could not save. Check that Documents/Luthier/Presets/User is writable.",
                            juce::dontSendNotification);
        }
    };

    addAndMakeVisible (status);
    status.setFont (Fonts::ui (11.5f));
    status.setColour (juce::Label::textColourId, Palette::warning);
}

void SaveAsPanel::overlayShown()
{
    auto& manager = processor.getPresetManager();

    nameEditor.setText (manager.getCurrentPresetName(), false);
    nameEditor.selectAll();
    nameEditor.grabKeyboardFocus();

    categoryBox.clear (juce::dontSendNotification);

    int id = 1;
    for (const auto& category : manager.getCategories())
        categoryBox.addItem (category, id++);

    if (id == 1)
        categoryBox.addItem ("User", 1);

    categoryBox.setText ("User", juce::dontSendNotification);
    status.setText ({}, juce::dontSendNotification);
}

void SaveAsPanel::layoutContent (juce::Rectangle<int> content)
{
    nameEditor.setBounds (content.removeFromTop (28));
    content.removeFromTop (Metrics::gridHalf);

    categoryBox.setBounds (content.removeFromTop (26));
    content.removeFromTop (Metrics::gridHalf);

    tagsEditor.setBounds (content.removeFromTop (26));
    content.removeFromTop (Metrics::gridHalf);

    auto buttons = content.removeFromBottom (Metrics::buttonHeight);
    saveButton.setBounds (buttons.removeFromRight (110));

    content.removeFromBottom (Metrics::gridHalf);
    status.setBounds (content.removeFromBottom (20));
    content.removeFromBottom (Metrics::gridHalf);

    descriptionEditor.setBounds (content);
}

//==============================================================================
//  ChordAndTabPanel
//==============================================================================
int ChordAndTabPanel::ChordListModel::getNumRows()
{
    return owner.matches.size();
}

void ChordAndTabPanel::ChordListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                         int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, owner.matches.size()))
        return;

    const auto& chord = ChordVoicer::getLibraryChord (owner.matches[row]);

    if (selected)
    {
        g.setColour (Palette::accent.withAlpha (0.16f));
        g.fillRect (0, 0, width, height);
    }

    g.setColour (selected ? Palette::accent : Palette::textPrimary);
    g.setFont (Fonts::ui (12.5f, selected));
    g.drawText (chord.name, 10, 0, width - 14, height, juce::Justification::centredLeft, true);
}

void ChordAndTabPanel::ChordListModel::selectedRowsChanged (int lastRow)
{
    if (juce::isPositiveAndBelow (lastRow, owner.matches.size()))
    {
        owner.selectedChord = owner.matches[lastRow];
        owner.diagram.repaint();
    }
}

void ChordAndTabPanel::DiagramComponent::paint (juce::Graphics& g)
{
    const auto& chord = ChordVoicer::getLibraryChord (owner.selectedChord);

    auto bounds = getLocalBounds().reduced (Metrics::grid);

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (18.0f, true));
    g.drawText (chord.name, bounds.removeFromTop (28), juce::Justification::centred, false);

    bounds.reduce (Metrics::grid, Metrics::grid);

    // Find the window of frets to show.
    int lowest = 99, highest = 0;

    for (int s = 0; s < 6; ++s)
    {
        if (chord.frets[s] > 0)
        {
            lowest = juce::jmin (lowest, chord.frets[s]);
            highest = juce::jmax (highest, chord.frets[s]);
        }
    }

    const int firstFret = (lowest > 4 && lowest != 99) ? lowest : 1;
    const int numFrets = juce::jmax (4, juce::jmin (5, highest - firstFret + 1));

    const float stringSpacing = (float) bounds.getWidth() / 5.0f;
    const float fretSpacing = (float) bounds.getHeight() / (float) (numFrets + 1);

    // Strings run left to right with the low E on the left, as a chord chart is
    // conventionally drawn - which is the mirror of the fretboard above, on purpose.
    for (int s = 0; s < 6; ++s)
    {
        const float x = (float) bounds.getX() + stringSpacing * (float) s;
        g.setColour (Palette::textDisabled);
        g.drawVerticalLine ((int) x, (float) bounds.getY() + fretSpacing,
                            (float) bounds.getBottom());
    }

    for (int f = 0; f <= numFrets; ++f)
    {
        const float y = (float) bounds.getY() + fretSpacing * (float) (f + 1);

        g.setColour (f == 0 && firstFret == 1 ? Palette::textPrimary : Palette::textDisabled);
        g.fillRect ((float) bounds.getX(), y, (float) bounds.getWidth(),
                    (f == 0 && firstFret == 1) ? 3.0f : 1.0f);
    }

    if (firstFret > 1)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::mono (11.0f));
        g.drawText (juce::String (firstFret) + "fr",
                    bounds.getX() - 34, (int) ((float) bounds.getY() + fretSpacing), 32, 16,
                    juce::Justification::centredRight, false);
    }

    // Dots. Library chords index string 0 as the high E, so the chart is drawn in
    // reverse to put the low E on the left.
    for (int s = 0; s < 6; ++s)
    {
        const int display = 5 - s;
        const float x = (float) bounds.getX() + stringSpacing * (float) display;
        const int fret = chord.frets[s];

        if (fret < 0)
        {
            g.setColour (Palette::textDisabled);
            g.setFont (Fonts::ui (13.0f));
            g.drawText ("x", (int) x - 8, bounds.getY() - 2, 16, 16, juce::Justification::centred, false);
            continue;
        }

        if (fret == 0)
        {
            g.setColour (Palette::textMuted);
            g.drawEllipse (x - 5.0f, (float) bounds.getY() + 2.0f, 10.0f, 10.0f, 1.2f);
            continue;
        }

        const float y = (float) bounds.getY() + fretSpacing * ((float) (fret - firstFret) + 1.5f);

        g.setColour (Palette::accent);
        g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);

        if (chord.fingers[s] > 0)
        {
            g.setColour (Palette::backgroundDeep);
            g.setFont (Fonts::ui (10.0f, true));
            g.drawText (juce::String (chord.fingers[s]), (int) x - 8, (int) y - 8, 16, 16,
                        juce::Justification::centred, false);
        }
    }
}

ChordAndTabPanel::ChordAndTabPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Chords and Tab"), processor (p)
{
    lastNotes.fill (-1);

    addAndMakeVisible (searchBox);
    searchBox.setTextToShowWhenEmpty ("Search chords...", Palette::textDisabled);
    searchBox.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    searchBox.onTextChange = [this]
    {
        int found[256];
        const int count = ChordVoicer::searchLibrary (searchBox.getText(), found, 256);

        matches.clear();

        for (int i = 0; i < count; ++i)
            matches.add (found[i]);

        chordList.updateContent();

        if (matches.size() > 0)
            chordList.selectRow (0);
    };

    addAndMakeVisible (chordList);
    chordList.setModel (&listModel);
    chordList.setRowHeight (24);
    chordList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);

    addAndMakeVisible (diagram);

    addAndMakeVisible (tabView);
    tabView.setMultiLine (true);
    tabView.setReadOnly (true);
    tabView.setScrollbarsShown (true);
    tabView.setCaretVisible (false);
    tabView.setFont (Fonts::mono (12.0f));
    tabView.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);

    addAndMakeVisible (recordTabToggle);
    recordTabToggle.setTooltip ("Write what you play into the tab below, in real time");
    recordTabToggle.setToggleState (true, juce::dontSendNotification);

    addAndMakeVisible (clearTabButton);
    clearTabButton.onClick = [this]
    {
        tabLines.clear();
        tabView.clear();
        lastNotes.fill (-1);
    };

    addAndMakeVisible (exportTabButton);
    exportTabButton.onClick = [this]
    {
        fileChooser.launch (*this, "Export the tab",
                            PresetManager::getRenderFolder().getChildFile ("Luthier Tab.txt"), "*.txt",
                            juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                            [this] (const juce::File& file)
        {
            file.replaceWithText (tabView.getText());
        });
    };

    searchBox.onTextChange();
    startTimerHz (12);
}

ChordAndTabPanel::~ChordAndTabPanel()
{
    stopTimer();
}

void ChordAndTabPanel::overlayShown()
{
    searchBox.onTextChange();
}

void ChordAndTabPanel::captureTabColumn()
{
    auto& engine = processor.getEngine();
    const int numStrings = engine.getNumStrings();

    bool anyNew = false;
    int column[kMaxStrings];

    for (int s = 0; s < numStrings; ++s)
    {
        const int note = engine.getStringMidiNote (s);
        column[s] = -1;

        if (note >= 0 && note != lastNotes[(size_t) s])
        {
            column[s] = (int) std::round (engine.getStringFret (s));
            anyNew = true;
        }

        lastNotes[(size_t) s] = note;
    }

    if (! anyNew)
        return;

    // Tab is written with the high E on top, which is the convention.
    while (tabLines.size() < numStrings)
        tabLines.add ("|");

    for (int s = 0; s < numStrings; ++s)
    {
        const auto cell = (column[s] >= 0) ? juce::String (column[s]) : juce::String ("-");
        tabLines.set (s, tabLines[s] + cell.paddedRight ('-', 3));
    }

    // Wrap at a sensible width so the tab stays readable.
    if (tabLines[0].length() > 76)
    {
        juce::String block;

        for (const auto& line : tabLines)
            block << line << "\n";

        block << "\n";

        tabView.moveCaretToEnd (false);
        tabView.insertTextAtCaret (block);

        for (int s = 0; s < tabLines.size(); ++s)
            tabLines.set (s, "|");
    }
}

void ChordAndTabPanel::timerCallback()
{
    if (recordTabToggle.getToggleState())
        captureTabColumn();
}

void ChordAndTabPanel::layoutContent (juce::Rectangle<int> content)
{
    auto left = content.removeFromLeft (juce::jmax (200, content.getWidth() / 3));
    content.removeFromLeft (Metrics::grid);

    searchBox.setBounds (left.removeFromTop (26));
    left.removeFromTop (Metrics::gridHalf);
    chordList.setBounds (left);

    auto diagramArea = content.removeFromTop (juce::jmax (180, content.getHeight() / 2));
    diagram.setBounds (diagramArea);

    content.removeFromTop (Metrics::grid);

    auto tabButtons = content.removeFromTop (Metrics::buttonHeight);
    recordTabToggle.setBounds (tabButtons.removeFromLeft (170));
    exportTabButton.setBounds (tabButtons.removeFromRight (110));
    tabButtons.removeFromRight (Metrics::gridHalf);
    clearTabButton.setBounds (tabButtons.removeFromRight (100));

    content.removeFromTop (Metrics::gridHalf);
    tabView.setBounds (content);
}

//==============================================================================
//  SecretPanel
//==============================================================================
SecretPanel::SecretPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Wolf"), processor (p)
{
    addAndMakeVisible (blurb);
    blurb.setFont (Fonts::ui (11.5f));
    blurb.setColour (juce::Label::textColourId, Palette::secondary);
    blurb.setJustificationType (juce::Justification::topLeft);
    blurb.setText ("You found it.\n\n"
                   "A wolf tone is what happens when a note lands exactly on a strong body "
                   "resonance and the two feed each other until the instrument howls. Luthiers "
                   "spend real effort designing it out. This puts it back, and then some: a "
                   "dispersive feedback network that smears transients into a rising chirp and "
                   "blooms sustained notes into a howl.\n\n"
                   "It sits right at the end of the chain, after the room. Regeneration is "
                   "capped below unity, so it will misbehave but it will not run away.",
                   juce::dontSendNotification);

    const juce::String secretTip = "Hidden effect - Wolf. Found by clicking the notch in the "
                                   "top-left corner of the window.";

    addAndMakeVisible (enableToggle);
    enableToggle.attachTo (processor, ParamIDs::secretOn, secretTip + " Engages the effect.");

    addAndMakeVisible (rateKnob);
    rateKnob.attachTo (processor, ParamIDs::secretRate,
                       secretTip + " How fast the network's delay is swept.");
    rateKnob.setAccentColour (Palette::secondary);

    addAndMakeVisible (depthKnob);
    depthKnob.attachTo (processor, ParamIDs::secretDepth,
                        secretTip + " How far it sweeps, and how much dispersion goes with it.");
    depthKnob.setAccentColour (Palette::secondary);

    addAndMakeVisible (feedbackKnob);
    feedbackKnob.attachTo (processor, ParamIDs::secretFeedback,
                           secretTip + " How much of the output goes back in. Capped below unity.");
    feedbackKnob.setAccentColour (Palette::secondary);

    addAndMakeVisible (mixKnob);
    mixKnob.attachTo (processor, ParamIDs::secretMix, secretTip + " Wet/dry balance.");
    mixKnob.setAccentColour (Palette::secondary);
}

void SecretPanel::layoutContent (juce::Rectangle<int> content)
{
    enableToggle.setBounds (content.removeFromTop (Metrics::buttonHeight).removeFromLeft (120));
    content.removeFromTop (Metrics::grid);

    auto knobRow = content.removeFromTop (LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));
    const int cell = knobRow.getWidth() / 4;

    rateKnob.setBounds (knobRow.removeFromLeft (cell));
    depthKnob.setBounds (knobRow.removeFromLeft (cell));
    feedbackKnob.setBounds (knobRow.removeFromLeft (cell));
    mixKnob.setBounds (knobRow);

    content.removeFromTop (Metrics::grid);
    blurb.setBounds (content);
}

} // namespace luthier
