/*  Tuning stability (tuning-stability.md 9), group TuningStability.

    Most tests drive the StabilityModel with a TuningEngine directly, block by
    block at 48 kHz and 128 samples, with the string levels, bends and whammy
    it would be fed; the hardware is built the way LuthierEngine builds it
    (StringMaterials, nickel-plated steel, regular gauge, 648 mm). TS-01, TS-12
    and TS-13 run the whole engine; TS-15 the processor.
*/

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Model/Playing/StabilityModel.h"

#include <cstdio>

using namespace luthier;
using namespace luthier::tests;

namespace luthier::tests { long realismCAllocationCount() noexcept; }

namespace
{
    constexpr int kBlock = 128;
    constexpr double kStandard[6] = { 329.628, 246.942, 195.998, 146.832, 110.0, 82.407 };

    using Bridge = WhammyEngine::BridgeType;

    TuningHardware makeHardware (Bridge bridge = Bridge::Fixed, int numStrings = 6)
    {
        TuningHardware hw;
        hw.bridge = bridge;
        hw.numStrings = numStrings;

        const double youngs = StringMaterials::get (StringMaterial::NickelPlatedSteel).youngsModulusPa;

        for (int s = 0; s < numStrings; ++s)
        {
            const double hz = kStandard[s % 6] * (s >= 6 ? 2.0 : 1.0);
            const auto spec = StringMaterials::computeSpec (StringMaterial::NickelPlatedSteel, StringGauge::Regular,
                                                            StringAge::BrokenIn, s % 6, hz, 648.0);
            const double core = spec.coreDiameterMm * 0.001;
            hw.tensionN[(size_t) s] = spec.tensionNewtons;
            hw.eaOverT[(size_t) s] = youngs * constants::kPi * 0.25 * core * core / spec.tensionNewtons;
            hw.openHz[(size_t) s] = hz;
        }

        return hw;
    }

    StabilitySettings physical (double amount = 1.0)
    {
        StabilitySettings s;
        s.amount = amount;
        return s;
    }

    StabilitySettings only (StabilityModel::Cause cause)
    {
        StabilitySettings s;
        s.amount = 1.0;
        s.settling = cause == StabilityModel::settle ? 1.0 : 0.0;
        s.nutBinding = cause == StabilityModel::nut ? 1.0 : 0.0;
        s.backlash = cause == StabilityModel::backlash ? 1.0 : 0.0;
        s.saddleCreep = cause == StabilityModel::bridge ? 1.0 : 0.0;
        s.bendMemory = cause == StabilityModel::memory ? 1.0 : 0.0;
        s.capoBias = cause == StabilityModel::capo ? 1.0 : 0.0;
        s.autoRetune = AutoRetune::off;
        return s;
    }

    struct Rig
    {
        explicit Rig (TuningHardware hardware = makeHardware(), StabilitySettings settings = physical(),
                      double sampleRate = 48000.0, StringAge age = StringAge::BrokenIn)
            : sr (sampleRate), hw (hardware)
        {
            tuning.prepare (sr);
            model.prepare (sr);
            model.setHardware (hw);
            model.setSettings (settings);
            model.setStringAge (age);
            model.reset();
        }

        /** Runs `seconds` of blocks; `perBlock (block)` sets levels, bends and the rest. */
        template <typename PerBlock>
        void run (double seconds, PerBlock perBlock)
        {
            const int blocks = (int) std::llround (seconds * sr / kBlock);

            for (int b = 0; b < blocks; ++b)
            {
                perBlock (b);
                StabilityModel::BlockInput in;
                in.numSamples = kBlock;
                in.levels = levels.data();
                in.bendCents = bends.data();
                in.whammyCents = whammy.data();
                in.capoFret = capo.data();
                in.transportPlaying = playing;
                last = model.advance (in, tuning);
                ++blocksRun;
            }
        }

        void run (double seconds) { run (seconds, [] (int) {}); }

        /** One bend up to `peak` cents and back down, over 0.3 s. */
        void bend (int s, double peak)
        {
            run (0.3, [this, s, peak] (int b) { bends[(size_t) s] = b < 50 ? peak * juce::jmin (1.0, b / 20.0) : 0.0; });
            bends[(size_t) s] = 0.0;
        }

