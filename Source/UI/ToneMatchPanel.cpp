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

    // SPEC-SWEEP TM-11 (tone-match 1): start and end trim, in samples of the file.
    for (auto* trim : { &startTrim, &endTrim })
    {
        styleSlider (*trim, 0.0, 48000.0, 1.0, " smp");
        trim->setSkewFactorFromMidPoint (2000.0);
        addAndMakeVisible (*trim);
    }

    startTrim.setTitle (juce::String (getSlotName (which)) + " start trim");
    startTrim.setTooltip ("Samples cut from the start of the impulse response.");
    startTrim.onValueChange = [this]
    {
        if (! updatingControls)
            slot().setStartTrim ((int) startTrim.getValue());
    };

    endTrim.setTitle (juce::String (getSlotName (which)) + " end trim");
    endTrim.setTooltip ("Samples cut from the end of the impulse response.");
    endTrim.onValueChange = [this]
    {
        if (! updatingControls)
            slot().setEndTrim ((int) endTrim.getValue());
    };

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
    startTrim.setValue (s.getStartTrim(), juce::dontSendNotification);   // SPEC-SWEEP TM-11
    endTrim.setValue (s.getEndTrim(), juce::dontSendNotification);

    const bool loaded = s.isLoaded();

    channelBox.setEnabled (loaded);
    gainTrim.setEnabled (loaded);
    predelay.setEnabled (loaded);
    mix.setEnabled (loaded);
    reverseButton.setEnabled (loaded);
    startTrim.setEnabled (loaded);
    endTrim.setEnabled (loaded);
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

    {
        // SPEC-SWEEP TM-11.
        auto r = row (20);
        startTrim.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        endTrim.setBounds (r);
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

        // SPEC-SWEEP TM-23 (tone-match 3.1): a reference from a file.
        referenceButton.setTooltip ("Use an audio file as the reference instead of recording it. "
                                    "You can also drop one onto this card.");
        referenceButton.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a reference recording",
                                                           juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                           "*.wav;*.aif;*.aiff;*.flac;*.mp3");

            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult() != juce::File())
                    useReferenceFile (fc.getResult());
            });
        };
        addAndMakeVisible (referenceButton);

        // SPEC-SWEEP TM-25 (tone-match 3): the band to correct over.
        styleSlider (lowBand, 20.0, 2000.0, 1.0, " Hz");
        lowBand.setSkewFactorFromMidPoint (200.0);
        lowBand.setValue (20.0, juce::dontSendNotification);
        lowBand.setTitle ("EQ match low band edge");
        lowBand.setTooltip ("Lowest frequency the match corrects.");
        addAndMakeVisible (lowBand);

        styleSlider (highBand, 1000.0, 20000.0, 10.0, " Hz");
        highBand.setSkewFactorFromMidPoint (6000.0);
        highBand.setValue (20000.0, juce::dontSendNotification);
        highBand.setTitle ("EQ match high band edge");
        highBand.setTooltip ("Highest frequency the match corrects.");
        addAndMakeVisible (highBand);
    }

    if (kind == Kind::capture)
    {
        // SPEC-SWEEP TM-31 / TM-33 (tone-match 4).
        styleSlider (captureLength, Capture::kMinSeconds, Capture::kMaxSeconds, 0.1, " s");
        captureLength.setSkewFactorFromMidPoint (10.0);
        captureLength.setValue (10.0, juce::dontSendNotification);
        captureLength.setTitle ("Capture length");
        captureLength.setTooltip ("How long the capture records.");
        addAndMakeVisible (captureLength);

        autoTrim.setClickingTogglesState (true);
        autoTrim.setToggleState (true, juce::dontSendNotification);
        autoTrim.setTooltip ("Trim the silence off each end of the capture before saving.");
        addAndMakeVisible (autoTrim);
    }

    restart();
    motion.startTimerHz (*this, 10);
}

