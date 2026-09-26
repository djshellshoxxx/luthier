/*  qa-polish.md 5, realism modules: the spec's own numbers, where the existing
    tests held looser ones (QA report items 39). */

#include "TestFramework.h"

#include "../DSP/Circuit/GuitarCircuit.h"
#include "../DSP/Noise/FretBuzz.h"
#include "../DSP/Noise/PlayingNoise.h"
#include "../LuthierEngine.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
}

//==============================================================================
/*  QA-5.18: "Slide continuous pitch: the per-string frequency follows the
    slide bar within 2 cents." Once the bar has arrived two frets up, the
    string sits two semitones above where it sat before the move, to 2 cents.
    Both readings are taken after the pluck has settled, so the amplitude-
    dependent sharpness of a fresh pluck (the reason Slide.pitchIsContinuous
    allows 25 cents from its first reading) is not in either. */
LUTHIER_TEST (Slide, theBarArrivesWithinTwoCents)
{
    for (double assist : { 0.0, 0.15 })
    {
        LuthierEngine engine;
        engine.prepare (kSr, 256);
        engine.setGuitarType (GuitarType::Stratocaster);

        SlideSettings s;
        s.enabled = true;
        s.mode = SlideMode::lapSteel;
        s.intonationAssist = assist;
        s.dampingBehind = 1.0;
        engine.setSlideSettings (s);

        auto note = [] (double fret)
        {
            NoteOnEvent e;
            e.stringIndex = 2;
            e.fretPosition = fret;
            e.velocity = 0.8;
            e.pitchHz = 110.0 * std::pow (2.0, fret / 12.0);
            e.technique = Technique::SlideGuitar;
            return e;
        };

        auto run = [&engine] (double seconds)
        {
            juce::AudioBuffer<float> block (2, 256);

            for (int b = 0; b < (int) (seconds * kSr / 256.0); ++b)
            {
                block.clear();
                juce::MidiBuffer none;
                engine.processBlock (block, none);
            }
        };

        engine.triggerNoteNow (note (3.0));
        run (0.5);
        const double before = engine.getStringFrequency (2);

        auto sweep = note (5.0);
        sweep.slideFromFret = 3.0;
        sweep.slideSeconds = 0.5;
        engine.triggerNoteNow (sweep);
        run (0.9);
        const double after = engine.getStringFrequency (2);

        const double cents = 1200.0 * std::log2 (after / before);
        CHECK_MSG (std::abs (cents - 200.0) <= 2.0,
                   "assist " + juce::String (assist, 2) + ": the bar arrived " + juce::String (cents - 200.0, 2)
                     + " cents from two frets up");
    }
}

//==============================================================================
/*  QA-5.14: "Squeak determinism: fixed seed, same slide event, byte-identical
    buffer across 1000 runs." */
LUTHIER_TEST (Squeak, aThousandRunsAreByteIdentical)
{
    StringNoiseInfo wound;
    wound.wound = true;
    wound.windingPitchPerMm = 6.5;
    wound.windingDepth = 1.0;
    wound.material = StringMaterial::PhosphorBronze;

    auto render = [&wound]
    {
        PlayingNoise noise;
        noise.prepare (kSr);
        noise.setSeed (0x5EED);

        SqueakSettings squeak;
        squeak.amount = 1.0;
        squeak.probability = 1.0;
        noise.setSqueak (squeak);

        std::array<double, 12> exc {}, surf {};
        std::vector<double> out (4096);

        noise.onShift (0, wound, 648.0, 2.0, 7.0, 0.1, 1u);

        for (auto& x : out)
            x = noise.processSample (exc.data(), surf.data(), 6);

        return out;
    };

    const auto reference = render();
    double energy = 0.0;

    for (auto x : reference)
        energy += x * x;

    CHECK_MSG (energy > 0.0, "the squeak fixture made no sound");

    int different = 0;

    for (int run = 1; run < 1000; ++run)
    {
        const auto again = render();

        if (std::memcmp (again.data(), reference.data(), reference.size() * sizeof (double)) != 0)
            ++different;
    }

    CHECK_MSG (different == 0, juce::String (different) + " of 999 runs differed from the first");
}

