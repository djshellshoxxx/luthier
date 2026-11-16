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
    prepare/destruction require stopped audio; load calls are serialized. */
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
    struct Bank
    {
        explicit Bank (int partition) : convolution (juce::dsp::Convolution::Latency {partition}) {}
        juce::dsp::Convolution convolution;
        IrVariants variants;
    };
    std::unique_ptr<Bank> makeBank();
    bool publish (std::unique_ptr<Bank>);
    void adopt() noexcept;
    std::mutex loading;
    std::array<std::unique_ptr<Bank>, 3> banks;
    std::array<std::atomic<bool>, 3> owned {};
    std::atomic<int> pending {-1}, latency {0};
    int current = -1, incoming = -1, warmLeft = 0, fadePosition = 0, fadeLength = 1, level = 0;
    double sr = 44100.0, budget = 0;
    int maxBlock = 512, channels = 1, partition = 128;
    std::array<double, 2> caps {};
    juce::AudioBuffer<float> scratch;
    std::array<std::atomic<double>, 3> publishedSeconds {};
    std::array<std::atomic<bool>, 3> publishedShorter {};
    std::atomic<double> publishedMegabytes {0};
};
}