MatchWizard::~MatchWizard()
{
    motion.stopTimer();
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
    analysing = false;   // SPEC-SWEEP TM-5: a result still on its way is dropped

    if (kind == Kind::cabMatch)
        processor.getCabMatchSignal().stop();   // SPEC-SWEEP TM-17

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

            if (kind == Kind::cabMatch)
            {
                // SPEC-SWEEP TM-17 (tone-match 2): the test signal goes out to the
                // rig, and the capture starts in the block it does.
                const auto signal = (CabMatch::TestSignal) juce::jmax (0, signalBox.getSelectedId() - 1);
                const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

                processor.getCabMatchSignal().arm (CabMatch::generateTestSignal (signal, rate, 6.0), 6.0);
            }
            else
            {
                // SPEC-SWEEP TM-31: the capture utility's own length.
                capture.start (kind == Kind::capture ? captureLength.getValue() : 10.0);
            }

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
                if (autoTrim.getToggleState())   // SPEC-SWEEP TM-33
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
            /*  SPEC-SWEEP TM-5 (tone-match 0.5): the deconvolution and the fit
                run on a worker thread, and only the result - a file to load
                and a line of text - comes back to the message thread. */
            struct Job
            {
                Kind kind;
                double sampleRate;
                CabMatch::TestSignal signal;
                EqMatch::Options options;
                std::vector<float> reference, current;
            };

            auto job = std::make_shared<Job>();
            job->kind = kind;
            job->sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
            job->signal = (CabMatch::TestSignal) juce::jmax (0, signalBox.getSelectedId() - 1);
            job->reference = reference;
            job->current = current;

            if (kind == Kind::eqMatch)
            {
                job->options.length = (EqMatch::FilterLength) juce::jlimit (
                    0, (int) EqMatch::FilterLength::numLengths - 1, lengthBox.getSelectedId() - 1);

                job->options.aggressiveness = aggressiveness.getValue() * 0.01;
                job->options.preserveDynamics = preserveDynamics.getToggleState();
                job->options.lowHz = lowBand.getValue();    // SPEC-SWEEP TM-25
                job->options.highHz = juce::jmax (lowBand.getValue() * 2.0, highBand.getValue());
            }

            juce::Component::SafePointer<MatchWizard> safeThis (this);
            analysing = true;
            actionButton.setEnabled (false);
            actionButton.setButtonText ("Analysing...");
            resultLabel.setText ("Analysing...", juce::dontSendNotification);

            juce::Thread::launch ([job, safeThis]
            {
                juce::File file;
                juce::String text;
                int slotIndex = 0;
                double nullDb = 0.0;
                bool fitted = false;

                if (job->kind == Kind::cabMatch)
                {
                    const auto testSignal = CabMatch::generateTestSignal (job->signal, job->sampleRate, 6.0);
                    const auto ir = CabMatch::deconvolve (testSignal, job->reference, job->sampleRate, job->signal);

                    IrMetadata metadata;
                    metadata.name = "Cab Match " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M");
                    metadata.type = "cabinet";
                    metadata.author = "user";
                    metadata.notes = "captured with Luthier cab match";
                    metadata.tags = { "cab-match" };

                    file = CabMatch::getMatchDirectory()
                             .getChildFile (juce::File::createLegalFileName (metadata.name) + ".wav");

                    if (CabMatch::saveIr (ir, file, metadata))
                    {
                        nullDb = CabMatch::measureNull (job->reference, job->current);
                        fitted = true;
                        text = "Saved " + file.getFileName()
                                 + "    null " + juce::String (nullDb, 1) + " dB"
                                 + "    tail " + juce::String (ir.getLengthMs(), 0) + " ms";
                    }
                    else
                    {
                        text = "Could not write the matched IR.";
                    }
                }
                else
                {
                    const auto filter = EqMatch::fit (job->reference, job->current, job->sampleRate, job->options);
                    slotIndex = 1;

                    if (! filter.isEmpty())
                    {
                        // The fitted filter is an IR like any other, so it goes into
                        // the second cabinet slot rather than into a special path.
                        file = IrLibraryPaths::getSpecial()
                                 .getChildFile ("EQ Match " + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M")
                                                  + ".wav");

                        IrMetadata metadata;
                        metadata.name = file.getFileNameWithoutExtension();
                        metadata.type = "special";
                        metadata.tags = { "eq-match" };

                        fitted = CabMatch::saveIr (filter, file, metadata);
                        text = fitted ? "Fitted " + juce::String (filter.getLength()) + " taps, saved as " + file.getFileName()
                                      : juce::String ("Could not write the fitted filter.");
                    }
                    else
                    {
                        text = "The fit produced nothing; try a longer passage.";
                    }
                }

                juce::MessageManager::callAsync ([safeThis, file, text, slotIndex, nullDb, fitted]
                {
                    if (auto* wizard = safeThis.getComponent())
                        wizard->finishAnalysis (file, text, slotIndex, nullDb, fitted);
                });
            });

            return;
        }

        case 3:
        default:
            if (analysing)
                return;

            restart();
            break;
    }

    stepLabel.setText (getStepText(), juce::dontSendNotification);
    repaint();
}

