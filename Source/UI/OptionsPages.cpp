#include "OptionsPages.h"
#include "RangesUi.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"

namespace luthier
{

namespace
{
    void styleNote (juce::Label& label, juce::Colour colour = Palette::textDisabled,
                    float size = 10.0f)
    {
        label.setFont (juce::Font (juce::FontOptions (size)));
        label.setColour (juce::Label::textColourId, colour);
        label.setJustificationType (juce::Justification::topLeft);
    }

    void styleSlider (juce::Slider& slider, double minimum, double maximum,
                      double interval, const juce::String& suffix)
    {
        slider.setRange (minimum, maximum, interval);
        slider.setTextValueSuffix (suffix);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void drawHeading (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& text)
    {
        g.setColour (Palette::accent);
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        g.drawText (text, bounds, juce::Justification::centredLeft, false);
    }
}

//==============================================================================
ControllersPage::ControllersPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    library.refresh();

    int itemId = 1;

    for (const auto& name : library.getDisplayNames())
        profileBox.addItem (name, itemId++);

    profileBox.setSelectedId (1, juce::dontSendNotification);
    profileBox.onChange = [this] { if (! updatingControls) applySelectedProfile(); };
    addAndMakeVisible (profileBox);

    styleNote (profileNotes);
    styleNote (routingLabel, Palette::textMuted);

    addAndMakeVisible (profileNotes);
    addAndMakeVisible (routingLabel);

    styleSlider (latencySlider, 0.0, 50.0, 0.1, " ms");
    latencySlider.setTooltip ("How late this controller's notes arrive. Measured by the "
                              "wizard below, or set by hand.");
    addAndMakeVisible (latencySlider);

    styleSlider (deadZoneSlider, 0.0, 50.0, 0.5, " cents");
    deadZoneSlider.setTooltip ("Pitch movement smaller than this is treated as tracking "
                               "noise rather than as a bend.");

    deadZoneSlider.onValueChange = [this]
    {
        if (! updatingControls)
            processor.getEngine().getMidiInterpreter()
                     .setPitchDeadZoneCents (deadZoneSlider.getValue());
    };

    addAndMakeVisible (deadZoneSlider);

    styleSlider (minimumNoteSlider, 0.0, 200.0, 1.0, " ms");
    minimumNoteSlider.setTooltip ("Hold every note at least this long, for controllers "
                                  "that send a note-off far too early.");

    minimumNoteSlider.onValueChange = [this]
    {
        if (! updatingControls)
            processor.getEngine().getMidiInterpreter()
                     .setMinimumNoteDurationMs (minimumNoteSlider.getValue());
    };

    addAndMakeVisible (minimumNoteSlider);

    guitarModeToggle.setTooltip ("On a LinnStrument, map each row to a string.");
    addAndMakeVisible (guitarModeToggle);

    wizardButton.setTooltip ("Plays a click and measures how long after it your "
                             "controller's note arrives, ten times over.");
    wizardButton.onClick = [this] { runWizardStep(); };
    addAndMakeVisible (wizardButton);

    styleNote (wizardLabel, Palette::textMuted);
    addAndMakeVisible (wizardLabel);

    saveProfileButton.setTooltip ("Write this profile, with your measured latency, into "
                                  "your own Controllers folder. It will be used instead "
                                  "of the built-in one from now on.");

    saveProfileButton.onClick = [this]
    {
        const int index = profileBox.getSelectedId() - 1;

        if (! juce::isPositiveAndBelow (index, library.getNumProfiles()))
            return;

        auto profile = library.getProfile (index);

        profile.latencyMsMeasured = latencySlider.getValue();
        profile.pitchDeadZoneCents = deadZoneSlider.getValue();
        profile.minimumNoteDurationMs = minimumNoteSlider.getValue();
        profile.rowsAsStrings = guitarModeToggle.getToggleState();

        library.save (profile);

        wizardLabel.setText ("Saved to your Controllers folder.", juce::dontSendNotification);
    };

    addAndMakeVisible (saveProfileButton);

    refresh();
}

void ControllersPage::applySelectedProfile()
{
    const int index = profileBox.getSelectedId() - 1;

    if (! juce::isPositiveAndBelow (index, library.getNumProfiles()))
        return;

    const auto& profile = library.getProfile (index);

    ControllerProfileLibrary::apply (profile, processor.getEngine().getMidiInterpreter());

    refresh();
}

void ControllersPage::runWizardStep()
{
    /*  The latency wizard (controllers.md 3).

        A real measurement needs a click and a player. What the wizard does here
        is arm itself and then take each incoming note as a measurement; the
        click is the metronome, which the wizard switches on. Ten runs later it
        reports the median and the scatter, and only writes the result into the
        profile if the scatter is small enough to mean anything. */
    if (! wizard.isRunning())
    {
        wizard.begin();

        processor.getMetronome().setTempo (100.0);
        processor.getMetronome().setEnabled (true);

        wizardButton.setButtonText ("Stop measuring");
        wizardLabel.setText ("Play along with the click. Ten notes.",
                             juce::dontSendNotification);
        return;
    }

    wizard.cancel();
    processor.getMetronome().setEnabled (false);
    wizardButton.setButtonText ("Measure latency");
    wizardLabel.setText ("Cancelled.", juce::dontSendNotification);
}

void ControllersPage::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    const int index = profileBox.getSelectedId() - 1;

    if (! juce::isPositiveAndBelow (index, library.getNumProfiles()))
        return;

    const auto& profile = library.getProfile (index);

    profileNotes.setText (profile.notes, juce::dontSendNotification);

    // What the profile actually routes, spelled out, because a silent controller
    // is almost always a routing mismatch.
    juce::String routing;

    switch (profile.mode)
    {
        case ControllerMode::standard:
            routing = "Standard MIDI, pitch bend " + juce::String (profile.pitchBendSemis, 0)
                        + " semitones.";
            break;

        case ControllerMode::mpe:
            routing = "MPE: master channel " + juce::String (profile.mpeMasterChannel)
                        + ", members " + juce::String (profile.mpeFirstMemberChannel)
                        + " to " + juce::String (profile.mpeLastMemberChannel)
                        + ", bend " + juce::String (profile.memberPitchBendSemis, 0) + ".";
            break;

        case ControllerMode::perChannel:
        {
            juce::StringArray parts;

            for (int s = 0; s < kMaxStrings; ++s)
                if (profile.perString[(size_t) s].channel > 0)
                    parts.add ("string " + juce::String (s + 1) + " on channel "
                                 + juce::String (profile.perString[(size_t) s].channel));

            routing = parts.joinIntoString (", ") + ".";
            break;
        }

        default:
            break;
    }

    routingLabel.setText (routing, juce::dontSendNotification);

    latencySlider.setValue (profile.getEffectiveLatencyMs(), juce::dontSendNotification);
    deadZoneSlider.setValue (profile.pitchDeadZoneCents, juce::dontSendNotification);
    minimumNoteSlider.setValue (profile.minimumNoteDurationMs, juce::dontSendNotification);
    guitarModeToggle.setToggleState (profile.rowsAsStrings, juce::dontSendNotification);

    guitarModeToggle.setEnabled (profile.id == "linnstrument");

    // ---- the wizard's own readout -------------------------------------------------
    if (wizard.isRunning())
    {
        wizardLabel.setText (juce::String (wizard.getNumMeasurements()) + " of "
                               + juce::String (wizard.getRunsWanted()) + " notes",
                             juce::dontSendNotification);
    }
    else if (wizard.getNumMeasurements() >= wizard.getRunsWanted())
    {
        const auto measured = wizard.getMeasuredLatencyMs();
        const auto sigma = wizard.getSigmaMs();

        if (wizard.isReliable())
        {
            latencySlider.setValue (measured, juce::dontSendNotification);

            wizardLabel.setText (juce::String (measured, 2) + " ms, scatter "
                                   + juce::String (sigma, 2) + " ms. Good enough to use.",
                                 juce::dontSendNotification);
        }
        else
        {
            // controllers 7 wants a sigma inside half a millisecond. Saying so is
            // more use than quietly writing an unreliable number into a profile.
            wizardLabel.setText (juce::String (measured, 2) + " ms, but the scatter was "
                                   + juce::String (sigma, 2) + " ms. Try again, more evenly.",
                                 juce::dontSendNotification);
        }

        wizardButton.setButtonText ("Measure latency");
    }
}

void ControllersPage::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawHeading (g, bounds.removeFromTop (18), "CONTROLLER PROFILE");

    // The wizard's section heading, positioned where resized() put its controls.
    drawHeading (g, { 0, getHeight() - 116, getWidth(), 18 }, "LATENCY WIZARD");
}

void ControllersPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (20);

    profileBox.setBounds (bounds.removeFromTop (28));
    bounds.removeFromTop (4);

    routingLabel.setBounds (bounds.removeFromTop (28));
    profileNotes.setBounds (bounds.removeFromTop (28));

    bounds.removeFromTop (8);

    latencySlider.setBounds (bounds.removeFromTop (24));
    bounds.removeFromTop (4);
    deadZoneSlider.setBounds (bounds.removeFromTop (24));
    bounds.removeFromTop (4);
    minimumNoteSlider.setBounds (bounds.removeFromTop (24));
    bounds.removeFromTop (4);
    guitarModeToggle.setBounds (bounds.removeFromTop (24));

    // ---- wizard, pinned to the bottom -------------------------------------------
    auto wizardArea = getLocalBounds().removeFromBottom (96);

    auto row = wizardArea.removeFromTop (28);

    wizardButton.setBounds (row.removeFromLeft (140));
    row.removeFromLeft (8);
    saveProfileButton.setBounds (row.removeFromLeft (160));

    wizardArea.removeFromTop (4);
    wizardLabel.setBounds (wizardArea.removeFromTop (36));
}

//==============================================================================
ExpressionPage::ExpressionPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    for (int cc = 1; cc <= 127; ++cc)
        ccBox.addItem ("CC " + juce::String (cc), cc);

    // CC 11 is the expression pedal's conventional home.
    ccBox.setSelectedId (11, juce::dontSendNotification);
    ccBox.onChange = [this] { refresh(); };
    addAndMakeVisible (ccBox);

    for (int i = 0; i < (int) ExpressionCalibration::Curve::numCurves; ++i)
        curveBox.addItem (getExpressionCurveName ((ExpressionCalibration::Curve) i), i + 1);

    curveBox.setSelectedId (1, juce::dontSendNotification);

    curveBox.onChange = [this]
    {
        if (updatingControls)
            return;

        auto calibration = processor.getExpression().get (ccBox.getSelectedId());
        calibration.ccNumber = ccBox.getSelectedId();
        calibration.curve = (ExpressionCalibration::Curve) (curveBox.getSelectedId() - 1);

        processor.getExpression().set (calibration);
        processor.getExpression().save();
    };

    addAndMakeVisible (curveBox);

    styleNote (promptLabel, Palette::textPrimary, 12.0f);
    styleNote (rangeLabel);

    addAndMakeVisible (promptLabel);
    addAndMakeVisible (rangeLabel);

    calibrateButton.onClick = [this] { advanceWizard(); };
    addAndMakeVisible (calibrateButton);

    cancelButton.onClick = [this]
    {
        processor.getExpression().cancelCalibration();
        refresh();
    };

    addAndMakeVisible (cancelButton);

    styleSlider (heelDeadZone, 0.0, 45.0, 0.5, " %");
    styleSlider (toeDeadZone, 0.0, 45.0, 0.5, " %");

    auto writeDeadZones = [this]
    {
        if (updatingControls)
            return;

        auto calibration = processor.getExpression().get (ccBox.getSelectedId());
        calibration.ccNumber = ccBox.getSelectedId();
        calibration.heelDeadZone = heelDeadZone.getValue() * 0.01;
        calibration.toeDeadZone = toeDeadZone.getValue() * 0.01;

        processor.getExpression().set (calibration);
        processor.getExpression().save();
    };

    heelDeadZone.onValueChange = writeDeadZones;
    toeDeadZone.onValueChange = writeDeadZones;

    addAndMakeVisible (heelDeadZone);
    addAndMakeVisible (toeDeadZone);

    calibratedList.setModel (&listModel);
    calibratedList.setRowHeight (20);
    calibratedList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    addAndMakeVisible (calibratedList);

    refresh();
}

void ExpressionPage::advanceWizard()
{
    auto& expression = processor.getExpression();

    switch (expression.getWizardStage())
    {
        case ExpressionCalibrationSet::WizardStage::idle:
        case ExpressionCalibrationSet::WizardStage::done:
            expression.beginCalibration (ccBox.getSelectedId());
            break;

        case ExpressionCalibrationSet::WizardStage::heel:
        case ExpressionCalibrationSet::WizardStage::toe:
            expression.confirmStage();

            if (expression.getWizardStage() == ExpressionCalibrationSet::WizardStage::done)
                expression.save();

            break;

        default:
            break;
    }

    refresh();
}

void ExpressionPage::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    auto& expression = processor.getExpression();

    // live-performance 8: the prompts, in order.
    switch (expression.getWizardStage())
    {
        case ExpressionCalibrationSet::WizardStage::heel:
            promptLabel.setText ("Heel down: rock the pedal fully back, then confirm.",
                                 juce::dontSendNotification);
            calibrateButton.setButtonText ("Confirm heel");
            break;

        case ExpressionCalibrationSet::WizardStage::toe:
            promptLabel.setText ("Toe down: rock the pedal fully forward, then confirm.",
                                 juce::dontSendNotification);
            calibrateButton.setButtonText ("Confirm toe");
            break;

        case ExpressionCalibrationSet::WizardStage::done:
            promptLabel.setText ("Calibrated.", juce::dontSendNotification);
            calibrateButton.setButtonText ("Calibrate again");
            break;

        case ExpressionCalibrationSet::WizardStage::idle:
        default:
            promptLabel.setText ("Pick the CC your pedal sends, then calibrate it.",
                                 juce::dontSendNotification);
            calibrateButton.setButtonText ("Calibrate");
            break;
    }

    cancelButton.setEnabled (expression.getWizardStage()
                               != ExpressionCalibrationSet::WizardStage::idle);

    const int cc = ccBox.getSelectedId();

    const auto calibration = expression.get (cc);

    rangeLabel.setText (expression.has (cc)
                          ? ("Travel " + juce::String (calibration.rawMinimum) + " to "
                               + juce::String (calibration.rawMaximum))
                          : juce::String ("Not calibrated; the full 0 to 127 is used."),
                        juce::dontSendNotification);

    curveBox.setSelectedId ((int) calibration.curve + 1, juce::dontSendNotification);
    heelDeadZone.setValue (calibration.heelDeadZone * 100.0, juce::dontSendNotification);
    toeDeadZone.setValue (calibration.toeDeadZone * 100.0, juce::dontSendNotification);

    const auto nowCalibrated = expression.getCalibratedCcNumbers();

    if (nowCalibrated != calibratedCcs)
    {
        calibratedCcs = nowCalibrated;
        calibratedList.updateContent();
        calibratedList.repaint();
    }
}

int ExpressionPage::ListModel::getNumRows()
{
    return owner.calibratedCcs.size();
}

void ExpressionPage::ListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                  int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, owner.calibratedCcs.size()))
        return;

    const int cc = owner.calibratedCcs[row];
    const auto calibration = owner.processor.getExpression().get (cc);

    if (selected)
    {
        g.setColour (Palette::accentDim);
        g.fillRect (0, 0, width, height);
    }

    g.setColour (Palette::textMuted);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    g.drawText ("CC " + juce::String (cc) + "    "
                  + juce::String (calibration.rawMinimum) + "-"
                  + juce::String (calibration.rawMaximum) + "    "
                  + getExpressionCurveName (calibration.curve),
                6, 0, width - 12, height, juce::Justification::centredLeft, true);
}

void ExpressionPage::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawHeading (g, bounds.removeFromTop (18), "EXPRESSION PEDAL");

    drawHeading (g, { 0, getHeight() - 136, getWidth(), 18 }, "CALIBRATED PEDALS");
}

void ExpressionPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (20);

    {
        auto row = bounds.removeFromTop (28);

        ccBox.setBounds (row.removeFromLeft (120));
        row.removeFromLeft (8);
        curveBox.setBounds (row.removeFromLeft (160));
    }

    bounds.removeFromTop (6);
    promptLabel.setBounds (bounds.removeFromTop (24));
    rangeLabel.setBounds (bounds.removeFromTop (20));

    bounds.removeFromTop (4);

    {
        auto row = bounds.removeFromTop (28);

        calibrateButton.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (8);
        cancelButton.setBounds (row.removeFromLeft (100));
    }

    bounds.removeFromTop (8);

    heelDeadZone.setBounds (bounds.removeFromTop (24));
    bounds.removeFromTop (4);
    toeDeadZone.setBounds (bounds.removeFromTop (24));

    calibratedList.setBounds (getLocalBounds().removeFromBottom (116));
}

