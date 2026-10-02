/*  Slide technique controls (slide-technique-controls.md 7).

    SlideEngine's control layer on its own (sources, speed limit, gestures,
    auto-vibrato, the contact mask), then through the engine: the mod wheel
    moving the bar and the string following it.
*/

#include "TestFramework.h"

#include "../DSP/Slide/SlideEngine.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    struct Bar
    {
        explicit Bar (SlideControlSettings c = {})
        {
            slide.prepare (kSr);
            SlideSettings s;
            s.enabled = true;
            s.mode = SlideMode::bottleneck;
            slide.setSettings (s);
            slide.setControls (c);
            slide.noteOn (2, 6);
        }

        void cc (int number, int value)
        {
            juce::MidiBuffer m;
            m.addEvent (juce::MidiMessage::controllerEvent (1, number, value), 0);
            controls.processMidi (m);
        }

        /** Advances `seconds` in blocks of `block`; returns the bar fret after. */
        double advance (double seconds, int block = 64)
        {
            const int n = juce::jmax (1, (int) std::round (seconds * kSr / block));

            for (int i = 0; i < n; ++i)
            {
                slide.advanceControls (block, controls, triggers);
                fret = slide.controlledBarFret (2, 5.0);
            }

            return fret;
        }

        SlideEngine slide;
        TechniqueControls controls;
        TechniqueTriggers triggers;
        double fret = 0.0;
    };
}

//==============================================================================
/*  7: "Modwheel drives position through the mapped range." Absolute: 0-1 is
    fret 0-24 (1). */
LUTHIER_TEST (SlideControls, theModWheelDrivesThePosition)
{
    SlideControlSettings c;
    c.positionSource = ControlSource::modWheel;
    c.speedLimitCentsPerSecond = 1.0e6;
    Bar bar (c);

    for (int value : { 0, 32, 64, 96, 127 })
    {
        bar.cc (1, value);
        const double fret = bar.advance (0.1);
        CHECK_MSG (std::abs (fret - value / 127.0 * 24.0) < 0.05,
                   "wheel " + juce::String (value) + " put the bar at " + juce::String (fret, 3));
    }

    // With no source the notes place the bar, as before.
    Bar none;
    CHECK_NEAR (none.advance (0.05), 5.0, 1.0e-12);

    // Relative: the control adds to the fretted position.
    SlideControlSettings rel = c;
    rel.relative = true;
    rel.relativeRangeFrets = 12.0;
    Bar relative (rel);
    relative.cc (1, 127);
    CHECK_NEAR (relative.advance (0.1), 5.0 + 12.0, 0.05);
}

/*  7: "Speed limit clamps a full-throw modwheel jump to the configured
    cents-per-second." A fret is 100 cents. */
LUTHIER_TEST (SlideControls, theSpeedLimitClampsAJump)
{
    SlideControlSettings c;
    c.positionSource = ControlSource::modWheel;
    c.speedLimitCentsPerSecond = 4800.0;   // 48 frets a second
    Bar bar (c);

    bar.cc (1, 0);
    bar.advance (0.05);

    bar.cc (1, 127);
    double last = bar.fret, fastest = 0.0;

    for (int i = 0; i < 200; ++i)
    {
        const double now = bar.advance (64.0 / kSr);
        fastest = juce::jmax (fastest, (now - last) / (64.0 / kSr));
        last = now;
    }

    CHECK_MSG (fastest <= 48.0 * 1.001, "the bar moved at " + juce::String (fastest, 1) + " frets/s");
    CHECK_NEAR (bar.advance (1.0), 24.0, 0.05);   // and it does get there
}

/*  7: "Scripted gesture reaches target position within duration +/- 5 ms." */
LUTHIER_TEST (SlideControls, aScriptedGestureArrivesOnTime)
{
    for (auto curve : { SlideCurve::linear, SlideCurve::easeIn, SlideCurve::easeOut, SlideCurve::easeInOut })
    {
        Bar bar;
        SlideGesture g;
        g.fromFret = 3.0;
        g.toFret = 12.0;
        g.durationMs = 500.0;
        g.curve = curve;
        g.slantStartDegrees = -10.0;
        g.slantEndDegrees = 10.0;
        bar.slide.triggerGesture (g);

        double arrived = -1.0;

        for (int i = 0; i < 1000 && arrived < 0.0; ++i)
        {
            bar.advance (32.0 / kSr, 32);

            if (std::abs (bar.fret - 12.0) < 1.0e-6)
                arrived = (double) (i + 1) * 32.0 / kSr * 1000.0;
        }

        CHECK_MSG (std::abs (arrived - 500.0) <= 5.0, "arrived at " + juce::String (arrived, 2) + " ms");
        CHECK_NEAR (bar.slide.getSettings().slantDegrees, 10.0, 1.0e-9);
    }

    // Through its keyswitch (21), with Slide Mode on.
    auto engine = std::make_unique<LuthierEngine>();
    engine->prepare (kSr, 256);
    SlideSettings s;
    s.enabled = true;
    engine->setSlideSettings (s);
    SlideControlSettings c;
    c.gesture.durationMs = 100.0;
    engine->setSlideControls (c);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::slideGesture, (juce::uint8) 100), 0);
    engine->processBlock (buffer, m);
    CHECK (engine->getSlideEngine().isGestureRunning());
}