        /** A retuning to `newHz`: the tension follows f^2 and the model hears it. */
        void retuneTo (int s, double newHz)
        {
            const double oldHz = hw.openHz[(size_t) s];
            hw.tensionN[(size_t) s] *= (newHz / oldHz) * (newHz / oldHz);
            hw.eaOverT[(size_t) s] *= (oldHz / newHz) * (oldHz / newHz);
            hw.openHz[(size_t) s] = newHz;
            model.setHardware (hw);
            model.onTuningChanged (s, oldHz, newHz);
        }

        double cents (int s) const { return tuning.getStringTuning (s).stabilityCents; }

        double maxAbs (int from = 0, int to = 6) const
        {
            double m = 0.0;
            for (int s = from; s < to; ++s) m = juce::jmax (m, std::abs (cents (s)));
            return m;
        }

        double sr;
        TuningHardware hw;
        TuningEngine tuning;
        StabilityModel model;
        std::array<double, kMaxStrings> levels {}, bends {}, whammy {};
        std::array<int, kMaxStrings> capo {};
        bool playing = false;
        StabilityModel::BlockOutput last;
        int blocksRun = 0;
    };

    /** A performance through the whole engine: plucks, bends, whammy dives,
        a tuning change and a capo, with a hook to read state per block. */
    template <typename PerBlock>
    std::vector<float> performance (LuthierEngine& engine, double seconds, PerBlock perBlock)
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, kBlock);

        for (int b = 0; b < (int) (seconds * 48000.0 / kBlock); ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            const int step = b % 375;

            if (step == 0)   midi.addEvent (juce::MidiMessage::noteOn (1, 57 + (b / 375) % 5, (juce::uint8) 120), 0);
            if (step == 20)  midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);   // a full bend
            if (step == 90)  midi.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 0);
            if (step == 120) midi.addEvent (juce::MidiMessage::controllerEvent (1, 2, 127), 0);   // whammy
            if (step == 180) midi.addEvent (juce::MidiMessage::controllerEvent (1, 2, 64), 0);
            if (step == 300) midi.addEvent (juce::MidiMessage::noteOff (1, 57 + (b / 375) % 5), 0);

            if (b == 2000) engine.setTuningPreset (TuningPreset::DropD);
            if (b == 4000) engine.getTuningEngine().setCapoFret (2);

            engine.processBlock (buffer, midi);
            perBlock (b);

            for (int i = 0; i < kBlock; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    }
}

//==============================================================================
// TS-01
LUTHIER_TEST (TuningStability, offIsInert)
{
    auto run = [] (bool bypass, bool& allZero)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, kBlock);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.getWhammyEngine().setBridgeType (Bridge::FloydRose);
        engine.setStabilityBypassedForTest (bypass);
        engine.reset();
        allZero = true;

        return performance (engine, 60.0, [&engine, &allZero] (int)
        {
            for (int s = 0; s < engine.getNumStrings(); ++s)
                allZero = allZero && engine.getTuningEngine().getStringTuning (s).stabilityCents == 0.0;
        });
    };

    bool zeroA = false, zeroB = false;
    const auto a = run (false, zeroA), b = run (true, zeroB);
    CHECK_MSG (zeroA, "stabilityCents moved at amount 0");
    CHECK_MSG (a == b, "the audio differs from the build without the model");
}

//==============================================================================
// TS-02
LUTHIER_TEST (TuningStability, nutBinding)
{
    auto after = [] (double friction, Bridge bridge)
    {
        auto hw = makeHardware (bridge);
        hw.nutFriction = friction;
        Rig rig (hw, only (StabilityModel::nut));
        rig.bend (3, 200.0);
        rig.run (0.05);
        return rig.cents (3);
    };

    const double bone = after (0.35, Bridge::Fixed);
    CHECK_NEAR (bone, 2.1, 0.3);
    CHECK_MSG (std::abs (after (0.1, Bridge::Fixed)) <= 0.8, "the graphite nut held too much");
    CHECK_MSG (after (0.35, Bridge::FloydRose) == 0.0, "a Floyd's locking nut held the string");
}

