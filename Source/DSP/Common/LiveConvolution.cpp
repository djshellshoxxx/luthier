#include "LiveConvolution.h"

namespace luthier
{
void LiveConvolution::prepare (double rate, int block, int numChannels, int partitionSize,
                               std::array<double, 2> capSeconds, double memoryBudget)
{
    const std::lock_guard<std::mutex> guard (loading);
    sr = rate; maxBlock = juce::jmax (1, block); channels = numChannels;
    partition = partitionSize; caps = capSeconds; budget = memoryBudget;
    pending.store (-1); current = incoming = -1; warmLeft = fadePosition = 0;
    fadeLength = juce::jmax (1, (int) std::round (0.020 * sr));
    for (size_t i = 0; i < banks.size(); ++i) { banks[i].reset(); owned[i].store (false); }
    scratch.setSize (channels, maxBlock, false, true, true);
    // Ask the real engine: fixed latency also depends on the prepared block size.
    juce::dsp::Convolution probe (juce::dsp::Convolution::Latency {partition});
    probe.prepare ({sr, (juce::uint32) ConvolutionInstaller::preparedBlockSize (maxBlock), (juce::uint32) channels});
    latency.store ((int) probe.getLatency());
    for (size_t i = 0; i < 3; ++i) { publishedSeconds[i].store (0); publishedShorter[i].store (false); }
    publishedMegabytes.store (0);
}

std::unique_ptr<LiveConvolution::Bank> LiveConvolution::makeBank()
{
    auto bank = std::make_unique<Bank> (partition);
    bank->convolution.prepare ({sr, (juce::uint32) ConvolutionInstaller::preparedBlockSize (maxBlock), (juce::uint32) channels});
    bank->variants.prepare (sr, maxBlock, channels, partition, caps, budget);
    return bank;
}

bool LiveConvolution::loadFile (const juce::File& file, juce::dsp::Convolution::Stereo stereo)
{
    const std::lock_guard<std::mutex> guard (loading);
    if (! file.existsAsFile()) return false;
    auto bank = makeBank();
    bank->convolution.loadImpulseResponse (file, stereo, juce::dsp::Convolution::Trim::yes, 0,
                                          juce::dsp::Convolution::Normalise::yes);
    if (! ConvolutionInstaller::pumpUntilInstalled (bank->convolution, channels, maxBlock, 1,
            ConvolutionInstaller::kInstallTimeoutMs, (int) std::ceil (0.06 * sr))) return false;
    bank->convolution.reset();
    bank->variants.buildFromFile (file, stereo, juce::dsp::Convolution::Trim::yes,
                                 juce::dsp::Convolution::Normalise::yes);
    return publish (std::move (bank));
}

bool LiveConvolution::loadBuffer (const juce::AudioBuffer<float>& raw, double rate)
{
    const std::lock_guard<std::mutex> guard (loading);
    if (raw.getNumSamples() <= 0 || raw.getNumChannels() <= 0 || ! std::isfinite (rate) || rate <= 0) return false;
    auto bank = makeBank();
    auto input = raw;
    // A two-sample response distinguishes installation from JUCE's unit impulse.
    if (input.getNumSamples() == 1) { input.setSize (input.getNumChannels(), 2, true, true); }
    bank->convolution.loadImpulseResponse (std::move (input), rate, juce::dsp::Convolution::Stereo::no,
                                          juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    if (! ConvolutionInstaller::pumpUntilInstalled (bank->convolution, channels, maxBlock, 1,
            ConvolutionInstaller::kInstallTimeoutMs, (int) std::ceil (0.06 * sr))) return false;
    bank->convolution.reset();
    bank->variants.buildFromBuffer (raw, rate, juce::dsp::Convolution::Stereo::no,
                                   juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    return publish (std::move (bank));
}

bool LiveConvolution::publish (std::unique_ptr<Bank> bank)
{
    int slot = -1;
    for (int attempt = 0; attempt < 3 && slot < 0; ++attempt)
    {
        for (size_t i = 0; i < owned.size(); ++i)
        {
            bool free = false;
            if (owned[i].compare_exchange_strong (free, true, std::memory_order_acq_rel)) { slot = (int) i; break; }
        }
        // A pending bank is exclusive after exchange. If audio just consumed it,
        // it necessarily released another slot; rescan rather than drop a load.
        if (slot < 0) slot = pending.exchange (-1, std::memory_order_acq_rel);
    }
    if (slot < 0) return false;
    for (size_t i = 0; i < 3; ++i)
    {
        publishedSeconds[i].store (bank->variants.getSecondsForLevel ((int) i));
        publishedShorter[i].store (bank->variants.hasVariant ((int) i));
    }
    banks[(size_t) slot] = std::move (bank); // Any retired destruction is control-thread only.
    double held = 0.0;
    for (const auto& retained : banks) if (retained) held += retained->variants.getHeldMegabytes();
    publishedMegabytes.store (held);
    const int replaced = pending.exchange (slot, std::memory_order_acq_rel);
    if (replaced >= 0) owned[(size_t) replaced].store (false, std::memory_order_release);
    return true;
}

void LiveConvolution::adopt() noexcept
{
    if (incoming >= 0) return;
    const int next = pending.exchange (-1, std::memory_order_acq_rel);
    if (next < 0) return;
    auto& bank = *banks[(size_t) next];
    bank.variants.setLevel (level, true);
    if (current < 0) { current = next; return; }
    incoming = next;
    // Keep the old response audible while the new history and partition fill.
    warmLeft = getLatency() + (int) bank.convolution.getCurrentIRSize();
    fadePosition = 0;
}

bool LiveConvolution::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    adopt();
    if (current < 0) return false;
    const int n = (int) block.getNumSamples(), nch = (int) block.getNumChannels();
    jassert (n <= maxBlock && nch <= channels);
    if (n > maxBlock || nch > channels) return false;
    if (incoming >= 0)
        for (int ch = 0; ch < nch; ++ch) scratch.copyFrom (ch, 0, block.getChannelPointer ((size_t) ch), n);
    auto& active = *banks[(size_t) current];
    active.variants.process (active.convolution, block);
    if (incoming < 0) return true;
    auto next = juce::dsp::AudioBlock<float> (scratch).getSubsetChannelBlock (0, (size_t) nch).getSubBlock (0, (size_t) n);
    auto& fresh = *banks[(size_t) incoming];
    fresh.variants.process (fresh.convolution, next);
    for (int i = 0; i < n; ++i)
    {
        if (warmLeft > 0) { --warmLeft; continue; }
        const float mix = (float) juce::jmin (++fadePosition, fadeLength) / (float) fadeLength;
        for (int ch = 0; ch < nch; ++ch)
            block.setSample (ch, i, block.getSample (ch, i) * (1.0f - mix) + next.getSample (ch, i) * mix);
    }
    if (fadePosition >= fadeLength)
    {
        const int retired = current;
        current = incoming; incoming = -1;
        owned[(size_t) retired].store (false, std::memory_order_release);
    }
    return true;
}

void LiveConvolution::reset() noexcept
{
    const int next = pending.exchange (-1, std::memory_order_acq_rel);
    const int chosen = next >= 0 ? next : (incoming >= 0 ? incoming : current);
    for (int old : {current, incoming})
        if (old >= 0 && old != chosen) owned[(size_t) old].store (false, std::memory_order_release);
    current = chosen; incoming = -1; warmLeft = fadePosition = 0;
    if (current >= 0)
    {
        auto& bank = *banks[(size_t) current];
        bank.variants.setLevel (level, true);
        bank.convolution.reset(); bank.variants.reset();
    }
}

void LiveConvolution::setLevel (int requested, bool hard) noexcept
{
    level = requested;
    for (int slot : {current, incoming}) if (slot >= 0) banks[(size_t) slot]->variants.setLevel (level, hard);
}

LiveConvolution::VariantInfo LiveConvolution::getVariantInfo() const noexcept
{
    VariantInfo info;
    for (size_t i = 0; i < 3; ++i) { info.seconds[i] = publishedSeconds[i].load(); info.shorter[i] = publishedShorter[i].load(); }
    info.megabytes = publishedMegabytes.load();
    return info;
}
}
