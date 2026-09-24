#include "Theme.h"
#include "RangesUi.h"
#include "../Accessibility/Accessibility.h"
#include "../Support/IrLibrary.h"

namespace luthier
{

//==============================================================================
//  Palette
//==============================================================================
namespace
{
    PaletteColours& appliedPalette()
    {
        static PaletteColours applied;
        return applied;
    }

    /** Every role, as pointers into a PaletteColours, in a fixed order. */
    std::vector<const juce::Colour*> rolesOf (const PaletteColours& p)
    {
        return { &p.backgroundDeep, &p.background, &p.panel, &p.panelRaised, &p.panelSunken,
                 &p.edge, &p.edgeBright, &p.accent, &p.accentBright, &p.accentDim,
                 &p.secondary, &p.secondaryDim, &p.textPrimary, &p.textMuted, &p.textDisabled,
                 &p.success, &p.warning, &p.clip, &p.dataStream };
    }
}

void Palette::apply (const PaletteColours& c, bool texturedSurfaces)
{
    backgroundDeep = c.backgroundDeep;
    background     = c.background;
    panel          = c.panel;
    panelRaised    = c.panelRaised;
    panelSunken    = c.panelSunken;
    edge           = c.edge;
    edgeBright     = c.edgeBright;
    accent         = c.accent;
    accentBright   = c.accentBright;
    accentDim      = c.accentDim;
    secondary      = c.secondary;
    secondaryDim   = c.secondaryDim;
    textPrimary    = c.textPrimary;
    textMuted      = c.textMuted;
    textDisabled   = c.textDisabled;
    success        = c.success;
    warning        = c.warning;
    clip           = c.clip;
    dataStream     = c.dataStream;
    shadow         = c.shadow;

    textured = texturedSurfaces;

    // Black bell knobs with cream pointers on every palette but High contrast,
    // where the knob is black and the pointer is the text colour.
    knobBody    = juce::Colour (0xff151312);
    knobPointer = texturedSurfaces ? juce::Colour (0xffefe3cc) : c.textPrimary;

    // Engraved plates are brass with dark lettering; High contrast uses the accent.
    plate     = texturedSurfaces ? juce::Colour (0xffc9a25a) : c.accent;
    plateText = juce::Colour (0xff2a1a0c);

    appliedPalette() = c;
}

const PaletteColours& Palette::current()
{
    return appliedPalette();
}

void Palette::remap (juce::Component& root, const PaletteColours& from, const PaletteColours& to)
{
    const auto oldRoles = rolesOf (from);
    const auto newRoles = rolesOf (to);

    std::function<void (juce::Component&)> visit = [&] (juce::Component& c)
    {
        // Component colours live in properties named "jcclr_<id in hex>".
        juce::Array<std::pair<int, juce::Colour>> changes;
        const auto& props = c.getProperties();

        for (int i = 0; i < props.size(); ++i)
        {
            const auto name = props.getName (i).toString();

            if (! name.startsWith ("jcclr_"))
                continue;

            const juce::Colour colour ((juce::uint32) (int) props.getValueAt (i));

            for (size_t r = 0; r < oldRoles.size(); ++r)
            {
                // A role used as is, or with its own alpha (withAlpha).
                if ((oldRoles[r]->getARGB() & 0x00ffffffu) == (colour.getARGB() & 0x00ffffffu))
                {
                    changes.add ({ name.substring (6).getHexValue32(), newRoles[r]->withAlpha (colour.getAlpha()) });
                    break;
                }
            }
        }

        for (auto& change : changes)
            c.setColour (change.first, change.second);

        for (auto* child : c.getChildren())
            visit (*child);
    };

    visit (root);
}

//==============================================================================
namespace
{
    /** The typefaces shipped in Resources/Fonts (visual-polish.md 6.2), loaded once. */
    struct BundledFonts
    {
        juce::Typeface::Ptr regular, bold, display;

