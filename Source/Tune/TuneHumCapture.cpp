#include "TuneHumCapture.h"

#include <algorithm>

namespace luthier
{

void TuneHumCapture::prepare (double rate)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    hop = juce::jmax (32, (int) std::round (kHopSeconds * sampleRate));

    // ~43 ms frames: two periods of 60 Hz, short enough to follow a sung line.
    const int frame = juce::nextPowerOfTwo ((int) std::round (0.043 * sampleRate));
    tracker.prepare (sampleRate, frame, 65.0, 1200.0);

    const int capacity = juce::nextPowerOfTwo ((int) (kRingSeconds * sampleRate));
    fifo = std::make_unique<juce::AbstractFifo> (capacity);
    ring.assign ((size_t) capacity, 0.0f);
    mono.assign (8192, 0.0f);
    pending.reserve ((size_t) (frame + capacity));
    begin (tempo, 0.0);
}

void TuneHumCapture::begin (double tempoBpm, double beat)
{
    tempo = juce::jmax (1.0, tempoBpm);
    startBeat = beat;
    frames.clear();
    pending.clear();
    consumed = 0;

    if (fifo != nullptr)
        fifo->reset();
}

void TuneHumCapture::pushAudio (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (! isArmed() || fifo == nullptr || channels == nullptr || numChannels <= 0)
        return;

    int offset = 0;

    while (offset < numSamples)
    {
        const int n = juce::jmin (numSamples - offset, (int) mono.size());

        for (int i = 0; i < n; ++i)
        {
            float sum = 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
                if (channels[ch] != nullptr)
                    sum += channels[ch][offset + i];

            mono[(size_t) i] = sum / (float) numChannels;
        }

        // A full ring drops the newest audio rather than blocking the audio thread.
        int start1, size1, start2, size2;
        fifo->prepareToWrite (n, start1, size1, start2, size2);
        std::copy (mono.begin(), mono.begin() + size1, ring.begin() + start1);
        std::copy (mono.begin() + size1, mono.begin() + size1 + size2, ring.begin() + start2);
        fifo->finishedWrite (size1 + size2);

        offset += n;
    }
}

void TuneHumCapture::process()
{
    if (fifo == nullptr)
        return;

    int start1, size1, start2, size2;
    const int ready = fifo->getNumReady();
    fifo->prepareToRead (ready, start1, size1, start2, size2);
    pending.insert (pending.end(), ring.begin() + start1, ring.begin() + start1 + size1);
    pending.insert (pending.end(), ring.begin() + start2, ring.begin() + start2 + size2);
    fifo->finishedRead (size1 + size2);

    const int frame = tracker.getFrameSize();
    size_t read = 0;

    while (pending.size() - read >= (size_t) frame)
    {
        const auto estimate = tracker.analyse (pending.data() + read);

        Frame f;
        f.seconds = (double) (consumed + frame / 2) / sampleRate;
        f.midi = frequencyToMidi (estimate.frequency);
        f.confidence = estimate.confidence;
        f.rms = estimate.rms;
        frames.push_back (f);

        read += (size_t) hop;
        consumed += hop;
    }

    pending.erase (pending.begin(), pending.begin() + (std::ptrdiff_t) read);
}

std::vector<RecordedNote> TuneHumCapture::segment() const
{
    std::vector<RecordedNote> notes;

    if (frames.empty())
        return notes;

    double loudest = 0.0;

    for (const auto& f : frames)
        loudest = juce::jmax (loudest, f.rms);

    // Voiced: confident (13's 0.6) and not the room between phrases (-30 dB under the take's peak).
    const double gate = loudest * 0.0316;
    std::vector<int> pitch (frames.size(), -1);

    for (size_t i = 0; i < frames.size(); ++i)
        if (frames[i].confidence >= kMinConfidence && frames[i].rms >= gate && frames[i].midi > 20.0)
            pitch[i] = (int) std::lround (frames[i].midi);

    // A five-frame median over the voiced frames, so a note's attack or a
    // wobble across a semitone boundary does not split it.
    std::vector<int> smooth (pitch);

    for (size_t i = 0; i < pitch.size(); ++i)
    {
        if (pitch[i] < 0)
            continue;

        int window[5];
        int n = 0;

        for (int k = -2; k <= 2; ++k)
        {
            const auto j = (std::ptrdiff_t) i + k;

            if (j >= 0 && j < (std::ptrdiff_t) pitch.size() && pitch[(size_t) j] >= 0)
                window[n++] = pitch[(size_t) j];
        }

        std::sort (window, window + n);
        smooth[i] = window[n / 2];
    }

    const double hopSeconds = (double) hop / sampleRate;
    const double beatsPerSecond = tempo / 60.0;

    size_t i = 0;

    while (i < smooth.size())
    {
        if (smooth[i] < 0)
        {
            ++i;
            continue;
        }

        size_t j = i;
        double level = 0.0;

        while (j < smooth.size() && smooth[j] == smooth[i])
            level = juce::jmax (level, frames[j++].rms);

        const double from = frames[i].seconds - hopSeconds * 0.5;
        const double to = frames[j - 1].seconds + hopSeconds * 0.5;

        if (to - from >= kMinNoteSeconds)
        {
            RecordedNote note;
            note.startBeat = startBeat + from * beatsPerSecond;
            note.endBeat = startBeat + to * beatsPerSecond;
            note.pitch = smooth[i];
            note.velocity = juce::jlimit (1, 127, (int) std::lround (50.0 + 70.0 * level / juce::jmax (1.0e-9, loudest)));
            notes.push_back (note);
        }

        i = j;
    }

    return notes;
}

std::vector<MelodyNote> TuneHumCapture::finish (const Tune& tune, int sectionIndex, QuantiseGrid grid, bool snapToKey)
{
    process();
    return quantiseRecording (segment(), grid, tune, sectionIndex, false, snapToKey);
}

} // namespace luthier
