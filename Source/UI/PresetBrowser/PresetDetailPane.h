#pragma once

/*  preset-browser-previews.md 7.1 / 7.2 / 7.3: the right-hand pane.

    The guitar thumbnail (guitar-illustration.md 15), name, category, the tag
    chips (author tags filled, auto-descriptors outlined with "Detected from
    the sound"; a click adds the word to the search), author, guitar, amp,
    techniques and description. Easy mode ends with [More like this];
    Advanced mode lists SOUNDS LIKE: the 8 nearest, each with a play glyph.
*/

#include "PresetBrowserWidgets.h"
#include <map>

namespace luthier
{

class PresetBrowserPanel;

class PresetDetailPane : public juce::Component
{
public:
    explicit PresetDetailPane (PresetBrowserPanel& owner);

    /** Shows an index entry (or nothing, for -1). */
    void setEntry (int entry);
    int getEntry() const noexcept { return entry; }

    /** Rebuilds the SOUNDS LIKE list and the glyph states. */
    void refresh();
    void refreshPlayState();

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Button& getMoreLikeThis() noexcept { return moreLikeThis; }
    const juce::OwnedArray<ChipButton>& getChips() const noexcept { return chips; }
    int getNumSimilarRows() const noexcept { return similarRows.size(); }
    juce::Button& getSimilarGlyph (int i) noexcept { return similarRows[i]->glyph; }

    /** The thumbnail currently drawn (tests: it is a real render). */
    const juce::Image& getThumbnail() const noexcept { return thumbnail; }

private:
    struct SimilarRow : public juce::Component
    {
        SimilarRow (PresetDetailPane& p, int e);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

        PresetDetailPane& pane;
        int entry;
        PlayGlyph glyph;
    };

    void rebuildChips();
    void updateThumbnail();

    PresetBrowserPanel& owner;
    int entry = -1;

    juce::Image thumbnail;
    juce::String thumbnailKey;
    std::map<juce::String, juce::Image> thumbnailCache;

    juce::Label nameLabel, categoryLabel, metaLabel, descriptionLabel, similarHeading;
    juce::OwnedArray<ChipButton> chips;
    juce::TextButton moreLikeThis { "More like this" };
    juce::OwnedArray<SimilarRow> similarRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetDetailPane)
};

} // namespace luthier
