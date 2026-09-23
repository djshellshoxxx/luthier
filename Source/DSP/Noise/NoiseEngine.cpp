#include "NoiseEngine.h"

namespace luthier
{

//==============================================================================
juce::uint32 noiseHash (juce::uint32 seed, juce::uint32 index) noexcept
{
    // A 32-bit avalanche (lowbias32) over the seed and index. Stateless, so
    // event N's choices do not depend on how many events came before it.
    juce::uint32 x = seed ^ (index * 0x9e3779b9u);
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

double noiseUniform (juce::uint32 seed, juce::uint32 index) noexcept
{
    return (double) noiseHash (seed, index) / 4294967296.0;
}

const char* getNoiseClassName (NoiseClass c) noexcept
{
    switch (c)
    {
        case NoiseClass::squeak:     return "Squeak";
        case NoiseClass::pickClick:  return "Click";
        case NoiseClass::pickChirp:  return "Chirp";
        case NoiseClass::pickScrape: return "Scrape";
        case NoiseClass::fretBuzz:   return "Buzz";
        case NoiseClass::clank:      return "Clank";
        case NoiseClass::numClasses:
        default:                     return "";
    }
}

//==============================================================================
//  NoiseGenerator
//==============================================================================
void NoiseGenerator::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (8000.0, sampleRate);
    active = false;
}

void NoiseGenerator::updateResonators (double hz) noexcept
{
    const double nyquistGuard = sr * 0.45;
    const double f = juce::jlimit (40.0, nyquistGuard, hz);

    fundamental.setBandpass (sr, f, event.q);

    if (w2 > 0.0) second.setBandpass (sr, juce::jmin (f * 2.0, nyquistGuard), event.q * 1.2);
    if (w3 > 0.0) third.setBandpass (sr, juce::jmin (f * 3.0, nyquistGuard), event.q * 1.4);

    if (event.subResonanceRatio > 0.0)
        sub.setBandpass (sr, juce::jlimit (40.0, nyquistGuard, f * event.subResonanceRatio), event.q * 0.8);
}

void NoiseGenerator::start (const NoiseEvent& e, const std::vector<float>& textureTable,
                            juce::uint32 randomSeed) noexcept
{
    event = e;
    active = true;
    releasing = false;
    heldOpen = e.burstHz > 0.0;       // buzz holds until its contact ends
    sustainLevel = 1.0;

    table = textureTable.empty() ? nullptr : textureTable.data();
    tableSize = (int) textureTable.size();
    readIndex = tableSize > 0 ? (int) (randomSeed % (juce::uint32) tableSize) : 0;
    rng = randomSeed | 1u;

    w2 = juce::jlimit (0.0, 1.0, e.brightness);
    w3 = w2 * w2;

    fundamental.reset();
    second.reset();
    third.reset();
    sub.reset();
    updateResonators (e.startHz);

    sampleIndex = 0;
    attackSamples = juce::jmax (1, (int) (e.attackMs * 0.001 * sr));
    holdSamples = juce::jmax (0, (int) (e.holdMs * 0.001 * sr));
    glideSamples = juce::jmax (1, attackSamples + holdSamples);

    // -60 dB over decayMs.
    decayCoefficient = std::exp (std::log (0.001) / juce::jmax (1.0, e.decayMs * 0.001 * sr));
    envelope = 0.0;

    burstPhase = 0.0;
    burstIncrement = e.burstHz > 0.0 ? e.burstHz / sr : 0.0;
}

void NoiseGenerator::setSustainLevel (double newLevel) noexcept
{
    sustainLevel = juce::jlimit (0.0, 4.0, newLevel);
}

void NoiseGenerator::release() noexcept
{
    heldOpen = false;
}

double NoiseGenerator::process() noexcept
{
    if (! active)
        return 0.0;

    // ---- envelope -----------------------------------------------------------------
    if (sampleIndex < attackSamples)
        envelope = (double) (sampleIndex + 1) / (double) attackSamples;
    else if (sampleIndex < attackSamples + holdSamples || heldOpen)
        envelope = 1.0;
    else
        envelope *= decayCoefficient;

    // ---- glide ----------------------------------------------------------------------
    if (event.endHz != event.startHz && (sampleIndex % kRetune) == 0 && sampleIndex <= glideSamples)
    {
        const double t = juce::jmin (1.0, (double) sampleIndex / (double) glideSamples);

        // Geometric: a glide is heard in pitch, so it moves evenly in log frequency.
        updateResonators (event.startHz * std::pow (event.endHz / event.startHz, t));
    }

    ++sampleIndex;

    // ---- excitation --------------------------------------------------------------------
    double x;

    if (table != nullptr)
    {
        x = (double) table[readIndex];

        if (++readIndex >= tableSize)
            readIndex = 0;
    }
    else
    {
        rng = rng * 1664525u + 1013904223u;
        x = ((double) (rng >> 8) / 8388608.0) - 1.0;
    }

    if (burstIncrement > 0.0)
    {
        // One contact per string cycle: a short raised-cosine gate at the
        // start of each period.
        burstPhase += burstIncrement;

        if (burstPhase >= 1.0)
            burstPhase -= 1.0;

        x *= burstPhase < 0.18 ? 0.5 - 0.5 * std::cos (constants::kTwoPi * burstPhase / 0.18) : 0.0;
    }

    // ---- resonators ----------------------------------------------------------------------
    double y = fundamental.process (x);

    if (w2 > 0.0) y += w2 * second.process (x);
    if (w3 > 0.0) y += w3 * third.process (x);

    if (event.subResonanceRatio > 0.0)
        y += event.subResonanceLevel * sub.process (x);

    const double out = y * envelope * event.level * sustainLevel;

    if (sampleIndex > attackSamples + holdSamples && ! heldOpen && envelope < 1.0e-5)
        active = false;

    return sanitise (out);
}

//==============================================================================
//  NoiseEngine
//==============================================================================
NoiseEngine::NoiseEngine() = default;

void NoiseEngine::synthesiseTexture (std::vector<float>& table, NoiseTexture texture, juce::uint32 seed)
{
    table.assign ((size_t) kTextureLength, 0.0f);

    // Coarseness is the share of the texture that is sparse impulses rather
    // than smooth noise: a rough winding or a matte pick catches, and each
    // catch is a click. Metallic adds a ringing high partial.
    double impulseShare = 0.0, impulseDensity = 0.0;

    switch (texture)
    {
        case NoiseTexture::fine:     impulseShare = 0.25; impulseDensity = 0.004; break;
        case NoiseTexture::medium:   impulseShare = 0.45; impulseDensity = 0.008; break;
        case NoiseTexture::coarse:   impulseShare = 0.65; impulseDensity = 0.015; break;
        case NoiseTexture::metallic: impulseShare = 0.55; impulseDensity = 0.02;  break;
        case NoiseTexture::smooth:
        case NoiseTexture::numTextures:
        default:                     break;
    }

    double ring = 0.0, ringVelocity = 0.0;
    double peak = 1.0e-9;

    for (int i = 0; i < kTextureLength; ++i)
    {
        const double white = noiseUniform (seed, (juce::uint32) i) * 2.0 - 1.0;
        double v = white * (1.0 - impulseShare);

        if (noiseUniform (seed ^ 0xa5a5a5a5u, (juce::uint32) i) < impulseDensity)
            v += (white >= 0.0 ? 1.0 : -1.0) * impulseShare * 4.0;

        if (texture == NoiseTexture::metallic)
        {
            // A lightly damped oscillator excited by the texture itself.
            ringVelocity += (v - ring) * 0.35;
            ringVelocity *= 0.93;
            ring += ringVelocity;
            v = 0.6 * v + 0.4 * ring;
        }

        table[(size_t) i] = (float) v;
        peak = juce::jmax (peak, std::abs (v));
    }

    // Normalised so every texture drives the resonators at the same level;
    // the event's level is then the only thing that sets loudness.
    double power = 0.0;

    for (auto s : table)
        power += (double) s * s;

    const double rms = std::sqrt (power / kTextureLength);

    for (auto& s : table)
        s = (float) (s / juce::jmax (1.0e-9, rms) * 0.5);
}

void NoiseEngine::prepare (double sampleRate)
{
    for (int t = 0; t < (int) NoiseTexture::numTextures; ++t)
        if (textures[(size_t) t].empty())
            synthesiseTexture (textures[(size_t) t], (NoiseTexture) t, 0x7e57u + (juce::uint32) t * 7919u);

    for (auto& pool : pools)
        for (auto& g : pool)
            g.prepare (sampleRate);

    reset();
}

void NoiseEngine::reset() noexcept
{
    for (auto& pool : pools)
        for (auto& g : pool)
            g.stop();

    activeCount = 0;

    // Determinism is "the same event sequence from the same start": a reset is
    // a start, so the event indices the random choices hash on start again.
    triggerCounts.fill (0);
    stealCounts.fill (0);
    startCounter = 0;
}

void NoiseEngine::setDegraded (bool halvePools) noexcept
{
    degraded = halvePools;
}

int NoiseEngine::getPoolLimit (NoiseClass c) const noexcept
{
    const int full = kPoolSizes[(size_t) c];
    return degraded ? juce::jmax (1, full / 2) : full;
}

int NoiseEngine::getActiveCount (NoiseClass c) const noexcept
{
    int n = 0;

    for (int i = 0; i < getPoolLimit (c); ++i)
        if (pools[(size_t) c][(size_t) i].isActive())
            ++n;

    return n;
}

NoiseGenerator* NoiseEngine::getGenerator (NoiseClass c, int index) noexcept
{
    if (! juce::isPositiveAndBelow (index, getPoolLimit (c)))
        return nullptr;

    return &pools[(size_t) c][(size_t) index];
}

int NoiseEngine::trigger (const NoiseEvent& event) noexcept
{
    // Zero is silent and free: nothing is taken for an event nobody would hear.
    if (! (event.level > 0.0) || event.noiseClass == NoiseClass::numClasses)
        return -1;

    auto& pool = pools[(size_t) event.noiseClass];
    const int limit = getPoolLimit (event.noiseClass);

    int chosen = -1;
    juce::int64 oldest = std::numeric_limits<juce::int64>::max();

    for (int i = 0; i < limit; ++i)
    {
        if (! pool[(size_t) i].isActive())
        {
            chosen = i;
            break;
        }

        if (pool[(size_t) i].getStartOrder() < oldest)
        {
            oldest = pool[(size_t) i].getStartOrder();
            chosen = i;
        }
    }

    if (chosen < 0)
        return -1;

    if (pool[(size_t) chosen].isActive())
        ++stealCounts[(size_t) event.noiseClass];

    const auto eventIndex = (juce::uint32) (triggerCounts[(size_t) event.noiseClass]
                                            + (juce::int64) event.noiseClass * 1000003);

    pool[(size_t) chosen].start (event, textures[(size_t) event.texture],
                                 noiseHash (instanceSeed, eventIndex));
    pool[(size_t) chosen].setStartOrder (++startCounter);

    ++triggerCounts[(size_t) event.noiseClass];
    ++activeCount;   // so isIdle() is false before the next processSample recounts

    // The strip's record. Dropped rather than blocking if the reader is behind.
    const int write = eventWrite.load (std::memory_order_relaxed);
    const int next = (write + 1) % kEventRing;

    if (next != eventRead.load (std::memory_order_acquire))
    {
        events[(size_t) write] = { samplePosition, event.noiseClass, event.stringIndex, (float) event.level };
        eventWrite.store (next, std::memory_order_release);
    }

    return chosen;
}

int NoiseEngine::drainEvents (EventRecord* destination, int maxRecords) noexcept
{
    int count = 0;
    int read = eventRead.load (std::memory_order_relaxed);
    const int write = eventWrite.load (std::memory_order_acquire);

    while (read != write && count < maxRecords)
    {
        destination[count++] = events[(size_t) read];
        read = (read + 1) % kEventRing;
    }

    eventRead.store (read, std::memory_order_release);
    return count;
}

double NoiseEngine::processSample (double* excitationNoise, double* surfaceNoise, int numStrings) noexcept
{
    for (int s = 0; s < numStrings; ++s)
    {
        excitationNoise[s] = 0.0;
        surfaceNoise[s] = 0.0;
    }

    double total = 0.0;
    int stillActive = 0;

    for (int c = 0; c < (int) NoiseClass::numClasses; ++c)
    {
        auto& pool = pools[(size_t) c];

        // The whole pool, not the degraded limit: a generator started before a
        // degradation step still finishes.
        for (auto& g : pool)
        {
            if (! g.isActive())
                continue;

            const double v = g.process();
            const int s = juce::jlimit (0, juce::jmax (0, numStrings - 1), g.getEvent().stringIndex);

            if ((NoiseClass) c == NoiseClass::pickClick)
                excitationNoise[s] += v;
            else
                surfaceNoise[s] += v;

            total += v;

            if (g.isActive())
                ++stillActive;
        }
    }

    activeCount = stillActive;
    return total;
}

} // namespace luthier
