/*  SPEC-SWEEP: engine.md tests the audit found missing (EN-16, EN-75, EN-77). */

#include "TestFramework.h"

#include "../LuthierEngine.h"
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