        BundledFonts()
        {
            const auto dir = IrLibrary::getResourcesFolder().getChildFile ("Fonts");

            auto load = [&dir] (const char* name) -> juce::Typeface::Ptr
            {
                juce::MemoryBlock data;

                if (dir.getChildFile (name).loadFileAsData (data) && data.getSize() > 1024)
                    return juce::Typeface::createSystemTypefaceFor (data.getData(), data.getSize());

                return nullptr;
            };

            regular = load ("Lato-Regular.ttf");
            bold    = load ("Lato-Bold.ttf");
            display = load ("BebasNeue-Regular.ttf");
        }
    };

    const BundledFonts& bundledFonts()
    {
        static const BundledFonts fonts;
        return fonts;
    }
}

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
    const auto& bundled = bundledFonts();

    if (auto typeface = semiBold ? bundled.bold : bundled.regular)
        return juce::Font (juce::FontOptions (typeface).withHeight (height));

    static const juce::String family = findAvailable (
        { "Inter", "Space Grotesk", "Segoe UI Variable Text", "Segoe UI",
          "SF Pro Text", "Helvetica Neue", "DejaVu Sans" },
        juce::Font::getDefaultSansSerifFontName());

    auto options = juce::FontOptions (family, height, juce::Font::plain);

    if (semiBold)
        options = options.withStyle ("Bold");

    return juce::Font (options);
}

juce::Font Fonts::display (float height)
{
    if (auto typeface = bundledFonts().display)
        return juce::Font (juce::FontOptions (typeface).withHeight (height));

    return ui (height, true);
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
    return display (14.0f);
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
    // The first window opens in the palette the user chose (accessibility.md 6);
    // later changes arrive through the editor's listener, not through here.
    static bool applied = false;

    if (! applied)
    {
        auto& settings = AccessibilitySettings::get();
        Palette::apply (settings.getColours(), settings.getPalette() != PaletteId::highContrast);
        applied = true;
    }

    refreshColours();
}

void LuthierLookAndFeel::refreshColours()
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

    if (Palette::textured && ! raised)
    {
        // Walnut: a faint grain, deterministic so repaints do not shimmer.
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (bounds.toNearestInt());
        g.setColour (juce::Colours::black.withAlpha (0.10f));

        float k = 0.0f;

        for (float y = bounds.getY() + 3.0f; y < bounds.getBottom(); y += 5.0f + std::fmod (k * 7.3f, 6.0f))
        {
            juce::Path grain;
            grain.startNewSubPath (bounds.getX(), y);
            grain.cubicTo (bounds.getX() + bounds.getWidth() * 0.3f, y + std::sin (k) * 1.5f,
                           bounds.getX() + bounds.getWidth() * 0.7f, y - std::cos (k) * 1.5f,
                           bounds.getRight(), y + std::sin (k * 0.7f));
            g.strokePath (grain, juce::PathStrokeType (0.8f));
            k += 1.0f;
        }
    }

    // A raised frame, like a cabinet or a pedalboard: light top edge, dark bottom edge.
    g.setColour (Palette::edge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

    if (Palette::textured)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine (bounds.getX() + corner, bounds.getY() + 1.5f, bounds.getRight() - corner, bounds.getY() + 1.5f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawLine (bounds.getX() + corner, bounds.getBottom() - 1.5f, bounds.getRight() - corner, bounds.getBottom() - 1.5f, 1.0f);

        if (bounds.getWidth() >= 220.0f && bounds.getHeight() >= 140.0f)
            drawCornerScrews (g, bounds);
    }
}

