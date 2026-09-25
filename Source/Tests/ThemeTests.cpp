/*  The Luthier guitar-shop theme: proposals/visual-polish.md 6 and 7, and
    accessibility.md 6's palettes reaching the panels. */

#include "TestFramework.h"

#include <set>

#include "../UI/Theme.h"
#include "../UI/Widgets.h"
#include "../Accessibility/Accessibility.h"
#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/OptionsPages.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** Restores the palette in force when it goes out of scope, so a test leaves no trace. */
    struct PaletteGuard
    {
        // The first LookAndFeel in the process applies the settings' palette;
        // have that happen here, not inside a render after a test's own apply().
        LuthierLookAndFeel firstLookAndFeel;
        PaletteColours saved = Palette::current();
        bool textured = Palette::textured;
        bool illustrations = Palette::illustrationMaterials;
        ~PaletteGuard() { Palette::apply (saved, textured); Palette::illustrationMaterials = illustrations; }
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

    for (auto id : { PaletteId::defaultDark, PaletteId::light, PaletteId::highContrast, PaletteId::modernDark })
    {
        Palette::apply (AccessibilitySettings::buildPalette (id), id);

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

    // The palettes look different.
    CHECK (renders[0].getPixelAt (200, 180) != renders[1].getPixelAt (200, 180));
    CHECK (renders[1].getPixelAt (200, 180) != renders[2].getPixelAt (200, 180));
    CHECK (renders[2].getPixelAt (200, 180) != renders[3].getPixelAt (200, 180));
    CHECK (renders[0].getPixelAt (200, 180) != renders[3].getPixelAt (200, 180));
}

//==============================================================================
//  Modern Dark: a plain, flat, modern dark mode beside the guitar-shop look.
//==============================================================================
LUTHIER_TEST (Theme, modernDarkExistsIsSelectableAndRoundTrips)
{
    CHECK (juce::String (getPaletteName (PaletteId::modernDark)) == "Modern Dark");
    CHECK ((int) PaletteId::modernDark == (int) PaletteId::numPalettes - 1);   // appended: saved numbers keep their meaning

    auto& settings = AccessibilitySettings::get();
    const auto original = settings.getPalette();

    settings.setPalette (PaletteId::modernDark);
    CHECK (settings.getPalette() == PaletteId::modernDark);
    CHECK (settings.getColours().background == AccessibilitySettings::buildPalette (PaletteId::modernDark).background);

    // Saved the way the others are: through the settings' own state.
    const auto saved = juce::JSON::toString (settings.toVar(), false);
    settings.setPalette (PaletteId::defaultDark);
    settings.fromVar (juce::JSON::parse (saved));
    CHECK_MSG (settings.getPalette() == PaletteId::modernDark, "Modern Dark did not survive a save and load");

    // And as a theme file, like every built-in palette.
    const auto dark = AccessibilitySettings::buildPalette (PaletteId::modernDark);
    const auto restored = PaletteColours::fromVar (juce::JSON::parse (juce::JSON::toString (dark.toVar(), false)));
    CHECK (restored.background == dark.background && restored.accent == dark.accent
           && restored.textPrimary == dark.textPrimary && restored.edge == dark.edge);

    settings.setPalette (original);
    settings.dispatchPendingMessages();
}

LUTHIER_TEST (Theme, modernDarkIsOfferedAndSwitchesLive)
{
    PaletteGuard guard;
    auto& settings = AccessibilitySettings::get();
    const auto original = settings.getPalette();

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // Options -> Appearance lists it by its UI name.
    {
        AppearancePage page (processor);
        bool offered = false;

        for (auto* child : page.getChildren())
            if (auto* box = dynamic_cast<juce::ComboBox*> (child))
                for (int i = 0; i < box->getNumItems(); ++i)
                    offered = offered || box->getItemText (i) == "Modern Dark";

        CHECK_MSG (offered, "Options -> Appearance does not offer Modern Dark");
    }

    // An open editor picks it up without a restart.
    settings.setPalette (PaletteId::defaultDark);
    settings.dispatchPendingMessages();
    Palette::apply (settings.getColours(), settings.getPalette());

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr);

    if (editor != nullptr)
    {
        editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);
        CHECK (Palette::usesMaterials());

        settings.setPalette (PaletteId::modernDark);
        settings.dispatchPendingMessages();

        const auto dark = AccessibilitySettings::buildPalette (PaletteId::modernDark);
        CHECK_MSG (Palette::background == dark.background, "the open editor did not switch to Modern Dark");
        CHECK (! Palette::usesMaterials());
        CHECK_MSG (Palette::illustrationMaterials, "Modern Dark flattened the guitar illustration");
        CHECK (editor->getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId) == dark.background);

        settings.setPalette (PaletteId::defaultDark);
        settings.dispatchPendingMessages();
        CHECK (Palette::usesMaterials());
    }

    editor.reset();
    settings.setPalette (original);
    settings.dispatchPendingMessages();
}

