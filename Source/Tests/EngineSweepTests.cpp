/*  SPEC-SWEEP: engine.md tests the audit found missing (EN-16, EN-75, EN-77). */

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../DSP/Whammy/WhammyEngine.h"
#include "../DSP/Amp/CabinetEngine.h"
#include "../DSP/Circuit/GuitarCircuit.h"
#include "../Model/Playing/MidiInterpreter.h"
#include "../Model/Playing/TuningEngine.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
}

//==============================================================================
/*  EN-16: engine.md 2's default CC map, and the two pedals that hold notes. */
LUTHIER_TEST (Midi, defaultCcMapMatchesTheSpec)
{
    MidiInterpreter midi;
    midi.resetCcMapToDefaults();

    CHECK (midi.getCcTarget (1)  == MidiTarget::VibratoDepth);
    CHECK (midi.getCcTarget (2)  == MidiTarget::WhammyBar);
    CHECK (midi.getCcTarget (4)  == MidiTarget::Expression);
    CHECK (midi.getCcTarget (11) == MidiTarget::MasterLevel);
    CHECK (midi.getCcTarget (65) == MidiTarget::SlideToggle);
    CHECK (midi.getCcTarget (67) == MidiTarget::PalmMute);

    // 70-79 are user-mappable, and ship with something useful on every one.
    for (int cc = 70; cc <= 79; ++cc)
        CHECK_MSG (midi.getCcTarget (cc) != MidiTarget::None, "CC " + juce::String (cc) + " has no default");

    // CC 64 and 66 are the pedals, not map entries: a note released under
    // either keeps ringing.
    auto ringingAfterRelease = [] (int pedalCc)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setGuitarType (GuitarType::Stratocaster);

        juce::AudioBuffer<float> buffer (2, kBlock);
        double level = 0.0;

        for (int b = 0; b < 200; ++b)
        {
            juce::MidiBuffer m;

            if (b == 0)
            {
                if (pedalCc == 64)
                    m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
                m.addEvent (juce::MidiMessage::noteOn (1, 52, 0.9f), 1);
            }

            // Sostenuto holds the notes already down when it is pressed.
            if (b == 2 && pedalCc == 66)
                m.addEvent (juce::MidiMessage::controllerEvent (1, 66, 127), 0);

            if (b == 10)
                m.addEvent (juce::MidiMessage::noteOff (1, 52), 0);

            buffer.clear();
            engine.processBlock (buffer, m);

            if (b == 199)
                level = buffer.getRMSLevel (0, 0, kBlock);
        }

        return level;
    };

    const double released = ringingAfterRelease (0);
    const double sustained = ringingAfterRelease (64);
    const double sostenuto = ringingAfterRelease (66);

    CHECK_MSG (sustained > released * 4.0,
               "CC 64 should hold the note: " + juce::String (sustained, 6) + " vs " + juce::String (released, 6));
    CHECK_MSG (sostenuto > released * 4.0,
               "CC 66 should hold the note: " + juce::String (sostenuto, 6) + " vs " + juce::String (released, 6));
}