void LuthierLookAndFeel::drawCornerScrews (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (! Palette::textured)
        return;

    const float inset = 7.0f, r = 2.6f;

    for (auto c : { juce::Point<float> (bounds.getX() + inset, bounds.getY() + inset),
                    juce::Point<float> (bounds.getRight() - inset, bounds.getY() + inset),
                    juce::Point<float> (bounds.getX() + inset, bounds.getBottom() - inset),
                    juce::Point<float> (bounds.getRight() - inset, bounds.getBottom() - inset) })
    {
        juce::ColourGradient head (juce::Colour (0xffd8d2c4), c.x - r, c.y - r, juce::Colour (0xff6e6860), c.x + r, c.y + r, false);
        g.setGradientFill (head);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawLine (c.x - r * 0.7f, c.y + r * 0.7f, c.x + r * 0.7f, c.y - r * 0.7f, 0.8f);
    }
}

void LuthierLookAndFeel::drawMiniToggle (juce::Graphics& g, juce::Rectangle<float> area, bool on, bool enabled)
{
    const auto c = area.getCentre();
    const float bushing = juce::jmin (area.getWidth(), area.getHeight()) * 0.34f;

    // The threaded bushing and its nut.
    if (Palette::textured)
    {
        juce::ColourGradient nut (juce::Colour (0xffe2ddd2), c.x - bushing, c.y - bushing,
                                  juce::Colour (0xff5d5850), c.x + bushing, c.y + bushing, false);
        g.setGradientFill (nut);
    }
    else
    {
        g.setColour (Palette::edge);
    }

    g.fillEllipse (c.x - bushing, c.y - bushing, bushing * 2.0f, bushing * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - bushing, c.y - bushing, bushing * 2.0f, bushing * 2.0f, 0.8f);

    // The bat lever: up for on, down for off, with its rounded tip.
    const float length = area.getHeight() * 0.48f;
    const auto tip = c.translated (0.0f, on ? -length : length);
    const auto leverColour = enabled ? (Palette::textured ? juce::Colour (0xffd9d4c8) : Palette::textPrimary)
                                     : Palette::textDisabled;

    g.setColour (leverColour.darker (0.3f));
    g.drawLine ({ c, tip }, 3.2f);
    g.setColour (leverColour);
    g.drawLine ({ c, tip }, 2.0f);
    g.fillEllipse (tip.x - 2.6f, tip.y - 2.6f, 5.2f, 5.2f);

    // On also lights the accent at the bushing, so the state reads without the lever.
    if (on && enabled)
    {
        g.setColour (Palette::accent);
        g.fillEllipse (c.x - 1.8f, c.y - 1.8f, 3.6f, 3.6f);
    }
}