LUTHIER_TEST (Theme, modernDarkContrastMeetsItsTargets)
{
    // Body text 7:1 (WCAG AAA) on every surface; secondary text, the accent and
    // its bright variant 4.5:1 (AA); the value arc, meter colours and disabled
    // text 3:1 (WCAG 1.4.11 for graphics, and "readable" for disabled); the
    // knob track visible against the panel it sits on.
    const auto p = AccessibilitySettings::buildPalette (PaletteId::modernDark);

    auto check = [&] (const char* what, juce::Colour fg, juce::Colour bg, double target)
    {
        const double ratio = PaletteColours::contrastRatio (fg, bg);
        CHECK_MSG (ratio >= target, juce::String ("Modern Dark ") + what + " " + fg.toDisplayString (false) + " on "
                                      + bg.toDisplayString (false) + " is " + juce::String (ratio, 2)
                                      + ":1, needs " + juce::String (target, 1));
    };

    for (auto surface : { p.backgroundDeep, p.background, p.panel, p.panelRaised, p.panelSunken })
    {
        check ("body text",     p.textPrimary,  surface, 7.0);
        check ("muted text",    p.textMuted,    surface, 4.5);
        check ("accent",        p.accent,       surface, 4.5);
        check ("bright accent", p.accentBright, surface, 4.5);
        check ("disabled text", p.textDisabled, surface, 3.0);
        check ("success",       p.success,      surface, 3.0);
        check ("warning",       p.warning,      surface, 3.0);
        check ("clip",          p.clip,         surface, 3.0);
        check ("data stream",   p.dataStream,   surface, 3.0);
    }

    // A button that is on: accent text on the accent's 15 % wash over a panel.
    check ("on-button text", p.accent, p.panel.overlaidWith (p.accent.withAlpha (0.15f)), 4.5);

    // The knob: its track, its pointer on the flat cap, and the value arc against the track.
    PaletteGuard guard;
    Palette::apply (p, PaletteId::modernDark);
    check ("knob track", Palette::knobTrack, p.panel, 2.0);
    check ("knob pointer", Palette::knobPointer, Palette::knobBody, 7.0);
    check ("value arc on track", p.accent, Palette::knobTrack, 2.0);
    check ("focus ring", p.accent, p.panelRaised, 3.0);

    // It is a neutral dark mode: no warm cast, surfaces stepping up in lightness.
    for (auto c : { p.backgroundDeep, p.background, p.panel, p.panelRaised, p.panelSunken, p.edge })
        CHECK (c.getRed() == c.getGreen() && c.getGreen() == c.getBlue());

    CHECK (p.background.getPerceivedBrightness() < p.panel.getPerceivedBrightness());
    CHECK (p.panel.getPerceivedBrightness() < p.panelRaised.getPerceivedBrightness());
    CHECK (p.getWorstTextContrast() >= 7.0);
}

LUTHIER_TEST (Theme, modernDarkDrawsNoMaterials)
{
    PaletteGuard guard;

    // Which palettes wear the guitar-shop materials.
    CHECK (paletteUsesMaterials (PaletteId::defaultDark));
    CHECK (paletteUsesMaterials (PaletteId::light));
    CHECK (! paletteUsesMaterials (PaletteId::highContrast));
    CHECK (! paletteUsesMaterials (PaletteId::modernDark));
    CHECK (paletteLightsIllustrations (PaletteId::modernDark));
    CHECK (! paletteLightsIllustrations (PaletteId::highContrast));

    /*  A large panel (big enough for corner screws), a section header and a
        fader: in the guitar-shop palette the panel has grain and screws and the
        header is a brass plate; in Modern Dark the panel interior is one flat
        colour, the corners carry no screw, and no brass appears anywhere. */
    auto renderPrimitives = []
    {
        LuthierLookAndFeel lnf;
        juce::Image image (juce::Image::ARGB, 400, 220, true);
        juce::Graphics g (image);
        g.fillAll (Palette::background);
        LuthierLookAndFeel::drawPanel (g, { 10.0f, 10.0f, 380.0f, 200.0f });
        LuthierLookAndFeel::drawSectionHeader (g, { 30, 150, 200, 22 }, "Amp");
        return image;
    };

    auto isBrass = [] (juce::Colour c)
    {
        const float h = c.getHue() * 360.0f;
        // The plate brass and its lit and shaded ends: a mid-saturated, bright
        // gold. A saturated amber (the warning colour) blended over a dark
        // surface never lands here: by the time it is this bright it is far
        // more saturated.
        return h > 30.0f && h < 50.0f && c.getSaturation() > 0.38f && c.getSaturation() < 0.68f
               && c.getBrightness() > 0.55f;
    };

    auto brassPixels = [&] (const juce::Image& image)
    {
        int n = 0;
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                n += isBrass (image.getPixelAt (x, y)) ? 1 : 0;
        return n;
    };

    auto distinctIn = [] (const juce::Image& image, juce::Rectangle<int> area)
    {
        std::set<juce::uint32> seen;
        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x)
                seen.insert (image.getPixelAt (x, y).getARGB());
        return (int) seen.size();
    };

    const juce::Rectangle<int> interior (40, 30, 320, 100);   // clear of the header and the edges
    const juce::Rectangle<int> screwCorner (12, 12, 12, 12);

    Palette::apply (AccessibilitySettings::buildPalette (PaletteId::defaultDark), PaletteId::defaultDark);
    const auto shop = renderPrimitives();

    Palette::apply (AccessibilitySettings::buildPalette (PaletteId::modernDark), PaletteId::modernDark);
    CHECK (! Palette::usesMaterials());
    const auto dark = renderPrimitives();

    // The probes work: the guitar shop does have grain, screws and brass.
    CHECK (distinctIn (shop, interior) > 1);
    CHECK (distinctIn (shop, screwCorner) > 2);
    CHECK (brassPixels (shop) > 100);

    CHECK_MSG (distinctIn (dark, interior) == 1, "Modern Dark panels still show wood grain");
    CHECK_MSG (dark.getPixelAt (interior.getCentreX(), interior.getCentreY()) == Palette::panel, "Modern Dark panel is not flat panel colour");
    CHECK_MSG (distinctIn (dark, screwCorner.reduced (2)) <= 2, "Modern Dark panels still carry corner screws");
    CHECK_MSG (brassPixels (dark) == 0, "Modern Dark still draws a brass plate: " + juce::String (brassPixels (dark)) + " pixels");

    CHECK (savePng (dark, renderFolder().getChildFile ("Modern_Dark_primitives.png")));
}