//==============================================================================
AudioPage::AudioPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    addAndMakeVisible (oversampling);
    oversampling.attachTo (processor, ParamIDs::oversample,
                           "Oversampling for the amp and the drive pedals. 4x is the default; "
                           "2x sounds very close and costs noticeably less.");

    addAndMakeVisible (deviceButton);
    deviceButton.setTooltip ("Where the device, sample rate and buffer settings actually live");
    deviceButton.onClick = [this]
    {
        /*  In a plugin the host owns the devices; in the standalone build the
            wrapper does. Either way this plugin does not, so the honest thing is
            to say where the setting lives rather than offer a dead control. */
        const bool standalone =
            (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::InfoIcon)
                .withTitle (standalone ? "Audio and MIDI settings"
                                       : "Audio and MIDI are handled by your host")
                .withMessage (standalone
                                ? "Use the Options button in the standalone window's own toolbar "
                                  "to choose the audio device, the sample rate, the buffer size "
                                  "and which MIDI inputs are active.\n\nThose settings belong to "
                                  "the wrapper rather than to the plugin, so they are remembered "
                                  "separately from your presets."
                                : "When Luthier runs as a plugin, your host chooses the audio "
                                  "device, the sample rate, the buffer size and which MIDI inputs "
                                  "reach the track.\n\nChange them in your host's audio "
                                  "preferences. The standalone version has its own device "
                                  "settings in its toolbar.")
                .withButton ("OK"),
            nullptr);
    };

    styleNote (deviceNote, Palette::textMuted, 11.0f);
    addAndMakeVisible (deviceNote);

    styleNote (sidechainNote, Palette::textMuted, 11.0f);
    sidechainNote.setText ("The sidechain input is an input bus, so your host decides what feeds "
                           "it. Enable it in the host's routing, then pick what listens to it in "
                           "the Routing panel.",
                           juce::dontSendNotification);
    addAndMakeVisible (sidechainNote);

    styleNote (latencyLabel, Palette::textMuted, 11.0f);
    addAndMakeVisible (latencyLabel);

    refresh();
}

void AudioPage::refresh()
{
    const bool standalone =
        (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

    deviceNote.setText (standalone
                          ? "The output device, the sample rate and the buffer size belong to the "
                            "standalone wrapper, and are set from its own toolbar."
                          : "The output device, the sample rate and the buffer size belong to your "
                            "host. Luthier uses whatever the host gives it.",
                        juce::dontSendNotification);

    const double rate = processor.getSampleRate();
    const int latency = processor.getLatencySamples();

    latencyLabel.setText (
        rate > 0.0
          ? ("Running at " + juce::String (rate / 1000.0, 1) + " kHz, reporting "
               + juce::String (latency) + " samples of latency ("
               + juce::String (1000.0 * latency / rate, 2) + " ms) to the host.")
          : juce::String ("Not playing yet, so there is no sample rate to report."),
        juce::dontSendNotification);
}

void AudioPage::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawHeading (g, bounds.removeFromTop (18), "QUALITY");
    drawHeading (g, { 0, 96, getWidth(), 18 }, "DEVICE, RATE AND BUFFER");
    drawHeading (g, { 0, 214, getWidth(), 18 }, "SIDECHAIN");
}

void AudioPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (22);

    oversampling.setBounds (bounds.removeFromTop (40).removeFromLeft (200));

    bounds = getLocalBounds().withTrimmedTop (118);

    deviceNote.setBounds (bounds.removeFromTop (34));
    bounds.removeFromTop (4);
    deviceButton.setBounds (bounds.removeFromTop (Metrics::buttonHeight).removeFromLeft (240));
    bounds.removeFromTop (4);
    latencyLabel.setBounds (bounds.removeFromTop (18));

    bounds = getLocalBounds().withTrimmedTop (236);
    sidechainNote.setBounds (bounds.removeFromTop (48));
}

//==============================================================================
MidiPage::MidiPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    addAndMakeVisible (chordWindow);
    chordWindow.attachTo (processor, ParamIDs::chordWindow,
                          "How long Poly mode waits to collect a chord. Longer catches chords "
                          "split across buffers; shorter has less latency.");

    styleNote (portNote, Palette::textMuted, 11.0f);
    addAndMakeVisible (portNote);

    styleNote (outNote, Palette::textMuted, 11.0f);
    outNote.setText ("MIDI out is a plugin output bus rather than a virtual port: enable it in "
                     "your host and pick what it sends in the Routing panel. The standalone "
                     "build has no virtual port of its own.",
                     juce::dontSendNotification);
    addAndMakeVisible (outNote);

    styleNote (learnLabel, Palette::textMuted, 11.0f);
    addAndMakeVisible (learnLabel);

    addAndMakeVisible (clearLearnButton);
    clearLearnButton.setTooltip ("Forgets every CC you have taught Luthier. This cannot be undone.");
    clearLearnButton.onClick = [this]
    {
        processor.getMidiLearn().clearAllMappings();
        refresh();
    };

    refresh();
}

void MidiPage::refresh()
{
    const bool standalone =
        (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

    portNote.setText (standalone
                        ? "Which MIDI inputs are open belongs to the standalone wrapper, and is "
                          "set from its own toolbar."
                        : "Which MIDI reaches Luthier belongs to your host's track routing.",
                      juce::dontSendNotification);

    const int mappings = processor.getMidiLearn().getNumMappings();

    learnLabel.setText (mappings == 0
                          ? juce::String ("Nothing is mapped. Arm MIDI Learn from the header, or "
                                          "with Ctrl+L, then move a control.")
                          : (juce::String (mappings) + (mappings == 1 ? " control is" : " controls are")
                               + " mapped to a CC."),
                        juce::dontSendNotification);

    clearLearnButton.setEnabled (mappings > 0);
}

void MidiPage::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawHeading (g, bounds.removeFromTop (18), "HOW LUTHIER READS MIDI");
    drawHeading (g, { 0, 120, getWidth(), 18 }, "PORTS");
    drawHeading (g, { 0, 220, getWidth(), 18 }, "MIDI LEARN");
}

void MidiPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (22);

    chordWindow.setBounds (bounds.removeFromTop (
        LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal))
            .removeFromLeft (LuthierKnob::preferredWidthFor (LuthierKnob::Size::Normal)));

    bounds = getLocalBounds().withTrimmedTop (142);

    portNote.setBounds (bounds.removeFromTop (30));
    bounds.removeFromTop (4);
    outNote.setBounds (bounds.removeFromTop (48));

    bounds = getLocalBounds().withTrimmedTop (242);

    learnLabel.setBounds (bounds.removeFromTop (32));
    bounds.removeFromTop (4);
    clearLearnButton.setBounds (bounds.removeFromTop (Metrics::buttonHeight).removeFromLeft (220));
}

//==============================================================================
AppearancePage::AppearancePage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    for (int i = 0; i < (int) PaletteId::numPalettes; ++i)
        paletteBox.addItem (getPaletteName ((PaletteId) i), i + 1);

    paletteBox.onChange = [this]
    {
        if (updatingControls)
            return;

        AccessibilitySettings::get().setPalette ((PaletteId) (paletteBox.getSelectedId() - 1));
        AccessibilitySettings::get().save();

        refresh();
    };

    addAndMakeVisible (paletteBox);

    for (int i = 0; i < AccessibilitySettings::kNumScales; ++i)
        scaleBox.addItem (juce::String ((int) (AccessibilitySettings::kScales[(size_t) i] * 100.0))
                            + " %", i + 1);

    scaleBox.onChange = [this]
    {
        if (updatingControls)
            return;

        AccessibilitySettings::get().setUiScale (
            AccessibilitySettings::kScales[(size_t) juce::jlimit (
                0, AccessibilitySettings::kNumScales - 1, scaleBox.getSelectedId() - 1)]);

        AccessibilitySettings::get().save();
    };

    addAndMakeVisible (scaleBox);

    reducedMotionToggle.onClick = [this]
    {
        AccessibilitySettings::get().setReducedMotion (reducedMotionToggle.getToggleState());
        AccessibilitySettings::get().save();
    };

    addAndMakeVisible (reducedMotionToggle);

    tooltipsToggle.onClick = [this]
    {
        processor.getUiState().tooltipsEnabled = tooltipsToggle.getToggleState();
    };

    addAndMakeVisible (tooltipsToggle);

    styleNote (contrastLabel, Palette::textMuted);
    addAndMakeVisible (contrastLabel);

    /*  Section 5 also asks for an accent tint, a scrolling data-stream toggle and
        a noise-event strip toggle. None of the three has a setting behind it -
        the noise strip waits on pick-noise.md - and a switch that does nothing is
        worse than an absent one, so they are listed here rather than faked. */
    styleNote (pendingLabel, Palette::textDisabled);
    pendingLabel.setText ("Accent tint, the data-stream toggle and the noise-event strip toggle "
                          "are not built yet.",
                          juce::dontSendNotification);
    addAndMakeVisible (pendingLabel);

    refresh();
}

