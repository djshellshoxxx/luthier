#include "PracticePanel.h"
#include "MidiExportDefaults.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    void styleSlider (juce::Slider& slider, double minimum, double maximum,
                      double interval, const juce::String& suffix)
    {
        slider.setRange (minimum, maximum, interval);
        slider.setTextValueSuffix (suffix);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void styleReadout (juce::Label& label, juce::Colour colour = Palette::textMuted)
    {
        label.setFont (juce::Font (juce::FontOptions (10.0f)));
        label.setColour (juce::Label::textColourId, colour);
    }

    /** Lays a row of controls out left to right in the space given. */
    struct RowLayout
    {
        juce::Rectangle<int> bounds;

        juce::Rectangle<int> take (int width, int gap = 4)
        {
            auto r = bounds.removeFromLeft (width);
            bounds.removeFromLeft (gap);
            return r;
        }

        juce::Rectangle<int> rest() { return bounds; }
    };
}

//==============================================================================
BeatIndicator::BeatIndicator (LuthierAudioProcessor& p)
    : processor (p)
{
}

void BeatIndicator::refresh()
{
    const auto& metronome = processor.getMetronome();

    const int beat = metronome.getCurrentBeat();
    const bool silent = metronome.isCurrentBarSilent();

    if (beat == lastBeat && silent == lastSilent)
        return;

    lastBeat = beat;
    lastSilent = silent;

    repaint();
}

void BeatIndicator::paint (juce::Graphics& g)
{
    const auto& metronome = processor.getMetronome();

    const int beats = juce::jlimit (1, 12, metronome.getTimeSignature().numerator);
    const int current = metronome.getCurrentBeat();

    const float diameter = juce::jmin (12.0f, (float) getHeight() - 2.0f);
    const float spacing = (float) getWidth() / (float) beats;

    for (int beat = 0; beat < beats; ++beat)
    {
        const bool lit = metronome.isEnabled() && beat == current;
        const auto accent = metronome.getBeatAccent (beat);

        const float x = spacing * ((float) beat + 0.5f) - diameter * 0.5f;
        const float y = ((float) getHeight() - diameter) * 0.5f;

        // A silent bar is drawn hollow rather than merely dim, so that "the
        // click has stopped" and "the volume is down" do not look the same.
        if (lastSilent)
        {
            g.setColour (lit ? Palette::textMuted : Palette::edge);
            g.drawEllipse (x, y, diameter, diameter, 1.5f);
            continue;
        }

        switch (accent)
        {
            case BeatAccent::silent:
                g.setColour (Palette::edge);
                g.drawEllipse (x, y, diameter, diameter, 1.0f);
                break;

            case BeatAccent::accent:
                g.setColour (lit ? Palette::accentBright : Palette::accentDim);
                g.fillEllipse (x, y, diameter, diameter);
                break;

            case BeatAccent::ghost:
                g.setColour (lit ? Palette::secondary : Palette::edge);
                g.fillEllipse (x + diameter * 0.25f, y + diameter * 0.25f,
                               diameter * 0.5f, diameter * 0.5f);
                break;

            case BeatAccent::normal:
            case BeatAccent::numLevels:
            default:
                g.setColour (lit ? Palette::accent : Palette::edge);
                g.fillEllipse (x + diameter * 0.15f, y + diameter * 0.15f,
                               diameter * 0.7f, diameter * 0.7f);
                break;
        }
    }
}

//==============================================================================
MetronomeTab::MetronomeTab (LuthierAudioProcessor& p)
    : PracticeTab (p)
{
    enableToggle = std::make_unique<LuthierToggle> ("METRONOME");
    enableToggle->getButton().setClickingTogglesState (true);
    enableToggle->getButton().onClick = [this]
    {
        if (! updatingControls)
            metronome().setEnabled (enableToggle->getButton().getToggleState());
    };
    addAndMakeVisible (*enableToggle);

    // practice-tools 0.2: the click goes to the monitor bus unless sent here.
    mainOutToggle = std::make_unique<LuthierToggle> ("CLICK TO MAIN");
    mainOutToggle->getButton().setClickingTogglesState (true);
    mainOutToggle->getButton().setTooltip ("Send the click (and the TUNE tab's) to the main output "
                                           "instead of the monitor bus. Without a monitor bus it "
                                           "always goes to the main output.");
    mainOutToggle->getButton().onClick = [this]
    {
        if (! updatingControls)
            processor.setClickToMain (mainOutToggle->getButton().getToggleState());
    };
    addAndMakeVisible (*mainOutToggle);

    styleSlider (tempoSlider, 20.0, 300.0, 1.0, " bpm");
    tempoSlider.onValueChange = [this]
    {
        if (! updatingControls)
            metronome().setTempo (tempoSlider.getValue());
    };
    addAndMakeVisible (tempoSlider);

    // practice-tools 1's time signatures.
    struct Signature { int numerator, denominator; };

    static const Signature signatures[] =
    {
        { 2, 4 }, { 3, 4 }, { 4, 4 }, { 5, 4 }, { 6, 8 },
        { 7, 4 }, { 7, 8 }, { 9, 8 }, { 12, 8 }
    };

    int itemId = 1;

    for (const auto& signature : signatures)
        signatureBox.addItem (juce::String (signature.numerator) + "/"
                                + juce::String (signature.denominator), itemId++);

    signatureBox.onChange = [this]
    {
        if (updatingControls)
            return;

        const auto text = signatureBox.getText();
        const auto parts = juce::StringArray::fromTokens (text, "/", "");

        if (parts.size() == 2)
            metronome().setTimeSignature (parts[0].getIntValue(), parts[1].getIntValue());
    };

    addAndMakeVisible (signatureBox);

    for (int i = 0; i < (int) ClickSubdivision::numSubdivisions; ++i)
        subdivisionBox.addItem (getClickSubdivisionName ((ClickSubdivision) i), i + 1);

    subdivisionBox.onChange = [this]
    {
        if (! updatingControls)
            metronome().setSubdivision ((ClickSubdivision) (subdivisionBox.getSelectedId() - 1));
    };

    addAndMakeVisible (subdivisionBox);

    for (int i = 0; i < (int) ClickSound::numSounds; ++i)
        soundBox.addItem (getClickSoundName ((ClickSound) i), i + 1);

    soundBox.onChange = [this]
    {
        if (! updatingControls)
            metronome().setSound ((ClickSound) (soundBox.getSelectedId() - 1));
    };

    addAndMakeVisible (soundBox);

    styleSlider (levelSlider, -40.0, 6.0, 0.5, " dB");
    levelSlider.onValueChange = [this]
    {
        if (! updatingControls)
            metronome().setLevelDb (levelSlider.getValue());
    };
    addAndMakeVisible (levelSlider);

    styleSlider (silentBarsSlider, 0.0, 16.0, 1.0, "");
    silentBarsSlider.setTooltip ("Mute every Nth bar, so you have to keep time yourself. "
                                 "Zero never mutes.");
    silentBarsSlider.onValueChange = [this]
    {
        if (! updatingControls)
            metronome().setSilentBarPeriod ((int) silentBarsSlider.getValue());
    };
    addAndMakeVisible (silentBarsSlider);

    // ---- progressive tempo -----------------------------------------------------------
    styleSlider (fromSlider, 20.0, 300.0, 1.0, " bpm");
    styleSlider (toSlider, 20.0, 300.0, 1.0, " bpm");
    styleSlider (overBarsSlider, 1.0, 64.0, 1.0, " bars");

    fromSlider.setValue (80.0, juce::dontSendNotification);
    toSlider.setValue (120.0, juce::dontSendNotification);
    overBarsSlider.setValue (8.0, juce::dontSendNotification);

    addAndMakeVisible (fromSlider);
    addAndMakeVisible (toSlider);
    addAndMakeVisible (overBarsSlider);

    rampButton.setTooltip ("Ramp the tempo from one to the other over that many bars.");
    rampButton.onClick = [this]
    {
        metronome().startProgressiveTempo (fromSlider.getValue(), toSlider.getValue(),
                                           (int) overBarsSlider.getValue());
        metronome().setEnabled (true);
    };

    addAndMakeVisible (rampButton);

    // ---- accents ---------------------------------------------------------------------
    for (int beat = 0; beat < 12; ++beat)
    {
        auto* button = accentButtons.add (new juce::TextButton (juce::String (beat + 1)));

        button->setTooltip ("Click to cycle this beat between accent, normal, ghost and silent.");

        button->onClick = [this, beat]
        {
            const auto now = metronome().getBeatAccent (beat);
            const int next = ((int) now + 1) % (int) BeatAccent::numLevels;

            metronome().setBeatAccent (beat, (BeatAccent) next);
            refresh();
        };

        addAndMakeVisible (*button);
    }

    indicator = std::make_unique<BeatIndicator> (processor);
    addAndMakeVisible (*indicator);

    refresh();
}