//==============================================================================
// TS-03
LUTHIER_TEST (TuningStability, thePingReleasesTheBind)
{
    auto releasingPluck = [] (juce::uint32 seed, double* afterRelease)
    {
        Rig rig (makeHardware(), only (StabilityModel::nut));
        rig.model.setSeed (seed);
        rig.bend (3, 200.0);

        for (int k = 0; k < 20; ++k)
        {
            rig.model.onPluck (3, 1.0);
            rig.run (0.1);

            if (rig.model.getPingPluckIndex (3) >= 0)
            {
                // Within 25 ms of the releasing pluck the bind is gone.
                Rig probe (makeHardware(), only (StabilityModel::nut));
                probe.model.setSeed (seed);
                probe.bend (3, 200.0);
                for (int j = 0; j < k; ++j) { probe.model.onPluck (3, 1.0); probe.run (0.1); }
                probe.model.onPluck (3, 1.0);
                probe.run (0.025);

                if (afterRelease != nullptr)
                    *afterRelease = std::abs (probe.model.getStuckCents (3));

                return rig.model.getPingPluckIndex (3);
            }
        }

        return -1;
    };

    double residual = 1.0;
    const int first = releasingPluck (0x5EEDu, &residual);
    CHECK_MSG (first > 0 && first <= 20, "no ping within 20 plucks");
    CHECK (releasingPluck (0x5EEDu, nullptr) == first);
    CHECK_MSG (residual < 0.05, "25 ms after the ping " + juce::String (residual, 3) + " c remained");

    bool differs = false;
    for (juce::uint32 seed = 1; seed <= 8; ++seed)
        differs = differs || releasingPluck (seed * 7919u, nullptr) != first;

    CHECK_MSG (differs, "the ping's pluck does not depend on the seed");
}

//==============================================================================
// TS-04
LUTHIER_TEST (TuningStability, backlashNeedsADownwardApproach)
{
    Rig rig (makeHardware(), only (StabilityModel::backlash));
    rig.retuneTo (5, 73.416);   // Standard to Drop D
    rig.run (0.01);
    CHECK (rig.model.isBacklashArmed (5));
    CHECK (! rig.model.isBacklashArmed (4));

    rig.bend (5, 100.0);
    const double b = StabilityModel::backlashCents (rig.hw, 5);
    CHECK_MSG (std::abs (rig.cents (5) + b) <= 0.2 * b,
               "backlash " + juce::String (rig.cents (5), 3) + " c, formula " + juce::String (-b, 3));

    rig.model.requestRetune (1u << 5);
    rig.run (0.01);
    rig.bend (5, 100.0);
    CHECK_MSG (std::abs (rig.cents (5)) < 0.05, "a bend after the retune added " + juce::String (rig.cents (5), 3));
}

//==============================================================================
// TS-05
LUTHIER_TEST (TuningStability, floatingEquilibrium)
{
    Rig floyd (makeHardware (Bridge::FloydRose), only (StabilityModel::bridge));
    floyd.retuneTo (5, 73.416);
    floyd.run (0.01);

    for (int s = 0; s < 5; ++s)
        CHECK_MSG (floyd.cents (s) >= 7.0 && floyd.cents (s) <= 15.0,
                   "string " + juce::String (s) + " moved " + juce::String (floyd.cents (s), 2) + " c");

    Rig hardtail (makeHardware (Bridge::Fixed), only (StabilityModel::bridge));
    hardtail.retuneTo (5, 73.416);
    hardtail.run (0.01);
    CHECK (hardtail.maxAbs (0, 5) < 0.5);

    const double before = floyd.maxAbs();
    floyd.model.requestRetuneAll();
    floyd.run (0.01);
    const double once = floyd.maxAbs();
    floyd.model.requestRetuneAll();
    floyd.run (0.01);
    const double twice = floyd.maxAbs();

    std::printf ("      Floyd after Drop D: %.2f c; one Retune all %.2f c, two %.2f c\n", before, once, twice);
    CHECK (once <= 0.40 * before);
    CHECK (twice <= 0.15 * before);
}