void AppearancePage::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    auto& settings = AccessibilitySettings::get();

    paletteBox.setSelectedId ((int) settings.getPalette() + 1, juce::dontSendNotification);
    reducedMotionToggle.setToggleState (settings.isReducedMotion(), juce::dontSendNotification);
    tooltipsToggle.setToggleState (processor.getUiState().tooltipsEnabled,
                                   juce::dontSendNotification);

    for (int i = 0; i < AccessibilitySettings::kNumScales; ++i)
        if (std::abs (AccessibilitySettings::kScales[(size_t) i] - settings.getUiScale()) < 1.0e-6)
            scaleBox.setSelectedId (i + 1, juce::dontSendNotification);

    // accessibility 10: the palette's contrast is shown, so a user editing a
    // theme file can see whether it still passes.
    const double contrast = settings.getColours().getWorstTextContrast();

    contrastLabel.setText ("Worst text contrast: " + juce::String (contrast, 2) + " to 1"
                             + (contrast >= 4.5 ? "  (meets WCAG AA)" : "  (below WCAG AA)"),
                           juce::dontSendNotification);

    contrastLabel.setColour (juce::Label::textColourId,
                             contrast >= 4.5 ? Palette::textMuted : Palette::warning);
}

void AppearancePage::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawHeading (g, bounds.removeFromTop (18), "THEME AND SIZE");
    drawHeading (g, { 0, 130, getWidth(), 18 }, "NOT BUILT YET");
}

void AppearancePage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (20);

    {
        auto row = bounds.removeFromTop (26);

        paletteBox.setBounds (row.removeFromLeft (180));
        row.removeFromLeft (8);
        scaleBox.setBounds (row.removeFromLeft (100));
    }

    bounds.removeFromTop (4);

    {
        auto row = bounds.removeFromTop (26);

        tooltipsToggle.setBounds (row.removeFromLeft (220));
        row.removeFromLeft (8);
        reducedMotionToggle.setBounds (row.removeFromLeft (160));
    }

    bounds.removeFromTop (4);
    contrastLabel.setBounds (bounds.removeFromTop (18));

    pendingLabel.setBounds (getLocalBounds().withTrimmedTop (150).withHeight (32));
}

//==============================================================================
AccessibilityPage::AccessibilityPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    setWantsKeyboardFocus (true);

    for (int i = 0; i < (int) AccessibilitySettings::Verbosity::numLevels; ++i)
        verbosityBox.addItem (AccessibilitySettings::getVerbosityName (
                                  (AccessibilitySettings::Verbosity) i), i + 1);

    verbosityBox.onChange = [this]
    {
        if (! updatingControls)
        {
            AccessibilitySettings::get().setVerbosity (
                (AccessibilitySettings::Verbosity) (verbosityBox.getSelectedId() - 1));

            AccessibilitySettings::get().save();
        }
    };

    addAndMakeVisible (verbosityBox);

    fontBox.addItem ("Theme default", 1);
    fontBox.addItem ("System default", 2);

    // Whatever the machine actually has, so the choice is real rather than
    // aspirational.
    {
        int itemId = 3;

        for (const auto& name : juce::Font::findAllTypefaceNames())
            fontBox.addItem (name, itemId++);
    }

    fontBox.onChange = [this]
    {
        if (updatingControls)
            return;

        const int id = fontBox.getSelectedId();

        AccessibilitySettings::get().setFontOverride (
            id <= 1 ? juce::String()
                    : (id == 2 ? juce::Font::getDefaultSansSerifFontName() : fontBox.getText()));

        AccessibilitySettings::get().save();
    };

    addAndMakeVisible (fontBox);

    // ---- shortcuts ------------------------------------------------------------------------
    shortcutList.setModel (&shortcutModel);
    shortcutList.setRowHeight (20);
    shortcutList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    addAndMakeVisible (shortcutList);

    searchBox.setTextToShowWhenEmpty ("Search shortcuts", Palette::textDisabled);
    searchBox.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    searchBox.onTextChange = [this] { rebuildShortcutList(); };
    addAndMakeVisible (searchBox);

    resetAllButton.onClick = [this]
    {
        AccessibilitySettings::get().resetAllShortcuts();
        AccessibilitySettings::get().save();
        rebuildShortcutList();
    };

    addAndMakeVisible (resetAllButton);

    styleNote (rebindHint);
    addAndMakeVisible (rebindHint);

    rebuildShortcutList();
    refresh();
}

void AccessibilityPage::rebuildShortcutList()
{
    visibleShortcuts.clearQuick();

    const auto& shortcuts = AccessibilitySettings::get().getShortcuts();
    const auto query = searchBox.getText().trim();

    for (int i = 0; i < (int) shortcuts.size(); ++i)
    {
        const auto& binding = shortcuts[(size_t) i];

        const auto description = tr (binding.descriptionKey);

        if (query.isEmpty()
              || binding.id.containsIgnoreCase (query)
              || description.containsIgnoreCase (query)
              || binding.key.getTextDescription().containsIgnoreCase (query))
            visibleShortcuts.add (i);
    }

    shortcutList.updateContent();
    shortcutList.repaint();
}

int AccessibilityPage::ShortcutModel::getNumRows()
{
    return owner.visibleShortcuts.size();
}

void AccessibilityPage::ShortcutModel::paintListBoxItem (int row, juce::Graphics& g,
                                                         int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, owner.visibleShortcuts.size()))
        return;

    const auto& shortcuts = AccessibilitySettings::get().getShortcuts();
    const int index = owner.visibleShortcuts[row];

    if (! juce::isPositiveAndBelow (index, (int) shortcuts.size()))
        return;

    const auto& binding = shortcuts[(size_t) index];

    const bool capturing = owner.capturingRow == row;

    if (selected || capturing)
    {
        g.setColour (capturing ? Palette::accent : Palette::accentDim);
        g.fillRect (0, 0, width, height);
    }

    g.setColour (capturing ? Palette::backgroundDeep : Palette::textMuted);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    g.drawText (tr (binding.descriptionKey), 6, 0, width - 140, height,
                juce::Justification::centredLeft, true);

    g.setColour (capturing ? Palette::backgroundDeep
                           : (binding.isRebound() ? Palette::accent : Palette::textDisabled));

    g.drawText (capturing ? "press a key..." : binding.key.getTextDescription(),
                width - 134, 0, 128, height, juce::Justification::centredRight, true);
}

void AccessibilityPage::ShortcutModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    owner.capturingRow = (owner.capturingRow == row) ? -1 : row;

    owner.rebindHint.setText (owner.capturingRow >= 0
                                ? juce::String ("Press the key you want. Escape cancels.")
                                : juce::String(),
                              juce::dontSendNotification);

    owner.grabKeyboardFocus();
    owner.shortcutList.repaint();
}

bool AccessibilityPage::keyPressed (const juce::KeyPress& key)
{
    if (capturingRow < 0)
        return false;

    if (key == juce::KeyPress::escapeKey)
    {
        capturingRow = -1;
        rebindHint.setText ({}, juce::dontSendNotification);
        shortcutList.repaint();
        return true;
    }

    const auto& shortcuts = AccessibilitySettings::get().getShortcuts();

    if (! juce::isPositiveAndBelow (capturingRow, visibleShortcuts.size()))
        return false;

    const int index = visibleShortcuts[capturingRow];

    if (! juce::isPositiveAndBelow (index, (int) shortcuts.size()))
        return false;

    const auto id = shortcuts[(size_t) index].id;

    // accessibility 2: a clash is shown rather than silently overwriting.
    if (AccessibilitySettings::get().rebind (id, key))
    {
        AccessibilitySettings::get().save();
        rebindHint.setText ({}, juce::dontSendNotification);
        capturingRow = -1;
    }
    else
    {
        const auto clash = AccessibilitySettings::get().findAction (key);

        rebindHint.setText (key.getTextDescription() + " is already used by "
                              + clash + ". Pick another.",
                            juce::dontSendNotification);
    }

    rebuildShortcutList();
    return true;
}

void AccessibilityPage::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    auto& settings = AccessibilitySettings::get();

    verbosityBox.setSelectedId ((int) settings.getVerbosity() + 1, juce::dontSendNotification);
}

