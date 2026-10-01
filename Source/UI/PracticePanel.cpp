#include "PracticePanel.h"
#include "../PluginProcessor.h"
#include "MidiOutPanel.h"        // MODEL-GAPS: the drag-out take
#include "MidiExportDefaults.h"
#include "../DSP/Common/DspCommon.h"   // SPEC-SWEEP PT-39: hzToMidi
#include "../Riffs/Riff.h"             // FEAT2-TAB: play an imported tab
#include "../Riffs/RiffCompiler.h"
#include "../Riffs/RiffDestinations.h"
#include "../Notation/TabFingering.h"  // tab-import-export 9: the MIDI capture as tab
#include "../Export/MidiPerformance.h"

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

    // SPEC-SWEEP PT-6 (practice-tools 1): the click follows the host's tempo,
    // or a tapped one while the host is stopped; typing a tempo takes over.
    followToggle = std::make_unique<LuthierToggle> ("FOLLOW TEMPO");
    followToggle->getButton().setClickingTogglesState (true);
    followToggle->getButton().setTooltip ("Follow the host's tempo, or the tapped tempo while the host "
                                          "is stopped. Setting a tempo here turns this off.");
    followToggle->getButton().onClick = [this]
    {
        if (! updatingControls)
            metronome().setFollowsTempo (followToggle->getButton().getToggleState());
    };
    addAndMakeVisible (*followToggle);

    styleSlider (tempoSlider, 20.0, 300.0, 1.0, " bpm");
    tempoSlider.onValueChange = [this]
    {
        if (updatingControls)
            return;

        metronome().setFollowsTempo (false);   // SPEC-SWEEP PT-6: a typed tempo wins
        metronome().setTempo (tempoSlider.getValue());
        followToggle->getButton().setToggleState (false, juce::dontSendNotification);
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
        metronome().setFollowsTempo (false);   // SPEC-SWEEP PT-6: the ramp's end tempo stays
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
    followToggle->getButton().setToggleState (m.getFollowsTempo(), juce::dontSendNotification);
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
        followToggle->setBounds (r.take (150));
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

    statusLed.setTitle ("Looper status");   // SPEC-SWEEP GD-31
    addAndMakeVisible (statusLed);

    stopButton.onClick = [this] { looper().stop(); refresh(); };
    addAndMakeVisible (stopButton);

    clearButton.onClick = [this]
    {
        // action-and-undo.md 3.14: a cleared loop can be restored.
        auto* looperPtr = &looper();
        looper().clear();
        processor.pushUndoCallback ("Clear looper", "looper-clear", {},
                                    [looperPtr] { looperPtr->restoreCleared(); },
                                    [looperPtr] { looperPtr->clear(); });
        refresh();
    };
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
            editLayer (i, "mute", [this, i] { looper().getLayer (i).setMuted (layers[(size_t) i].mute->getToggleState()); });   // action-and-undo.md 3.14
        };
        addAndMakeVisible (*strip.mute);

        strip.reverse = std::make_unique<juce::TextButton> ("Rev");
        strip.reverse->setClickingTogglesState (true);
        strip.reverse->onClick = [this, i]
        {
            editLayer (i, "reverse", [this, i] { looper().getLayer (i).setReversed (layers[(size_t) i].reverse->getToggleState()); });   // action-and-undo.md 3.14
        };
        addAndMakeVisible (*strip.reverse);

        strip.halfSpeed = std::make_unique<juce::TextButton> ("1/2");
        strip.halfSpeed->setClickingTogglesState (true);
        strip.halfSpeed->onClick = [this, i]
        {
            editLayer (i, "halfSpeed", [this, i] { looper().getLayer (i).setHalfSpeed (layers[(size_t) i].halfSpeed->getToggleState()); });   // action-and-undo.md 3.14
        };
        addAndMakeVisible (*strip.halfSpeed);

        strip.mode = std::make_unique<juce::ComboBox>();

        for (int m = 0; m < (int) LayerMode::numModes; ++m)
            strip.mode->addItem (getLayerModeName ((LayerMode) m), m + 1);

        strip.mode->setSelectedId (1, juce::dontSendNotification);
        strip.mode->onChange = [this, i]
        {
            editLayer (i, "mode", [this, i] { looper().getLayer (i).setMode ((LayerMode) (layers[(size_t) i].mode->getSelectedId() - 1)); });   // action-and-undo.md 3.14
        };
        addAndMakeVisible (*strip.mode);

        strip.level = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal,
                                                      juce::Slider::NoTextBox);
        strip.level->setRange (-40.0, 6.0, 0.5);
        strip.level->setValue (0.0, juce::dontSendNotification);
        strip.level->onValueChange = [this, i]
        {
            editLayer (i, "level", [this, i] { looper().getLayer (i).setLevelDb (layers[(size_t) i].level->getValue()); });   // action-and-undo.md 3.14
        };
        addAndMakeVisible (*strip.level);

        strip.pan = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal,
                                                    juce::Slider::NoTextBox);
        strip.pan->setRange (-1.0, 1.0, 0.01);
        strip.pan->setValue (0.0, juce::dontSendNotification);
        strip.pan->onValueChange = [this, i]
        {
            editLayer (i, "pan", [this, i] { looper().getLayer (i).setPan (layers[(size_t) i].pan->getValue()); });   // action-and-undo.md 3.14
        };
        addAndMakeVisible (*strip.pan);

        // SPEC-SWEEP PT-20 (practice-tools 2): per-layer low-cut and high-cut.
        strip.lowCut = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal,
                                                       juce::Slider::NoTextBox);
        strip.lowCut->setRange (20.0, 2000.0, 1.0);
        strip.lowCut->setSkewFactorFromMidPoint (200.0);
        strip.lowCut->setValue (20.0, juce::dontSendNotification);
        strip.lowCut->setTextValueSuffix (" Hz");
        strip.lowCut->setTooltip ("Layer low-cut");
        strip.lowCut->setTitle ("Layer " + juce::String (i + 1) + " low-cut");
        strip.lowCut->onValueChange = [this, i]
        {
            looper().getLayer (i).setLowCutHz (layers[(size_t) i].lowCut->getValue());
        };
        addAndMakeVisible (*strip.lowCut);

        strip.highCut = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal,
                                                        juce::Slider::NoTextBox);
        strip.highCut->setRange (200.0, 20000.0, 1.0);
        strip.highCut->setSkewFactorFromMidPoint (2000.0);
        strip.highCut->setValue (20000.0, juce::dontSendNotification);
        strip.highCut->setTextValueSuffix (" Hz");
        strip.highCut->setTooltip ("Layer high-cut");
        strip.highCut->setTitle ("Layer " + juce::String (i + 1) + " high-cut");
        strip.highCut->onValueChange = [this, i]
        {
            looper().getLayer (i).setHighCutHz (layers[(size_t) i].highCut->getValue());
        };
        addAndMakeVisible (*strip.highCut);

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

