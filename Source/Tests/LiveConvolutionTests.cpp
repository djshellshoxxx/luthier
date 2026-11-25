#include "IrSwapTestSupport.h"
#include "../DSP/Common/LiveConvolution.h"
#include "../Support/ThreadProbe.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <initializer_list>
#include <vector>
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

namespace
{
    // A mono 48 kHz response with taps across several 128-sample partitions.
    void writeResponse (const juce::File& file, float sign, int length)
    {
        juce::AudioBuffer<float> ir (1, length);
        ir.clear();
        for (const auto& [index, value] : std::initializer_list<std::pair<int, float>> {
                 {0, 0.9f}, {37, -0.4f}, {300, 0.25f}, {1100, -0.15f}, {length - 1, 0.05f}})
            ir.setSample (0, index, sign * value);
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}.withSampleRate (48000.0)
                                                       .withNumChannels (1).withBitsPerSample (24));
        if (writer != nullptr) writer->writeFromAudioSampleBuffer (ir, 0, length);
    }

    // What the published response makes of a unit impulse, from a reset.
    std::vector<float> impulseResponseOf (LiveConvolution& convolution)
    {
        convolution.reset();
        juce::AudioBuffer<float> buffer (1, 64);
        std::vector<float> out;
        for (int block = 0; block < 48; ++block)
        {
            buffer.clear();
            if (block == 0) buffer.setSample (0, 0, 1.0f);
            auto audio = juce::dsp::AudioBlock<float> (buffer);
            convolution.process (audio);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 64);
        }
        return out;
    }

    // Long enough for any replacement's warm-up and crossfade to finish.
    void play (LiveConvolution& convolution, int blocks)
    {
        juce::AudioBuffer<float> buffer (1, 64);
        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < 64; ++i) buffer.setSample (0, i, 0.1f * (float) std::sin (0.05 * (block * 64 + i)));
            auto audio = juce::dsp::AudioBlock<float> (buffer);
            convolution.process (audio);
        }
    }

    struct Built
    {
        int convolutions = ThreadProbe::convolutionsCreated.load();
        int responses = ThreadProbe::convolutionResponsesBuilt.load();
        int newConvolutions() const { return ThreadProbe::convolutionsCreated.load() - convolutions; }
        int newResponses() const { return ThreadProbe::convolutionResponsesBuilt.load() - responses; }
    };
}

LUTHIER_TEST (LiveConvolution, aRetiredFileResponseIsRepublishedExactlyAsBuilt)
{
    juce::TemporaryFile first (".wav"), second (".wav");
    writeResponse (first.getFile(), 1.0f, 2400);
    writeResponse (second.getFile(), -1.0f, 1800);
    const auto stereo = juce::dsp::Convolution::Stereo::no;

    LiveConvolution reference;
    reference.prepare (48000.0, 64, 1, 128, {0, 0}, 0);
    CHECK (reference.loadFile (first.getFile(), stereo));
    const auto expected = impulseResponseOf (reference);
    CHECK (std::abs (expected[128]) > 0.01f);   // the response, after the partition latency

    LiveConvolution convolution;
    convolution.prepare (48000.0, 64, 1, 128, {0, 0}, 0);
    CHECK (convolution.loadFile (first.getFile(), stereo));
    play (convolution, 20);
    CHECK (convolution.loadFile (second.getFile(), stereo));
    play (convolution, 200);   // the first is retired once the crossfade ends

    {
        const Built before;
        CHECK (convolution.loadFile (first.getFile(), stereo));
        CHECK_MSG (before.newConvolutions() == 0 && before.newResponses() == 0,
                   "reloading a retired file built it again");
        CHECK (convolution.getLatency() == reference.getLatency());
        CHECK_MSG (impulseResponseOf (convolution) == expected, "the republished response differs from a new build");
    }

    {
        // A host re-activation at the same specification, then its reload.
        const Built before;
        convolution.prepare (48000.0, 64, 1, 128, {0, 0}, 0);
        CHECK (convolution.loadFile (first.getFile(), stereo));
        CHECK_MSG (before.newConvolutions() == 0 && before.newResponses() == 0,
                   "an identical re-prepare built " + juce::String (before.newConvolutions()) + " convolutions and "
                       + juce::String (before.newResponses()) + " responses");
        CHECK_MSG (impulseResponseOf (convolution) == expected, "the response after an identical re-prepare differs");
    }

    {
        // Another block size is another specification: everything is rebuilt.
        const Built before;
        convolution.prepare (48000.0, 32, 1, 128, {0, 0}, 0);
        CHECK (convolution.loadFile (first.getFile(), stereo));
        CHECK (before.newResponses() == 1);
        CHECK (before.newConvolutions() >= 1);
    }
}

