#include "VisualAidsSection.h"
#include "Theme.h"
#include "Guitar/StringAnimator.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"

namespace luthier
{

VisualAidsSection::VisualAidsSection()
{
    setTitle (tr ("options.appearance.visualAids.heading"));

    animateStringsToggle.setButtonText (tr ("options.appearance.visualAids.animateStrings"));
    animateStringsToggle.setTitle (tr ("options.appearance.visualAids.animateStrings"));
    animateStringsToggle.onClick = [this]
    {
        if (updating)
            return;

        // Written through at once; every open editor picks it up on its next tick (5).
        StringAnimationSettings::setEnabled (animateStringsToggle.getToggleState());
        refresh();
    };
    addAndMakeVisible (animateStringsToggle);

    qualityLabel.setText (tr ("options.appearance.visualAids.quality"), juce::dontSendNotification);
    qualityLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    qualityLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (qualityLabel);

    animateQualityBox.addItem (tr ("options.appearance.visualAids.qualityLow"), 1);
    animateQualityBox.addItem (tr ("options.appearance.visualAids.qualityHigh"), 2);
    animateQualityBox.setTitle (tr ("options.appearance.visualAids.qualityName"));
    animateQualityBox.onChange = [this]
    {
        if (updating)
            return;

        StringAnimationSettings::setQuality (animateQualityBox.getSelectedId() == 1 ? StringAnimationQuality::low
                                                                                     : StringAnimationQuality::high);
        refresh();
    };
    addAndMakeVisible (animateQualityBox);

    animateHelpLabel.setText (tr ("options.appearance.visualAids.help"), juce::dontSendNotification);
    animateHelpLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    animateHelpLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    animateHelpLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (animateHelpLabel);

    animateStatusLabel.setText (tr ("options.appearance.visualAids.paused"), juce::dontSendNotification);
    animateStatusLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    animateStatusLabel.setColour (juce::Label::textColourId, Palette::textDisabled);
    animateStatusLabel.setJustificationType (juce::Justification::topLeft);
    addChildComponent (animateStatusLabel);

    // 8: after "Reduced motion" in the page's Tab order.
    animateStringsToggle.setExplicitFocusOrder (1);
    animateQualityBox.setExplicitFocusOrder (2);

    refresh();
}

juce::String VisualAidsSection::describe() const
{
    auto text = tr ("options.appearance.visualAids.help");

    if (AccessibilitySettings::get().isReducedMotion())
        text << " " << tr ("options.appearance.visualAids.paused");

    return text;
}

void VisualAidsSection::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    animateStringsToggle.setToggleState (StringAnimationSettings::isEnabled(), juce::dontSendNotification);
    animateQualityBox.setSelectedId (StringAnimationSettings::getQuality() == StringAnimationQuality::low ? 1 : 2,
                                     juce::dontSendNotification);

    // The status line shows only when it applies (5, AS-25).
    const bool paused = AccessibilitySettings::get().isReducedMotion();

    if (paused != animateStatusLabel.isVisible())
    {
        animateStatusLabel.setVisible (paused);
        resized();
    }

    const auto description = describe();
    animateStringsToggle.setDescription (description);
    animateQualityBox.setDescription (description);
    animateStringsToggle.setHelpText (description);
}

int VisualAidsSection::getPreferredHeight() const
{
    return kHeadingHeight + kRowHeight + 30 + (animateStatusLabel.isVisible() ? 16 : 0);
}

void VisualAidsSection::paint (juce::Graphics& g)
{
    g.setColour (Palette::accent);
    g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    g.drawText (tr ("options.appearance.visualAids.heading"), getLocalBounds().removeFromTop (kHeadingHeight),
                juce::Justification::centredLeft, false);
}

void VisualAidsSection::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (kHeadingHeight);

    auto row = bounds.removeFromTop (kRowHeight);
    animateStringsToggle.setBounds (row.removeFromLeft (220));
    row.removeFromLeft (8);
    qualityLabel.setBounds (row.removeFromLeft (52));
    row.removeFromLeft (4);
    animateQualityBox.setBounds (row.removeFromLeft (100));

    auto text = bounds.withTrimmedLeft (24);
    animateHelpLabel.setBounds (text.removeFromTop (30));
    animateStatusLabel.setBounds (text.removeFromTop (16));
}

} // namespace luthier
