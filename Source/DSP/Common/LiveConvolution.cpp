#include "LiveConvolution.h"
#include "../../Support/ThreadProbe.h"

namespace luthier
{
LiveConvolution::Source LiveConvolution::Source::of (const juce::File& file, bool stereo)
{
    return { file.getFullPathName(), file.getSize(), file.getLastModificationTime().toMilliseconds(), stereo };
}

bool LiveConvolution::Source::sameAs (const Source& other) const noexcept
{
    return path.isNotEmpty() && path == other.path && bytes == other.bytes
        && modified == other.modified && stereo == other.stereo;
}

LiveConvolution::Bank::Bank (int partitionSize) : convolution (juce::dsp::Convolution::Latency {partitionSize})
{
    ThreadProbe::noteConvolutionCreated();
}

void LiveConvolution::prepare (double rate, int block, int numChannels, int partitionSize,
                               std::array<double, 2> capSeconds, double memoryBudget)
{
    const std::lock_guard<std::mutex> guard (loading);
    const int blockSize = juce::jmax (1, block);
    // Unchanged, every bank still holds a valid response: all are kept as
    // retired ones, and the reload that follows republishes one instead of
    // building it again. Any change starts from nothing, as a kept bank would
    // hold the old rate's response or the old partition's latency.
    const bool unchanged = prepared && rate == sr && blockSize == maxBlock && numChannels == channels
                        && partitionSize == partition && capSeconds == caps && memoryBudget == budget;
    sr = rate; maxBlock = blockSize; channels = numChannels;
    partition = partitionSize; caps = capSeconds; budget = memoryBudget; prepared = true;
    pending.store (-1); current = incoming = -1; warmLeft = fadePosition = 0;
    settled = handed = -1;
    fadeLength = juce::jmax (1, (int) std::round (0.020 * sr));
    for (size_t i = 0; i < banks.size(); ++i) { if (! unchanged) banks[i].reset(); owned[i].store (false); }
    scratch.setSize (channels, maxBlock, false, true, true);
    // Ask the real engine: fixed latency also depends on the prepared block size.
    if (! (spec() == probedSpec) || partition != probedPartition)
    {
        juce::dsp::Convolution probe (juce::dsp::Convolution::Latency {partition});
        ThreadProbe::noteConvolutionCreated();
        probe.prepare (spec());
        latency.store ((int) probe.getLatency());
        probedSpec = spec(); probedPartition = partition;
    }
    for (size_t i = 0; i < 3; ++i) { publishedSeconds[i].store (0); publishedShorter[i].store (false); }
    publishedMegabytes.store (0);
}

juce::dsp::ProcessSpec LiveConvolution::spec() const noexcept
{
    return {sr, (juce::uint32) ConvolutionInstaller::preparedBlockSize (maxBlock), (juce::uint32) channels};
}

std::unique_ptr<LiveConvolution::Bank> LiveConvolution::makeBank()
{
    // build() installs every response with installExclusively, which prepares the convolution.
    auto bank = std::make_unique<Bank> (partition);
    bank->variants.prepare (sr, maxBlock, channels, partition, caps, budget);
    return bank;
}

template <typename Fill>
bool LiveConvolution::build (Source source, Fill&& fill)
{
    ThreadProbe::noteConvolutionResponseBuilt();
    const int slot = claimRetiredSlot();
    if (slot < 0)
    {
        // Audio holds all three banks (current, fading, pending): build aside
        // and let publish() replace the pending one.
        auto bank = makeBank();
        fill (*bank);
        bank->source = std::move (source);
        return publish (std::move (bank));
    }
    try
    {
        // Built in place: no thread to start, and none to join for the old response.
        auto& bank = banks[(size_t) slot];
        if (bank == nullptr) bank = makeBank();
        bank->source = {};
        fill (*bank);
        bank->source = std::move (source);
    }
    catch (...)
    {
        owned[(size_t) slot].store (false, std::memory_order_release);
        throw;
    }
    publishSlot (slot);
    return true;
}

bool LiveConvolution::loadFile (const juce::File& file, juce::dsp::Convolution::Stereo stereo)
{
    const std::lock_guard<std::mutex> guard (loading);
    if (! file.existsAsFile()) return false;
    auto source = Source::of (file, stereo == juce::dsp::Convolution::Stereo::yes);
    if (keepsPlaying (source) || republish (source)) return true;
    return build (std::move (source), [&] (Bank& bank)
    {
        ConvolutionInstaller::installExclusively (bank.convolution, spec(), [&] (juce::dsp::Convolution& convolution)
        {
            convolution.loadImpulseResponse (file, stereo, juce::dsp::Convolution::Trim::yes, 0,
                                             juce::dsp::Convolution::Normalise::yes);
        });
        bank.variants.buildFromFile (file, stereo, juce::dsp::Convolution::Trim::yes,
                                     juce::dsp::Convolution::Normalise::yes);
    });
}

bool LiveConvolution::loadBuffer (const juce::AudioBuffer<float>& raw, double rate)
{
    const std::lock_guard<std::mutex> guard (loading);
    if (raw.getNumSamples() <= 0 || raw.getNumChannels() <= 0 || ! std::isfinite (rate) || rate <= 0) return false;
    return build ({}, [&] (Bank& bank)
    {
        auto input = raw;
        // Two samples, not JUCE's one-sample unit impulse: the length sets a
        // replacement's warm-up, and it has always been at least two.
        if (input.getNumSamples() == 1) { input.setSize (input.getNumChannels(), 2, true, true); }
        ConvolutionInstaller::installExclusively (bank.convolution, spec(), [&] (juce::dsp::Convolution& convolution)
        {
            convolution.loadImpulseResponse (std::move (input), rate, juce::dsp::Convolution::Stereo::no,
                                             juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
        });
        bank.variants.buildFromBuffer (raw, rate, juce::dsp::Convolution::Stereo::no,
                                       juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    });
}

bool LiveConvolution::republish (const Source& source)
{
    for (size_t i = 0; i < banks.size(); ++i)
    {
        bool free = false;
        if (banks[i] == nullptr || owned[i].load (std::memory_order_acquire) || ! banks[i]->source.sameAs (source)
            || ! owned[i].compare_exchange_strong (free, true, std::memory_order_acq_rel)) continue;
        // Retired after its last block, it differs from a new build of the same
        // file only by that block's history.
        auto& bank = *banks[i];
        bank.convolution.reset();
        bank.variants.reset();
        publishSlot ((int) i);
        return true;
    }
    return false;
}

bool LiveConvolution::keepsPlaying (const Source& source)
{
    const auto holds = [this, &source] (int slot)
    {
        return slot >= 0 && banks[(size_t) slot] != nullptr && banks[(size_t) slot]->source.sameAs (source);
    };
    // The reader ends on the bank handed to it last, whether or not it has
    // taken it yet.
    if (holds (handed)) return true;
    if (! holds (settled)) return false;
    if (handed < 0) return true;
    // A replacement was queued after the reader's bank: take it back. If the
    // reader took it meanwhile, it ends on that one instead, and this file is
    // republished or built as any other.
    const int back = handOver (-1);
    if (back < 0) return false;
    owned[(size_t) back].store (false, std::memory_order_release);
    auto& kept = *banks[(size_t) settled];
    kept.publication = ++publications;
    announce (kept);
    return true;
}

int LiveConvolution::claimRetiredSlot() noexcept
{
    // An empty slot first, so every retired response stays available; then the
    // retired bank published longest ago.
    int chosen = -1;
    for (size_t i = 0; i < banks.size(); ++i)
    {
        if (owned[i].load (std::memory_order_acquire)) continue;
        if (banks[i] == nullptr) { chosen = (int) i; break; }
        if (chosen < 0 || banks[i]->publication < banks[(size_t) chosen]->publication) chosen = (int) i;
    }
    bool free = false;
    return chosen >= 0 && owned[(size_t) chosen].compare_exchange_strong (free, true, std::memory_order_acq_rel)
               ? chosen : -1;
}

void LiveConvolution::publishSlot (int slot)
{
    auto& bank = *banks[(size_t) slot];
    bank.publication = ++publications;
    announce (bank);
    double held = 0.0;
    for (const auto& retained : banks) if (retained) held += retained->variants.getHeldMegabytes();
    publishedMegabytes.store (held);
    const int replaced = handOver (slot);
    if (replaced >= 0) owned[(size_t) replaced].store (false, std::memory_order_release);
}

int LiveConvolution::handOver (int slot)
{
    // The reader takes a pending bank and never hands one back, so a bank
    // handed over and not returned by this exchange is the one it ends on.
    const int back = pending.exchange (slot, std::memory_order_acq_rel);
    jassert (back < 0 || back == handed);
    if (back < 0 && handed >= 0) settled = handed;
    handed = slot;
    return back;
}

void LiveConvolution::announce (const Bank& bank)
{
    for (size_t i = 0; i < 3; ++i)
    {
        publishedSeconds[i].store (bank.variants.getSecondsForLevel ((int) i));
        publishedShorter[i].store (bank.variants.hasVariant ((int) i));
    }
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
        if (slot < 0) slot = handOver (-1);
    }
    if (slot < 0) return false;
    banks[(size_t) slot] = std::move (bank); // Any retired destruction is control-thread only.
    publishSlot (slot);
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
