#include "NormalizationBadge.h"
#include "../PluginProcessor.h"

namespace luthier
{

NormalizationBadge::NormalizationBadge (LuthierAudioProcessor& p)
    : juce::Button ("Output normalization"), processor (p)
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setVisible (false);
    refresh();
    startTimerHz (10);
}

NormalizationBadge::~NormalizationBadge()
{
    stopTimer();
}

void NormalizationBadge::refresh()
{
    const auto status = processor.getNormalizationStatus();
    const bool on = status.enabled;

    const auto newText = OutputNormalization::badgeText (status);

    if (newText != text)
    {
        text = newText;
        repaint();
    }

    if (on)
    {
        setTooltip (OutputNormalization::badgeTooltip (status));
        setTitle (OutputNormalization::accessibleBadgeName (status));
        setDescription (OutputNormalization::badgeTooltip (status));
    }

    if (on != isVisible())
    {
        setVisible (on);

        if (onVisibilityChanged)
            onVisibilityChanged();
    }
}

void NormalizationBadge::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f, 3.0f);

    g.setColour (Palette::secondary.withAlpha (down ? 0.35f : (highlighted ? 0.25f : 0.14f)));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (hasKeyboardFocus (false) ? Palette::accentBright : Palette::secondary.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    g.setColour (Palette::textPrimary);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawFittedText (text, getLocalBounds().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
}

bool NormalizationBadge::keyPressed (const juce::KeyPress& key)
{
    // 5.1: Enter on the badge opens the options, as a click does.
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        triggerClick();
        return true;
    }

    return juce::Button::keyPressed (key);
}

void NormalizationBadge::clicked()
{
    if (onOpenOptions)
        onOpenOptions();
}

} // namespace luthier
