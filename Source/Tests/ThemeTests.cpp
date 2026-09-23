/*  The Luthier guitar-shop theme: proposals/visual-polish.md 6 and 7, and
    accessibility.md 6's palettes reaching the panels. */

#include "TestFramework.h"

#include "../UI/Theme.h"
#include "../UI/Widgets.h"
#include "../Accessibility/Accessibility.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** Restores the palette in force when it goes out of scope, so a test leaves no trace. */
    struct PaletteGuard
    {
        PaletteColours saved = Palette::current();
        bool textured = Palette::textured;
        ~PaletteGuard() { Palette::apply (saved, textured); }
    };

    juce::File renderFolder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-theme-renders");
        dir.createDirectory();
        return dir;
    }

    /** A knob, a slider, two toggles and a section plate on a panel, in the palette in force. */
    juce::Image renderControls()
    {
        LuthierLookAndFeel lnf;
        juce::Image image (juce::Image::ARGB, 420, 200, true);
        juce::Graphics g (image);

        g.fillAll (Palette::background);
        LuthierLookAndFeel::drawPanel (g, { 8.0f, 8.0f, 404.0f, 184.0f });
        LuthierLookAndFeel::drawSectionHeader (g, { 20, 16, 200, 22 }, "Amp");
        LuthierLookAndFeel::drawSignatureNotch (g, { 0, 0, 420, 200 });

        juce::Slider knob (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        knob.setLookAndFeel (&lnf);
        knob.setRange (0.0, 1.0);
        knob.setValue (0.65, juce::dontSendNotification);
        knob.setBounds (20, 50, 80, 80);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.setOrigin (knob.getPosition());
            knob.paintEntireComponent (g, false);
        }

        juce::Slider fader (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        fader.setLookAndFeel (&lnf);
        fader.setRange (0.0, 1.0);
        fader.setValue (0.4, juce::dontSendNotification);
        fader.setBounds (120, 70, 160, 40);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.setOrigin (fader.getPosition());
            fader.paintEntireComponent (g, false);
        }

        for (int i = 0; i < 2; ++i)
        {
            juce::ToggleButton toggle (i == 0 ? "Bright" : "Standby");
            toggle.setLookAndFeel (&lnf);
            toggle.setToggleState (i == 0, juce::dontSendNotification);
            toggle.setBounds (300, 50 + i * 40, 100, 28);
            juce::Graphics::ScopedSaveState s (g);
            g.setOrigin (toggle.getPosition());
            toggle.paintEntireComponent (g, false);
        }

        knob.setLookAndFeel (nullptr);
        fader.setLookAndFeel (nullptr);
        return image;
    }

    bool savePng (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat png;
        return out.openedOk() && png.writeImageToStream (image, out);
    }
}

//==============================================================================
LUTHIER_TEST (Theme, everyTextPairMeetsContrastOnTheThreePalettes)
{
    // visual-polish.md 6.1: "Every pair still meets 4.5:1 for text", on Default,
    // Light and High contrast; accent text (button labels, active tabs) included.
    for (auto id : { PaletteId::defaultDark, PaletteId::light, PaletteId::highContrast })
    {
        const auto p = AccessibilitySettings::buildPalette (id);

        for (auto surface : { p.backgroundDeep, p.background, p.panel, p.panelRaised, p.panelSunken })
            for (auto [name, text] : { std::pair<const char*, juce::Colour> { "primary", p.textPrimary },
                                       { "muted", p.textMuted }, { "accent", p.accent } })
            {
                const double ratio = PaletteColours::contrastRatio (text, surface);
                CHECK_MSG (ratio >= 4.5, juce::String (getPaletteName (id)) + " " + name + " text on "
                                             + surface.toDisplayString (false) + " is " + juce::String (ratio, 2) + ":1");
            }
    }
}

