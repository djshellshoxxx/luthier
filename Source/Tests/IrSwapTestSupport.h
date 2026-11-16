#pragma once
#include "TestFramework.h"
#include <atomic>
#include <thread>
using luthier::tests::TestContext;

namespace
{
    template <typename Process, typename Load, typename Latency>
    void checkSwap (TestContext& ctx, Process process, Load load, Latency latency)
    {
        constexpr int blockSize = 64;
        constexpr double rate = 48000.0;
        juce::AudioBuffer<float> buffer (2, blockSize);
        std::array<float, 128> response {};
        response[0] = 1.0f;
        response[1] = 0.25f;
        load (response.data(), (int) response.size());
        int sample = 0;
        const int initialLatency = latency();
        double beforeCorrelation = 0.0, afterCorrelation = 0.0;
        bool finalMeasurement = false;
        double previous = 0.0, steadyDelta = 0.0, transitionDelta = 0.0;
        auto render = [&] (bool measureTransition)
        {
            for (int i = 0; i < blockSize; ++i, ++sample)
                for (int ch = 0; ch < 2; ++ch)
                    buffer.setSample (ch, i, (float) (0.25 * std::sin (
                        juce::MathConstants<double>::twoPi * 220.0 * sample / rate)));
            process (buffer);
            for (int i = 0; i < blockSize; ++i)
            {
                const double value = buffer.getSample (0, i);
                if (sample > (int) rate / 2)
                {
                    auto& delta = measureTransition ? transitionDelta : steadyDelta;
                    delta = std::max (delta, std::abs (value - previous));
                }
                const double reference = std::sin (juce::MathConstants<double>::twoPi
                    * 220.0 * (sample - blockSize + i - initialLatency) / rate);
                if (! measureTransition && sample > (int) rate / 2) beforeCorrelation += value * reference;
                if (finalMeasurement) afterCorrelation += value * reference;
                previous = value;
            }
        };
        for (int i = 0; i < 750; ++i) render (false);
        const int expectedLatency = latency();
        CHECK (expectedLatency > 0);
        std::atomic<bool> loaded {false};
        for (auto& value : response) value = -value;
        std::thread loader ([&] { load (response.data(), (int) response.size()); loaded.store (true); });
        bool stableLatency = true;
        for (int i = 0; i < 1500; ++i)
        {
            render (true);
            stableLatency = stableLatency && latency() == expectedLatency;
            // Give the real JUCE loader time; callback DSP never sleeps.
            if (! loaded.load()) juce::Thread::sleep (1);
        }
        loader.join();
        finalMeasurement = true;
        for (int i = 0; i < 750; ++i) render (true);
        CHECK_MSG (stableLatency, "IR replacement changed the reported latency");
        CHECK (steadyDelta > 0.001);
        CHECK (beforeCorrelation > 0.0);
        CHECK (afterCorrelation < -0.2 * beforeCorrelation);
        CHECK_MSG (transitionDelta <= 2.0 * steadyDelta,
                   "IR replacement step " + juce::String (transitionDelta, 6)
                   + " exceeds twice steady delta " + juce::String (steadyDelta, 6));
    }
}