Metronome& MetronomeTab::metronome()
{
    return processor.getMetronome();
}

void MetronomeTab::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    auto& m = metronome();

    enableToggle->getButton().setToggleState (m.isEnabled(), juce::dontSendNotification);
    mainOutToggle->getButton().setToggleState (processor.isClickToMain(), juce::dontSendNotification);
    tempoSlider.setValue (m.getTempo(), juce::dontSendNotification);

    const auto signature = m.getTimeSignature();
    const auto text = juce::String (signature.numerator) + "/" + juce::String (signature.denominator);

    for (int i = 0; i < signatureBox.getNumItems(); ++i)
        if (signatureBox.getItemText (i) == text)
            signatureBox.setSelectedItemIndex (i, juce::dontSendNotification);

    subdivisionBox.setSelectedId ((int) m.getSubdivision() + 1, juce::dontSendNotification);
    soundBox.setSelectedId ((int) m.getSound() + 1, juce::dontSendNotification);
    levelSlider.setValue (m.getLevelDb(), juce::dontSendNotification);
    silentBarsSlider.setValue (m.getSilentBarPeriod(), juce::dontSendNotification);

    // The accent buttons show their level as text, not just colour.
    for (int beat = 0; beat < accentButtons.size(); ++beat)
    {
        auto* button = accentButtons[beat];

        const bool inBar = beat < signature.numerator;

        button->setVisible (inBar);

        if (! inBar)
            continue;

        switch (m.getBeatAccent (beat))
        {
            case BeatAccent::accent: button->setButtonText (">" + juce::String (beat + 1)); break;
            case BeatAccent::normal: button->setButtonText (juce::String (beat + 1)); break;
            case BeatAccent::ghost:  button->setButtonText ("." + juce::String (beat + 1)); break;
            case BeatAccent::silent: button->setButtonText ("-"); break;
            default: break;
        }
    }

    indicator->refresh();
}

void MetronomeTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    auto row = [&bounds] (int height, int gap = 3)
    {
        auto r = bounds.removeFromTop (height);
        bounds.removeFromTop (gap);
        return r;
    };

    {
        RowLayout r { row (Metrics::buttonHeight) };
        enableToggle->setBounds (r.take (130));
        mainOutToggle->setBounds (r.take (150));
        indicator->setBounds (r.rest());
    }

    {
        RowLayout r { row (22) };
        tempoSlider.setBounds (r.take (juce::jmax (160, r.bounds.getWidth() / 3)));
        signatureBox.setBounds (r.take (80));
        subdivisionBox.setBounds (r.take (120));
        soundBox.setBounds (r.rest());
    }

    {
        RowLayout r { row (22) };
        const int half = r.bounds.getWidth() / 2 - 2;
        levelSlider.setBounds (r.take (half));
        silentBarsSlider.setBounds (r.rest());
    }

    // Accents.
    {
        auto r = row (22);
        const int width = juce::jmax (22, r.getWidth() / 12);

        for (auto* button : accentButtons)
            button->setBounds (r.removeFromLeft (width).reduced (1, 0));
    }

    // Progressive tempo.
    {
        RowLayout r { row (22) };
        const int quarter = r.bounds.getWidth() / 4 - 4;

        fromSlider.setBounds (r.take (quarter));
        toSlider.setBounds (r.take (quarter));
        overBarsSlider.setBounds (r.take (quarter));
        rampButton.setBounds (r.rest());
    }
}

//==============================================================================
LooperTab::LooperTab (LuthierAudioProcessor& p)
    : PracticeTab (p)
{
    transportButton.setTooltip ("Record, then close the loop, then overdub. One button, "
                                "like a looper pedal.");
    transportButton.onClick = [this] { looper().press(); refresh(); };
    addAndMakeVisible (transportButton);

    stopButton.onClick = [this] { looper().stop(); refresh(); };
    addAndMakeVisible (stopButton);

    clearButton.onClick = [this] { looper().clear(); refresh(); };
    addAndMakeVisible (clearButton);

    styleReadout (statusLabel);
    addAndMakeVisible (statusLabel);

    for (int i = 0; i < Looper::kMaxLayers; ++i)
    {
        auto& strip = layers[(size_t) i];

        strip.select = std::make_unique<juce::TextButton> (juce::String (i + 1));
        strip.select->onClick = [this, i] { looper().setActiveLayer (i); refresh(); };
        addAndMakeVisible (*strip.select);

        strip.mute = std::make_unique<juce::TextButton> ("M");
        strip.mute->setClickingTogglesState (true);
        strip.mute->onClick = [this, i]
        {
            looper().getLayer (i).setMuted (layers[(size_t) i].mute->getToggleState());
        };
        addAndMakeVisible (*strip.mute);

        strip.reverse = std::make_unique<juce::TextButton> ("Rev");
        strip.reverse->setClickingTogglesState (true);
        strip.reverse->onClick = [this, i]
        {
            looper().getLayer (i).setReversed (layers[(size_t) i].reverse->getToggleState());
        };
        addAndMakeVisible (*strip.reverse);

        strip.halfSpeed = std::make_unique<juce::TextButton> ("1/2");
        strip.halfSpeed->setClickingTogglesState (true);
        strip.halfSpeed->onClick = [this, i]
        {
            looper().getLayer (i).setHalfSpeed (layers[(size_t) i].halfSpeed->getToggleState());
        };
        addAndMakeVisible (*strip.halfSpeed);

        strip.mode = std::make_unique<juce::ComboBox>();

        for (int m = 0; m < (int) LayerMode::numModes; ++m)
            strip.mode->addItem (getLayerModeName ((LayerMode) m), m + 1);

        strip.mode->setSelectedId (1, juce::dontSendNotification);
        strip.mode->onChange = [this, i]
        {
            looper().getLayer (i).setMode ((LayerMode) (layers[(size_t) i].mode->getSelectedId() - 1));
        };
        addAndMakeVisible (*strip.mode);

        strip.level = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal,
                                                      juce::Slider::NoTextBox);
        strip.level->setRange (-40.0, 6.0, 0.5);
        strip.level->setValue (0.0, juce::dontSendNotification);
        strip.level->onValueChange = [this, i]
        {
            looper().getLayer (i).setLevelDb (layers[(size_t) i].level->getValue());
        };
        addAndMakeVisible (*strip.level);

        strip.pan = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal,
                                                    juce::Slider::NoTextBox);
        strip.pan->setRange (-1.0, 1.0, 0.01);
        strip.pan->setValue (0.0, juce::dontSendNotification);
        strip.pan->onValueChange = [this, i]
        {
            looper().getLayer (i).setPan (layers[(size_t) i].pan->getValue());
        };
        addAndMakeVisible (*strip.pan);

        strip.undo = std::make_unique<juce::TextButton> ("Undo");
        strip.undo->onClick = [this, i]
        {
            auto& layer = looper().getLayer (i);

            if (! layer.undo())
                layer.redo();

            refresh();
        };
        addAndMakeVisible (*strip.undo);
    }

    exportMix.setTooltip ("Bounce every audible layer to one WAV.");
    exportMix.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Bounce the loop", Looper::getUserDirectory().getChildFile ("loop.wav"), "*.wav");

        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
                looper().exportMixdown (fc.getResult());
        });
    };

    exportStems.setTooltip ("Write each layer to its own WAV.");
    exportStems.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Choose a folder for the stems", Looper::getUserDirectory());

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
                looper().exportStems (fc.getResult());
        });
    };

    saveButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Save the loop",
            Looper::getUserDirectory().getChildFile ("My Loop" + juce::String (Looper::kFileExtension)),
            juce::String ("*") + Looper::kFileExtension);

        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
                looper().save (fc.getResult());
        });
    };

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Open a loop", Looper::getUserDirectory(),
            juce::String ("*") + Looper::kFileExtension);

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
                looper().load (fc.getResult());

            refresh();
        });
    };

    for (auto* button : { &exportMix, &exportStems, &saveButton, &loadButton })
        addAndMakeVisible (*button);

    refresh();
}

