/*  GuitarCircuit (volume-knob-interaction.md 6).

    Most of these measure the exact continuous response, because that is what
    the spec's numbers are about: where the resonance sits and how it moves.
    The audio path is then held to that response by a separate test, so a
    network that is right on paper and wrong in the discrete solver still fails.
*/

#include "TestFramework.h"

#include "../DSP/Circuit/GuitarCircuit.h"
#include "../LuthierEngine.h"
#include "../Parameters.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  Counts global allocations on the calling thread, for the no-allocation
    tests (engine.md 0). Replacing the global operators is program-wide, so it
    lives here once; everything else in the suite just pays one increment. */
namespace
{
    thread_local long threadAllocationCount = 0;
}

/*  notation-export 7.1 (MODEL-GAPS, TODO 2k): the counter the other suites'
    no-allocation checks read; CMake defines LUTHIER_ALLOCATION_COUNTER for the
    test target so those checks compile in. */
namespace luthier::tests
{
    long allocationsOnThisThread() noexcept { return threadAllocationCount; }
}

struct AllocationCounter
{
    static long count() noexcept { return threadAllocationCount; }
};

/** The same count for other test files (REALISM-A's budget tests). */
long luthierAllocationCount() noexcept { return threadAllocationCount; }
// REALISM-B: the same counter for the suites in other files.
long luthierAllocationsOnThisThread() noexcept { return threadAllocationCount; }

