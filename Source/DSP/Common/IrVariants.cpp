#include "IrVariants.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "ConvolutionInstaller.h"
#include "../../Support/QualityProfile.h"
#include <numeric>

namespace luthier
{

//==============================================================================
void IrVariants::prepare (double sampleRate, int maxBlockSize, int numChannels, int partitionSize,
                          std::array<double, kNumVariants> capsSeconds, double memoryBudgetMegabytes)
{
    budgetMegabytes = memoryBudgetMegabytes;
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);
    channels = juce::jlimit (1, 2, numChannels);
    partition = partitionSize;
    caps = capsSeconds;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = (juce::uint32) maxBlock;
    spec.numChannels = (juce::uint32) channels;

    for (auto& v : variants)
    {
        if (v == nullptr)
            v = std::make_unique<juce::dsp::Convolution> (juce::dsp::Convolution::Latency { partition });

        v->prepare (spec);
    }

    scratch.setSize (channels, maxBlock, false, true, true);
    clear();
}

void IrVariants::clear()
{
    valid.fill (false);
    seconds.fill (0.0);
    fullSeconds = 0.0;
    heldMegabytes = 0.0;
    running = previous = 0;
    fadeLeft = 0;
    wanted = resolve (requested);
}

void IrVariants::reset() noexcept
{
    for (size_t i = 0; i < variants.size(); ++i)
        if (valid[i] && variants[i] != nullptr)
            variants[i]->reset();

    fadeLeft = 0;
}

double IrVariants::getSecondsForLevel (int level) const noexcept
{
    const int r = resolve (level);
    return r == 0 ? fullSeconds : seconds[(size_t) r - 1];
}

//==============================================================================
juce::AudioBuffer<float> IrVariants::prepareLikeJuce (const juce::AudioBuffer<float>& raw, double irSampleRate,
                                                      double processRate, juce::dsp::Convolution::Stereo stereo,
                                                      juce::dsp::Convolution::Trim trim,
                                                      juce::dsp::Convolution::Normalise normalise)
{
    // fixNumChannels
    const int numChannels = juce::jmin (raw.getNumChannels(), stereo == juce::dsp::Convolution::Stereo::yes ? 2 : 1);
    juce::AudioBuffer<float> buf (juce::jmax (1, numChannels), raw.getNumSamples());

    for (int ch = 0; ch < numChannels; ++ch)
        buf.copyFrom (ch, 0, raw.getReadPointer (ch), raw.getNumSamples());

    if (buf.getNumSamples() == 0 || numChannels == 0)
    {
        buf.setSize (1, 1);
        buf.setSample (0, 0, 1.0f);
    }

    // trimImpulseResponse
    if (trim == juce::dsp::Convolution::Trim::yes)
    {
        const auto threshold = juce::Decibels::decibelsToGain (-80.0f);
        const int n = buf.getNumSamples();
        int begin = n, end = n;

        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer (ch);
            int first = 0;
            while (first < n && std::abs (d[first]) < threshold) ++first;
            int last = 0;
            while (last < n && std::abs (d[n - 1 - last]) < threshold) ++last;
            begin = juce::jmin (begin, first);
            end = juce::jmin (end, last);
        }

        if (begin == n)
        {
            juce::AudioBuffer<float> silent (buf.getNumChannels(), 1);
            silent.clear();
            buf = silent;
        }
        else
        {
            const int newLength = juce::jmax (1, n - (begin + end));
            juce::AudioBuffer<float> trimmed (buf.getNumChannels(), newLength);

            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                trimmed.copyFrom (ch, 0, buf.getReadPointer (ch, begin), newLength);

            buf = trimmed;
        }
    }

    // resampleImpulseResponse
    if (! juce::approximatelyEqual (irSampleRate, processRate))
    {
        const auto factorReading = irSampleRate / processRate;
        juce::AudioBuffer<float> original = buf;
        juce::MemoryAudioSource memorySource (original, false);
        juce::ResamplingAudioSource resamplingSource (&memorySource, false, buf.getNumChannels());

        const auto finalSize = juce::roundToInt (juce::jmax (1.0, buf.getNumSamples() / factorReading));
        resamplingSource.setResamplingRatio (factorReading);
        resamplingSource.prepareToPlay (finalSize, irSampleRate);

        juce::AudioBuffer<float> result (buf.getNumChannels(), finalSize);
        resamplingSource.getNextAudioBlock ({ &result, 0, result.getNumSamples() });
        buf = result;
    }

    // normaliseImpulseResponse, or the gain JUCE applies without it
    if (normalise == juce::dsp::Convolution::Normalise::yes)
    {
        const int n = buf.getNumSamples();
        auto* const* ptrs = buf.getArrayOfWritePointers();

        const auto maxSumSq = std::accumulate (ptrs, ptrs + buf.getNumChannels(), 0.0f, [n] (auto max, auto* channel)
        {
            return juce::jmax (max, std::accumulate (channel, channel + n, 0.0f, [] (auto sum, auto samp)
            {
                return sum + (samp * samp);
            }));
        });

        const float factor = maxSumSq < 1e-8f ? 1.0f : 0.125f / std::sqrt (maxSumSq);

        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            juce::FloatVectorOperations::multiply (ptrs[ch], factor, n);
    }
    else
    {
        buf.applyGain ((float) (irSampleRate / processRate));
    }

    return buf;
}

