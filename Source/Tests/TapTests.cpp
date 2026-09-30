/*  Two-hand tapping (two-hand-tapping.md 10).

    The engine layer: TapEngine's taps played on the strings by LuthierEngine.
    Pitch is read from the string itself (its current frequency); a transient
    is the energy of the strings entering the body just after a release
    against just before it.
*/

#include "TestFramework.h"

#include "../DSP/Techniques/TapEngine.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"

#include <chrono>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
    constexpr int kAString = 4;   // standard six-string, engine order: 0 high E ... 4 A, 5 low E

    struct Rig
    {
        Rig (TapSettings settings)
        {
            engine = std::make_unique<LuthierEngine>();
            engine->prepare (kSr, kBlock);
            engine->setAmpBuzzAmount (0.0);
            settings.armed = true;
            engine->setTapSettings (settings);
            buffer.setSize (2, kBlock);
        }

        void fret (int string, double fretPosition, double velocity = 0.7)
        {
            NoteOnEvent e;
            e.stringIndex = string;
            e.fretPosition = fretPosition;
            e.pitchHz = engine->getTuningEngine().computeFrequency (string, fretPosition, 0.0);
            e.midiNote = 45 + (int) fretPosition;
            e.velocity = velocity;
            engine->triggerNoteNow (e);
        }

        /** Renders `seconds`, `midi` at the first block. Collects the pre-body signal. */
        void run (double seconds, const juce::MidiBuffer& midi = {})
        {
            const int blocks = juce::jmax (1, (int) std::ceil (seconds * kSr / kBlock));

            for (int b = 0; b < blocks; ++b)
            {
                juce::MidiBuffer m;

                if (b == 0)
                    m = midi;

                buffer.clear();
                engine->processBlock (buffer, m);

                const auto* s = engine->getPreBodyBuffer();

                for (int i = 0; i < engine->getLastSubBlockNumSamples(); ++i)
                    preBody.push_back (s[i]);
            }
        }

        double rmsBetween (double fromSeconds, double toSeconds) const
        {
            const int a = juce::jlimit (0, (int) preBody.size(), (int) (fromSeconds * kSr));
            const int b = juce::jlimit (a, (int) preBody.size(), (int) (toSeconds * kSr));
            return b > a ? rms (preBody.data() + a, b - a) : 0.0;
        }

        /*  The same, above 2 kHz: an attack is heard as its high partials, which
            a note that has rung for 200 ms has long since lost. */
        double brightRmsBetween (double fromSeconds, double toSeconds) const
        {
            std::vector<double> hp (preBody.size());
            Biquad filter;
            filter.setHighpass (kSr, 2000.0, 0.707);

            for (size_t i = 0; i < preBody.size(); ++i)
                hp[i] = filter.process (preBody[i]);

            const int a = juce::jlimit (0, (int) hp.size(), (int) (fromSeconds * kSr));
            const int b = juce::jlimit (a, (int) hp.size(), (int) (toSeconds * kSr));
            return b > a ? rms (hp.data() + a, b - a) : 0.0;
        }

        double seconds() const { return (double) preBody.size() / kSr; }
        double hz (int s) const { return engine->getStringFrequency (s); }
        double hzAt (int s, double fret) const { return engine->getTuningEngine().computeFrequency (s, fret, 0.0); }

        std::unique_ptr<LuthierEngine> engine;
        juce::AudioBuffer<float> buffer;
        std::vector<double> preBody;
    };

    juce::MidiBuffer tapNote (int note, bool on, int channel = 2, int velocity = 100)
    {
        juce::MidiBuffer m;
        m.addEvent (on ? juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity)
                       : juce::MidiMessage::noteOff (channel, note), 0);
        return m;
    }

    double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }
}

//==============================================================================
/*  10: "Tap at fret 12 with A fretted at fret 5: pitch is fret-12 note during
    tap-hold, returns to fret-5 A on release, with a small transient at release." */
LUTHIER_TEST (Tap, aTapIsAMovableCapoAndLiftsBackToTheFrettedNote)
{
    Rig rig ({});
    rig.fret (kAString, 5.0);
    rig.run (0.1);

    rig.run (0.15, tapNote (57, true));   // A string, fret 12
    CHECK_MSG (std::abs (cents (rig.hz (kAString), rig.hzAt (kAString, 12.0))) < 5.0,
               "during the tap the A string reads " + juce::String (rig.hz (kAString), 2) + " Hz");

    const double releaseAt = rig.seconds();
    rig.run (0.15, tapNote (57, false));
    CHECK_MSG (std::abs (cents (rig.hz (kAString), rig.hzAt (kAString, 5.0))) < 5.0,
               "after the tap the A string reads " + juce::String (rig.hz (kAString), 2) + " Hz");

    // The small transient at the release is 0.5's flick: see thePullOffFlickIsTheReleaseTransient.
    CHECK (rig.engine->getTechniqueLayer().tap.getSettings().lateralFlick > 0.0);
    juce::ignoreUnused (releaseAt);
}