LUTHIER_TEST (Theme, theDefaultIsTheGuitarShop)
{
    // visual-polish.md 6.1: dark rosewood, walnut panels, ivory text, aged brass.
    const auto p = AccessibilitySettings::buildPalette (PaletteId::defaultDark);

    CHECK (p.background == juce::Colour (0xff1e1511));
    CHECK (p.panel == juce::Colour (0xff2a1e17));
    CHECK (p.textPrimary == juce::Colour (0xffefe3cc));
    CHECK (p.accent == juce::Colour (0xffd4a24c));

    // Warm, not blue-black: red outweighs blue in every neutral.
    for (auto c : { p.backgroundDeep, p.background, p.panel, p.panelSunken })
        CHECK (c.getRed() > c.getBlue());
}

LUTHIER_TEST (Theme, highContrastIsFlat)
{
    PaletteGuard guard;

    Palette::apply (AccessibilitySettings::buildPalette (PaletteId::highContrast), false);
    CHECK (! Palette::textured);

    Palette::apply (AccessibilitySettings::buildPalette (PaletteId::defaultDark), true);
    CHECK (Palette::textured);
}

LUTHIER_TEST (Theme, aPaletteChangeReachesBuiltComponents)
{
    // accessibility.md 6: choosing a palette recolours what is already on screen.
    PaletteGuard guard;

    const auto dark = AccessibilitySettings::buildPalette (PaletteId::defaultDark);
    const auto light = AccessibilitySettings::buildPalette (PaletteId::light);
    Palette::apply (dark, true);

    juce::Component root;
    juce::Label label;
    juce::TextButton button;
    root.addAndMakeVisible (label);
    root.addAndMakeVisible (button);

    label.setColour (juce::Label::textColourId, Palette::textMuted);
    button.setColour (juce::TextButton::buttonOnColourId, Palette::accent.withAlpha (0.15f));

    Palette::apply (light, true);
    Palette::remap (root, dark, light);

    CHECK (label.findColour (juce::Label::textColourId) == light.textMuted);
    CHECK (button.findColour (juce::TextButton::buttonOnColourId) == light.accent.withAlpha (0.15f));
    CHECK (Palette::textPrimary == light.textPrimary);
}

LUTHIER_TEST (Theme, theBundledFontsLoad)
{
    // visual-polish.md 6.2: the display face and the body face ship with the plugin.
    CHECK_MSG (Fonts::ui (12.0f).getTypefaceName().containsIgnoreCase ("Lato"),
               "body font is " + Fonts::ui (12.0f).getTypefaceName());
    CHECK_MSG (Fonts::display (14.0f).getTypefaceName().containsIgnoreCase ("Bebas"),
               "display font is " + Fonts::display (14.0f).getTypefaceName());
}

LUTHIER_TEST (Theme, controlsRenderInEveryPaletteAndRepeatExactly)
{
    // visual-polish.md 7: the standard knob, toggle and slider render in all three
    // palettes (PNG renders for review), and every lit surface renders identically twice.
    PaletteGuard guard;
    juce::Array<juce::Image> renders;

    for (auto id : { PaletteId::defaultDark, PaletteId::light, PaletteId::highContrast })
    {
        Palette::apply (AccessibilitySettings::buildPalette (id), id != PaletteId::highContrast);

        const auto a = renderControls();
        const auto b = renderControls();

        const juce::Image::BitmapData da (a, juce::Image::BitmapData::readOnly), db (b, juce::Image::BitmapData::readOnly);
        int diff = 0;
        for (int y = 0; y < a.getHeight(); ++y)
            for (int x = 0; x < a.getWidth(); ++x)
                diff += da.getPixelColour (x, y) == db.getPixelColour (x, y) ? 0 : 1;

        CHECK_MSG (diff == 0, juce::String (getPaletteName (id)) + ": " + juce::String (diff) + " pixels differ between two renders");
        CHECK (savePng (a, renderFolder().getChildFile (juce::String (getPaletteName (id)).replaceCharacter (' ', '_') + ".png")));
        renders.add (a);
    }

    // The three palettes look different.
    CHECK (renders[0].getPixelAt (200, 180) != renders[1].getPixelAt (200, 180));
    CHECK (renders[1].getPixelAt (200, 180) != renders[2].getPixelAt (200, 180));
}
