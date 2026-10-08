#include "SlapGroup.h"
#include "../PluginProcessor.h"

namespace luthier
{

SlapGroup::SlapGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    heading.setText ("SLAP", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    for (auto [label, text] : { std::pair<juce::Label*, const char*> { &slapLabel, "Thumb slap" },
                                { &popLabel, "Pop" }, { &ghostLabel, "Ghost notes" }, { &fingerLabel, "Fingerstyle" } })
    {
        label->setText (text, juce::dontSendNotification);
        label->setFont (Fonts::ui (11.0f));
        label->setColour (juce::Label::textColourId, Palette::textMuted);
        addAndMakeVisible (*label);
    }

    auto attach = [this] (LuthierSlider& slider, const char* id, const char* tip)
    {
        slider.attachTo (processor, id, tip);
        addAndMakeVisible (slider);
    };

    auto attachToggle = [this] (LuthierToggle& toggle, const char* id, const char* tip)
    {
        toggle.attachTo (processor, id, tip);
        addAndMakeVisible (toggle);
    };

    attach (slapStrength, ParamIDs::slapStrength, "How hard the thumb drives the string");
    attach (slapPosition, ParamIDs::slapPositionMm, "Where the thumb lands, from the last fret toward the bridge");
    attach (thumbHardness, ParamIDs::slapThumbHardness, "Contact stiffness: harder is clackier");
    attach (fretContact, ParamIDs::slapFretContact,
            "How far the string is driven into the frets. 0 is a thumb thump with no clack (Motown-style).");
    attach (popStrength, ParamIDs::popStrength, "How far the finger pulls the string off the board");
    attach (popPosition, ParamIDs::popPositionMm, "Where the finger hooks the string, from the last fret");
    attachToggle (doubleThump, ParamIDs::doubleThumpEnabled, "The thumb strikes down and again on the way back up");
    attach (reboundGap, ParamIDs::slapReboundGap, "Time between the down-stroke and rebound, in milliseconds");
    attach (upRatio, ParamIDs::doubleThumpUpRatio, "How loud the up-stroke is against the down-stroke");
    attach (ghostLevel, ParamIDs::ghostLevel, "A ghost note's level against a full note");
    attach (ghostDamping, ParamIDs::ghostDamping, "How firmly the fretting hand rests on a ghosted string");
    attachToggle (ghostAuto, ParamIDs::ghostAuto, "Quiet notes become ghost notes, which is most of a groove");
    attach (ghostThreshold, ParamIDs::ghostVelocityThreshold, "Notes softer than this velocity are ghosted");
    attach (alternation, ParamIDs::fingerAlternationVariation,
            "How different the index and middle fingers are in timing and tone. 0 makes them identical.");
    attachToggle (restStroke, ParamIDs::restStroke,
                  "Each finger comes to rest on the next-lower string and stops it, as a real bassist's does");

    shown = isBassLoaded();
    setVisible (shown);
    startTimerHz (4);
}

SlapGroup::~SlapGroup()
{
    stopTimer();
}

bool SlapGroup::isBassLoaded() const
{
    return processor.getEngine().getGuitarSpec().category == GuitarCategory::Bass;
}

juce::StringArray SlapGroup::getAttachedParameterIds() const
{
    juce::StringArray ids;

    for (auto* s : { &slapStrength, &slapPosition, &thumbHardness, &fretContact, &popStrength, &popPosition,
                     &upRatio, &reboundGap, &ghostLevel, &ghostDamping, &ghostThreshold, &alternation })
        ids.add (s->getLearnParameterId());

    for (auto* t : { &doubleThump, &ghostAuto, &restStroke })
        ids.add (t->getLearnParameterId());

    return ids;
}

void SlapGroup::timerCallback()
{
    const bool bass = isBassLoaded();

    if (bass != shown)
    {
        shown = bass;
        setVisible (bass);

        if (onShownChanged)
            onShownChanged();
    }
}

int SlapGroup::preferredHeight() const
{
    if (! shown)
        return 0;

    return 20 + 4 * 16 + 12 * 24 + 3 * 26 + 8;
}

void SlapGroup::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (2);
        return r;
    };

    heading.setBounds (take (20));

    slapLabel.setBounds (take (14));
    for (auto* s : { &slapStrength, &slapPosition, &thumbHardness, &fretContact })
        s->setBounds (take (22));

    popLabel.setBounds (take (14));
    popStrength.setBounds (take (22));
    popPosition.setBounds (take (22));
    doubleThump.setBounds (take (24).removeFromLeft (140));
    reboundGap.setBounds (take (22));
    upRatio.setBounds (take (22));

    ghostLabel.setBounds (take (14));
    ghostLevel.setBounds (take (22));
    ghostDamping.setBounds (take (22));
    ghostAuto.setBounds (take (24).removeFromLeft (140));
    ghostThreshold.setBounds (take (22));

    fingerLabel.setBounds (take (14));
    alternation.setBounds (take (22));
    restStroke.setBounds (take (24).removeFromLeft (140));
}

} // namespace luthier
