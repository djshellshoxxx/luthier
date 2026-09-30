/*  Microtonal bends (microtonal-bends.md 9).

    Through the engine: notes started on named strings, controllers sent as
    MIDI, the pitch read from each string's own frequency. Quantise is checked
    on the bend the engine settled on (BendEngine::getLiveCents) against the
    grid, and that bend against the string's frequency.
*/

#include "TestFramework.h"

#include "../DSP/Techniques/BendEngine.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }

    struct Rig
    {
        explicit Rig (BendSettings s)
        {
            engine = std::make_unique<LuthierEngine>();
            engine->prepare (kSr, kBlock);
            s.armed = true;
            engine->setBendSettings (s);
            buffer.setSize (2, kBlock);
        }

        void note (int string, double fret, double velocity = 0.7, int channel = 1)
        {
            NoteOnEvent e;
            e.stringIndex = string;
            e.fretPosition = fret;
            e.pitchHz = engine->getTuningEngine().computeFrequency (string, fret, 0.0);
            e.midiNote = 40 + (int) fret;
            e.velocity = velocity;
            e.midiChannel = channel;
            engine->triggerNoteNow (e);
        }

        void run (double seconds, const juce::MidiBuffer& midi = {}, std::function<void()> perBlock = {})
        {
            const int blocks = juce::jmax (1, (int) std::ceil (seconds * kSr / kBlock));

            for (int b = 0; b < blocks; ++b)
            {
                juce::MidiBuffer m;

                if (b == 0)
                    m = midi;

                buffer.clear();
                engine->processBlock (buffer, m);

                if (perBlock)
                    perBlock();
            }
        }

        double hz (int s) const { return engine->getStringFrequency (s); }

        /** What the string sounds with no bend: the engine's own frequency path at zero bend. */
        double unbent (int s, double fret) const { return engine->getTuningEngine().computeFrequency (s, fret, 0.0); }

        double live (int s) const { return engine->getTechniqueLayer().bend.getLiveCents (s); }

        std::unique_ptr<LuthierEngine> engine;
        juce::AudioBuffer<float> buffer;
    };

    juce::MidiBuffer pitchWheel (double bipolar, int channel = 1)
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::pitchWheel (channel, juce::jlimit (0, 16383, 8192 + (int) std::round (bipolar * 8191.0))), 0);
        return m;
    }

    juce::MidiBuffer controller (int cc, int value, int channel = 1)
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::controllerEvent (channel, cc, value), 0);
        return m;
    }

    BendSettings plain()
    {
        BendSettings s;
        s.vibratoSource = VibratoSource::off;
        s.stringSource = StringBendSource::none;
        return s;
    }

    double midiCents (double hz) { return 6900.0 + 1200.0 * std::log2 (hz / 440.0); }
}

//==============================================================================
/*  9: "Global bend at +100 cents: every ringing note reads +100 cents." */
LUTHIER_TEST (Bend, theGlobalBendMovesEveryString)
{
    Rig rig (plain());

    for (int s : { 0, 2, 5 })
        rig.note (s, 3.0);

    rig.run (0.05);
    std::array<double, 6> before {};

    for (int s : { 0, 2, 5 })
        before[(size_t) s] = rig.hz (s);

    rig.run (0.1, pitchWheel (0.5));   // half of a 200-cent range

    for (int s : { 0, 2, 5 })
        CHECK_MSG (std::abs (cents (rig.hz (s), before[(size_t) s]) - 100.0) < 1.0,
                   "string " + juce::String (s) + " moved " + juce::String (cents (rig.hz (s), before[(size_t) s]), 2) + " cents");
}

/*  9: "Per-string bend on string 3 at +200 cents: only string 3 shifts;
    others unchanged." String 3 is engine index 2; its custom CC is base + 2. */
LUTHIER_TEST (Bend, aPerStringBendMovesOnlyItsString)
{
    auto s = plain();
    s.stringSource = StringBendSource::customCc;
    s.stringCcBase = 21;
    Rig rig (s);

    for (int string = 0; string < 6; ++string)
        rig.note (string, 2.0);

    rig.run (0.05);
    std::array<double, 6> before {};

    for (int string = 0; string < 6; ++string)
        before[(size_t) string] = rig.hz (string);

    rig.run (0.1, controller (23, 127));

    for (int string = 0; string < 6; ++string)
    {
        const double moved = cents (rig.hz (string), before[(size_t) string]);
        CHECK_MSG (std::abs (moved - (string == 2 ? 200.0 : 0.0)) < 1.0,
                   "string " + juce::String (string + 1) + " moved " + juce::String (moved, 2) + " cents");
    }

    // MPE: each note's own pitch bend moves only its string.
    auto mpe = plain();
    mpe.stringSource = StringBendSource::mpePitchBend;
    Rig rig2 (mpe);
    rig2.note (0, 5.0, 0.7, 2);
    rig2.note (1, 5.0, 0.7, 3);
    rig2.run (0.05);
    const double s0 = rig2.hz (0), s1 = rig2.hz (1);
    rig2.run (0.1, pitchWheel (1.0, 3));
    CHECK (std::abs (cents (rig2.hz (0), s0)) < 1.0);
    CHECK (std::abs (cents (rig2.hz (1), s1) - 200.0) < 1.0);
}