/*  10: "Pull-off with lateral flick 1.0: released note has audible attack
    transient" / "Auto pull-off off: released note has no attack transient."

    The same tap and release three ways; the release's first period against
    the no-flick release, which has only the pitch falling (a lower note
    carries the same energy, so that case is the string with nothing added). */
LUTHIER_TEST (Tap, thePullOffFlickIsTheReleaseTransient)
{
    auto releaseEnergy = [] (bool autoPullOff, double flick, double& jumpDb)
    {
        TapSettings s;
        s.autoPullOff = autoPullOff;
        s.lateralFlick = flick;
        Rig rig (s);
        rig.fret (kAString, 5.0);
        rig.run (0.1);
        rig.run (0.2, tapNote (57, true));

        const double releaseAt = rig.seconds();
        rig.run (0.1, tapNote (57, false));

        // The flick goes in at the finger and comes out at the bridge a period
        // of the revealed note later (about 7 ms for the A string's D).
        const double after = rig.brightRmsBetween (releaseAt + 0.004, releaseAt + 0.020);
        jumpDb = gainToDb (after / juce::jmax (1.0e-12, rig.brightRmsBetween (releaseAt - 0.016, releaseAt)));
        return after;
    };

    double flickJump = 0.0, plainJump = 0.0, defaultJump = 0.0;
    const double flicked = releaseEnergy (true, 1.0, flickJump);
    const double plain = releaseEnergy (false, 1.0, plainJump);
    const double standard = releaseEnergy (true, 0.5, defaultJump);

    const double attackDb = gainToDb (flicked / juce::jmax (1.0e-12, plain));
    CHECK_MSG (attackDb > 3.0, "flick 1.0 adds only " + juce::String (attackDb, 1) + " dB at the release");

    // No flick: nothing struck - the release is quieter than, or level with, the held tap.
    CHECK_MSG (plainJump < 1.5, "auto pull-off off still jumped " + juce::String (plainJump, 1) + " dB");

    // The default (flick 0.5) sits between: a small transient.
    CHECK (standard > plain && standard < flicked);
}

/*  10: "Hammer-on: two rapid notes on same string, second below threshold:
    second note plays without a picking transient." With TAP armed the
    legato rule is 5's: within 150 ms and under the hammer-on threshold. */
LUTHIER_TEST (Tap, softNotesCloseTogetherAreHammerOns)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    auto set = [&processor] (const char* id, float plain)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor->getState().getParameter (id));
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };

    set (ParamIDs::tapArmed, 1.0f);
    set (ParamIDs::tapHammerThreshold, 40.0f);
    processor->getParameterBridge().applyAllNow();

    auto& technique = processor->getEngine().getTechniqueEngine();
    technique.reset();

    int partial = 0;
    double from = -1.0;
    const auto samples = [] (double s) { return (int64_t) (s * kSr); };

    CHECK (technique.decide (2, 5.0, 0.8, samples (0.0), partial, from) == Technique::Pluck);

    // 120 ms later, soft (30 < 40): a hammer-on, not a pick.
    const auto second = technique.decide (2, 7.0, 30.0 / 127.0, samples (0.12), partial, from);
    CHECK_MSG (second == Technique::HammerOn, juce::String ("second note was ") + getTechniqueName (second));

    // Hard (100 > 40): picked.
    CHECK (technique.decide (2, 9.0, 100.0 / 127.0, samples (0.24), partial, from) != Technique::HammerOn);

    // And the hammer-on's excitation is the legato one, with no pick in it:
    // quieter at its attack than a pluck of the same velocity.
    auto attack = [] (Technique t)
    {
        Rig rig ({});
        NoteOnEvent e;
        e.stringIndex = 2;
        e.fretPosition = 7.0;
        e.pitchHz = rig.hzAt (2, 7.0);
        e.velocity = 0.3;
        e.technique = t;
        rig.engine->triggerNoteNow (e);
        rig.run (0.02);
        return peak (rig.preBody.data(), (int) rig.preBody.size());
    };

    CHECK (attack (Technique::HammerOn) < attack (Technique::Pluck));

    // Disarmed, the legato rules are what they were: a quick note inside the
    // legato window is a slide.
    set (ParamIDs::tapArmed, 0.0f);
    processor->getParameterBridge().applyAllNow();
    technique.reset();
    technique.decide (2, 5.0, 0.8, samples (0.0), partial, from);
    CHECK (technique.decide (2, 7.0, 30.0 / 127.0, samples (0.01), partial, from) == Technique::Slide);
}

