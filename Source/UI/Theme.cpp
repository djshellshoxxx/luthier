#include "Theme.h"

namespace luthier
{

//==============================================================================
//  Fonts
//==============================================================================
juce::String Fonts::findAvailable (const juce::StringArray& candidates, const juce::String& fallback)
{
    static juce::StringArray systemFonts;

    if (systemFonts.isEmpty())
        systemFonts = juce::Font::findAllTypefaceNames();

    for (const auto& name : candidates)
        if (systemFonts.contains (name))
            return name;

    return fallback;
}

juce::Font Fonts::ui (float height, bool semiBold)
{
    static const juce::String family = findAvailable (
        { "Inter", "Space Grotesk", "Segoe UI Variable Text", "Segoe UI",
          "SF Pro Text", "Helvetica Neue", "DejaVu Sans" },
        juce::Font::getDefaultSansSerifFontName());

    auto options = juce::FontOptions (family, height, juce::Font::plain);

    if (semiBold)
        options = options.withStyle ("Bold");

    return juce::Font (options);
}

juce::Font Fonts::mono (float height)
{
    static const juce::String family = findAvailable (
        { "JetBrains Mono", "IBM Plex Mono", "Cascadia Mono", "Consolas",
          "SF Mono", "Menlo", "DejaVu Sans Mono" },
        juce::Font::getDefaultMonospacedFontName());

    return juce::Font (juce::FontOptions (family, height, juce::Font::plain));
}

juce::Font Fonts::label()
{
    return ui (11.0f, true);
}

juce::Font Fonts::sectionHeader()
{
    return ui (12.0f, true);
}

void Fonts::drawTrackedText (juce::Graphics& g, const juce::String& text,
                             juce::Rectangle<int> area, juce::Justification justification,
                             float tracking)
{
    if (text.isEmpty())
        return;

    const auto font = g.getCurrentFont();
    const float extra = font.getHeight() * tracking;

    // Measure with the tracking included so the justification stays correct.
    float total = 0.0f;

    for (int i = 0; i < text.length(); ++i)
        total += font.getStringWidthFloat (text.substring (i, i + 1)) + extra;

    total -= extra;

    float x = (float) area.getX();

    if (justification.testFlags (juce::Justification::horizontallyCentred))
        x = (float) area.getCentreX() - total * 0.5f;
    else if (justification.testFlags (juce::Justification::right))
        x = (float) area.getRight() - total;

    const float baseline = (float) area.getCentreY() + font.getAscent() * 0.5f - font.getDescent() * 0.35f;

    for (int i = 0; i < text.length(); ++i)
    {
        const auto glyph = text.substring (i, i + 1);
        g.drawSingleLineText (glyph, juce::roundToInt (x), juce::roundToInt (baseline));
        x += font.getStringWidthFloat (glyph) + extra;
    }
}

//==============================================================================
//  LookAndFeel
//==============================================================================
LuthierLookAndFeel::LuthierLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);
    setColour (juce::DocumentWindow::textColourId,        Palette::textPrimary);

    setColour (juce::Label::textColourId,                 Palette::textPrimary);
    setColour (juce::Label::backgroundColourId,           juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId,              juce::Colours::transparentBlack);
    setColour (juce::Label::textWhenEditingColourId,      Palette::textPrimary);
    setColour (juce::Label::backgroundWhenEditingColourId, Palette::panelSunken);
    setColour (juce::Label::outlineWhenEditingColourId,   Palette::accent);

    setColour (juce::Slider::rotarySliderFillColourId,    Palette::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, Palette::edge);
    setColour (juce::Slider::thumbColourId,               Palette::accent);
    setColour (juce::Slider::trackColourId,               Palette::accent);
    setColour (juce::Slider::backgroundColourId,          Palette::edge);
    setColour (juce::Slider::textBoxTextColourId,         Palette::textPrimary);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,    Palette::accentDim);

    setColour (juce::TextButton::buttonColourId,          Palette::panel);
    setColour (juce::TextButton::buttonOnColourId,        Palette::accent.withAlpha (0.15f));
    setColour (juce::TextButton::textColourOffId,         Palette::textMuted);
    setColour (juce::TextButton::textColourOnId,          Palette::accent);

    setColour (juce::ToggleButton::textColourId,          Palette::textPrimary);
    setColour (juce::ToggleButton::tickColourId,          Palette::accent);
    setColour (juce::ToggleButton::tickDisabledColourId,  Palette::edge);

    setColour (juce::ComboBox::backgroundColourId,        Palette::panelSunken);
    setColour (juce::ComboBox::textColourId,              Palette::textPrimary);
    setColour (juce::ComboBox::outlineColourId,           Palette::edge);
    setColour (juce::ComboBox::arrowColourId,             Palette::textMuted);
    setColour (juce::ComboBox::buttonColourId,            juce::Colours::transparentBlack);
    setColour (juce::ComboBox::focusedOutlineColourId,    Palette::accent);

    setColour (juce::PopupMenu::backgroundColourId,       Palette::panelRaised);
    setColour (juce::PopupMenu::textColourId,             Palette::textPrimary);
    setColour (juce::PopupMenu::headerTextColourId,       Palette::textMuted);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accent.withAlpha (0.20f));
    setColour (juce::PopupMenu::highlightedTextColourId,  Palette::accentBright);

    setColour (juce::TooltipWindow::backgroundColourId,   Palette::panelRaised);
    setColour (juce::TooltipWindow::textColourId,         Palette::textMuted);
    setColour (juce::TooltipWindow::outlineColourId,      Palette::edge);

    setColour (juce::ScrollBar::thumbColourId,            Palette::edgeBright);
    setColour (juce::ScrollBar::trackColourId,            Palette::panelSunken);

    setColour (juce::TextEditor::backgroundColourId,      Palette::panelSunken);
    setColour (juce::TextEditor::textColourId,            Palette::textPrimary);
    setColour (juce::TextEditor::outlineColourId,         Palette::edge);
    setColour (juce::TextEditor::focusedOutlineColourId,  Palette::accent);
    setColour (juce::TextEditor::highlightColourId,       Palette::accentDim);
    setColour (juce::TextEditor::highlightedTextColourId, Palette::textPrimary);
    setColour (juce::CaretComponent::caretColourId,       Palette::accent);

    setColour (juce::TabbedComponent::backgroundColourId, Palette::background);
    setColour (juce::TabbedComponent::outlineColourId,    Palette::edge);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, Palette::edge);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, Palette::accent);
    setColour (juce::TabbedButtonBar::tabTextColourId,    Palette::textMuted);
    setColour (juce::TabbedButtonBar::frontTextColourId,  Palette::accent);

    setColour (juce::ListBox::backgroundColourId,         Palette::panelSunken);
    setColour (juce::ListBox::textColourId,               Palette::textPrimary);
    setColour (juce::ListBox::outlineColourId,            Palette::edge);
}