//==============================================================================
// TS-06 (and TS-14's time constant at 44.1 and 96 kHz)
LUTHIER_TEST (TuningStability, creepTimeConstant)
{
    for (double sr : { 48000.0, 44100.0, 96000.0 })
    {
        Rig rig (makeHardware (Bridge::VintageTrem), only (StabilityModel::bridge), sr);
        rig.retuneTo (5, 73.416);
        rig.run (90.0);
        const double at90 = rig.model.getCreepCents (5) / rig.model.getCreepTarget (5);
        rig.run (180.0);
        const double at270 = rig.model.getCreepCents (5) / rig.model.getCreepTarget (5);

        CHECK_MSG (std::abs (at90 - 0.632) <= (sr == 48000.0 ? 0.07 : 0.632 * 0.02),
                   juce::String (sr) + ": creep at 90 s is " + juce::String (at90 * 100.0, 1) + "%");
        CHECK (at270 >= 0.95);
        CHECK (rig.model.getCreepTarget (5) < -5.0);   // Drop D on a vintage trem: ~6 c flat
    }
}

//==============================================================================
// TS-07
LUTHIER_TEST (TuningStability, bendMemory)
{
    auto after = [] (int bends, bool locking)
    {
        auto hw = makeHardware();
        hw.tunerLocking = locking;
        Rig rig (hw, only (StabilityModel::memory), 48000.0, StringAge::Fresh);

        for (int k = 0; k < bends; ++k)
            rig.bend (2, 200.0);

        return rig.cents (2);
    };

    const double twenty = after (20, false);
    CHECK_NEAR (twenty, -9.6, 1.0);
    CHECK (std::abs (after (20, true)) <= 0.35 * std::abs (twenty));
    CHECK (after (50, false) >= -10.0 - 1.0e-9);
}

//==============================================================================
// TS-08
LUTHIER_TEST (TuningStability, settling)
{
    Rig fresh (makeHardware(), only (StabilityModel::settle), 48000.0, StringAge::Fresh);
    fresh.levels[1] = 0.3;
    fresh.run (60.0);
    const double first = fresh.cents (1);
    CHECK_MSG (first <= -3.0 && first >= -8.0, "after a minute " + juce::String (first, 2) + " c");

    fresh.model.requestRetune (1u << 1);
    fresh.run (0.01);
    fresh.run (60.0);
    CHECK_MSG (std::abs (fresh.cents (1)) < 0.40 * std::abs (first), "the second minute drifted as far");

    Rig old (makeHardware(), only (StabilityModel::settle), 48000.0, StringAge::Old);
    old.levels[1] = 0.3;
    old.run (60.0);
    CHECK (std::abs (old.cents (1)) < 0.5);
}

//==============================================================================
// TS-09
LUTHIER_TEST (TuningStability, capoBias)
{
    Rig rig (makeHardware(), only (StabilityModel::capo));
    rig.hw.capoPressure = 0.7;
    rig.hw.capoGapMm = 6.0;
    rig.model.setHardware (rig.hw);
    rig.capo.fill (2);
    rig.run (0.01);

    for (int s = 0; s < 6; ++s)
        CHECK_MSG (rig.cents (s) >= 1.0 && rig.cents (s) <= 12.0, "string " + juce::String (s) + ": " + juce::String (rig.cents (s), 2));

    for (int s : { 2, 5 })
        CHECK (std::abs (rig.cents (s) / StabilityModel::capoBiasCents (rig.hw, s, 2) - 1.0) <= 0.10);

    std::printf ("      trigger capo at 2: high E %.1f, G %.1f, low E %.1f c\n", rig.cents (0), rig.cents (2), rig.cents (5));

    // A partial capo biases only the strings it clamps.
    Rig partial (rig.hw, only (StabilityModel::capo));
    partial.capo = { 0, 0, 2, 2, 2, 0 };
    partial.run (0.01);
    CHECK (partial.cents (0) == 0.0 && partial.cents (5) == 0.0 && partial.cents (3) > 1.0);

    // Retuned with the capo on, then the capo comes off: flat by the bias.
    std::array<double, 6> bias {};
    for (int s = 0; s < 6; ++s) bias[(size_t) s] = rig.cents (s);
    rig.model.requestRetuneAll();
    rig.run (0.01);
    rig.capo.fill (0);
    rig.run (0.01);

    for (int s = 0; s < 6; ++s)
        CHECK_NEAR (rig.cents (s), -bias[(size_t) s], 0.2);
}

