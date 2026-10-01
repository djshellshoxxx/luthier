#include "FreezeOverlay.h"

namespace luthier
{

//==============================================================================
void FreezeOverlay::prepare (double sampleRate, int numChannels)
{
    sr = juce::jmax (8000.0, sampleRate);
    channels = juce::jlimit (1, 2, numChannels);

    // The longest window the spec allows, so a capture never allocates.
    const int maxSamples = (int) std::ceil (sr * kMaxCaptureMs / 1000.0) + 4;

    captured.setSize (2, maxSamples, false, true, true);

    filtersDirty = true;
    reset();
}

void FreezeOverlay::reset() noexcept
{
    captured.clear();

    state = State::idle;
    capturedSoFar = 0;
    captureLength = 0;
    loopLength = 0;
    crossfade = 0;
    readPosition = 0;
    envelope = 0.0;

    for (auto& f : lowpass)  f.reset();
    for (auto& f : highpass) f.reset();
}

//==============================================================================
void FreezeOverlay::setEnabled (bool shouldBeEnabled) noexcept
{
    if (shouldBeEnabled == enabled)
        return;

    enabled = shouldBeEnabled;

    if (enabled)
    {
        // "A new freeze replaces the layer" - unconditional, even mid-hold.
        beginCapture();
    }
    else if (state != State::idle)
    {
        state = State::releasing;
    }
}

void FreezeOverlay::beginCapture() noexcept
{
    captureLength = juce::jlimit (1, captured.getNumSamples(),
                                  (int) std::round (sr * captureMs / 1000.0));

    /*  The seam blend is taken out of the captured window rather than added to
        it, so the loop never reads past what was actually recorded. Keep it well
        under half the window or the blend would overlap itself. */
    crossfade = juce::jlimit (1, juce::jmax (1, captureLength / 4),
                              (int) std::round (sr * kCrossfadeMs / 1000.0));

    loopLength = juce::jmax (1, captureLength - crossfade);

    capturedSoFar = 0;
    readPosition = 0;
    envelope = 0.0;
    state = State::capturing;

    captured.clear();

    for (auto& f : lowpass)  f.reset();
    for (auto& f : highpass) f.reset();
}

void FreezeOverlay::buildLoop() noexcept
{
    /*  Hide the seam.

        Samples [loopLength, captureLength) are how the signal actually continued
        past the loop point. Fading that continuation out across the first
        `crossfade` samples, while the loop's own start fades in, means the join
        from the last sample of the loop to the first is continuous. Equal-gain
        rather than equal-power: the two sides are the same source a loop apart,
        so they are correlated and amplitudes add.
    */
    for (int channel = 0; channel < 2; ++channel)
    {
        auto* data = captured.getWritePointer (channel);

        for (int i = 0; i < crossfade; ++i)
        {
            const double t = (double) (i + 1) / (double) (crossfade + 1);

            data[i] = (float) ((1.0 - t) * (double) data[loopLength + i]
                                 + t * (double) data[i]);
        }
    }
}

//==============================================================================
void FreezeOverlay::setCaptureMs (double ms) noexcept
{
    captureMs = juce::jlimit (kMinCaptureMs, kMaxCaptureMs, ms);
}

void FreezeOverlay::setLevelDb (double db) noexcept
{
    // The spec's range is -inf to 0 dB, so anything at the floor is silence.
    // SPEC-SWEEP AR-10: the floor is freeze_level's own -60 dB bottom; it was
    // -90, which the parameter could never reach.
    levelLinear = (db <= -59.95) ? 0.0
                                : juce::Decibels::decibelsToGain (juce::jmin (0.0, db));
}

void FreezeOverlay::setAttackMs (double ms) noexcept
{
    attackMs = juce::jlimit (kMinAttackMs, kMaxAttackMs, ms);
}

void FreezeOverlay::setReleaseMs (double ms) noexcept
{
    releaseMs = juce::jlimit (kMinReleaseMs, kMaxReleaseMs, ms);
}

void FreezeOverlay::setLowpassHz (double hz) noexcept
{
    const double clamped = juce::jlimit (200.0, 20000.0, hz);

    if (std::abs (clamped - lowpassHz) > 1.0e-9)
    {
        lowpassHz = clamped;
        filtersDirty = true;
    }
}

void FreezeOverlay::setHighpassHz (double hz) noexcept
{
    const double clamped = juce::jlimit (20.0, 2000.0, hz);

    if (std::abs (clamped - highpassHz) > 1.0e-9)
    {
        highpassHz = clamped;
        filtersDirty = true;
    }
}

void FreezeOverlay::updateFilters() noexcept
{
    // Butterworth Q, so the pair is flat in the passband rather than peaking at
    // the corners, which would move the layer's level as the cutoffs are dialled.
    constexpr double q = 0.7071067811865476;

    const double lp = juce::jmin (lowpassHz, sr * 0.45);
    const double hp = juce::jmin (highpassHz, lp * 0.5);

    for (auto& f : lowpass)  f.setLowpass  (sr, lp, q);
    for (auto& f : highpass) f.setHighpass (sr, hp, q);

    filtersDirty = false;
}

//==============================================================================
void FreezeOverlay::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (state == State::idle)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (channels, buffer.getNumChannels());

    if (numSamples <= 0 || numChannels <= 0)
        return;

    if (filtersDirty)
        updateFilters();

    const double attackStep  = 1.0 / juce::jmax (1.0, sr * attackMs  / 1000.0);
    const double releaseStep = 1.0 / juce::jmax (1.0, sr * releaseMs / 1000.0);

    //--------------------------------------------------------------------------
    // Filling the window. The input passes through untouched; nothing is added
    // until there is a complete window to add.
    if (state == State::capturing)
    {
        const int wanted = juce::jmin (numSamples, captureLength - capturedSoFar);

        for (int channel = 0; channel < numChannels; ++channel)
            captured.copyFrom (channel, capturedSoFar, buffer, channel, 0, wanted);

        // A mono source still needs both sides of the layer filled.
        if (numChannels == 1)
            captured.copyFrom (1, capturedSoFar, buffer, 0, 0, wanted);

        capturedSoFar += wanted;

        if (capturedSoFar < captureLength)
            return;

        buildLoop();

        state = State::holding;
        readPosition = 0;
    }

    //--------------------------------------------------------------------------
    // Holding: one read head over a loop whose seam is already hidden, so every
    // cycle is identical to the last.
    auto* const* read = captured.getArrayOfReadPointers();

    for (int i = 0; i < numSamples; ++i)
    {
        if (state == State::releasing)
        {
            envelope -= releaseStep;

            if (envelope <= 0.0)
            {
                envelope = 0.0;
                state = State::idle;
                return;
            }
        }
        else if (envelope < 1.0)
        {
            envelope = juce::jmin (1.0, envelope + attackStep);
        }

        const double gain = envelope * levelLinear;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const double grain = (double) read[channel][readPosition];

            double shaped = highpass[(size_t) channel].process (grain);
            shaped = lowpass[(size_t) channel].process (shaped);

            buffer.getWritePointer (channel)[i] += (float) sanitise (shaped * gain);
        }

        if (++readPosition >= loopLength)
            readPosition = 0;
    }
}

} // namespace luthier
