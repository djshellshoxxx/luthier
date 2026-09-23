#pragma once

/*  The shared noise-generator pool (pick-noise.md 1).

    Every playing noise in the realism specs - pick click, chirp and scrape,
    finger squeak, fret buzz, slide clank - is the same three things:

        excitation (a texture table read, or a noise burst)
          -> resonator (one to three band-passes, set by the event)
          -> envelope (attack, hold, decay)

    and differs only in how the event that triggers it sets those. So one
    generator type serves all six classes, and each class gets a fixed pool of
    them, allocated in prepare() and never again.

    Ground rules this enforces rather than documents:

    - **Events, not layers.** A generator is triggered by something the player
      did and runs to completion. Nothing here drones.
    - **Zero is free.** A trigger at level zero never takes a generator, and
      an empty pool costs nothing per sample beyond one flag test.
    - **Steal the oldest.** A trigger into a full pool takes the generator
      that has been running longest: a missing recent transient is more
      audible than a truncated old one.
    - **Deterministic.** Every random choice comes from a counter-based hash of
      the instance seed and the event index, so a repeated performance
      repeats sample for sample (character-wear.md 0.1).
*/

#include "../Common/DspCommon.h"
#include <array>
#include <atomic>
#include <vector>

namespace luthier
{

//==============================================================================
enum class NoiseClass
{
    squeak = 0,
    pickClick,
    pickChirp,
    pickScrape,
    fretBuzz,
    clank,
    numClasses
};

const char* getNoiseClassName (NoiseClass c) noexcept;

/*  Per-material noise textures (pick-noise.md 1, string-squeak.md 4). Each is
    synthesised once, at prepare(), and read at a per-event random offset -
    which is what keeps a thousand clicks from being one click repeated. */
enum class NoiseTexture
{
    smooth = 0,     ///< plain white noise, band-shaped by the resonator
    fine,           ///< nickel, coated, halfwound, pick surfaces
    medium,         ///< phosphor bronze
    coarse,         ///< 80/20 bronze, stainless, matte picks
    metallic,       ///< fret contact
    numTextures
};

//==============================================================================
/** What a trigger asks a generator to do. */
struct NoiseEvent
{
    NoiseClass noiseClass = NoiseClass::pickClick;
    int stringIndex = 0;

    /** Peak amplitude, in the string output's units. <= 0 never triggers. */
    double level = 0.0;

    /** Resonator centre at the start and end of the event, Hz. Equal for no glide. */
    double startHz = 2000.0;
    double endHz = 2000.0;

    /** Resonator Q. */
    double q = 4.0;

    /*  Balance between the fundamental band and its 2nd and 3rd harmonics,
        0 (fundamental only) to 1 (the harmonics as loud as the fundamental). */
    double brightness = 0.3;

    /** A second, lower resonance at this ratio of the first, 0 for none (pick wear). */
    double subResonanceRatio = 0.0;
    double subResonanceLevel = 0.0;

    NoiseTexture texture = NoiseTexture::smooth;

    double attackMs = 0.5;
    /** How long the event holds at full level before decaying. The glide spans it. */
    double holdMs = 0.0;
    double decayMs = 10.0;

    /*  Fret buzz: the excitation is gated into one burst per period at this
        rate - a buzzing string hits the fret once a cycle. 0 is continuous. */
    double burstHz = 0.0;
};

//==============================================================================
/** One noise voice. Allocation-free after prepare(). */
class NoiseGenerator
{
public:
    void prepare (double sampleRate) noexcept;

    void start (const NoiseEvent& event, const std::vector<float>& textureTable,
                juce::uint32 randomSeed) noexcept;

    /** One sample. Returns 0 and goes idle when the envelope has finished. */
    double process() noexcept;

    /*  For fret buzz: a sustained contact updates its level (and so its
        envelope target) block by block rather than restarting. */
    void setSustainLevel (double newLevel) noexcept;

    /** Lets a held event (buzz) fall into its decay. */
    void release() noexcept;

    /** Silences it at once. For reset and panic, not for musical endings. */
    void stop() noexcept { active = false; }

    bool isActive() const noexcept { return active; }
    juce::int64 getStartOrder() const noexcept { return startOrder; }
    void setStartOrder (juce::int64 order) noexcept { startOrder = order; }
    const NoiseEvent& getEvent() const noexcept { return event; }

private:
    void updateResonators (double hz) noexcept;

    double sr = 48000.0;
    bool active = false;
    juce::int64 startOrder = 0;

    NoiseEvent event;
    const float* table = nullptr;
    int tableSize = 0;
    int readIndex = 0;

    Biquad fundamental, second, third, sub;
    double w2 = 0.0, w3 = 0.0;

    // Envelope.
    int sampleIndex = 0, attackSamples = 1, holdSamples = 0;
    double decayCoefficient = 0.99;
    double envelope = 0.0;
    bool releasing = false;
    bool heldOpen = false;
    double sustainLevel = 1.0;

    // Glide: the resonators are re-tuned every kRetune samples.
    static constexpr int kRetune = 16;
    int glideSamples = 1;

    // Burst gate for buzz.
    double burstPhase = 0.0, burstIncrement = 0.0;