//==============================================================================
// TS-10
LUTHIER_TEST (TuningStability, retuneScope)
{
    Rig rig (makeHardware(), only (StabilityModel::nut));

    for (int s = 0; s < 6; ++s)
        rig.bend (s, 200.0);

    std::array<double, 6> before {};
    for (int s = 0; s < 6; ++s) before[(size_t) s] = rig.cents (s);

    rig.model.requestRetune (1u << 2);
    rig.run (0.01);

    for (int s = 0; s < 6; ++s)
    {
        if (s == 2)
            CHECK (rig.cents (s) == 0.0);
        else
            CHECK (std::abs (rig.cents (s) - before[(size_t) s]) <= 1.0e-9);
    }

    // Retune all through the engine: every string, and the character engine's drift too.
    LuthierEngine engine;
    engine.prepare (48000.0, kBlock);
    engine.setGuitarType (GuitarType::Stratocaster);
    engine.getCharacterEngine().setEnabled (true);
    engine.getCharacterEngine().setTunerLooseness (100.0);
    engine.getStabilityModel().setSettings (physical());
    engine.reset();

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;

    for (int b = 0; b < 3000; ++b)
        engine.processBlock (buffer, midi);

    engine.getStabilityModel().requestRetuneAll();
    engine.processBlock (buffer, midi);

    for (int s = 0; s < engine.getNumStrings(); ++s)
    {
        CHECK (engine.getTuningEngine().getStringTuning (s).stabilityCents == 0.0);
        CHECK_MSG (engine.getCharacterEngine().getTunerDriftCents (s) == 0.0, "character drift survived Retune all");
    }
}

//==============================================================================
// TS-11 (and TS-14's idle time at 44.1 and 96 kHz)
LUTHIER_TEST (TuningStability, autoRetuneOnIdle)
{
    for (double sr : { 48000.0, 44100.0, 96000.0 })
    {
        Rig rig (makeHardware(), only (StabilityModel::nut), sr);
        auto settings = only (StabilityModel::nut);
        settings.autoRetune = AutoRetune::idle;
        rig.model.setSettings (settings);
        rig.bend (3, 200.0);

        const int start = rig.blocksRun;
        int firedAt = -1;

        rig.run (12.0, [&] (int)
        {
            if (firedAt < 0 && rig.model.getAutoRetuneCount() > 0)
                firedAt = rig.blocksRun;
        });

        // The idle clock started with the last bend block (silent all along).
        const double seconds = (firedAt - start) * kBlock / sr + 0.3;
        const double tolerance = sr == 48000.0 ? 0.2 : 10.0 * 0.02;
        CHECK_MSG (firedAt > 0 && std::abs (seconds - 10.0) <= tolerance + 0.3,
                   juce::String (sr) + ": fired at " + juce::String (seconds, 3) + " s");
        CHECK (rig.cents (3) == 0.0);
    }

    // A string ringing: never.
    {
        Rig rig (makeHardware(), only (StabilityModel::nut));
        auto settings = only (StabilityModel::nut);
        settings.autoRetune = AutoRetune::idle;
        rig.model.setSettings (settings);
        rig.bend (3, 200.0);
        rig.levels[0] = 0.01;
        rig.run (20.0);
        CHECK (rig.model.getAutoRetuneCount() == 0);
    }

    // Off: never.
    {
        Rig rig (makeHardware(), only (StabilityModel::nut));
        rig.bend (3, 200.0);
        rig.run (20.0);
        CHECK (rig.model.getAutoRetuneCount() == 0);
    }
}