void* operator new (std::size_t size)
{
    ++threadAllocationCount;

    if (auto* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    ++threadAllocationCount;

    if (auto* p = std::malloc (size == 0 ? 1 : size))
        return p;

    throw std::bad_alloc();
}

void operator delete (void* p) noexcept                   { std::free (p); }
void operator delete[] (void* p) noexcept                 { std::free (p); }
void operator delete (void* p, std::size_t) noexcept      { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept    { std::free (p); }

namespace
{
    /** The spec's reference guitar: a single coil, 500 k pots, 22 nF, 3 m of
        standard cable into 1 M. */
    CircuitComponents reference()
    {
        CircuitComponents p;
        p.coilInductance = 2.5;
        p.coilResistance = 6000.0;
        p.coilCapacitance = 200.0e-12;
        p.volumePot = 500.0e3;
        p.tonePot = 500.0e3;
        p.toneCap = 22.0e-9;
        p.cableLength = 3.0;
        p.cableQuality = CableQuality::standard;
        p.ampInputImpedance = 1.0e6;
        return p;
    }

    double level (const CircuitComponents& p, double hz)
    {
        return std::abs (GuitarCircuit::response (p, hz));
    }

    /*  The -3 dB corner: the first frequency above the resonant peak where the
        response has fallen 3 dB below its 100 Hz level. For a response with no
        peak above 100 Hz it is simply the first -3 dB point. */
    double cornerHz (const CircuitComponents& p)
    {
        const double reference = level (p, 100.0);
        const double target = reference * std::pow (10.0, -3.0 / 20.0);

        double hz = 100.0;
        bool pastPeak = false;
        double previous = reference;

        for (; hz < 40000.0; hz *= std::pow (2.0, 1.0 / 192.0))
        {
            const double now = level (p, hz);

            if (now < previous)
                pastPeak = true;

            if (pastPeak && now < target)
                return hz;

            previous = now;
        }

        return hz;
    }

    /*  How much top the guitar has: the 4 kHz level against the 100 Hz level,
        in dB. 4 kHz is where the loaded coil's resonance sits on the spec's
        reference guitar, and losing that peak is what "darker" means here.

        The spec's tests are phrased as a -3 dB corner moving. On this network
        that measure goes the wrong way: turning down flattens the resonance,
        and a flatter, higher-Q-free rolloff crosses -3 dB *later*, not earlier,
        while the ear hears the lost peak. DECISIONS.md has the numbers. */
    double presenceDb (const CircuitComponents& p)
    {
        return gainToDb (level (p, 4000.0) / level (p, 100.0));
    }

    /** Drives the discrete circuit with a sine and returns its steady-state gain. */
    double discreteGain (const CircuitComponents& p, double sampleRate, double hz)
    {
        GuitarCircuit circuit;
        circuit.prepare (sampleRate);
        circuit.setComponents (p);

        const int settle = (int) (sampleRate * 0.2);
        const int measure = (int) (sampleRate * 0.2);

        double inputPower = 0.0, outputPower = 0.0;

        for (int i = 0; i < settle + measure; ++i)
        {
            const double x = std::sin (constants::kTwoPi * hz * i / sampleRate);
            const double y = circuit.process (x);

            if (i >= settle)
            {
                inputPower += x * x;
                outputPower += y * y;
            }
        }

        return std::sqrt (outputPower / juce::jmax (1.0e-30, inputPower));
    }
}

//==============================================================================
LUTHIER_TEST (Circuit, turningDownDarkensAsWellAsQuietens)
{
    auto full = reference();
    auto down = reference();
    down.volume = 0.7;

    /*  "Level drops within 0.5 dB of the audio taper's prediction": the taper's
        prediction for the level is the resistive divider the wiper makes with
        the amp's input, which is what the pot does at a frequency where the
        coil and the capacitors are out of the picture. */
    const double wiper = GuitarCircuit::taperFraction (0.7, PotTaper::audio);
    const double bottom = 500.0e3 * wiper, top = 500.0e3 * (1.0 - wiper);
    const double loaded = (bottom * 1.0e6) / (bottom + 1.0e6);
    const double predictedDb = gainToDb (loaded / (top + loaded));

    const double measuredDb = gainToDb (level (down, 100.0) / level (full, 100.0));

    CHECK_MSG (std::abs (measuredDb - predictedDb) < 0.5,
               "level at 0.7 is " + juce::String (measuredDb, 2) + " dB, the taper predicts "
                 + juce::String (predictedDb, 2));

    const double before = presenceDb (full), after = presenceDb (down);

    CHECK_MSG (after < before - 3.0,
               "the presence only went from " + juce::String (before, 1) + " to "
                 + juce::String (after, 1) + " dB - turning down has to darken");

    // Ground rule 4, honest magnitudes: a few dB at 5 kHz, not a wah sweep.
    const double lostAt5k = gainToDb (level (full, 5000.0) / level (full, 100.0))
                          - gainToDb (level (down, 5000.0) / level (down, 100.0));

    CHECK_MSG (lostAt5k > 1.0 && lostAt5k < 10.0,
               "rolling to 7 lost " + juce::String (lostAt5k, 1) + " dB at 5 kHz");
}

LUTHIER_TEST (Circuit, aKinmanBleedKeepsTheTop)
{
    auto full = reference();
    full.bleed = TrebleBleed::kinman;

    auto down = full;
    down.volume = 0.7;

    const double before = presenceDb (full), after = presenceDb (down);

    CHECK_MSG (std::abs (after - before) < 1.0,
               "with a Kinman bleed the presence went from " + juce::String (before, 1)
                 + " to " + juce::String (after, 1) + " dB");

    // And it keeps more than no bleed at all does.
    auto plain = reference();
    plain.volume = 0.7;
    CHECK (after > presenceDb (plain) + 3.0);
}

LUTHIER_TEST (Circuit, activeModeRemovesTheLoading)
{
    auto full = reference();
    full.active = true;

    auto down = full;
    down.volume = 0.7;

    double lowest = 1.0e9, highest = -1.0e9;

    for (double hz = 20.0; hz <= 20000.0; hz *= std::pow (2.0, 1.0 / 12.0))
    {
        const double differenceDb = GuitarCircuit::magnitudeDb (full, hz) - GuitarCircuit::magnitudeDb (down, hz);
        lowest = juce::jmin (lowest, differenceDb);
        highest = juce::jmax (highest, differenceDb);
    }

    CHECK_MSG (highest - lowest < 0.1,
               "active volume is not a plain attenuator: the difference varies by "
                 + juce::String (highest - lowest, 3) + " dB");

    // Nor does the cable reach the pickup through the buffer.
    auto longCable = full;
    longCable.cableLength = 15.0;
    longCable.cableQuality = CableQuality::vintage;

    CHECK (std::abs (GuitarCircuit::findResonantPeakHz (full)
                     - GuitarCircuit::findResonantPeakHz (longCable)) < 1.0);
}

LUTHIER_TEST (Circuit, potValueMovesTheResonance)
{
    auto small = reference();
    small.volumePot = 250.0e3;
    small.tonePot = 250.0e3;

    auto large = reference();
    large.volumePot = 1.0e6;
    large.tonePot = 1.0e6;

    const double smallPeak = GuitarCircuit::findResonantPeakHz (small);
    const double largePeak = GuitarCircuit::findResonantPeakHz (large);

    // "Differs by at least 10 % and the 1 M case is higher": heavier loading
    // damps the peak, and a damped resonance peaks lower.
    CHECK_MSG (largePeak > smallPeak * 1.1,
               "250 k peaks at " + juce::String (smallPeak, 0) + " Hz and 1 M at "
                 + juce::String (largePeak, 0) + " Hz");
}

LUTHIER_TEST (Circuit, cableCapacitanceMovesTheResonance)
{
    auto shortCable = reference();
    shortCable.cableLength = 1.0;
    shortCable.cableQuality = CableQuality::studio;

    auto longCable = reference();
    longCable.cableLength = 10.0;
    longCable.cableQuality = CableQuality::cheap;

    const double shortPeak = GuitarCircuit::findResonantPeakHz (shortCable);
    const double longPeak = GuitarCircuit::findResonantPeakHz (longCable);

    CHECK_MSG (longPeak < shortPeak * 0.8,
               "1 m studio peaks at " + juce::String (shortPeak, 0) + " Hz and 10 m cheap at "
                 + juce::String (longPeak, 0) + " Hz");
}

LUTHIER_TEST (Circuit, theToneControlSweepsDownAndNeverBoosts)
{
    double previousPresence = 1.0e9;
    const double openLowLevel = level (reference(), 100.0);

    for (int step = 10; step >= 0; --step)
    {
        auto p = reference();
        p.tone = step / 10.0;

        // Monotonically less top. (The -3 dB corner is not monotonic near 0:
        // the tone cap resonates with the coil around 600 Hz - the familiar
        // "honk" of a fully rolled-off tone knob - and the corner is measured
        // past that new peak.)
        const double presence = presenceDb (p);

        CHECK_MSG (presence <= previousPresence + 0.01,
                   "the top came back as the tone came down, at " + juce::String (p.tone, 1));
        previousPresence = presence;

        /*  "No setting produces a gain above unity", read against the open
            guitar: its own loaded resonance already peaks well above the EMF,
            and a rolled-off tone control moves that peak down to the honk
            rather than removing it. What must never happen is the tone control
            making the guitar's loudest point louder. */
        double loudest = 0.0, openLoudest = 0.0;

        for (double hz = 20.0; hz <= 20000.0; hz *= 1.05)
        {
            loudest = juce::jmax (loudest, level (p, hz));
            openLoudest = juce::jmax (openLoudest, level (reference(), hz));
        }

        CHECK_MSG (loudest <= openLoudest * 1.0001,
                   "tone " + juce::String (p.tone, 1) + " peaks above the open guitar");
    }

    CHECK (openLowLevel > 0.9);
}

LUTHIER_TEST (Circuit, bypassIsNeutral)
{
    /*  "cable_on off and circuit_active on with pots at 1.0 produces a response
        flat within 0.1 dB". An active pickup still has its coil - 1.4 keeps its
        resonance "put" - so flat is measured against that bare coil: the
        buffered circuit adds nothing to it. */
    auto p = reference();
    p.active = true;
    p.cableOn = false;

    auto bare = p;
    bare.volume = 1.0;
    bare.tone = 1.0;

    double worst = 0.0;

    for (double hz = 20.0; hz <= 20000.0; hz *= std::pow (2.0, 1.0 / 12.0))
    {
        // The bare coil into the buffer, and nothing else.
        const double coilOnly = level (bare, hz);
        worst = juce::jmax (worst, std::abs (gainToDb (level (p, hz) / coilOnly)));
    }

    CHECK (worst < 0.1);

    // And no coil at all - a piezo into an active preamp - is flat outright.
    p.hasCoil = false;

    for (double hz = 20.0; hz <= 20000.0; hz *= std::pow (2.0, 1.0 / 12.0))
        worst = juce::jmax (worst, std::abs (GuitarCircuit::magnitudeDb (p, hz)));

    CHECK_MSG (worst < 0.1, "a coil-less buffered circuit is not flat: " + juce::String (worst, 3) + " dB");
}

LUTHIER_TEST (Circuit, theAudioPathMatchesTheResponse)
{
    // Below a quarter of the sample rate the bilinear warp is small; this holds
    // the discrete solver to the continuous one there.
    for (double volume : { 1.0, 0.7, 0.3 })
        for (double hz : { 110.0, 440.0, 1000.0, 2500.0 })
        {
            auto p = reference();
            p.volume = volume;
            p.bleed = TrebleBleed::kinman;

            const double expected = level (p, hz);
            const double measured = discreteGain (p, 48000.0, hz);

            CHECK_MSG (std::abs (gainToDb (measured / expected)) < 0.3,
                       "at volume " + juce::String (volume, 1) + ", " + juce::String (hz, 0)
                         + " Hz: audio " + juce::String (gainToDb (measured), 2) + " dB, response "
                         + juce::String (gainToDb (expected), 2) + " dB");
        }
}

LUTHIER_TEST (Circuit, everyCornerOfTheAdvancedRangeIsStable)
{
    int configurations = 0;

    for (double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (double pot : { 1.0e3, 500.0e3, 10.0e6 })
            for (double cap : { 1.0e-9, 1.0e-6 })
                for (double metres : { 0.0, 100.0 })
                    for (double input : { 10.0e3, 10.0e6 })
                        for (double volume : { 0.0, 0.05, 0.5, 1.0 })
                            for (auto bleed : { TrebleBleed::none, TrebleBleed::kinman })
                            {
                                auto p = reference();
                                p.volumePot = pot;
                                p.tonePot = pot;
                                p.toneCap = cap;
                                p.cableLength = metres;
                                p.ampInputImpedance = input;
                                p.volume = volume;
                                p.tone = volume;
                                p.bleed = bleed;

                                GuitarCircuit circuit;
                                circuit.prepare (sampleRate);
                                circuit.setComponents (p);

                                // An impulse, then silence: a stable network rings
                                // down; an unstable one grows or never settles.
                                double peakOut = 0.0, tail = 0.0;
                                const int n = (int) (sampleRate * 0.5);

                                for (int i = 0; i < n; ++i)
                                {
                                    const double y = circuit.process (i == 0 ? 1.0 : 0.0);

                                    if (! std::isfinite (y))
                                    {
                                        peakOut = 1.0e9;
                                        break;
                                    }

                                    peakOut = juce::jmax (peakOut, std::abs (y));

                                    if (i > n - (int) (sampleRate * 0.05))
                                        tail = juce::jmax (tail, std::abs (y));
                                }

                                ++configurations;

                                if (peakOut > dbToGain (24.0) || tail > 1.0e-6)
                                {
                                    ctx.fail ("unstable at " + juce::String (sampleRate, 0) + " Hz, pot "
                                              + juce::String (pot) + ", cap " + juce::String (cap)
                                              + ", cable " + juce::String (metres) + " m, input "
                                              + juce::String (input) + ", volume " + juce::String (volume)
                                              + ": peak " + juce::String (peakOut) + ", tail " + juce::String (tail));
                                    return;
                                }
                            }

    CHECK (configurations > 0);
}

LUTHIER_TEST (Circuit, sweepingEveryControlDoesNotAllocate)
{
    GuitarCircuit circuit;
    circuit.prepare (48000.0);

    auto p = reference();

    // The engine's allocation guard is a counter of global operator new calls
    // on this thread; the circuit has no container that could grow.
    const auto before = AllocationCounter::count();

    for (int step = 0; step < 200; ++step)
    {
        p.volume = (step % 21) / 20.0;
        p.tone = (step % 13) / 12.0;
        p.bleed = (TrebleBleed) (step % 4);
        p.bleedSeries = (step % 2) == 0;
        p.active = (step % 17) == 0;
        p.taper = (PotTaper) (step % 3);
        p.cableLength = step * 0.5;
        circuit.setComponents (p);

        for (int i = 0; i < 64; ++i)
            circuit.process (std::sin (i * 0.1));
    }

    CHECK_MSG (AllocationCounter::count() == before, "the circuit allocated on the audio path");
}

//==============================================================================
LUTHIER_TEST (Circuit, theEngineRunsThroughTheCircuit)
{
    // The guitar's volume knob is the circuit's wiper now, not a gain: rolling
    // it back from 10 to 5 costs level, and at 0 the guitar is silent.
    auto rmsAt = [] (float volume)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.getCabinetEngine().setEnabled (false);
        engine.getRoomEngine().setEnabled (false);
        engine.getAmpEngine().setGain (0.0);

        CircuitComponents controls = reference();
        controls.volume = volume;
        engine.setCircuitControls (controls);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.9f), 0);

        juce::AudioBuffer<float> block (2, 256);
        double power = 0.0;

        for (int b = 0; b < 100; ++b)
        {
            block.clear();
            juce::MidiBuffer none;
            engine.processBlock (block, b == 0 ? midi : none);

            for (int i = 0; i < 256; ++i)
                power += (double) block.getSample (0, i) * block.getSample (0, i);
        }

        return std::sqrt (power / (100.0 * 256.0));
    };

    const double full = rmsAt (1.0f), half = rmsAt (0.5f), off = rmsAt (0.0f);

    CHECK_MSG (half < full * 0.7, "volume 5 is not quieter than 10");
    // What is left at 0 is the amp's own hiss and hum, which come after the
    // guitar; the note itself is gone.
    CHECK_MSG (off < full * 0.02, "volume 0 still makes sound: " + juce::String (off / full));
}