void LuthierLookAndFeel::drawSectionHeader (juce::Graphics& g, juce::Rectangle<int> bounds,
                                            const juce::String& text, juce::Colour accent)
{
    auto area = bounds;
    const auto font = Fonts::sectionHeader();
    const auto label = text.toUpperCase();

    if (! Palette::textured)
    {
        // High contrast: theme.md's plain accent bar, no plate.
        auto bar = area.removeFromLeft (2).withSizeKeepingCentre (2, 12);
        g.setColour (accent);
        g.fillRect (bar);
        area.removeFromLeft (Metrics::grid);
        g.setColour (Palette::textPrimary);
        g.setFont (font);
        Fonts::drawTrackedText (g, label, area, juce::Justification::centredLeft);
        return;
    }

    // An engraved brass plate sized to its text (visual-polish.md 6.2).
    g.setFont (font);
    float textWidth = 0.0f;
    for (int i = 0; i < label.length(); ++i)
        textWidth += font.getStringWidthFloat (label.substring (i, i + 1)) + font.getHeight() * 0.06f;

    const float plateW = juce::jmin ((float) area.getWidth(), textWidth + 16.0f);
    const float plateH = juce::jmin ((float) area.getHeight(), font.getHeight() + 6.0f);
    const auto plateArea = juce::Rectangle<float> ((float) area.getX(), (float) area.getCentreY() - plateH * 0.5f, plateW, plateH);

    juce::ColourGradient brass (Palette::plate.brighter (0.3f), plateArea.getX(), plateArea.getY(),
                                Palette::plate.darker (0.25f), plateArea.getX(), plateArea.getBottom(), false);
    g.setGradientFill (brass);
    g.fillRoundedRectangle (plateArea, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (plateArea.reduced (0.5f), 2.0f, 1.0f);

    // Engraved: a light line under the dark lettering.
    const auto textArea = plateArea.reduced (8.0f, 0.0f).toNearestInt();
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    Fonts::drawTrackedText (g, label, textArea.translated (0, 1), juce::Justification::centredLeft, 0.06f);
    g.setColour (Palette::plateText);
    Fonts::drawTrackedText (g, label, textArea, juce::Justification::centredLeft, 0.06f);
    juce::ignoreUnused (accent);
}

void LuthierLookAndFeel::drawSeparator (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour (Palette::edge);
    g.fillRect (bounds.withHeight (1).withY (bounds.getCentreY()));
}

void LuthierLookAndFeel::drawSignatureNotch (juce::Graphics& g, juce::Rectangle<int> windowBounds,
                                             juce::Colour accent)
{
    // A small headstock outline inlaid in brass (visual-polish.md 6.4): an
    // open-book 3+3 head with its six tuner posts, 12 x 18 px.
    const float x = (float) windowBounds.getX() + 6.0f, y = (float) windowBounds.getY() + 5.0f;
    const float w = 12.0f, h = 18.0f;

    juce::Path head;
    head.startNewSubPath (x + w * 0.3f, y + h);
    head.lineTo (x + w * 0.05f, y + h * 0.55f);
    head.quadraticTo (x - w * 0.05f, y + h * 0.15f, x + w * 0.2f, y + h * 0.05f);
    head.quadraticTo (x + w * 0.5f, y + h * 0.18f, x + w * 0.8f, y + h * 0.05f);
    head.quadraticTo (x + w * 1.05f, y + h * 0.15f, x + w * 0.95f, y + h * 0.55f);
    head.lineTo (x + w * 0.7f, y + h);
    head.closeSubPath();

    const auto brass = Palette::textured ? Palette::plate : accent;
    g.setColour (brass);
    g.strokePath (head, juce::PathStrokeType (1.4f));

    for (int i = 0; i < 3; ++i)
    {
        const float py = y + h * (0.3f + 0.2f * (float) i);
        g.fillEllipse (x + w * 0.2f - 1.0f, py - 1.0f, 2.0f, 2.0f);
        g.fillEllipse (x + w * 0.8f - 1.0f, py - 1.0f, 2.0f, 2.0f);
    }
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

        /*  advanced-ranges.md 6.1: the part of the value arc beyond stock is
            drawn in the warning colour. The slider carries its stock pair as
            properties (RangesUi::tagSlider); a non-physical slider has none and
            draws nothing here. Marking follows the value, so an unlocked
            control sitting inside stock shows no warning at all. */
        const auto& properties = slider.getProperties();

        if (enabled && properties.contains (RangesUi::kStockMaxProperty) && RangesUi::markInWarningColour())
        {
            const double stockMin = properties[RangesUi::kStockMinProperty];
            const double stockMax = properties[RangesUi::kStockMaxProperty];
            const double value = slider.getValue();

            auto angleOf = [&] (double plain)
            {
                const double clamped = juce::jlimit (slider.getMinimum(), slider.getMaximum(), plain);
                return rotaryStartAngle + (float) slider.valueToProportionOfLength (clamped)
                                            * (rotaryEndAngle - rotaryStartAngle);
            };

            auto strokeWarning = [&] (float a0, float a1)
            {
                juce::Path warningArc;
                warningArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                          juce::jmin (a0, a1), juce::jmax (a0, a1), true);

                g.setColour (Palette::warning);
                g.strokePath (warningArc, juce::PathStrokeType (Metrics::arcThickness,
                                                                juce::PathStrokeType::curved,
                                                                juce::PathStrokeType::butt));
            };

            // The filled arc intersected with the two out-of-stock bands, so a
            // bipolar control filling from the centre is marked correctly too.
            const float filledLo = juce::jmin (from, angle);
            const float filledHi = juce::jmax (from, angle);

            if (value > stockMax)
            {
                const float lo = juce::jmax (filledLo, angleOf (stockMax));

                if (filledHi > lo)
                    strokeWarning (lo, filledHi);
            }
            else if (value < stockMin)
            {
                const float hi = juce::jmin (filledHi, angleOf (stockMin));

                if (hi > filledLo)
                    strokeWarning (filledLo, hi);
            }
        }
    }

    // ---- body: a black bell amp knob (visual-polish.md 6.3) ------------------------
    juce::Path bodyPath;
    bodyPath.addEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    juce::DropShadow (Palette::shadow, 6, { 0, 2 }).drawForPath (g, bodyPath);

    const auto body = hover ? Palette::knobBody.brighter (0.12f) : Palette::knobBody;

    // The skirt: the full circle, knurled, rotating with the knob.
    g.setColour (body.brighter (0.08f));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    drawKnurledSkirt (g, centre, radius, angle, juce::Colours::black.withAlpha (enabled ? 0.7f : 0.35f));

    // The bell cap, lit from the top left.
    const float cap = radius * 0.74f;

    if (Palette::textured)
    {
        juce::ColourGradient gradient (body.brighter (0.55f), centre.x - cap * 0.6f, centre.y - cap * 0.7f,
                                       body, centre.x + cap * 0.3f, centre.y + cap * 0.5f, true);
        g.setGradientFill (gradient);
    }
    else
    {
        g.setColour (body);
    }

    g.fillEllipse (centre.x - cap, centre.y - cap, cap * 2.0f, cap * 2.0f);

    if (Palette::textured)
    {
        // A soft highlight on the dome.
        g.setColour (juce::Colours::white.withAlpha (hover ? 0.16f : 0.1f));
        g.fillEllipse (centre.x - cap * 0.62f, centre.y - cap * 0.72f, cap * 0.8f, cap * 0.5f);
    }

    g.setColour (Palette::textured ? juce::Colours::black.withAlpha (0.8f) : Palette::edge);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    // ---- pointer: a cream line across cap and skirt ---------------------------------
    const auto pointerOuter = centre.getPointOnCircumference (radius * 0.96f, angle);
    const auto pointerInner = centre.getPointOnCircumference (radius * 0.22f, angle);

    g.setColour (enabled ? Palette::knobPointer : Palette::textDisabled);
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

        // A brass fader cap with a grip line across it (visual-polish.md 6.3).
        const auto brass = Palette::textured ? Palette::plate : Palette::panelRaised;

        if (Palette::textured)
        {
            juce::ColourGradient gradient (brass.brighter (0.35f), thumb.getX(), thumb.getY(),
                                           brass.darker (0.35f), thumb.getX(), thumb.getBottom(), false);
            g.setGradientFill (gradient);
        }
        else
        {
            g.setColour (brass);
        }

        g.fillRoundedRectangle (thumb, Metrics::controlCorner);

        g.setColour (Palette::textured ? juce::Colours::black.withAlpha (0.55f) : accent);
        g.drawRoundedRectangle (thumb.reduced (0.5f), Metrics::controlCorner, 1.0f);

        g.setColour (Palette::textured ? Palette::plateText : accent);
        if (vertical)
            g.fillRect (thumb.withSizeKeepingCentre (thumb.getWidth() - 6.0f, 1.5f));
        else
            g.fillRect (thumb.withSizeKeepingCentre (1.5f, thumb.getHeight() - 6.0f));
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

    // A mini toggle switch, the kind on a guitar or an amp (visual-polish.md 6.3).
    juce::ignoreUnused (accent, isHighlighted);
    auto box = bounds.removeFromLeft (juce::jmax (14, bounds.getHeight())).withSizeKeepingCentre (14, 20).toFloat();
    drawMiniToggle (g, box, on, button.isEnabled());

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
    drawChevron (g, { (float) width - 14.0f, (float) height * 0.5f }, 4.0f, 2,
                 box.isEnabled() ? box.findColour (juce::ComboBox::arrowColourId)
                                 : Palette::textDisabled);
}