/*  9: "Vibrato onset delay: no pitch modulation for the configured delay
    after note-on, then engages." and "Vibrato depth reaches configured
    amplitude within 50 ms of onset." */
LUTHIER_TEST (Bend, vibratoWaitsForItsOnsetThenReachesDepthIn50Ms)
{
    auto s = plain();
    s.vibratoSource = VibratoSource::lfo;
    s.vibratoOnsetMs = 200.0;
    s.vibratoDepthCents = 20.0;
    s.vibratoRateHz = 6.0;
    Rig rig (s);

    rig.note (1, 5.0);
    const double base = rig.unbent (1, 5.0);

    double before = 0.0, after = 0.0, t = 0.0;

    rig.run (0.6, {}, [&]
    {
        t += (double) kBlock / kSr;
        const double dev = std::abs (rig.live (1));

        if (t < 0.195)
            before = juce::jmax (before, dev);
        else if (t > 0.250 && t < 0.600)
            after = juce::jmax (after, dev);
    });

    juce::ignoreUnused (base);
    CHECK_MSG (before < 0.5, "vibrato before its onset: " + juce::String (before, 2) + " cents");
    CHECK_MSG (after > 0.9 * 20.0, "vibrato reached only " + juce::String (after, 2) + " cents within 50 ms of onset");
}

/*  9: "Quarter-tone quantise snap 1.0: every bend held pitch lands on the
    nearest 50-cent step." */
LUTHIER_TEST (Bend, quarterToneQuantiseLandsOnTheGrid)
{
    auto s = plain();
    s.quantise = BendQuantise::quarterTone;
    s.snap = 1.0;
    Rig rig (s);

    rig.note (3, 4.0);
    rig.run (0.05);

    const double base = midiCents (rig.unbent (3, 4.0));

    for (double wheel : { 0.07, 0.15, 0.31, -0.2, 0.62 })
    {
        rig.run (0.08, pitchWheel (wheel));
        const double held = base + rig.live (3);
        const double offGrid = std::abs (held - std::round (held / 50.0) * 50.0);
        CHECK_MSG (offGrid < 0.01, "wheel " + juce::String (wheel) + ": held pitch " + juce::String (offGrid, 3) + " cents off the grid");

        // And the string sounds it.
        CHECK (std::abs (cents (rig.hz (3), rig.unbent (3, 4.0)) - rig.live (3)) < 1.0);
    }

    // Snap 0.3 is only an attraction: part of the way.
    BendSettings light = s;
    light.snap = 0.3;
    light.armed = true;
    rig.engine->setBendSettings (light);
    rig.run (0.08, pitchWheel (0.15));   // 30 cents raw
    const double partial = rig.live (3);
    CHECK_MSG (partial > 30.0 && partial < 50.0, "snap 0.3 bent to " + juce::String (partial, 2));
}

/*  9: "Custom scale .scala: bend pitches match the loaded scale within 0.5 cents." */
LUTHIER_TEST (Bend, aLoadedScaleIsTheGrid)
{
    // Just major on middle C, as a Scala file with a comment and ratios.
    const juce::String scl = "! just.scl\n!\nJust major\n 7\n!\n9/8\n5/4\n4/3\n3/2\n5/3\n15/8\n2/1\n";

    MicrotonalScale scale;
    juce::String error;
    CHECK_MSG (scale.loadScala (scl, error), error);
    CHECK_NEAR (scale.nearest (6000.0 + 390.0), 6000.0 + 1200.0 * std::log2 (5.0 / 4.0), 1.0e-9);
    CHECK_NEAR (scale.nearest (7200.0 + 10.0), 7200.0, 1.0e-9);   // the octave repeats

    // A malformed file is refused with a reason.
    MicrotonalScale bad;
    CHECK (! bad.loadScala ("Just\n 3\n9/8\n", error) && error.isNotEmpty());

    // AnaMark .tun: an absolute pitch per note.
    MicrotonalScale tun;
    CHECK (tun.loadTun ("[Tuning]\nnote 60=6010\nnote 61=6090\nnote 62=6205\n", error));
    CHECK_NEAR (tun.nearest (6100.0), 6090.0, 1.0e-9);

    auto s = plain();
    s.quantise = BendQuantise::custom;
    s.snap = 1.0;
    Rig rig (s);
    rig.engine->getTechniqueLayer().bend.setCustomScale (scale);

    rig.note (2, 5.0);
    rig.run (0.05);
    const double base = midiCents (rig.unbent (2, 5.0));

    for (double wheel : { 0.1, 0.3, 0.55, 0.8 })
    {
        rig.run (0.08, pitchWheel (wheel));
        const double held = base + rig.live (2);
        CHECK_MSG (std::abs (held - scale.nearest (held)) < 0.5,
                   "held " + juce::String (held, 2) + " is " + juce::String (std::abs (held - scale.nearest (held)), 3)
                     + " cents from the scale");
    }

    // The scale travels with the preset (engine-technique-layer 7).
    const auto block = rig.engine->getTechniqueLayer().toVar();
    TechniqueLayer other;
    other.prepare (kSr);
    other.fromVar (juce::JSON::parse (juce::JSON::toString (block)));
    CHECK (other.bend.getCustomScale().getNumPoints() == scale.getNumPoints());
    CHECK (other.bend.getCustomScale().getName() == "Just major");
}

