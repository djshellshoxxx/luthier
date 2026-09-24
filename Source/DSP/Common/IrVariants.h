#pragma once

/*  cpu-quality-modes.md 2.3: truncated impulse-response variants.

    On every IR load the message thread builds one shorter response per
    quality level whose cap is shorter than the IR:

      - the IR is prepared exactly as juce::dsp::Convolution prepares the full
        one (channels, trim, resampling to the processing rate, normalisation),
        so a variant's head is sample-for-sample the full response's;
      - it is cut at the cap, the cut moved later until the energy after it is
        at most -40 dB of the total;
      - a 50 ms raised-cosine fade ends it; nothing is renormalised.

    Each variant is its own juce::dsp::Convolution with the same fixed
    partition latency as the full one, so switching never moves the latency.
    A variant not shorter than the IR is not built: that level shares the full
    instance. A build that fails (memory) leaves that level on the full IR.

    The audio thread only selects: it runs the variant for the effective level
    and, on a switch, runs the old and the new responses together for 20 ms
    under a linear crossfade. The one being switched to is reset first, so its
    tail builds from the switch on rather than replaying stale history.

    The owner holds its own lock around load() and process(), exactly as it
    does for the full convolution.
*/

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>

namespace luthier
{

class IrVariants
{
public:
    static constexpr int kNumVariants = 2;   // Medium, Low

    /** Message thread. `caps` are the Medium and Low lengths in seconds
        (0 = no variant for that level). */
    void prepare (double sampleRate, int maxBlockSize, int numChannels, int partitionSize,
                  std::array<double, kNumVariants> capsSeconds, double memoryBudgetMegabytes);

    /*  cpu-quality-modes 2.3 / CQ-31: what a variant costs, as juce::dsp::
        Convolution holds it (measured: about 60 bytes per sample per channel
        with a 128-sample partition), and the rules that keep an instance's
        variants within 12 MB - the body gets 8 MB and each cabinet mic 2 MB:
          - a variant that would not be at least 10 % shorter is not built;
          - Low reuses Medium's variant when its own cut is no shorter;
          - a variant that would exceed the budget is not built (11: that
            level then runs the next longer response). */
    static constexpr double kBytesPerSampleChannel = 64.0;
    static constexpr double kMinimumSaving = 0.10;
    double getHeldMegabytes() const noexcept { return heldMegabytes; }

    /** Message thread: drops the variants (the IR changed or went away). */
    void clear();

    /** Message thread: builds the variants from a file, as the full
        convolution loads it. Returns how many were built. */
    int buildFromFile (const juce::File& file, juce::dsp::Convolution::Stereo stereo,
                       juce::dsp::Convolution::Trim trim, juce::dsp::Convolution::Normalise normalise);

    /** Message thread: the same from raw samples. */
    int buildFromBuffer (const juce::AudioBuffer<float>& raw, double irSampleRate,
                         juce::dsp::Convolution::Stereo stereo, juce::dsp::Convolution::Trim trim,
                         juce::dsp::Convolution::Normalise normalise);

    //==========================================================================
    /** Audio thread: the quality level to run (0 High, 1 Medium, 2 Low). A hard
        switch changes at once; otherwise the next process() crossfades. */
    void setLevel (int level, bool hard) noexcept;

    /** Audio thread: convolves `block` in place with the full convolution or a
        variant, crossfading on a switch. `full` must be the owner's full
        convolution, prepared with the same spec. */
    void process (juce::dsp::Convolution& full, juce::dsp::AudioBlock<float>& block) noexcept;

    /** Audio thread: clears the variants' histories. */
    void reset() noexcept;

    //==========================================================================
    /** Which response is running: 0 full, 1 Medium's, 2 Low's. */
    int getRunningIndex() const noexcept { return running; }
    bool isCrossfading() const noexcept { return fadeLeft > 0; }
    /** Whether `level` runs a shorter response than the full one. */
    bool hasVariant (int level) const noexcept { return resolve (level) != 0; }

    /** Seconds of the response each level runs (the full length for High). */
    double getSecondsForLevel (int level) const noexcept;
    double getFullSeconds() const noexcept { return fullSeconds; }

    /** CQ-31: the tests measure memory with and without the variants. */
    static void setEnabledForTesting (bool on) noexcept { enabled.store (on); }

    /** The tail rule as a pure function, for the tests (CQ-16): returns the
        cut index for `ir`, or ir's length if no variant is needed. */
    static int findCut (const juce::AudioBuffer<float>& ir, int capSamples, double tailEnergyDb);

    /** Truncates and fades `ir` at `cut` (in place). */
    static void truncateWithFade (juce::AudioBuffer<float>& ir, int cut, int fadeSamples);

    /** juce::dsp::Convolution's own preparation of a response (fixNumChannels,
        trim, resample, normalise), reproduced so a variant's head matches. */
    static juce::AudioBuffer<float> prepareLikeJuce (const juce::AudioBuffer<float>& raw, double irSampleRate,
                                                     double processRate, juce::dsp::Convolution::Stereo stereo,
                                                     juce::dsp::Convolution::Trim trim,
                                                     juce::dsp::Convolution::Normalise normalise);

private:
    /** Low without its own variant runs Medium's; either without runs the full IR. */
    int resolve (int level) const noexcept
    {
        if (level >= 2 && valid[1]) return 2;
        if (level >= 1 && valid[0]) return 1;
        return 0;
    }
    juce::dsp::Convolution& convFor (juce::dsp::Convolution& full, int index) noexcept
    {
        return index == 0 ? full : *variants[(size_t) index - 1];
    }

    int build (const juce::AudioBuffer<float>& prepared);

    static inline std::atomic<bool> enabled { true };

    double sr = 44100.0;
    int maxBlock = 512, channels = 1, partition = 128;
    std::array<double, kNumVariants> caps {};
    double budgetMegabytes = 0.0, heldMegabytes = 0.0;

    std::array<std::unique_ptr<juce::dsp::Convolution>, kNumVariants> variants;
    std::array<bool, kNumVariants> valid {};
    std::array<double, kNumVariants> seconds {};
    double fullSeconds = 0.0;

    int requested = 0;                      ///< the level last asked for
    int running = 0, previous = 0, wanted = 0;
    bool hardResetPending = false;
    int fadeLeft = 0, fadeTotal = 1;
    juce::AudioBuffer<float> scratch;
};

} // namespace luthier