int IrVariants::findCut (const juce::AudioBuffer<float>& ir, int capSamples, double tailEnergyDb)
{
    const int n = ir.getNumSamples();

    if (capSamples <= 0 || capSamples >= n)
        return n;

    // Energy after each index, summed over channels.
    std::vector<double> after ((size_t) n + 1, 0.0);

    for (int i = n - 1; i >= 0; --i)
    {
        double e = 0.0;

        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
        {
            const double v = (double) ir.getSample (ch, i);
            e += v * v;
        }

        after[(size_t) i] = after[(size_t) i + 1] + e;
    }

    const double total = after[0];

    if (total <= 0.0)
        return capSamples;

    const double limit = total * std::pow (10.0, tailEnergyDb / 10.0);
    int cut = capSamples;

    while (cut < n && after[(size_t) cut] > limit)
        ++cut;

    return cut;
}

void IrVariants::truncateWithFade (juce::AudioBuffer<float>& ir, int cut, int fadeSamples)
{
    cut = juce::jlimit (1, ir.getNumSamples(), cut);
    fadeSamples = juce::jlimit (0, cut, fadeSamples);

    juce::AudioBuffer<float> out (ir.getNumChannels(), cut);

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        out.copyFrom (ch, 0, ir.getReadPointer (ch), cut);

        for (int i = 0; i < fadeSamples; ++i)
        {
            // Raised cosine from 1 to 0 across the last fadeSamples samples.
            const double t = (double) (i + 1) / (double) fadeSamples;
            const double g = 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * t));
            const int idx = cut - fadeSamples + i;
            out.setSample (ch, idx, (float) (out.getSample (ch, idx) * g));
        }
    }

    ir = out;
}

//==============================================================================
int IrVariants::build (const juce::AudioBuffer<float>& prepared)
{
    clear();
    fullSeconds = (double) prepared.getNumSamples() / sr;

    int built = 0;
    const auto stereo = prepared.getNumChannels() > 1 ? juce::dsp::Convolution::Stereo::yes
                                                      : juce::dsp::Convolution::Stereo::no;

    for (size_t v = 0; v < variants.size(); ++v)
    {
        if (caps[v] <= 0.0 || variants[v] == nullptr || ! enabled.load())
            continue;

        const int cut = findCut (prepared, (int) std::round (caps[v] * sr), QualityProfile::kTailEnergyDb);
        const int length = prepared.getNumSamples();

        // Not worth a convolution of its own: this level shares the full one.
        if ((double) cut > (1.0 - kMinimumSaving) * (double) length)
            continue;

        // Low's cut no shorter than Medium's: it runs Medium's (resolve()).
        if (v == 1 && valid[0] && (double) cut >= seconds[0] * sr - 0.5)
            continue;

        const double cost = (double) cut * prepared.getNumChannels() * kBytesPerSampleChannel / (1024.0 * 1024.0);

        if (heldMegabytes + cost > budgetMegabytes)
            continue;   // over the memory budget: the next longer response runs

        auto ir = prepared;
        truncateWithFade (ir, cut, (int) std::round (QualityProfile::kTailFadeSeconds * sr));

        auto& conv = *variants[v];
        ConvolutionInstaller::installUnitImpulse (conv, sr, channels, maxBlock);

        // Already at the processing rate and normalised with the full IR's
        // factor, so JUCE applies no further gain (rate ratio 1).
        conv.loadImpulseResponse (std::move (ir), sr, stereo,
                                  juce::dsp::Convolution::Trim::no,
                                  juce::dsp::Convolution::Normalise::no);

        if (! ConvolutionInstaller::pumpUntilInstalled (conv, channels, maxBlock, 1, 4000, (int) (0.06 * sr)))
            continue;   // 11: that level uses the full IR

        conv.reset();
        valid[v] = true;
        seconds[v] = (double) cut / sr;
        heldMegabytes += cost;
        ++built;
    }

    running = previous = wanted = resolve (requested);
    fadeLeft = 0;
    return built;
}

