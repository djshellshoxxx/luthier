/*  Options -> Appearance (gui-integration.md 5) and the live displays
    (visual-polish.md 4 and 5; ui-wiring.md 10 and 11; gui-integration 12 and
    21), workstream VISUAL-WORKSHOP-QA. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/Widgets.h"
#include "../UI/NoiseGroups.h"
#include "../UI/UiPreferences.h"
#include "../UI/Theme.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct SettingsScope
    {
        PaletteId palette = AccessibilitySettings::get().getPalette();
        int accent = AccessibilitySettings::get().getAccent();
        bool reduced = AccessibilitySettings::get().isReducedMotion();
        bool stream = DataStreamDisplay::isEnabledByUser();
        bool strip = NoiseEventStrip::isEnabledByUser();

        ~SettingsScope()
        {
            auto& s = AccessibilitySettings::get();
            s.setAccent (accent);
            s.setPalette (palette);
            s.setReducedMotion (reduced);
            DataStreamDisplay::setEnabledByUser (stream);
            NoiseEventStrip::setEnabledByUser (strip);
        }
    };
}

//==============================================================================
/*  visual-polish.md 7: "every accent option on every palette meets 4.5:1 for
    text" - and the guitar's finish, whatever it is, after adjustment. */
LUTHIER_TEST (Accent, everyChoiceMeetsContrastOnEveryPalette)
{
    SettingsScope scope;
    auto& settings = AccessibilitySettings::get();

    const juce::Colour finishes[] = { juce::Colour (0xff0f0f0f), juce::Colour (0xfff5f1e8), juce::Colour (0xff7a2e1b),
                                      juce::Colour (0xff96b8d2), juce::Colour (0xffb22820) };

    for (int p = 0; p < (int) PaletteId::numPalettes; ++p)
    {
        settings.setPalette ((PaletteId) p);

        for (int a = 0; a < AccessibilitySettings::kNumAccents; ++a)
        {
            settings.setAccent (a);
            const double c = AccessibilitySettings::accentContrast (settings.getColours().accent, settings.getColours());
            CHECK_MSG (c >= 4.5, juce::String (getPaletteName ((PaletteId) p)) + " / " + AccessibilitySettings::getAccentNames()[a]
                                     + ": " + juce::String (c, 2) + " to 1");
        }

        settings.setAccent (AccessibilitySettings::kFollowGuitar);

        for (auto finish : finishes)
        {
            settings.setGuitarAccentSource (finish);
            const double c = AccessibilitySettings::accentContrast (settings.getColours().accent, settings.getColours());
            CHECK_MSG (c >= 4.5, "the guitar's " + finish.toDisplayString (false) + " on "
                                     + getPaletteName ((PaletteId) p) + ": " + juce::String (c, 2));
        }
    }

    // Choice 0 is the palette's own brass, untouched.
    settings.setPalette (PaletteId::defaultDark);
    settings.setAccent (0);
    CHECK (settings.getColours().accent == PaletteColours().accent);

    // A choice survives a palette change, and is saved with the settings.
    settings.setAccent (4);
    const auto blue = settings.getColours().accent;
    settings.setPalette (PaletteId::light);
    CHECK (settings.getColours().accent != PaletteColours().accent);
    CHECK ((int) settings.toVar().getProperty ("accent", 0) == 4);
    settings.setPalette (PaletteId::defaultDark);
    CHECK (settings.getColours().accent == blue);
}

/*  The accent reaches the window: choosing one repaints the plugin in it, and
    "Follow the guitar" follows a finish change. */
LUTHIER_TEST (Accent, theWindowTakesTheAccentAndFollowsTheGuitar)
{
    SettingsScope scope;
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr);
    if (editor == nullptr)
        return;

    auto& settings = AccessibilitySettings::get();
    settings.setAccent (2);
    settings.dispatchPendingMessages();
    CHECK (Palette::accent == settings.getColours().accent);
    CHECK (Palette::accent != PaletteColours().accent);

    settings.setAccent (AccessibilitySettings::kFollowGuitar);
    settings.setGuitarAccentSource (juce::Colour (0xff425878));   // Lake Placid Blue
    settings.dispatchPendingMessages();
    CHECK (Palette::accent == AccessibilitySettings::accentFor (AccessibilitySettings::kFollowGuitar, settings.getColours(),
                                                                juce::Colour (0xff425878)));
}