/*  9: "Pre-bend keyswitch: next note-on starts -200 cents flat, releases to nominal." */
LUTHIER_TEST (Bend, aPreBendStartsFlatAndReleases)
{
    auto s = plain();
    s.preBendCents = -200.0;
    s.preBendReleaseMs = 300.0;
    Rig rig (s);

    juce::MidiBuffer ks;
    ks.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::preBend, (juce::uint8) 100), 0);
    ks.addEvent (juce::MidiMessage::noteOff (1, TechniqueKeyswitch::preBend), 10);
    rig.run (0.01, ks);
    CHECK (rig.engine->getTechniqueLayer().bend.isPreBendArmed());

    // The keyswitch never sounds.
    for (int string = 0; string < 6; ++string)
        CHECK (! rig.engine->getString (string).hasSounded());

    rig.note (1, 7.0);
    rig.run (0.006);
    CHECK_MSG (rig.live (1) < -190.0, "the pre-bent note started at " + juce::String (rig.live (1), 1) + " cents");

    rig.run (0.4);
    CHECK_MSG (std::abs (rig.live (1)) < 0.5, "it released to " + juce::String (rig.live (1), 2) + " cents");
    CHECK (std::abs (cents (rig.hz (1), rig.unbent (1, 7.0))) < 1.0);

    // One note only: the next starts at pitch.
    rig.note (1, 9.0);
    rig.run (0.006);
    CHECK (std::abs (rig.live (1)) < 0.5);
}

/*  9: "Bend range change mid-note does not cause a pitch jump." (4: it takes
    effect at the next note-on.) */
LUTHIER_TEST (Bend, aRangeChangeWaitsForTheNextNote)
{
    auto s = plain();
    s.globalRangeCents = 200.0;
    Rig rig (s);

    rig.note (0, 5.0);
    rig.run (0.05, pitchWheel (1.0));
    CHECK_NEAR (rig.live (0), 200.0, 0.5);

    auto wider = s;
    wider.globalRangeCents = 400.0;
    wider.armed = true;
    rig.engine->setBendSettings (wider);
    rig.run (0.05);
    CHECK_MSG (std::abs (rig.live (0) - 200.0) < 0.5, "the held note jumped to " + juce::String (rig.live (0), 1));

    rig.note (0, 5.0);
    rig.run (0.05);
    CHECK_NEAR (rig.live (0), 400.0, 0.5);
}

/*  9: "Cascade: bend + slap on low E: bent slap event pitches correctly." */
LUTHIER_TEST (Bend, aBentSlapPitchesCorrectly)
{
    auto s = plain();
    Rig rig (s);
    rig.engine->setGuitarType (GuitarType::PrecisionBass);

    SlapSettings slap;
    slap.armed = true;
    slap.trigger = TriggerSource::velocityZone;
    slap.velocityZone = 100;
    rig.engine->setSlapSettings (slap);

    // Wheel first, then a slapped low E.
    rig.run (0.02, pitchWheel (0.5));

    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (1, 28, (juce::uint8) 120), 0);
    rig.run (0.1, m);

    int string = -1;

    for (int i = 0; i < rig.engine->getNumStrings(); ++i)
        if (rig.engine->getString (i).hasSounded())
            string = i;

    CHECK (string >= 0);

    if (string >= 0)
    {
        const double fret = rig.engine->getStringFret (string);
        CHECK_MSG (std::abs (cents (rig.hz (string), rig.unbent (string, fret)) - 100.0) < 2.0,
                   "the slapped E reads " + juce::String (cents (rig.hz (string), rig.unbent (string, fret)), 2) + " cents");
    }
}

