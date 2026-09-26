/*  The Luthier guitar-shop theme: proposals/visual-polish.md 6 and 7, and
    accessibility.md 6's palettes reaching the panels. */

#include "TestFramework.h"

#include "../UI/Theme.h"
#include "../UI/Widgets.h"
#include "../UI/HeaderBar.h"
#include "../Accessibility/Accessibility.h"
#include "../PluginProcessor.h"
#include "../Parameters.h"

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

//==============================================================================
// gui-integration.md / ui-wiring.md gap fill: rows this batch closes out with a
// test rather than new behaviour, because the behaviour was already there.
//==============================================================================

LUTHIER_TEST (Theme, monoFontIsAPreferredFaceOrFallback)
{
    // theme.md 6: JetBrains Mono / IBM Plex Mono, or a sane monospaced fallback
    // when neither is installed (no binary font is bundled on this checkout -
    // docs/coverage/GAPS-GUI.md TH-6).
    const auto name = Fonts::mono (12.0f).getTypefaceName();
    CHECK_MSG (name.isNotEmpty(), "Fonts::mono resolved to no typeface at all");

    static const char* preferred[] = { "JetBrains Mono", "IBM Plex Mono", "Cascadia Mono",
                                       "Consolas", "SF Mono", "Menlo", "DejaVu Sans Mono" };
    bool isKnownMono = false;
    for (auto* candidate : preferred)
        if (name.equalsIgnoreCase (candidate))
            isKnownMono = true;

    // Either a preferred mono face, or JUCE's own default monospaced fallback -
    // never a proportional UI face standing in by accident.
    CHECK_MSG (isKnownMono || name == juce::Font::getDefaultMonospacedFontName(),
               "Fonts::mono resolved to " + name);
}

LUTHIER_TEST (Theme, knobSizesAreTheSpecsThree)
{
    // theme.md 8: 48 / 36 / 64 px knob bodies.
    CHECK (Metrics::knobDefault == 48);
    CHECK (Metrics::knobSmall == 36);
    CHECK (Metrics::knobLarge == 64);

    using Size = LuthierKnob::Size;
    CHECK (LuthierKnob::preferredHeightFor (Size::Small) - 28 == Metrics::knobSmall);
    CHECK (LuthierKnob::preferredHeightFor (Size::Normal) - 28 == Metrics::knobDefault);
    CHECK (LuthierKnob::preferredHeightFor (Size::Large) - 28 == Metrics::knobLarge);
    CHECK (LuthierKnob::preferredHeightFor (Size::Macro) - 28 == Metrics::knobMacro);
}