LUTHIER_TEST (Circuit, ohmsReadTheWayThePartsArePrinted)
{
    CHECK (Parameters::formatOhms (500.0e3) == "500k");
    CHECK (Parameters::formatOhms (1.0e6) == "1M");
    CHECK (Parameters::formatOhms (1.5e6) == "1.5M");
    CHECK (Parameters::formatOhms (220.0) == "220");

    CHECK (std::abs (Parameters::parseOhms ("250k") - 250.0e3) < 1.0);
    CHECK (std::abs (Parameters::parseOhms ("1M") - 1.0e6) < 1.0);
    CHECK (std::abs (Parameters::parseOhms ("1 meg") - 1.0e6) < 1.0);
    CHECK (std::abs (Parameters::parseOhms ("470 kohm") - 470.0e3) < 1.0);
    CHECK (std::abs (Parameters::parseOhms ("330") - 330.0) < 1.0e-6);
}

//==============================================================================
/*  advanced-ranges.md 3.1: an unlocked amp control past the end of its knob
    has to sound different from the end of the knob. Before this, every amp
    setter clamped to 0-1 and the advanced range was a number and nothing more.
*/
LUTHIER_TEST (AmpRanges, pastTheKnobIsAudible)
{
    auto render = [] (double gain, double treble)
    {
        AmpEngine amp;
        amp.prepare (48000.0, 512);
        amp.setModel (AmpModel::MarshallPlexi);
        amp.setGain (gain);
        amp.setTreble (treble);
        amp.setMaster (0.5);

        for (int i = 0; i < 48000; ++i)
            amp.processSample (0.0);

        std::vector<double> out (16384);

        for (size_t i = 0; i < out.size(); ++i)
            out[i] = amp.processSample (0.05 * std::sin (constants::kTwoPi * 220.0 * (double) i / 48000.0));

        return out;
    };

    auto difference = [] (const std::vector<double>& a, const std::vector<double>& b)
    {
        double d = 0.0;

        for (size_t i = 8192; i < a.size(); ++i)
            d += (a[i] - b[i]) * (a[i] - b[i]);

        return std::sqrt (d / 8192.0);
    };

    const auto atEnd = render (1.0, 0.5);
    const auto past = render (1.8, 0.5);
    CHECK_MSG (difference (atEnd, past) > 1.0e-3, "amp gain 1.8 sounds the same as 1.0");

    const auto trebleEnd = render (0.5, 1.0);
    const auto treblePast = render (0.5, 1.4);
    CHECK_MSG (difference (trebleEnd, treblePast) > 1.0e-4, "treble 1.4 sounds the same as 1.0");

    for (double v : past)
        if (! std::isfinite (v)) { ctx.fail ("non-finite output past the knob"); break; }
}
