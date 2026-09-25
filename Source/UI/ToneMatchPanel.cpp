#include "ToneMatchPanel.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    void styleHeading (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        label.setColour (juce::Label::textColourId, Palette::accent);
    }

    void styleSlider (juce::Slider& slider, double minimum, double maximum,
                      double interval, const juce::String& suffix)
    {
        slider.setRange (minimum, maximum, interval);
        slider.setTextValueSuffix (suffix);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    const char* getSlotName (IrSlotEditor::Slot slot)
    {
        switch (slot)
        {
            case IrSlotEditor::Slot::body: return "Body IR";
            case IrSlotEditor::Slot::cab1: return "Cabinet IR 1";
            case IrSlotEditor::Slot::cab2: return "Cabinet IR 2";
            default:                       return "IR";
        }
    }
}

//==============================================================================
IrSlotEditor::IrSlotEditor (LuthierAudioProcessor& p, Slot s)
    : processor (p), which (s)
{
    engageToggle = std::make_unique<LuthierToggle> (getSlotName (which));
    engageToggle->getButton().setClickingTogglesState (true);

    engageToggle->getButton().onClick = [this]
    {
        if (updatingControls)
            return;

        pushIrEdit ("engage", false);   // action-and-undo.md (tone-match IR slots)
        slot().setEngaged (engageToggle->getButton().getToggleState());
    };

    engageToggle->setTooltip ("Use this impulse response instead of the built-in model.");
    addAndMakeVisible (*engageToggle);

    nameLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    nameLabel.setColour (juce::Label::textColourId, Palette::textPrimary);
    addAndMakeVisible (nameLabel);

    infoLabel.setFont (juce::Font (juce::FontOptions (9.0f)));
    infoLabel.setColour (juce::Label::textColourId, Palette::textDisabled);
    addAndMakeVisible (infoLabel);

    loadButton.setTooltip ("Load a WAV, AIFF or FLAC impulse response. "
                           "You can also drag one onto this card.");

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Load an impulse response", IrLibraryPaths::getRoot(),
            "*.wav;*.aif;*.aiff;*.flac");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();

            if (file != juce::File())
                load (file);
        });
    };

    addAndMakeVisible (loadButton);

    clearButton.setTooltip ("Go back to the built-in model.");
    clearButton.onClick = [this]
    {
        pushIrEdit ("clear", false);   // action-and-undo.md (tone-match IR slots)
        slot().unload();
        refresh();

        if (onChanged != nullptr)
            onChanged();
    };

    addAndMakeVisible (clearButton);

    // tone-match 1: which channel of a multi-channel IR, or a sum.
    channelBox.addItem ("Sum to mono", 1);

    for (int channel = 0; channel < 6; ++channel)
        channelBox.addItem ("Channel " + juce::String (channel + 1), channel + 2);

    channelBox.setSelectedId (1, juce::dontSendNotification);

    channelBox.onChange = [this]
    {
        if (updatingControls)
            return;

        pushIrEdit ("channel", true);   // action-and-undo.md (tone-match IR slots)
        slot().setChannel (channelBox.getSelectedId() - 2);
    };

    addAndMakeVisible (channelBox);

    styleSlider (gainTrim, -24.0, 24.0, 0.1, " dB");
    gainTrim.onValueChange = [this]
    {
        if (updatingControls)
            return;

        pushIrEdit ("gain trim", true);   // action-and-undo.md (tone-match IR slots)
        slot().setGainTrimDb (gainTrim.getValue());
    };
    addAndMakeVisible (gainTrim);

    styleSlider (predelay, 0.0, 100.0, 0.1, " ms");
    predelay.setTooltip ("Delay before the impulse, for cab IRs with unwanted pre-ringing.");
    predelay.onValueChange = [this]
    {
        if (updatingControls)
            return;

        pushIrEdit ("predelay", true);   // action-and-undo.md (tone-match IR slots)
        slot().setPredelayMs (predelay.getValue());
    };
    addAndMakeVisible (predelay);

    styleSlider (mix, 0.0, 100.0, 1.0, " %");
    mix.setTooltip ("How much of the impulse response against the built-in model.");
    mix.onValueChange = [this]
    {
        if (updatingControls)
            return;

        pushIrEdit ("mix", true);   // action-and-undo.md (tone-match IR slots)
        slot().setMix (mix.getValue() * 0.01);
    };
    addAndMakeVisible (mix);

    reverseButton.setClickingTogglesState (true);
    reverseButton.onClick = [this]
    {
        if (updatingControls)
            return;

        pushIrEdit ("reverse", false);   // action-and-undo.md (tone-match IR slots)
        slot().setReversed (reverseButton.getToggleState());
    };
    addAndMakeVisible (reverseButton);

    refresh();
}

