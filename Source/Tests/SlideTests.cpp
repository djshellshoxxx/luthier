/*  Slide Mode (slide-guitar.md 9). */

#include "TestFramework.h"

#include "../DSP/Slide/SlideEngine.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/CharacterPanel.h"
#include "../UI/SlideGroup.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    SlideSettings slideOn (SlideMode mode, double assist = 0.15)
    {
        SlideSettings s;
        s.enabled = true;
        s.mode = mode;
        s.intonationAssist = assist;
        s.dampingBehind = (mode == SlideMode::lapSteel || mode == SlideMode::dobro) ? 1.0 : 0.55;
        return s;
    }

    void run (LuthierEngine& engine, int blocks, std::function<void (int)> perBlock = {})
    {
        juce::AudioBuffer<float> block (2, 256);

        for (int b = 0; b < blocks; ++b)
        {
            block.clear();
            juce::MidiBuffer none;
            engine.processBlock (block, none);

            if (perBlock)
                perBlock (b);
        }
    }

    NoteOnEvent slideNote (int string, double fret, double velocity = 0.8)
    {
        NoteOnEvent e;
        e.stringIndex = string;
        e.fretPosition = fret;
        e.velocity = velocity;
        e.pitchHz = 110.0 * std::pow (2.0, fret / 12.0);
        e.technique = Technique::SlideGuitar;
        return e;
    }
}

//==============================================================================
LUTHIER_TEST (Slide, pitchIsContinuous)
{
    LuthierEngine engine;
    engine.prepare (48000.0, 256);
    engine.setGuitarType (GuitarType::Stratocaster);
    engine.setSlideSettings (slideOn (SlideMode::lapSteel, 0.0));

    engine.triggerNoteNow (slideNote (2, 3.0));
    run (engine, 10);

    auto sweep = slideNote (2, 5.0);
    sweep.slideFromFret = 3.0;
    sweep.slideSeconds = 1.0;
    engine.triggerNoteNow (sweep);

    std::vector<double> f0;
    // A little past the second the move takes, so the last reading is after it ends.
    run (engine, (int) (1.05 * 48000.0 / 256.0), [&] (int) { f0.push_back (engine.getStringFrequency (2)); });

    int backwards = 0, offGrid = 0;

    for (size_t i = 1; i < f0.size(); ++i)
    {
        if (f0[i] < f0[i - 1] - 1.0e-6)
            ++backwards;

        const double semis = 12.0 * std::log2 (f0[i] / f0.front());
        if (std::abs (semis - std::round (semis)) > 0.1)
            ++offGrid;
    }

    CHECK_MSG (backwards == 0, juce::String (backwards) + " blocks where the sweep went backwards");
    CHECK_MSG (offGrid > (int) f0.size() / 4, "the sweep sat on the semitone grid - it quantised");
    // And the bar arrives: two frets up, where the move ends. Within 25 cents,
    // because the first reading is taken just after the pluck, when a string
    // sits a little sharp from its own amplitude.
    CHECK_MSG (std::abs (12.0 * std::log2 (f0.back() / f0.front()) - 2.0) < 0.25,
               "swept from " + juce::String (f0.front(), 2) + " to " + juce::String (f0.back(), 2)
                 + " Hz, midpoint " + juce::String (f0[f0.size() / 2], 2));
}

LUTHIER_TEST (Slide, theAssistPullsToPitch)
{
    SlideEngine slide;
    slide.prepare (48000.0);

    auto s = slideOn (SlideMode::bottleneck, 1.0);
    slide.setSettings (s);
    slide.noteOn (0, 6);

    // 40 cents sharp of fret 5, settling for 400 ms in 256-sample blocks.
    double fret = 0.0;

    for (int b = 0; b < (int) (0.4 * 48000.0 / 256.0); ++b)
        fret = slide.assist (0, 5.4, 256);

    CHECK_MSG (std::abs (fret - 5.0) * 100.0 < 5.0,
               "after 400 ms the assist is " + juce::String ((fret - 5.0) * 100.0, 1) + " cents off");

    // At 0 it leaves the bar where it is.
    s.intonationAssist = 0.0;
    slide.setSettings (s);
    slide.noteOn (1, 6);

    for (int b = 0; b < 100; ++b)
        fret = slide.assist (1, 5.4, 256);

    CHECK (std::abs (fret - 5.4) < 1.0e-9);
}