LUTHIER_TEST (LiveConvolution, aFileRewrittenOnDiskIsReadAgain)
{
    juce::TemporaryFile temporary (".wav"), other (".wav");
    const auto file = temporary.getFile();
    const auto stereo = juce::dsp::Convolution::Stereo::no;
    writeResponse (file, 1.0f, 2400);

    LiveConvolution convolution;
    convolution.prepare (48000.0, 64, 1, 128, {0, 0}, 0);
    CHECK (convolution.loadFile (file, stereo));
    const auto before = impulseResponseOf (convolution);

    // The same name, size and length, other samples, and a later timestamp.
    const auto stamp = file.getLastModificationTime();
    writeResponse (file, -1.0f, 2400);
    file.setLastModificationTime (stamp + juce::RelativeTime::seconds (2.0));
    writeResponse (other.getFile(), -1.0f, 2400);

    LiveConvolution reference;
    reference.prepare (48000.0, 64, 1, 128, {0, 0}, 0);
    CHECK (reference.loadFile (other.getFile(), stereo));
    const auto expected = impulseResponseOf (reference);

    convolution.prepare (48000.0, 64, 1, 128, {0, 0}, 0);
    const Built built;
    CHECK (convolution.loadFile (file, stereo));
    CHECK (built.newResponses() == 1);
    const auto after = impulseResponseOf (convolution);
    CHECK_MSG (after == expected, "the rewritten file's response was not the one published");
    CHECK (after != before);
}

LUTHIER_TEST (LiveConvolution, reloadingThePlayingFileTakesBackTheQueuedReplacement)
{
    /*  A prepare loads the guitar's cabinet and resets, so the reader takes it,
        then loads the parameters' cabinet and the guitar's again. The reader's
        bank cannot be republished while it holds it, and the second load of
        the guitar's file built it again: the Free edition's startup guitar,
        whose compiled and factory cabinets are one file, did that on every
        re-activation (PrepareCost). */
    juce::TemporaryFile first (".wav"), second (".wav");
    writeResponse (first.getFile(), 1.0f, 2400);
    writeResponse (second.getFile(), -1.0f, 1800);
    const auto stereo = juce::dsp::Convolution::Stereo::no;

    LiveConvolution reference;
    reference.prepare (48000.0, 64, 1, 128, {0, 0}, 0);
    CHECK (reference.loadFile (first.getFile(), stereo));
    const auto expected = impulseResponseOf (reference);

    {
        // Handed over and not taken yet: the reader ends on it all the same.
        LiveConvolution convolution;
        convolution.prepare (48000.0, 64, 1, 128, {0, 0}, 0);
        CHECK (convolution.loadFile (first.getFile(), stereo));
        const Built before;
        CHECK (convolution.loadFile (first.getFile(), stereo));
        CHECK_MSG (before.newResponses() == 0, "reloading the file just published built it again");
        CHECK (impulseResponseOf (convolution) == expected);
    }

    LiveConvolution convolution;
    convolution.prepare (48000.0, 64, 1, 128, {0, 0}, 0);
    CHECK (convolution.loadFile (first.getFile(), stereo));
    convolution.reset();   // the reader takes it, as a prepare's reset does
    CHECK (convolution.loadFile (second.getFile(), stereo));

    {
        const Built before;
        CHECK (convolution.loadFile (first.getFile(), stereo));
        CHECK_MSG (before.newConvolutions() == 0 && before.newResponses() == 0,
                   "reloading the file the reader plays built it again");
        CHECK_MSG (impulseResponseOf (convolution) == expected, "the replacement queued after it was not taken back");
    }

    // Once the reader has taken the replacement, the file is loaded as any
    // other: here its bank still sounds through the crossfade, so it is built.
    CHECK (convolution.loadFile (second.getFile(), stereo));
    play (convolution, 1);
    CHECK (convolution.loadFile (first.getFile(), stereo));
    CHECK_MSG (impulseResponseOf (convolution) == expected, "a load after the reader took the replacement was lost");
}
