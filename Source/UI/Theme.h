#pragma once

/*  Visual identity: the Luthier guitar-shop theme.

    theme.md is the house style shared by a family of plugins. For this one,
    proposals/visual-polish.md section 6 (approved by the user, 2026-09-23)
    replaces its look with a guitar shop and a workbench: rosewood and walnut
    panels, Tolex black, ivory text, an aged-brass accent, black bell knobs with
    cream pointers, mini toggle switches, brass fader caps, engraved plates for
    section headers, and a vintage display face. What section 6 does not
    mention still comes from theme.md: the 8 px grid, the 270-degree value arcs
    drawn outside the knob (they carry the value and the advanced-range
    marking), the corner radii, the header layout, the output LED and the data
    stream.

    The palette is live: the colours below are the ones in force, set from
    AccessibilitySettings by Palette::apply(), so the colourblind, High-contrast
    and Light palettes (accessibility.md 6) reach every panel. High contrast
    turns textures and sheen off (visual-polish.md 0.2).
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{

//==============================================================================
struct PaletteColours;

namespace Palette
{
    // --- neutrals: rosewood, walnut and Tolex -----------------------------------
    inline juce::Colour backgroundDeep  { 0xff17100c };
    inline juce::Colour background      { 0xff1e1511 };
    inline juce::Colour panel           { 0xff2a1e17 };
    inline juce::Colour panelRaised     { 0xff1d1a18 };
    inline juce::Colour panelSunken     { 0xff140e0b };
    inline juce::Colour edge            { 0xff4a3726 };
    inline juce::Colour edgeBright      { 0xff6b5033 };

    // --- accents ---------------------------------------------------------------
    /** Aged brass. */
    inline juce::Colour accent          { 0xffd4a24c };
    inline juce::Colour accentBright    { 0xffe9be6e };
    inline juce::Colour accentDim       { 0xff8c6a2e };

    /** The green-teal of an old amp's jewel light. */
    inline juce::Colour secondary       { 0xff6fa58a };
    inline juce::Colour secondaryDim    { 0xff3e6450 };

    // --- text -------------------------------------------------------------------
    inline juce::Colour textPrimary     { 0xffefe3cc };
    inline juce::Colour textMuted       { 0xffb9a58a };
    inline juce::Colour textDisabled    { 0xff756650 };

    // --- status -----------------------------------------------------------------
    inline juce::Colour success         { 0xff8fbf6f };
    inline juce::Colour warning         { 0xfff0824a };
    inline juce::Colour clip            { 0xffe0503a };

    /** The scrolling internals readout. */
    inline juce::Colour dataStream      { 0xff7fd18a };

    inline juce::Colour shadow          { 0x99000000 };

    // --- materials (visual-polish.md 6.3) ----------------------------------------
    inline juce::Colour knobBody        { 0xff151312 };   ///< black bell knob
    inline juce::Colour knobPointer     { 0xffefe3cc };   ///< cream pointer line
    inline juce::Colour plate           { 0xffc9a25a };   ///< engraved brass plate
    inline juce::Colour plateText       { 0xff2a1a0c };

    /** False under High contrast: no grain, sheen, screws or gradients. */
    inline bool textured = true;

    /** Puts a palette in force. Components already built keep their own
        colours; remap() moves those across. Message thread. */
    void apply (const PaletteColours& colours, bool texturedSurfaces);

    /** The palette last applied. */
    const PaletteColours& current();

    /** Walks a component tree and replaces every stored colour that was a role
        of `from` with the same role of `to`, so a palette change reaches
        colours set with setColour() when the panel was built. */
    void remap (juce::Component& root, const PaletteColours& from, const PaletteColours& to);
}

//==============================================================================
namespace Metrics
{
    inline constexpr int grid = 8;
    inline constexpr int gridHalf = 4;

    inline constexpr int headerHeight = 48;
    inline constexpr int footerHeight = 18;

    inline constexpr float windowCorner = 6.0f;
    inline constexpr float panelCorner = 4.0f;
    inline constexpr float controlCorner = 2.0f;