Looper& LooperTab::looper()
{
    return processor.getLooper();
}

void LooperTab::refresh()
{
    auto& l = looper();

    switch (l.getState())
    {
        case Looper::State::stopped:
            transportButton.setButtonText (l.getLoopLengthSamples() > 0 ? "Play" : "Record");
            break;

        case Looper::State::recordingFirst:
            transportButton.setButtonText ("Close loop");
            break;

        case Looper::State::playing:
            transportButton.setButtonText ("Overdub");
            break;

        case Looper::State::overdubbing:
            transportButton.setButtonText ("Stop overdub");
            break;

        default:
            break;
    }

    statusLabel.setText (l.getLoopLengthSamples() > 0
                           ? (juce::String (l.getLoopSeconds(), 2) + " s    "
                                + juce::String (l.getNumRecordedLayers()) + " of "
                                + juce::String (Looper::kMaxLayers) + " layers")
                           : juce::String ("No loop yet."),
                         juce::dontSendNotification);

    for (int i = 0; i < Looper::kMaxLayers; ++i)
    {
        auto& strip = layers[(size_t) i];
        auto& layer = l.getLayer (i);

        const bool active = i == l.getActiveLayer();
        const bool hasContent = layer.hasContent();

        // The active layer is marked with a bracket as well as being lit, so it
        // reads without relying on colour.
        strip.select->setButtonText (active ? ("[" + juce::String (i + 1) + "]")
                                            : juce::String (i + 1));

        strip.select->setEnabled (true);
        strip.mute->setToggleState (layer.isMuted(), juce::dontSendNotification);
        strip.reverse->setToggleState (layer.isReversed(), juce::dontSendNotification);
        strip.halfSpeed->setToggleState (layer.isHalfSpeed(), juce::dontSendNotification);
        strip.mode->setSelectedId ((int) layer.getMode() + 1, juce::dontSendNotification);

        strip.undo->setButtonText (layer.canUndo() ? "Undo" : (layer.canRedo() ? "Redo" : "Undo"));
        strip.undo->setEnabled (layer.canUndo() || layer.canRedo());

        for (auto* control : { (juce::Component*) strip.mute.get(),
                               (juce::Component*) strip.reverse.get(),
                               (juce::Component*) strip.halfSpeed.get(),
                               (juce::Component*) strip.mode.get(),
                               (juce::Component*) strip.level.get(),
                               (juce::Component*) strip.pan.get() })
            control->setEnabled (hasContent);
    }
}

void LooperTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    {
        RowLayout r { bounds.removeFromTop (Metrics::buttonHeight) };

        transportButton.setBounds (r.take (110));
        stopButton.setBounds (r.take (64));
        clearButton.setBounds (r.take (64));
        statusLabel.setBounds (r.rest());
    }

    bounds.removeFromTop (3);

    auto footer = bounds.removeFromBottom (Metrics::buttonHeight);

    {
        RowLayout r { footer };
        exportMix.setBounds (r.take (80));
        exportStems.setBounds (r.take (70));
        saveButton.setBounds (r.take (64));
        loadButton.setBounds (r.take (64));
    }

    bounds.removeFromBottom (3);

    const int stripHeight = juce::jmax (16, bounds.getHeight() / Looper::kMaxLayers);

    for (int i = 0; i < Looper::kMaxLayers; ++i)
    {
        auto& strip = layers[(size_t) i];

        RowLayout r { bounds.removeFromTop (stripHeight).reduced (0, 1) };

        strip.select->setBounds (r.take (34, 2));
        strip.mute->setBounds (r.take (24, 2));
        strip.mode->setBounds (r.take (88, 2));
        strip.reverse->setBounds (r.take (40, 2));
        strip.halfSpeed->setBounds (r.take (34, 2));
        strip.undo->setBounds (r.take (52, 4));

        const int half = juce::jmax (40, r.bounds.getWidth() / 2 - 2);

        strip.level->setBounds (r.take (half));
        strip.pan->setBounds (r.rest());
    }
}

//==============================================================================
TrackTab::TrackTab (LuthierAudioProcessor& p)
    : PracticeTab (p)
{
    openButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Open a backing track",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
                track().load (fc.getResult());

            refresh();
        });
    };

    playButton.onClick = [this]
    {
        if (track().isPlaying())
            track().pause();
        else
            track().play();

        refresh();
    };

    stopButton.onClick = [this] { track().stop(); refresh(); };

    for (auto* button : { &openButton, &playButton, &stopButton })
        addAndMakeVisible (*button);

    styleReadout (titleLabel, Palette::textPrimary);
    styleReadout (positionLabel);
    styleReadout (tempoLabel);

    for (auto* label : { &titleLabel, &positionLabel, &tempoLabel })
        addAndMakeVisible (*label);

    positionSlider.setRange (0.0, 1.0, 0.0001);
    positionSlider.onDragStart = [this] { draggingPosition = true; };
    positionSlider.onDragEnd = [this]
    {
        track().setPositionSeconds (positionSlider.getValue() * track().getLengthSeconds());
        draggingPosition = false;
    };
    addAndMakeVisible (positionSlider);

    styleSlider (levelSlider, -40.0, 6.0, 0.5, " dB");
    levelSlider.setValue (-6.0, juce::dontSendNotification);
    levelSlider.onValueChange = [this] { track().setLevelDb (levelSlider.getValue()); };
    addAndMakeVisible (levelSlider);

    styleSlider (pitchSlider, -12.0, 12.0, 1.0, " st");
    pitchSlider.onValueChange = [this] { track().setPitchShiftSemitones (pitchSlider.getValue()); };
    addAndMakeVisible (pitchSlider);

    styleSlider (tempoSlider, 25.0, 200.0, 1.0, " %");
    tempoSlider.setValue (100.0, juce::dontSendNotification);
    tempoSlider.onValueChange = [this] { track().setTempoRatio (tempoSlider.getValue() * 0.01); };
    addAndMakeVisible (tempoSlider);

    loopButton.setClickingTogglesState (true);
    loopButton.onClick = [this] { track().setLoopEnabled (loopButton.getToggleState()); };
    addAndMakeVisible (loopButton);

    monoButton.setClickingTogglesState (true);
    monoButton.onClick = [this] { track().setMonoSum (monoButton.getToggleState()); };
    addAndMakeVisible (monoButton);

    setLoopStart.setTooltip ("Set the loop's start to where the track is now, snapped "
                             "to a zero crossing.");
    setLoopStart.onClick = [this]
    {
        track().setLoopSeconds (track().getPositionSeconds(), track().getLoopEndSeconds());
    };

    setLoopEnd.onClick = [this]
    {
        track().setLoopSeconds (track().getLoopStartSeconds(), track().getPositionSeconds());
    };

    addAndMakeVisible (setLoopStart);
    addAndMakeVisible (setLoopEnd);

    addMarker.onClick = [this]
    {
        track().addMarker ({}, track().getPositionSeconds());
        refresh();
    };
    addAndMakeVisible (addMarker);

    markerBox.onChange = [this]
    {
        if (markerBox.getSelectedId() > 0)
            track().jumpToMarker (markerBox.getSelectedId() - 1);
    };
    addAndMakeVisible (markerBox);

    refresh();
}

BackingTrackPlayer& TrackTab::track()
{
    return processor.getBackingTrack();
}

