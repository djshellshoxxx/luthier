#include "RoomEngine.h"
#include "../../Support/QualityProfile.h"

namespace luthier
{

namespace
{
    struct RoomProfile
    {
        const char* name;
        double dimensionM;     ///< Characteristic dimension: sets the reflection times.
        double decaySeconds;   ///< RT60 with a neutral material.
        double diffusion;      ///< How quickly the early reflections thicken.
        double erLevel;        ///< Level of the early reflections.
        double lateLevel;
    };

    const RoomProfile kRooms[(size_t) RoomSize::NumRoomSizes] =
    {
        { "Iso Booth",     1.8,  0.18, 0.35, 0.55, 0.20 },
        { "Small Booth",   2.6,  0.32, 0.45, 0.65, 0.35 },
        { "Small Studio",  5.0,  0.55, 0.60, 0.70, 0.55 },
        { "Large Studio",  9.0,  1.10, 0.72, 0.60, 0.75 },
        { "Live Room",    14.0,  1.80, 0.80, 0.55, 0.90 },
        { "Concert Hall", 26.0,  2.60, 0.88, 0.40, 1.00 },
        { "Cathedral",    45.0,  6.50, 0.94, 0.30, 1.00 }
    };

    struct MaterialProfile
    {
        const char* name;
        double absorption;   ///< 1 = dead, 0 = fully reflective.
        double dampingHz;    ///< Where the reflections start losing top end.
    };

    const MaterialProfile kMaterials[(size_t) RoomMaterial::NumMaterials] =
    {
        { "Dry",   0.75,  2200.0 },
        { "Wood",  0.45,  5000.0 },
        { "Tile",  0.18, 11000.0 },
        { "Stone", 0.10,  8000.0 }
    };
}

//==============================================================================
void RoomEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;

    roomTap.setSize (2, juce::jmax (1, maxBlockSize), false, true, false);
    roomTap.clear();
    roomTapValid = false;
    roomTapSamples = 0;

    // Longest early reflection we support: a cathedral's far wall.
    erSize = juce::nextPowerOfTwo ((int) (sr * 0.5) + 16);
    erMask = erSize - 1;
    erBuffer.assign ((size_t) erSize, 0.0);
    erIndex = 0;

    for (int i = 0; i < kFdnSize; ++i)
        lines[i].assign ((size_t) (sr * 0.45) + 1, 0.0);   // rebuild's longest line

    for (int i = 0; i < kFdnSize; ++i)
        lineDamp[i].prepare (sr);

    blendSmooth.prepare (sr, constants::kParamSmoothSeconds);
    widthSmooth.prepare (sr, constants::kParamSmoothSeconds);
    blendSmooth.snapTo (0.2);
    widthSmooth.snapTo (0.6);

    dcL.prepare (sr, 12.0);
    dcR.prepare (sr, 12.0);

    rebuild();
    reset();
}

void RoomEngine::reset() noexcept
{
    roomTap.clear();
    roomTapValid = false;
    roomTapSamples = 0;

    std::fill (erBuffer.begin(), erBuffer.end(), 0.0);
    erIndex = 0;

    for (int i = 0; i < kFdnSize; ++i)
    {
        std::fill (lines[i].begin(), lines[i].end(), 0.0);
        lineIndex[i] = 0;
        lineDamp[i].reset();
    }

    tapFilterL.reset();
    tapFilterR.reset();
    dcL.reset();
    dcR.reset();

    blendSmooth.snapToTarget();
    widthSmooth.snapToTarget();
}

//==============================================================================
void RoomEngine::setRoomSize (RoomSize s) noexcept
{
    if (s == roomSize)
        return;

    roomSize = s;
    rebuild();
}

void RoomEngine::setMaterial (RoomMaterial m) noexcept
{
    if (m == material)
        return;

    material = m;
    rebuild();
}

void RoomEngine::setRoomBlend (double blend) noexcept
{
    blendSmooth.setTarget (juce::jlimit (0.0, 1.0, blend));
}

void RoomEngine::setDecayScale (double scale) noexcept
{
    const double clamped = juce::jlimit (0.25, 4.0, scale);

    // Sent every block by the parameter bridge: only the loop gain depends on it,
    // and rebuilding would clear the late reverb every block.
    if (clamped == decayScale)
        return;

    decayScale = clamped;
    updateFeedbackGain();
}

