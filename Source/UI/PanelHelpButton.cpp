#include "PanelHelpButton.h"
#include "Theme.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

PanelHelpButton::PanelHelpButton (const juce::String& t)
    : juce::Button ("?")
{
    setTopic (t);
}

void PanelHelpButton::setTopic (const juce::String& newTopic)
{
    topic = newTopic;
    setTooltip ("Help on " + topic + " (F1)");
    AccessibleSetup::configureButton (*this, "Help on " + topic, "Opens Help pinned to the " + topic + " panel.");
}

void PanelHelpButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.5f);
    const auto d = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto circle = bounds.withSizeKeepingCentre (d, d);

    g.setColour (down ? Palette::accent : (highlighted ? Palette::panelRaised : Palette::panelSunken));
    g.fillEllipse (circle);
    g.setColour (highlighted ? Palette::accent : Palette::edgeBright);
    g.drawEllipse (circle, 1.0f);

    g.setColour (down ? Palette::plateText : (highlighted ? Palette::accentBright : Palette::textMuted));
    g.setFont (Fonts::ui (d * 0.7f, true));
    g.drawText ("?", circle, juce::Justification::centred);
}

void PanelHelpButton::clicked()
{
    if (onHelp != nullptr)
        onHelp (topic);
}

} // namespace luthier