//==============================================================================
/*  EN-75: every preset tuning is equal-tempered A440 to a tenth of a cent. */
LUTHIER_TEST (Tuning, everyPresetIsExactToATenthOfACent)
{
    struct Row { TuningPreset preset; std::vector<int> notes; };

    const Row rows[] =
    {
        { TuningPreset::Standard,       { 64, 59, 55, 50, 45, 40 } },
        { TuningPreset::DropD,          { 64, 59, 55, 50, 45, 38 } },
        { TuningPreset::DropC,          { 62, 57, 53, 48, 43, 36 } },
        { TuningPreset::DropB,          { 61, 56, 52, 47, 42, 35 } },
        { TuningPreset::DADGAD,         { 62, 57, 55, 50, 45, 38 } },
        { TuningPreset::OpenG,          { 62, 59, 55, 50, 43, 38 } },
        { TuningPreset::OpenD,          { 62, 57, 54, 50, 45, 38 } },
        { TuningPreset::OpenE,          { 64, 59, 56, 52, 47, 40 } },
        { TuningPreset::OpenC,          { 64, 60, 55, 48, 43, 36 } },
        { TuningPreset::HalfStepDown,   { 63, 58, 54, 49, 44, 39 } },
        { TuningPreset::FullStepDown,   { 62, 57, 53, 48, 43, 38 } },
        { TuningPreset::Nashville,      { 64, 59, 67, 62, 57, 52 } },
        { TuningPreset::SevenString,    { 64, 59, 55, 50, 45, 40, 35 } },
        { TuningPreset::EightString,    { 64, 59, 55, 50, 45, 40, 35, 30 } },
        { TuningPreset::BaritoneB,      { 59, 54, 50, 45, 40, 35 } },
        { TuningPreset::BassStandard,   { 43, 38, 33, 28 } },
        { TuningPreset::BassFiveString, { 43, 38, 33, 28, 23 } },
    };

    for (const auto& row : rows)
    {
        double hz[12] {};
        TuningEngine::getPresetFrequencies (row.preset, hz, (int) row.notes.size());

        for (size_t s = 0; s < row.notes.size(); ++s)
        {
            const double expected = 440.0 * std::pow (2.0, (row.notes[s] - 69) / 12.0);
            const double cents = 1200.0 * std::log2 (hz[s] / expected);

            CHECK_MSG (std::abs (cents) < 0.1,
                       "preset " + juce::String ((int) row.preset) + " string " + juce::String ((int) s)
                       + " is " + juce::String (cents, 3) + " cents off");
        }
    }
}

//==============================================================================
/*  EN-77: the pickup's resonance has the Q its L, C and R imply. Unloaded (the
    pots, cable and amp all but removed), the circuit's -3 dB bandwidth around
    the peak gives Q = f0 / bandwidth, against the series-RLC (1/R) sqrt(L/C)
    PickupEngine reports, within 20 %. */
LUTHIER_TEST (Pickup, resonantQMatchesTheLcrValues)
{
    for (auto type : { PickupType::SingleCoil, PickupType::Humbucker })
    {
        const auto spec = PickupSpec::makeDefault (type, 0.13);

        PickupEngine p;
        p.prepare (kSr, 6);
        p.setPickupSpec (0, spec);

        CircuitComponents c;
        c.coilInductance = spec.inductanceHenries;
        c.coilResistance = spec.resistanceKOhm * 1000.0;
        c.coilCapacitance = spec.capacitancePf * 1.0e-12;
        c.volumePot = 1.0e10;
        c.tonePot = 1.0e10;
        c.toneCap = 1.0e-15;
        c.cableOn = false;
        c.ampInputImpedance = 1.0e10;

        // Find the peak and the -3 dB points on a fine log grid.
        double peakHz = 0.0, peakDb = -1.0e9;

        for (double f = 500.0; f < 30000.0; f *= 1.0005)
        {
            const double db = GuitarCircuit::magnitudeDb (c, f);
            if (db > peakDb) { peakDb = db; peakHz = f; }
        }

        double lo = peakHz, hi = peakHz;
        while (lo > 100.0 && GuitarCircuit::magnitudeDb (c, lo) > peakDb - 3.0103) lo /= 1.0005;
        while (hi < 60000.0 && GuitarCircuit::magnitudeDb (c, hi) > peakDb - 3.0103) hi *= 1.0005;

        const double measuredQ = peakHz / (hi - lo);
        const double expectedQ = p.getResonantQ (0);

        CHECK_NEAR (peakHz, p.getResonantFrequency (0), p.getResonantFrequency (0) * 0.05);
        CHECK_MSG (std::abs (measuredQ / expectedQ - 1.0) < 0.2,
                   juce::String (type == PickupType::SingleCoil ? "single coil" : "humbucker")
                   + ": measured Q " + juce::String (measuredQ, 2) + " vs LCR " + juce::String (expectedQ, 2));
    }
}