void TrackTab::refresh()
{
    auto& t = track();

    titleLabel.setText (t.isLoaded() ? t.getTitle() : juce::String ("No track loaded."),
                        juce::dontSendNotification);

    playButton.setButtonText (t.isPlaying() ? "Pause" : "Play");
    playButton.setEnabled (t.isLoaded());
    stopButton.setEnabled (t.isLoaded());

    const double position = t.getPositionSeconds();
    const double length = t.getLengthSeconds();

    auto format = [] (double seconds)
    {
        const int total = (int) seconds;
        return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
    };

    positionLabel.setText (t.isLoaded() ? (format (position) + " / " + format (length))
                                        : juce::String(),
                           juce::dontSendNotification);

    tempoLabel.setText (t.getDetectedTempo() > 0.0
                          ? (juce::String (t.getDetectedTempo(), 0) + " bpm detected")
                          : juce::String(),
                        juce::dontSendNotification);

    if (! draggingPosition && length > 0.0)
        positionSlider.setValue (position / length, juce::dontSendNotification);

    loopButton.setToggleState (t.isLoopEnabled(), juce::dontSendNotification);
    monoButton.setToggleState (t.isMonoSummed(), juce::dontSendNotification);

    // Rebuild the marker list only when it has changed.
    if (markerBox.getNumItems() != t.getNumMarkers())
    {
        markerBox.clear (juce::dontSendNotification);

        for (int i = 0; i < t.getNumMarkers(); ++i)
        {
            const auto marker = t.getMarker (i);

            markerBox.addItem (marker.name + "  " + format (marker.seconds), i + 1);
        }
    }
}

void TrackTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    auto row = [&bounds] (int height, int gap = 3)
    {
        auto r = bounds.removeFromTop (height);
        bounds.removeFromTop (gap);
        return r;
    };

    {
        RowLayout r { row (Metrics::buttonHeight) };
        openButton.setBounds (r.take (76));
        playButton.setBounds (r.take (64));
        stopButton.setBounds (r.take (56));
        titleLabel.setBounds (r.rest());
    }

    {
        RowLayout r { row (20) };
        positionLabel.setBounds (r.take (100));
        tempoLabel.setBounds (r.bounds.removeFromRight (120));
        positionSlider.setBounds (r.rest());
    }

    {
        RowLayout r { row (22) };
        const int third = r.bounds.getWidth() / 3 - 4;

        levelSlider.setBounds (r.take (third));
        pitchSlider.setBounds (r.take (third));
        tempoSlider.setBounds (r.rest());
    }

    {
        RowLayout r { row (Metrics::buttonHeight) };

        loopButton.setBounds (r.take (56));
        setLoopStart.setBounds (r.take (70));
        setLoopEnd.setBounds (r.take (70));
        monoButton.setBounds (r.take (56));
        addMarker.setBounds (r.take (56));
        markerBox.setBounds (r.rest());
    }
}

//==============================================================================
ScaleTab::ScaleTab (LuthierAudioProcessor& p)
    : PracticeTab (p)
{
    static const char* const noteNames[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    for (int i = 0; i < 12; ++i)
        keyBox.addItem (noteNames[i], i + 1);

    keyBox.setSelectedId (1, juce::dontSendNotification);
    keyBox.onChange = [this] { trainer().setKey (keyBox.getSelectedId() - 1); repaint(); };
    addAndMakeVisible (keyBox);

    for (int i = 0; i < (int) ScaleType::custom; ++i)
        scaleBox.addItem (getScaleTypeName ((ScaleType) i), i + 1);

    scaleBox.setSelectedId (1, juce::dontSendNotification);
    scaleBox.onChange = [this]
    {
        trainer().setScale ((ScaleType) (scaleBox.getSelectedId() - 1));
        repaint();
    };
    addAndMakeVisible (scaleBox);

    modeBox.addItem ("Explore", 1);
    modeBox.addItem ("Quiz", 2);
    modeBox.addItem ("Interval trainer", 3);
    modeBox.addItem ("Chord tone trainer", 4);
    modeBox.setSelectedId (1, juce::dontSendNotification);

    modeBox.onChange = [this]
    {
        trainer().setMode ((ScaleTrainer::Mode) (modeBox.getSelectedId() - 1));
        refresh();
    };

    addAndMakeVisible (modeBox);

    styleReadout (questionLabel, Palette::textPrimary);
    questionLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    addAndMakeVisible (questionLabel);

    styleReadout (scoreLabel);
    addAndMakeVisible (scoreLabel);

    nextButton.setTooltip ("Ask the next question. After the session's question count "
                           "(set on the PRACTICE tab) it shows the score, and the next press "
                           "starts a new session.");
    nextButton.onClick = [this]
    {
        auto& t = trainer();

        // 11.2 question count: the summary is shown once; the press after it
        // files the completed session in the history and starts the next.
        if (t.isSessionComplete())
        {
            if (summaryShown)
            {
                processor.getPracticeActivityTracker().recordScaleTrainer (t, processor.getPracticeStats(),
                                                                           PracticeStats::today());
                t.resetScore();
                summaryShown = false;
            }
            else
            {
                summaryShown = true;
            }
        }

        questionLabel.setText (t.nextQuestion (random), juce::dontSendNotification);
        refresh();
    };
    addAndMakeVisible (nextButton);

    addAndMakeVisible (scaleView);

    refresh();
}

ScaleTrainer& ScaleTab::trainer()
{
    return processor.getScaleTrainer();
}

void ScaleTab::refresh()
{
    auto& t = trainer();

    // "3 of 5" against the session's count when there is one, "3 of 5 asked"
    // when the trainer runs open-ended.
    const juce::String asked = t.getQuestionCount() > 0
                                 ? juce::String (t.getAsked()) + "/" + juce::String (t.getQuestionCount())
                                 : juce::String (t.getAsked());

    scoreLabel.setText (t.getAsked() > 0
                          ? (juce::String (t.getScore()) + " of " + asked
                               + "   " + juce::String (t.getSuccessRate() * 100.0, 0) + "%")
                          : juce::String(),
                        juce::dontSendNotification);

    nextButton.setButtonText (t.isSessionComplete() && summaryShown ? "New" : "Ask");
    nextButton.setEnabled (t.getMode() != ScaleTrainer::Mode::explore);

    repaint();
}

void ScaleTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    {
        RowLayout r { bounds.removeFromTop (Metrics::buttonHeight) };

        keyBox.setBounds (r.take (64));
        scaleBox.setBounds (r.take (150));
        modeBox.setBounds (r.take (150));
        nextButton.setBounds (r.take (64));
        scoreLabel.setBounds (r.rest());
    }

    bounds.removeFromTop (3);
    questionLabel.setBounds (bounds.removeFromTop (20));
    bounds.removeFromTop (3);

    scaleView.setBounds (bounds);
}

//==============================================================================
EarTab::EarTab (LuthierAudioProcessor& p)
    : PracticeTab (p)
{
    exerciseBox.addItem ("Intervals", 1);
    exerciseBox.addItem ("Chord quality", 2);
    exerciseBox.addItem ("Progressions", 3);
    exerciseBox.setSelectedId (1, juce::dontSendNotification);

    exerciseBox.onChange = [this]
    {
        trainer().setExercise ((EarTrainer::Exercise) (exerciseBox.getSelectedId() - 1));
        refresh();
    };

    addAndMakeVisible (exerciseBox);

    nextButton.setTooltip ("Play a new question. After the session's question count "
                           "(set on the PRACTICE tab) it shows the score, and the next press "
                           "starts a new session.");
    nextButton.onClick = [this]
    {
        auto& t = trainer();

        if (t.isSessionComplete())
        {
            if (summaryShown)
            {
                processor.getPracticeActivityTracker().recordEarTrainer (t, processor.getPracticeStats(),
                                                                         PracticeStats::today());
                t.resetScore();
                summaryShown = false;
            }
            else
            {
                summaryShown = true;
            }
        }

        numNotes = t.nextQuestion (random, notes, offsets, EarTrainer::kMaxNotesInQuestion);
        feedbackLabel.setText ({}, juce::dontSendNotification);
        playCurrentQuestion();
        refresh();
    };

    playButton.onClick = [this] { playCurrentQuestion(); };

    adaptiveButton.setClickingTogglesState (true);
    adaptiveButton.setToggleState (true, juce::dontSendNotification);
    adaptiveButton.setTooltip ("Let the difficulty follow how you are doing.");
    adaptiveButton.onClick = [this]
    {
        trainer().setAdaptive (adaptiveButton.getToggleState());
    };

    for (auto* button : { &playButton, &nextButton, &adaptiveButton })
        addAndMakeVisible (*button);

    styleReadout (questionLabel, Palette::textPrimary);
    questionLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    addAndMakeVisible (questionLabel);

    styleReadout (scoreLabel);
    addAndMakeVisible (scoreLabel);

    styleReadout (feedbackLabel, Palette::accent);
    addAndMakeVisible (feedbackLabel);

    // Enough buttons for the longest answer list any exercise offers.
    for (int i = 0; i < 16; ++i)
    {
        auto* button = choiceButtons.add (new juce::TextButton());

        button->onClick = [this, i]
        {
            const bool right = trainer().answer (i);

            feedbackLabel.setText (right ? "Correct." : ("No: " + trainer().getChoices()
                                                           [trainer().getCorrectChoice()]),
                                   juce::dontSendNotification);

            feedbackLabel.setColour (juce::Label::textColourId,
                                     right ? Palette::success : Palette::warning);

            refresh();
        };

        addChildComponent (*button);
    }

    refresh();
}

