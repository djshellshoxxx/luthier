/*  F-C1 (real-time-safety review): a legacy rack pedal replaced while the
    audio thread is running must never be freed under it.

    EffectsChain::setSlotType used to free the outgoing pedal straight after
    the swap. Its swapLock only covers processStereo / applySlotState /
    resetFromAudioThread; the per-block latency query (updateLatency ->
    LuthierEngine::getLatencySamples), the tempo and expression fan-out and the
    quality path's oversampling change walk the slots without it, so a preset
    load on the message thread could free a pedal between the audio thread
    loading its pointer and calling through it.

    A use-after-free does not fail reliably in a normal build. These tests are
    the stress half of the proof and are meant to be run under AddressSanitizer
    (-DLUTHIER_SANITIZE=address,undefined), where the first touch of a freed
    pedal aborts with a heap-use-after-free report. Extra "latency probe"
    threads repeat the exact query the audio callback makes every block (and
    the processor's timer makes 30 times a second); they widen the window so a
    probe preempted while holding a slot's pointer is likely to find it swapped
    and freed when it resumes. The deterministic half (the retire rule itself)
    is in EffectsChainRetireTests.cpp.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Edition.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr int kTypes = (int) PedalType::NumTypes - 1;   // excluding None

    struct RackTypes
    {
        int type[2][EffectsChain::kNumSlots] {};
    };

    /** Two racks that differ in every slot, from a fixed seed. */
    void makeRacks (juce::Random& rng, RackTypes& a, RackTypes& b)
    {
        for (int chain = 0; chain < 2; ++chain)
            for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
            {
                a.type[chain][slot] = 1 + rng.nextInt (kTypes);
                b.type[chain][slot] = a.type[chain][slot] % kTypes + 1;
            }
    }

    void writeRack (LuthierAudioProcessor& processor, const RackTypes& rack)
    {
        for (int chain = 0; chain < 2; ++chain)
            for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
                if (auto* p = processor.getState().getParameter (ParamIDs::slotType (chain == 1, slot)))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) rack.type[chain][slot]));
    }

    /** What the probes and the audio thread found, read on the test thread
        (CHECK is not thread-safe). */
    struct Shared
    {
        std::atomic<bool> stop { false };
        std::atomic<long> blocks { 0 };
        std::atomic<long> probes { 0 };
        std::atomic<bool> finite { true };
        std::atomic<long> sink { 0 };
    };

    void runLatencyProbe (LuthierEngine& engine, Shared& shared)
    {
        long local = 0, n = 0;

        while (! shared.stop.load (std::memory_order_acquire))
        {
            local += engine.getLatencySamples();
            local += engine.getLatencySamples (AuxBus::ampPreCab);
            ++n;
        }

        shared.sink.fetch_add (local);
        shared.probes.fetch_add (n);
    }

    int expectedType (const RackTypes& rack, int chain, int slot)
    {
        return edition::effectivePedalIndex (rack.type[chain][slot]);
    }
}

//==============================================================================
/*  The real object graph and the real message-thread path: a running
    LuthierAudioProcessor, and preset loads through PresetManager::fromVar ->
    onPedalTypesLoaded -> ParameterBridge::adoptPedalTypesFromParameters ->
    EffectsChain::setSlotType, which takes no engine lock. */