IrSlotEditor::~IrSlotEditor() = default;

void IrSlotEditor::pushIrEdit (const char* what, bool groups)
{
    const juce::String name = juce::String (getSlotName (which)) + " IR " + what;
    processor.pushUndoAction ("Change " + name, groups ? "ir-edit" : juce::String(), groups ? name : juce::String());
}

IrSlot& IrSlotEditor::slot()
{
    switch (which)
    {
        case Slot::cab1: return processor.getCabIrSlot (0);
        case Slot::cab2: return processor.getCabIrSlot (1);

        case Slot::body:
        default:         return processor.getBodyIrSlot();
    }
}

void IrSlotEditor::load (const juce::File& file)
{
    pushIrEdit ("load", false);   // action-and-undo.md (tone-match IR slots)

    if (! slot().load (file))
    {
        infoLabel.setText (slot().getLastError(), juce::dontSendNotification);
        infoLabel.setColour (juce::Label::textColourId, Palette::warning);
        return;
    }

    // Loading one is a request to hear it.
    slot().setEngaged (true);

    refresh();

    if (onChanged != nullptr)
        onChanged();
}

void IrSlotEditor::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    auto& s = slot();

    engageToggle->getButton().setToggleState (s.isEngaged(), juce::dontSendNotification);

    if (s.isLoaded())
    {
        nameLabel.setText (s.getName(), juce::dontSendNotification);

        const auto metadata = IrMetadata::forFile (s.getFile());

        infoLabel.setColour (juce::Label::textColourId, Palette::textDisabled);
        infoLabel.setText (juce::String (s.getLengthMs(), 1) + " ms"
                             + (metadata.tags.isEmpty() ? juce::String()
                                                        : ("   " + metadata.tags.joinIntoString (", "))),
                           juce::dontSendNotification);
    }
    else
    {
        nameLabel.setText ("Built-in model", juce::dontSendNotification);

        infoLabel.setColour (juce::Label::textColourId, Palette::textDisabled);
        infoLabel.setText (s.getLastError().isNotEmpty() ? s.getLastError()
                                                         : juce::String ("Drop an IR here, or load one."),
                           juce::dontSendNotification);
    }

    channelBox.setSelectedId (s.getChannel() + 2, juce::dontSendNotification);
    gainTrim.setValue (s.getGainTrimDb(), juce::dontSendNotification);
    predelay.setValue (s.getPredelayMs(), juce::dontSendNotification);
    mix.setValue (s.getMix() * 100.0, juce::dontSendNotification);
    reverseButton.setToggleState (s.isReversed(), juce::dontSendNotification);

    const bool loaded = s.isLoaded();

    channelBox.setEnabled (loaded);
    gainTrim.setEnabled (loaded);
    predelay.setEnabled (loaded);
    mix.setEnabled (loaded);
    reverseButton.setEnabled (loaded);
    clearButton.setEnabled (loaded);

    repaint();
}

bool IrSlotEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        const auto extension = juce::File (path).getFileExtension().toLowerCase();

        if (extension == ".wav" || extension == ".aif" || extension == ".aiff"
              || extension == ".flac")
            return true;
    }

    return false;
}

void IrSlotEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragging = false;

    for (const auto& path : files)
    {
        const juce::File file (path);

        if (file.existsAsFile())
        {
            load (file);
            break;
        }
    }

    repaint();
}

void IrSlotEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (slot().isEngaged() ? Palette::accent : Palette::edge);
    g.drawRoundedRectangle (bounds, 3.0f, slot().isEngaged() ? 1.5f : 1.0f);
}

void IrSlotEditor::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    auto row = [&bounds] (int height, int gap = 2)
    {
        auto r = bounds.removeFromTop (height);
        bounds.removeFromTop (gap);
        return r;
    };

    {
        auto r = row (Metrics::buttonHeight);
        engageToggle->setBounds (r.removeFromLeft (juce::jmax (110, r.getWidth() / 3)));
        r.removeFromLeft (4);
        clearButton.setBounds (r.removeFromRight (60));
        r.removeFromRight (4);
        loadButton.setBounds (r.removeFromRight (70));
    }

    nameLabel.setBounds (row (14));
    infoLabel.setBounds (row (12));

    channelBox.setBounds (row (24));
    gainTrim.setBounds (row (20));
    predelay.setBounds (row (20));

    {
        auto r = row (20);
        reverseButton.setBounds (r.removeFromRight (80));
        r.removeFromRight (4);
        mix.setBounds (r);
    }
}

//==============================================================================
MatchWizard::MatchWizard (LuthierAudioProcessor& p, Kind k)
    : processor (p), kind (k)
{
    stepLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    stepLabel.setColour (juce::Label::textColourId, Palette::textPrimary);
    stepLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (stepLabel);

    resultLabel.setFont (juce::Font (juce::FontOptions (9.0f)));
    resultLabel.setColour (juce::Label::textColourId, Palette::textDisabled);
    addAndMakeVisible (resultLabel);

    actionButton.onClick = [this] { advance(); };
    addAndMakeVisible (actionButton);

    cancelButton.onClick = [this] { restart(); };
    addAndMakeVisible (cancelButton);

    if (kind == Kind::cabMatch)
    {
        for (int i = 0; i < (int) CabMatch::TestSignal::numSignals; ++i)
            signalBox.addItem (CabMatch::getTestSignalName ((CabMatch::TestSignal) i), i + 1);

        signalBox.setSelectedId (1, juce::dontSendNotification);
        signalBox.setTooltip ("The sweep deconvolves most cleanly. MLS is the fallback "
                              "when the sweep provokes the amp; the burst biases the "
                              "match toward guitar frequencies.");

        addAndMakeVisible (signalBox);
    }

    if (kind == Kind::eqMatch)
    {
        lengthBox.addItem ("256 taps", 1);
        lengthBox.addItem ("1024 taps", 2);
        lengthBox.addItem ("4096 taps", 3);
        lengthBox.setSelectedId (2, juce::dontSendNotification);
        addAndMakeVisible (lengthBox);

        styleSlider (aggressiveness, 0.0, 100.0, 1.0, " %");
        aggressiveness.setValue (100.0, juce::dontSendNotification);
        aggressiveness.setTooltip ("How much of the measured difference to apply.");
        addAndMakeVisible (aggressiveness);

        preserveDynamics.setClickingTogglesState (true);
        preserveDynamics.setTooltip ("Correct only the spectral shape, not the overall level.");
        addAndMakeVisible (preserveDynamics);
    }

    restart();
    startTimerHz (10);
}

MatchWizard::~MatchWizard()
{
    stopTimer();
}

juce::String MatchWizard::getStepText() const
{
    switch (kind)
    {
        case Kind::cabMatch:
            switch (step)
            {
                case 0:  return "Cab Match captures a real cabinet as an impulse response.\n"
                                "Route the DI aux output to your rig, and its return to "
                                "Luthier's sidechain input.";
                case 1:  return "Step 1 of 2: recording the reference return.\n"
                                "The test signal is playing through your rig now.";
                case 2:  return "Step 2 of 2: recording Luthier's own cabinet, for comparison.";
                case 3:  return "Done. The matched IR has been saved and loaded into "
                                "cabinet slot 1.";
                default: return {};
            }

        case Kind::eqMatch:
            switch (step)
            {
                case 0:  return "EQ Match fits Luthier's tone to a reference recording.\n"
                                + juce::String (EqMatch::getDescription());
                case 1:  return "Step 1 of 2: play or drop in the reference passage.";
                case 2:  return "Step 2 of 2: play the same passage through Luthier.";
                case 3:  return "Done. The correction filter has been fitted.";
                default: return {};
            }

        case Kind::capture:
            switch (step)
            {
                case 0:  return "Capture records any of Luthier's outputs to a 32-bit "
                                "float WAV in your Captures folder.";
                case 1:  return "Recording...";
                case 2:  return "Done.";
                default: return {};
            }

        default:
            return {};
    }
}