LUTHIER_TEST (Theme, knobRenderShowsIndicatorArcAndDot)
{
    // theme.md 10/12: a 2px indicator line from centre to rim, a value arc that
    // fills as the value rises, and a 4px centre dot that is muted at the
    // default and accent once the value has moved away from it.
    PaletteGuard guard;
    Palette::apply (AccessibilitySettings::buildPalette (PaletteId::defaultDark), true);

    LuthierLookAndFeel lnf;

    auto renderAt = [&] (double value, double doubleClickReturn)
    {
        juce::Image image (juce::Image::ARGB, 64, 64, true);
        juce::Graphics g (image);

        juce::Slider knob (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        knob.setLookAndFeel (&lnf);
        knob.setRange (0.0, 1.0);
        knob.setDoubleClickReturnValue (true, doubleClickReturn);
        knob.setValue (value, juce::dontSendNotification);
        knob.setBounds (0, 0, 64, 64);
        knob.paintEntireComponent (g, false);
        knob.setLookAndFeel (nullptr);
        return image;
    };

    // At the default value the centre dot is muted; away from it, it is accent.
    const auto atDefault = renderAt (0.5, 0.5);
    const auto awayFromDefault = renderAt (0.9, 0.5);

    CHECK_MSG (atDefault.getPixelAt (32, 32) != awayFromDefault.getPixelAt (32, 32),
               "the centre dot looks the same at default and away from it");

    // A knob at its minimum draws (almost) no value arc; a knob near its
    // maximum fills most of the 270 degree sweep, which lands ink well
    // outside the knob body along its lower-right quadrant.
    const auto low = renderAt (0.0, 0.0);
    const auto high = renderAt (0.95, 0.0);

    int litLow = 0, litHigh = 0;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
        {
            if (low.getPixelAt (x, y).getAlpha() > 10) ++litLow;
            if (high.getPixelAt (x, y).getAlpha() > 10) ++litHigh;
        }

    CHECK_MSG (litHigh > litLow, "a knob near its maximum does not paint more than one near its minimum ("
                                     + juce::String (litHigh) + " vs " + juce::String (litLow) + ")");
}

LUTHIER_TEST (Theme, knobDoubleClickReturnsToDefaultAndMenuResets)
{
    // theme.md 13: double-click resets to the default; the right-click menu's
    // "Reset to default" (result id 2) does the same thing without a mouse.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    LuthierKnob knob ("Gain");
    knob.attachTo (processor, ParamIDs::ampGain);

    auto* param = processor.getState().getParameter (ParamIDs::ampGain);
    CHECK (param != nullptr);

    // The attachment (juce::AudioProcessorValueTreeState::SliderAttachment)
    // wires setDoubleClickReturnValue itself - this is the regression test that
    // it still does.
    CHECK (knob.getSlider().isDoubleClickReturnEnabled());
    CHECK_NEAR (knob.getSlider().getDoubleClickReturnValue(), (double) param->getDefaultValue(), 1.0e-6);

    param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, param->getDefaultValue() + 0.3f));
    CHECK (std::abs (param->getValue() - param->getDefaultValue()) > 0.05f);

    juce::Component owner;
    applyParameterMenuResult (2, owner, processor, ParamIDs::ampGain);   // "Reset to default"
    CHECK_NEAR (param->getValue(), param->getDefaultValue(), 1.0e-6);
}

LUTHIER_TEST (Theme, knobShowsValueOnlyWhileHovered)
{
    // theme.md 14: the label sits below always; the value row above only
    // appears on hover or while dragging.
    PaletteGuard guard;
    Palette::apply (AccessibilitySettings::buildPalette (PaletteId::defaultDark), true);

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    LuthierKnob knob ("Gain");
    knob.attachTo (processor, ParamIDs::ampGain);
    knob.setBounds (0, 0, 64, 76);

    auto render = [&]
    {
        juce::Image image (juce::Image::ARGB, 64, 76, true);
        juce::Graphics g (image);
        knob.paintEntireComponent (g, false);
        return image;
    };

    const auto idle = render();

    knob.mouseEnter (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 32.0f, 38.0f },
                                       juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &knob, &knob,
                                       juce::Time::getCurrentTime(), { 32.0f, 38.0f }, juce::Time::getCurrentTime(), 1, false));
    const auto hovered = render();

    int litValueRowIdle = 0, litValueRowHovered = 0;
    for (int x = 0; x < 64; ++x)
        for (int y = 0; y < 14; ++y)   // the value row, per LuthierKnob::paint
        {
            if (idle.getPixelAt (x, y).getAlpha() > 10) ++litValueRowIdle;
            if (hovered.getPixelAt (x, y).getAlpha() > 10) ++litValueRowHovered;
        }

    CHECK_MSG (litValueRowHovered > litValueRowIdle,
               "hovering did not reveal the value row (" + juce::String (litValueRowHovered)
                   + " vs " + juce::String (litValueRowIdle) + " lit pixels)");
}