/*  7: "Auto-vibrato engages after 300 ms of position hold." */
LUTHIER_TEST (SlideControls, autoVibratoEngagesAfterAHold)
{
    SlideControlSettings c;
    c.positionSource = ControlSource::modWheel;
    c.autoVibrato = true;
    c.autoVibratoDepthCents = 10.0;
    c.autoVibratoRateHz = 5.0;
    c.speedLimitCentsPerSecond = 1.0e6;
    Bar bar (c);

    bar.cc (1, 64);
    bar.advance (0.02);

    double early = 0.0, late = 0.0;

    for (int i = 0; i < 750; ++i)   // 1 s of 64-sample blocks
    {
        bar.advance (64.0 / kSr);
        const double t = (i + 1) * 64.0 / kSr;
        const double v = std::abs (bar.slide.getControlVibratoCents());

        if (t < 0.28)
            early = juce::jmax (early, v);
        else if (t > 0.45)
            late = juce::jmax (late, v);
    }

    CHECK_MSG (early < 1.0e-9, "vibrato before the hold: " + juce::String (early, 3));
    CHECK_MSG (late > 9.0, "auto-vibrato reached " + juce::String (late, 2) + " cents");

    // Off by default.
    Bar off;
    off.advance (1.0);
    CHECK (off.slide.getControlVibratoCents() == 0.0);
}

/*  7: "Position source swap mid-play does not click (crossfade over 10 ms)."
    The bar glides to the new source's position rather than jumping. */
LUTHIER_TEST (SlideControls, swappingTheSourceGlides)
{
    SlideControlSettings c;
    c.positionSource = ControlSource::modWheel;
    c.speedLimitCentsPerSecond = 1.0e6;
    Bar bar (c);

    bar.cc (1, 32);
    bar.cc (11, 110);
    const double from = bar.advance (0.1, 32);

    c.positionSource = ControlSource::expression;
    bar.slide.setControls (c);

    const double firstStep = bar.advance (32.0 / kSr, 32) - from;
    const double total = 110.0 / 127.0 * 24.0 - from;

    CHECK_MSG (std::abs (firstStep) < 0.3 * std::abs (total), "the first 0.7 ms moved " + juce::String (firstStep / total * 100.0, 1) + " % of the way");
    CHECK_NEAR (bar.advance (0.1, 32), 110.0 / 127.0 * 24.0, 0.05);   // ten time constants
}

/*  7: "Partial string mask: bass-only slide leaves treble strings freely
    pluckable." */
LUTHIER_TEST (SlideControls, aBassOnlyBarLeavesTheTrebleFree)
{
    CHECK (slideContactMaskFor (0, 6) == 0);
    CHECK (slideContactMaskFor (1, 6) == 0x38);   // bass 3: indices 3-5
    CHECK (slideContactMaskFor (2, 6) == 0x07);   // treble 3: indices 0-2

    auto engine = std::make_unique<LuthierEngine>();
    engine->prepare (kSr, 256);

    SlideSettings s;
    s.enabled = true;
    s.mode = SlideMode::bottleneck;
    engine->setSlideSettings (s);

    SlideControlSettings c;
    c.contactMask = slideContactMaskFor (1, 6);
    engine->setSlideControls (c);

    for (int string : { 0, 5 })
    {
        NoteOnEvent e;
        e.stringIndex = string;
        e.fretPosition = 7.0;
        e.pitchHz = engine->getTuningEngine().computeFrequency (string, 7.0, 0.0);
        e.technique = Technique::SlideGuitar;
        engine->triggerNoteNow (e);
    }

    CHECK (! engine->getSlideEngine().isUnderBar (0));
    CHECK (engine->getSlideEngine().isUnderBar (5));
    CHECK (engine->getString (0).hasSounded());   // plucked as a fretted note
}

