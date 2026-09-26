#include "UndoHistoryPanel.h"
#include "Theme.h"
#include "../PluginProcessor.h"

namespace luthier
{

UndoHistoryPanel::UndoHistoryPanel (LuthierAudioProcessor& p)
    : processor (p)
{
    search.setTextToShowWhenEmpty ("Search undo history", Palette::textDisabled);
    search.setTitle ("Search undo history");
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (search);

    list.setModel (this);
    list.setRowHeight (kRowHeight);
    list.setTitle ("Undo history");
    addAndMakeVisible (list);

    refresh();
    setSize (320, 360);
}

UndoHistoryPanel::~UndoHistoryPanel()
{
    list.setModel (nullptr);
}

void UndoHistoryPanel::setSearchText (const juce::String& text)
{
    search.setText (text, false);
    refresh();
}

void UndoHistoryPanel::refresh()
{
    const auto filter = search.getText().trim();
    shown.clearQuick();

    for (const auto& item : processor.getUndoHistory())
        if (filter.isEmpty() || item.description.containsIgnoreCase (filter))
            shown.add (item);

    list.updateContent();
    list.repaint();
}

void UndoHistoryPanel::chooseRow (int row)
{
    if (! juce::isPositiveAndBelow (row, shown.size()))
        return;

    processor.undoSteps (shown.getReference (row).stepsBack);

    if (onChosen != nullptr)
        onChosen();
}

int UndoHistoryPanel::getNumRows()
{
    return shown.size();
}

void UndoHistoryPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, shown.size()))
        return;

    const auto& item = shown.getReference (row);

    if (selected)
        g.fillAll (Palette::accent.withAlpha (0.25f));

    auto bounds = juce::Rectangle<int> (0, 0, width, height).reduced (6, 0);

    // Section 5: a boundary is a rule with its subtitle.
    if (item.boundary)
    {
        g.setColour (Palette::edge);
        g.fillRect (0, 0, width, 1);
    }

    g.setColour (item.boundary ? Palette::secondary : Palette::textPrimary);
    g.setFont (Fonts::ui (12.0f));
    g.drawText (item.description, bounds, juce::Justification::centredLeft, true);
}

void UndoHistoryPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    chooseRow (row);
}

void UndoHistoryPanel::resized()
{
    auto bounds = getLocalBounds().reduced (6);
    search.setBounds (bounds.removeFromTop (24));
    bounds.removeFromTop (6);
    list.setBounds (bounds);
}

void UndoHistoryPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);
}

} // namespace luthier