LUTHIER_TEST (Theme, sliderTicksAreDrawnOutsideTheTrack)
{
    // theme.md 15: 1px muted ticks outside the track.
    PaletteGuard guard;
    Palette::apply (AccessibilitySettings::buildPalette (PaletteId::defaultDark), true);

    LuthierLookAndFeel lnf;
    juce::Image image (juce::Image::ARGB, 200, 40, true);
    juce::Graphics g (image);

    juce::Slider fader (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    fader.setLookAndFeel (&lnf);
    fader.setRange (0.0, 1.0);
    fader.setValue (0.5, juce::dontSendNotification);
    fader.setBounds (0, 0, 200, 40);
    fader.paintEntireComponent (g, false);
    fader.setLookAndFeel (nullptr);

    // The track sits vertically centred; the ticks are drawn a few pixels
    // below it, outside the 4px track band.
    const int tickY = 40 / 2 + 2 + 3 + 2;   // track half-thickness + gap + tick centre, generously
    int litBelowTrack = 0;
    for (int x = 0; x < 200; ++x)
        if (image.getPixelAt (x, tickY).getAlpha() > 10)
            ++litBelowTrack;

    CHECK_MSG (litBelowTrack >= 5, "found only " + juce::String (litBelowTrack) + " tick pixels below the track");
}

LUTHIER_TEST (Theme, buttonStatesMatchTheSpec)
{
    // theme.md 16/23: 28px tall, 4px corner radius; on state carries an accent
    // border the off state does not.
    PaletteGuard guard;
    Palette::apply (AccessibilitySettings::buildPalette (PaletteId::defaultDark), true);

    CHECK (Metrics::buttonHeight == 28);
    CHECK (Metrics::panelCorner == 4.0f);

    LuthierLookAndFeel lnf;

    auto renderButton = [&] (bool on)
    {
        juce::Image image (juce::Image::ARGB, 80, Metrics::buttonHeight, true);
        juce::Graphics g (image);
        juce::TextButton button ("Test");
        button.setLookAndFeel (&lnf);
        button.setClickingTogglesState (true);
        button.setToggleState (on, juce::dontSendNotification);
        button.setBounds (0, 0, 80, Metrics::buttonHeight);
        button.paintEntireComponent (g, false);
        button.setLookAndFeel (nullptr);
        return image;
    };

    const auto off = renderButton (false);
    const auto on = renderButton (true);

    // The exact top-left pixel sits outside the rounded corner, so the radius
    // is what is keeping it unfilled.
    CHECK_MSG (off.getPixelAt (0, 0).getAlpha() < 40, "the off button is not rounded at its corner");
    CHECK_MSG (on.getPixelAt (0, 0).getAlpha() < 40, "the on button is not rounded at its corner");

    const auto onBorder = on.getPixelAt (40, 1);
    const auto offBorder = off.getPixelAt (40, 1);
    CHECK_MSG (onBorder != offBorder, "the on and off border colours look the same");
}

LUTHIER_TEST (Theme, knobsAndSlidersShowAVerticalResizeCursor)
{
    // theme.md 27: a vertical-drag control shows the vertical-resize cursor.
    LuthierKnob knob ("Test");
    CHECK (knob.getSlider().getMouseCursor() == juce::MouseCursor::UpDownResizeCursor);

    LuthierSlider slider ("Test");
    CHECK (slider.getSlider().getMouseCursor() == juce::MouseCursor::UpDownResizeCursor);
}

LUTHIER_TEST (Theme, theHeaderCarriesPresetSelectorAndAB)
{
    // theme.md 31: name left; preset selector and A/B right.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    HeaderBar header (processor);
    header.setBounds (0, 0, 1280, Metrics::headerHeight);

    CHECK (header.getPresetNameButton().isVisible());
    CHECK (header.getCompareAButton().isVisible());
    CHECK (header.getCompareBButton().isVisible());

    CHECK (! processor.isSlotBActive());
    header.getCompareBButton().triggerClick();
    CHECK (processor.isSlotBActive());

    header.getCompareAButton().triggerClick();
    CHECK (! processor.isSlotBActive());
}

namespace
{
    /** Feeds a fixed-level block straight into the master bus, bypassing the
        rest of the engine, so a meter/LED test controls the peak precisely. */
    void feedMasterBusLevel (LuthierAudioProcessor& processor, float level, int numSamples = 512)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            buffer.setSample (0, i, level);
            buffer.setSample (1, i, level);
        }
        processor.getEngine().getMasterBus().processBlock (buffer);
    }
}

