#pragma once

/*  A band on a bench (jam-mode.md 17): a JamEngine with the factory styles,
    a fixture host and a note list, rendered block by block. Shared by
    JamTests.cpp and JamDspTests.cpp. */

#include "TestFramework.h"
#include "../Jam/JamEngine.h"
#include <functional>

namespace luthier::tests
{

struct JamBench
{
    struct Note
    {
        int64_t sample = 0;
        juce::MidiMessage message;
    };

    double sr = 48000.0;
    int block = 128;
    int prepareBlock = 128;
    JamStyleLibrary library;
    JamEngine engine;
    JamEngine::Settings settings;
    JamEngine::BlockContext context;

    // The fixture host.
    bool host = false;
    bool hostPlaying = false;
    double hostPpq = 0.0;
    std::function<double (int64_t)> hostBpmAt;   ///< tempo per block start, else 120
    int numerator = 4, denominator = 4;
    bool sendBarStart = true;

    std::vector<Note> notes;
    std::vector<float> left, right;   ///< drums + bass, summed
    bool keepAudio = false;
    int64_t position = 0;

    std::function<void (JamBench&)> beforeBlock;

    explicit JamBench (double sampleRate = 48000.0, int blockSize = 128, int preparedBlock = -1)
        : sr (sampleRate), block (blockSize), prepareBlock (preparedBlock > 0 ? preparedBlock : blockSize)
    {
        engine.prepare (sr, prepareBlock);

        for (int i = 0; i < jam::kNumFactoryStyles; ++i)
            engine.setStyleSlot (i, library.getFactoryStyle (i));

        settings.enabled = true;
        settings.humanise = 0.0;
        settings.fillEvery = 0;
        settings.stopOnSilence = false;
        settings.dynamicsFollow = false;
        context.effectiveTempo = 120.0;
    }

    double bpmAt (int64_t s) const { return hostBpmAt ? hostBpmAt (s) : 120.0; }

    void chord (int64_t sample, std::initializer_list<int> pitches, int velocity = 100)
    {
        for (int p : pitches)
            notes.push_back ({ sample, juce::MidiMessage::noteOn (1, p, (juce::uint8) velocity) });
    }

    void release (int64_t sample, std::initializer_list<int> pitches)
    {
        for (int p : pitches)
            notes.push_back ({ sample, juce::MidiMessage::noteOff (1, p) });
    }

    int64_t samplesPerQuarter() const { return (int64_t) std::llround (60.0 * sr / 120.0); }

    /** Renders `count` samples. */
    void run (int64_t count)
    {
        std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.sample < b.sample; });
        const int64_t end = position + count;

        while (position < end)
        {
            const int n = (int) juce::jmin ((int64_t) block, end - position);

            // A host block bigger than prepare promised is sliced, as the processor does.
            for (int offset = 0; offset < n;)
            {
                const int m = juce::jmin (prepareBlock, n - offset);
                runSlice (m);
                offset += m;
            }
        }
    }

    void runSeconds (double seconds) { run ((int64_t) std::llround (seconds * sr)); }

private:
    void runSlice (int n)
    {
        if (beforeBlock)
            beforeBlock (*this);

        const double bpm = bpmAt (position);

        context.hasPlayHead = host;
        context.hostHasPpq = host;
        context.hostPlaying = host && hostPlaying;
        context.hostPpq = hostPpq;
        context.hostBpm = bpm;
        context.hostHasMeter = host;
        context.hostNumerator = numerator;
        context.hostDenominator = denominator;

        const double barLength = numerator * 4.0 / denominator;
        context.hostHasBarStart = host && sendBarStart;
        context.hostBarStartPpq = std::floor (hostPpq / barLength + 1.0e-9) * barLength;

        juce::MidiBuffer midi;

        for (const auto& note : notes)
            if (note.sample >= position && note.sample < position + n)
                midi.addEvent (note.message, (int) (note.sample - position));

        engine.setSettings (settings);
        engine.process (context, midi, nullptr, n);

        if (keepAudio)
            for (int i = 0; i < n; ++i)
            {
                left.push_back (engine.getDrums (0)[i] + engine.getBass (0)[i]);
                right.push_back (engine.getDrums (1)[i] + engine.getBass (1)[i]);
            }

        if (host && hostPlaying)
            hostPpq += (double) n * bpm / 60.0 / sr;

        position += n;
    }

public:
    //==========================================================================
    std::vector<JamCaptureEvent> events() const { return engine.getCapture().copyLastBars (0); }

    std::vector<JamCaptureEvent> noteOns (int part, int note = -1) const
    {
        std::vector<JamCaptureEvent> out;

        for (const auto& e : events())
            if (e.part == part && e.velocity > 0 && (note < 0 || e.note == note))
                out.push_back (e);

        return out;
    }

    /** The first bass note-on at or after `sample` whose pitch class is `pc`. */
    const JamCaptureEvent* firstBass (int pc, int64_t fromSample, std::vector<JamCaptureEvent>& store) const
    {
        store = noteOns (1);

        for (const auto& e : store)
            if (e.sample >= fromSample && e.note % 12 == pc)
                return &e;

        return nullptr;
    }
};

} // namespace luthier::tests