/*  Through the engine: Slide Mode on, the mod wheel drives the bar, and the
    string under it sounds where the bar is. */
LUTHIER_TEST (SlideControls, theStringFollowsTheControlledBar)
{
    auto engine = std::make_unique<LuthierEngine>();
    engine->prepare (kSr, 256);

    SlideSettings s;
    s.enabled = true;
    s.mode = SlideMode::bottleneck;
    s.intonationAssist = 0.0;
    engine->setSlideSettings (s);

    SlideControlSettings c;
    c.positionSource = ControlSource::modWheel;
    c.speedLimitCentsPerSecond = 1.0e6;
    engine->setSlideControls (c);

    NoteOnEvent e;
    e.stringIndex = 1;
    e.fretPosition = 3.0;
    e.pitchHz = engine->getTuningEngine().computeFrequency (1, 3.0, 0.0);
    e.technique = Technique::SlideGuitar;
    engine->triggerNoteNow (e);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::controllerEvent (1, 1, 64), 0);

    for (int b = 0; b < 40; ++b)
    {
        engine->processBlock (buffer, m);
        m.clear();
    }

    const double expectedFret = 64.0 / 127.0 * 24.0;
    const double expected = engine->getTuningEngine().computeFrequency (1, expectedFret, 0.0);
    const double error = 1200.0 * std::log2 (engine->getStringFrequency (1) / expected);

    // Within the bar's own vibrato and the magnet's pull: a few cents.
    CHECK_MSG (std::abs (error) < 15.0, "the string is " + juce::String (error, 1) + " cents from the bar");
}

/*  7: "Preset save / restore round-trips every added control." */
LUTHIER_TEST (SlideControls, everyControlRoundTrips)
{
    const std::pair<const char*, float> values[] =
    {
        { ParamIDs::slidePosSource, 3.0f }, { ParamIDs::slidePosCc, 30.0f }, { ParamIDs::slidePosMode, 1.0f },
        { ParamIDs::slidePosRange, 7.0f }, { ParamIDs::slideSlantSource, 7.0f }, { ParamIDs::slideSlantCc, 31.0f },
        { ParamIDs::slidePressureSource, 8.0f }, { ParamIDs::slidePressureCc, 32.0f }, { ParamIDs::slideContact, 2.0f },
        { ParamIDs::slideSpeedLimit, 2400.0f }, { ParamIDs::slideAutoVibrato, 1.0f }, { ParamIDs::slideAutoVibDepth, 22.0f },
        { ParamIDs::slideAutoVibRate, 7.0f }, { ParamIDs::slideGestureTrigger, 1.0f }, { ParamIDs::slideGestureCc, 33.0f },
        { ParamIDs::slideGestureFrom, 2.0f }, { ParamIDs::slideGestureTo, 14.0f }, { ParamIDs::slideGestureTime, 900.0f },
        { ParamIDs::slideGestureCurve, 1.0f }, { ParamIDs::slideGestureSlantStart, -8.0f }, { ParamIDs::slideGestureSlantEnd, 6.0f },
        { ParamIDs::slideGesturePressure, 0.3f },
    };

    auto source = std::make_unique<LuthierAudioProcessor>();
    source->prepareToPlay (kSr, 256);

    for (auto [id, v] : values)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (source->getState().getParameter (id));
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    juce::MemoryBlock state;
    source->getStateInformation (state);

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->prepareToPlay (kSr, 256);
    restored->setStateInformation (state.getData(), (int) state.getSize());
    restored->getParameterBridge().applyAllNow();

    for (auto [id, v] : values)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (restored->getState().getParameter (id));
        CHECK_MSG (std::abs (p->convertFrom0to1 (p->getValue()) - v) < 0.01f * juce::jmax (1.0f, std::abs (v)),
                   juce::String (id) + " did not round-trip");
    }

    const auto& c = restored->getEngine().getSlideEngine().getControls();
    CHECK (c.positionSource == ControlSource::mpeY && c.relative && c.pressureSource == ControlSource::mpeZ
           && c.slantSource == ControlSource::aftertouch && c.autoVibrato && c.gestureOnCc
           && c.gesture.curve == SlideCurve::easeIn);
    CHECK_NEAR (c.speedLimitCentsPerSecond, 2400.0, 1.0);
    CHECK (c.contactMask == 0x07);
}