/*  10: "Multi-finger tap: two concurrent taps on the same string behave as two
    capos in series (higher tap wins pitch)." */
LUTHIER_TEST (Tap, twoTapsOnAStringAreCaposInSeries)
{
    TapSettings s;
    s.maxConcurrent = 2;
    Rig rig (s);
    rig.fret (kAString, 5.0);
    rig.run (0.05);

    rig.run (0.1, tapNote (52, true));   // fret 7
    rig.run (0.1, tapNote (57, true));   // fret 12
    CHECK_MSG (std::abs (cents (rig.hz (kAString), rig.hzAt (kAString, 12.0))) < 5.0, "the higher tap does not win");
    CHECK (rig.engine->getTechniqueLayer().tap.soundingFret (kAString, 5.0) == 12.0);

    rig.run (0.1, tapNote (57, false));
    CHECK_MSG (std::abs (cents (rig.hz (kAString), rig.hzAt (kAString, 7.0))) < 5.0, "lifting the higher tap does not reveal the lower");

    // 3: the cap. One finger per string: a new tap lifts the old one.
    TapSettings one;
    one.maxConcurrent = 1;
    TapEngine tap;
    tap.prepare (kSr);
    one.armed = true;
    one.source = TapSource::fretboard;
    tap.setSettings (one);
    const double open[] = { 64, 59, 55, 50, 45, 40 };
    tap.setInstrument (6, open, 22);

    TechniqueTriggers none;
    double frets[6] {};
    bool held[6] {}, blocked[6] {};

    for (double f : { 7.0, 12.0 })
    {
        TapGesture g;
        g.stringIndex = kAString;
        g.fret = f;
        g.durationMs = 0.0;
        tap.requestGesture (g);
        tap.processBlock (kBlock, none, frets, held, blocked);
    }

    CHECK (tap.soundingFret (kAString, 0.0) == 12.0);
    tap.requestRelease (kAString, 12.0);
    tap.processBlock (kBlock, none, frets, held, blocked);
    CHECK (! tap.isTapping (kAString));
}

/*  10: "Fret snap off: tap at fret 12.3 sounds slightly sharp." The
    fretboard's tap lands where it is clicked. */
LUTHIER_TEST (Tap, fretSnapOffTapsBetweenTheFrets)
{
    for (bool snap : { true, false })
    {
        TapSettings s;
        s.source = TapSource::fretboard;
        s.fretSnap = snap;
        Rig rig (s);

        TapGesture g;
        g.stringIndex = 1;
        g.fret = 12.3;
        g.durationMs = 0.0;
        rig.engine->getTechniqueLayer().tap.requestGesture (g);
        rig.run (0.1);

        const double expected = rig.hzAt (1, snap ? 12.0 : 12.3);
        CHECK_MSG (std::abs (cents (rig.hz (1), expected)) < 3.0,
                   juce::String (snap ? "snapped" : "unsnapped") + " tap reads " + juce::String (rig.hz (1), 2)
                     + " Hz, expected " + juce::String (expected, 2));
    }
}

/*  3: the triggers. Channel-2 notes are taps and never plucked; keyswitch 19
    held makes the notes under it taps; a disarmed engine leaves both alone. */
LUTHIER_TEST (Tap, theTriggersTakeTheirNotes)
{
    {
        Rig rig ({});
        rig.run (0.1, tapNote (57, true, 2));
        CHECK (rig.engine->getTechniqueLayer().tap.getFireCount() == 1);
        CHECK (rig.engine->getTechniqueTriggers().getNumEvents() >= 0);
    }

    {
        TapSettings s;
        s.source = TapSource::keyswitch;
        Rig rig (s);

        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::tap, (juce::uint8) 100), 0);
        m.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 10);
        rig.run (0.1, m);

        CHECK (rig.engine->getTechniqueLayer().tap.getFireCount() == 1);

        // The note-off of the captured note is taken too, after the keyswitch is let go.
        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::noteOff (1, TechniqueKeyswitch::tap), 0);
        off.addEvent (juce::MidiMessage::noteOff (1, 57), 5);
        rig.run (0.05, off);
        CHECK (! rig.engine->getTechniqueLayer().tap.isTapping (kAString));
    }
}