bool MatchWizard::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (kind != Kind::eqMatch)
        return false;

    for (const auto& path : files)
    {
        const auto extension = juce::File (path).getFileExtension().toLowerCase();

        if (extension == ".wav" || extension == ".aif" || extension == ".aiff"
              || extension == ".flac" || extension == ".mp3")
            return true;
    }

    return false;
}

void MatchWizard::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
        if (useReferenceFile (juce::File (path)))
            break;
}

bool MatchWizard::useReferenceFile (const juce::File& file)
{
    // SPEC-SWEEP TM-23: read the file as the reference (mono, at the plugin's
    // rate), then record Luthier's own pass as step 2.
    if (kind != Kind::eqMatch || analysing || ! file.existsAsFile())
        return false;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
    {
        resultLabel.setText ("Could not read " + file.getFileName() + ".", juce::dontSendNotification);
        return false;
    }

    const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const int length = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (Capture::kMaxSeconds * reader->sampleRate));

    juce::AudioBuffer<float> audio ((int) juce::jmax (1u, juce::jmin (2u, reader->numChannels)), length);
    reader->read (&audio, 0, length, 0, true, audio.getNumChannels() > 1);

    std::vector<float> mono ((size_t) length, 0.0f);

    for (int c = 0; c < audio.getNumChannels(); ++c)
        for (int i = 0; i < length; ++i)
            mono[(size_t) i] += audio.getSample (c, i) / (float) audio.getNumChannels();

    // Linear resampling to the plugin's rate: the fit is a long-term spectrum.
    const double ratio = reader->sampleRate / rate;
    const int outLength = juce::jmax (1, (int) (length / juce::jmax (1.0e-6, ratio)));
    reference.assign ((size_t) outLength, 0.0f);

    for (int i = 0; i < outLength; ++i)
    {
        const double at = i * ratio;
        const int j = juce::jmin (length - 1, (int) at);
        const int k = juce::jmin (length - 1, j + 1);
        const double t = at - j;
        reference[(size_t) i] = (float) (mono[(size_t) j] * (1.0 - t) + mono[(size_t) k] * t);
    }

    // Step 2: Luthier's own pass, as long as the reference (up to the capture's limit).
    auto& capture = processor.getCapture();
    capture.reset();
    capture.setSource (Capture::Source::mainOut);
    capture.start (juce::jlimit (Capture::kMinSeconds, Capture::kMaxSeconds, (double) outLength / rate));

    step = 2;
    actionButton.setButtonText ("Recording...");
    actionButton.setEnabled (false);
    resultLabel.setText ("Reference: " + file.getFileName(), juce::dontSendNotification);
    stepLabel.setText (getStepText(), juce::dontSendNotification);
    repaint();
    return true;
}

