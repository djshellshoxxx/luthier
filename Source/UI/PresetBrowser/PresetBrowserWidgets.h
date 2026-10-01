#pragma once

/*  preset-browser-previews.md 7.3 and 10: the small controls the browser is
    made of - chips, the play glyph, the heart, the stars and the waveform.

    Each one has an accessible name and role of its own (10: "every row,
    glyph, heart, star control and chip has an accessible name"). Author and
    auto chips differ by fill versus outline, never by colour alone.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Theme.h"
#include "../../Presets/Preview/PreviewRenderer.h"

namespace luthier
{

//==============================================================================
/** A filter or tag chip: a toggle, filled when on (or when it is an author
    tag), outlined otherwise. */
class ChipButton : public juce::Button
{
public:
    enum class Style { filter, authorTag, autoTag };

    ChipButton (const juce::String& text, Style style = Style::filter);

    Style getStyle() const noexcept { return style; }

    /** The width the chip's text wants at the chip font. */
    int getIdealWidth() const;

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Style style;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChipButton)
};

//==============================================================================
/** 7.3's play glyph: one state at a time. */
class PlayGlyph : public juce::Button
{
public:
    enum class State { idle, playing, preparing, failed, hidden };

    PlayGlyph();

    void setState (State, float progress = 0.0f, const juce::String& failure = {});
    State getGlyphState() const noexcept { return state; }

    /** 10: the static label and the text replace motion. */
    void setReducedMotion (bool r) noexcept { reducedMotion = r; }

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    State state = State::idle;
    float progress = 0.0f;
    bool reducedMotion = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayGlyph)
};

//==============================================================================
/** The favourite heart, a toggle. */
class HeartButton : public juce::Button
{
public:
    HeartButton();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeartButton)
};

//==============================================================================
/** 1-5 stars; clicking the current star clears the rating. Exposes an
    accessible value from 0 to 5 (10). */
class StarRating : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    StarRating();

    void setRating (int stars);
    int getRating() const noexcept { return rating; }

    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** What a click on star `n` (1-5) does to the rating. */
    int ratingAfterClickOn (int star) const noexcept { return star == rating ? 0 : star; }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    int rating = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StarRating)
};

//==============================================================================
/** Paints a 64-peak waveform, the played part in the accent colour. */
void paintPeaks (juce::Graphics&, juce::Rectangle<float> area,
                 const std::array<float, PreviewResult::kNumPeaks>& peaks, float playedFraction);

} // namespace luthier