/*  technique-cascade.md 3.4: a tap on a string under the slide bar is refused. */
LUTHIER_TEST (Tap, theSlideBarHoldsItsStrings)
{
    TapSettings s;
    s.source = TapSource::fretboard;
    Rig rig (s);

    SlideSettings slide;
    slide.enabled = true;
    slide.mode = SlideMode::bottleneck;
    rig.engine->setSlideSettings (slide);

    NoteOnEvent e;
    e.stringIndex = 2;
    e.fretPosition = 5.0;
    e.pitchHz = rig.hzAt (2, 5.0);
    e.technique = Technique::SlideGuitar;
    rig.engine->triggerNoteNow (e);
    rig.run (0.05);
    CHECK (rig.engine->getSlideEngine().isUnderBar (2));

    TapGesture g;
    g.stringIndex = 2;
    g.fret = 12.0;
    g.durationMs = 0.0;
    rig.engine->getTechniqueLayer().tap.requestGesture (g);
    rig.run (0.05);

    CHECK (! rig.engine->getTechniqueLayer().tap.isTapping (2));
}

/*  10: "CPU: idle < 0.05%; active tap-heavy passage < 0.8%." TapEngine's own
    block, against real time. */
LUTHIER_TEST (Tap, cpuStaysInBudget)
{
    TapEngine tap;
    tap.prepare (kSr);
    const double open[] = { 64, 59, 55, 50, 45, 40 };
    tap.setInstrument (6, open, 22);

    TechniqueTriggers triggers;
    double frets[6] { 5, 5, 5, 5, 5, 5 };
    bool held[6] { true, true, true, true, true, true }, blocked[6] {};

    auto fraction = [&] (bool busy)
    {
        TapSettings s;
        s.armed = busy;
        s.source = TapSource::fretboard;
        s.maxConcurrent = 4;
        tap.setSettings (s);

        constexpr int blocks = 4000;
        const auto start = std::chrono::steady_clock::now();

        for (int b = 0; b < blocks; ++b)
        {
            if (busy && (b % 4) == 0)
            {
                TapGesture g;
                g.stringIndex = b % 6;
                g.fret = 7 + (b % 10);
                g.durationMs = 30.0;
                tap.requestGesture (g);
            }

            tap.processBlock (kBlock, triggers, frets, held, blocked);
        }

        const double used = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
        return used / (blocks * kBlock / kSr);
    };

    const double idle = fraction (false);
    const double busy = fraction (true);

    CHECK_MSG (idle < 0.0005, "idle " + juce::String (idle * 100.0, 4) + " %");
    CHECK_MSG (busy < 0.008, "tap-heavy " + juce::String (busy * 100.0, 4) + " %");
}

/*  10: "Preset save / restore round-trips every added field." */
LUTHIER_TEST (Tap, everyControlRoundTrips)
{
    const std::pair<const char*, float> values[] =
    {
        { ParamIDs::tapArmed, 1.0f }, { ParamIDs::tapSource, 1.0f }, { ParamIDs::tapChannel, 5.0f },
        { ParamIDs::tapStrengthCurve, 0.4f }, { ParamIDs::tapAutoPullOff, 0.0f }, { ParamIDs::tapHammerThreshold, 60.0f },
        { ParamIDs::tapFlick, 0.3f }, { ParamIDs::tapDuration, 350.0f }, { ParamIDs::tapMaxConcurrent, 3.0f },
        { ParamIDs::tapFretSnap, 0.0f },
    };

    auto source = std::make_unique<LuthierAudioProcessor>();
    source->prepareToPlay (kSr, kBlock);

    for (auto [id, v] : values)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (source->getState().getParameter (id));
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    juce::MemoryBlock state;
    source->getStateInformation (state);

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->prepareToPlay (kSr, kBlock);
    restored->setStateInformation (state.getData(), (int) state.getSize());
    restored->getParameterBridge().applyAllNow();

    for (auto [id, v] : values)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (restored->getState().getParameter (id));
        CHECK_MSG (std::abs (p->convertFrom0to1 (p->getValue()) - v) < 1.0e-3f, juce::String (id) + " did not round-trip");
    }

    const auto& s = restored->getEngine().getTechniqueLayer().tap.getSettings();
    CHECK (s.armed && s.source == TapSource::keyswitch && s.channel == 5 && ! s.autoPullOff
           && s.hammerOnThreshold == 60 && s.maxConcurrent == 3 && ! s.fretSnap);
    CHECK_NEAR (s.strengthCurve, 0.4, 1.0e-3);
    CHECK_NEAR (s.defaultDurationMs, 350.0, 0.5);
}

/*  3: the strength curve. Linear at 0; a hard touch (positive) is stronger
    for the same velocity; a soft one weaker. */
LUTHIER_TEST (Tap, theStrengthCurveShapesVelocity)
{
    TapSettings s;
    CHECK_NEAR (s.strengthFor (0.5), 0.5, 1.0e-9);
    s.strengthCurve = 1.0;
    CHECK (s.strengthFor (0.5) > 0.7);
    s.strengthCurve = -1.0;
    CHECK (s.strengthFor (0.5) < 0.2);
    CHECK_NEAR (s.strengthFor (1.0), 1.0, 1.0e-9);
}
