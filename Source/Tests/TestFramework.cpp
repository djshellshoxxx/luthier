#include "TestFramework.h"

namespace luthier::tests
{

namespace
{
    /** Copies into a power-of-two buffer, windows it, and runs a real FFT.
        Returns the magnitude spectrum and the FFT size actually used. */
    int prepareSpectrum (const double* samples, int numSamples,
                         std::vector<float>& magnitudes)
    {
        int order = 10;

        while ((1 << (order + 1)) <= numSamples && order < 16)
            ++order;

        const int fftSize = 1 << order;

        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) fftSize * 2, 0.0f);

        // Hann window: the sidelobes of a rectangular window swamp the partials
        // we are trying to measure.
        for (int i = 0; i < fftSize; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi
                                                   * i / (double) (fftSize - 1));
            data[(size_t) i] = (float) (samples[i] * w);
        }

        fft.performFrequencyOnlyForwardTransform (data.data());

        magnitudes.assign (data.begin(), data.begin() + fftSize / 2);
        return fftSize;
    }
}

//==============================================================================
double findPeakFrequency (const double* samples, int numSamples, double sampleRate,
                          double minHz, double maxHz)
{
    if (samples == nullptr || numSamples < 64)
        return 0.0;

    std::vector<float> mags;
    const int fftSize = prepareSpectrum (samples, numSamples, mags);

    const double binHz = sampleRate / (double) fftSize;
    const int firstBin = juce::jmax (1, (int) (minHz / binHz));
    const int lastBin = juce::jmin ((int) mags.size() - 2, (int) (maxHz / binHz));

    int bestBin = firstBin;
    float bestMag = 0.0f;

    for (int i = firstBin; i <= lastBin; ++i)
    {
        if (mags[(size_t) i] > bestMag)
        {
            bestMag = mags[(size_t) i];
            bestBin = i;
        }
    }

    if (bestMag <= 0.0f || bestBin <= 0 || bestBin + 1 >= (int) mags.size())
        return 0.0;

    // Parabolic interpolation around the peak, which gets well inside one bin.
    const double y0 = mags[(size_t) (bestBin - 1)];
    const double y1 = mags[(size_t) bestBin];
    const double y2 = mags[(size_t) (bestBin + 1)];

    const double denom = y0 - 2.0 * y1 + y2;
    const double offset = (std::abs (denom) > 1.0e-12) ? 0.5 * (y0 - y2) / denom : 0.0;

    return ((double) bestBin + juce::jlimit (-0.5, 0.5, offset)) * binHz;
}

//==============================================================================
std::vector<double> findPartials (const double* samples, int numSamples, double sampleRate,
                                  int howMany, double fundamentalHz)
{
    std::vector<double> result;

    if (samples == nullptr || numSamples < 64 || fundamentalHz <= 0.0)
        return result;

    std::vector<float> mags;
    const int fftSize = prepareSpectrum (samples, numSamples, mags);
    const double binHz = sampleRate / (double) fftSize;

    for (int n = 1; n <= howMany; ++n)
    {
        // Search a window around where a purely harmonic partial would sit; wide
        // enough to catch stretching, narrow enough not to catch its neighbours.
        const double nominal = fundamentalHz * n;

        if (nominal > sampleRate * 0.45)
            break;

        const int centre = (int) std::round (nominal / binHz);
        const int span = juce::jmax (2, (int) (fundamentalHz * 0.35 / binHz));

        const int from = juce::jmax (1, centre - span);
        const int to = juce::jmin ((int) mags.size() - 2, centre + span);

        int bestBin = from;
        float bestMag = 0.0f;

        for (int i = from; i <= to; ++i)
        {
            if (mags[(size_t) i] > bestMag)
            {
                bestMag = mags[(size_t) i];
                bestBin = i;
            }
        }

        if (bestMag <= 0.0f)
        {
            result.push_back (0.0);
            continue;
        }

        const double y0 = mags[(size_t) juce::jmax (1, bestBin - 1)];
        const double y1 = mags[(size_t) bestBin];
        const double y2 = mags[(size_t) juce::jmin ((int) mags.size() - 1, bestBin + 1)];

        const double denom = y0 - 2.0 * y1 + y2;
        const double offset = (std::abs (denom) > 1.0e-12) ? 0.5 * (y0 - y2) / denom : 0.0;

        result.push_back (((double) bestBin + juce::jlimit (-0.5, 0.5, offset)) * binHz);
    }

    return result;
}

//==============================================================================
double rms (const double* samples, int numSamples)
{
    if (samples == nullptr || numSamples <= 0)
        return 0.0;

    double sum = 0.0;

    for (int i = 0; i < numSamples; ++i)
        sum += samples[i] * samples[i];

    return std::sqrt (sum / (double) numSamples);
}

double peak (const double* samples, int numSamples)
{
    double p = 0.0;

    for (int i = 0; i < numSamples; ++i)
        p = juce::jmax (p, std::abs (samples[i]));

    return p;
}

//==============================================================================
double measureDecayTime (const double* samples, int numSamples, double sampleRate, double db)
{
    if (samples == nullptr || numSamples <= 0)
        return -1.0;

    // Envelope in short windows, so a zero crossing is not mistaken for silence.
    const int window = juce::jmax (16, (int) (sampleRate * 0.005));

    double peakEnvelope = 0.0;
    int peakIndex = 0;

    std::vector<double> envelope;
    envelope.reserve ((size_t) (numSamples / window + 1));

    for (int i = 0; i + window <= numSamples; i += window)
    {
        const double e = rms (samples + i, window);
        envelope.push_back (e);

        if (e > peakEnvelope)
        {
            peakEnvelope = e;
            peakIndex = (int) envelope.size() - 1;
        }
    }

    if (peakEnvelope <= 1.0e-12)
        return -1.0;

    const double target = peakEnvelope * std::pow (10.0, -std::abs (db) / 20.0);

    for (size_t i = (size_t) peakIndex; i < envelope.size(); ++i)
        if (envelope[i] <= target)
            return (double) ((int) i - peakIndex) * window / sampleRate;

    return -1.0;
}

//==============================================================================
double measureThd (const double* samples, int numSamples, double sampleRate, double fundamentalHz)
{
    const auto partials = findPartials (samples, numSamples, sampleRate, 10, fundamentalHz);

    if (partials.empty())
        return 0.0;

    std::vector<float> mags;
    const int fftSize = prepareSpectrum (samples, numSamples, mags);
    const double binHz = sampleRate / (double) fftSize;

    auto magnitudeAt = [&mags, binHz] (double hz)
    {
        const int bin = (int) std::round (hz / binHz);

        if (! juce::isPositiveAndBelow (bin, (int) mags.size()))
            return 0.0;

        return (double) mags[(size_t) bin];
    };

    const double fundamental = magnitudeAt (fundamentalHz);

    if (fundamental <= 1.0e-12)
        return 0.0;

    double harmonicPower = 0.0;

    for (size_t i = 1; i < partials.size(); ++i)
    {
        const double m = magnitudeAt (partials[i]);
        harmonicPower += m * m;
    }

    return std::sqrt (harmonicPower) / fundamental;
}

} // namespace luthier::tests