//==============================================================================
// SPEC-SWEEP (GD-31)
juce::Colour LooperTab::ledColourFor (Looper::State state, double nowMs) noexcept
{
    switch (state)
    {
        case Looper::State::recordingFirst:
        case Looper::State::overdubbing:
        {
            const bool bright = ((juce::int64) (nowMs / 125.0) & 1) == 0;   // 4 Hz
            return juce::Colour (0xfff2544e).withAlpha (bright ? 1.0f : 0.35f);
        }

        case Looper::State::playing:   return juce::Colour (0xff4caf6a);
        case Looper::State::stopped:
        default:                       return juce::Colours::transparentBlack;
    }
}

void LooperTab::StatusLed::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    auto dot = r.withSizeKeepingCentre (d, d);

    g.setColour (Palette::edge);
    g.drawEllipse (dot, 1.0f);

    if (! colour.isTransparent())
    {
        g.setColour (colour);
        g.fillEllipse (dot.reduced (1.0f));
    }
}

void LooperTab::editLayer (int index, const char* what, const std::function<void()>& change)
{
    auto* layer = &looper().getLayer (index);
    const auto before = layer->settingsToVar();

    change();

    const auto after = layer->settingsToVar();
    processor.pushUndoCallback ("Change loop layer " + juce::String (index + 1) + " " + what, "looper-layer",
                                juce::String (index) + what,
                                // The layer is the processor's; the tab may be gone by then.
                                [layer, before] { layer->settingsFromVar (before); },
                                [layer, after] { layer->settingsFromVar (after); });
}

void LooperTab::refresh()
{
    auto& l = looper();

    // SPEC-SWEEP (GD-31): the LED follows the state on the panel's 20 Hz tick.
    {
        const auto c = ledColourFor (l.getState(), juce::Time::getMillisecondCounterHiRes());

        if (c != statusLed.colour)
        {
            statusLed.colour = c;
            statusLed.repaint();
        }
    }

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

        // SPEC-SWEEP PT-20: a loaded loop brings its filters with it.
        strip.lowCut->setValue (layer.getLowCutHz(), juce::dontSendNotification);
        strip.highCut->setValue (layer.getHighCutHz(), juce::dontSendNotification);

        for (auto* control : { (juce::Component*) strip.mute.get(),
                               (juce::Component*) strip.reverse.get(),
                               (juce::Component*) strip.halfSpeed.get(),
                               (juce::Component*) strip.mode.get(),
                               (juce::Component*) strip.level.get(),
                               (juce::Component*) strip.pan.get(),
                               (juce::Component*) strip.lowCut.get(),
                               (juce::Component*) strip.highCut.get() })
            control->setEnabled (hasContent);
    }
}

void LooperTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    {
        RowLayout r { bounds.removeFromTop (Metrics::buttonHeight) };

        statusLed.setBounds (r.take (18));   // SPEC-SWEEP GD-31
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

        const int quarter = juce::jmax (24, r.bounds.getWidth() / 4 - 2);

        strip.level->setBounds (r.take (quarter));
        strip.pan->setBounds (r.take (quarter));
        strip.lowCut->setBounds (r.take (quarter));    // SPEC-SWEEP PT-20
        strip.highCut->setBounds (r.rest());
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
            // PT-24: no MP3 reader is registered (registerBasicFormats() does not
            // include one), so offering *.mp3 here just invites a load that fails.
            "*.wav;*.aif;*.aiff;*.flac;*.ogg");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
            {
                // action-and-undo.md 3.14: practice-track-load.
                auto* player = &track();
                const auto before = player->isLoaded() ? player->getFile() : juce::File();
                const auto after = fc.getResult();

                if (track().load (after))
                    processor.pushUndoCallback ("Load backing track " + after.getFileName(), "practice-track-load", {},
                                                [player, before] { if (before.existsAsFile()) player->load (before); else player->unload(); },
                                                [player, after] { player->load (after); });
            }

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

    // SPEC-SWEEP PT-30 (practice-tools 3): a marker takes the typed name, or
    // a numbered one; the list shows the names.
    markerName.setTextToShowWhenEmpty ("Marker name", Palette::textMuted);
    markerName.setTitle ("Marker name");
    markerName.onReturnKey = [this] { addMarker.triggerClick(); };
    addAndMakeVisible (markerName);

    addMarker.onClick = [this]
    {
        auto name = markerName.getText().trim();

        if (name.isEmpty())
            name = "Marker " + juce::String (track().getNumMarkers() + 1);

        track().addMarker (name, track().getPositionSeconds());
        markerName.clear();
        refresh();
    };
    addAndMakeVisible (addMarker);

    // SPEC-SWEEP PT-26 (practice-tools 3): the track's own pan and filters.
    styleSlider (panSlider, -1.0, 1.0, 0.01, "");
    panSlider.setTitle ("Track pan");
    panSlider.setTooltip ("Track pan");
    panSlider.onValueChange = [this] { track().setPan (panSlider.getValue()); };
    addAndMakeVisible (panSlider);

    styleSlider (lowCutSlider, 20.0, 2000.0, 1.0, " Hz");
    lowCutSlider.setSkewFactorFromMidPoint (200.0);
    lowCutSlider.setTitle ("Track low-cut");
    lowCutSlider.setTooltip ("Track low-cut");
    lowCutSlider.onValueChange = [this] { track().setLowCutHz (lowCutSlider.getValue()); };
    addAndMakeVisible (lowCutSlider);

    styleSlider (highCutSlider, 200.0, 20000.0, 1.0, " Hz");
    highCutSlider.setSkewFactorFromMidPoint (2000.0);
    highCutSlider.setTitle ("Track high-cut");
    highCutSlider.setTooltip ("Track high-cut");
    highCutSlider.onValueChange = [this] { track().setHighCutHz (highCutSlider.getValue()); };
    addAndMakeVisible (highCutSlider);

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

    // SPEC-SWEEP PT-26.
    panSlider.setValue (t.getPan(), juce::dontSendNotification);
    lowCutSlider.setValue (t.getLowCutHz(), juce::dontSendNotification);
    highCutSlider.setValue (t.getHighCutHz(), juce::dontSendNotification);

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
        // SPEC-SWEEP PT-26.
        RowLayout r { row (22) };
        const int third = r.bounds.getWidth() / 3 - 4;

        panSlider.setBounds (r.take (third));
        lowCutSlider.setBounds (r.take (third));
        highCutSlider.setBounds (r.rest());
    }

    {
        RowLayout r { row (Metrics::buttonHeight) };

        loopButton.setBounds (r.take (56));
        setLoopStart.setBounds (r.take (70));
        setLoopEnd.setBounds (r.take (70));
        monoButton.setBounds (r.take (56));
        markerName.setBounds (r.take (90));   // SPEC-SWEEP PT-30
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
    keyBox.onChange = [this]
    {
        // action-and-undo.md 3.14: practice-scale, grouped.
        auto* t = &trainer();
        const int before = t->getKey(), after = keyBox.getSelectedId() - 1;
        processor.pushUndoCallback ("Change practice key", "practice-scale", "key",
                                    [t, before] { t->setKey (before); }, [t, after] { t->setKey (after); });
        trainer().setKey (after);
        repaint();
    };
    addAndMakeVisible (keyBox);

    for (int i = 0; i < (int) ScaleType::custom; ++i)
        scaleBox.addItem (getScaleTypeName ((ScaleType) i), i + 1);

    // SPEC-SWEEP PT-37 (practice-tools 4): a custom scale is its step list.
    scaleBox.addItem ("Custom...", (int) ScaleType::custom + 1);

    scaleBox.setSelectedId (1, juce::dontSendNotification);
    scaleBox.onChange = [this]
    {
        // action-and-undo.md 3.14: practice-scale, grouped.
        auto* t = &trainer();
        const auto before = t->getScale();
        const auto after = (ScaleType) (scaleBox.getSelectedId() - 1);
        processor.pushUndoCallback ("Change practice scale", "practice-scale", "scale",
                                    [t, before] { t->setScale (before); }, [t, after] { t->setScale (after); });

        // SPEC-SWEEP PT-37: a custom scale is its step list.
        customSteps.setVisible (after == ScaleType::custom);

        if (after == ScaleType::custom)
            setCustomSteps (customSteps.getText());
        else
            trainer().setScale (after);

        resized();
        repaint();
    };
    addAndMakeVisible (scaleBox);

    customSteps.setText ("2 2 1 2 2 2 1", juce::dontSendNotification);
    customSteps.setTitle ("Custom scale steps");
    customSteps.setTooltip ("The scale's steps in semitones, e.g. 2 1 2 2 1 2 2 for natural minor.");
    customSteps.onTextChange = [this] { setCustomSteps (customSteps.getText()); repaint(); };
    addChildComponent (customSteps);

    // SPEC-SWEEP PT-33: note names, intervals from the root or scale degrees.
    overlayBox.addItem ("Notes", 1);
    overlayBox.addItem ("Intervals", 2);
    overlayBox.addItem ("Degrees", 3);
    overlayBox.setSelectedId (1, juce::dontSendNotification);
    overlayBox.setTitle ("Fretboard labels");
    overlayBox.onChange = [this] { repaint(); };
    addAndMakeVisible (overlayBox);

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

    nextButton.onClick = [this] { ask(); };
    addAndMakeVisible (nextButton);

    // SPEC-SWEEP PT-35: the interval trainer's answers.
    static const char* const intervalNames[12] =
        { "P1", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7" };

    for (int i = 0; i < 12; ++i)
    {
        auto* button = intervalButtons.add (new juce::TextButton (intervalNames[i]));
        button->onClick = [this, i] { chooseInterval (i); };
        addChildComponent (*button);
    }

    addAndMakeVisible (scaleView);

    styleReadout (feedbackLabel);
    addAndMakeVisible (feedbackLabel);

    refresh();
}

juce::String ScaleTab::labelFor (int pitchClass, Overlay overlay)
{
    auto& t = trainer();
    pitchClass = ((pitchClass % 12) + 12) % 12;

    if (! t.containsPitchClass (pitchClass))
        return {};

    static const char* const noteNames[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* const intervalNames[12] =
        { "1", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7" };

    switch (overlay)
    {
        case Overlay::intervals: return intervalNames[(pitchClass - t.getKey() + 12) % 12];
        case Overlay::degrees:   return juce::String (t.getDegreeOf (pitchClass));
        case Overlay::notes:
        case Overlay::numOverlays:
        default:                 return noteNames[pitchClass];
    }
}

bool ScaleTab::setCustomSteps (const juce::String& text)
{
    const auto tokens = juce::StringArray::fromTokens (text, " ,-", "");
    int offsets[ScaleTrainer::kMaxIntervals] {};
    int count = 0, total = 0;

    for (const auto& token : tokens)
    {
        if (token.trim().isEmpty())
            continue;

        const int step = token.getIntValue();

        if (step < 1 || step > 11 || count >= ScaleTrainer::kMaxIntervals)
            return false;

        offsets[count++] = total;
        total += step;
    }

    // The steps come back to the octave, or stop short of it - and then the
    // last step lands on a note of the scale too.
    if (count == 0 || total > 12)
        return false;

    if (total < 12)
    {
        if (count >= ScaleTrainer::kMaxIntervals)
            return false;

        offsets[count++] = total;
    }

    trainer().setCustomIntervals (offsets, count);
    return true;
}

void ScaleTab::paint (juce::Graphics& g)
{
    // SPEC-SWEEP PT-33 (practice-tools 4, "explore"): the scale on the neck.
    const auto area = scaleView.getBounds().toFloat().reduced (4.0f);

    if (area.getWidth() < 40.0f || area.getHeight() < 30.0f)
        return;

    const auto& tuning = processor.getEngine().getTuningEngine();
    const int numStrings = juce::jlimit (1, 12, processor.getEngine().getNumStrings());
    constexpr int frets = 15;

    const float fretWidth = area.getWidth() / (float) (frets + 1);
    const float stringGap = area.getHeight() / (float) numStrings;
    const auto overlay = (Overlay) juce::jlimit (0, 2, overlayBox.getSelectedId() - 1);

    g.setColour (Palette::edge);

    for (int f = 1; f <= frets + 1; ++f)
    {
        const float x = area.getX() + fretWidth * (float) f;
        g.drawLine (x, area.getY(), x, area.getBottom(), f == 1 ? 3.0f : 1.0f);
    }

    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    for (int s = 0; s < numStrings; ++s)
    {
        const float y = area.getY() + stringGap * ((float) s + 0.5f);

        g.setColour (Palette::edgeBright);
        g.drawLine (area.getX(), y, area.getRight(), y, 1.0f);

        const int open = (int) std::round (hzToMidi (tuning.computeFrequency (s, 0.0), tuning.getConcertA()));

        for (int f = 0; f <= frets; ++f)
        {
            const int pitchClass = (open + f) % 12;
            const auto label = labelFor (pitchClass, overlay);

            if (label.isEmpty())
                continue;

            const float x = area.getX() + fretWidth * ((float) f + 0.5f);
            const float r = juce::jmin (fretWidth, stringGap) * 0.42f;
            const bool root = pitchClass == trainer().getKey();

            g.setColour (root ? Palette::accent : Palette::secondary);
            g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);

            g.setColour (Palette::backgroundDeep);
            g.drawText (label, juce::Rectangle<float> (x - r, y - r, 2.0f * r, 2.0f * r),
                        juce::Justification::centred, false);
        }
    }
}

void ScaleTab::ask()
{
    questionLabel.setText (trainer().nextQuestion (random), juce::dontSendNotification);
    feedbackLabel.setText ({}, juce::dontSendNotification);
    askedAtMs = juce::Time::getMillisecondCounterHiRes();
    playQuestion();
    refresh();
}

void ScaleTab::playQuestion()
{
    // SPEC-SWEEP PT-35: the interval and chord-tone questions are heard.
    int notes[8] {};
    const int count = trainer().getQuestionNotes (notes, 8);

    if (count == 0)
        return;

    int strings[8] {}, frets[8] {};
    EarTab::placeOnStrings (processor.getEngine().getTuningEngine(), processor.getEngine().getNumStrings(),
                            notes, count, strings, frets);

    for (int i = 0; i < count; ++i)
        if (strings[i] >= 0)
            processor.triggerPreviewNote (strings[i], (double) frets[i], 0.7);
}

void ScaleTab::chooseInterval (int semitones)
{
    auto& t = trainer();

    if (t.getMode() != ScaleTrainer::Mode::intervalTrainer || ! t.isQuestionOpen())
        return;

    static const char* const intervalNames[12] =
        { "P1", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7" };

    const int wanted = t.getIntervalSemitones();

    feedbackLabel.setText (t.answerInterval (semitones) ? juce::String ("Right")
                                                        : juce::String ("No - ") + intervalNames[wanted],
                           juce::dontSendNotification);
    refresh();
}

void ScaleTab::notePlayed (int midiNote)
{
    // SPEC-SWEEP PT-34 (practice-tools 4): the quiz is answered on the guitar.
    auto& t = trainer();

    if (t.getMode() == ScaleTrainer::Mode::explore || t.getExpectedPitchClass() < 0)
        return;

    static const char* const names[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const int expected = t.getExpectedPitchClass();

    // SPEC-SWEEP PT-35: the interval trainer is answered with its buttons, and
    // the chord-tone trainer wants both tones, in time.
    if (t.getMode() == ScaleTrainer::Mode::intervalTrainer)
        return;

    if (t.getMode() == ScaleTrainer::Mode::chordToneTrainer)
    {
        const double seconds = (juce::Time::getMillisecondCounterHiRes() - askedAtMs) * 0.001;

        if (t.answer (midiNote, seconds))
            feedbackLabel.setText ("Right - both chord tones", juce::dontSendNotification);
        else if (! t.isQuestionOpen())
            feedbackLabel.setText ("Too late - ask again", juce::dontSendNotification);
        else
            feedbackLabel.setText ("Keep going", juce::dontSendNotification);

        refresh();
        return;
    }

    if (t.answer (midiNote))
    {
        feedbackLabel.setText (juce::String ("Right - ") + names[expected], juce::dontSendNotification);
        questionLabel.setText (t.nextQuestion (random), juce::dontSendNotification);
    }
    else
    {
        feedbackLabel.setText (juce::String ("That was ") + names[((midiNote % 12) + 12) % 12]
                                 + " - try again", juce::dontSendNotification);
    }

    refresh();
}

ScaleTrainer& ScaleTab::trainer()
{
    return processor.getScaleTrainer();
}

void ScaleTab::refresh()
{
    auto& t = trainer();

    scoreLabel.setText (t.getAsked() > 0
                          ? (juce::String (t.getScore()) + " of " + juce::String (t.getAsked())
                               + "   " + juce::String (t.getSuccessRate() * 100.0, 0) + "%")
                          : juce::String(),
                        juce::dontSendNotification);

    nextButton.setEnabled (t.getMode() != ScaleTrainer::Mode::explore);

    // SPEC-SWEEP PT-35: interval answers only for the interval trainer; the
    // chord-tone question runs out.
    const bool intervals = t.getMode() == ScaleTrainer::Mode::intervalTrainer;

    for (auto* button : intervalButtons)
        if (button->isVisible() != intervals)
            button->setVisible (intervals), resized();

    if (t.getMode() == ScaleTrainer::Mode::chordToneTrainer && t.isQuestionOpen()
          && (juce::Time::getMillisecondCounterHiRes() - askedAtMs) * 0.001 > t.getTimeLimitSeconds())
    {
        t.answer (-1, t.getTimeLimitSeconds() + 1.0);   // missed
        feedbackLabel.setText ("Time's up - ask again", juce::dontSendNotification);
    }

    repaint();
}

void ScaleTab::applyCustomIntervals()
{
    juce::StringArray tokens;
    tokens.addTokens (customIntervalsEditor.getText(), " ,", "");
    tokens.removeEmptyStrings();

    std::array<int, ScaleTrainer::kMaxIntervals> intervals {};
    int count = 0;

    for (const auto& token : tokens)
    {
        if (count >= ScaleTrainer::kMaxIntervals)
            break;

        intervals[(size_t) count++] = token.getIntValue();
    }

    trainer().setCustomIntervals (intervals.data(), count);
}

void ScaleTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    {
        RowLayout r { bounds.removeFromTop (Metrics::buttonHeight) };

        keyBox.setBounds (r.take (64));
        scaleBox.setBounds (r.take (150));

        if (customSteps.isVisible())
            customSteps.setBounds (r.take (110));   // SPEC-SWEEP PT-37

        modeBox.setBounds (r.take (150));
        overlayBox.setBounds (r.take (96));         // SPEC-SWEEP PT-33
        nextButton.setBounds (r.take (64));
        scoreLabel.setBounds (r.rest());
    }

    bounds.removeFromTop (3);

    // SPEC-SWEEP PT-35.
    if (! intervalButtons.isEmpty() && intervalButtons[0]->isVisible())
    {
        auto line = bounds.removeFromTop (22);
        const int width = juce::jmax (24, line.getWidth() / 12);

        for (auto* button : intervalButtons)
            button->setBounds (line.removeFromLeft (width).reduced (1, 0));

        bounds.removeFromTop (3);
    }

    {
        auto line = bounds.removeFromTop (20);
        feedbackLabel.setBounds (line.removeFromRight (juce::jmin (220, line.getWidth() / 2)));
        questionLabel.setBounds (line);
    }

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

    nextButton.onClick = [this]
    {
        numNotes = trainer().nextQuestion (random, notes, offsets,
                                           EarTrainer::kMaxNotesInQuestion);
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
    /*  The preview path takes a string and a fret; the trainer works in
        pitches. SPEC-SWEEP PT-39: each note goes where it sounds - it was
        string i at fret (note mod 24), which played the wrong pitch for
        almost every question. */
    int strings[EarTrainer::kMaxNotesInQuestion] {};
    int frets[EarTrainer::kMaxNotesInQuestion] {};

    const int placed = placeOnStrings (processor.getEngine().getTuningEngine(),
                                       processor.getEngine().getNumStrings(),
                                       notes, numNotes, strings, frets);

    for (int i = 0; i < placed; ++i)
    {
        juce::ignoreUnused (offsets[i]);

        if (strings[i] >= 0)
            processor.triggerPreviewNote (strings[i], (double) frets[i], 0.75);
    }
}

int EarTab::placeOnStrings (const TuningEngine& tuning, int numStrings, const int* midiNotes, int count,
                            int* stringsOut, int* fretsOut)
{
    numStrings = juce::jlimit (1, 12, numStrings);
    std::array<bool, 12> used {};

    for (int i = 0; i < count; ++i)
    {
        int best = -1, bestFret = 0;

        // The lowest free position that sounds it; out of reach, an octave
        // nearer the neck is still the same answer.
        for (int pass = 0; pass < 3 && best < 0; ++pass)
        {
            const int wanted = midiNotes[i] + (pass == 1 ? -12 : (pass == 2 ? 12 : 0));

            for (int s = 0; s < numStrings; ++s)
            {
                if (used[(size_t) s])
                    continue;

                const int open = (int) std::round (hzToMidi (tuning.computeFrequency (s, 0.0),
                                                             tuning.getConcertA()));
                const int fret = wanted - open;

                if (fret >= 0 && fret <= 15 && (best < 0 || fret < bestFret))
                {
                    best = s;
                    bestFret = fret;
                }
            }
        }

        if (best >= 0)
            used[(size_t) best] = true;

        stringsOut[i] = best;
        fretsOut[i] = bestFret;
    }

    return count;
}

void EarTab::refresh()
{
    auto& t = trainer();

    questionLabel.setText (t.getQuestionText(), juce::dontSendNotification);

    scoreLabel.setText (juce::String (t.getCorrect()) + " of " + juce::String (t.getAsked())
                          + "    level " + juce::String (t.getDifficulty() + 1)
                          + "    streak " + juce::String (t.getStreak()),
                        juce::dontSendNotification);

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
void TabReaderTab::showStatus (const juce::String& text, bool warning)
{
    statusLabel.setText (text, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, warning ? Palette::warning : Palette::textMuted);
    statusLabel.setTooltip (text);
}

bool TabReaderTab::openTab (const juce::File& file, const juce::File& libraryFile)
{
    const bool read = importer.read (file, score);

    if (read)
    {
        // practice-tools 11.2: the PRACTICE tab lists recent tab files.
        PracticeLibrary library;
        library.load (libraryFile);
        library.noteTabOpened (file);

        juce::String error;
        library.save (libraryFile, error);

        scoreTitle = file.getFileNameWithoutExtension();

        /*  tab-import-export 7: a page that was only partly readable is still
            opened, and the status says what was skipped ("Loaded 3 bars, 12
            notes (6 strings); 2 lines skipped") in the warning colour, so the
            player knows the tab is not all there. The details are the tooltip. */
        const auto& d = importer.getLastDiagnostics();
        auto text = (d.notes > 0 ? d.summary() : juce::String (score.getTotalNoteCount()) + " notes.")
                      + " - " + file.getFileName();

        if (! d.warnings.isEmpty())
            text += "\n" + d.warnings.joinIntoString ("\n");

        showStatus (text, d.isPartial());
    }
    else
    {
        showStatus (importer.getLastError(), true);
    }

    refresh();
    return read;
}

bool TabReaderTab::openLivePerformance()
{
    // tab-import-export 9: the session take is already a score (notation-export 6).
    auto& take = processor.getPerformanceCapture();
    processor.drainPerformanceCapture();

    PerformanceScore live;
    juce::String source;

    if (! take.getNotes().empty())
    {
        take.toScore (live);
        source = "live take";
    }
    else
    {
        // No engine take (capture off, or MIDI went straight out): the MIDI
        // capture holds the input as played, pitches only, so it is fingered.
        const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
        const auto performance = MidiPerformance::fromCapture (processor.getMidiCapture(), rate,
                                                               processor.getHostTempo());
        performance.toScore (live);

        if (live.getTotalNoteCount() > 0 && ! TabFingering::isPlausible (live))
            TabFingering::assign (live);

        source = "MIDI capture (fingering guessed)";
    }

    if (live.getTotalNoteCount() == 0)
    {
        showStatus ("Nothing has been played yet. Play something, then press Live.", true);
        return false;
    }

    live.getMeta().title = "Live performance";
    openScore (live, "Live performance");
    showStatus (juce::String (live.getTotalNoteCount()) + " notes from the " + source
                  + " - Export writes it as tab, MIDI, MusicXML or Guitar Pro.", false);
    return true;
}

void TabReaderTab::openScore (const PerformanceScore& newScore, const juce::String& title)
{
    // riff-library 6.4: a riff opened with Learn It.
    score = newScore;
    scoreTitle = title;
    statusLabel.setText (title + " - " + juce::String (score.getTotalNoteCount()) + " notes",
                         juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    refresh();
}

//==============================================================================
void TabReaderTab::togglePlay()
{
    auto& player = processor.getEngine().getRiffPlayer();

    if (player.isPlaying() || player.isWaiting())
    {
        player.stop();
        playButton.setButtonText ("Play");
        return;
    }

    if (score.getTotalNoteCount() == 0)
    {
        statusLabel.setText ("Open a tab first.", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, Palette::warning);
        return;
    }

    // FEAT2-TAB: the parsed score becomes a riff, compiled against the loaded
    // instrument and handed to the audition player - the same path riffs use.
    const auto riff = Riff::fromScore (score);
    const auto guitar = RiffDestinations::guitarSummary (processor);

    player.setCompiled (RiffCompiler::compile (riff, RiffPlaySettings{}, guitar), true);
    player.setClockMode (RiffPlayer::ClockMode::own);
    player.setAbsoluteBpm (score.getMeta().tempoBpm);
    player.setLooping (true);
    player.play();

    playButton.setButtonText ("Stop");
    statusLabel.setText ("Playing " + scoreTitle, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, Palette::textMuted);
}

//==============================================================================
TabReaderTab::TabReaderTab (LuthierAudioProcessor& p)
    : PracticeTab (p)
{
    openButton.onClick = [this]
    {
        // tab-import-export 8: MIDI files open as tab too.
        chooser = std::make_unique<juce::FileChooser> (
            "Open tablature or MIDI",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.txt;*.tab;*.musicxml;*.xml;*.mid;*.midi");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();

            if (file != juce::File())
                openTab (file);
        });
    };

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

            if (score.getTotalNoteCount() == 0)
            {
                showStatus ("Nothing to export: open a tab or press Live first.", true);
                return;
            }

            const bool ok = exporter.write (score, format, fc.getResult(), options);
            showStatus (ok ? ("Exported " + juce::String (getNotationFormatName (format)) + " to "
                                + fc.getResult().getFileName())
                           : exporter.getLastError(),
                        ! ok);
        });
    };

    playButton.setTooltip ("Play the imported tab through the engine.");
    playButton.onClick = [this] { togglePlay(); };

    openButton.setTooltip ("Open ASCII tab, MusicXML or a MIDI file (fingered as tab).");
    exportButton.setTooltip ("Write the shown score in the chosen format: ASCII tab, MIDI, MusicXML or Guitar Pro.");
    liveButton.setTooltip ("Show what you just played as tab, ready to play back or export.");
    liveButton.onClick = [this] { openLivePerformance(); };

    addAndMakeVisible (openButton);
    addAndMakeVisible (exportButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (liveButton);

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
    barsSlider.setTooltip ("How many bars the view shows at once.");
    addAndMakeVisible (barsSlider);

    // A tab longer than the window is scrolled, not cut off at its first bars.
    styleSlider (fromBarSlider, 1.0, 1.0, 1.0, "");
    fromBarSlider.setTooltip ("The first bar shown. Scroll through a long tab.");
    fromBarSlider.onValueChange = [this] { refresh(); };
    addAndMakeVisible (fromBarSlider);

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

void TabReaderTab::refresh()
{
    // notation-export 3: the live view shows a window of bars.
    NotationExportOptions options;
    options.lineWidth = 200;

    // The scroller spans the score's bars; a new score pulls it back into range.
    const int numMeasures = score.getNumTracks() > 0 ? (int) score.getTrack (0).measures.size() : 0;
    fromBarSlider.setRange (1.0, (double) juce::jmax (1, numMeasures), 1.0);
    fromBarSlider.setEnabled (numMeasures > 1);

    const auto text = exporter.renderAsciiTabWindow (score, (int) fromBarSlider.getValue() - 1,
                                                     (int) barsSlider.getValue(), options);

    // The Play button follows the player, which a riff audition or Stop elsewhere can change.
    auto& player = processor.getEngine().getRiffPlayer();
    playButton.setButtonText (player.isPlaying() || player.isWaiting() ? "Stop" : "Play");

    if (text != tabView.getText())
        tabView.setText (text, false);
}

void TabReaderTab::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    {
        RowLayout r { bounds.removeFromTop (Metrics::buttonHeight) };

        openButton.setBounds (r.take (70));
        liveButton.setBounds (r.take (46));
        playButton.setBounds (r.take (54));
        formatBox.setBounds (r.take (112));
        exportButton.setBounds (r.take (74));
        barsSlider.setBounds (r.take (116));
        fromBarSlider.setBounds (r.take (116));
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
SessionTab::SessionTab (LuthierAudioProcessor& p)
    : PracticeTab (p)
{
    enableToggle = std::make_unique<LuthierToggle> ("SESSION RECORDER");
    enableToggle->getButton().setClickingTogglesState (true);

    enableToggle->getButton().onClick = [this]
    {
        auto& recorder = processor.getSessionRecorder();
        const bool on = enableToggle->getButton().getToggleState();

        // The ring is sized from the PRACTICE tab's setup as it goes on, never
        // while it records.
        if (on)
        {
            const auto setup = storedSetup();
            requestedMinutes = storedMinutes = setup.ringMinutes;
            setup.applyTo (recorder, processor.getSampleRate());
            recorder.setEnabled (true);
        }
        else if (recorder.stop (SessionRecorder::getSessionDirectory()))
        {
            // practice-tools 11.2's auto-save (MODEL-GAPS): stopping kept the take.
            statusLabel.setText ("Saved to your Sessions folder.", juce::dontSendNotification);
        }

        refresh();
    };

    enableToggle->setTooltip ("Continuously record everything the plugin plays into a "
                              "ring buffer, so you can keep a take after playing it.");

    addAndMakeVisible (*enableToggle);

    styleReadout (lengthLabel);
    lengthLabel.setTooltip ("How much the recorder keeps. Set it on the PRACTICE tab.");
    addAndMakeVisible (lengthLabel);

    saveButton.setTooltip ("Freeze what is in the buffer to a WAV and a MIDI file.");
    saveButton.setTooltip ("Freeze what is in the buffer to a WAV and a MIDI file. "
                           "Drag this button to drop the take into your DAW.");
    saveButton.onClick = [this] { saveTake(); };

    openFolderButton.onClick = [this]
    {
        const auto folder = SessionRecorder::getSessionDirectory();
        folder.createDirectory();
        folder.revealToUser();
    };

    addAndMakeVisible (saveButton);
    addAndMakeVisible (openFolderButton);

    styleReadout (statusLabel);
    addAndMakeVisible (statusLabel);

    styleReadout (warningLabel, Palette::warning);
    addAndMakeVisible (warningLabel);

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

bool SessionTab::saveTake()
{
    // SPEC-SWEEP MX-25: the MIDI in the user's default export profile.
    const auto midiOptions = MidiExportDefaults::load();
    const bool saved = processor.getSessionRecorder().saveLastTake (SessionRecorder::getSessionDirectory(),
                                                                    0.0, &midiOptions);

    statusLabel.setText (saved ? "Saved to your Sessions folder."
                               : "There is nothing recorded to save.",
                         juce::dontSendNotification);
    return saved;
}

juce::StringArray SessionTab::SaveButton::filesToDrag (bool forceGeneric)
{
    auto& recorder = tab.processor.getSessionRecorder();
    auto files = recorder.getLastSavedFiles();

    if (files.isEmpty() && tab.saveTake())
        files = recorder.getLastSavedFiles();

    juce::StringArray paths;
    bool haveTakeMidi = false;

    // midi-export 4.2: a valid MIDI file in the Luthier profile (Generic with
    // Alt), of the take's span of what the engine played.
    const auto performance = MidiTakeExport::capturedPerformance (tab.processor);
    const double seconds = (double) recorder.getRecordedSamples() / juce::jmax (1.0, tab.processor.getSampleRate());

    if (performance.getLengthInSamples() > 0)
    {
        auto defaults = MidiExportDefaults::load();
        defaults.range = seconds > 0.0 ? performance.getLastSecondsRange (seconds) : juce::Range<juce::int64>();

        const auto midi = MidiProfiles::writeDragOutFile (performance, forceGeneric, defaults);

        if (midi.existsAsFile())
        {
            paths.add (midi.getFullPathName());
            haveTakeMidi = true;
        }
    }

    for (const auto& f : files)
        if (f.existsAsFile())
        {
            if (f.hasFileExtension ("mid"))
            {
                if (! haveTakeMidi)
                    paths.insert (0, f.getFullPathName());   // nothing captured: the raw take, first
            }
            else
            {
                paths.add (f.getFullPathName());
            }
        }

    return paths;
}

void SessionTab::SaveButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragged || e.getDistanceFromDragStart() < 6)
        return;

    dragged = true;
    const auto paths = filesToDrag (e.mods.isAltDown());

    if (! paths.isEmpty())
        juce::DragAndDropContainer::performExternalDragDropOfFiles (paths, false, this);
}

void SessionTab::SaveButton::mouseUp (const juce::MouseEvent& e)
{
    // A drag is not also a click.
    if (dragged)
    {
        setState (juce::Button::buttonNormal);
        return;
    }

    juce::TextButton::mouseUp (e);
}

SessionRecorderSetup SessionTab::storedSetup()
{
    PracticeDefaults defaults;
    juce::String error;

    if (! PracticeDefaults::load (PracticeDefaults::getDefaultsFile(), defaults, error))
        return {};

    return SessionRecorderSetup::fromVar (defaults.extra["session_recorder"]);
}

void SessionTab::paint (juce::Graphics& g)
{
    // SPEC-SWEEP (GD-33): recorded / capacity.
    if (fillBarBounds.isEmpty())
        return;

    g.setColour (Palette::panelSunken);
    g.fillRect (fillBarBounds);
    g.setColour (Palette::accent);
    g.fillRect (fillBarBounds.withWidth (juce::roundToInt ((float) fillBarBounds.getWidth() * fillFraction)));
}

void SessionTab::refresh()
{
    auto& recorder = processor.getSessionRecorder();

    enableToggle->getButton().setToggleState (recorder.isEnabled(), juce::dontSendNotification);

    const double minutes = recorder.getCapacityMinutes();
    const double recorded = (double) recorder.getRecordedSamples()
                              / juce::jmax (1.0, processor.getSampleRate()) / 60.0;

    statusLabel.setText (juce::String (recorded, 1) + " of " + juce::String (minutes, 1)
                           + " minutes held",
                         juce::dontSendNotification);

    // SPEC-SWEEP (GD-33): the fill bar under it.
    {
        const auto fraction = (float) juce::jlimit (0.0, 1.0, minutes > 0.0 ? recorded / minutes : 0.0);

        if (std::abs (fraction - fillFraction) > 1.0e-4f)
        {
            fillFraction = fraction;
            repaint (fillBarBounds);
        }
    }

    // practice-tools 8 sizes the default at 1.4 GB, which this machine will not
    // allocate. The capacity is reported rather than the request, and the
    // difference is stated rather than hidden.
    lengthLabel.setText ("Keeps " + juce::String (storedMinutes, 0) + " min",
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
        lengthLabel.setBounds (r.take (110));
        saveButton.setBounds (r.take (120));
        openFolderButton.setBounds (r.take (100));
    }

    bounds.removeFromTop (4);
    statusLabel.setBounds (bounds.removeFromTop (16));
    fillBarBounds = bounds.removeFromTop (4).reduced (0, 1);   // SPEC-SWEEP GD-33
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
    motion.stopTimer();
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

    // practice-tools 0.1: a closed drawer pauses the routine it was running,
    // and opening it again carries on - unless the player had paused it.
    auto& runner = processor.getPracticeRoutineRunner();

    if (open)
    {
        if (pausedByClosing && runner.getPhase() == PracticeRoutineRunner::Phase::paused)
            runner.resume();

        pausedByClosing = false;
        lastTickMs = juce::Time::getMillisecondCounterHiRes();
        motion.startTimerHz (*this, 20);
    }
    else
    {
        motion.stopTimer();

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

    // SPEC-SWEEP PT-34: the notes played since the last tick go to the tab on
    // show (the feed fills only while the drawer is open).
    for (int note = 0; processor.getPracticeNoteFeed().pop (note);)
        if (juce::isPositiveAndBelow (currentTab, tabs.size()))
            tabs[currentTab]->notePlayed (note);

    if (juce::isPositiveAndBelow (currentTab, tabs.size()))
        tabs[currentTab]->refresh();
}

void PracticePanel::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

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
