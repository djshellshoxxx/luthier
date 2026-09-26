#include "StringInteractionGroup.h"
#include "../PluginProcessor.h"

namespace luthier
{

StringInteractionGroup::StringInteractionGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    heading.setText ("STRING INTERACTION", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    styleNote.setFont (Fonts::ui (10.0f));
    styleNote.setColour (juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible (styleNote);

    auto attach = [this] (LuthierSlider& s, const char* id, const char* tip)
    {
        s.attachTo (processor, id, tip);
        addAndMakeVisible (s);
    };

    attach (air, ParamIDs::couplingAirAmount,
            "The top's sound driving every string through the air: what lets the high E sing along with a low-E note "
            "on an acoustic. 15-35 dB under the bridge path there, inaudible on a solid body.");
    attach (palmWidth, ParamIDs::palmMuteSpread,
            "How wide the palm lies across the strings when palm muting (CC 67). 35 mm covers about three strings "
            "fully and the next one partly; the strip below shows which.");
    attach (neighbour, ParamIDs::adjacentMuteAmount,
            "A fretting finger's underside rests on the next thinner string and its tip on the next thicker one, "
            "muting them if they are not played. Rock players rely on it; classical technique arches to avoid it.");
    attach (stagger, ParamIDs::releaseStaggerMs,
            "Fingers leave a chord one by one: the largest delay between the first and last string to stop.");
    attach (order, ParamIDs::releaseStaggerBias,
            "Which strings let go first: toward +1 the treble, toward -1 the bass, 0 no preference.");
    attach (aperture, ParamIDs::pickupApertureScale,
            "How wide each pole piece senses. A bent string moves off its pole: about 1 dB quieter on a neck single coil for a whole step.");
    attach (thump, ParamIDs::mutedThumpLevel,
            "A strum across a string the fretting hand mutes still hits it: a short pitchless thump.");

    startTimerHz (10);
    timerCallback();
}

StringInteractionGroup::~StringInteractionGroup()
{
    stopTimer();
}

juce::String StringInteractionGroup::describeFrettingStyle() const
{
    const bool classical = processor.getEngine().getStringInteraction().frettingStyle < 0.5;
    return classical ? "Fretting style: classical fingertips (from the RIGHT HAND style) - neighbour mute x0.1"
                     : "Fretting style: rock spread - neighbour mute at full depth";
}

void StringInteractionGroup::timerCallback()
{
    const auto text = describeFrettingStyle();

    if (styleNote.getText() != text)
        styleNote.setText (text, juce::dontSendNotification);

    bool changed = false;

    for (int s = 0; s < (int) palm.size(); ++s)
    {
        const float w = s < processor.getEngine().getNumStrings() ? processor.getEngine().getPalmWeight (s) : 0.0f;
        changed = changed || std::abs (w - palm[(size_t) s]) > 0.01f;
        palm[(size_t) s] = w;
    }

    if (changed)
        repaint (palmStrip);
}

void StringInteractionGroup::paint (juce::Graphics& g)
{
    // gui-techniques-updates.md 4's mute zone, in small: band opacity is the palm's weight.
    const int n = juce::jmax (1, processor.getEngine().getNumStrings());
    const float w = (float) palmStrip.getWidth() / (float) n;

    g.setColour (Palette::panelSunken);
    g.fillRect (palmStrip);

    for (int s = 0; s < n; ++s)
    {
        // String 0 is the high E: draw it on the right, as the player sees the neck from above.
        auto cell = juce::Rectangle<float> ((float) palmStrip.getRight() - (float) (s + 1) * w, (float) palmStrip.getY(),
                                            w, (float) palmStrip.getHeight()).reduced (1.0f);
        g.setColour (Palette::accent.withAlpha (juce::jlimit (0.0f, 1.0f, palm[(size_t) s]) * 0.85f));
        g.fillRect (cell);
        g.setColour (Palette::textDisabled);
        g.drawVerticalLine ((int) cell.getCentreX(), cell.getY(), cell.getBottom());
    }
}

void StringInteractionGroup::resized()
{
    auto bounds = getLocalBounds();
    auto take = [&bounds] (int h) { auto r = bounds.removeFromTop (h); bounds.removeFromTop (gap); return r; };

    heading.setBounds (take (headingHeight));
    air.setBounds (take (rowHeight));
    palmWidth.setBounds (take (rowHeight));
    palmStrip = take (stripHeight);
    neighbour.setBounds (take (rowHeight));
    styleNote.setBounds (take (noteHeight));

    for (auto* s : { &stagger, &order, &aperture, &thump })
        s->setBounds (take (rowHeight));
}

} // namespace luthier
