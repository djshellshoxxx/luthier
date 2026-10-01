#include "StrumGroup.h"
#include "../PluginProcessor.h"

namespace luthier
{

//==============================================================================
StrumGroup::StrumGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    heading.setText ("STRUM", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    sourceLabel.setFont (Fonts::ui (10.0f));
    sourceLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible (sourceLabel);

    // ambiguity-resolutions 6: a kit's default sits above the knob. Clearing it
    // is the one-click way to make the knob the source again.
    followKnobButton.setTooltip ("Let the Crossing control set the strum speed instead of the genre kit");
    followKnobButton.onClick = [this]
    {
        rhythm().setStrumDurationMs (0.0);
        refresh();
    };
    addChildComponent (followKnobButton);

    auto attach = [this] (LuthierSlider& slider, const char* id, const char* tip)
    {
        slider.attachTo (processor, id, tip);
        addAndMakeVisible (slider);
    };

    attach (crossing, ParamIDs::strumCrossingSps,
            "How fast the hand crosses the strings, in strings per second. 200 is a medium strum "
            "(six strings in 25 ms); 60 is a slow, deliberate one.");
    attach (acceleration, ParamIDs::strumAcceleration,
            "0 crosses at a constant speed. Higher starts and finishes slower than the middle, "
            "as a real hand does.");
    attach (upRatio, ParamIDs::strumUpVelocityRatio,
            "How much faster the up-stroke is than the down-stroke");
    attach (tilt, ParamIDs::strumTilt,
            "Positive puts more force on the treble strings, negative on the bass");

    // strum_evenness is the rhythm engine's own state (a kit sets it), so this
    // one slider is not a parameter attachment.
    evenness.getSlider().setRange (0.0, 1.0, 0.01);
    evenness.getSlider().setDoubleClickReturnValue (true, StrumSettings::guitarDefaults().evenness);
    evenness.setTooltip ("1 strikes every string with the same force; lower lets each string vary, "
                         "up to 40 % at 0");
    evenness.getSlider().onValueChange = [this]
    {
        if (! updating)
            rhythm().setStrumEvenness (evenness.getSlider().getValue());
    };
    AccessibleSetup::configureSlider (evenness.getSlider(), "Strum evenness");   // not an attachment, so named here
    addAndMakeVisible (evenness);

    attach (misses, ParamIDs::strumMissProbability,
            "The chance the hand crosses a string without striking it. The first string it "
            "reaches is missed three times as often.");

    strikerDown.attachTo (processor, ParamIDs::strumStrikerDown,
                          "What crosses the strings on a down-strum. A thumb is slower and has no pick click.");
    strikerUp.attachTo (processor, ParamIDs::strumStrikerUp,
                        "What crosses the strings on an up-strum. Fingerstyle players often use nails up.");
    addAndMakeVisible (strikerDown);
    addAndMakeVisible (strikerUp);

    attach (chuckAmount, ParamIDs::chuckAmount,
            "Blends every strum toward a chuck: the fretting hand flat on the strings. Chuck steps "
            "in a pattern are always full chucks.");
    attach (chuckDamping, ParamIDs::chuckDamping,
            "How hard the fretting hand stops the strings during a chuck");

    refresh();
    startTimerHz (5);
}

StrumGroup::~StrumGroup()
{
    stopTimer();
}

RhythmEngine& StrumGroup::rhythm()
{
    return processor.getEngine().getRhythmEngine();
}

const RhythmEngine& StrumGroup::rhythm() const
{
    return processor.getEngine().getRhythmEngine();
}

//==============================================================================
juce::String StrumGroup::describeCrossingSource() const
{
    double sps = 0.0;

    switch (rhythm().getCrossingSource (sps))
    {
        case RhythmEngine::CrossingSource::pattern:
            return "Crossing set by the pattern: " + juce::String (juce::roundToInt (sps)) + " sps";

        case RhythmEngine::CrossingSource::kit:
            return "Crossing set by the genre kit: " + juce::String (juce::roundToInt (sps)) + " sps";

        case RhythmEngine::CrossingSource::step:
        case RhythmEngine::CrossingSource::global:
        default:
            return "Crossing set by the control below";
    }
}

void StrumGroup::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    // A drag in progress is the user's; the engine catches up when it ends.
    if (! evenness.getSlider().isMouseButtonDown())
        evenness.getSlider().setValue (rhythm().getStrumEvenness(), juce::dontSendNotification);

    const auto text = describeCrossingSource();

    if (text != sourceLabel.getText())
        sourceLabel.setText (text, juce::dontSendNotification);

    double sps = 0.0;
    const bool kitInCharge = rhythm().getCrossingSource (sps) == RhythmEngine::CrossingSource::kit;

    if (kitInCharge != followKnobButton.isVisible())
    {
        followKnobButton.setVisible (kitInCharge);
        resized();
    }
}

void StrumGroup::timerCallback()
{
    refresh();
}

//==============================================================================
void StrumGroup::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void StrumGroup::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (gap);
        return r;
    };

    heading.setBounds (take (headingHeight));

    {
        auto r = take (sourceHeight);

        if (followKnobButton.isVisible())
        {
            followKnobButton.setBounds (r.removeFromRight (76).reduced (0, 1));
            r.removeFromRight (4);
        }

        sourceLabel.setBounds (r);
    }

    for (auto* s : { &crossing, &acceleration, &upRatio, &tilt, &evenness, &misses })
        s->setBounds (take (rowHeight));

    strikerDown.setBounds (take (choiceHeight));
    strikerUp.setBounds (take (choiceHeight));

    chuckAmount.setBounds (take (rowHeight));
    chuckDamping.setBounds (take (rowHeight));
}

} // namespace luthier