void LuthierLookAndFeel::drawChevron (juce::Graphics& g, juce::Point<float> c, float halfWidth,
                                      int direction, juce::Colour colour, float thickness)
{
    // The combo's chevron: two strokes meeting at a rounded point, the point
    // 4.5 units past the ends for a half width of 4.
    const float depth = halfWidth * 1.125f;
    const float back = halfWidth * 0.5f;

    juce::Path arrow;

    switch (direction & 3)
    {
        case 0:   // up
            arrow.startNewSubPath (c.x - halfWidth, c.y + back);
            arrow.lineTo (c.x, c.y - (depth - back));
            arrow.lineTo (c.x + halfWidth, c.y + back);
            break;

        case 1:   // right
            arrow.startNewSubPath (c.x - back, c.y - halfWidth);
            arrow.lineTo (c.x + (depth - back), c.y);
            arrow.lineTo (c.x - back, c.y + halfWidth);
            break;

        case 3:   // left
            arrow.startNewSubPath (c.x + back, c.y - halfWidth);
            arrow.lineTo (c.x - (depth - back), c.y);
            arrow.lineTo (c.x + back, c.y + halfWidth);
            break;

        case 2:   // down
        default:
            arrow.startNewSubPath (c.x - halfWidth, c.y - back);
            arrow.lineTo (c.x, c.y + (depth - back));
            arrow.lineTo (c.x + halfWidth, c.y - back);
            break;
    }

    g.setColour (colour);
    g.strokePath (arrow, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void LuthierLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                                    int standardMenuItemHeight,
                                                    int& idealWidth, int& idealHeight)
{
    juce::LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator, standardMenuItemHeight,
                                                     idealWidth, idealHeight);

    if (! isSeparator)
        idealHeight = juce::jmax (minimumPopupItemHeight, idealHeight);
}

