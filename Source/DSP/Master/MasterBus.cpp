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

    // output-normalization.md 4.1: the calibrated gain and the true-peak stage.
    normalizer.prepare (sr);
    ceilingStepDb = (kCeilingDb - (-1.0)) / (LoudnessNormalizer::kGlideSeconds * sr);
    bypassFadeLength = juce::jmax (1, 2 * lookDelay);

    // The box must fit inside the lookahead after the true-peak detector's own
    // latency, so a peak's required gain has been averaged in for the whole box
    // before the peak leaves.
    boxLength = juce::jmax (1, lookDelay - TruePeakDetector::kLatency - 2);
    minValues.assign ((size_t) lookSize, 1.0);
    minIndices.assign ((size_t) lookSize, 0);
    boxRing.assign ((size_t) lookSize, 1.0);

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

    gainSmooth.snapToTarget();

    // output-normalization.md 4.1 / 12: a re-prepare keeps the calibration.
    truePeakL.reset();
    truePeakR.reset();
    normPathRunning = false;
    ceilingNowDb = kCeilingDb;
    bypassFade = 0;
    minHead = minTail = boxIndex = 0;
    minCounter = 0;
    boxSum = (double) boxLength;
    std::fill (boxRing.begin(), boxRing.end(), 1.0);
    releasedEnv = 1.0;

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

    // output-normalization.md 4.1: a block-level branch. While the normalizer
    // is inactive this function runs exactly the loop below, as it always has.
    if (normalizationStageCompiledIn().load (std::memory_order_relaxed))
    {
        normalizer.beginBlock (timelineNext, resultStartSample);
        resultStartSample = -1;

        if (normalizer.isActive() || normPathRunning)
        {
            processBlockNormalized (buffer);
            timelineNext += numSamples;
            return;
        }

        timelineNext += numSamples;
    }

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
        /*  The look-ahead line runs whether the limiter is on or not. Fed only
            while it was on, switching it back on played up to 1.5 ms of stale
            audio from when it was last on, and toggling it moved the output by
            the look-ahead - a latency change nothing reported. */
        if (! limiterEnabled)
        {
            lookL[(size_t) lookIndex] = (float) l;
            lookR[(size_t) lookIndex] = (float) r;

            const int readIndex = (lookIndex - lookDelay) & mask;
            l = (double) lookL[(size_t) readIndex];
            r = (double) lookR[(size_t) readIndex];

            lookIndex = (lookIndex + 1) & mask;
            limiterEnv = 1.0;
        }
        else
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

//==============================================================================
/*  output-normalization.md 4.1: the loop above with three differences.
      - The normalizer's gain multiplies the master gain, at the same point.
      - The limiter runs whatever limiter_on says, detecting the 4x true peak
        of the incoming sample against a ceiling that glides (300 ms) from
        -0.3 dBFS to -1.0 dBTP while normalization is on, and back.
      - With limiter_on off, today's loop has no lookahead delay, so entering
        and leaving this path crossfades between the undelayed and the
        delayed signal over 2 x the lookahead instead of jumping 1.5 ms.
    Attack, release, lookahead and the backstop clamp are unchanged. */