//==============================================================================
/*  EN-52: engine.md 8.2 - a Floyd Rose's springs ring on a fast return: a
    burst concentrated in 200-500 Hz whose envelope falls to 1/e in 50-100 ms.
    A vintage trem's do not. */
LUTHIER_TEST (Whammy, floydSpringsRingOnReturn)
{
    auto burst = [] (WhammyEngine::BridgeType type)
    {
        WhammyEngine w;
        w.prepare (kSr, 6);
        w.setBridgeType (type);
        w.setSpringAmount (1.0);

        // Dive, hold, then snap back.
        w.setPosition (-1.0);
        for (int b = 0; b < 200; ++b)
        {
            w.updateBlock (kBlock);
            for (int i = 0; i < kBlock; ++i)
                w.processSpringNoise();
        }

        w.setPosition (0.0);

        std::vector<double> out;
        for (int b = 0; b < (int) (0.6 * kSr / kBlock); ++b)
        {
            w.updateBlock (kBlock);
            for (int i = 0; i < kBlock; ++i)
                out.push_back (w.processSpringNoise());
        }

        return out;
    };

    const auto vintage = burst (WhammyEngine::BridgeType::VintageTrem);
    double vintageEnergy = 0.0;
    for (double v : vintage) vintageEnergy += v * v;
    CHECK_MSG (vintageEnergy == 0.0, "a vintage trem's springs should not ring");

    const auto floyd = burst (WhammyEngine::BridgeType::FloydRose);

    // Envelope in 5 ms windows.
    const int window = (int) (0.005 * kSr);
    std::vector<double> env;
    for (size_t start = 0; start + (size_t) window <= floyd.size(); start += (size_t) window)
    {
        double e = 0.0;
        for (int i = 0; i < window; ++i) e += floyd[start + (size_t) i] * floyd[start + (size_t) i];
        env.push_back (std::sqrt (e / window));
    }

    size_t peakAt = 0;
    for (size_t i = 0; i < env.size(); ++i)
        if (env[i] > env[peakAt]) peakAt = i;

    CHECK_MSG (env[peakAt] > 1.0e-5, "the Floyd's springs did not ring");

    size_t fallAt = peakAt;
    while (fallAt < env.size() && env[fallAt] > env[peakAt] / std::exp (1.0)) ++fallAt;
    const double decayMs = (double) (fallAt - peakAt) * 5.0;
    CHECK_MSG (decayMs >= 50.0 && decayMs <= 100.0, "spring decay to 1/e took " + juce::String (decayMs) + " ms");

    // Spectrum: most of the energy between 200 and 500 Hz.
    double inBand = 0.0, total = 0.0;
    const int n = juce::jmin ((int) floyd.size(), 8192);

    for (double f = 50.0; f <= 5000.0; f += 25.0)
    {
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double a = 2.0 * juce::MathConstants<double>::pi * f * i / kSr;
            re += floyd[(size_t) i] * std::cos (a);
            im += floyd[(size_t) i] * std::sin (a);
        }

        const double p = re * re + im * im;
        total += p;
        if (f >= 200.0 && f <= 500.0) inBand += p;
    }

    CHECK_MSG (inBand / total > 0.6, "only " + juce::String (100.0 * inBand / total, 1) + " % of the burst is in 200-500 Hz");
}

//==============================================================================
/*  EN-49: engine.md 7.3-7.6 - the magnets colour their stated bands, a
    humbucker's two coils comb and the coil tap removes it, the piezo is band
    limited with a 3 kHz presence peak, and the internal mic tilts down and
    warms the low mids. */
namespace
{
    /** Steady-state gain of a single-string feed through a pickup, at `hz`. */
    double pickupGain (const PickupSpec& spec, double hz)
    {
        PickupEngine p;
        p.prepare (kSr, 1);
        p.setNumPickups (1);
        auto s = spec;
        s.inductanceHenries = 0.0;
        s.capacitancePf = 0.0;
        p.setPickupSpec (0, s);
        p.setSelector (PickupSelector::Bridge);
        p.reset();

        const int n = 16384;
        double in = 0.0, out = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double x = std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / kSr);
            const double ins[1] = { x };
            const double delays[1] = { 400.0 };
            const double y = p.processStrings (ins, delays, 1);