void RoomEngine::setWidth (double width) noexcept
{
    widthSmooth.setTarget (juce::jlimit (0.0, 1.0, width));
}

//==============================================================================
void RoomEngine::rebuild()
{
    const auto& room = kRooms[(size_t) juce::jlimit (0, (int) RoomSize::NumRoomSizes - 1, (int) roomSize)];
    const auto& mat = kMaterials[(size_t) juce::jlimit (0, (int) RoomMaterial::NumMaterials - 1, (int) material)];

    // ---- early reflections ---------------------------------------------------
    // Reflection times come from the geometry: sound covers the characteristic
    // dimension in dimension / 343 seconds, and each successive reflection has
    // travelled further and lost more energy to the surfaces.
    const double baseDelay = room.dimensionM / 343.0;

    RtRandom rng { 0x8007ACC5ull };

    for (int i = 0; i < kNumTaps; ++i)
    {
        // Irregular spacing: parallel walls in a real room are never quite
        // parallel, and evenly-spaced taps ring.
        const double spread = 0.35 + (double) i * (0.55 + room.diffusion * 0.9);
        const double jitter = 0.85 + rng.nextDouble() * 0.30;
        const double seconds = baseDelay * spread * jitter;

        tapDelays[i] = juce::jlimit (1, erSize - 2, (int) (seconds * sr));

        // Each reflection has bounced (i/2 + 1) times, losing (1 - absorption)
        // of its energy each time.
        const double bounces = 1.0 + (double) i * 0.5;
        const double gain = std::pow (1.0 - mat.absorption, bounces * 0.45)
                            / (1.0 + (double) i * 0.35);

        // Alternate which side each reflection arrives from.
        const double pan = (i % 2 == 0) ? 0.72 : 0.28;

        tapGainsL[i] = gain * pan;
        tapGainsR[i] = gain * (1.0 - pan);
    }

    computeTapCompensation();

    tapFilterL.setLowpass (sr, juce::jmin (mat.dampingHz, sr * 0.46), 0.707);
    tapFilterR.setLowpass (sr, juce::jmin (mat.dampingHz * 0.94, sr * 0.46), 0.707);

    // ---- late reverb ----------------------------------------------------------
    static const int primes[kFdnSize] = { 1447, 1621, 1789, 1951, 2113, 2297, 2459, 2647 };

    const double scale = juce::jlimit (0.25, 6.0, room.dimensionM / 9.0) * (sr / 44100.0);

    for (int i = 0; i < kFdnSize; ++i)
    {
        lineLengths[i] = juce::jlimit (64, (int) (sr * 0.45), (int) (primes[i] * scale));

        // Sized in prepare for the longest line, so a room change never
        // reallocates under the audio thread.
        if (lines[i].size() < (size_t) lineLengths[i])
            lines[i].assign ((size_t) lineLengths[i], 0.0);
        else
            std::fill (lines[i].begin(), lines[i].begin() + lineLengths[i], 0.0);

        lineIndex[i] = 0;
        lineDamp[i].setCutoff (juce::jmin (mat.dampingHz, sr * 0.46));
    }

    updateFeedbackGain();
}

