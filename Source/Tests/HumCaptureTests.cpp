/*  Sung / hummed melody capture (tune-builder.md 13, 15-09).
    TUNE-HELP-ONBOARDING workstream. */

#include "TestFramework.h"

#include "../DSP/Common/PitchTracker.h"
#include "../PluginProcessor.h"
#include "../Tune/TuneHarmony.h"
#include "../Tune/TuneHumCapture.h"
#include "../UI/TunePanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    struct HummedNote { double startBeat, beats; int midi; };

    /** A voice-like hum: a few harmonics falling off, 5 Hz vibrato of +-20
        cents, a little breath noise, 15 ms swells in and out and a short
        breath between notes - a person, not a sine generator. */
    std::vector<float> hum (const std::vector<HummedNote>& melody, double bpm, double totalBeats, juce::uint32 seed = 7)
    {
        const double secondsPerBeat = 60.0 / bpm;
        std::vector<float> out ((size_t) (totalBeats * secondsPerBeat * kSr), 0.0f);
        juce::Random random ((juce::int64) seed);

        for (const auto& n : melody)
        {
            const auto from = (size_t) (n.startBeat * secondsPerBeat * kSr);
            const auto length = (size_t) ((n.beats * secondsPerBeat - 0.04) * kSr);   // a breath before the next
            const double f0 = 440.0 * std::pow (2.0, (n.midi - 69) / 12.0);
            double phase = 0.0;

            for (size_t i = 0; i < length && from + i < out.size(); ++i)
            {
                const double t = (double) i / kSr;
                const double cents = 20.0 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * t);
                phase += 2.0 * juce::MathConstants<double>::pi * f0 * std::pow (2.0, cents / 1200.0) / kSr;

                double v = 0.0;

                for (int h = 1; h <= 5; ++h)
                    v += std::sin (h * phase) / (h * h);

                const double swell = juce::jmin (1.0, juce::jmin (t / 0.015, ((double) length / kSr - t) / 0.015));
                out[from + i] += (float) (0.3 * v * juce::jmax (0.0, swell));
            }
        }

        for (auto& s : out)
            s += 0.003f * (random.nextFloat() * 2.0f - 1.0f);

        return out;
    }
}

//==============================================================================
LUTHIER_TEST (HumCapture, thePitchTrackerFindsAVoicesPitchAndDoubtsNoise)
{
    PitchTracker tracker;
    tracker.prepare (kSr, 2048);

    for (int midi : { 45, 57, 64, 72, 81 })
    {
        const auto signal = hum ({ { 0.0, 2.0, midi } }, 120.0, 2.0);
        const auto e = tracker.analyse (signal.data() + 4800);
        CHECK_MSG (std::abs (frequencyToMidi (e.frequency) - midi) < 0.3,
                   "MIDI " + juce::String (midi) + " read as " + juce::String (frequencyToMidi (e.frequency), 2));
        CHECK (e.confidence > 0.8);
    }

    std::vector<float> noise (2048);
    juce::Random random (3);

    for (auto& s : noise)
        s = random.nextFloat() * 2.0f - 1.0f;

    CHECK_MSG (tracker.analyse (noise.data()).confidence < TuneHumCapture::kMinConfidence, "noise passed as a pitch");
}

//==============================================================================
/*  15-09: "a fixture hum of a known melody is transcribed with pitch correct
    to the nearest semitone in 95% of notes and rhythm correct to the quantise
    grid." */
