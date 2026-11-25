#pragma once

#include "ConvolutionInstaller.h"
#include "IrVariants.h"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>

namespace luthier
{
/** Builds replacements on the control thread. The audio reader owns its current
    and fading banks; a three-slot handoff keeps destruction off that reader.
    prepare/destruction require stopped audio; load calls are serialized.

    A retired bank is kept, not destroyed: a later response is built into it,
    and a load of the file it still holds republishes it as it is. A host
    re-activating at an unchanged specification (clap-validator's param fuzzing
    activates 50 times) keeps every bank the same way, so its reloads build
    nothing. Constructing a bank starts JUCE's loader thread and destroying one
    waits for that thread to wake: most of a re-activation's ~330 ms.

    A load of the file the reader already ends on builds nothing either: the
    replacement queued after it is taken back. Its bank cannot be republished
    while the reader holds it, so a prepare that loads A, resets (the reader
    takes A), then loads B and A again built A a second time. */
class LiveConvolution
{
public:
    struct VariantInfo
    {
        std::array<double, 3> seconds {};
        std::array<bool, 3> shorter {};
        double megabytes = 0;
        double getHeldMegabytes() const noexcept { return megabytes; }
        double getSecondsForLevel (int level) const noexcept { return seconds[(size_t) juce::jlimit (0, 2, level)]; }
        bool hasVariant (int level) const noexcept { return shorter[(size_t) juce::jlimit (0, 2, level)]; }
    };

    void prepare (double rate, int block, int channels, int partition,
                  std::array<double, 2> caps, double budget);
    bool loadFile (const juce::File&, juce::dsp::Convolution::Stereo);
    bool loadBuffer (const juce::AudioBuffer<float>&, double rate);
    // Returns false only before any response has been published. A replacement
    // never removes the old response from the signal path.
    bool process (juce::dsp::AudioBlock<float>&) noexcept;
    void reset() noexcept;
    void setLevel (int level, bool hard) noexcept;
    int getLatency() const noexcept { return latency.load (std::memory_order_acquire); }
    VariantInfo getVariantInfo() const noexcept;

private:
    /** The file a bank's response was read from, as it was on disk then. A
        buffer's response has no path and is never republished. */
    struct Source
    {
        juce::String path;
        juce::int64 bytes = 0, modified = 0;
        bool stereo = false;
        static Source of (const juce::File&, bool stereo);
        bool sameAs (const Source&) const noexcept;
    };

    struct Bank
    {
        explicit Bank (int partition);
        juce::dsp::Convolution convolution;
        IrVariants variants;
        Source source;
        juce::uint64 publication = 0;   ///< when last published: the oldest retired bank is rebuilt first
    };
    std::unique_ptr<Bank> makeBank();
    juce::dsp::ProcessSpec spec() const noexcept;
    template <typename Fill> bool build (Source, Fill&&);
    bool republish (const Source&);
    bool keepsPlaying (const Source&);
    int claimRetiredSlot() noexcept;
    void publishSlot (int slot);
    int handOver (int slot);
    void announce (const Bank&);
    bool publish (std::unique_ptr<Bank>);
    void adopt() noexcept;
    std::mutex loading;
    std::array<std::unique_ptr<Bank>, 3> banks;
    std::array<std::atomic<bool>, 3> owned {};
    std::atomic<int> pending {-1}, latency {0};
    // Control thread, under `loading`: the bank the reader was last seen to have
    // taken, and the bank handed to it since, which it may not have taken yet.
    int settled = -1, handed = -1;
    int current = -1, incoming = -1, warmLeft = 0, fadePosition = 0, fadeLength = 1, level = 0;
    double sr = 44100.0, budget = 0;
    int maxBlock = 512, channels = 1, partition = 128;
    std::array<double, 2> caps {};
    bool prepared = false;
    juce::uint64 publications = 0;
    juce::dsp::ProcessSpec probedSpec {};   ///< what the latency was measured at
    int probedPartition = -1;
    juce::AudioBuffer<float> scratch;
    std::array<std::atomic<double>, 3> publishedSeconds {};
    std::array<std::atomic<bool>, 3> publishedShorter {};
    std::atomic<double> publishedMegabytes {0};
};
}
