/*  The E-Bow on the feedback path (ambiguity-resolutions.md 2.2, tests 2.4). */

#include "TestFramework.h"

#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    void set (LuthierAudioProcessor& processor, const char* id, float plain)
    {
        if (auto* p = processor.getState().getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    struct Rig
    {
        Rig()
        {
            set (processor, ParamIDs::macroHumanize, 0.0f);
            set (processor, ParamIDs::ebowEnable, 1.0f);

            // The per-string outputs, so the string itself can be heard: its
            // level meter releases over 60 ms and cannot show a faster stop.
            auto layout = processor.getBusesLayout();

            for (int s = 0; s < kNumPerStringBuses; ++s)
                layout.outputBuses.getReference (1 + kNumAuxBuses + s) = juce::AudioChannelSet::mono();

            processor.setBusesLayout (layout);
            processor.prepareToPlay (kSr, kBlock);

            /*  REALISM-B: the character seed is drawn from the clock per
                instance, so every rig was a different guitar - dead spots and
                drift included. A fixed one makes each run the same guitar. */
            processor.getEngine().getCharacterEngine().setSeed (0x5EEDE80Bull);
        }

        /** Runs `seconds`, recording the loudest string's level each block. */
        void run (double seconds, juce::MidiBuffer first = {})
        {
            juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                         processor.getTotalNumInputChannels(), 2), kBlock);

            for (int b = 0; b < (int) (seconds * kSr / kBlock); ++b)
            {
                buffer.clear();
                juce::MidiBuffer midi;

                if (b == 0)
                    midi = first;

                processor.processBlock (buffer, midi);

                if (string < 0)
                {
                    double best = 1.0e-4;

                    for (int s = 0; s < processor.getEngine().getNumStrings(); ++s)
                        if (processor.getEngine().getStringLevel (s) > best)
                        {
                            best = processor.getEngine().getStringLevel (s);
                            string = s;
                        }
                }

                levels.push_back (string >= 0 ? processor.getEngine().getStringLevel (string) : 0.0);

                const auto own = processor.getBusBuffer (buffer, false, 1 + kNumAuxBuses + juce::jmax (0, string));

                for (int i = 0; i < kBlock; ++i)
                {
                    out.push_back (buffer.getSample (0, i));
                    stringOut.push_back (string >= 0 && own.getNumChannels() > 0 ? own.getSample (0, i) : 0.0f);
                }
            }
        }

        static juce::MidiBuffer note (bool on, int key = 64)
        {
            juce::MidiBuffer midi;
            midi.addEvent (on ? juce::MidiMessage::noteOn (1, key, (juce::uint8) 100)
                              : juce::MidiMessage::noteOff (1, key), 0);
            return midi;
        }

        double levelAt (double seconds) const
        {
            return levels[(size_t) juce::jlimit (0, (int) levels.size() - 1, (int) (seconds * kSr / kBlock))];
        }

        LuthierAudioProcessor processor;
        std::vector<double> levels;
        std::vector<float> out, stringOut;
        int string = -1;
    };

    double rms (const std::vector<float>& v, double fromSeconds, double toSeconds)
    {
        const auto from = (size_t) (fromSeconds * kSr), to = juce::jmin (v.size(), (size_t) (toSeconds * kSr));
        double sum = 0.0;

        for (size_t i = from; i < to; ++i)
            sum += (double) v[i] * v[i];

        return std::sqrt (sum / (double) juce::jmax ((size_t) 1, to - from));
    }
}

//==============================================================================
LUTHIER_TEST (EBow, aHeldNoteIsSteadyWithinHalfASecondAtHalfIntensity)
{
    Rig rig;
    set (rig.processor, ParamIDs::ebowIntensity, 50.0f);
    rig.run (3.0, Rig::note (true));

    CHECK (rig.string >= 0);

    // Steady: from 500 ms on, within 1 dB of where it settles.
    const double settled = rig.levelAt (2.9);
    const double target = EBowDriver::targetLevelFor (0.5);

    CHECK_MSG (settled > 0.5 * target && settled < 1.5 * target,
               "settled at " + juce::String (settled, 4) + " for a target of " + juce::String (target, 4));

    for (double t = 0.5; t < 2.9; t += 0.1)
        CHECK_MSG (std::abs (juce::Decibels::gainToDecibels (rig.levelAt (t) / settled)) <= 1.0,
                   "at " + juce::String (t, 1) + " s the level is " + juce::String (rig.levelAt (t), 4)
                     + ", " + juce::String (juce::Decibels::gainToDecibels (rig.levelAt (t) / settled), 2)
                     + " dB from where it settles");
}

