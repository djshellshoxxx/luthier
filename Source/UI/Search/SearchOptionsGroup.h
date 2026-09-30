#pragma once

/*  global-search.md 7: Options -> ACCESSIBILITY's "Search" group.

      - Switch mode automatically to show a result (search.autoSwitchMode, on)
      - Remember recent searches (search.rememberRecent, on; off clears both stores)
      - Clear recent searches (the same function as cmd:clearRecentSearches)

    UiPreferences values, saved immediately; not parameters, not preset data.
    Its own component so the shared Options page only places it.
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier::search
{

constexpr const char* kAutoSwitchModeKey = "search.autoSwitchMode";

bool isAutoSwitchModeOn();
void setAutoSwitchMode (bool on);

/** Clears recent items and recent searches (7, and the command). */
void clearRecentSearches();

class SearchOptionsGroup : public juce::Component
{
public:
    SearchOptionsGroup();

    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredHeight = 52;

    juce::ToggleButton& getAutoSwitchToggle() noexcept { return autoSwitch; }
    juce::ToggleButton& getRememberToggle() noexcept   { return remember; }
    juce::TextButton& getClearButton() noexcept        { return clear; }

private:
    juce::ToggleButton autoSwitch, remember;
    juce::TextButton clear;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchOptionsGroup)
};

} // namespace luthier::search