//==============================================================================
/*  gui-integration 12 and ui-wiring 11: the footer's data stream keeps 200
    lines, stops 500 ms after the last record, holds still under reduced
    motion, and Options -> Appearance hides it. */
LUTHIER_TEST (DataStream, itKeeps200StopsAfter500msAndHonoursReducedMotion)
{
    SettingsScope scope;
    AccessibilitySettings::get().setReducedMotion (false);

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::Component parent;
    DataStreamDisplay stream;
    parent.addAndMakeVisible (stream);
    stream.setSource (&processor);
    DataStreamDisplay::setEnabledByUser (true);

    auto play = [&processor] (int blocks)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 40 + (b % 24), 0.8f), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 40 + (b % 24)), 256);
            processor.processBlock (buffer, midi);
        }
    };

    for (int round = 0; round < 40; ++round)
    {
        play (4);
        stream.update (1000.0 + round);
    }

    CHECK (stream.isScrolling());
    CHECK (stream.getNumLinesKept() > 0 && stream.getNumLinesKept() <= DataStreamDisplay::kMaxLines);

    // Nothing new: still scrolling at 499 ms, stopped at 500.
    stream.update (1039.0 + 499.0);
    CHECK (stream.isScrolling());
    stream.update (1039.0 + 500.0);
    CHECK (! stream.isScrolling());

    // Reduced motion: new records do not move it.
    AccessibilitySettings::get().setReducedMotion (true);
    const int kept = stream.getNumLinesKept();
    play (8);
    stream.update (5000.0);
    CHECK (! stream.isScrolling());
    CHECK (stream.getNumLinesKept() == kept);

    // Appearance hides it.
    DataStreamDisplay::setEnabledByUser (false);
    stream.update (6000.0);
    CHECK (! stream.isVisible());
}

/*  gui-integration 21 / ui-wiring 10: under reduced motion the noise strip is a
    static count per kind; Appearance hides it. */
LUTHIER_TEST (NoiseStrip, reducedMotionShowsAStaticCountAndAppearanceHidesIt)
{
    SettingsScope scope;
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::Component parent;
    NoiseEventStrip strip (processor);
    parent.addAndMakeVisible (strip);
    strip.setSize (300, NoiseEventStrip::preferredHeight);

    // Playing with the pick click up makes events.
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::pickClickAmount)))
        p->setValueNotifyingHost (1.0f);
    processor.getParameterBridge().applyAllNow();

    juce::AudioBuffer<float> buffer (2, 512);
    for (int b = 0; b < 20; ++b)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52 + (b % 5), 1.0f), 0);
        processor.processBlock (buffer, midi);
    }

    strip.pollNow();
    int total = 0;
    for (auto c : strip.getClassCounts())
        total += c;
    CHECK_MSG (total > 0, "no noise events to count");

    AccessibilitySettings::get().setReducedMotion (true);
    juce::Image a (juce::Image::ARGB, 300, NoiseEventStrip::preferredHeight, true, juce::SoftwareImageType());
    juce::Image b (a.createCopy());
    {
        juce::Graphics g (a);
        strip.paintEntireComponent (g, true);
    }
    juce::Thread::sleep (60);
    {
        juce::Graphics g (b);
        strip.paintEntireComponent (g, true);
    }

    bool same = true;
    for (int y = 0; y < a.getHeight() && same; ++y)
        for (int x = 0; x < a.getWidth() && same; ++x)
            same = a.getPixelAt (x, y) == b.getPixelAt (x, y);
    CHECK_MSG (same, "the reduced-motion strip moved between two paints");

    NoiseEventStrip::setEnabledByUser (false);
    strip.timerCallbackForTest();
    CHECK (! strip.isVisible());
}
