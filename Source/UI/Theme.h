#pragma once

/*  Visual identity.

    theme.md defines a cool, near-black, burnt-orange identity shared across a
    family of plugins. This plugin keeps that spec's *structure* exactly - the 8 px
    grid, the 270-degree value arcs drawn outside the knob body, the section rules,
    the corner radii, the header layout, the signature notch, the output LED and
    the scrolling data stream - and re-tints it for a guitar, which the brief
    explicitly allows:

      - the neutrals move from blue-black to walnut-black, the warm end of the
        same darkness;
      - the accent becomes aged amber, the colour of a tube amp's pilot lamp,
        rather than burnt orange;
      - the secondary becomes oxidised-brass patina rather than teal;
      - text moves to aged ivory, the colour of old binding, rather than cool grey;
      - knobs gain a knurled skirt and a pointer, like an amp's control, instead of
        a plain disc.

    It is still flat: the depth comes from gradients and shadows, never from faux
    wood grain or fake metal.
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{

//==============================================================================
namespace Palette
{
    // --- neutrals (warm walnut, not blue-black) -------------------------------
    inline const juce::Colour backgroundDeep  { 0xff120f0c };
    inline const juce::Colour background      { 0xff191512 };
    inline const juce::Colour panel           { 0xff221c17 };
    inline const juce::Colour panelRaised     { 0xff2a221b };
    inline const juce::Colour panelSunken     { 0xff15110e };
    inline const juce::Colour edge            { 0xff3e3226 };
    inline const juce::Colour edgeBright      { 0xff554330 };

    // --- accents ---------------------------------------------------------------
    /** Aged amber: a tube amp's pilot lamp. */
    inline const juce::Colour accent          { 0xffe08a3c };
    inline const juce::Colour accentBright    { 0xfff5ac63 };
    inline const juce::Colour accentDim       { 0xff8a5426 };

    /** Oxidised brass, for secondary indicators and modulation. */
    inline const juce::Colour secondary       { 0xff6fa5a0 };
    inline const juce::Colour secondaryDim    { 0xff3f6663 };

    // --- text -------------------------------------------------------------------
    inline const juce::Colour textPrimary     { 0xffede4d6 };
    inline const juce::Colour textMuted       { 0xff9c9082 };
    inline const juce::Colour textDisabled    { 0xff655c51 };

    // --- status -----------------------------------------------------------------
    inline const juce::Colour success         { 0xff8fbf6f };
    inline const juce::Colour warning         { 0xfff0c24e };
    inline const juce::Colour clip            { 0xffd9452f };

    /** The scrolling internals readout. */
    inline const juce::Colour dataStream      { 0xff7fd18a };

    inline const juce::Colour shadow          { 0x99000000 };
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
    /** Body and label font. Falls back through the list until something resolves. */
    static juce::Font ui (float height, bool semiBold = false);

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

    /** A section header: uppercase text with the 2x12 px accent bar to its left. */
    static void drawSectionHeader (juce::Graphics&, juce::Rectangle<int> bounds,
                                   const juce::String& text,
                                   juce::Colour accent = Palette::accent);

    /** The 1 px horizontal rule that separates sections. */
    static void drawSeparator (juce::Graphics&, juce::Rectangle<int> bounds);

    /** The signature 12 px, 45-degree accent notch in the top-left corner. */
    static void drawSignatureNotch (juce::Graphics&, juce::Rectangle<int> windowBounds,
                                    juce::Colour accent = Palette::accent);

    /** Meter gradient: patina -> amber -> yellow -> red. */
    static juce::Colour meterColourFor (float normalisedLevel);

    /** Whether this control should currently show its value instead of its label. */
    static bool shouldShowValue (const juce::Component&);

private:
    void drawKnurledSkirt (juce::Graphics&, juce::Point<float> centre, float radius,
                           float angle, juce::Colour colour) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierLookAndFeel)
};

} // namespace luthier
