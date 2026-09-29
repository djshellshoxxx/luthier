#pragma once

/*  Tempo and pitch shift for the backing track (SPEC-SWEEP PT-28 / PT-29;
    practice-tools 3: "pitch shift -12 to +12 semitones without tempo change",
    "tempo shift 25% to 200% without pitch change").

    A WSOLA time-stretcher whose grains are read at the pitch ratio:

        - every grain is kGrainSize output samples, Hann-windowed, overlapped
          by half, so the windows sum to one;
        - consecutive grains start kHop x tempo input samples apart, which sets
          the duration;
        - inside a grain the input is read at the pitch ratio (cubic
          interpolation), which sets the pitch without touching the duration;
        - each grain's start is moved by up to kSearch samples to where it best
          continues the previous grain's waveform (the WSOLA step), which is
          what keeps sustained notes from phasing.

    Runs on the audio thread: prepare() allocates, process() does not, and its
    cost is bounded (one coarse and one fine correlation search per hop).
    Neutral settings are the caller's to bypass - see isNeutral().
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace luthier
{

class TimePitchShifter
{
public:
    static constexpr int kGrainSize = 2048;
    static constexpr int kHop = kGrainSize / 2;
    static constexpr int kSearch = 384;         ///< +/- samples a grain may move
    static constexpr int kCorrelation = 768;    ///< samples compared per candidate
    static constexpr int kRingSize = 1 << 15;   ///< input history and look-ahead

    /** True when the settings change nothing, so the caller can bypass. */
    static bool isNeutral (double tempoRatio, double pitchSemitones) noexcept
    {
        return std::abs (tempoRatio - 1.0) < 1.0e-6 && std::abs (pitchSemitones) < 1.0e-6;
    }

    /** The most input one call can ask for per output sample, for sizing the source. */
    static constexpr double kMaxInputPerOutput = 4.5;

    void prepare();
    void reset() noexcept;

    /*  Produces `numSamples` of stereo output. `pull (float* left, float* right, int count)`
        must write exactly `count` new input samples (silence past the end of
        the stream); it is called with at most `maxPull` samples at a time. */
    template <typename Pull>
    void process (float* left, float* right, int numSamples, double tempoRatio, double pitchSemitones,
                  int maxPull, Pull&& pull) noexcept
    {
        if (ring[0].empty())
        {
            juce::FloatVectorOperations::clear (left, numSamples);
            juce::FloatVectorOperations::clear (right, numSamples);
            return;
        }

        tempo = juce::jlimit (0.25, 2.0, tempoRatio);
        pitch = std::pow (2.0, juce::jlimit (-12.0, 12.0, pitchSemitones) / 12.0);

        for (int i = 0; i < numSamples; ++i)
        {
            if (outRead >= kHop)
            {
                makeGrain (maxPull, pull);
                outRead = 0;
            }

            left[i] = out[0][(size_t) outRead];
            right[i] = out[1][(size_t) outRead];
            ++outRead;
        }
    }

    /** Input samples buffered ahead of what has been played, for tests. */
    juce::int64 getInputCount() const noexcept { return inputCount; }

private:
    template <typename Pull>
    void makeGrain (int maxPull, Pull& pull) noexcept
    {
        // The input this grain can reach: its search window and its length at
        // the pitch ratio, plus the interpolator's neighbours.
        const double reach = analysisPos + kSearch + kGrainSize * pitch + 4.0;
        const double continuation = hasPrevious ? previousStart + kHop * pitch : analysisPos;
        const double needed = juce::jmax (reach, continuation + kCorrelation + 4.0);

        while ((double) inputCount < needed)
        {
            const int count = juce::jlimit (1, juce::jmax (1, (int) pullScratch[0].size()), maxPull);
            pull (pullScratch[0].data(), pullScratch[1].data(), count);

            for (int i = 0; i < count; ++i)
            {
                const auto at = (size_t) ((inputCount + i) & (kRingSize - 1));
                ring[0][at] = pullScratch[0][(size_t) i];
                ring[1][at] = pullScratch[1][(size_t) i];
            }

            inputCount += count;
        }

        const double start = hasPrevious ? bestStart (continuation) : analysisPos;

        // Overlap-add the grain, read at the pitch ratio.
        for (int j = 0; j < kGrainSize; ++j)
        {
            const double position = start + j * pitch;
            const float w = window[(size_t) j];
            accumulator[0][(size_t) j] += w * readInterpolated (0, position);
            accumulator[1][(size_t) j] += w * readInterpolated (1, position);
        }

        // The first hop is complete: it is the next output.
        for (int c = 0; c < 2; ++c)
        {
            std::copy (accumulator[(size_t) c].begin(), accumulator[(size_t) c].begin() + kHop, out[(size_t) c].begin());
            std::copy (accumulator[(size_t) c].begin() + kHop, accumulator[(size_t) c].end(), accumulator[(size_t) c].begin());
            std::fill (accumulator[(size_t) c].begin() + kHop, accumulator[(size_t) c].end(), 0.0f);
        }

        previousStart = start;
        hasPrevious = true;
        analysisPos += kHop * tempo;
    }

    /** WSOLA: the start near analysisPos whose waveform best continues `target`. */
    double bestStart (double target) const noexcept;

    float readInterpolated (int channel, double position) const noexcept;
    float mono (juce::int64 index) const noexcept;

    std::array<std::vector<float>, 2> ring, accumulator, out, pullScratch;
    std::vector<float> window;

    juce::int64 inputCount = 0;
    double analysisPos = 0.0, previousStart = 0.0;
    bool hasPrevious = false;
    int outRead = kHop;

    double tempo = 1.0, pitch = 1.0;
};

} // namespace luthier
