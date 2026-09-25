#include "FirstEncounterHint.h"
#include "FirstRun.h"
#include "HelpContent.h"
#include "UiPreferences.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

FirstEncounterHint::FirstEncounterHint (const juce::String& key, const juce::String& t)
    : preferenceKey (key), text (t)
{
    dismissButton.onClick = [this] { dismiss(); };
    AccessibleSetup::configureButton (dismissButton, "Got it", "Dismiss this hint");
    addAndMakeVisible (dismissButton);

    AccessibleSetup::configureDescriptive (*this, "Hint", getText());

    // Hidden until owed; the host adds it with addChildComponent.
    setVisible (false);
}

bool FirstEncounterHint::showIfDue()
{
    if (isVisible())
        return true;

    if (! FirstRun::isFirstSession() || UiPreferences::get().getBool (preferenceKey, false))
        return false;

    // Recorded before it is on screen, as the range explainer is.
    UiPreferences::get().setBool (preferenceKey, true);

    setHelpText (getText());
    setVisible (true);

    if (onShownOrDismissed != nullptr)
        onShownOrDismissed();

    return true;
}

void FirstEncounterHint::dismiss()
{
    if (! isVisible())
        return;

    setVisible (false);

    if (onShownOrDismissed != nullptr)
        onShownOrDismissed();
}

void FirstEncounterHint::setText (const juce::String& newText)
{
    if (newText == text)
        return;

    text = newText;
    setHelpText (getText());
    repaint();
}

juce::String FirstEncounterHint::getText() const
{
    return HelpContent::resolveKeys (text);
}

void FirstEncounterHint::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (Palette::panelRaised);
    g.fillRoundedRectangle (bounds, Metrics::panelCorner);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (bounds, Metrics::panelCorner, 1.0f);

    // The bar marks it as a hint by position and shape, not by colour alone.
    g.setColour (Palette::accent);
    g.fillRect (getLocalBounds().removeFromLeft (3).reduced (0, 4));

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (12.0f));
    g.drawFittedText (getText(), textBounds, juce::Justification::centredLeft, 2);
}

void FirstEncounterHint::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::grid, Metrics::gridHalf);

    dismissButton.setBounds (bounds.removeFromRight (72).withSizeKeepingCentre (72, Metrics::buttonHeight - 4));
    bounds.removeFromRight (Metrics::grid);
    textBounds = bounds;
}

} // namespace luthier