LuthierLookAndFeel::~LuthierLookAndFeel() = default;

//==============================================================================
bool LuthierLookAndFeel::shouldShowValue (const juce::Component& c)
{
    // theme.md: the value readout replaces the label on hover or during a drag.
    if (auto* slider = dynamic_cast<const juce::Slider*> (&c))
        return slider->isMouseOverOrDragging (true) || slider->isMouseButtonDown();

    return c.isMouseOverOrDragging (true);
}

juce::Colour LuthierLookAndFeel::meterColourFor (float level)
{
    level = juce::jlimit (0.0f, 1.0f, level);

    if (level < 0.55f)
        return Palette::secondary.interpolatedWith (Palette::accent, level / 0.55f);

    if (level < 0.82f)
        return Palette::accent.interpolatedWith (Palette::warning, (level - 0.55f) / 0.27f);

    return Palette::warning.interpolatedWith (Palette::clip, (level - 0.82f) / 0.18f);
}

//==============================================================================
void LuthierLookAndFeel::drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                                    bool raised, float corner)
{
    // Soft drop shadow: 8 px blur, y+2, as the spec asks.
    juce::DropShadow (Palette::shadow, 8, { 0, 2 })
        .drawForRectangle (g, bounds.toNearestInt());

    g.setColour (raised ? Palette::panelRaised : Palette::panel);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
}