LUTHIER_TEST (HumCapture, aHummedMelodyIsTranscribedToTheSemitoneAndTheGrid)
{
    Tune tune;
    TuneSection verse;
    verse.name = "Verse";
    verse.lengthBars = 4;
    tune.addSection (verse);
    applyProgressionText (tune, 0, "C F G C");

    // Sixteen notes on the eighth-note grid, up and down the C major scale.
    const std::vector<HummedNote> melody =
    {
        { 0.0, 1.0, 60 }, { 1.0, 0.5, 62 }, { 1.5, 0.5, 64 }, { 2.0, 1.0, 65 }, { 3.0, 1.0, 67 },
        { 4.0, 1.5, 69 }, { 5.5, 0.5, 67 }, { 6.0, 1.0, 65 }, { 7.0, 1.0, 64 },
        { 8.0, 0.5, 62 }, { 8.5, 0.5, 64 }, { 9.0, 1.0, 67 }, { 10.0, 2.0, 72 },
        { 12.0, 1.0, 71 }, { 13.0, 1.0, 67 }, { 14.0, 2.0, 60 }
    };

    const double bpm = 100.0;
    const auto audio = hum (melody, bpm, 16.0);

    TuneHumCapture capture;
    capture.prepare (kSr);
    capture.begin (bpm, 0.0);
    capture.setArmed (true);

    // In the audio thread's blocks, with the UI draining between them.
    for (size_t pos = 0; pos < audio.size(); pos += 480)
    {
        const float* channels[] = { audio.data() + pos };
        capture.pushAudio (channels, 1, (int) juce::jmin ((size_t) 480, audio.size() - pos));

        if ((pos / 480) % 10 == 0)
            capture.process();
    }

    const auto notes = capture.finish (tune, 0, QuantiseGrid::eighth, true);
    CHECK_MSG (notes.size() == melody.size(), "transcribed " + juce::String ((int) notes.size()) + " notes of "
                                                + juce::String ((int) melody.size()));

    int pitchRight = 0, rhythmRight = 0;

    for (const auto& expected : melody)
    {
        for (const auto& n : notes)
        {
            if (std::abs (n.startBeat - expected.startBeat) < 1.0e-6)
            {
                ++rhythmRight;
                pitchRight += n.pitch.value == expected.midi ? 1 : 0;
                break;
            }
        }
    }

    const double pitchShare = (double) pitchRight / (double) melody.size();
    CHECK_MSG (pitchShare >= 0.95, "pitch right on " + juce::String (pitchShare * 100.0, 0) + "% of notes");
    CHECK_MSG (rhythmRight >= (int) melody.size() - 1, "only " + juce::String (rhythmRight) + " notes on the grid");

    for (const auto& n : notes)
    {
        CHECK_NEAR (n.startBeat * 2.0, std::round (n.startBeat * 2.0), 1.0e-9);   // on the 1/8 grid
        CHECK (n.locked);
    }

    // Unarmed, nothing is taken in.
    capture.begin (bpm, 0.0);
    capture.setArmed (false);
    const float* channels[] = { audio.data() };
    capture.pushAudio (channels, 1, 4800);
    capture.process();
    CHECK (capture.getFrames().empty());
}

//==============================================================================
/*  13 in the TUNE tab: Sing captures against the section and writes the
    melody as a `sing` take, one undo step. */
LUTHIER_TEST (HumCapture, singingIntoTheTuneTabWritesTheSectionsMelody)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, 480);

    Tune tune;
    tune.meta.tempoBpm = 100.0;
    TuneSection verse;
    verse.name = "Verse";
    verse.lengthBars = 2;
    tune.addSection (verse);
    applyProgressionText (tune, 0, "C G");
    processor->getTuneSession().newTune (tune);

    TunePanel panel (*processor, processor->getTunePlayer(), processor->getTuneSession());
    CHECK (panel.startSinging());
    CHECK (processor->getHumCapture().isArmed());

    const auto audio = hum ({ { 0.0, 2.0, 64 }, { 2.0, 2.0, 67 }, { 4.0, 4.0, 72 } }, 100.0, 8.0);

    for (size_t pos = 0; pos < audio.size(); pos += 480)
    {
        const float* channels[] = { audio.data() + pos };
        processor->getHumCapture().pushAudio (channels, 1, (int) juce::jmin ((size_t) 480, audio.size() - pos));
        processor->getHumCapture().process();
    }

    CHECK (panel.stopSinging());
    processor->getTunePlayer().stop();

    const auto* s = processor->getTuneSession().getTune().getSection (0);
    CHECK (s != nullptr && s->melody.has_value());

    if (s != nullptr && s->melody.has_value())
    {
        CHECK (s->melody->source == MelodySource::sing);
        CHECK (s->melody->notes.size() == 3);

        if (s->melody->notes.size() == 3)
        {
            CHECK (s->melody->notes[0].pitch.value == 64);
            CHECK (s->melody->notes[1].pitch.value == 67);
            CHECK (s->melody->notes[2].pitch.value == 72);
        }
    }

    CHECK (processor->getTuneSession().getUndoDescription() == "Record melody (Sing)");
    CHECK (! panel.stopSinging());
}
