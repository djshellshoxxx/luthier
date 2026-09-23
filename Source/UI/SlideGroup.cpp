#include "SlideGroup.h"
#include "../PluginProcessor.h"

namespace luthier
{

SlideGroup::SlideGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    heading.setText ("SLIDE", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    lowAction.setText (SlideEngine::kLowActionMessage, juce::dontSendNotification);
    lowAction.setFont (Fonts::ui (11.0f));
    lowAction.setColour (juce::Label::textColourId, Palette::warning);
    lowAction.setJustificationType (juce::Justification::topLeft);
    addChildComponent (lowAction);

    // The style is applied as a choice the user makes, never automatically.
    useSlideSetup.setTooltip ("Apply the Slide setup style: 2.8 / 3.2 mm action, 0.30 mm relief");
    useSlideSetup.onClick = [this]
    {
        const LuthierAudioProcessor::ScopedUndoAction undo (processor, "Setup style Slide setup");
        const auto& style = getSetupStyle (3);

        for (auto [id, plain] : { std::pair<const char*, double> { ParamIDs::setupStyle, 3.0 },
                                  { ParamIDs::setupActionTreble, style.actionTreble },
                                  { ParamIDs::setupActionBass, style.actionBass },
                                  { ParamIDs::setupRelief, style.relief } })
            if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) plain));
                parameter->endChangeGesture();
            }
    };
    addChildComponent (useSlideSetup);

    mode.attachTo (processor, ParamIDs::slideMode,
                   "Bottleneck and hybrid damp partly behind the bar; lap steel and dobro fully. "
                   "Hybrid puts one string under the bar and frets the rest.");
    addAndMakeVisible (mode);

    auto attach = [this] (LuthierSlider& slider, const char* id, const char* tip)
    {
        slider.attachTo (processor, id, tip);
        addAndMakeVisible (slider);
    };

    attach (pressure, ParamIDs::slidePressure,
            "How firmly the bar sits. Too light and the string rattles against it; "
            "too heavy and it chokes on the frets underneath.");
    attach (slant, ParamIDs::slideSlant, "Tilts the bar across the strings, so each string stops at a different place");
    attach (damping, ParamIDs::slideDampingBehind, "How much of the string behind the bar is muted");
    attach (assist, ParamIDs::slideIntonationAssist,
            "A playability aid: pulls the pitch toward the nearest note. 0 is exactly where the bar is.");
    attach (noise, ParamIDs::slideNoiseAmount, "Friction while the bar moves");
    attach (clank, ParamIDs::slideClankAmount, "The bar landing on the strings");

    for (auto* label : { &barMirror, &pressureState })
    {
        label->setFont (Fonts::ui (11.0f));
        label->setColour (juce::Label::textColourId, Palette::textMuted);
        addAndMakeVisible (*label);
    }

    shown = isSlideModeOn();
    setVisible (shown);
    startTimerHz (4);
    timerCallback();
}

SlideGroup::~SlideGroup()
{
    stopTimer();
}

bool SlideGroup::isSlideModeOn() const
{
    auto* p = processor.getState().getParameter (ParamIDs::slideGuitar);
    return p != nullptr && p->getValue() > 0.5f;
}

juce::String SlideGroup::describePressure (double value)
{
    if (value < 0.3)  return "Rattling: the bar is too light to hold the string";
    if (value > 0.85) return "Choking: pressed into the frets underneath";
    return "Seated";
}

void SlideGroup::timerCallback()
{
    const bool on = isSlideModeOn();

    if (on != shown)
    {
        shown = on;
        setVisible (on);

        if (onShownChanged)
            onShownChanged();
    }

    auto& state = processor.getState();

    auto plain = [&state] (const char* id)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
            return (double) parameter->convertFrom0to1 (parameter->getValue());

        return 0.0;
    };

    // slide-guitar.md 3: each mode has its own damping behind the bar. A mode
    // change by the user brings that with it; the control stays free after.
    const int modeNow = juce::roundToInt (plain (ParamIDs::slideMode));

    if (lastMode >= 0 && modeNow != lastMode)
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (ParamIDs::slideDampingBehind)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) Parameters::defaultDampingBehind (modeNow)));

    lastMode = modeNow;

    pressureState.setText (describePressure (plain (ParamIDs::slidePressure)), juce::dontSendNotification);

    const auto& bar = processor.getEngine().getSlideEngine().getBar();
    barMirror.setText ("Bar: " + juce::String (getSlideMaterial (bar.material).name) + ", "
                         + juce::String ((int) bar.massGrams) + " g, " + juce::String ((int) bar.lengthMm)
                         + " mm (Workshop part)",
                       juce::dontSendNotification);

    const bool low = on && plain (ParamIDs::setupActionBass) < SlideEngine::kMinimumBassActionMm;

    if (low != lowAction.isVisible())
    {
        lowAction.setVisible (low);
        useSlideSetup.setVisible (low);
        resized();
    }
}

int SlideGroup::preferredHeight() const
{
    if (! shown)
        return 0;

    return 20 + 36 + 6 * 24 + 18 + 18 + 44 + 26 + 8;
}

void SlideGroup::paint (juce::Graphics&) {}

void SlideGroup::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (2);
        return r;
    };

    heading.setBounds (take (20));
    mode.setBounds (take (36));
    pressure.setBounds (take (22));
    pressureState.setBounds (take (16));

    for (auto* s : { &slant, &damping, &assist, &noise, &clank })
        s->setBounds (take (22));

    barMirror.setBounds (take (16));
    lowAction.setBounds (take (42));
    useSlideSetup.setBounds (take (24).removeFromLeft (140));
}

} // namespace luthier