EarTrainer& EarTab::trainer()
{
    return processor.getEarTrainer();
}

void EarTab::playCurrentQuestion()
{
    // practice-tools 5: the exercise is heard on the instrument the user has
    // built, so the notes go through the plugin's own preview path.
    for (int i = 0; i < numNotes; ++i)
    {
        juce::ignoreUnused (offsets[i]);

        // The preview path takes a string and a fret; the trainer works in
        // pitches, so each note is placed wherever the voicer would put it.
        const int midiNote = notes[i];

        processor.triggerPreviewNote (juce::jlimit (0, 5, i % 6),
                                      juce::jlimit (0.0, 22.0, (double) (midiNote % 24)),
                                      0.75);
    }
}

void EarTab::refresh()
{
    auto& t = trainer();

    questionLabel.setText (t.getQuestionText(), juce::dontSendNotification);

    const juce::String asked = t.getQuestionCount() > 0
                                 ? juce::String (t.getAsked()) + "/" + juce::String (t.getQuestionCount())
                                 : juce::String (t.getAsked());

    scoreLabel.setText (juce::String (t.getCorrect()) + " of " + asked
                          + "    level " + juce::String (t.getDifficulty() + 1)
                          + "    streak " + juce::String (t.getStreak()),
                        juce::dontSendNotification);

    nextButton.setButtonText (t.isSessionComplete() && summaryShown ? "Again" : "New");

    const auto choices = t.getChoices();

    for (int i = 0; i < choiceButtons.size(); ++i)
    {
        auto* button = choiceButtons[i];

        const bool used = i < choices.size();

        button->setVisible (used);

        if (used)
            button->setButtonText (choices[i]);
    }

    playButton.setEnabled (numNotes > 0);

    resized();
}

void EarTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    {
        RowLayout r { bounds.removeFromTop (Metrics::buttonHeight) };

        exerciseBox.setBounds (r.take (140));
        nextButton.setBounds (r.take (64));
        playButton.setBounds (r.take (64));
        adaptiveButton.setBounds (r.take (86));
        scoreLabel.setBounds (r.rest());
    }

    bounds.removeFromTop (3);
    questionLabel.setBounds (bounds.removeFromTop (20));
    feedbackLabel.setBounds (bounds.removeFromBottom (16));

    bounds.removeFromTop (3);

    // The answer buttons wrap onto as many rows as they need.
    const auto choices = processor.getEarTrainer().getChoices();

    if (choices.isEmpty())
        return;

    const int perRow = juce::jmax (1, bounds.getWidth() / 130);
    const int rows = (choices.size() + perRow - 1) / perRow;
    const int rowHeight = juce::jmax (20, juce::jmin (26, bounds.getHeight() / juce::jmax (1, rows)));

    int index = 0;

    for (int row = 0; row < rows; ++row)
    {
        auto r = bounds.removeFromTop (rowHeight);

        for (int column = 0; column < perRow && index < choices.size(); ++column, ++index)
            choiceButtons[index]->setBounds (
                r.removeFromLeft (r.getWidth() / juce::jmax (1, perRow - column)).reduced (1));
    }
}

//==============================================================================
TabReaderTab::TabReaderTab (LuthierAudioProcessor& p)
    : PracticeTab (p)
{
    openButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Open tablature",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.txt;*.tab;*.musicxml;*.xml");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
                openTabFile (fc.getResult());
        });
    };

    openButton.setTooltip ("Open a tab file (ASCII tab or MusicXML) to play along to. "
                           "It is added to the recent list on the PRACTICE tab.");
    AccessibleSetup::configureButton (openButton, "Open tab file",
                                      "Open a tab file to play along to.");

    exportButton.onClick = [this]
    {
        const auto format = (NotationFormat) juce::jlimit (
            0, (int) NotationFormat::numFormats - 1, formatBox.getSelectedId() - 1);

        chooser = std::make_unique<juce::FileChooser> (
            "Export notation",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
              .getChildFile ("Luthier" + juce::String (getNotationFormatExtension (format))),
            juce::String ("*") + getNotationFormatExtension (format));

        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, format] (const juce::FileChooser& fc)
        {
            if (fc.getResult() == juce::File())
                return;

            NotationExportOptions options;
            options.lineWidth = 80;

            statusLabel.setText (exporter.write (score, format, fc.getResult(), options)
                                   ? ("Exported to " + fc.getResult().getFileName())
                                   : exporter.getLastError(),
                                 juce::dontSendNotification);
        });
    };

    addAndMakeVisible (openButton);
    addAndMakeVisible (exportButton);

    for (int i = 0; i < (int) NotationFormat::numFormats; ++i)
        formatBox.addItem (getNotationFormatName ((NotationFormat) i), i + 1);

    formatBox.setSelectedId (3, juce::dontSendNotification);   // ASCII tab

    formatBox.onChange = [this]
    {
        const auto format = (NotationFormat) juce::jmax (0, formatBox.getSelectedId() - 1);

        statusLabel.setText (getNotationFormatLoss (format), juce::dontSendNotification);
    };

    addAndMakeVisible (formatBox);

    styleSlider (barsSlider, 1.0, 8.0, 1.0, " bars");
    barsSlider.setValue (4.0, juce::dontSendNotification);
    barsSlider.onValueChange = [this] { refresh(); };
    addAndMakeVisible (barsSlider);

    styleReadout (statusLabel);
    addAndMakeVisible (statusLabel);

    tabView.setMultiLine (true);
    tabView.setReadOnly (true);
    tabView.setScrollbarsShown (true);
    tabView.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                    11.0f, juce::Font::plain)));
    tabView.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    tabView.setColour (juce::TextEditor::textColourId, Palette::textPrimary);
    addAndMakeVisible (tabView);

    refresh();
}

bool TabReaderTab::openTabFile (const juce::File& file)
{
    const bool read = importer.read (file, score);

    if (read)
    {
        // practice-tools 11.2: the PRACTICE tab lists recent tab files. The
        // list is read back first, so two windows' opens merge rather than the
        // last one winning.
        PracticeLibrary library;
        library.load (libraryFile);
        library.noteTabOpened (file);

        juce::String error;
        library.save (libraryFile, error);

        statusLabel.setText (juce::String (score.getTotalNoteCount()) + " notes from "
                               + file.getFileName(),
                             juce::dontSendNotification);

        statusLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    }
    else
    {
        statusLabel.setText (importer.getLastError(), juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, Palette::warning);
    }

    refresh();
    return read;
}

void TabReaderTab::refresh()
{
    // notation-export 3: the live view shows a window of bars.
    NotationExportOptions options;
    options.lineWidth = 200;

    const auto text = exporter.renderAsciiTabWindow (score, 0, (int) barsSlider.getValue(), options);

    if (text != tabView.getText())
        tabView.setText (text, false);
}

void TabReaderTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    {
        RowLayout r { bounds.removeFromTop (Metrics::buttonHeight) };

        openButton.setBounds (r.take (76));
        formatBox.setBounds (r.take (120));
        exportButton.setBounds (r.take (80));
        barsSlider.setBounds (r.take (130));
        statusLabel.setBounds (r.rest());
    }

    bounds.removeFromTop (3);
    tabView.setBounds (bounds);
}