LUTHIER_TEST (EBow, switchingItOffSilencesTheStringWithin200ms)
{
    /*  2.4 is about the note: the string the E-Bow was driving. The guitar's
        other strings, set ringing sympathetically by two seconds of sustain,
        keep ringing as a real guitar's would - they are not the note. */
    Rig rig;
    rig.run (2.0, Rig::note (true));

    const double stringBefore = rms (rig.stringOut, 1.9, 2.0);
    CHECK_MSG (rig.processor.getEngine().getEBow().isDriving (rig.string), "the E-Bow was not driving the note");

    set (rig.processor, ParamIDs::ebowEnable, 0.0f);
    rig.run (0.5);

    const double stringAfter = rms (rig.stringOut, 2.2, 2.25);

    CHECK_MSG (stringBefore > 1.0e-3, "the E-Bow note never sounded");
    CHECK_MSG (juce::Decibels::gainToDecibels (stringAfter / stringBefore) < -60.0,
               "200 ms after the E-Bow let go the string was still at "
                 + juce::String (juce::Decibels::gainToDecibels (stringAfter / stringBefore), 1) + " dB");
}

LUTHIER_TEST (EBow, heldStringsLetGoOnReleaseChosenStringsDoNot)
{
    // Mask 0, "held strings": releasing the note is letting go.
    {
        Rig rig;
        rig.run (1.5, Rig::note (true));
        rig.run (1.0, Rig::note (false));
        CHECK_MSG (rig.levelAt (2.5) < 0.01 * rig.levelAt (1.4), "a released note kept its E-Bow");
    }

    // The string chosen explicitly: it keeps going after the note is released.
    {
        Rig probe;
        probe.run (0.2, Rig::note (true));
        const int s = probe.string;
        CHECK (s >= 0);

        Rig rig;
        set (rig.processor, ParamIDs::ebowStringMask, (float) (1 << juce::jmax (0, s)));
        rig.run (1.5, Rig::note (true));
        rig.run (1.0, Rig::note (false));
        CHECK_MSG (rig.levelAt (2.5) > 0.5 * rig.levelAt (1.4), "a chosen string stopped when its note was released");
    }
}

LUTHIER_TEST (EBow, theHarmonicChoiceTakesTheString)
{
    // With the 2nd partial chosen, the octave outgrows the fundamental.
    auto partialRatio = [] (int harmonicChoice)
    {
        Rig rig;
        set (rig.processor, ParamIDs::ebowHarmonic, (float) harmonicChoice);
        rig.run (3.0, Rig::note (true, 52));

        const double f0 = 440.0 * std::pow (2.0, (52 - 69) / 12.0);

        /*  REALISM-B: the strongest Hann-windowed bin within 1.5 % of each
            partial, not one Goertzel bin at the nominal frequency. A second
            of signal resolves 1 Hz, so a few cents of drift, stretch or a
            dead spot's detune put the old single bin on the skirt of the
            peak - the intermittent failure this test was known for. */
        auto power = [&rig] (double f)
        {
            // The render covers [0, 3 s) but only (int)(3*kSr/kBlock)*kBlock samples
            // exist (143 872, not 144 000), so a full kSr window from 2 s read 128
            // samples past the end of rig.out - uninitialised heap that passed when
            // zero-filled but intermittently collapsed the measured ratio under load.
            // Clamp the window to the samples that actually exist (both power() calls
            // share the same length, so the fundamental/octave comparison stays fair).
            const size_t from = (size_t) (2.0 * kSr);
            const size_t n = juce::jmin ((size_t) kSr,
                                         rig.out.size() > from ? rig.out.size() - from : (size_t) 0);
            double best = 0.0;

            for (double g = f * 0.985; g <= f * 1.015; g += f * 0.001)
            {
                double re = 0.0, im = 0.0;

                for (size_t i = 0; i < n; ++i)
                {
                    const double w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) i / (double) (n - 1));
                    const double ph = juce::MathConstants<double>::twoPi * g * (double) i / kSr;
                    re += rig.out[from + i] * w * std::cos (ph);
                    im -= rig.out[from + i] * w * std::sin (ph);
                }

                best = juce::jmax (best, re * re + im * im);
            }

            return best;
        };

        return power (2.0 * f0) / juce::jmax (1.0e-30, power (f0));
    };

    const double fundamental = partialRatio (0);
    const double second = partialRatio (1);

    CHECK_MSG (second > 4.0 * fundamental,
               "choosing the 2nd harmonic did not favour it: octave/fundamental "
                 + juce::String (second, 3) + " against " + juce::String (fundamental, 3));
}