/*  Disarmed, the interpreter's bend is what the strings get: exactly as before. */
LUTHIER_TEST (Bend, disarmedIsAsBefore)
{
    auto engine = std::make_unique<LuthierEngine>();
    engine->prepare (kSr, kBlock);
    CHECK (! engine->getTechniqueLayer().bend.isArmed());

    NoteOnEvent e;
    e.stringIndex = 1;
    e.fretPosition = 5.0;
    e.pitchHz = engine->getTuningEngine().computeFrequency (1, 5.0, 0.0);
    engine->triggerNoteNow (e);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer none;

    for (int b = 0; b < 10; ++b)
        engine->processBlock (buffer, none);

    CHECK_NEAR (engine->getTechniqueLayer().bend.getLiveCents (1), 0.0, 1.0e-12);
    CHECK (std::abs (cents (engine->getStringFrequency (1), e.pitchHz)) < 20.0);
}

/*  2's curves: exponential is late (finger mechanics), drawn follows its points. */
LUTHIER_TEST (Bend, theCurvesShapeTheThrow)
{
    BendEngine bend;
    bend.prepare (kSr);

    BendSettings s;
    bend.setSettings (s);
    CHECK_NEAR (bend.shapeBend (0.5), 0.5, 1.0e-12);
    CHECK_NEAR (bend.shapeRelease (0.25), 0.75, 1.0e-12);

    s.bendCurve = BendCurve::exponential;
    bend.setSettings (s);
    CHECK_NEAR (bend.shapeBend (0.5), 0.25, 1.0e-12);
    CHECK_NEAR (bend.shapeBend (-0.5), -0.25, 1.0e-12);

    s.bendCurve = BendCurve::drawn;
    bend.setSettings (s);
    bend.setDrawnCurvePoint (2, 0.9);
    CHECK_NEAR (bend.shapeBend (0.5), 0.9, 1.0e-12);
    CHECK_NEAR (bend.shapeBend (1.0), 1.0, 1.0e-12);
}

/*  9: "Preset save / restore round-trips every added field." */
LUTHIER_TEST (Bend, everyControlRoundTrips)
{
    std::vector<std::pair<juce::String, float>> values =
    {
        { ParamIDs::bendArmed, 1.0f }, { ParamIDs::bendGlobalSource, 2.0f }, { ParamIDs::bendGlobalCc, 30.0f },
        { ParamIDs::bendGlobalRange, 700.0f }, { ParamIDs::bendStringSource, 3.0f }, { ParamIDs::bendStringCc, 40.0f },
        { ParamIDs::bendVibratoSource, 2.0f }, { ParamIDs::bendVibratoRate, 8.0f }, { ParamIDs::bendVibratoDepth, 33.0f },
        { ParamIDs::bendVibratoOnset, 90.0f }, { ParamIDs::bendQuantise, 5.0f }, { ParamIDs::bendSnap, 0.4f },
        { ParamIDs::bendPreBendAmount, 150.0f }, { ParamIDs::bendPreBendTrigger, 1.0f }, { ParamIDs::bendPreBendCc, 50.0f },
        { ParamIDs::bendPreBendRelease, 800.0f }, { ParamIDs::bendCurve, 1.0f }, { ParamIDs::bendReleaseCurve, 2.0f },
    };

    for (int n = 1; n <= 6; ++n)
        values.push_back ({ ParamIDs::bendStringRange (n), 100.0f * (float) n });

    auto source = std::make_unique<LuthierAudioProcessor>();
    source->prepareToPlay (kSr, kBlock);

    for (auto& [id, v] : values)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (source->getState().getParameter (id));
        CHECK_MSG (p != nullptr, id + " is missing");

        if (p != nullptr)
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    source->getEngine().getTechniqueLayer().bend.setDrawnCurvePoint (1, 0.6);

    juce::MemoryBlock state;
    source->getStateInformation (state);

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->prepareToPlay (kSr, kBlock);
    restored->setStateInformation (state.getData(), (int) state.getSize());
    restored->getParameterBridge().applyAllNow();

    for (auto& [id, v] : values)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (restored->getState().getParameter (id)))
            CHECK_MSG (std::abs (p->convertFrom0to1 (p->getValue()) - v) < 0.01f * juce::jmax (1.0f, std::abs (v)),
                       id + " did not round-trip");

    const auto& s = restored->getEngine().getTechniqueLayer().bend.getSettings();
    CHECK (s.armed && s.quantise == BendQuantise::edo31 && s.stringSource == StringBendSource::customCc && s.preBendOnCc);
    CHECK_NEAR (s.stringRangeCents[3], 400.0, 0.5);
    CHECK_NEAR (restored->getEngine().getTechniqueLayer().bend.getDrawnCurvePoint (1), 0.6, 1.0e-9);
}
