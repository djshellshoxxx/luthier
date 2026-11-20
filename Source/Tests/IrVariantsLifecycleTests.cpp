// Luthier™
// Copyright © 2026 Sheldon Davidson. All rights reserved.
// Proprietary source code. See LICENSE and COPYRIGHT-TRADEMARK.md.
// SPDX-License-Identifier: LicenseRef-Proprietary

#include "TestFramework.h"
#include "../DSP/Common/IrVariants.h"
#include "../DSP/Common/ConvolutionInstaller.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    juce::AudioBuffer<float> impulseResponse (int samples)
    {
        juce::AudioBuffer<float> raw (1, samples);
        raw.clear();
        raw.setSample (0, 0, 1.0f);
        return raw;
    }

    int impulseArrival (IrVariants& variants, juce::dsp::Convolution& full, int blockSize)
    {
        juce::AudioBuffer<float> samples (1, blockSize);
        for (int offset = 0; offset < 4096; offset += blockSize)
        {
            samples.clear();
            if (offset == 0) samples.setSample (0, 0, 1.0f);
            auto block = juce::dsp::AudioBlock<float> (samples);
            variants.process (full, block);
            for (int i = 0; i < blockSize; ++i)
                if (std::abs (samples.getSample (0, i)) > 0.01f)
                    return offset + i;
        }
        return -1;
    }
}

LUTHIER_TEST (IrVariantsLifecycle, reprepareKeepsVariantLatencyAlignedWithTheFullResponse)
{
    IrVariants variants;
    const auto raw = impulseResponse (4096);
    for (int partition : { 128, 512, 0, 256 })
    {
        constexpr int blockSize = 64;
        variants.prepare (48000.0, blockSize, 1, partition, { 0.020, 0.010 }, 1.0);
        CHECK (variants.buildFromBuffer (raw, 48000.0, juce::dsp::Convolution::Stereo::no,
                                        juce::dsp::Convolution::Trim::no,
                                        juce::dsp::Convolution::Normalise::yes) == 2);
        juce::dsp::Convolution full (juce::dsp::Convolution::Latency { partition });
        full.prepare ({ 48000.0, blockSize, 1 });
        for (int level : { 1, 2 })
        {
            variants.reset();
            variants.setLevel (level, true);
            CHECK (variants.hasVariant (level));
            CHECK_MSG (impulseArrival (variants, full, blockSize) == (int) full.getLatency(),
                       "quality response retained an earlier prepare's partition latency");
        }
    }
}

LUTHIER_TEST (IrVariantsLifecycle, shortOrOverBudgetResponsesFallBackAndCanBuildAgain)
{
    IrVariants variants;
    const auto longIr = impulseResponse (4096);
    const auto shortIr = impulseResponse (64);
    const auto build = [&] (const juce::AudioBuffer<float>& raw)
    {
        return variants.buildFromBuffer (raw, 48000.0, juce::dsp::Convolution::Stereo::no,
                                        juce::dsp::Convolution::Trim::no,
                                        juce::dsp::Convolution::Normalise::yes);
    };
    variants.prepare (48000.0, 64, 1, 128, { 0.020, 0.010 }, 1.0);
    CHECK (build (shortIr) == 0);
    CHECK (! variants.hasVariant (1) && ! variants.hasVariant (2));
    CHECK (build (longIr) == 2);
    CHECK (build (shortIr) == 0);
    variants.setLevel (2, true);
    CHECK (variants.getRunningIndex() == 0);
    CHECK_NEAR (variants.getSecondsForLevel (2), 64.0 / 48000.0, 1.0e-9);
    CHECK (build (longIr) == 2);
    CHECK (variants.getRunningIndex() == 2);

    variants.prepare (96000.0, 7, 1, 256, { 0.020, 0.010 }, 0.0);
    CHECK (build (longIr) == 0);
    CHECK (! variants.hasVariant (1) && ! variants.hasVariant (2));
    CHECK (variants.getHeldMegabytes() == 0.0);

    variants.prepare (48000.0, 1, 1, 128, { 0.020, 0.010 }, 1.0);
    CHECK (build (longIr) == 2);
    juce::dsp::Convolution full (juce::dsp::Convolution::Latency { 128 });
    full.prepare ({ 48000.0, (juce::uint32) ConvolutionInstaller::preparedBlockSize (1), 1 });
    variants.setLevel (2, true);
    CHECK (impulseArrival (variants, full, 1) == (int) full.getLatency());
}