void MatchWizard::restart()
{
    step = 0;
    haveResult = false;

    reference.clear();
    current.clear();

    processor.getCapture().reset();

    actionButton.setButtonText ("Start");
    resultLabel.setText ({}, juce::dontSendNotification);
    stepLabel.setText (getStepText(), juce::dontSendNotification);

    repaint();
}

void MatchWizard::advance()
{
    auto& capture = processor.getCapture();

    switch (step)
    {
        case 0:
            // Start recording the first pass: the reference, which comes back
            // on the sidechain for both matches, or the plugin's own output.
            capture.setSource (kind == Kind::capture ? Capture::Source::mainOut
                                                     : Capture::Source::sidechain);
            capture.start (kind == Kind::cabMatch ? 6.0 : 10.0);
            step = 1;
            actionButton.setButtonText ("Recording...");
            actionButton.setEnabled (false);
            break;

        case 1:
        {
            // Keep the first pass, and start the second.
            const auto& buffer = capture.getBuffer();
            const int length = capture.getRecordedSamples();

            reference.assign ((size_t) juce::jmax (0, length), 0.0f);

            for (int i = 0; i < length; ++i)
                reference[(size_t) i] = buffer.getSample (0, i);

            if (kind == Kind::capture)
            {
                // The capture utility has only one pass, and it writes a file.
                capture.autoTrim();

                const auto file = Capture::getCaptureDirectory().getChildFile (
                    "capture-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".wav");

                resultLabel.setText (capture.save (file) ? ("Saved to " + file.getFileName())
                                                         : juce::String ("Could not save the capture."),
                                     juce::dontSendNotification);

                step = 2;
                actionButton.setButtonText ("Again");
                actionButton.setEnabled (true);
                break;
            }

            capture.reset();
            capture.setSource (Capture::Source::mainOut);   // Luthier's own, to compare
            capture.start (kind == Kind::cabMatch ? 6.0 : 10.0);

            step = 2;
            actionButton.setButtonText ("Recording...");
            actionButton.setEnabled (false);
            break;
        }

        case 2:
        {
            const auto& buffer = capture.getBuffer();
            const int length = capture.getRecordedSamples();

            current.assign ((size_t) juce::jmax (0, length), 0.0f);

            for (int i = 0; i < length; ++i)
                current[(size_t) i] = buffer.getSample (0, i);

            // ---- do the work -----------------------------------------------------
            if (kind == Kind::cabMatch)
            {
                const auto signal = (CabMatch::TestSignal) juce::jmax (0, signalBox.getSelectedId() - 1);

                const auto testSignal = CabMatch::generateTestSignal (
                    signal, processor.getSampleRate(), 6.0);

                const auto ir = CabMatch::deconvolve (testSignal, reference,
                                                      processor.getSampleRate(), signal);

                IrMetadata metadata;
                metadata.name = "Cab Match " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M");
                metadata.type = "cabinet";
                metadata.author = "user";
                metadata.notes = "captured with Luthier cab match";
                metadata.tags = { "cab-match" };

                const auto file = CabMatch::getMatchDirectory()
                                    .getChildFile (juce::File::createLegalFileName (metadata.name) + ".wav");

                if (CabMatch::saveIr (ir, file, metadata))
                {
                    processor.pushUndoState ("Load IR " + file.getFileNameWithoutExtension());   // action-and-undo.md
                    processor.getCabIrSlot (0).load (file);
                    processor.getCabIrSlot (0).setEngaged (true);

                    nullResultDb = CabMatch::measureNull (reference, current);
                    haveResult = true;

                    resultLabel.setText ("Saved " + file.getFileName()
                                           + "    null " + juce::String (nullResultDb, 1) + " dB"
                                           + "    tail " + juce::String (ir.getLengthMs(), 0) + " ms",
                                         juce::dontSendNotification);
                }
                else
                {
                    resultLabel.setText ("Could not write the matched IR.",
                                         juce::dontSendNotification);
                }
            }
            else if (kind == Kind::eqMatch)
            {
                EqMatch::Options options;

                options.length = (EqMatch::FilterLength) juce::jlimit (
                    0, (int) EqMatch::FilterLength::numLengths - 1, lengthBox.getSelectedId() - 1);

                options.aggressiveness = aggressiveness.getValue() * 0.01;
                options.preserveDynamics = preserveDynamics.getToggleState();

                const auto filter = EqMatch::fit (reference, current,
                                                  processor.getSampleRate(), options);

                if (! filter.isEmpty())
                {
                    // The fitted filter is an IR like any other, so it goes into
                    // the second cabinet slot rather than into a special path.
                    const auto file = IrLibraryPaths::getSpecial()
                                        .getChildFile ("EQ Match "
                                                         + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M")
                                                         + ".wav");

                    IrMetadata metadata;
                    metadata.name = file.getFileNameWithoutExtension();
                    metadata.type = "special";
                    metadata.tags = { "eq-match" };

                    if (CabMatch::saveIr (filter, file, metadata))
                    {
                        processor.pushUndoState ("Load IR " + file.getFileNameWithoutExtension());   // action-and-undo.md
                        processor.getCabIrSlot (1).load (file);
                        processor.getCabIrSlot (1).setEngaged (true);

                        resultLabel.setText ("Fitted " + juce::String (filter.getLength())
                                               + " taps, saved as " + file.getFileName(),
                                             juce::dontSendNotification);
                    }
                }
                else
                {
                    resultLabel.setText ("The fit produced nothing; try a longer passage.",
                                         juce::dontSendNotification);
                }
            }

            step = 3;
            actionButton.setButtonText ("Again");
            actionButton.setEnabled (true);

            if (onFinished != nullptr)
                onFinished();

            break;
        }

        case 3:
        default:
            restart();
            break;
    }

    stepLabel.setText (getStepText(), juce::dontSendNotification);
    repaint();
}

void MatchWizard::timerCallback()
{
    auto& capture = processor.getCapture();

    // A recording step advances itself when the capture is full, so the user is
    // not left holding a button while the sweep plays.
    if ((step == 1 || step == 2) && ! actionButton.isEnabled())
    {
        if (capture.isComplete())
        {
            actionButton.setEnabled (true);
            advance();
        }
        else if (capture.isRecording())
        {
            resultLabel.setText (juce::String ((int) (capture.getProgress() * 100.0)) + "%",
                                 juce::dontSendNotification);
        }
    }
}

void MatchWizard::paint (juce::Graphics& g)
{
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 3.0f);

    g.setColour (step > 0 ? Palette::secondary : Palette::edge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 3.0f, 1.0f);

    // A progress bar while recording, so the wizard is not a frozen dialog.
    const auto& capture = processor.getCapture();

    if (capture.isRecording())
    {
        auto bar = getLocalBounds().reduced (6).removeFromBottom (4).toFloat();

        g.setColour (Palette::edge);
        g.fillRect (bar);

        g.setColour (Palette::accent);
        g.fillRect (bar.withWidth (bar.getWidth() * (float) capture.getProgress()));
    }
}