int IrVariants::buildFromFile (const juce::File& file, juce::dsp::Convolution::Stereo stereo,
                               juce::dsp::Convolution::Trim trim, juce::dsp::Convolution::Normalise normalise)
{
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (manager.createReaderFor (file));

    if (reader == nullptr)
    {
        clear();
        return 0;
    }

    juce::AudioBuffer<float> raw (juce::jlimit (1, 2, (int) reader->numChannels), (int) reader->lengthInSamples);
    reader->read (raw.getArrayOfWritePointers(), raw.getNumChannels(), 0, raw.getNumSamples());

    return build (prepareLikeJuce (raw, reader->sampleRate, sr, stereo, trim, normalise));
}

int IrVariants::buildFromBuffer (const juce::AudioBuffer<float>& raw, double irSampleRate,
                                 juce::dsp::Convolution::Stereo stereo, juce::dsp::Convolution::Trim trim,
                                 juce::dsp::Convolution::Normalise normalise)
{
    return build (prepareLikeJuce (raw, irSampleRate, sr, stereo, trim, normalise));
}

//==============================================================================
void IrVariants::setLevel (int level, bool hard) noexcept
{
    requested = juce::jlimit (0, 2, level);
    wanted = resolve (requested);

    if (hard)
    {
        if (wanted != running)
            hardResetPending = true;

        running = previous = wanted;
        fadeLeft = 0;
    }
}

void IrVariants::process (juce::dsp::Convolution& full, juce::dsp::AudioBlock<float>& block) noexcept
{
    const int n = (int) block.getNumSamples();
    const int nch = juce::jmin ((int) block.getNumChannels(), scratch.getNumChannels());

    if (hardResetPending)
    {
        hardResetPending = false;
        convFor (full, running).reset();
    }

    if (fadeLeft == 0 && wanted != running)
    {
        previous = running;
        running = wanted;
        convFor (full, running).reset();
        fadeTotal = juce::jmax (1, (int) std::round (QualityProfile::kConvolutionFadeSeconds * sr));
        fadeLeft = fadeTotal;
    }

    if (fadeLeft == 0 || n > scratch.getNumSamples())
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        convFor (full, running).process (ctx);
        fadeLeft = 0;
        return;
    }

    // Both responses for the length of the fade.
    for (int ch = 0; ch < nch; ++ch)
        scratch.copyFrom (ch, 0, block.getChannelPointer ((size_t) ch), n);

    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        convFor (full, previous).process (ctx);
    }

    {
        juce::dsp::AudioBlock<float> s (scratch);
        auto sub = s.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) nch);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        convFor (full, running).process (ctx);
    }

    const int start = fadeTotal - fadeLeft;

    for (int ch = 0; ch < nch; ++ch)
    {
        auto* out = block.getChannelPointer ((size_t) ch);
        const auto* in = scratch.getReadPointer (ch);

        for (int i = 0; i < n; ++i)
        {
            const float t = juce::jmin (1.0f, (float) (start + i + 1) / (float) fadeTotal);
            out[i] = out[i] * (1.0f - t) + in[i] * t;
        }
    }

    fadeLeft = juce::jmax (0, fadeLeft - n);
}

} // namespace luthier
