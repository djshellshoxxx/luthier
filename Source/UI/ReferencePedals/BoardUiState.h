// OWNER: WP17
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace luthier::reference
{
struct BoardUiState
{
    juce::Rectangle<int> window {120,80,1100,520};
    bool popoutOpen = false, dockOpen = false, cables = true, browser = true;
    int dockHeight = 232;
    float zoom = 1.f;
    int scrollX = 0, scrollY = 0;
    static juce::Rectangle<int> clampWindow (juce::Rectangle<int>, juce::Rectangle<int> work);
    static BoardUiState read (const juce::var&);
    juce::var write (const juce::var& previous) const;
};
}