void MatchWizard::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    auto buttons = bounds.removeFromBottom (Metrics::buttonHeight);

    actionButton.setBounds (buttons.removeFromLeft (92).reduced (1));
    buttons.removeFromLeft (4);
    cancelButton.setBounds (buttons.removeFromLeft (72).reduced (1));

    bounds.removeFromBottom (2);
    resultLabel.setBounds (bounds.removeFromBottom (12));

    if (kind == Kind::cabMatch)
        signalBox.setBounds (bounds.removeFromBottom (24).reduced (0, 1));

    if (kind == Kind::eqMatch)
    {
        auto row = bounds.removeFromBottom (24).reduced (0, 1);

        lengthBox.setBounds (row.removeFromLeft (96));
        row.removeFromLeft (4);
        preserveDynamics.setBounds (row.removeFromRight (128));
        row.removeFromRight (4);
        aggressiveness.setBounds (row);
    }

    stepLabel.setBounds (bounds);
}

//==============================================================================
int ToneMatchPanel::LibraryModel::getNumRows()
{
    return owner.visibleFiles.size();
}

void ToneMatchPanel::LibraryModel::paintListBoxItem (int row, juce::Graphics& g,
                                                     int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, owner.visibleFiles.size()))
        return;

    const auto file = owner.visibleFiles[row];

    if (selected)
    {
        g.setColour (Palette::accentDim);
        g.fillRect (0, 0, width, height);
    }

    g.setColour (selected ? Palette::textPrimary : Palette::textMuted);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (file.getFileNameWithoutExtension(), 6, 0, width - 80, height,
                juce::Justification::centredLeft, true);

    // The folder it came from, which is the library's organisation showing.
    g.setColour (Palette::textDisabled);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText (file.getParentDirectory().getFileName(), width - 76, 0, 70, height,
                juce::Justification::centredRight, true);
}