LUTHIER_TEST (Slide, theSegmentBehindIsDamped)
{
    auto decaySeconds = [] (bool underBar)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.setSlideSettings (slideOn (SlideMode::lapSteel));

        auto note = slideNote (0, 12.0);

        if (! underBar)
        {
            engine.setSlideSettings (SlideSettings());
            note.technique = Technique::Pluck;
        }

        engine.triggerNoteNow (note);

        double peak = 0.0, when = -1.0;

        run (engine, (int) (8.0 * 48000.0 / 256.0), [&] (int b)
        {
            const double level = engine.getStringLevel (0);
            peak = juce::jmax (peak, level);

            if (when < 0.0 && peak > 0.0 && level < peak * 0.001 && b > 10)
                when = b * 256.0 / 48000.0;
        });

        return when < 0.0 ? 8.0 : when;
    };

    const double fretted = decaySeconds (false), slid = decaySeconds (true);

    CHECK_MSG (slid <= fretted * 0.75,
               "fretted decays in " + juce::String (fretted, 2) + " s, under a lap-steel bar in "
                 + juce::String (slid, 2) + " s - not 25% shorter");
}

LUTHIER_TEST (Slide, slantGivesEachStringItsOwnInterval)
{
    SlideEngine slide;
    auto s = slideOn (SlideMode::bottleneck);
    s.slantDegrees = 20.0;
    slide.setSettings (s);

    const double top = slide.contactFret (0, 7.0, 6, 648.0);
    const double bottom = slide.contactFret (5, 7.0, 6, 648.0);

    CHECK_MSG (std::abs (top - bottom) * 100.0 >= 40.0,
               "20 degrees of slant moved the strings only " + juce::String (std::abs (top - bottom) * 100.0, 1)
                 + " cents apart");
}

LUTHIER_TEST (Slide, aHeavierBarSustainsLonger)
{
    SlideEngine slide;
    slide.setSettings (slideOn (SlideMode::bottleneck));
    slide.noteOn (0, 6);

    SlideBar light, heavy;
    light.massGrams = 30.0;
    heavy.massGrams = 200.0;

    slide.setBar (light);
    const double lightScale = slide.sustainScale (0);
    slide.setBar (heavy);
    const double heavyScale = slide.sustainScale (0);

    CHECK_MSG (heavyScale >= lightScale * 1.15,
               "200 g sustains " + juce::String (heavyScale / lightScale, 2) + "x a 30 g bar");
}

LUTHIER_TEST (Slide, theBarClanksWhenItLands)
{
    LuthierEngine engine;
    engine.prepare (48000.0, 256);
    engine.setGuitarType (GuitarType::Stratocaster);
    engine.setSlideSettings (slideOn (SlideMode::lapSteel));

    auto& pool = engine.getPlayingNoise().getPool();

    // Triggered at the contact itself, well inside 5 ms.
    engine.triggerNoteNow (slideNote (1, 5.0));
    CHECK (pool.getTriggerCount (NoiseClass::clank) == 1);

    // A second string under a bar already down does not land it again.
    engine.triggerNoteNow (slideNote (2, 5.0));
    CHECK (pool.getTriggerCount (NoiseClass::clank) == 1);

    // Brass clanks lower than glass.
    SlideEngine slide;
    slide.setSettings (slideOn (SlideMode::lapSteel));

    SlideBar glass, brass;
    brass.material = SlideMaterial::brass;

    slide.setBar (glass);
    const double glassHz = slide.makeClank (0, 0.8).startHz;
    slide.setBar (brass);
    const double brassHz = slide.makeClank (0, 0.8).startHz;

    CHECK (brassHz < glassHz);
}

