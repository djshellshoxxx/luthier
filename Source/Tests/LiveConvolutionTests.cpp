#include "IrSwapTestSupport.h"
#include "../DSP/Common/LiveConvolution.h"
using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (IrSwapContinuity, preparedBankKeepsItsDelayAndCrossfadesReplacement)
{
    std::atomic<int> failures {0};
    LiveConvolution convolution;
    convolution.prepare (48000.0, 64, 2, 128, {0, 0}, 0);
    checkSwap (ctx, [&] (auto& b) { juce::dsp::AudioBlock<float> block (b); convolution.process (block); },
               [&] (const float* s, int n) {
                   juce::AudioBuffer<float> raw (1, n); raw.copyFrom (0, 0, s, n);
                   if (! convolution.loadBuffer (raw, 48000.0)) failures.fetch_add (1);
               }, [&] { return convolution.getLatency(); });
    CHECK (failures.load() == 0);
}

#if LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
#endif
LUTHIER_TEST (LiveConvolution, repeatedPublicationQualityAndResetKeepOwnershipOffAudio)
{
    LiveConvolution convolution;
    convolution.prepare (48000.0, 64, 2, 128, {0.002, 0.001}, 2);
    juce::AudioBuffer<float> first (1, 256);
    first.clear(); first.setSample (0, 0, 1.0f);
    CHECK (convolution.loadBuffer (first, 48000.0));
    convolution.reset();
    std::atomic<bool> finished {false};
    std::atomic<int> failures {0};
    std::thread writer ([&]
    {
        for (int i = 0; i < 24; ++i)
        {
            juce::AudioBuffer<float> raw (1, 256 + i);
            raw.clear(); raw.setSample (0, 0, (i % 2) == 0 ? -1.0f : 1.0f);
            if (! convolution.loadBuffer (raw, 48000.0)) failures.fetch_add (1);
        }
        finished.store (true);
    });
    juce::AudioBuffer<float> buffer (2, 64);
    bool finite = true, processed = true, allocated = false;
    int calls = 0;
    do
    {
        const int n = calls % 3 == 0 ? 1 : (calls % 3 == 1 ? 7 : 64);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i) buffer.setSample (ch, i, 0.2f);
        auto block = juce::dsp::AudioBlock<float> (buffer).getSubBlock (0, (size_t) n);
#if LUTHIER_ALLOCATION_COUNTER
        const auto before = allocationsOnThisThread();
#endif
        if (calls % 61 == 0) convolution.setLevel ((calls / 61) % 3, calls % 122 == 0);
        if (calls % 233 == 0) convolution.reset();
        processed = convolution.process (block) && processed;
#if LUTHIER_ALLOCATION_COUNTER
        allocated = allocated || allocationsOnThisThread() != before;
#endif
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i)
            finite = finite && std::isfinite (buffer.getSample (ch, i));
        if (++calls % 100 == 0) juce::Thread::sleep (1);
    } while (! finished.load());
    writer.join();
    convolution.reset();
    for (int i = 0; i < 20; ++i)
    {
        for (int ch = 0; ch < 2; ++ch) for (int j = 0; j < 64; ++j) buffer.setSample (ch, j, 0.2f);
        auto block = juce::dsp::AudioBlock<float> (buffer);
        convolution.process (block);
    }
    CHECK (calls > 100);
    CHECK (failures.load() == 0);
    CHECK (finite);
    CHECK (processed);
    CHECK (! allocated);
    CHECK (buffer.getSample (0, 63) > 0.0f); // The final positive response actually arrived.
    CHECK (convolution.getLatency() == 128);
}