    inline constexpr int knobSmall = 36;
    inline constexpr int knobDefault = 48;
    inline constexpr int knobLarge = 64;
    inline constexpr int knobMacro = 76;

    inline constexpr int buttonHeight = 28;
    inline constexpr int rowHeight = 24;

    inline constexpr int windowPadding = 16;

    /** Value arc: 270 degrees from 7 o'clock to 5 o'clock. */
    inline constexpr float arcStart = juce::MathConstants<float>::pi * 1.25f;
    inline constexpr float arcEnd   = juce::MathConstants<float>::pi * 2.75f;
    inline constexpr float arcThickness = 3.0f;
    inline constexpr float arcGap = 4.0f;

    inline constexpr int tooltipDelayMs = 400;
    inline constexpr int animationMs = 80;
}

//==============================================================================
class Fonts
{
public:
    /** Body and label font: Lato, shipped in Resources/Fonts (visual-polish.md
        6.2), falling back through the system list if it cannot be loaded. */
    static juce::Font ui (float height, bool semiBold = false);

    /** Headings and plates: Bebas Neue, the condensed vintage display face. */
    static juce::Font display (float height);

    /** Tabular numeric readouts. */
    static juce::Font mono (float height);

    /** Small uppercase label with the letter spacing theme.md asks for. */
    static juce::Font label();

    /** Section header. */
    static juce::Font sectionHeader();

    /** Draws text with the extra tracking that the label style specifies, since
        JUCE has no letter-spacing attribute. */
    static void drawTrackedText (juce::Graphics& g, const juce::String& text,
                                 juce::Rectangle<int> area, juce::Justification justification,
                                 float tracking = 0.08f);

private:
    static juce::String findAvailable (const juce::StringArray& candidates,
                                       const juce::String& fallback);
};

//==============================================================================
class LuthierLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LuthierLookAndFeel();
    ~LuthierLookAndFeel() override;

    /** Re-reads the Palette into the LookAndFeel's colour ids (after apply()). */
    void refreshColours();

    //==========================================================================
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText,
                                           juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;

    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override;

    //==========================================================================
    /** Shared drawing helpers used by the custom components. */

    /** A panel with the standard corner radius, edge stroke and drop shadow. */
    static void drawPanel (juce::Graphics&, juce::Rectangle<float> bounds,
                           bool raised = false, float corner = Metrics::panelCorner);

    /** A section header: an engraved brass plate carrying the name
        (visual-polish.md 6.2); a plain accent bar under High contrast. */
    static void drawSectionHeader (juce::Graphics&, juce::Rectangle<int> bounds,
                                   const juce::String& text,
                                   juce::Colour accent = Palette::accent);

    /** The 1 px horizontal rule that separates sections. */
    static void drawSeparator (juce::Graphics&, juce::Rectangle<int> bounds);

    /** The brand mark (visual-polish.md 6.4): a small inlaid brass headstock
        outline in the top-left corner, where theme.md's notch was. */
    static void drawSignatureNotch (juce::Graphics&, juce::Rectangle<int> windowBounds,
                                    juce::Colour accent = Palette::accent);

    /** Meter gradient: patina -> amber -> yellow -> red. */
    static juce::Colour meterColourFor (float normalisedLevel);

    /** Whether this control should currently show its value instead of its label. */
    static bool shouldShowValue (const juce::Component&);

    /** Four small screw heads in a panel's corners, for the larger frames. */
    static void drawCornerScrews (juce::Graphics&, juce::Rectangle<float> bounds);

    /** A mini toggle switch: a threaded bushing and a bat lever up (on) or down. */
    static void drawMiniToggle (juce::Graphics&, juce::Rectangle<float> area, bool on, bool enabled);

private:
    void drawKnurledSkirt (juce::Graphics&, juce::Point<float> centre, float radius,
                           float angle, juce::Colour colour) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierLookAndFeel)
};

} // namespace luthier