            if (i >= n / 2) { in += x * x; out += y * y; }
        }

        return std::sqrt (out / juce::jmax (1.0e-30, in));
    }

    template <typename Fn>
    double filterGain (Fn&& process, double hz)
    {
        const int n = 16384;
        double in = 0.0, out = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double x = std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / kSr);
            const double y = process (x);
            if (i >= n / 2) { in += x * x; out += y * y; }
        }

        return std::sqrt (out / juce::jmax (1.0e-30, in));
    }
}

LUTHIER_TEST (Pickup, magnetsDifferInTheirStatedBands)
{
    auto withMagnet = [] (MagnetType m)
    {
        auto s = PickupSpec::makeDefault (PickupType::SingleCoil, 0.13);
        s.magnet = m;
        return s;
    };

    // Relative to alnico 3, the flattest: alnico 2's bump is at 800 Hz, and
    // ceramic lifts the top.
    auto rel = [&] (MagnetType m, double hz) { return gainToDb (pickupGain (withMagnet (m), hz) / pickupGain (withMagnet (MagnetType::Alnico3), hz)); };

    CHECK_MSG (rel (MagnetType::Alnico2, 800.0) > rel (MagnetType::Alnico2, 150.0) + 1.5, "alnico 2 has no 800 Hz bump");
    CHECK_MSG (rel (MagnetType::Ceramic, 8000.0) > rel (MagnetType::Ceramic, 300.0) + 2.0, "ceramic is not brighter");
}

LUTHIER_TEST (Pickup, humbuckerCombAndCoilTap)
{
    auto hb = PickupSpec::makeDefault (PickupType::Humbucker, 0.14);
    auto tapped = hb;
    tapped.coilTapped = true;

    double minDb = 1.0e9, maxDb = -1.0e9;

    for (double hz = 200.0; hz < 8000.0; hz *= 1.12)
    {
        const double d = gainToDb (pickupGain (hb, hz) / pickupGain (tapped, hz));
        minDb = juce::jmin (minDb, d);
        maxDb = juce::jmax (maxDb, d);
    }

    CHECK_MSG (maxDb - minDb > 6.0,
               "the humbucker's inter-coil comb should shape it against the tapped coil by more than 6 dB, got "
               + juce::String (maxDb - minDb, 2));
}

LUTHIER_TEST (Pickup, piezoAndMicFilters)
{
    PickupEngine p;
    p.prepare (kSr, 6);

    auto piezo = [&p] (double hz) { p.reset(); return filterGain ([&p] (double x) { return p.processPiezo (x); }, hz); };
    auto mic = [&p] (double hz) { p.reset(); return filterGain ([&p] (double x) { return p.processInternalMic (x); }, hz); };

    const double ref = piezo (1000.0);
    CHECK_NEAR (gainToDb (piezo (40.0) / ref), -3.0, 1.5);
    CHECK_NEAR (gainToDb (piezo (15000.0) / ref), -3.0, 2.5);
    CHECK_MSG (piezo (3000.0) > ref * dbToGain (2.0), "the piezo has no 3 kHz peak");

    CHECK_MSG (mic (10000.0) < mic (1000.0) * dbToGain (-1.5), "the internal mic does not tilt down");
    CHECK_MSG (mic (250.0) > mic (1000.0), "the internal mic has no low-mid warmth");
}

//==============================================================================
/*  EN-50: engine.md 7.7 - the selector crossfades within 5 ms without a step,
    and each pickup's volume scales only its own slot. */
