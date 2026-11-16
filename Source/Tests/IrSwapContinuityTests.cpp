#include "TestFramework.h"
#include "../DSP/Amp/CabinetEngine.h"
#include "../DSP/Body/BodyEngine.h"
#include "../DSP/Common/LiveConvolution.h"
#include <atomic>
#include <thread>

using namespace luthier;
using namespace luthier::tests;

#include "IrSwapTestSupport.h"

LUTHIER_TEST (IrSwapContinuity, cabinetKeepsItsDelayAndCrossfadesReplacement)
{
    CabinetEngine cabinet;
    cabinet.prepare (48000.0, 64);
    cabinet.setPlacementBypassed (0, true);
    checkSwap (ctx, [&] (auto& b) { cabinet.processBlock (b); },
               [&] (const float* s, int n) { cabinet.loadImpulseResponse (0, s, n, 48000.0); },
               [&] { return cabinet.getLatencySamples(); });
}

LUTHIER_TEST (IrSwapContinuity, bodyKeepsItsDelayAndCrossfadesReplacement)
{
    BodyEngine body;
    body.prepare (48000.0, 64);
    checkSwap (ctx, [&] (auto& b) { juce::dsp::AudioBlock<float> block (b); body.processBlock (block); },
               [&] (const float* s, int n) { body.loadImpulseResponse (s, n, 48000.0); },
               [&] { return body.getLatencySamples(); });
}


LUTHIER_TEST (IrSwapContinuity, cabinetBypassPreservesPhysicalDelay)
{
    for (int blockSize : {1, 64, 512})
    {
        CabinetEngine cabinet;
        cabinet.prepare (48000.0, blockSize);
        std::array<float, 128> ir {}; ir[0] = 1.0f;
        cabinet.loadImpulseResponse (0, ir.data(), (int) ir.size(), 48000.0);
        const int latency = cabinet.getLatencySamples();
        cabinet.setEnabled (false);
        CHECK (cabinet.getLatencySamples() == latency);
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int samples = 0; samples < 4800; samples += blockSize)
        { buffer.clear(); cabinet.processBlock (buffer); }
        int onset = -1;
        for (int samples = 0; samples < latency + 2 * blockSize; samples += blockSize)
        {
            buffer.clear();
            if (samples == 0) { buffer.setSample (0, 0, 1.0f); buffer.setSample (1, 0, 0.5f); }
            cabinet.processBlock (buffer);
            for (int i = 0; i < blockSize; ++i)
                if (onset < 0 && std::abs (buffer.getSample (0, i)) > 0.9f)
                { onset = samples + i; CHECK_NEAR (buffer.getSample (1, i), 0.5f, 1.0e-6); }
        }
        CHECK (onset == latency);
        CHECK (! cabinet.hasMicTap (0));
        cabinet.setEnabled (true);
        CHECK (cabinet.getLatencySamples() == latency);
    }
}

LUTHIER_TEST (IrSwapContinuity, cabinetBypassEdgesDoNotIntroduceAnImpulse)
{
    for (int blockSize : {1, 64, 512}) for (bool startEnabled : {false, true})
    {
        CabinetEngine cabinet;
        cabinet.setEnabled (startEnabled);
        cabinet.prepare (48000.0, blockSize);
        cabinet.setPlacementBypassed (0, true);
        std::array<float, 128> ir {}; ir[0] = 1.0f; ir[1] = 0.25f;
        cabinet.loadImpulseResponse (0, ir.data(), (int) ir.size(), 48000.0);
        juce::AudioBuffer<float> buffer (2, blockSize);
        double previous = 0.0, steady = 0.0, transition = 0.0;
        bool switched = false;
        int change = 0;
        for (int sample = 0; sample < 144000; sample += blockSize)
        {
            if (! switched && sample >= 49137)
            { cabinet.setEnabled (! startEnabled); switched = true; change = sample; }
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < blockSize; ++i)
                buffer.setSample (ch, i, (float) (0.25 * std::sin (juce::MathConstants<double>::twoPi * 220.0 * (sample + i) / 48000.0)));
            cabinet.processBlock (buffer);
            for (int i = 0; i < blockSize; ++i)
            {
                const double value = buffer.getSample (0, i), delta = std::abs (value - previous);
                if (switched && sample + i < change + 4800) transition = std::max (transition, delta);
                else if (sample + i > 24000) steady = std::max (steady, delta);
                previous = value;
            }
        }
        CHECK (steady > 0.001);
        CHECK_MSG (transition <= 2.0 * steady,
            "cabinet bypass " + juce::String (startEnabled ? "off" : "on") + " block " + juce::String (blockSize)
            + " step " + juce::String (transition, 6) + " exceeds twice steady " + juce::String (steady, 6));
    }
}

LUTHIER_TEST (IrSwapContinuity, rawResponsesSurviveHostReprepare)
{
    BodyEngine body;
    CabinetEngine cabinet;
    std::array<float, 128> ir {}; ir[0] = 1.0f;
    body.loadImpulseResponse (ir.data(), (int) ir.size(), 48000.0);
    cabinet.loadImpulseResponse (0, ir.data(), (int) ir.size(), 48000.0);
    for (const auto settings : {std::pair<double, int> {48000.0, 64}, {48000.0, 1}, {96000.0, 512}, {44100.0, 37}})
    {
        const int blockSize = settings.second;
        body.prepare (settings.first, blockSize);
        cabinet.prepare (settings.first, blockSize);
        cabinet.setPlacementBypassed (0, true);
        for (bool isCabinet : {false, true})
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            const int latency = isCabinet ? cabinet.getLatencySamples() : body.getLatencySamples();
            int onset = -1;
            for (int sample = 0; sample < 1024; sample += blockSize)
            {
                buffer.clear();
                if (sample == 0) buffer.setSample (0, 0, 1.0f);
                if (isCabinet) cabinet.processBlock (buffer);
                else { juce::dsp::AudioBlock<float> block (buffer); body.processBlock (block); }
                for (int i = 0; i < blockSize; ++i)
                    if (onset < 0 && std::abs (buffer.getSample (0, i)) > 0.01f) onset = sample + i;
            }
            CHECK (latency >= 128);
            CHECK_MSG (onset == latency, juce::String (isCabinet ? "cabinet" : "body")
                + " reprepare block " + juce::String (blockSize) + " onset " + juce::String (onset)
                + " expected " + juce::String (latency));
        }
    }
}