juce::PopupMenu::Options LuthierLookAndFeel::getOptionsForComboBoxPopupMenu (juce::ComboBox& box,
                                                                             juce::Label& label)
{
    return juce::LookAndFeel_V4::getOptionsForComboBoxPopupMenu (box, label)
             .withStandardItemHeight (juce::jmax (minimumPopupItemHeight, label.getHeight()));
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

    /*  Accent at every state, dimmed at rest: the earlier edge-coloured thumb
        vanished into the track, and a scrollbar nobody can see is a column
        nobody knows scrolls. */
    g.setColour (isMouseDown ? Palette::accentBright
                             : (isMouseOver ? Palette::accent : Palette::accentDim));
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (thumb.toFloat().reduced (0.5f), 3.0f, 1.0f);
}

void LuthierLookAndFeel::drawScrollbarButton (juce::Graphics& g, juce::ScrollBar&, int width, int height,
                                              int buttonDirection, bool, bool isMouseOverButton,
                                              bool isButtonDown)
{
    juce::Rectangle<int> area (0, 0, width, height);

    g.setColour (Palette::panelSunken);
    g.fillRect (area);

    if (isMouseOverButton || isButtonDown)
    {
        g.setColour (Palette::accent.withAlpha (isButtonDown ? 0.30f : 0.15f));
        g.fillRoundedRectangle (area.reduced (1).toFloat(), 2.0f);
    }

    const float half = juce::jlimit (2.0f, 4.0f, (float) juce::jmin (width, height) * 0.3f);

    drawChevron (g, area.toFloat().getCentre(), half, buttonDirection,
                 isButtonDown ? Palette::accentBright : Palette::accent);
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