LUTHIER_TEST (Pickup, selectorChangesCrossfadeIn5ms)
{
    auto make = [] (PickupEngine& p)
    {
        p.prepare (kSr, 1);
        p.setNumPickups (2);

        for (int slot = 0; slot < 2; ++slot)
        {
            auto s = PickupSpec::makeDefault (PickupType::SingleCoil, slot == 0 ? 0.10 : 0.30);
            s.inductanceHenries = 0.0;
            s.capacitancePf = 0.0;
            p.setPickupSpec (slot, s);
        }

        p.setSelector (PickupSelector::Bridge);
        p.reset();
    };

    auto sample = [] (PickupEngine& p, int i)
    {
        const double ins[1] = { std::sin (2.0 * juce::MathConstants<double>::pi * 330.0 * i / kSr) };
        const double delays[1] = { 400.0 };
        return p.processStrings (ins, delays, 1);
    };

    PickupEngine switching, neckOnly;
    make (switching);
    make (neckOnly);
    neckOnly.setSelector (PickupSelector::Neck);
    neckOnly.reset();

    const int switchAt = 8000;
    double worstStep = 0.0, steadyStep = 0.0, prev = 0.0;
    double settledError = 0.0;

    for (int i = 0; i < 16000; ++i)
    {
        if (i == switchAt)
            switching.setSelector (PickupSelector::Neck);

        const double y = sample (switching, i);
        const double ref = sample (neckOnly, i);

        if (i > 100)
        {
            const double step = std::abs (y - prev);
            if (i < switchAt) steadyStep = juce::jmax (steadyStep, step);
            else if (i < switchAt + 480) worstStep = juce::jmax (worstStep, step);
        }

        if (i >= switchAt + (int) (0.005 * kSr) && i < switchAt + 2000)
            settledError = juce::jmax (settledError, std::abs (y - ref));

        prev = y;
    }

    CHECK_MSG (worstStep < steadyStep * 2.0, "the switch stepped: " + juce::String (worstStep, 5) + " vs steady " + juce::String (steadyStep, 5));
    CHECK_MSG (settledError < 0.02, "not settled 5 ms after the switch: error " + juce::String (settledError, 5));

    // Volumes: the neck's knob scales the neck, and not the bridge.
    auto level = [&] (PickupSelector sel, int slotTurnedDown)
    {
        PickupEngine p;
        make (p);
        p.setSelector (sel);
        p.setPickupVolume (slotTurnedDown, 0.5);
        p.reset();

        double e = 0.0;
        for (int i = 0; i < 8000; ++i) { const double y = sample (p, i); if (i > 4000) e += y * y; }
        return std::sqrt (e);
    };

    auto full = [&] (PickupSelector sel)
    {
        PickupEngine p;
        make (p);
        p.setSelector (sel);
        p.reset();
        double e = 0.0;
        for (int i = 0; i < 8000; ++i) { const double y = sample (p, i); if (i > 4000) e += y * y; }
        return std::sqrt (e);
    };

    CHECK_NEAR (level (PickupSelector::Neck, 1) / full (PickupSelector::Neck), 0.5, 0.01);
    CHECK_NEAR (level (PickupSelector::Bridge, 1) / full (PickupSelector::Bridge), 1.0, 1.0e-6);
}

//==============================================================================
/*  EN-65: engine.md 13.1 - the cabinet's convolution is a convolution: its
    mic tap matches a direct time-domain convolution with the IR (up to the
    loader's normalisation gain and the reported latency) to -80 dB. */