void RoomEngine::computeTapCompensation() noexcept
{
    // Merge taps at the same delay: exactly the same output, fewer reads.
    reducedCount = 0;

    for (int i = 0; i < kNumTaps; ++i)
    {
        int k = 0;

        while (k < reducedCount && reducedDelays[k] != tapDelays[i])
            ++k;

        if (k == reducedCount)
        {
            reducedDelays[k] = tapDelays[i];
            reducedGainsL[k] = reducedGainsR[k] = 0.0;
            ++reducedCount;
        }

        reducedGainsL[k] += tapGainsL[i];
        reducedGainsR[k] += tapGainsR[i];
    }

    // Loudest first.
    for (int i = 1; i < reducedCount; ++i)
        for (int j = i; j > 0; --j)
        {
            const double a = reducedGainsL[j] * reducedGainsL[j] + reducedGainsR[j] * reducedGainsR[j];
            const double b = reducedGainsL[j - 1] * reducedGainsL[j - 1] + reducedGainsR[j - 1] * reducedGainsR[j - 1];

            if (a <= b)
                break;

            std::swap (reducedDelays[j], reducedDelays[j - 1]);
            std::swap (reducedGainsL[j], reducedGainsL[j - 1]);
            std::swap (reducedGainsR[j], reducedGainsR[j - 1]);
        }

    // Energy compensation for the first n merged taps.
    double allL = 0.0, allR = 0.0;

    for (int k = 0; k < reducedCount; ++k)
    {
        allL += reducedGainsL[k] * reducedGainsL[k];
        allR += reducedGainsR[k] * reducedGainsR[k];
    }

    double sumL = 0.0, sumR = 0.0;
    reducedCompL[0] = reducedCompR[0] = 1.0;

    for (int n = 1; n <= kNumTaps; ++n)
    {
        if (n <= reducedCount)
        {
            sumL += reducedGainsL[n - 1] * reducedGainsL[n - 1];
            sumR += reducedGainsR[n - 1] * reducedGainsR[n - 1];
        }

        reducedCompL[n] = sumL > 0.0 ? std::sqrt (allL / sumL) : 1.0;
        reducedCompR[n] = sumR > 0.0 ? std::sqrt (allR / sumR) : 1.0;
    }
}

void RoomEngine::setTapCount (int count, bool hard) noexcept
{
    tapTarget = juce::jlimit (1, kNumTaps, count);
    reducedStep = 1.0 / juce::jmax (1.0, std::round (QualityProfile::kDroppedVoiceRampSeconds * sr));

    if (hard)
        reducedMix = tapTarget < kNumTaps ? 1.0 : 0.0;
}

void RoomEngine::updateFeedbackGain() noexcept
{
    const auto& room = kRooms[(size_t) juce::jlimit (0, (int) RoomSize::NumRoomSizes - 1, (int) roomSize)];
    const auto& mat = kMaterials[(size_t) juce::jlimit (0, (int) RoomMaterial::NumMaterials - 1, (int) material)];

    double avgLength = 0.0;

    for (int i = 0; i < kFdnSize; ++i)
        avgLength += (double) lineLengths[i];

    avgLength /= (double) kFdnSize;

    const double rt60 = juce::jlimit (0.05, 30.0, room.decaySeconds * decayScale
                                                   * (1.0 + (1.0 - mat.absorption) * 1.2));

    feedbackGain = std::exp (-6.907755 * avgLength / (rt60 * sr));
    feedbackGain = juce::jlimit (0.0, kMaxFeedback, feedbackGain);   // SPEC-SWEEP: EN-90
}