//==============================================================================
ProgressionTab::ProgressionTab (LuthierAudioProcessor& p)
    : PracticeTab (p)
{
    entryBox.setMultiLine (false);
    entryBox.setText ("Am - F - C - G x4");
    entryBox.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    entryBox.onReturnKey = [this] { parseButton.triggerClick(); };
    addAndMakeVisible (entryBox);

    parseButton.onClick = [this]
    {
        auto& progression = processor.getProgressionLooper();

        if (progression.parse (entryBox.getText()))
        {
            statusLabel.setText (juce::String (progression.getNumChords()) + " chords, "
                                   + juce::String (progression.getRepeats()) + "x",
                                 juce::dontSendNotification);

            statusLabel.setColour (juce::Label::textColourId, Palette::textMuted);
        }
        else
        {
            statusLabel.setText ("That does not read as a chord progression.",
                                 juce::dontSendNotification);

            statusLabel.setColour (juce::Label::textColourId, Palette::warning);
        }
    };

    addAndMakeVisible (parseButton);

    playButton.setClickingTogglesState (true);
    playButton.setTooltip ("Voice and strum the progression through the rhythm engine.");

    playButton.onClick = [this]
    {
        // practice-tools 7: the progression is played by the rhythm engine, so
        // this switches that on and lets it free-run.
        auto& rhythm = processor.getEngine().getRhythmEngine();

        rhythm.setEnabled (playButton.getToggleState());
        rhythm.setFreeRun (playButton.getToggleState());
    };

    addAndMakeVisible (playButton);

    processor.getGenreKits().refresh();

    int itemId = 1;

    for (const auto& name : processor.getGenreKits().getNames())
        genreBox.addItem (name, itemId++);

    genreBox.setTextWhenNothingSelected ("Feel");

    genreBox.onChange = [this]
    {
        processor.applyGenreKit (genreBox.getSelectedId() - 1);
    };

    addAndMakeVisible (genreBox);

    styleReadout (statusLabel);
    addAndMakeVisible (statusLabel);

    styleReadout (currentChordLabel, Palette::accent);
    currentChordLabel.setFont (juce::Font (juce::FontOptions (20.0f)).boldened());
    currentChordLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (currentChordLabel);

    parseButton.triggerClick();
}

void ProgressionTab::refresh()
{
    const auto& progression = processor.getProgressionLooper();

    if (progression.getNumChords() == 0)
    {
        currentChordLabel.setText ("--", juce::dontSendNotification);
        return;
    }

    // Which chord the transport is on, so the display follows the loop.
    const double beats = processor.getEngine().getRhythmEngine().isEnabled()
                           ? (double) juce::Time::getMillisecondCounter() * 0.001
                               * processor.getEffectiveTempo() / 60.0
                           : 0.0;

    const int index = progression.getChordAtBeat (beats);

    if (index >= 0)
        currentChordLabel.setText (progression.getChord (index).symbol,
                                   juce::dontSendNotification);
}

void ProgressionTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    {
        RowLayout r { bounds.removeFromTop (Metrics::buttonHeight) };

        entryBox.setBounds (r.take (juce::jmax (200, r.bounds.getWidth() / 2)));
        parseButton.setBounds (r.take (56));
        playButton.setBounds (r.take (64));
        genreBox.setBounds (r.take (150));
        statusLabel.setBounds (r.rest());
    }

    bounds.removeFromTop (6);
    currentChordLabel.setBounds (bounds.removeFromTop (juce::jmin (48, bounds.getHeight())));
}

//==============================================================================
/*  midi-export 4.2 for the session recorder: "drag from the session recorder's
    own Save button". A click saves to the Sessions folder; pressing and dragging
    out of the window carries the take as files - the WAV and the MIDI, the MIDI
    in the Luthier profile or Generic with Alt - written when the drag starts, so
    it is the take as it stands. */
class SessionTab::SaveButton final : public juce::TextButton
{
public:
    explicit SaveButton (SessionTab& t)
        : juce::TextButton ("Save last take"), tab (t)
    {
        setTooltip ("Freeze what is in the buffer to a WAV and a MIDI file in your Sessions "
                    "folder. Drag this button into your DAW or a folder to drop the take there "
                    "instead; hold Alt while dragging for Generic MIDI.");
        AccessibleSetup::configureButton (*this, "Save last take",
                                          "Save the recorder's buffer as WAV and MIDI files, "
                                          "or drag it out of the plugin as files.");
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragStarted = false;
        juce::TextButton::mouseDown (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStarted || e.getDistanceFromDragStart() < 6 || ! isEnabled())
        {
            juce::TextButton::mouseDrag (e);
            return;
        }

        dragStarted = true;
        setState (buttonNormal);

        const auto files = tab.processor.getSessionRecorder().writeDragOutFiles (e.mods.isAltDown());

        if (files.isEmpty())
            return;

        juce::StringArray paths;

        for (const auto& f : files)
            paths.add (f.getFullPathName());

        juce::DragAndDropContainer::performExternalDragDropOfFiles (paths, false, this);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        // A drag is not a click: the take went with the drag, not to Sessions.
        if (dragStarted)
        {
            dragStarted = false;
            setState (buttonNormal);
            return;
        }

        juce::TextButton::mouseUp (e);
    }

private:
    SessionTab& tab;
    bool dragStarted = false;
};

//==============================================================================
SessionTab::SessionTab (LuthierAudioProcessor& p)
    : PracticeTab (p)
{
    enableToggle = std::make_unique<LuthierToggle> ("SESSION RECORDER");
    enableToggle->getButton().setClickingTogglesState (true);
    enableToggle->getButton().onClick = [this] { setRecorderEnabled (enableToggle->getButton().getToggleState()); };

    enableToggle->setTooltip ("Continuously record everything the plugin plays, and the MIDI "
                              "that played it, into a ring buffer so you can keep a take after "
                              "playing it. Stopping saves the take when auto-save is set on the "
                              "PRACTICE tab.");
    AccessibleSetup::configureButton (enableToggle->getButton(), "Session recorder",
                                      "Start or stop the session recorder.");

    addAndMakeVisible (*enableToggle);

    styleReadout (lengthLabel);
    lengthLabel.setTooltip ("How much the recorder keeps, and what. Set it on the PRACTICE tab.");
    addAndMakeVisible (lengthLabel);

    saveButton = std::make_unique<SaveButton> (*this);
    saveButton->onClick = [this]
    {
        SessionRecorder::SavedTake saved;
        const bool ok = processor.getSessionRecorder().saveLastTake (processor.getSessionRecorder().getSaveDirectory(),
                                                                     &saved);

        statusLabel.setText (ok ? ("Saved " + (saved.wav != juce::File() ? saved.wav : saved.midi).getFileName()
                                     + " to your Sessions folder.")
                                : "There is nothing recorded to save.",
                             juce::dontSendNotification);
    };

    openFolderButton.setTooltip ("Open your Sessions folder, where saved takes go.");
    AccessibleSetup::configureButton (openFolderButton, "Open sessions folder",
                                      "Open the folder saved takes are written to.");
    openFolderButton.onClick = [this]
    {
        const auto folder = processor.getSessionRecorder().getSaveDirectory();
        folder.createDirectory();
        folder.revealToUser();
    };

    addAndMakeVisible (*saveButton);
    addAndMakeVisible (openFolderButton);

    styleReadout (statusLabel);
    addAndMakeVisible (statusLabel);

    styleReadout (warningLabel, Palette::warning);
    addAndMakeVisible (warningLabel);

    refresh();
}

juce::Button& SessionTab::getSaveButton() noexcept
{
    return *saveButton;
}