void LuthierLookAndFeel::drawSectionHeader (juce::Graphics& g, juce::Rectangle<int> bounds,
                                            const juce::String& text, juce::Colour accent)
{
    auto area = bounds;

    // The 2 px wide, 12 px tall accent bar to the left of every section header.
    auto bar = area.removeFromLeft (2).withSizeKeepingCentre (2, 12);
    g.setColour (accent);
    g.fillRect (bar);

    area.removeFromLeft (Metrics::grid);

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::sectionHeader());
    Fonts::drawTrackedText (g, text.toUpperCase(), area, juce::Justification::centredLeft);
}

void LuthierLookAndFeel::drawSeparator (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour (Palette::edge);
    g.fillRect (bounds.withHeight (1).withY (bounds.getCentreY()));
}

void LuthierLookAndFeel::drawSignatureNotch (juce::Graphics& g, juce::Rectangle<int> windowBounds,
                                             juce::Colour accent)
{
    const float length = 12.0f;
    const float inset = 6.0f;

    g.setColour (accent);
    g.drawLine ((float) windowBounds.getX() + inset,
                (float) windowBounds.getY() + inset + length,
                (float) windowBounds.getX() + inset + length,
                (float) windowBounds.getY() + inset,
                2.0f);
}

//==============================================================================
void LuthierLookAndFeel::drawKnurledSkirt (juce::Graphics& g, juce::Point<float> centre,
                                           float radius, float angle, juce::Colour colour) const
{
    // The fine vertical grooves around an amp knob. Subtle: they read as texture
    // at a glance rather than as decoration, and they rotate with the control so
    // the knob's position is legible even without looking at the pointer.
    const int teeth = 30;

    g.setColour (colour);

    for (int i = 0; i < teeth; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) teeth;

        const auto inner = centre.getPointOnCircumference (radius * 0.86f, a);
        const auto outer = centre.getPointOnCircumference (radius * 0.99f, a);

        g.drawLine ({ inner, outer }, 1.0f);
    }
}

void LuthierLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();

    // Leave room for the value arc, which is drawn OUTSIDE the knob body.
    const float arcSpace = Metrics::arcThickness + Metrics::arcGap;
    const float radius = juce::jmax (6.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - arcSpace);

    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const bool enabled = slider.isEnabled();
    const bool hover = slider.isMouseOverOrDragging (true);

    auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    if (! enabled)
        accent = Palette::textDisabled;

    // ---- value arc -------------------------------------------------------------
    const float arcRadius = radius + Metrics::arcGap + Metrics::arcThickness * 0.5f;

    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);

    g.setColour (Palette::edge.withAlpha (0.6f));
    g.strokePath (backgroundArc, juce::PathStrokeType (Metrics::arcThickness,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        juce::Path valueArc;

        // Bipolar controls fill outward from the centre, which is the only way a
        // pan or a bend control reads correctly.
        const bool bipolar = slider.getMinimum() < -0.0001 && slider.getMaximum() > 0.0001
                             && std::abs (slider.getMinimum() + slider.getMaximum()) < 0.0001;

        const float from = bipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f : rotaryStartAngle;

        valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                juce::jmin (from, angle), juce::jmax (from, angle), true);

        g.setColour (hover ? accent.brighter (0.12f) : accent);
        g.strokePath (valueArc, juce::PathStrokeType (Metrics::arcThickness,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // ---- body ------------------------------------------------------------------
    juce::DropShadow (Palette::shadow, 6, { 0, 2 })
        .drawForPath (g, [centre, radius]
        {
            juce::Path p;
            p.addEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
            return p;
        }());

    const auto top = hover ? Palette::panelRaised.brighter (0.08f) : Palette::panelRaised;
    const auto bottom = Palette::panelSunken;

    juce::ColourGradient gradient (top, centre.x, centre.y - radius,
                                   bottom, centre.x, centre.y + radius, false);
    g.setGradientFill (gradient);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    // Knurled skirt, rotating with the knob.
    drawKnurledSkirt (g, centre, radius, angle, Palette::edge.withAlpha (enabled ? 0.55f : 0.25f));

    g.setColour (Palette::edge);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    // ---- pointer ----------------------------------------------------------------
    const auto pointerOuter = centre.getPointOnCircumference (radius * 0.80f, angle);
    const auto pointerInner = centre.getPointOnCircumference (radius * 0.18f, angle);

    g.setColour (accent);
    g.drawLine ({ pointerInner, pointerOuter }, 2.0f);

    // ---- centre dot --------------------------------------------------------------
    const bool atDefault = std::abs (slider.getValue() - slider.getDoubleClickReturnValue()) < 1.0e-6;
    const float dotRadius = 2.0f;

    g.setColour (atDefault ? Palette::textDisabled : accent);
    g.fillEllipse (centre.x - dotRadius, centre.y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
}

//==============================================================================
juce::Slider::SliderLayout LuthierLookAndFeel::getSliderLayout (juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;

    if (slider.isRotary())
    {
        layout.sliderBounds = slider.getLocalBounds();
        layout.textBoxBounds = {};
        return layout;
    }

    return LookAndFeel_V4::getSliderLayout (slider);
}

void LuthierLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float minSliderPos, float maxSliderPos,
                                           juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos);

    const bool vertical = slider.isVertical();
    const bool enabled = slider.isEnabled();

    auto accent = slider.findColour (juce::Slider::trackColourId);

    if (! enabled)
        accent = Palette::textDisabled;

    // ---- track (4 px, rounded ends) ---------------------------------------------
    const float trackThickness = 4.0f;

    juce::Rectangle<float> track;

    if (vertical)
        track = { (float) x + width * 0.5f - trackThickness * 0.5f, (float) y,
                  trackThickness, (float) height };
    else
        track = { (float) x, (float) y + height * 0.5f - trackThickness * 0.5f,
                  (float) width, trackThickness };

    g.setColour (Palette::edge);
    g.fillRoundedRectangle (track, trackThickness * 0.5f);

    // ---- fill ---------------------------------------------------------------------
    juce::Rectangle<float> fill = track;

    if (vertical)
        fill = fill.withTop (sliderPos);
    else
        fill = fill.withRight (sliderPos);

    g.setColour (accent);
    g.fillRoundedRectangle (fill, trackThickness * 0.5f);

    // ---- thumb (16 x 24 rounded rect) ----------------------------------------------
    if (style != juce::Slider::LinearBarVertical && style != juce::Slider::LinearBar)
    {
        const float thumbW = vertical ? 24.0f : 16.0f;
        const float thumbH = vertical ? 16.0f : 24.0f;

        juce::Rectangle<float> thumb (thumbW, thumbH);

        if (vertical)
            thumb.setCentre ((float) x + width * 0.5f, sliderPos);
        else
            thumb.setCentre (sliderPos, (float) y + height * 0.5f);

        juce::DropShadow (Palette::shadow, 5, { 0, 2 }).drawForRectangle (g, thumb.toNearestInt());

        juce::ColourGradient gradient (Palette::panelRaised, thumb.getX(), thumb.getY(),
                                        Palette::panelSunken, thumb.getX(), thumb.getBottom(), false);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (thumb, Metrics::controlCorner);

        g.setColour (accent);
        g.drawRoundedRectangle (thumb.reduced (0.5f), Metrics::controlCorner, 1.0f);
    }
}

//==============================================================================
void LuthierLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool isHighlighted, bool isDown)
{
    juce::ignoreUnused (backgroundColour);

    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    auto accent = button.findColour (juce::TextButton::textColourOnId);

    if (on)
    {
        // On state: accent fill at 15 % with an accent border and accent text.
        g.setColour (accent.withAlpha (0.15f));
        g.fillRoundedRectangle (bounds, Metrics::panelCorner);

        g.setColour (accent);
        g.drawRoundedRectangle (bounds, Metrics::panelCorner, 1.0f);
    }
    else
    {
        g.setColour (isHighlighted ? Palette::panelRaised.brighter (0.08f) : Palette::panel);
        g.fillRoundedRectangle (bounds, Metrics::panelCorner);

        g.setColour (Palette::edge);
        g.drawRoundedRectangle (bounds, Metrics::panelCorner, 1.0f);
    }

    // Momentary presses flash the accent briefly.
    if (isDown)
    {
        g.setColour (accent.withAlpha (0.25f));
        g.fillRoundedRectangle (bounds, Metrics::panelCorner);
    }
}

void LuthierLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                         bool isHighlighted, bool isDown)
{
    juce::ignoreUnused (isDown);

    const bool on = button.getToggleState();

    auto colour = on ? button.findColour (juce::TextButton::textColourOnId)
                     : button.findColour (juce::TextButton::textColourOffId);

    if (! button.isEnabled())
        colour = Palette::textDisabled;
    else if (isHighlighted && ! on)
        colour = colour.brighter (0.25f);

    g.setColour (colour);
    g.setFont (getTextButtonFont (button, button.getHeight()));

    Fonts::drawTrackedText (g, button.getButtonText().toUpperCase(),
                            button.getLocalBounds(), juce::Justification::centred);
}

void LuthierLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                           bool isHighlighted, bool isDown)
{
    juce::ignoreUnused (isDown);

    auto bounds = button.getLocalBounds();
    const bool on = button.getToggleState();

    auto accent = button.findColour (juce::ToggleButton::tickColourId);

    // A small square indicator rather than a tick: it matches the flat control
    // language and stays legible at 11 px.
    auto box = bounds.removeFromLeft (bounds.getHeight()).withSizeKeepingCentre (14, 14).toFloat();

    g.setColour (on ? accent.withAlpha (0.18f) : Palette::panelSunken);
    g.fillRoundedRectangle (box, Metrics::controlCorner);

    g.setColour (on ? accent : (isHighlighted ? Palette::edgeBright : Palette::edge));
    g.drawRoundedRectangle (box.reduced (0.5f), Metrics::controlCorner, 1.0f);

    if (on)
    {
        g.setColour (accent);
        g.fillRoundedRectangle (box.reduced (4.0f), 1.0f);
    }

    bounds.removeFromLeft (Metrics::gridHalf);

    g.setColour (button.isEnabled() ? (on ? Palette::textPrimary : Palette::textMuted)
                                    : Palette::textDisabled);
    g.setFont (Fonts::ui (12.0f));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centredLeft, true);
}

//==============================================================================
void LuthierLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                       int buttonX, int buttonY, int buttonW, int buttonH,
                                       juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH);

    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, Metrics::controlCorner);

    const bool hover = box.isMouseOver (true);

    g.setColour (hover ? Palette::edgeBright : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, Metrics::controlCorner, 1.0f);

    // Chevron.
    const float cx = (float) width - 14.0f;
    const float cy = (float) height * 0.5f;

    juce::Path arrow;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);

    g.setColour (box.isEnabled() ? box.findColour (juce::ComboBox::arrowColourId)
                                 : Palette::textDisabled);
    g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void LuthierLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (Metrics::grid, 0, box.getWidth() - Metrics::grid - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font LuthierLookAndFeel::getComboBoxFont (juce::ComboBox&)  { return Fonts::ui (12.0f); }
juce::Font LuthierLookAndFeel::getLabelFont (juce::Label&)        { return Fonts::ui (12.0f); }
juce::Font LuthierLookAndFeel::getPopupMenuFont()                 { return Fonts::ui (13.0f); }

juce::Font LuthierLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return Fonts::ui (juce::jlimit (9.0f, 13.0f, (float) buttonHeight * 0.42f), true);
}

//==============================================================================
void LuthierLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    g.setColour (Palette::panelRaised);
    g.fillRoundedRectangle (bounds, Metrics::panelCorner);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), Metrics::panelCorner, 1.0f);
}

void LuthierLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                            bool isSeparator, bool isActive, bool isHighlighted,
                                            bool isTicked, bool hasSubMenu,
                                            const juce::String& text, const juce::String& shortcutKeyText,
                                            const juce::Drawable* icon, const juce::Colour* textColour)
{
    juce::ignoreUnused (icon, textColour);

    if (isSeparator)
    {
        auto line = area.reduced (Metrics::grid, 0).withHeight (1).withY (area.getCentreY());
        g.setColour (Palette::edge);
        g.fillRect (line);
        return;
    }

    auto bounds = area.reduced (2, 1);

    if (isHighlighted && isActive)
    {
        g.setColour (Palette::accent.withAlpha (0.20f));
        g.fillRoundedRectangle (bounds.toFloat(), Metrics::controlCorner);
    }

    auto textArea = bounds.reduced (Metrics::grid, 0);

    if (isTicked)
    {
        auto tick = textArea.removeFromLeft (14);
        g.setColour (Palette::accent);
        g.fillRoundedRectangle (tick.withSizeKeepingCentre (6, 6).toFloat(), 1.0f);
    }
    else
    {
        textArea.removeFromLeft (14);
    }

    textArea.removeFromLeft (Metrics::gridHalf);

    g.setColour (! isActive ? Palette::textDisabled
                            : (isHighlighted ? Palette::accentBright : Palette::textPrimary));
    g.setFont (getPopupMenuFont());
    g.drawText (text, textArea, juce::Justification::centredLeft, true);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::mono (11.0f));
        g.drawText (shortcutKeyText, textArea, juce::Justification::centredRight, true);
    }

    if (hasSubMenu)
    {
        const float cx = (float) textArea.getRight() - 6.0f;
        const float cy = (float) textArea.getCentreY();

        juce::Path arrow;
        arrow.startNewSubPath (cx - 3.0f, cy - 4.0f);
        arrow.lineTo (cx + 1.5f, cy);
        arrow.lineTo (cx - 3.0f, cy + 4.0f);

        g.setColour (Palette::textMuted);
        g.strokePath (arrow, juce::PathStrokeType (1.3f));
    }
}

//==============================================================================
void LuthierLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    // A dark pill, as specified.
    const float corner = juce::jmin (bounds.getHeight() * 0.5f, 10.0f);

    juce::DropShadow (Palette::shadow, 8, { 0, 2 }).drawForRectangle (g, bounds.toNearestInt());

    g.setColour (Palette::panelRaised);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (12.0f));

    juce::AttributedString s;
    s.append (text, Fonts::ui (12.0f), Palette::textMuted);
    s.setJustification (juce::Justification::centred);
    s.setWordWrap (juce::AttributedString::byWord);
    s.draw (g, bounds.reduced (10.0f, 6.0f));
}

juce::Rectangle<int> LuthierLookAndFeel::getTooltipBounds (const juce::String& tipText,
                                                           juce::Point<int> screenPos,
                                                           juce::Rectangle<int> parentArea)
{
    juce::AttributedString s;
    s.setJustification (juce::Justification::centred);
    s.append (tipText, Fonts::ui (12.0f));

    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, 360.0f);

    const int w = (int) (layout.getWidth() + 20.0f);
    const int h = (int) (layout.getHeight() + 12.0f);

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 12,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
                                 w, h)
             .constrainedWithin (parentArea);
}

//==============================================================================
void LuthierLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y,
                                        int width, int height, bool isVertical,
                                        int thumbStart, int thumbSize,
                                        bool isMouseOver, bool isMouseDown)
{
    juce::Rectangle<int> track (x, y, width, height);

    g.setColour (Palette::panelSunken);
    g.fillRect (track);

    if (thumbSize <= 0)
        return;

    juce::Rectangle<int> thumb = isVertical
        ? juce::Rectangle<int> (x + 2, thumbStart, width - 4, thumbSize)
        : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, height - 4);

    g.setColour (isMouseDown ? Palette::accent
                             : (isMouseOver ? Palette::edgeBright.brighter (0.2f) : Palette::edgeBright));
    g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
}

//==============================================================================
void LuthierLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                        bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused (isMouseDown);

    auto bounds = button.getLocalBounds();
    const bool active = button.getToggleState();

    if (active)
    {
        g.setColour (Palette::panel);
        g.fillRect (bounds);

        // An accent underline marks the active tab.
        g.setColour (Palette::accent);
        g.fillRect (bounds.removeFromBottom (2));
    }
    else if (isMouseOver)
    {
        g.setColour (Palette::panel.withAlpha (0.5f));
        g.fillRect (bounds);
    }

    g.setColour (active ? Palette::accent : Palette::textMuted);
    g.setFont (Fonts::ui (11.0f, active));

    Fonts::drawTrackedText (g, button.getButtonText().toUpperCase(),
                            button.getLocalBounds(), juce::Justification::centred);
}

void LuthierLookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar, juce::Graphics& g)
{
    g.setColour (Palette::background);
    g.fillRect (bar.getLocalBounds());

    g.setColour (Palette::edge);
    g.fillRect (bar.getLocalBounds().removeFromBottom (1));
}

} // namespace luthier