void AccessibilityPage::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawHeading (g, bounds.removeFromTop (18), "SCREEN READER AND TEXT");
    drawHeading (g, { 0, 76, getWidth(), 18 }, "KEYBOARD SHORTCUTS");
}

void AccessibilityPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (20);

    {
        auto row = bounds.removeFromTop (26);

        verbosityBox.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (8);
        fontBox.setBounds (row.removeFromLeft (220));
    }

    // ---- shortcuts ---------------------------------------------------------------------
    bounds = getLocalBounds().withTrimmedTop (96);

    {
        auto row = bounds.removeFromTop (26);

        searchBox.setBounds (row.removeFromLeft (200));
        row.removeFromLeft (8);
        resetAllButton.setBounds (row.removeFromLeft (150));
    }

    bounds.removeFromTop (2);
    rebindHint.setBounds (bounds.removeFromBottom (18));
    shortcutList.setBounds (bounds);
}

//==============================================================================
LocalizationPage::LocalizationPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    {
        int itemId = 1;

        for (const auto& locale : Localisation::getShipLocales())
        {
            localeBox.addItem (locale.englishName + "  (" + locale.nativeName + ")", itemId);
            fallbackBox.addItem (locale.englishName, itemId);
            ++itemId;
        }
    }

    localeBox.onChange = [this]
    {
        if (updatingControls)
            return;

        const int index = localeBox.getSelectedId() - 1;
        const auto& locales = Localisation::getShipLocales();

        if (juce::isPositiveAndBelow (index, (int) locales.size()))
        {
            const bool loaded = Localisation::get().setLocale (locales[(size_t) index].code);

            // accessibility 6: a locale with no catalog falls back rather than
            // showing keys, and the user is told rather than left guessing.
            localeNote.setText (loaded ? juce::String()
                                       : ("No catalog for that language yet; "
                                          "English is being used."),
                                juce::dontSendNotification);

            AccessibilitySettings::get().save();
        }

        refresh();
    };

    fallbackBox.onChange = [this]
    {
        if (updatingControls)
            return;

        const int index = fallbackBox.getSelectedId() - 1;
        const auto& locales = Localisation::getShipLocales();

        if (juce::isPositiveAndBelow (index, (int) locales.size()))
        {
            Localisation::get().setFallbackLocale (locales[(size_t) index].code);
            AccessibilitySettings::get().save();
        }
    };

    addAndMakeVisible (localeBox);
    addAndMakeVisible (fallbackBox);

    catalogButton.setTooltip ("Point Luthier at your own folder of translation files.");
    catalogButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Choose a string catalog folder", Localisation::getCatalogDirectory());

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
                Localisation::get().setCustomCatalogDirectory (fc.getResult());

            refresh();
        });
    };

    addAndMakeVisible (catalogButton);

    styleNote (localeNote, Palette::warning);
    addAndMakeVisible (localeNote);

    styleNote (catalogLabel, Palette::textMuted);
    addAndMakeVisible (catalogLabel);

    refresh();
}

void LocalizationPage::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    const auto& locales = Localisation::getShipLocales();
    const auto current = Localisation::get().getLocale();
    const auto fallback = Localisation::get().getFallbackLocale();

    for (int i = 0; i < (int) locales.size(); ++i)
    {
        if (locales[(size_t) i].code == current)
            localeBox.setSelectedId (i + 1, juce::dontSendNotification);

        if (locales[(size_t) i].code == fallback)
            fallbackBox.setSelectedId (i + 1, juce::dontSendNotification);
    }

    catalogLabel.setText ("Catalogs are read from "
                            + Localisation::getCatalogDirectory().getFullPathName(),
                          juce::dontSendNotification);
}

void LocalizationPage::paint (juce::Graphics& g)
{
    drawHeading (g, getLocalBounds().removeFromTop (18), "LANGUAGE");
}

void LocalizationPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (20);

    {
        auto row = bounds.removeFromTop (26);

        localeBox.setBounds (row.removeFromLeft (220));
        row.removeFromLeft (8);
        fallbackBox.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (8);
        catalogButton.setBounds (row.removeFromLeft (140));
    }

    bounds.removeFromTop (2);
    localeNote.setBounds (bounds.removeFromTop (18));
    catalogLabel.setBounds (bounds.removeFromTop (18));
}

//==============================================================================
//==============================================================================
//  RangesPage
//==============================================================================
namespace
{
    /*  One row of the out-of-stock summary: name, value, stock range, and the
        button that clamps that one parameter (advanced-ranges.md 6.2 item 4). */
    class RangeSummaryRow : public juce::Component
    {
    public:
        RangeSummaryRow()
        {
            clampButton.onClick = [this] { if (onClamp) onClamp (parameterId); };
            addAndMakeVisible (clampButton);
        }

        void setRow (const juce::String& id, const juce::String& name,
                     const juce::String& value, const juce::String& stock)
        {
            parameterId = id;
            nameText = name;
            valueText = value;
            stockText = stock;

            clampButton.setTitle ("Clamp " + name + " to stock");
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().withTrimmedRight (clampButton.getWidth() + 8);

            g.setFont (Fonts::ui (12.0f));
            g.setColour (Palette::textPrimary);
            g.drawText (nameText, bounds.removeFromLeft (bounds.getWidth() * 2 / 5),
                        juce::Justification::centredLeft, true);

            g.setFont (Fonts::mono (12.0f));
            g.setColour (Palette::warning);
            g.drawText (valueText, bounds.removeFromLeft (bounds.getWidth() / 2),
                        juce::Justification::centredLeft, true);

            g.setColour (Palette::textMuted);
            g.drawText (stockText, bounds, juce::Justification::centredLeft, true);
        }

        void resized() override
        {
            clampButton.setBounds (getLocalBounds().removeFromRight (70).reduced (0, 2));
        }

        std::function<void (const juce::String&)> onClamp;

    private:
        juce::String parameterId, nameText, valueText, stockText;
        juce::TextButton clampButton { "Clamp" };
    };
}

RangesPage::RangesPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    masterToggle.onClick = [this] { if (! updatingControls) masterToggled(); };
    addAndMakeVisible (masterToggle);

    warningToggle.onClick = [this]
    {
        if (updatingControls)
            return;

        RangesUi::setMarkInWarningColour (warningToggle.getToggleState());

        // Every knob on screen has to redraw its arc in the new colour.
        if (auto* top = getTopLevelComponent())
            top->repaint();
    };
    addAndMakeVisible (warningToggle);

    randomiseToggle.onClick = [this]
    {
        if (! updatingControls)
            RangesUi::setRandomiseRespectsStock (processor, randomiseToggle.getToggleState());
    };
    addAndMakeVisible (randomiseToggle);

    styleNote (masterNote, Palette::textMuted, 11.0f);
    addAndMakeVisible (masterNote);

    styleNote (emptyNote, Palette::textMuted, 11.0f);
    emptyNote.setText ("Every value in this preset is inside its stock range.",
                       juce::dontSendNotification);
    addAndMakeVisible (emptyNote);

    summary.setModel (&summaryModel);
    summary.setRowHeight (26);
    summary.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    summary.setTitle ("Values outside their stock range");
    addAndMakeVisible (summary);

    refresh();
}

int RangesPage::setAllFamilies (bool advanced)
{
    RangeState next;

    for (int i = 0; i < (int) RangeFamily::numFamilies; ++i)
        next.setFamilyAdvanced ((RangeFamily) i, advanced);

    // Per-control unlocks are subsumed by an unlocked family and are exactly
    // what a lock is meant to take away, so the master toggle clears them.
    const int clamped = RangesUi::apply (processor, next,
                                         advanced ? "Unlock all ranges" : "Lock all ranges",
                                         &masterToggle);
    refresh();
    return clamped;
}

void RangesPage::masterToggled()
{
    const bool wantAdvanced = masterToggle.getToggleState();

    if (wantAdvanced)
    {
        setAllFamilies (true);
        return;
    }

    // 6.2 item 1: locking shows the clamp count before it commits.
    const int wouldClamp = processor.getRanges().findValuesOutsideStock (processor.getState()).size();

    if (wouldClamp == 0)
    {
        setAllFamilies (false);
        return;
    }

    {
        const juce::ScopedValueSetter<bool> guard (updatingControls, true);
        masterToggle.setToggleState (true, juce::dontSendNotification);
    }

    const auto message = juce::String (wouldClamp)
                       + (wouldClamp == 1 ? " value is" : " values are")
                       + " outside the stock range and will be moved to the nearest stock value."
                         " Undo brings them back.";

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                                    .withTitle ("Lock to stock ranges?")
                                    .withMessage (message)
                                    .withButton ("Lock and clamp")
                                    .withButton ("Cancel")
                                    .withAssociatedComponent (this),
                                  [safeThis = juce::Component::SafePointer<RangesPage> (this)] (int result)
    {
        if (safeThis != nullptr && result == 1)
            safeThis->setAllFamilies (false);
    });
}

