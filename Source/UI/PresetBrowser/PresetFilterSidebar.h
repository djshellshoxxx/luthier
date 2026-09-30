#pragma once

/*  preset-browser-previews.md 7.2: the Advanced filter sidebar.

    SOURCE, FAMILY, GENRE, TECHNIQUES ("Uses Techniques" and the six
    technique chips, gui-techniques-updates 7), TONE, RATING >= and the
    Favourites / Recent toggles, with Clear filters. OR within a group, AND
    across groups (PresetSearch::passes); the state persists in UiPreferences
    through the panel.
*/

#include "PresetBrowserWidgets.h"

namespace luthier
{

class PresetBrowserPanel;

class PresetFilterSidebar : public juce::Component
{
public:
    explicit PresetFilterSidebar (PresetBrowserPanel& owner);

    /** Reads the panel's filters into the chips. */
    void refresh();

    /** The height the groups need at the current width. */
    int getIdealHeight (int width) const;

    void paint (juce::Graphics&) override;
    void resized() override;

    ChipButton* findChip (const juce::String& group, const juce::String& text) const;
    juce::ComboBox& getRatingBox() noexcept { return ratingBox; }

private:
    struct Group
    {
        juce::String title;
        juce::OwnedArray<ChipButton> chips;
        int top = 0;
    };

    Group& addGroup (const juce::String& title, const juce::StringArray& labels);
    void chipClicked (Group&, ChipButton&);
    int layoutGroups (int width, bool apply);

    PresetBrowserPanel& owner;
    juce::OwnedArray<Group> groups;
    juce::ComboBox ratingBox;
    juce::Label ratingLabel;
    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetFilterSidebar)
};

} // namespace luthier