LUTHIER_TEST (EffectsChainRetire, presetLoadsWhileAudioRunsNeverTouchAFreedPedal)
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 64;
    constexpr int kProbeThreads = 3;
    constexpr double kSeconds = 3.0;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    juce::Random rng (0x0FC1A11F);
    RackTypes rackA, rackB;
    makeRacks (rng, rackA, rackB);

    writeRack (processor, rackB);
    const auto presetB = processor.getPresetManager().toVar ("Retire B");
    writeRack (processor, rackA);
    const auto presetA = processor.getPresetManager().toVar ("Retire A");
    processor.getParameterBridge().applyAllNow();

    Shared shared;

    std::thread audio ([&]
    {
        const int channels = juce::jmax (1, juce::jmax (processor.getTotalNumInputChannels(),
                                                        processor.getTotalNumOutputChannels()));
        juce::AudioBuffer<float> buffer (channels, kBlock);
        juce::MidiBuffer midi;
        bool first = true;

        while (! shared.stop.load (std::memory_order_acquire))
        {
            buffer.clear();
            midi.clear();

            if (first)
            {
                for (int note : { 40, 47, 52 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
                first = false;
            }

            processor.processBlock (buffer, midi);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < kBlock; ++i)
                    if (! std::isfinite (buffer.getSample (ch, i)))
                        shared.finite.store (false);

            shared.blocks.fetch_add (1, std::memory_order_release);
        }
    });

    std::vector<std::thread> probes;

    for (int i = 0; i < kProbeThreads; ++i)
        probes.emplace_back ([&] { runLatencyProbe (processor.getEngine(), shared); });

    while (shared.blocks.load (std::memory_order_acquire) < 8)
        std::this_thread::yield();

    int loads = 0, refused = 0;
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + kSeconds * 1000.0;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (! processor.getPresetManager().fromVar ((loads % 2) == 0 ? presetB : presetA))
            ++refused;

        ++loads;
    }

    shared.stop.store (true, std::memory_order_release);
    audio.join();

    for (auto& t : probes)
        t.join();

    const auto& last = (loads % 2) == 1 ? rackB : rackA;   // the loop's final load

    CHECK (refused == 0);
    CHECK_MSG (loads >= 20, "only " + juce::String (loads) + " preset loads ran");
    CHECK_MSG (shared.blocks.load() >= 100, "only " + juce::String (shared.blocks.load()) + " blocks rendered");
    CHECK (shared.probes.load() > 0);
    CHECK (shared.finite.load());

    // The loads really replaced the pedals (the race was exercised, not skipped).
    for (int chain = 0; chain < 2; ++chain)
    {
        auto& fx = chain == 1 ? processor.getEngine().getPostEffects() : processor.getEngine().getPreEffects();

        for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
            CHECK_MSG ((int) fx.getSlotType (slot) == expectedType (last, chain, slot),
                       "chain " + juce::String (chain) + " slot " + juce::String (slot) + " did not follow the last load");
    }

    juce::Logger::writeToLog ("    EffectsChainRetire: " + juce::String (loads) + " preset loads, "
                              + juce::String (shared.blocks.load()) + " blocks, "
                              + juce::String (shared.probes.load()) + " latency probes");
}

/*  The same reader set against a bare LuthierEngine, with the message thread
    calling EffectsChain::setSlotType directly: thousands of swaps a second
    instead of a preset load's dozens, so the window is hit far more often.
    The "audio" thread runs the engine's block and the processor's per-block
    tempo and latency calls in the order processSlice makes them. */
LUTHIER_TEST (EffectsChainRetire, directSlotSwapsWhileAudioRunsNeverTouchAFreedPedal)
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 32;
    constexpr int kProbeThreads = 2;
    constexpr double kSeconds = 2.0;

    LuthierEngine engine;
    engine.prepare (kSr, kBlock);

    juce::Random rng (0x0FC1D1EC);
    RackTypes rackA, rackB;
    makeRacks (rng, rackA, rackB);

    Shared shared;

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;
        long local = 0;
        double bpm = 100.0;

        while (! shared.stop.load (std::memory_order_acquire))
        {
            buffer.clear();
            engine.setTempoBpm (bpm);
            bpm = bpm > 180.0 ? 100.0 : bpm + 0.5;
            engine.processBlock (buffer, midi);
            local += engine.getLatencySamples();

            for (int i = 0; i < kBlock; ++i)
                if (! std::isfinite (buffer.getSample (0, i)))
                    shared.finite.store (false);

            shared.blocks.fetch_add (1, std::memory_order_release);
        }

        shared.sink.fetch_add (local);
    });

    std::vector<std::thread> probes;

    for (int i = 0; i < kProbeThreads; ++i)
        probes.emplace_back ([&] { runLatencyProbe (engine, shared); });

    while (shared.blocks.load (std::memory_order_acquire) < 8)
        std::this_thread::yield();

    long swaps = 0;
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + kSeconds * 1000.0;

    for (int round = 0; juce::Time::getMillisecondCounterHiRes() < deadline; ++round)
    {
        const auto& rack = (round % 2) == 0 ? rackB : rackA;

        for (int chain = 0; chain < 2; ++chain)
        {
            auto& fx = chain == 1 ? engine.getPostEffects() : engine.getPreEffects();

            for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
            {
                fx.setSlotType (slot, (PedalType) rack.type[chain][slot]);
                ++swaps;
            }
        }
    }

    shared.stop.store (true, std::memory_order_release);
    audio.join();

    for (auto& t : probes)
        t.join();

    CHECK_MSG (swaps >= 1000, "only " + juce::String (swaps) + " swaps ran");
    CHECK (shared.blocks.load() >= 100);
    CHECK (shared.probes.load() > 0);
    CHECK (shared.finite.load());

    juce::Logger::writeToLog ("    EffectsChainRetire: " + juce::String (swaps) + " direct swaps, "
                              + juce::String (shared.blocks.load()) + " blocks, "
                              + juce::String (shared.probes.load()) + " latency probes");
}