//==============================================================================
/*  QA-5.17: "Fret buzz threshold: for a fixture setup at threshold - 1 dB, no
    buzz; at threshold + 3 dB, buzz present." For each setup style and string,
    the level at which the string first touches a fret is found; 1 dB under it
    the string is clear, 3 dB over it the buzz generator has a level. */
LUTHIER_TEST (Buzz, oneDecibelUnderTheThresholdIsCleanThreeOverBuzzes)
{
    int fixtures = 0;

    for (int style = 0; style < kNumSetupStyles; ++style)
    {
        FretBuzz buzz;
        SetupGeometry g;
        const auto& st = getSetupStyle (style);
        g.actionTreble = st.actionTreble;
        g.actionBass = st.actionBass;
        g.relief = st.relief;
        buzz.setGeometry (g);

        for (int string = 0; string < 6; ++string)
        {
            for (double fret : { 0.0, 5.0 })
            {
                // Bisect the level at which the worst excess crosses zero.
                double lo = 1.0e-6, hi = 100.0;

                if (buzz.sense (string, fret, hi, 0.2).excessMm <= 0.0)
                    continue;       // this string never reaches a fret

                for (int i = 0; i < 80; ++i)
                {
                    const double mid = std::sqrt (lo * hi);
                    (buzz.sense (string, fret, mid, 0.2).excessMm > 0.0 ? hi : lo) = mid;
                }

                const double threshold = hi;
                const auto under = buzz.sense (string, fret, threshold * juce::Decibels::decibelsToGain (-1.0), 0.2);
                const auto over = buzz.sense (string, fret, threshold * juce::Decibels::decibelsToGain (3.0), 0.2);

                CHECK_MSG (under.excessMm < 0.0, juce::String (st.name) + " string " + juce::String (string)
                                                   + ": 1 dB under the threshold still touches");
                CHECK_MSG (over.excessMm > 0.0 && buzz.levelFor (over.excessMm) > 0.0,
                           juce::String (st.name) + " string " + juce::String (string) + ": 3 dB over does not buzz");
                ++fixtures;
            }
        }
    }

    CHECK_MSG (fixtures >= 12, juce::String (fixtures) + " fixtures reached a fret");
}

//==============================================================================
/*  QA-5.23: "Treble bleed circuits: at volume 5, 4 kHz vs 500 Hz level within
    0.5 dB (Kinman) or ~3 dB (None)." Read as the tilt (4 kHz relative to
    500 Hz) at volume 5 against the same tilt at volume 10: a Kinman bleed
    keeps the top (the tilt moves by no more than 0.5 dB), no bleed loses it
    (the tilt falls by about 3 dB or more). Measured on the reference single
    coil (500 k pots, 3 m of cable into 1 M). */
LUTHIER_TEST (Circuit, atVolumeFiveKinmanHoldsTheTiltNoneLosesIt)
{
    auto reference = []
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
    };

    auto tiltShift = [&reference] (TrebleBleed bleed)
    {
        auto full = reference();
        full.bleed = bleed;
        auto half = full;
        half.volume = 0.5;

        auto tilt = [] (const CircuitComponents& p)
        {
            return GuitarCircuit::magnitudeDb (p, 4000.0) - GuitarCircuit::magnitudeDb (p, 500.0);
        };

        return tilt (half) - tilt (full);
    };

    const double kinman = tiltShift (TrebleBleed::kinman), none = tiltShift (TrebleBleed::none);

    CHECK_MSG (std::abs (kinman) <= 0.5, "Kinman: the 4 kHz / 500 Hz tilt moved " + juce::String (kinman, 2) + " dB at volume 5");
    CHECK_MSG (none <= -2.5, "no bleed: the tilt moved only " + juce::String (none, 2) + " dB at volume 5 (spec ~3 dB)");
}
