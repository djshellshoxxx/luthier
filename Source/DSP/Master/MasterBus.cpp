#include "MasterBus.h"

namespace luthier
{

void MasterBus::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;

    gainSmooth.prepare (sr, constants::kParamSmoothSeconds);
    gainSmooth.snapTo (dbToGain (gainDb));

    ceilingLinear = dbToGain (kCeilingDb);

    // 1.5 ms lookahead: long enough to catch a pick transient, short enough that
    // the added latency is negligible next to the convolution stages.
    lookDelay = juce::jmax (1, (int) (sr * 0.0015));
    lookSize = juce::nextPowerOfTwo (lookDelay + juce::jmax (1, maxBlockSize) + 8);
    lookL.assign ((size_t) lookSize, 0.0f);
    lookR.assign ((size_t) lookSize, 0.0f);
    lookIndex = 0;

    limiterAttack = std::exp (-1.0 / (0.0005 * sr));
    limiterRelease = std::exp (-1.0 / (0.080 * sr));

    dcL.prepare (sr, 5.0);
    dcR.prepare (sr, 5.0);

    // ITU-R BS.1770 K-weighting: a +4 dB high shelf at 1681 Hz and a highpass
    // at 38 Hz, applied before the mean-square measurement.
    kShelfL.setHighShelf (sr, 1681.0, 0.7071, 4.0);
    kShelfR.setHighShelf (sr, 1681.0, 0.7071, 4.0);
    kHpL.setHighpass (sr, 38.0, 0.5);
    kHpR.setHighpass (sr, 38.0, 0.5);

    lufsWindow = juce::jmax (1, (int) (sr * 0.400));

    reset();
}

void MasterBus::reset() noexcept
{
    std::fill (lookL.begin(), lookL.end(), 0.0f);
    std::fill (lookR.begin(), lookR.end(), 0.0f);
    lookIndex = 0;
    limiterEnv = 0.0;

    dcL.reset();
    dcR.reset();
    kShelfL.reset();
    kShelfR.reset();
    kHpL.reset();
    kHpR.reset();

    lufsAccum = 0.0;
    lufsCount = 0;

    resetMeters();
}

void MasterBus::resetMeters() noexcept
{
    peakL.store (0.0);
    peakR.store (0.0);
    rmsL.store (0.0);
    rmsR.store (0.0);
    lufs.store (-70.0);
    grDb.store (0.0);
    clipping.store (false);
}

void MasterBus::setGainDb (double db) noexcept
{
    gainDb = juce::jlimit (-60.0, 12.0, db);
    gainSmooth.setTarget (dbToGain (gainDb));
}

//==============================================================================
void MasterBus::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0 || lookSize <= 0)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = (numChannels > 1) ? buffer.getWritePointer (1) : left;

    const int mask = lookSize - 1;

    double blockPeakL = 0.0, blockPeakR = 0.0;
    double sumSqL = 0.0, sumSqR = 0.0;
    double maxReduction = 0.0;
    bool anyLimiting = false;

    for (int i = 0; i < numSamples; ++i)
    {
        const double g = gainSmooth.next();

        double l = dcL.process ((double) left[i] * g);
        double r = dcR.process ((double) right[i] * g);

        l = sanitise (l);
        r = sanitise (r);

        // ---- LUFS (K-weighted mean square over a 400 ms window) --------------
        {
            const double kl = kHpL.process (kShelfL.process (l));
            const double kr = kHpR.process (kShelfR.process (r));
            lufsAccum += kl * kl + kr * kr;

            if (++lufsCount >= lufsWindow)
            {
                const double meanSquare = lufsAccum / (double) (lufsCount * 2);
                lufs.store (meanSquare > 1.0e-12 ? (-0.691 + 10.0 * std::log10 (meanSquare)) : -70.0);
                lufsAccum = 0.0;
                lufsCount = 0;
            }
        }

        // ---- lookahead limiter ------------------------------------------------
        if (limiterEnabled)
        {
            lookL[(size_t) lookIndex] = (float) l;
            lookR[(size_t) lookIndex] = (float) r;

            // Track the envelope of the INCOMING sample so the gain is already
            // down by the time that sample reaches the output.
            const double peak = juce::jmax (std::abs (l), std::abs (r));
            const double required = (peak > ceilingLinear) ? (ceilingLinear / peak) : 1.0;

            // The envelope holds the smallest required gain, releasing slowly.
            const double coeff = (required < limiterEnv || limiterEnv == 0.0) ? limiterAttack : limiterRelease;
            limiterEnv = required + (limiterEnv - required) * coeff;
            limiterEnv = juce::jlimit (0.0, 1.0, limiterEnv);

            const int readIndex = (lookIndex - lookDelay) & mask;
            l = (double) lookL[(size_t) readIndex] * limiterEnv;
            r = (double) lookR[(size_t) readIndex] * limiterEnv;

            lookIndex = (lookIndex + 1) & mask;

            if (limiterEnv < 0.999)
            {
                anyLimiting = true;
                maxReduction = juce::jmax (maxReduction, -gainToDb (juce::jmax (1.0e-6, limiterEnv)));
            }

            // Absolute backstop: nothing leaves above the ceiling, ever.
            l = juce::jlimit (-ceilingLinear, ceilingLinear, l);
            r = juce::jlimit (-ceilingLinear, ceilingLinear, r);
        }

        left[i] = (float) l;

        if (numChannels > 1)
            right[i] = (float) r;

        blockPeakL = juce::jmax (blockPeakL, std::abs (l));
        blockPeakR = juce::jmax (blockPeakR, std::abs (r));
        sumSqL += l * l;
        sumSqR += r * r;
    }

    // Meters decay rather than snapping, so the UI reads smoothly.
    const double decay = 0.72;

    peakL.store (juce::jmax (blockPeakL, peakL.load() * decay));
    peakR.store (juce::jmax (blockPeakR, peakR.load() * decay));
    rmsL.store (std::sqrt (sumSqL / (double) numSamples));
    rmsR.store (std::sqrt (sumSqR / (double) numSamples));
    grDb.store (maxReduction);
    clipping.store (anyLimiting);

    // Mirror to any extra channels the host gave us.
    for (int ch = 2; ch < numChannels; ++ch)
        buffer.copyFrom (ch, 0, buffer, ch % 2, 0, numSamples);
}

} // namespace luthier