void SessionTab::setRecorderEnabled (bool on)
{
    auto& recorder = processor.getSessionRecorder();

    if (on)
    {
        // The ring is sized from the PRACTICE tab's setup as it goes on, never
        // while it records; what it records and whether stopping saves come
        // from the same place. The MIDI file takes the MIDI OUT defaults and
        // the tempo in force, so a take opens in a DAW on the right grid.
        const auto setup = storedSetup();
        requestedMinutes = storedMinutes = setup.ringMinutes;

        if (! recorder.isEnabled())
            setup.applyTo (recorder, processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0);
        else
            setup.applySwitchesTo (recorder);

        recorder.setTempoBpm (processor.getEffectiveTempo());
        recorder.setMidiExportOptions (MidiExportDefaults::load());
        recorder.setEnabled (true);
        statusLabel.setText ({}, juce::dontSendNotification);
    }
    else
    {
        // 11.2 "Auto-save on stop": the stop writes the take when it is set.
        SessionRecorder::SavedTake saved;
        const bool wasOn = recorder.isEnabled();
        const bool wrote = recorder.stop (&saved);

        if (wrote)
            statusLabel.setText ("Auto-saved " + (saved.wav != juce::File() ? saved.wav : saved.midi).getFileName()
                                   + " to your Sessions folder.",
                                 juce::dontSendNotification);
        else if (wasOn && recorder.isAutoSaveOnStop())
            statusLabel.setText ("Nothing was recorded, so nothing was auto-saved.", juce::dontSendNotification);
    }

    refresh();
}

void SessionTab::visibilityChanged()
{
    if (isVisible())
    {
        storedMinutes = storedSetup().ringMinutes;
        refresh();
    }
}

SessionRecorderSetup SessionTab::storedSetup() const
{
    PracticeDefaults defaults;
    juce::String error;

    if (! PracticeDefaults::load (defaultsFile, defaults, error))
        return {};

    return SessionRecorderSetup::fromVar (defaults.extra["session_recorder"]);
}

void SessionTab::refresh()
{
    auto& recorder = processor.getSessionRecorder();

    enableToggle->getButton().setToggleState (recorder.isEnabled(), juce::dontSendNotification);

    const double minutes = recorder.getCapacityMinutes();
    const double recorded = (double) recorder.getRecordedSamples()
                              / juce::jmax (1.0, processor.getSampleRate()) / 60.0;

    if (recorder.isEnabled() || statusLabel.getText().isEmpty()
        || statusLabel.getText().endsWith ("minutes held"))
        statusLabel.setText (juce::String (recorded, 1) + " of " + juce::String (minutes, 1)
                               + " minutes held",
                             juce::dontSendNotification);

    // practice-tools 8 sizes the default at 1.4 GB, which this machine will not
    // allocate. The capacity is reported rather than the request, and the
    // difference is stated rather than hidden.
    const juce::String what = recorder.isRecordingAudio() && recorder.isRecordingMidi() ? "audio + MIDI"
                            : recorder.isRecordingMidi() ? "MIDI only" : "audio only";

    lengthLabel.setText ("Keeps " + juce::String (storedMinutes, 0) + " min, " + what,
                         juce::dontSendNotification);

    warningLabel.setText (recorder.isEnabled() && requestedMinutes > minutes + 0.5
                            ? ("Only " + juce::String (minutes, 1)
                                 + " minutes could be allocated.")
                            : juce::String(),
                          juce::dontSendNotification);
}

void SessionTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    {
        RowLayout r { bounds.removeFromTop (Metrics::buttonHeight) };

        enableToggle->setBounds (r.take (170));
        lengthLabel.setBounds (r.take (150));
        saveButton->setBounds (r.take (120));
        openFolderButton.setBounds (r.take (100));
    }

    bounds.removeFromTop (4);
    statusLabel.setBounds (bounds.removeFromTop (16));
    warningLabel.setBounds (bounds.removeFromTop (16));
}

//==============================================================================
PracticePanel::PracticePanel (LuthierAudioProcessor& p)
    : processor (p)
{
    toggleButton.setClickingTogglesState (true);
    toggleButton.setTooltip ("Open the practice tools: metronome, looper, backing "
                             "tracks, trainers, tab and the session recorder.");

    toggleButton.onClick = [this] { setOpen (toggleButton.getToggleState()); };
    addAndMakeVisible (toggleButton);

    for (auto* label : { &stripTempo, &stripLoop, &stripTrack })
    {
        styleReadout (*label);
        addAndMakeVisible (*label);
    }

    practiceLevel.setRange (-40.0, 6.0, 0.5);
    practiceLevel.setValue (0.0, juce::dontSendNotification);
    practiceLevel.setTooltip ("Master level for the practice tools. Does not affect the "
                              "plugin's own output.");
    addAndMakeVisible (practiceLevel);

    tapButton.setTooltip ("Tap tempo. The same global tap the header uses.");
    tapButton.onClick = [this] { processor.tapTempoNow(); };
    addAndMakeVisible (tapButton);

    panicButton.setTooltip ("Silence every practice tool at once. Does not touch the "
                            "plugin's own audio.");

    panicButton.onClick = [this]
    {
        processor.getMetronome().setEnabled (false);
        processor.getLooper().stop();
        processor.getBackingTrack().pause();
    };

    addAndMakeVisible (panicButton);

    // ---- a running routine (practice-tools 10) ------------------------------------------
    styleReadout (routineReadout);
    addChildComponent (routineReadout);

    routinePause.setTooltip ("Pause or resume the routine.");
    routinePause.onClick = [this]
    {
        auto& runner = processor.getPracticeRoutineRunner();

        if (runner.getPhase() == PracticeRoutineRunner::Phase::paused)
            runner.resume();
        else
            runner.pause();

        refreshRoutineStrip();
    };

    routineNext.setTooltip ("Skip to the routine's next entry.");
    routineNext.onClick = [this]
    {
        processor.getPracticeRoutineRunner().next();
        refreshRoutineStrip();
    };

    routineStop.setTooltip ("Stop the routine.");
    routineStop.onClick = [this]
    {
        processor.getPracticeRoutineRunner().stop();
        refreshRoutineStrip();
    };

    for (auto* button : { &routinePause, &routineNext, &routineStop })
        addChildComponent (*button);

    // ---- the tabs ---------------------------------------------------------------------
    struct TabSpec { const char* name; };

    static const TabSpec specs[] =
    {
        { "METRO" }, { "LOOP" }, { "TRACK" }, { "SCALE" },
        { "EAR" }, { "TAB" }, { "PROG" }, { "SESSION" }
    };

    tabs.add (new MetronomeTab (processor));
    tabs.add (new LooperTab (processor));
    tabs.add (new TrackTab (processor));
    tabs.add (new ScaleTab (processor));
    tabs.add (new EarTab (processor));
    tabs.add (new TabReaderTab (processor));
    tabs.add (new ProgressionTab (processor));
    tabs.add (new SessionTab (processor));

    for (int i = 0; i < (int) (sizeof (specs) / sizeof (specs[0])); ++i)
    {
        auto* button = tabButtons.add (new juce::TextButton (specs[i].name));

        button->setClickingTogglesState (true);
        button->setRadioGroupId (0x10);
        button->onClick = [this, i] { showTab (i); };

        addChildComponent (*button);
    }

    for (auto* tab : tabs)
        addChildComponent (*tab);

    showTab (0);
}

PracticePanel::~PracticePanel()
{
    stopTimer();
    saveStats();
}

void PracticePanel::saveStats()
{
    secondsSinceSave = 0.0;
    processor.savePracticeStats();
}

void PracticePanel::setOpen (bool shouldBeOpen)
{
    if (open == shouldBeOpen)
        return;

    open = shouldBeOpen;

    toggleButton.setToggleState (open, juce::dontSendNotification);

    for (auto* button : tabButtons)
        button->setVisible (open);

    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setVisible (open && i == currentTab);

    /*  practice-tools 0.1: a closed tool consumes no CPU.

        The timer stops, and the processor is told so that it stops rendering the
        metronome, the looper and the backing track altogether. Leaving them
        running behind a hidden panel is exactly what the rule forbids. */
    processor.setPracticePanelOpen (open);
    processor.getUiState().practiceDrawerOpen = open;   // onboarding 11: saved with the session

    // practice-tools 0.1: a closed drawer pauses the routine it was running,
    // and opening it again carries on - unless the player had paused it.
    auto& runner = processor.getPracticeRoutineRunner();

    if (open)
    {
        if (pausedByClosing && runner.getPhase() == PracticeRoutineRunner::Phase::paused)
            runner.resume();

        pausedByClosing = false;
        lastTickMs = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (20);
    }
    else
    {
        stopTimer();

        pausedByClosing = runner.getPhase() == PracticeRoutineRunner::Phase::countIn
                          || runner.getPhase() == PracticeRoutineRunner::Phase::running;

        if (pausedByClosing)
            runner.pause();

        saveStats();
    }

    refreshRoutineStrip();

    if (onHeightChanged != nullptr)
        onHeightChanged();

    resized();
    repaint();
}