//==============================================================================
// TS-12
LUTHIER_TEST (TuningStability, resetAndDeterminism)
{
    auto trace = []
    {
        LuthierEngine engine;
        engine.prepare (48000.0, kBlock);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.getWhammyEngine().setBridgeType (Bridge::VintageTrem);
        engine.getStabilityModel().setSettings (physical());
        engine.reset();

        std::vector<double> t;
        performance (engine, 20.0, [&engine, &t] (int)
        {
            for (int s = 0; s < engine.getNumStrings(); ++s)
                t.push_back (engine.getTuningEngine().getStringTuning (s).stabilityCents);
        });
        return t;
    };

    const auto a = trace(), b = trace();
    CHECK (a == b);

    bool moved = false;
    for (double v : a) moved = moved || v != 0.0;
    CHECK_MSG (moved, "the performance made no offsets to compare");

    // reset() zeroes the offsets and restores sigma_committed; a retune with
    // the transport running does not commit.
    Rig rig (makeHardware(), only (StabilityModel::settle), 48000.0, StringAge::Fresh);
    rig.levels[0] = 0.3;
    rig.playing = true;
    rig.run (30.0);
    rig.model.requestRetuneAll();
    rig.run (0.01);
    CHECK (rig.model.getSigma (0) < 1.0);
    CHECK (rig.model.getSigmaCommitted (0) == 1.0);

    rig.levels[0] = 0.0;
    rig.run (1.0);
    rig.model.reset();
    rig.run (0.01);
    CHECK (rig.model.getSigma (0) == 1.0);
    CHECK (rig.maxAbs() == 0.0);
}

//==============================================================================
// TS-13
LUTHIER_TEST (TuningStability, smoothGlides)
{
    LuthierEngine engine;
    engine.prepare (48000.0, kBlock);
    engine.setGuitarType (GuitarType::Stratocaster);
    engine.getStabilityModel().setSettings (only (StabilityModel::capo));
    engine.getTuningEngine().setCapoFret (3);
    engine.reset();

    NoteOnEvent e;
    e.stringIndex = 5;
    e.pitchHz = engine.getTuningEngine().computeFrequency (5, 0.0);
    e.velocity = 1.0;
    engine.triggerNoteNow (e);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;

    for (int b = 0; b < 40; ++b)
        engine.processBlock (buffer, midi);

    const double start = engine.getTuningEngine().getStringTuning (5).stabilityCents;
    CHECK_MSG (start > 3.0, "no capo bias to glide from: " + juce::String (start, 2));

    engine.getStabilityModel().requestRetuneAll();

    double last = start, lastHz = engine.getStringFrequency (5), worstStep = 0.0;
    bool monotonic = true;

    for (int b = 0; b < (int) (0.3 * 48000.0 / kBlock); ++b)
    {
        engine.processBlock (buffer, midi);
        const double now = engine.getTuningEngine().getStringTuning (5).stabilityCents;
        const double hz = engine.getStringFrequency (5);
        worstStep = juce::jmax (worstStep, std::abs (now - last));
        // A millionth of the frequency (0.002 c) is the smoother settling.
        monotonic = monotonic && hz <= lastHz * (1.0 + 1.0e-6);
        last = now;
        lastHz = hz;
    }

    CHECK_MSG (engine.getStabilityModel().getTotalCents (5) == 0.0 || std::abs (last) < 1.0e-6, "the glide did not land");
    CHECK_MSG (worstStep <= 0.5, "a block moved " + juce::String (worstStep, 3) + " c");
    CHECK_MSG (monotonic, "the pitch did not fall monotonically");
}