LUTHIER_TEST (Slide, squeakStopsUnderTheBarButNotBesideIt)
{
    LuthierEngine engine;
    engine.prepare (48000.0, 256);
    engine.setGuitarType (GuitarType::Dreadnought);
    engine.setSlideSettings (slideOn (SlideMode::hybrid));

    SqueakSettings always;
    always.probability = 1.0;
    always.moisture = 0.0;
    engine.setSqueak (always);

    auto& pool = engine.getPlayingNoise().getPool();
    const int low = engine.getNumStrings() - 1;

    // The bar goes on the low string; a slide along it does not squeak.
    engine.triggerNoteNow (slideNote (low, 3.0));
    auto along = slideNote (low, 8.0);
    along.slideFromFret = 3.0;
    along.slideSeconds = 0.15;
    engine.triggerNoteNow (along);

    CHECK (pool.getTriggerCount (NoiseClass::squeak) == 0);

    // Hybrid: the next string is fretted, and a finger shift on it squeaks.
    engine.triggerNoteNow (slideNote (low - 1, 2.0));
    auto shift = slideNote (low - 1, 7.0);
    shift.slideFromFret = 2.0;
    shift.slideSeconds = 0.12;
    engine.triggerNoteNow (shift);

    CHECK_MSG (pool.getTriggerCount (NoiseClass::squeak) == 1,
               "a fretted shift beside the bar in hybrid mode did not squeak");
}

LUTHIER_TEST (Slide, switchingModeMidNoteIsClean)
{
    auto render = [] (bool toggle)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.setSlideSettings (slideOn (SlideMode::bottleneck, 0.0));
        engine.triggerNoteNow (slideNote (1, 5.0));

        std::vector<float> out;
        juce::AudioBuffer<float> block (2, 256);

        for (int b = 0; b < 80; ++b)
        {
            if (toggle && b == 40)
                engine.setSlideSettings (SlideSettings());

            block.clear();
            juce::MidiBuffer none;
            engine.processBlock (block, none);

            for (int i = 0; i < 256; ++i)
                out.push_back (block.getSample (0, i));
        }

        return out;
    };

    const auto steady = render (false), switched = render (true);

    double worst = 0.0;

    for (size_t i = 0; i < steady.size(); ++i)
        worst = juce::jmax (worst, (double) std::abs (steady[i] - switched[i]));

    CHECK_MSG (gainToDb (worst) < -60.0,
               "switching Slide Mode mid-note moved the output by " + juce::String (gainToDb (worst), 1) + " dBFS");
}

//==============================================================================
namespace
{
    template <typename T>
    T* findChild (juce::Component& root)
    {
        if (auto* hit = dynamic_cast<T*> (&root))
            return hit;

        for (auto* child : root.getChildren())
            if (auto* hit = findChild<T> (*child))
                return hit;

        return nullptr;
    }
}

LUTHIER_TEST (SlideUi, theSlideGroupAppearsWithSlideModeAndTheTabFitsIt)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 900);
    CHECK (panel.setWorkspaceTabNamed ("CHARACTER"));

    auto* character = findChild<CharacterPanel> (panel);
    auto* slide = findChild<SlideGroup> (panel);
    CHECK (character != nullptr && slide != nullptr);

    if (character == nullptr || slide == nullptr)
        return;

    // The tab is as tall as what is on it - it used to sit at 80 points.
    CHECK_MSG (character->getHeight() >= character->preferredHeight(),
               "CHARACTER is " + juce::String (character->getHeight()) + " tall for "
                 + juce::String (character->preferredHeight()) + " of content");

    CHECK_MSG (! slide->isVisible(), "the SLIDE group showed with Slide Mode off");
    const int before = character->getHeight();

    processor.getState().getParameter (ParamIDs::slideGuitar)->setValueNotifyingHost (1.0f);
    processor.getState().getParameter (ParamIDs::setupActionBass)->setValueNotifyingHost (0.0f);

    slide->refresh();

    CHECK_MSG (slide->isVisible(), "the SLIDE group did not appear in Slide Mode");
    CHECK (character->getHeight() > before);

    // 1.2 mm of bass action is not a slide setup, and the plugin says so without changing it.
    CHECK_MSG (slide->isLowActionWarningShowing(), "no low-action warning at 1.2 mm");
    CHECK (std::abs (processor.getState().getParameter (ParamIDs::setupActionBass)->getValue()) < 1.0e-6f);
}

LUTHIER_TEST (SlideUi, pressureSaysWhatItMeans)
{
    CHECK (SlideGroup::describePressure (0.1).startsWith ("Rattling"));
    CHECK (SlideGroup::describePressure (0.55) == "Seated");
    CHECK (SlideGroup::describePressure (0.95).startsWith ("Choking"));
}