LUTHIER_TEST (Cabinet, convolutionMatchesOfflineConvolution)
{
    constexpr int block = 512;
    CabinetEngine cab;
    cab.prepare (kSr, block);
    cab.setEnabled (true);
    cab.setDualMicEnabled (false);

    // A short decaying noise IR.
    juce::Random random (7);
    std::vector<float> ir (300);
    for (size_t i = 0; i < ir.size(); ++i)
        ir[i] = (float) ((random.nextDouble() * 2.0 - 1.0) * std::exp (-(double) i / 60.0));

    cab.loadImpulseResponse (0, ir.data(), (int) ir.size(), kSr);
    CHECK (cab.hasImpulseResponse (0));

    const int latency = cab.getLatencySamples();

    // Input: an impulse, then noise.
    const int total = block * 24;
    std::vector<float> in ((size_t) total, 0.0f), out;
    in[100] = 1.0f;
    for (int i = 2000; i < total; ++i)
        in[(size_t) i] = (float) (random.nextDouble() * 2.0 - 1.0) * 0.3f;

    for (int b = 0; b < total / block; ++b)
    {
        juce::AudioBuffer<float> buffer (2, block);
        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, in.data() + b * block, block);

        cab.processBlock (buffer);

        const float* tap = cab.getMicTap (0);
        CHECK (tap != nullptr);
        if (tap == nullptr) return;
        out.insert (out.end(), tap, tap + block);
    }

    // The reference, delayed by the reported latency.
    std::vector<double> ref ((size_t) total, 0.0);
    for (int n = 0; n < total; ++n)
    {
        double acc = 0.0;
        for (int k = 0; k < (int) ir.size() && k <= n; ++k)
            acc += (double) ir[(size_t) k] * in[(size_t) (n - k)];
        if (n + latency < total)
            ref[(size_t) (n + latency)] = acc;
    }

    // The loader normalises the IR, so fit the one gain, then look at what is left.
    double dot = 0.0, rr = 0.0, oo = 0.0;
    for (int n = latency; n < total; ++n)
    {
        dot += out[(size_t) n] * ref[(size_t) n];
        rr += ref[(size_t) n] * ref[(size_t) n];
        oo += (double) out[(size_t) n] * out[(size_t) n];
    }

    const double gain = dot / juce::jmax (1.0e-30, rr);
    double residual = 0.0;
    for (int n = latency; n < total; ++n)
    {
        const double e = out[(size_t) n] - gain * ref[(size_t) n];
        residual += e * e;
    }

    const double db = 10.0 * std::log10 (juce::jmax (1.0e-30, residual) / juce::jmax (1.0e-30, oo));
    CHECK_MSG (oo > 1.0e-6, "the cabinet produced nothing");
    CHECK_MSG (db < -80.0, "the convolution differs from a direct one by " + juce::String (db, 1) + " dB");
}

//==============================================================================
/*  EN-95: engine.md 22 - MIDI in to audio out within 2 ms of the reported
    latency: a note sent at sample k starts to change the output by
    k + latency + 2 ms, and not before k. The onset is measured against the
    same render without the note, so the idle noise floor (which is
    deterministic) cancels. The chord window is zero, the only other wait. */
LUTHIER_TEST (Engine, midiToAudioIsWithinTwoMillisecondsOfTheReportedLatency)
{
    for (int offset : { 0, 137, 400 })
    {
        int latency = 0;

        auto render = [offset, &latency] (bool withNote)
        {
            LuthierEngine engine;
            engine.prepare (kSr, 512);
            engine.setGuitarType (GuitarType::Stratocaster);
            engine.getMidiInterpreter().setChordWindowMs (0.0);
            engine.reset();
            latency = engine.getLatencySamples();

            std::vector<float> out;

            for (int b = 0; b < 8; ++b)
            {
                juce::AudioBuffer<float> buffer (2, 512);
                buffer.clear();
                juce::MidiBuffer midi;

                if (b == 2 && withNote)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 52, 1.0f), offset);

                engine.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
            }

            return out;
        };

        const auto with = render (true);
        const auto without = render (false);

        int onset = -1;
        for (size_t i = 0; i < with.size(); ++i)
            if (std::abs (with[i] - without[i]) > 1.0e-4f) { onset = (int) i; break; }

        const int sent = 2 * 512 + offset;

        CHECK_MSG (onset >= sent, "sound before the note at offset " + juce::String (offset));
        CHECK_MSG (onset >= 0 && onset - sent <= latency + (int) (0.002 * kSr),
                   "offset " + juce::String (offset) + ": onset " + juce::String (onset - sent)
                   + " samples after the note, reported latency " + juce::String (latency));
    }
}