    // White-noise fallback when the event has no texture.
    juce::uint32 rng = 1;
};

//==============================================================================
class NoiseEngine
{
public:
    /** performance-budget.md 1's pool sizes. */
    static constexpr std::array<int, (size_t) NoiseClass::numClasses> kPoolSizes { 16, 16, 16, 8, 16, 8 };
    static constexpr int kMaxPool = 16;
    static constexpr int kMaxStringsHere = 12;
    static constexpr int kTextureLength = 1 << 14;

    NoiseEngine();

    /** Allocates the textures and the pools. Message thread. */
    void prepare (double sampleRate);
    void reset() noexcept;

    /** The instance seed every random choice derives from (character-wear.md). */
    void setSeed (juce::uint32 seed) noexcept { instanceSeed = seed; }

    /*  performance-budget.md 9 step 4: halve every pool. A pool-size change,
        not a bypass - the quietest events go first because they are the ones
        stolen. */
    void setDegraded (bool halvePools) noexcept;

    //==========================================================================
    /*  Starts a generator for this event. Returns its index in its class's
        pool, or -1 if the event was silent (level <= 0) and so took nothing.
        Audio thread. */
    int trigger (const NoiseEvent& event) noexcept;

    /** A generator of a class, for a sustained event to update or release. */
    NoiseGenerator* getGenerator (NoiseClass c, int index) noexcept;

    //==========================================================================
    /*  Runs every active generator for one sample and writes each string's
        noise into the two outputs (pick-noise.md 1.2): the click goes into
        the string's excitation input, everything else is added to the string
        output before the body. Both arrays are overwritten. Returns the sum of
        all of it, for Aux 8. */
    double processSample (double* excitationNoise, double* surfaceNoise, int numStrings) noexcept;

    /** True if nothing is sounding, so the caller can skip the whole stage. */
    bool isIdle() const noexcept { return activeCount == 0; }

    //==========================================================================
    /*  The noise-event strip's feed (string-squeak.md 9.1): one record per
        trigger, pushed from the audio thread, drained on the message thread.
        Lock-free single-producer single-consumer. */
    struct EventRecord
    {
        juce::int64 samplePosition = 0;
        NoiseClass noiseClass = NoiseClass::pickClick;
        int stringIndex = 0;
        float level = 0.0f;
    };

    void setSamplePosition (juce::int64 position) noexcept { samplePosition = position; }
    int drainEvents (EventRecord* destination, int maxRecords) noexcept;

    //==========================================================================
    /*  midi-export.md 6: the same triggers, kept for the host block that made
        them, each at its sample, so the processor can send them as Luthier
        SysEx on the audio thread. Cleared by the engine at each block start;
        the offset is set by the engine as it walks the block. */
    struct BlockTrigger
    {
        NoiseClass noiseClass = NoiseClass::pickClick;
        int stringIndex = 0;
        int offset = 0;
        float level = 0.0f;
        float durationMs = 0.0f;
    };

    static constexpr int kMaxBlockTriggers = 32;

    void beginBlockTriggers() noexcept           { numBlockTriggers = 0; }
    void setTriggerOffset (int offset) noexcept  { triggerOffset = offset; }
    int getNumBlockTriggers() const noexcept     { return numBlockTriggers; }

    const BlockTrigger& getBlockTrigger (int index) const noexcept
    {
        return blockTriggers[(size_t) juce::jlimit (0, kMaxBlockTriggers - 1, index)];
    }

    //==========================================================================
    /** For tests: how many triggers each class has taken a generator for. */
    juce::int64 getTriggerCount (NoiseClass c) const noexcept { return triggerCounts[(size_t) c]; }
    int getStealCount (NoiseClass c) const noexcept { return stealCounts[(size_t) c]; }
    int getActiveCount (NoiseClass c) const noexcept;
    int getPoolLimit (NoiseClass c) const noexcept;

    const std::vector<float>& getTexture (NoiseTexture t) const noexcept { return textures[(size_t) t]; }

private:
    static void synthesiseTexture (std::vector<float>& table, NoiseTexture texture, juce::uint32 seed);

    std::array<std::array<NoiseGenerator, kMaxPool>, (size_t) NoiseClass::numClasses> pools;
    std::array<std::vector<float>, (size_t) NoiseTexture::numTextures> textures;

    std::array<juce::int64, (size_t) NoiseClass::numClasses> triggerCounts {};
    std::array<int, (size_t) NoiseClass::numClasses> stealCounts {};

    bool degraded = false;
    int activeCount = 0;
    juce::int64 startCounter = 0;
    juce::uint32 instanceSeed = 0x5eed1234u;
    juce::int64 samplePosition = 0;

    static constexpr int kEventRing = 256;
    std::array<EventRecord, kEventRing> events {};
    std::atomic<int> eventWrite { 0 }, eventRead { 0 };

    std::array<BlockTrigger, kMaxBlockTriggers> blockTriggers {};
    int numBlockTriggers = 0;
    int triggerOffset = 0;
};

/** A counter-based hash, for the deterministic per-event choices. */
juce::uint32 noiseHash (juce::uint32 seed, juce::uint32 index) noexcept;

/** Uniform in [0, 1) from noiseHash. */
double noiseUniform (juce::uint32 seed, juce::uint32 index) noexcept;

} // namespace luthier