void ToneMatchPanel::LibraryModel::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    owner.loadSelectedFromLibrary();
}

//==============================================================================
ToneMatchPanel::ToneMatchPanel (LuthierAudioProcessor& p)
    : processor (p)
{
    bodySlot = std::make_unique<IrSlotEditor> (processor, IrSlotEditor::Slot::body);
    cabSlot1 = std::make_unique<IrSlotEditor> (processor, IrSlotEditor::Slot::cab1);
    cabSlot2 = std::make_unique<IrSlotEditor> (processor, IrSlotEditor::Slot::cab2);

    for (auto* editor : { bodySlot.get(), cabSlot1.get(), cabSlot2.get() })
        addAndMakeVisible (*editor);

    cabMatch = std::make_unique<MatchWizard> (processor, MatchWizard::Kind::cabMatch);
    eqMatch  = std::make_unique<MatchWizard> (processor, MatchWizard::Kind::eqMatch);
    capture  = std::make_unique<MatchWizard> (processor, MatchWizard::Kind::capture);

    cabMatch->onFinished = [this] { cabSlot1->refresh(); refreshLibrary(); };
    eqMatch->onFinished  = [this] { cabSlot2->refresh(); refreshLibrary(); };
    capture->onFinished  = [this] { refreshLibrary(); };

    for (auto* wizard : { cabMatch.get(), eqMatch.get(), capture.get() })
        addAndMakeVisible (*wizard);

    // ---- library -------------------------------------------------------------------
    libraryList.setModel (&listModel);
    libraryList.setRowHeight (18);
    libraryList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    addAndMakeVisible (libraryList);

    tagFilter.onChange = [this] { refreshLibrary(); };
    addAndMakeVisible (tagFilter);

    refreshButton.setTooltip ("Re-scan your IR folder.");
    refreshButton.onClick = [this]
    {
        libraryFiles = IrLibraryPaths::findAll();
        refreshLibrary();
    };
    addAndMakeVisible (refreshButton);

    libraryHint.setFont (juce::Font (juce::FontOptions (9.0f)));
    libraryHint.setColour (juce::Label::textColourId, Palette::textDisabled);
    addAndMakeVisible (libraryHint);

    styleHeading (slotsHeading,   "IMPULSE RESPONSES");
    styleHeading (wizardsHeading, "MATCH AND CAPTURE");
    styleHeading (libraryHeading, "IR LIBRARY");

    for (auto* label : { &slotsHeading, &wizardsHeading, &libraryHeading })
        addAndMakeVisible (*label);

    libraryFiles = IrLibraryPaths::findAll();
    refreshLibrary();
}

ToneMatchPanel::~ToneMatchPanel() = default;