void RangesPage::clampOne (const juce::String& parameterId)
{
    const auto* physical = RangeRegistry::find (parameterId);
    auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (processor.getState().getParameter (parameterId));

    if (physical == nullptr || parameter == nullptr)
        return;

    // action-and-undo.md 0.1: one entry - the gesture below must not push a second.
    const LuthierAudioProcessor::ScopedUndoAction undo (processor, "Clamp " + parameter->getName (40) + " to stock");

    const float clamped = juce::jlimit (physical->stockMin, physical->stockMax, parameter->get());

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (clamped));
    parameter->endChangeGesture();

    refresh();
}

void RangesPage::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    const auto& ranges = processor.getRanges();

    int unlockedFamilies = 0;

    for (int i = 0; i < (int) RangeFamily::numFamilies; ++i)
        if (ranges.isFamilyAdvanced ((RangeFamily) i))
            ++unlockedFamilies;

    const bool allUnlocked = unlockedFamilies == (int) RangeFamily::numFamilies;

    masterToggle.setToggleState (allUnlocked, juce::dontSendNotification);
    warningToggle.setToggleState (RangesUi::markInWarningColour(), juce::dontSendNotification);
    randomiseToggle.setToggleState (RangesUi::randomiseRespectsStock(), juce::dontSendNotification);

    juce::String note;

    if (allUnlocked)
        note = "Every control in this preset can use its advanced range.";
    else if (ranges.isAnythingAdvanced())
        note = "Some controls in this preset are unlocked: "
             + juce::String (unlockedFamilies) + " of "
             + juce::String ((int) RangeFamily::numFamilies) + " families, plus any unlocked one at a time.";
    else
        note = "Stock: every control is limited to what a real guitar can do.";

    masterNote.setText (note, juce::dontSendNotification);

    const auto nowOutside = ranges.findValuesOutsideStock (processor.getState());

    if (nowOutside != outside)
    {
        outside = nowOutside;
        summary.updateContent();
    }

    summary.repaint();
    emptyNote.setVisible (outside.isEmpty());
}

void RangesPage::paint (juce::Graphics& g)
{
    drawHeading (g, getLocalBounds().removeFromTop (18), "RANGES");
    drawHeading (g, { 0, 140, getWidth(), 18 }, "OUTSIDE STOCK");
}

void RangesPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (20);
    masterToggle.setBounds (bounds.removeFromTop (24).removeFromLeft (320));
    masterNote.setBounds (bounds.removeFromTop (18));

    bounds.removeFromTop (8);
    warningToggle.setBounds (bounds.removeFromTop (24).removeFromLeft (360));
    randomiseToggle.setBounds (bounds.removeFromTop (24).removeFromLeft (360));

    bounds = getLocalBounds().withTrimmedTop (160);
    emptyNote.setBounds (bounds.removeFromTop (20));
    summary.setBounds (getLocalBounds().withTrimmedTop (160));
}

int RangesPage::SummaryModel::getNumRows()
{
    return owner.outside.size();
}

juce::Component* RangesPage::SummaryModel::refreshComponentForRow (int row, bool,
                                                                   juce::Component* existing)
{
    if (! juce::isPositiveAndBelow (row, owner.outside.size()))
    {
        delete existing;
        return nullptr;
    }

    auto* rowComponent = dynamic_cast<RangeSummaryRow*> (existing);

    if (rowComponent == nullptr)
    {
        delete existing;
        rowComponent = new RangeSummaryRow();
    }

    const auto& id = owner.outside[row];
    const auto* physical = RangeRegistry::find (id);
    auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (owner.processor.getState().getParameter (id));

    if (physical == nullptr || parameter == nullptr)
        return rowComponent;

    rowComponent->setRow (id, parameter->getName (40),
                          RangesUi::formatValue (owner.processor, id, parameter->get()) + "*",
                          "stock " + RangesUi::formatValue (owner.processor, id, physical->stockMin)
                            + " - " + RangesUi::formatValue (owner.processor, id, physical->stockMax));

    rowComponent->onClamp = [this] (const juce::String& parameterId) { owner.clampOne (parameterId); };
    return rowComponent;
}

//==============================================================================
UpdatesPage::UpdatesPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    auto wire = [this] (juce::ToggleButton& toggle, std::function<void (bool)> setter)
    {
        toggle.onClick = [this, &toggle, setter]
        {
            if (updatingControls)
                return;

            setter (toggle.getToggleState());
            telemetry().saveSettings();
            refresh();
        };

        addAndMakeVisible (toggle);
    };

    wire (updateCheckToggle, [this] (bool on) { telemetry().setUpdateCheckEnabled (on); });
    wire (betaToggle,        [this] (bool on) { telemetry().setBetaChannelEnabled (on); });

    checkNowButton.onClick = [this] { checkForUpdate(); };
    addAndMakeVisible (checkNowButton);

    styleNote (updateStatus, Palette::textMuted);
    addAndMakeVisible (updateStatus);

    styleNote (policyLabel, Palette::warning);
    addAndMakeVisible (policyLabel);

    /*  Section 5 asks for a changelog viewer. There is no changelog endpoint in
        the manifest, so what a check returns is the release note the manifest
        carries, and that is what this shows. */
    styleNote (changelogNote, Palette::textDisabled);
    changelogNote.setText ("What the last check returned:", juce::dontSendNotification);
    addAndMakeVisible (changelogNote);

    releaseNotes.setMultiLine (true);
    releaseNotes.setReadOnly (true);
    releaseNotes.setScrollbarsShown (true);
    releaseNotes.setCaretVisible (false);
    releaseNotes.setFont (Fonts::ui (11.0f));
    releaseNotes.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    addAndMakeVisible (releaseNotes);

    refresh();
}

Telemetry& UpdatesPage::telemetry()
{
    return processor.getTelemetry();
}

void UpdatesPage::checkForUpdate()
{
    const auto running = Version::parse (JucePlugin_VersionString);

    // updates-telemetry 0.4: never on the audio thread, and never blocking the
    // message thread either - the check goes to a background job.
    updateStatus.setText ("Checking...", juce::dontSendNotification);

    juce::Thread::launch ([this, running]
    {
        const auto result = telemetry().checkForUpdate (running, true);

        juce::MessageManager::callAsync ([this, result]
        {
            if (! result.checked)
            {
                // A failed check is reported here because the user asked for it.
                // It is silent everywhere else (updates-telemetry 8).
                updateStatus.setText (result.error.isNotEmpty()
                                        ? result.error
                                        : juce::String ("Could not check for updates."),
                                      juce::dontSendNotification);
                return;
            }

            updateStatus.setText (result.updateAvailable
                                    ? ("Version " + result.available.toString() + " is available.")
                                    : juce::String ("Luthier is up to date."),
                                  juce::dontSendNotification);

            /*  The manifest carries links rather than the text of a changelog,
                so this shows what it actually returned. Fetching and rendering
                the changelog itself would be a second network call that
                updates-telemetry 8 does not ask for. */
            juce::StringArray details;

            details.add ("Latest version:  " + result.available.toString());

            if (result.changelogUrl.isNotEmpty())
                details.add ("Changelog:       " + result.changelogUrl);

            if (result.downloadUrl.isNotEmpty())
                details.add ("Download:        " + result.downloadUrl);

            releaseNotes.setText (details.joinIntoString ("\n"), false);
        });
    });
}

void UpdatesPage::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    auto& t = telemetry();

    updateCheckToggle.setToggleState (t.isUpdateCheckEnabled(), juce::dontSendNotification);
    betaToggle.setToggleState (t.isBetaChannelEnabled(), juce::dontSendNotification);
    betaToggle.setEnabled (t.isUpdateCheckEnabled());

    const auto& policy = t.getPolicy();

    policyLabel.setText (policy.present && ! policy.allowUpdateCheck
                           ? juce::String ("Update checks are switched off by your administrator.")
                           : juce::String(),
                         juce::dontSendNotification);

    updateCheckToggle.setEnabled (policy.allowUpdateCheck);
}

void UpdatesPage::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawHeading (g, bounds.removeFromTop (18), "UPDATES");
    drawHeading (g, { 0, 110, getWidth(), 18 }, "WHAT IS NEW");
}

void UpdatesPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (20);

    {
        auto row = bounds.removeFromTop (24);

        updateCheckToggle.setBounds (row.removeFromLeft (240));
        betaToggle.setBounds (row.removeFromLeft (180));
        checkNowButton.setBounds (row.removeFromLeft (100));
    }

    bounds.removeFromTop (2);
    updateStatus.setBounds (bounds.removeFromTop (18));
    policyLabel.setBounds (bounds.removeFromTop (18));

    bounds = getLocalBounds().withTrimmedTop (130);

    changelogNote.setBounds (bounds.removeFromTop (16));
    bounds.removeFromTop (2);
    releaseNotes.setBounds (bounds);
}

//==============================================================================
PrivacyPage::PrivacyPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    auto wire = [this] (juce::ToggleButton& toggle, std::function<void (bool)> setter)
    {
        toggle.onClick = [this, &toggle, setter]
        {
            if (updatingControls)
                return;

            setter (toggle.getToggleState());
            telemetry().saveSettings();
            refresh();
        };

        addAndMakeVisible (toggle);
    };

    wire (usageToggle, [this] (bool on)
    {
        telemetry().setCategoryEnabled (Telemetry::Category::usage, on);
    });

    wire (diagnosticsToggle, [this] (bool on)
    {
        telemetry().setCategoryEnabled (Telemetry::Category::diagnostics, on);
    });

    wire (crashToggle, [this] (bool on) { telemetry().setCrashUploadEnabled (on); });

    // updates-telemetry 6: each category explained in plain English.
    styleNote (usageExplanation);
    usageExplanation.setText ("Which panels you open, which presets you load, how hard "
                              "the DSP is working, and which features are used. No preset "
                              "names, no file names, nothing that identifies you.",
                              juce::dontSendNotification);

    styleNote (diagnosticsExplanation);
    diagnosticsExplanation.setText ("Errors and warnings the plugin catches, plus your "
                                    "host's name, sample rate, buffer size and plugin "
                                    "format. Nothing that identifies you.",
                                    juce::dontSendNotification);

    styleNote (crashExplanation);
    crashExplanation.setText ("If Luthier crashes, offer to send the dump next time it "
                              "starts. You see exactly what would be sent first. Dumps "
                              "never contain audio, MIDI or preset data.",
                              juce::dontSendNotification);

    for (auto* label : { &usageExplanation, &diagnosticsExplanation, &crashExplanation })
        addAndMakeVisible (*label);

    viewLogButton.setTooltip ("Everything that has been sent, and everything that would "
                              "be, in the order it happened.");

    viewLogButton.onClick = [this]
    {
        logView.setText (telemetry().readNetworkLog().joinIntoString ("\n"), false);
        logView.moveCaretToEnd();
    };

    clearLogsButton.onClick = [this]
    {
        telemetry().clearLocalLogs();
        logView.clear();
        refresh();
    };

    paranoiaButton.setColour (juce::TextButton::buttonColourId, Palette::clip.withAlpha (0.4f));
    paranoiaButton.setTooltip ("Switches off every telemetry, crash-report and update "
                               "option, and deletes every diagnostic file on disk.");

    paranoiaButton.onClick = [this]
    {
        telemetry().turnEverythingOffAndDelete();
        logView.clear();
        refresh();
    };

    for (auto* button : { &viewLogButton, &clearLogsButton, &paranoiaButton })
        addAndMakeVisible (*button);

    logView.setMultiLine (true);
    logView.setReadOnly (true);
    logView.setScrollbarsShown (true);
    logView.setCaretVisible (false);
    logView.setFont (Fonts::mono (10.5f));
    logView.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    addAndMakeVisible (logView);

    auto wireUrl = [this] (juce::TextEditor& box, std::function<void (juce::String)> setter)
    {
        box.setFont (Fonts::mono (10.5f));
        box.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);

        box.onFocusLost = [this, &box, setter]
        {
            setter (box.getText().trim());
            telemetry().saveSettings();
            refresh();
        };

        box.onReturnKey = [this, &box, setter]
        {
            setter (box.getText().trim());
            telemetry().saveSettings();
            refresh();
        };

        addAndMakeVisible (box);
    };

    wireUrl (manifestUrlBox,  [this] (const juce::String& u) { telemetry().setManifestUrl (u); });
    wireUrl (telemetryUrlBox, [this] (const juce::String& u) { telemetry().setTelemetryUrl (u); });
    wireUrl (crashUrlBox,     [this] (const juce::String& u) { telemetry().setCrashUploadUrl (u); });

    styleNote (endpointsHeading, Palette::textDisabled, 9.0f);
    endpointsHeading.setText ("Endpoints, for a deployment that routes through a local proxy:",
                              juce::dontSendNotification);
    addAndMakeVisible (endpointsHeading);

    styleNote (policyLabel, Palette::warning);
    addAndMakeVisible (policyLabel);

    refresh();
}

Telemetry& PrivacyPage::telemetry()
{
    return processor.getTelemetry();
}

void PrivacyPage::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    auto& t = telemetry();

    usageToggle.setToggleState (t.isCategoryEnabled (Telemetry::Category::usage),
                                juce::dontSendNotification);
    diagnosticsToggle.setToggleState (t.isCategoryEnabled (Telemetry::Category::diagnostics),
                                      juce::dontSendNotification);
    crashToggle.setToggleState (t.isCrashUploadEnabled(), juce::dontSendNotification);

    manifestUrlBox.setText (t.getManifestUrl(), false);
    telemetryUrlBox.setText (t.getTelemetryUrl(), false);
    crashUrlBox.setText (t.getCrashUploadUrl(), false);

    // updates-telemetry 7: say so when a policy is in force, and disable what it
    // has taken away rather than letting the user tick a box that does nothing.
    const auto& policy = t.getPolicy();

    policyLabel.setText (policy.present
                           ? juce::String ("Managed by policy. Some options are set by "
                                           "your administrator and cannot be changed here.")
                           : juce::String(),
                         juce::dontSendNotification);

    usageToggle.setEnabled (policy.allowUsageTelemetry);
    diagnosticsToggle.setEnabled (policy.allowDiagnosticsTelemetry);
    crashToggle.setEnabled (policy.allowCrashUpload);

    manifestUrlBox.setEnabled (policy.updateManifestUrl.isEmpty());
    telemetryUrlBox.setEnabled (policy.telemetryUrl.isEmpty());
    crashUrlBox.setEnabled (policy.crashUploadUrl.isEmpty());
}

void PrivacyPage::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawHeading (g, bounds.removeFromTop (18), "WHAT LUTHIER WOULD SEND");
    drawHeading (g, { 0, 210, getWidth(), 18 }, "LOCAL LOG");
}

void PrivacyPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (20);

    usageToggle.setBounds (bounds.removeFromTop (22));
    usageExplanation.setBounds (bounds.removeFromTop (34));

    diagnosticsToggle.setBounds (bounds.removeFromTop (22));
    diagnosticsExplanation.setBounds (bounds.removeFromTop (34));

    crashToggle.setBounds (bounds.removeFromTop (22));
    crashExplanation.setBounds (bounds.removeFromTop (34));

    policyLabel.setBounds (bounds.removeFromTop (18));

    // ---- log and endpoints ------------------------------------------------------------
    bounds = getLocalBounds().withTrimmedTop (230);

    {
        auto row = bounds.removeFromTop (26);

        viewLogButton.setBounds (row.removeFromLeft (130));
        row.removeFromLeft (6);
        clearLogsButton.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (6);
        paranoiaButton.setBounds (row);
    }

    bounds.removeFromTop (4);

    auto endpoints = bounds.removeFromBottom (78);

    logView.setBounds (bounds);

    endpointsHeading.setBounds (endpoints.removeFromTop (14));

    manifestUrlBox.setBounds (endpoints.removeFromTop (20).reduced (0, 1));
    telemetryUrlBox.setBounds (endpoints.removeFromTop (20).reduced (0, 1));
    crashUrlBox.setBounds (endpoints.removeFromTop (20).reduced (0, 1));
}