void MasterBus::processBlockNormalized (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    auto* left = buffer.getWritePointer (0);
    auto* right = (numChannels > 1) ? buffer.getWritePointer (1) : left;

    const int mask = lookSize - 1;

    if (! normPathRunning)
    {
        normPathRunning = true;
        truePeakL.reset();
        truePeakR.reset();
        ceilingNowDb = kCeilingDb;
        bypassFade = 0;

        // Carry today's envelope in, so entering the path is seamless.
        const double start = juce::jlimit (1.0e-6, 1.0, limiterEnv > 0.0 ? limiterEnv : 1.0);
        minHead = minTail = boxIndex = 0;
        minCounter = 0;
        std::fill (boxRing.begin(), boxRing.end(), start);
        boxSum = start * boxLength;
        releasedEnv = start;

        if (! limiterEnabled)
        {
            // The lookahead holds stale audio from whenever the limiter last ran.
            std::fill (lookL.begin(), lookL.end(), 0.0f);
            std::fill (lookR.begin(), lookR.end(), 0.0f);
            limiterEnv = 1.0;
            bypassFadeIn = true;
            bypassFade = bypassFadeLength;
        }
    }

    const double ceilingTargetDb = normalizer.isEnabled() ? -1.0 : kCeilingDb;
    double ceilingLin = dbToGain (ceilingNowDb);

    double blockPeakL = 0.0, blockPeakR = 0.0;
    double sumSqL = 0.0, sumSqR = 0.0;
    double maxReduction = 0.0;
    bool anyLimiting = false;

    for (int i = 0; i < numSamples; ++i)
    {
        const double ng = normalizer.next();
        const double g = gainSmooth.next() * ng;

        if (gainLog != nullptr && gainLogPosition < gainLogCapacity)
            gainLog[gainLogPosition++] = (float) ng;

        if (ceilingNowDb != ceilingTargetDb)
        {
            ceilingNowDb = ceilingNowDb > ceilingTargetDb ? juce::jmax (ceilingTargetDb, ceilingNowDb - ceilingStepDb)
                                                          : juce::jmin (ceilingTargetDb, ceilingNowDb + ceilingStepDb);
            ceilingLin = dbToGain (ceilingNowDb);
        }

        double l = dcL.process ((double) left[i] * g);
        double r = dcR.process ((double) right[i] * g);

        l = sanitise (l);
        r = sanitise (r);

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

        // ---- lookahead limiter, forced on, true-peak detector ---------------------
        lookL[(size_t) lookIndex] = (float) l;
        lookR[(size_t) lookIndex] = (float) r;

        const double peak = (double) juce::jmax (truePeakL.process ((float) l), truePeakR.process ((float) r));
        const double required = (peak > ceilingLin) ? (ceilingLin / peak) : 1.0;

        // Sliding minimum of the required gain over the box.
        {
            const int dmask = (int) minValues.size() - 1;

            while (minTail != minHead && minValues[(size_t) ((minTail - 1) & dmask)] >= required)
                minTail = (minTail - 1) & dmask;

            minValues[(size_t) minTail] = required;
            minIndices[(size_t) minTail] = minCounter;
            minTail = (minTail + 1) & dmask;

            // The window outlasts the box by the detector's latency, so a peak's
            // requirement is still in it when the peak leaves (see prepare).
            while (minIndices[(size_t) minHead] <= minCounter - lookDelay)
                minHead = (minHead + 1) & dmask;

            ++minCounter;
        }

        const double windowMin = minValues[(size_t) minHead];

        // Box average of the minimum: the attack.
        boxSum += windowMin - boxRing[(size_t) boxIndex];
        boxRing[(size_t) boxIndex] = windowMin;
        if (++boxIndex >= boxLength)
            boxIndex = 0;

        if (boxIndex == 0)   // re-sum once per box, so rounding never accumulates
        {
            boxSum = 0.0;

            for (int k = 0; k < boxLength; ++k)
                boxSum += boxRing[(size_t) k];
        }
        const double attacked = juce::jlimit (0.0, 1.0, boxSum / boxLength);

        // The release is today's 80 ms.
        releasedEnv = juce::jmin (1.0, releasedEnv + (1.0 - releasedEnv) * (1.0 - limiterRelease));
        limiterEnv = juce::jlimit (0.0, 1.0, juce::jmin (attacked, releasedEnv));
        releasedEnv = limiterEnv;

        const int readIndex = (lookIndex - lookDelay) & mask;
        double dl = (double) lookL[(size_t) readIndex] * limiterEnv;
        double dr = (double) lookR[(size_t) readIndex] * limiterEnv;

        lookIndex = (lookIndex + 1) & mask;

        if (limiterEnv < 0.999)
        {
            anyLimiting = true;
            maxReduction = juce::jmax (maxReduction, -gainToDb (juce::jmax (1.0e-6, limiterEnv)));
        }

        dl = juce::jlimit (-ceilingLin, ceilingLin, dl);
        dr = juce::jlimit (-ceilingLin, ceilingLin, dr);

        if (bypassFade > 0)
        {
            // Between today's undelayed limiter-off output and the delayed one.
            const double t = (double) bypassFade / (double) bypassFadeLength;
            const double wDelayed = bypassFadeIn ? 1.0 - t : t;
            const double ul = juce::jlimit (-ceilingLin, ceilingLin, l);
            const double ur = juce::jlimit (-ceilingLin, ceilingLin, r);
            dl = ul * (1.0 - wDelayed) + dl * wDelayed;
            dr = ur * (1.0 - wDelayed) + dr * wDelayed;

            if (--bypassFade == 0 && ! bypassFadeIn)
            {
                // The rest of this block is today's limiter-off output.
                dl = ul;
                dr = ur;
            }
        }
        else if (! bypassFadeIn)
        {
            dl = l;
            dr = r;
        }

        left[i] = (float) dl;

        if (numChannels > 1)
            right[i] = (float) dr;

        blockPeakL = juce::jmax (blockPeakL, std::abs (dl));
        blockPeakR = juce::jmax (blockPeakR, std::abs (dr));
        sumSqL += dl * dl;
        sumSqR += dr * dr;
    }

    const double decay = 0.72;

    peakL.store (juce::jmax (blockPeakL, peakL.load() * decay));
    peakR.store (juce::jmax (blockPeakR, peakR.load() * decay));
    rmsL.store (std::sqrt (sumSqL / (double) numSamples));
    rmsR.store (std::sqrt (sumSqR / (double) numSamples));
    grDb.store (maxReduction);
    clipping.store (anyLimiting);

    for (int ch = 2; ch < numChannels; ++ch)
        buffer.copyFrom (ch, 0, buffer, ch % 2, 0, numSamples);

    // Leaving: once the gain rests at 0 dB and the ceiling is back at -0.3.
    // With the limiter off, first fade back to the undelayed signal.
    if (! normalizer.isActive() && ceilingNowDb == kCeilingDb && bypassFade == 0)
    {
        if (! limiterEnabled && bypassFadeIn)
        {
            bypassFadeIn = false;
            bypassFade = bypassFadeLength;
        }
        else
        {
            normPathRunning = false;
            bypassFadeIn = true;
        }
    }
}

} // namespace luthier