void MatchWizard::finishAnalysis (const juce::File& file, const juce::String& text, int slotIndex,
                                  double nullDb, bool fitted)
{
    // SPEC-SWEEP TM-5: back on the message thread with the result - unless
    // the wizard was cancelled while it worked.
    if (! analysing)
        return;

    analysing = false;

    if (fitted && file.existsAsFile())
    {
        processor.pushUndoState ("Load IR " + file.getFileNameWithoutExtension());   // action-and-undo.md
        processor.getCabIrSlot (slotIndex).load (file);
        processor.getCabIrSlot (slotIndex).setEngaged (true);

        if (kind == Kind::cabMatch)
        {
            nullResultDb = nullDb;
            haveResult = true;
        }
    }

    resultLabel.setText (text, juce::dontSendNotification);

    {
        step = 3;
        actionButton.setButtonText ("Again");
        actionButton.setEnabled (true);

        if (onFinished != nullptr)
            onFinished();
    }

    stepLabel.setText (getStepText(), juce::dontSendNotification);
    repaint();
}

void MatchWizard::timerCallback()
{
    auto& capture = processor.getCapture();

    // A recording step advances itself when the capture is full, so the user is
    // not left holding a button while the sweep plays.
    if ((step == 1 || step == 2) && ! actionButton.isEnabled() && ! analysing)   // SPEC-SWEEP TM-5
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
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

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
        referenceButton.setBounds (buttons.removeFromRight (120).reduced (1));   // SPEC-SWEEP TM-23

        auto row = bounds.removeFromBottom (24).reduced (0, 1);

        lengthBox.setBounds (row.removeFromLeft (96));
        row.removeFromLeft (4);
        preserveDynamics.setBounds (row.removeFromRight (128));
        row.removeFromRight (4);
        aggressiveness.setBounds (row);

        // SPEC-SWEEP TM-25.
        auto band = bounds.removeFromBottom (24).reduced (0, 1);
        lowBand.setBounds (band.removeFromLeft (band.getWidth() / 2 - 2));
        band.removeFromLeft (4);
        highBand.setBounds (band);
    }

    if (kind == Kind::capture)
    {
        // SPEC-SWEEP TM-31 / TM-33.
        auto row = bounds.removeFromBottom (24).reduced (0, 1);
        autoTrim.setBounds (row.removeFromRight (128));
        row.removeFromRight (4);
        captureLength.setBounds (row);
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

    // SPEC-SWEEP TM-38 (tone-match 6): search by name, tag or note.
    searchBox.setTextToShowWhenEmpty ("Search IRs", Palette::textMuted);
    searchBox.setTitle ("Search IRs");
    searchBox.onTextChange = [this] { refreshLibrary(); };
    addAndMakeVisible (searchBox);

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

    const auto search = searchBox.getText().trim();

    for (const auto& file : libraryFiles)
    {
        const auto metadata = IrMetadata::forFile (file);

        if (! all && ! metadata.tags.contains (wanted, true))
            continue;

        // SPEC-SWEEP TM-38: every word of the search in the name, tags or notes.
        if (search.isNotEmpty())
        {
            const auto haystack = file.getFileNameWithoutExtension() + " " + metadata.name + " "
                                  + metadata.tags.joinIntoString (" ") + " " + metadata.notes;
            bool matches = true;

            for (const auto& word : juce::StringArray::fromTokens (search, " ", ""))
                if (word.isNotEmpty() && ! haystack.containsIgnoreCase (word))
                    matches = false;

            if (! matches)
                continue;
        }

        visibleFiles.add (file);
    }

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
        searchBox.setBounds (r.removeFromRight (r.getWidth() / 2));   // SPEC-SWEEP TM-38
        r.removeFromRight (4);
        tagFilter.setBounds (r);
    }

    libraryList.setBounds (row (110));
    libraryHint.setBounds (row (14));
}

} // namespace luthier