//==============================================================================
DiagnosticsPage::DiagnosticsPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    addAndMakeVisible (debugWindowButton);
    debugWindowButton.setTooltip ("The live state and data-stream view (Ctrl+D)");
    debugWindowButton.onClick = [this]
    {
        if (onShowDebugWindow != nullptr)
            onShowDebugWindow();
    };

    addAndMakeVisible (crashLogToggle);
    crashLogToggle.setTooltip ("Off on every load. Turning it on also turns on the live data stream.");
    crashLogToggle.onClick = [this]
    {
        processor.getDiagnostics().setCrashLogEnabled (crashLogToggle.getToggleState());
        refresh();
    };

    addAndMakeVisible (recorderToggle);
    recorderToggle.setTooltip ("practice-tools 8: keeps the last hour so you can save a take "
                               "you did not know you wanted.");
    recorderToggle.onClick = [this]
    {
        processor.getSessionRecorder().setEnabled (recorderToggle.getToggleState());
        refresh();
    };

    addAndMakeVisible (troubleshootButton);
    troubleshootButton.setTooltip ("Writes a file describing the build, the host and the "
                                   "current state, for a support thread.");
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
                .withTitle (file != juce::File() ? "Troubleshooting file written"
                                                 : "Could not write the file")
                .withMessage (file != juce::File()
                                ? ("Written to\n" + file.getFullPathName())
                                : juce::String ("The diagnostics folder could not be written to."))
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
            [this] (int result)
            {
                if (result == 1)
                {
                    processor.hardResetAndClearCaches();
                    refresh();
                }
            });
    };

    styleNote (explanation, Palette::textMuted, 11.0f);
    explanation.setText ("Everything here is off until you switch it on, and everything it "
                         "writes stays on this machine until you send it somewhere.",
                         juce::dontSendNotification);
    addAndMakeVisible (explanation);

    styleNote (recorderNote);
    addAndMakeVisible (recorderNote);

    /*  Section 5 wants the Workshop, Slide and advanced-ranges booleans mirrored
        here, so a user can see what telemetry would report. None of the three
        exists, so there is nothing to mirror and saying so beats three faked
        rows that always read false. */
    styleNote (mirrorNote, Palette::textDisabled);
    mirrorNote.setText ("The Workshop, Slide and advanced-ranges feature flags are not built "
                        "yet, so there is nothing to mirror here.",
                        juce::dontSendNotification);
    addAndMakeVisible (mirrorNote);

    refresh();
}

void DiagnosticsPage::refresh()
{
    crashLogToggle.setToggleState (processor.getDiagnostics().isCrashLogEnabled(),
                                   juce::dontSendNotification);

    auto& recorder = processor.getSessionRecorder();

    recorderToggle.setToggleState (recorder.isEnabled(), juce::dontSendNotification);

    recorderNote.setText (
        "Buffer: " + juce::String (recorder.getCapacityMinutes(), 1) + " minutes allocated, "
          + juce::String (recorder.getRecordedSamples() > 0
                            ? juce::String (recorder.getRecordedSamples()) + " samples held"
                            : juce::String ("nothing recorded yet")),
        juce::dontSendNotification);
}

void DiagnosticsPage::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawHeading (g, bounds.removeFromTop (18), "WHAT LUTHIER RECORDS FOR YOU");
    drawHeading (g, { 0, 150, getWidth(), 18 }, "FILES AND WINDOWS");
    drawHeading (g, { 0, 262, getWidth(), 18 }, "FEATURE FLAGS");
}

void DiagnosticsPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (20);

    explanation.setBounds (bounds.removeFromTop (30));
    bounds.removeFromTop (4);

    crashLogToggle.setBounds (bounds.removeFromTop (22));
    recorderToggle.setBounds (bounds.removeFromTop (22));
    recorderNote.setBounds (bounds.removeFromTop (16));

    bounds = getLocalBounds().withTrimmedTop (172);

    {
        auto row = bounds.removeFromTop (Metrics::buttonHeight);

        debugWindowButton.setBounds (row.removeFromLeft (200));
        row.removeFromLeft (6);
        openFolderButton.setBounds (row.removeFromLeft (200));
    }

    bounds.removeFromTop (6);

    {
        auto row = bounds.removeFromTop (Metrics::buttonHeight);

        troubleshootButton.setBounds (row.removeFromLeft (220));
        row.removeFromLeft (6);
        hardResetButton.setBounds (row.removeFromLeft (280));
    }

    mirrorNote.setBounds (getLocalBounds().withTrimmedTop (284).withHeight (32));
}

//==============================================================================
FileLocationsPage::FileLocationsPage (LuthierAudioProcessor& p)
    : OptionsPage (p)
{
    addAndMakeVisible (openUserFolder);
    openUserFolder.onClick = [] { PresetManager::getUserPresetFolder().revealToUser(); };

    addAndMakeVisible (openRenderFolder);
    openRenderFolder.onClick = [] { PresetManager::getRenderFolder().revealToUser(); };

    addAndMakeVisible (openFactoryFolder);
    openFactoryFolder.onClick = [] { PresetManager::getFactoryPresetFolder().revealToUser(); };

    addAndMakeVisible (openDiagnosticsFolder);
    openDiagnosticsFolder.onClick = [] { Diagnostics::getDiagnosticsFolder().revealToUser(); };

    addAndMakeVisible (addFolderButton);
    addFolderButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a folder to scan for presets",
                                                       PresetManager::getUserPresetFolder());

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
        {
            const auto folder = fc.getResult();

            if (folder.isDirectory())
            {
                processor.getPresetManager().addSearchFolder (folder);
                folderList.updateContent();
            }
        });
    };

    addAndMakeVisible (rescanButton);
    rescanButton.onClick = [this]
    {
        processor.getPresetManager().refresh();
        folderList.updateContent();
    };

    addAndMakeVisible (pathLabel);
    pathLabel.setFont (Fonts::ui (11.0f));
    pathLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    pathLabel.setJustificationType (juce::Justification::topLeft);

    addAndMakeVisible (formatNote);
    formatNote.setFont (Fonts::ui (11.0f));
    formatNote.setColour (juce::Label::textColourId, Palette::textMuted);
    formatNote.setJustificationType (juce::Justification::topLeft);
    formatNote.setText ("Presets are plain JSON files with the extension .luthierpreset. The "
                        "folder a preset sits in becomes its category. If a preset does not "
                        "appear, press Rescan; if it still does not, check that it is in one of "
                        "the folders listed here and that its extension is exactly right.",
                        juce::dontSendNotification);

    addAndMakeVisible (folderList);
    folderList.setModel (&folderModel);
    folderList.setRowHeight (22);
    folderList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);

    refresh();
}

int FileLocationsPage::FolderListModel::getNumRows()
{
    return owner.processor.getPresetManager().getSearchFolders().size();
}

void FileLocationsPage::FolderListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                           int width, int height, bool selected)
{
    const auto folders = owner.processor.getPresetManager().getSearchFolders();

    if (! juce::isPositiveAndBelow (row, folders.size()))
        return;

    if (selected)
    {
        g.setColour (Palette::accent.withAlpha (0.14f));
        g.fillRect (0, 0, width, height);
    }

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::mono (11.0f));
    g.drawText (folders[row].getFullPathName(), 8, 0, width - 12, height,
                juce::Justification::centredLeft, true);
}

void FileLocationsPage::refresh()
{
    pathLabel.setText (
        "User presets   " + PresetManager::getUserPresetFolder().getFullPathName() + "\n"
        "Factory        " + PresetManager::getFactoryPresetFolder().getFullPathName() + "\n"
        "Renders        " + PresetManager::getRenderFolder().getFullPathName() + "\n"
        "Diagnostics    " + Diagnostics::getDiagnosticsFolder().getFullPathName(),
        juce::dontSendNotification);

    folderList.updateContent();
}

void FileLocationsPage::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawHeading (g, bounds.removeFromTop (18), "WHERE LUTHIER KEEPS THINGS");
    drawHeading (g, { 0, 178, getWidth(), 18 }, "PRESET SEARCH PATH");
}

void FileLocationsPage::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop (20);

    pathLabel.setBounds (bounds.removeFromTop (72));
    bounds.removeFromTop (Metrics::gridHalf);

    {
        auto row = bounds.removeFromTop (Metrics::buttonHeight);

        openUserFolder.setBounds (row.removeFromLeft (170));
        row.removeFromLeft (Metrics::gridHalf);
        openFactoryFolder.setBounds (row.removeFromLeft (180));
        row.removeFromLeft (Metrics::gridHalf);
        openRenderFolder.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (Metrics::gridHalf);
        openDiagnosticsFolder.setBounds (row.removeFromLeft (190));
    }

    bounds = getLocalBounds().withTrimmedTop (200);

    {
        auto row = bounds.removeFromTop (Metrics::buttonHeight);

        addFolderButton.setBounds (row.removeFromLeft (180));
        row.removeFromLeft (Metrics::gridHalf);
        rescanButton.setBounds (row.removeFromLeft (140));
    }

    bounds.removeFromTop (Metrics::grid);

    formatNote.setBounds (bounds.removeFromBottom (64));
    bounds.removeFromBottom (Metrics::gridHalf);

    folderList.setBounds (bounds);
}

} // namespace luthier
