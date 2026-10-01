#pragma once

/*  A monophonic pitch tracker (YIN: de Cheveigne and Kawahara, 2002).
    TUNE-HELP-ONBOARDING workstream, for tune-builder.md 13's sung / hummed
    melody capture; written to serve practice-tools.md 4's Bend Trainer as well,
    which the spec says shares it.

    analyse() looks at one frame and answers a frequency and a confidence in
    0..1 (1 minus YIN's cumulative-mean-normalised difference at the chosen
    lag): a clean periodic signal scores near 1, noise near 0, so the capture's
    "rejects samples with confidence below 0.6" is a threshold on this.
    Double precision throughout; everything is allocated in prepare(), so
    analyse() allocates nothing and could run on the audio thread.
*/

#include <vector>

namespace luthier
{

class PitchTracker
{
public:
    struct Estimate
    {
        double frequency = 0.0;    ///< Hz; 0 when nothing was found
        double confidence = 0.0;   ///< 0..1
        double rms = 0.0;          ///< the frame's level, for a gate
    };

    /** `frameSize` samples per analysis; the lowest detectable frequency is
        about 2 * sampleRate / frameSize. */
    void prepare (double sampleRate, int frameSize, double minFrequency = 60.0, double maxFrequency = 1500.0);

    int getFrameSize() const noexcept { return frameSize; }

    /** One frame of `frameSize` samples. */
    Estimate analyse (const float* samples) noexcept;

    /** YIN's absolute threshold: the first dip below it is taken. */
    static constexpr double kThreshold = 0.15;

private:
    double sampleRate = 48000.0;
    int frameSize = 2048, minLag = 32, maxLag = 800;
    std::vector<double> difference, cumulative;
};

/** Hz to fractional MIDI note. */
double frequencyToMidi (double hz) noexcept;

} // namespace luthier