void ToneMatchPanel::refreshLibrary()
{
    // Rebuild the tag list from what is actually on disk, so the filter can
    // never offer a tag that matches nothing.
    juce::StringArray tags;

    for (const auto& file : libraryFiles)
        for (const auto& tag : IrMetadata::forFile (file).tags)
            tags.addIfNotAlreadyThere (tag);

    tags.sort (true);

    const auto selected = tagFilter.getText();

    tagFilter.clear (juce::dontSendNotification);
    tagFilter.addItem ("All IRs", 1);

    int itemId = 2;

    for (const auto& tag : tags)
        tagFilter.addItem (tag, itemId++);

    // Keep the user's filter across a rescan if it still exists.
    bool restored = false;

    for (int i = 0; i < tagFilter.getNumItems(); ++i)
    {
        if (tagFilter.getItemText (i) == selected)
        {
            tagFilter.setSelectedItemIndex (i, juce::dontSendNotification);
            restored = true;
            break;
        }
    }

    if (! restored)
        tagFilter.setSelectedId (1, juce::dontSendNotification);

    // ---- the visible list ------------------------------------------------------------
    visibleFiles.clearQuick();

    const bool all = tagFilter.getSelectedId() <= 1;
    const auto wanted = tagFilter.getText();

    for (const auto& file : libraryFiles)
        if (all || IrMetadata::forFile (file).tags.contains (wanted, true))
            visibleFiles.add (file);

    libraryList.updateContent();
    libraryList.repaint();

    libraryHint.setText (libraryFiles.isEmpty()
                           ? ("Put IRs in " + IrLibraryPaths::getRoot().getFullPathName())
                           : (juce::String (visibleFiles.size()) + " of "
                                + juce::String (libraryFiles.size()) + " IRs"),
                         juce::dontSendNotification);
}

void ToneMatchPanel::loadSelectedFromLibrary()
{
    const int row = libraryList.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, visibleFiles.size()))
        return;

    const auto file = visibleFiles[row];

    // A body IR goes to the body slot, everything else to cabinet slot 1.
    const auto metadata = IrMetadata::forFile (file);

    if (metadata.type == "body")
    {
        processor.pushUndoState ("Load IR " + file.getFileNameWithoutExtension());   // action-and-undo.md
        processor.getBodyIrSlot().load (file);
        processor.getBodyIrSlot().setEngaged (true);
        bodySlot->refresh();
    }
    else
    {
        processor.pushUndoState ("Load IR " + file.getFileNameWithoutExtension());   // action-and-undo.md
        processor.getCabIrSlot (0).load (file);
        processor.getCabIrSlot (0).setEngaged (true);
        cabSlot1->refresh();
    }
}

//==============================================================================
int ToneMatchPanel::preferredHeight() const
{
    return 16 + IrSlotEditor::preferredHeight * 3 + 8
         + 16 + MatchWizard::preferredHeight * 3 + 8
         + 16 + 26 + 110 + 14 + 24;
}

void ToneMatchPanel::paint (juce::Graphics& g)
{
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
}

void ToneMatchPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    auto row = [&bounds] (int height, int gap = 2)
    {
        auto r = bounds.removeFromTop (height);
        bounds.removeFromTop (gap);
        return r;
    };

    slotsHeading.setBounds (row (16));

    bodySlot->setBounds (row (IrSlotEditor::preferredHeight));
    cabSlot1->setBounds (row (IrSlotEditor::preferredHeight));
    cabSlot2->setBounds (row (IrSlotEditor::preferredHeight));

    wizardsHeading.setBounds (row (16));

    cabMatch->setBounds (row (MatchWizard::preferredHeight));
    eqMatch->setBounds (row (MatchWizard::preferredHeight));
    capture->setBounds (row (MatchWizard::preferredHeight));

    libraryHeading.setBounds (row (16));

    {
        auto r = row (26);
        refreshButton.setBounds (r.removeFromRight (72));
        r.removeFromRight (4);
        tagFilter.setBounds (r);
    }

    libraryList.setBounds (row (110));
    libraryHint.setBounds (row (14));
}

} // namespace luthier