LUTHIER_TEST (Theme, outputLedTracksLevelHoldsRedAndGoesStale)
{
    // theme.md 34 / gui-engine-dataflow.md 8: grey at -inf, brightens toward
    // white near 0 dBFS, red (held 400 ms) above it, unlit 100 ms after the
    // last processed block.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.getEngine().getMasterBus().setLimiterEnabled (false);

    OutputLed led;
    led.setSource (&processor);

    feedMasterBusLevel (processor, 0.0f);
    led.refresh();
    CHECK (! led.isStale());
    CHECK (! led.isOverThreshold());
    const float quiet = led.getBrightness();

    feedMasterBusLevel (processor, 1.0f);
    led.refresh();
    CHECK_MSG (led.getBrightness() > quiet, "a loud block did not brighten the LED");
    CHECK (! led.isOverThreshold());

    feedMasterBusLevel (processor, 2.0f);   // ~+6 dBFS
    led.refresh();
    CHECK (led.isOverThreshold());

    // Drop the signal hard - the master bus's own meter decays per block, not
    // per millisecond, so several silent blocks are needed to bring the real
    // peak back under 0 dBFS. Red still holds: kOverHoldMs has not elapsed in
    // wall-clock time yet.
    for (int i = 0; i < 12; ++i)
        feedMasterBusLevel (processor, 0.0f);
    led.refresh();
    CHECK_MSG (led.isOverThreshold(), "red did not hold immediately after the peak dropped");

    juce::Thread::sleep (juce::roundToInt (OutputLed::kOverHoldMs) + 50);
    feedMasterBusLevel (processor, 0.0f);
    led.refresh();
    CHECK_MSG (! led.isOverThreshold(), "red held well past its 400 ms window");

    // No further blocks at all: stale, and dark.
    juce::Thread::sleep (juce::roundToInt (OutputLed::kStaleAfterMs) + 50);
    led.refresh();
    CHECK (led.isStale());
    CHECK (led.getBrightness() == 0.0f);
    CHECK (! led.isOverThreshold());
}

LUTHIER_TEST (Theme, levelMeterHoldsDecaysAndGoesStale)
{
    // ui-wiring.md 30 / gui-engine-dataflow.md 5: peak hold on the UI side,
    // and -inf 200 ms after the last processed block rather than a slow decay
    // from whatever the last real peak happened to be.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.getEngine().getMasterBus().setLimiterEnabled (false);

    LevelMeter meter;
    meter.setSource (&processor);

    feedMasterBusLevel (processor, 0.5f);
    meter.refresh();
    CHECK (! meter.isStale());
    const float held = meter.getPeakHoldLeft();
    CHECK_MSG (held > 0.0f, "a loud block left no peak hold");
    CHECK_MSG (meter.getDisplayPeakDb() > -100.0f, "the peak readout stayed at -inf after a loud block");

    // Quiet again straight after: the hold does not collapse instantly.
    feedMasterBusLevel (processor, 0.0f);
    meter.refresh();
    CHECK_NEAR (meter.getPeakHoldLeft(), held, 0.02);

    // No more blocks at all for the stale window: -inf, not a slow fall.
    juce::Thread::sleep (juce::roundToInt (LevelMeter::kStaleAfterMs) + 50);
    meter.refresh();
    CHECK (meter.isStale());
    CHECK (meter.getPeakHoldLeft() == 0.0f);
    CHECK (meter.getDisplayPeakDb() <= -99.0f);
}
