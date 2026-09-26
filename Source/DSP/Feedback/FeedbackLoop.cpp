#include "FeedbackLoop.h"

namespace luthier
{

namespace
{
    /** How strongly the air moves a string for a unit of amp output at the
        reference distance, facing the speaker. Set so that a loud high-gain
        amp at half a metre sustains a ringing note at full amount, and a clean
        amp at a fifth of it does not (FeedbackTests). */
    constexpr double kInjectionGain = 0.0205;

    constexpr double kReferenceDistance = 0.5;
    constexpr double kRingingLevel = 1.0e-4;
}

//==============================================================================
void FeedbackLoop::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = juce::jmax (1, maxBlockSize);

    // One block to break the loop, plus the longest trip through the air.
    const int longest = maxBlock + (int) std::ceil (kMaxDistance / kSpeedOfSound * sr) + 1;
    ring.assign ((size_t) longest + 1, 0.0);

    injectionRelease = std::exp (-1.0 / (0.150 * sr));

    setSettings (settings);
    reset();
}

void FeedbackLoop::reset() noexcept
{
    std::fill (ring.begin(), ring.end(), 0.0);
    writePos = 0;
    blockStartWrite = 0;

    for (auto& p : peaks)
        p.reset();

    designedHz.fill (0.0);
    ringing.fill (false);
    injectionEnv = 0.0;
}

void FeedbackLoop::setSettings (const FeedbackSettings& s) noexcept
{
    FeedbackSettings clamped;
    clamped.amount = juce::jlimit (0.0, 1.0, s.amount);
    clamped.distanceMetres = juce::jlimit (0.0, kMaxDistance, s.distanceMetres);
    clamped.angleDegrees = juce::jlimit (-180.0, 180.0, s.angleDegrees);
    clamped.focus = juce::jlimit (0.0, 1.0, s.focus);
    clamped.octaveBias = juce::jlimit (-2, 2, s.octaveBias);

    const bool retune = clamped.focus != settings.focus || clamped.octaveBias != settings.octaveBias;

    // Switched on: whatever the ring held from before it was last switched off
    // is not what the amp is playing now.
    if (settings.amount <= 0.0 && clamped.amount > 0.0)
    {
        std::fill (ring.begin(), ring.end(), 0.0);
    }

    settings = clamped;

    delaySamples = maxBlock + (int) std::round (settings.distanceMetres / kSpeedOfSound * sr);

    if (retune)
        designedHz.fill (0.0);

    updateCoupling();
}

void FeedbackLoop::updateCoupling() noexcept
{
    // Sound pressure falls with distance (1/r), capped close in; a speaker is
    // directional, so turning away loses most of it but not all (the room).
    const double distanceGain = juce::jmin (4.0, kReferenceDistance / juce::jmax (0.05, settings.distanceMetres));
    const double cosine = std::cos (juce::degreesToRadians (settings.angleDegrees));
    const double angleGain = 0.25 + 0.75 * (0.5 + 0.5 * cosine);

    for (int s = 0; s < kMaxStrings; ++s)
    {
        // A heavy wound string is moved less by the same air.
        const double stringGain = woundString[(size_t) s] ? 0.7 : 1.0;
        couple[(size_t) s] = settings.amount * distanceGain * angleGain * stringGain * bodyCoupling;
    }
}

void FeedbackLoop::setBodyCoupling (double factor) noexcept
{
    factor = std::isfinite (factor) ? juce::jlimit (0.0, 4.0, factor) : 1.0;

    if (factor != bodyCoupling)
    {
        bodyCoupling = factor;
        updateCoupling();
    }
}

double FeedbackLoop::bodyCouplingFor (double chamberFeedbackGain) noexcept
{
    /*  part-acoustics 2.1's column runs solid 0.1 (lowest), chambered 0.25,
        semi-hollow 0.5, hollow 0.8, acoustic 0 ("n/a"). Relative to the solid
        body, on a square root: the loop's gain is an amplitude and it
        saturates at the ceiling, so a hollow body's 8x on the table is about
        2.8x here - it takes over sooner and louder, not instantly. Acoustic is
        n/a: this loop is a magnetic pickup hearing the amp, so it is left at
        the reference rather than switched off. */
    constexpr double kSolid = 0.1;

    if (! (chamberFeedbackGain > 0.0))
        return 1.0;

    return std::sqrt (chamberFeedbackGain / kSolid);
}

//==============================================================================
void FeedbackLoop::beginBlock (const double* stringHz, const double* stringLevels, const bool* wound,
                               int numStrings) noexcept
{
    blockStartWrite = writePos;

    bool woundChanged = false;
    const double q = 2.0 + settings.focus * 28.0;
    const double bias = std::pow (2.0, (double) settings.octaveBias);

    for (int s = 0; s < kMaxStrings; ++s)
    {
        const bool present = s < numStrings;
        const bool isWound = present && wound != nullptr && wound[s];

        if (isWound != woundString[(size_t) s])
        {
            woundString[(size_t) s] = isWound;
            woundChanged = true;
        }

        const bool nowRinging = present && stringLevels[s] > kRingingLevel;

        if (! nowRinging)
        {
            if (ringing[(size_t) s])
                peaks[(size_t) s].reset();

            ringing[(size_t) s] = false;
            continue;
        }

        ringing[(size_t) s] = true;

        // 1.1: the peak sits on the ringing note, or on the octave the bias
        // asks for. Redesigned when the pitch has moved, not every block.
        const double target = juce::jlimit (40.0, sr * 0.45, stringHz[s] * bias);

        if (std::abs (target - designedHz[(size_t) s]) > target * 0.0005)
        {
            peaks[(size_t) s].setBandpass (sr, target, q);
            designedHz[(size_t) s] = target;
        }
    }

    if (woundChanged)
        updateCoupling();
}

double FeedbackLoop::process (int s, double ampSample) noexcept
{
    if (! ringing[(size_t) s])
        return 0.0;

    /*  Linear in the amp's output, which already carries its level: a quiet
        amp moves little air. (Scaling by an envelope of it as well would make
        the loop fall twice as fast as the volume knob, where 1.4 has it fall
        by exactly the circuit's attenuation.) */
    const double raw = peaks[(size_t) s].process (ampSample) * couple[(size_t) s] * kInjectionGain;

    // Saturates like a real loop rather than growing without bound.
    const double injected = kInjectionCeiling * std::tanh (raw / kInjectionCeiling);

    const double magnitude = std::abs (injected);
    injectionEnv = magnitude > injectionEnv ? magnitude : injectionEnv * injectionRelease;

    return sanitise (injected);
}

void FeedbackLoop::pushAmpOutput (const double* amp, int numSamples) noexcept
{
    const int size = (int) ring.size();

    for (int i = 0; i < numSamples; ++i)
    {
        ring[(size_t) writePos] = sanitise (amp[i]);
        writePos = (writePos + 1) % size;
    }
}

} // namespace luthier
