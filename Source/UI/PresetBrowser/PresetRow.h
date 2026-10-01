#pragma once

/*  preset-browser-previews.md 7.3: one row of the list.

    Play glyph, heart, name, then (Easy) the first tags or (Advanced) the
    Category / Guitar / Amp / Tags columns, the 64-peak waveform and the
    stars. The row's accessible name is 10's summary ("Jazz Hollowbody,
    Electric Jazz, factory, favourite, 4 stars, warm, clean, jazz").
*/

#include "PresetBrowserWidgets.h"

namespace luthier
{

class PresetBrowserPanel;

class PresetRow : public juce::Component
{
public:
    explicit PresetRow (PresetBrowserPanel& owner);

    void setEntry (int entry, bool selected);
    int getEntry() const noexcept { return entry; }

    /** Updates the glyph and the waveform's progress (the panel's timer). */
    void refreshPlayState();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    PlayGlyph& getGlyph() noexcept { return glyph; }
    HeartButton& getHeart() noexcept { return heart; }
    StarRating& getStars() noexcept { return stars; }

private:
    juce::Rectangle<int> waveformArea() const;

    PresetBrowserPanel& owner;
    int entry = -1;
    bool selected = false;
    float progress = -1.0f;

    PlayGlyph glyph;
    HeartButton heart;
    StarRating stars;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetRow)
};

} // namespace luthier