//==============================================================================
void RoomEngine::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (! enabled || numSamples <= 0 || numChannels <= 0 || erSize <= 0)
    {
        roomTapValid = false;
        roomTapSamples = 0;
        return;
    }

    // The tap only runs when it fits the buffer prepare() allocated; a host that
    // hands us a longer block than it promised gets no tap rather than a crash.
    const bool tapping = roomTapEnabled && roomTap.getNumChannels() >= 2
                           && roomTap.getNumSamples() >= numSamples;

    auto* tapL = tapping ? roomTap.getWritePointer (0) : nullptr;
    auto* tapR = tapping ? roomTap.getWritePointer (1) : nullptr;

    const auto& room = kRooms[(size_t) juce::jlimit (0, (int) RoomSize::NumRoomSizes - 1, (int) roomSize)];

    auto* left = buffer.getWritePointer (0);
    auto* right = (numChannels > 1) ? buffer.getWritePointer (1) : left;

    for (int n = 0; n < numSamples; ++n)
    {
        const double dryL = (double) left[n];
        const double dryR = (double) right[n];
        const double mono = (dryL + dryR) * 0.5;

        // ---- early reflections ------------------------------------------------
        erBuffer[(size_t) erIndex] = flushDenormal (mono);

        double erL = 0.0, erR = 0.0;

        // cpu-quality-modes 2.1: the full set, the reduced set, or both while switching.
        const double reducedGoal = tapTarget < kNumTaps ? 1.0 : 0.0;

        if (reducedMix != reducedGoal)
            reducedMix = reducedGoal > reducedMix ? juce::jmin (reducedGoal, reducedMix + reducedStep)
                                                  : juce::jmax (reducedGoal, reducedMix - reducedStep);

        if (reducedMix < 1.0)
        {
            for (int i = 0; i < kNumTaps; ++i)
            {
                const double s = erBuffer[(size_t) ((erIndex - tapDelays[i]) & erMask)];
                erL += s * tapGainsL[i];
                erR += s * tapGainsR[i];
            }
        }

        if (reducedMix > 0.0)
        {
            const int n = juce::jmin (tapTarget, reducedCount);
            double rL = 0.0, rR = 0.0;

            for (int k = 0; k < n; ++k)
            {
                const double s = erBuffer[(size_t) ((erIndex - reducedDelays[k]) & erMask)];
                rL += s * reducedGainsL[k];
                rR += s * reducedGainsR[k];
            }

            rL *= reducedCompL[n];
            rR *= reducedCompR[n];
            erL = erL * (1.0 - reducedMix) + rL * reducedMix;
            erR = erR * (1.0 - reducedMix) + rR * reducedMix;
        }

        erIndex = (erIndex + 1) & erMask;

        erL = tapFilterL.process (erL) * room.erLevel;
        erR = tapFilterR.process (erR) * room.erLevel;

        // ---- late reverb --------------------------------------------------------
        double taps[kFdnSize];

        for (int i = 0; i < kFdnSize; ++i)
            taps[i] = lines[i][(size_t) lineIndex[i]];

        double sum = 0.0;

        for (int i = 0; i < kFdnSize; ++i)
            sum += taps[i];

        const double coupling = sum * (2.0 / (double) kFdnSize);

        // The late tail is fed from the early reflections, not the dry signal:
        // in a real room the diffuse field is built up by the early bounces.
        const double lateInput = (erL + erR) * 0.35;

        for (int i = 0; i < kFdnSize; ++i)
        {
            double v = (taps[i] - coupling) * feedbackGain;
            v = lineDamp[i].process (v);
            v += lateInput * 0.3;

            lines[i][(size_t) lineIndex[i]] = flushDenormal (sanitise (v));
            lineIndex[i] = (lineIndex[i] + 1) % lineLengths[i];
        }

        double lateL = 0.0, lateR = 0.0;

        for (int i = 0; i < kFdnSize; ++i)
        {
            if (i % 2 == 0) lateL += taps[i];
            else            lateR += taps[i];
        }

        const double norm = (2.0 / (double) kFdnSize) * room.lateLevel;
        lateL *= norm;
        lateR *= norm;

        // ---- blend --------------------------------------------------------------
        const double blend = blendSmooth.next();
        const double width = widthSmooth.next();

        double wetL = erL + lateL;
        double wetR = erR + lateR;

        // Width collapses the two room mics toward the centre rather than
        // inverting anything, so the room stays mono-compatible.
        const double wetMid = (wetL + wetR) * 0.5;
        wetL = wetMid + (wetL - wetMid) * width;
        wetR = wetMid + (wetR - wetMid) * width;

        wetL = dcL.process (wetL);
        wetR = dcR.process (wetR);

        left[n] = (float) sanitise (dryL * (1.0 - blend) + wetL * blend);

        if (numChannels > 1)
            right[n] = (float) sanitise (dryR * (1.0 - blend) + wetR * blend);

        if (tapping)
        {
            // The room alone, at the blend it is being heard at, so that adding
            // it back to the attenuated dry signal reconstructs the output.
            tapL[n] = (float) sanitise (wetL * blend);
            tapR[n] = (float) sanitise ((numChannels > 1) ? wetR * blend : wetL * blend);
        }
    }

    roomTapValid = tapping;
    roomTapSamples = tapping ? numSamples : 0;
}

//==============================================================================
const char* RoomEngine::getRoomSizeName (RoomSize s) noexcept
{
    return kRooms[(size_t) juce::jlimit (0, (int) RoomSize::NumRoomSizes - 1, (int) s)].name;
}

const char* RoomEngine::getMaterialName (RoomMaterial m) noexcept
{
    return kMaterials[(size_t) juce::jlimit (0, (int) RoomMaterial::NumMaterials - 1, (int) m)].name;
}

} // namespace luthier