void PracticePanel::showTab (int index)
{
    const int leaving = currentTab;
    index = juce::jlimit (0, tabs.size() - 1, index);

    // 12.1: a trainer's answers since it was opened count as one session.
    if (leaving != index)
    {
        auto& stats = processor.getPracticeStats();
        auto& tracker = processor.getPracticeActivityTracker();

        if (leaving == (int) PracticeTool::scaleTrainer)
            tracker.recordScaleTrainer (processor.getScaleTrainer(), stats, PracticeStats::today());
        else if (leaving == (int) PracticeTool::earTrainer)
            tracker.recordEarTrainer (processor.getEarTrainer(), stats, PracticeStats::today());
    }

    currentTab = index;

    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setVisible (open && i == currentTab);

    for (int i = 0; i < tabButtons.size(); ++i)
        tabButtons[i]->setToggleState (i == currentTab, juce::dontSendNotification);

    resized();
    repaint();
}

bool PracticePanel::openTabFile (const juce::File& file)
{
    return static_cast<TabReaderTab*> (tabs[(int) PracticeTool::tabReader])->openTabFile (file);
}

SessionTab& PracticePanel::getSessionTab() noexcept
{
    return *static_cast<SessionTab*> (tabs[(int) PracticeTool::sessionRecorder]);
}

void PracticePanel::setLibraryFile (const juce::File& file)
{
    static_cast<TabReaderTab*> (tabs[(int) PracticeTool::tabReader])->setLibraryFile (file);
}

void PracticePanel::setDefaultsFile (const juce::File& file)
{
    getSessionTab().setDefaultsFile (file);
}

void PracticePanel::tick (double seconds)
{
    auto& runner = processor.getPracticeRoutineRunner();

    // practice-tools 10 and 12: the routine's clock and the minutes practised
    // both run on the drawer's timer, which runs only while it is open (0.1).
    runner.advance (seconds);
    processor.getPracticeActivityTracker().update (processor.getPracticeTargets(), open, seconds,
                                                   processor.getPracticeStats(), PracticeStats::today());

    if (runner.isActive() && (int) runner.getActiveTool() != currentTab)
        showTab ((int) runner.getActiveTool());

    secondsSinceSave += seconds;

    if (secondsSinceSave >= 30.0)
        saveStats();

    refreshRoutineStrip();
}

void PracticePanel::refreshRoutineStrip()
{
    auto& runner = processor.getPracticeRoutineRunner();
    const bool showing = open && runner.isActive();
    bool changed = false;

    for (auto* c : { static_cast<juce::Component*> (&routineReadout), static_cast<juce::Component*> (&routinePause),
                     static_cast<juce::Component*> (&routineNext), static_cast<juce::Component*> (&routineStop) })
    {
        if (c->isVisible() != showing)
        {
            c->setVisible (showing);
            changed = true;
        }
    }

    if (changed)
        resized();

    if (! showing)
        return;

    const auto& routine = runner.getRoutine();
    juce::String text = routine.name + "  " + juce::String (runner.getEntryIndex() + 1)
                          + "/" + juce::String ((int) routine.entries.size());

    const double remaining = runner.getEntryRemainingSeconds();

    if (runner.getPhase() == PracticeRoutineRunner::Phase::countIn)
        text << "  count-in";
    else if (remaining > 0.0)
        text << "  " << (int) remaining / 60 << ":" << juce::String ((int) remaining % 60).paddedLeft ('0', 2);
    else
        text << "  x" << runner.getRepetitionsDone();

    routineReadout.setText (text, juce::dontSendNotification);
    routinePause.setButtonText (runner.getPhase() == PracticeRoutineRunner::Phase::paused ? "Resume" : "Pause");
}

void PracticePanel::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double elapsed = juce::jlimit (0.0, 1.0, (now - lastTickMs) * 0.001);
    lastTickMs = now;
    tick (elapsed);

    // ---- the strip's readouts ---------------------------------------------------------
    const auto& metronome = processor.getMetronome();
    const auto& looper = processor.getLooper();
    const auto& track = processor.getBackingTrack();

    stripTempo.setText (juce::String (metronome.getTempo(), 0) + " bpm", juce::dontSendNotification);

    switch (looper.getState())
    {
        case Looper::State::recordingFirst: stripLoop.setText ("LOOP rec", juce::dontSendNotification); break;
        case Looper::State::overdubbing:    stripLoop.setText ("LOOP dub", juce::dontSendNotification); break;
        case Looper::State::playing:        stripLoop.setText ("LOOP play", juce::dontSendNotification); break;

        case Looper::State::stopped:
        default:
            stripLoop.setText (looper.getLoopLengthSamples() > 0 ? "LOOP ready" : "LOOP -",
                               juce::dontSendNotification);
            break;
    }

    stripTrack.setText (track.isLoaded() ? track.getTitle() : juce::String ("no track"),
                        juce::dontSendNotification);

    if (juce::isPositiveAndBelow (currentTab, tabs.size()))
        tabs[currentTab]->refresh();
}

void PracticePanel::paint (juce::Graphics& g)
{
    g.setColour (Palette::panel);
    g.fillRect (getLocalBounds());

    g.setColour (Palette::edge);
    g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);

    if (! open)
        return;

    // A grip at the top edge, so the drawer looks draggable, because it is.
    g.setColour (Palette::edgeBright);

    const float centre = (float) getWidth() * 0.5f;

    for (int i = -2; i <= 2; ++i)
        g.fillRect (centre + (float) i * 6.0f - 1.5f, 3.0f, 3.0f, 2.0f);
}

void PracticePanel::mouseDown (const juce::MouseEvent& event)
{
    if (open && event.y < 8)
        dragStartHeight = openHeight;
}

void PracticePanel::mouseDrag (const juce::MouseEvent& event)
{
    if (! open || event.getMouseDownY() >= 8)
        return;

    // practice-tools 9: resizable between 32 and 360.
    openHeight = juce::jlimit (collapsedHeight + 40, maximumHeight,
                               dragStartHeight - event.getDistanceFromDragStartY());

    if (onHeightChanged != nullptr)
        onHeightChanged();
}

void PracticePanel::resized()
{
    auto bounds = getLocalBounds();

    // ---- the strip, which exists in both states ------------------------------------
    auto strip = bounds.removeFromTop (collapsedHeight).reduced (Metrics::gridHalf, 2);

    RowLayout r { strip };

    toggleButton.setBounds (r.take (96));
    stripTempo.setBounds (r.take (70));
    stripLoop.setBounds (r.take (86));
    stripTrack.setBounds (r.take (juce::jmax (80, r.bounds.getWidth() / 4)));

    panicButton.setBounds (r.bounds.removeFromRight (64).reduced (1, 2));
    r.bounds.removeFromRight (4);
    tapButton.setBounds (r.bounds.removeFromRight (56).reduced (1, 2));
    r.bounds.removeFromRight (4);
    practiceLevel.setBounds (r.bounds.removeFromRight (juce::jmin (140, r.bounds.getWidth())));

    if (! open)
        return;

    // ---- a running routine ------------------------------------------------------------
    if (routineReadout.isVisible())
    {
        auto routineRow = bounds.removeFromTop (22).reduced (Metrics::gridHalf, 0);
        routineStop.setBounds (routineRow.removeFromRight (56).reduced (1, 1));
        routineNext.setBounds (routineRow.removeFromRight (56).reduced (1, 1));
        routinePause.setBounds (routineRow.removeFromRight (64).reduced (1, 1));
        routineReadout.setBounds (routineRow);
    }

    // ---- the tab strip --------------------------------------------------------------
    auto tabRow = bounds.removeFromTop (22);

    const int width = juce::jmax (48, tabRow.getWidth() / juce::jmax (1, tabButtons.size()));

    for (auto* button : tabButtons)
        button->setBounds (tabRow.removeFromLeft (width).reduced (1, 0));

    // ---- the tab itself ---------------------------------------------------------------
    for (int i = 0; i < tabs.size(); ++i)
        if (i == currentTab)
            tabs[i]->setBounds (bounds);
}

} // namespace luthier