//==============================================================================
// TS-15
LUTHIER_TEST (TuningStability, serialization)
{
    juce::MemoryBlock saved;
    std::array<double, 6> comp {};

    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (48000.0, kBlock);

        auto set = [&processor] (const char* id, float plain)
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
                p->setValueNotifyingHost (p->convertTo0to1 (plain));
        };

        set (ParamIDs::stabilityAmount, 1.0f);
        set (ParamIDs::capoFret, 2.0f);
        processor.getParameterBridge().applyAllNow();

        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
        processor.getEngine().getStabilityModel().requestRetuneAll();
        processor.processBlock (buffer, midi);

        for (int s = 0; s < 6; ++s)
            comp[(size_t) s] = processor.getEngine().getStabilityModel().getCapoComp (s);

        CHECK_MSG (comp[5] < -1.0, "no capo compensation to save: " + juce::String (comp[5], 3));
        processor.getStateInformation (saved);

        // A preset save carries no stability block.
        const auto preset = processor.getPresetManager().toVar ("Test", "Test");
        CHECK (! juce::JSON::toString (preset).contains ("capo_comp"));
    }

    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (48000.0, kBlock);
        processor.setStateInformation (saved.getData(), (int) saved.getSize());

        for (int s = 0; s < 6; ++s)
            CHECK_MSG (processor.getEngine().getStabilityModel().getCapoComp (s) == comp[(size_t) s],
                       juce::String (processor.getEngine().getStabilityModel().getCapoComp (s), 17) + " vs " + juce::String (comp[(size_t) s], 17));

        // A preset load resets sigma from string_age and clears capoComp.
        const auto preset = processor.getPresetManager().toVar ("Test", "Test");
        processor.getPresetManager().fromVar (preset);

        for (int s = 0; s < 6; ++s)
        {
            CHECK (processor.getEngine().getStabilityModel().getCapoComp (s) == 0.0);
            CHECK (processor.getEngine().getStabilityModel().getSigmaCommitted (s) == StabilityModel::sigmaForAge (StringAge::BrokenIn));
        }
    }
}

//==============================================================================
// TS-16
LUTHIER_TEST (TuningStability, costAndSafety)
{
    StabilitySettings maxed;
    maxed.amount = 4.0;
    maxed.settling = maxed.nutBinding = maxed.backlash = maxed.saddleCreep = maxed.bendMemory = maxed.capoBias = 8.0;

    Rig rig (makeHardware (Bridge::FloydRose, 12), maxed, 48000.0, StringAge::Fresh);
    rig.model.requestRetuneAll();
    rig.run (0.01);   // warm-up

    RtRandom r { 0x16 };
    bool finite = true;
    double worst = 0.0;
    const long before = realismCAllocationCount();
    const auto t0 = juce::Time::getMillisecondCounterHiRes();

    rig.run (600.0, [&] (int b)
    {
        for (int s = 0; s < 12; ++s)
        {
            rig.levels[(size_t) s] = 0.5 * r.nextDouble();
            rig.bends[(size_t) s] = (b / 40 + s) % 3 == 0 ? 300.0 * r.nextDouble() : 0.0;
            rig.whammy[(size_t) s] = (b / 90) % 4 == 1 ? -1200.0 : 0.0;
            rig.capo[(size_t) s] = (b / 5000) % 2 == 0 ? 0 : 4;
        }

        if (b % 700 == 0)
            rig.model.onPluck (b % 12, 1.0);

        for (int s = 0; s < 12; ++s)
        {
            const double c = rig.cents (s);
            finite = finite && std::isfinite (c);
            worst = juce::jmax (worst, std::abs (c));
        }
    });

    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    CHECK (finite);
    CHECK_MSG (worst <= 200.0 + 1.0e-9, "total reached " + juce::String (worst, 2));
    CHECK_MSG (realismCAllocationCount() == before, "the model allocated");

    // A unit is one real-time core: 600 s of blocks is 600000 ms of it. The
    // loop's own work (random numbers, the checks) is counted too.
    const double units = ms / 600000.0;
    std::printf ("      stability cost %.4f units\n", units);
    CHECK_MSG (units <= 0.02, "cost " + juce::String (units, 4) + " units");
}

//==============================================================================
// 3: Transport stop retunes; a stop with nothing to clear does nothing.
LUTHIER_TEST (TuningStability, autoRetuneOnTransportStop)
{
    auto settings = only (StabilityModel::nut);
    settings.autoRetune = AutoRetune::transportStop;

    Rig rig (makeHardware(), settings);
    rig.playing = true;
    rig.levels[0] = 0.1;   // a string ringing, so idle never counts
    rig.bend (3, 200.0);
    CHECK (rig.cents (3) > 1.0);

    rig.playing = false;
    rig.run (0.01);
    CHECK (rig.model.getAutoRetuneCount() == 1);
    CHECK (rig.cents (3) == 0.0);

    rig.playing = true;
    rig.run (0.1);
    rig.playing = false;
    rig.run (0.01);
    CHECK (rig.model.getAutoRetuneCount() == 1);
}
