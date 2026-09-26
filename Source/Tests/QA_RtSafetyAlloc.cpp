/*  QA-RTSAFETY (Task Q1): allocation/lock coverage for the pedal rack that
    AudioThreadSafetyTests.cpp's whole-processor fixture does not reach.

    That fixture drives LuthierAudioProcessor with whatever pedals the
    current preset happens to have loaded in a handful of slots. This file
    drives EffectsChain directly through every PedalType, in every one of
    its 8 slots, in both the pre-amp and post-amp positions - the part of
    the audio-thread call graph most likely to hide a per-pedal-type
    allocation that a single preset's fixture would never exercise.

    Same traps as AudioThreadSafetyTests.cpp (see its header comment):
    CircuitTests.cpp replaces the global operator new and counts per
    thread (luthier::tests::allocationsOnThisThread); ThreadProbe counts a
    blocking lock taken on a thread marked as the audio thread. EffectsChain
    protects its slot array with a CriticalSection, but every audio-thread
    entry point (processStereo, applySlotState) only ever ScopedTryLock's
    it, so a correctly-behaving chain trips neither counter here - setSlotType
    itself (the one call that allocates) is made from this test's own
    thread, never marked as the audio thread, exactly as the message thread
    does it in the plugin. */

#include "TestFramework.h"

#include "../DSP/Effects/EffectsChain.h"
#include "../DSP/Effects/Pedal.h"
#include "../Support/ThreadProbe.h"

#include <array>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;
}

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /** Marks this thread as the audio thread for the scope (see AudioThreadSafetyTests.cpp). */
    struct AudioThreadScope
    {
        AudioThreadScope()  { ThreadProbe::markAsAudioThread (true); }
        ~AudioThreadScope() { ThreadProbe::markAsAudioThread (false); }
    };

    /** Fills every slot of `chain` with a pedal type, cycling through the
        whole PedalType catalogue starting at `typeOffset` so repeated calls
        with a different offset cover every type over a few rounds. Every
        slot is engaged (not bypassed) with a non-trivial mix and every
        parameter pushed to a non-default value - a pedal that only
        allocates the first time a parameter leaves its default would
        otherwise slip past. This is message-thread work (setSlotType
        allocates by contract; see EffectsChain.h), so it is called outside
        any AudioThreadScope. */
    void fillSlots (TestContext& ctx, EffectsChain& chain, int typeOffset)
    {
        constexpr int numRealTypes = (int) PedalType::NumTypes - 1;   // excludes None

        for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
        {
            const int type = 1 + ((typeOffset + slot) % numRealTypes);
            chain.setSlotType (slot, (PedalType) type);

            auto* pedal = chain.getPedal (slot);
            CHECK (pedal != nullptr);

            if (pedal == nullptr)
                continue;

            const int n = juce::jmin (pedal->getNumParameters(), Pedal::kMaxParams);
            std::array<float, (size_t) Pedal::kMaxParams> params {};

            for (int p = 0; p < n; ++p)
                params[(size_t) p] = 0.65f;

            chain.applySlotState (slot, false, 0.75, params.data(), n);
        }
    }
}

//==============================================================================
LUTHIER_TEST (QaRtSafety, everyPedalTypeInEverySlotProcessesWithoutAllocatingOrLocking)
{
    EffectsChain pre, post;
    pre.prepare (kSr, kBlock);
    post.prepare (kSr, kBlock);
    pre.setPosition (EffectsChain::Position::PreAmp);
    post.setPosition (EffectsChain::Position::PostAmp);

    constexpr int numRealTypes = (int) PedalType::NumTypes - 1;

    long allocations = 0;
    int locks = 0;
    bool finite = true;
    int firstAllocRound = -1, firstLockRound = -1;

    std::vector<double> l ((size_t) kBlock), r ((size_t) kBlock);

    // Enough rounds to cycle the whole catalogue through both 8-slot chains
    // at least twice over, at different starting offsets so no two rounds
    // pair the same types with the same slots.
    const int rounds = 2 * ((numRealTypes + EffectsChain::kNumSlots - 1) / EffectsChain::kNumSlots) + 1;

    for (int round = 0; round < rounds; ++round)
    {
        fillSlots (ctx, pre,  round * 3);
        fillSlots (ctx, post, round * 3 + numRealTypes / 2);

        for (int block = 0; block < 25; ++block)
        {
            for (int i = 0; i < kBlock; ++i)
            {
                const double t = (double) (round * 25 + block) * kBlock + i;
                l[(size_t) i] = r[(size_t) i] = 0.4 * std::sin (0.037 * t) + 0.1 * std::sin (0.211 * t);
            }

            const auto before = allocationsOnThisThread();
            const int locksBefore = ThreadProbe::audioThreadLocks.load();

            {
                AudioThreadScope scope;
                pre.processStereo (l.data(), r.data(), kBlock);
                post.processStereo (l.data(), r.data(), kBlock);
            }

            const auto a = allocationsOnThisThread() - before;
            const int lk = ThreadProbe::audioThreadLocks.load() - locksBefore;

            if (a > 0 && firstAllocRound < 0) firstAllocRound = round;
            if (lk > 0 && firstLockRound < 0) firstLockRound = round;

            allocations += a;
            locks += lk;

            for (int i = 0; i < kBlock; ++i)
                finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
        }
    }

    CHECK (finite);
    CHECK_MSG (allocations == 0, juce::String (allocations)
               + " allocations processing pedals, first in round " + juce::String (firstAllocRound));
    CHECK_MSG (locks == 0, juce::String (locks)
               + " blocking locks processing pedals, first in round " + juce::String (firstLockRound));
}

//==============================================================================
/*  A slot type changing while the audio thread is mid-block (the message
    thread's setSlotType racing processStereo) must never be seen as a
    blocking wait or an allocation by the audio side: EffectsChain's
    contract is that processStereo takes a ScopedTryLock and simply skips
    that block for the slot being swapped (see EffectsChain.cpp). This
    drives both at once from two threads, the same way a plugin's message
    thread and audio thread actually overlap. */
LUTHIER_TEST (QaRtSafety, slotTypeSwapWhileProcessingNeverAllocatesOrLocksOnTheAudioSide)
{
    EffectsChain chain;
    chain.prepare (kSr, kBlock);
    fillSlots (ctx, chain, 0);

    std::atomic<bool> stop { false };
    std::atomic<long> allocations { 0 };
    std::atomic<int> locks { 0 };
    std::atomic<bool> finite { true };

    std::thread audio ([&]
    {
        std::vector<double> l ((size_t) kBlock), r ((size_t) kBlock);

        for (int block = 0; block < 400 && ! stop.load (std::memory_order_relaxed); ++block)
        {
            for (int i = 0; i < kBlock; ++i)
                l[(size_t) i] = r[(size_t) i] = 0.3 * std::sin (0.05 * (block * kBlock + i));

            const auto before = allocationsOnThisThread();
            const int locksBefore = ThreadProbe::audioThreadLocks.load();

            {
                AudioThreadScope scope;
                chain.processStereo (l.data(), r.data(), kBlock);
            }

            allocations.fetch_add (allocationsOnThisThread() - before, std::memory_order_relaxed);
            locks.fetch_add (ThreadProbe::audioThreadLocks.load() - locksBefore, std::memory_order_relaxed);

            for (int i = 0; i < kBlock; ++i)
                if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i]))
                    finite.store (false, std::memory_order_relaxed);
        }
    });

    // The message thread: never marked as the audio thread, so its own
    // allocations (setSlotType's contract) are not counted by this test.
    for (int i = 0; i < 60; ++i)
    {
        chain.setSlotType (i % EffectsChain::kNumSlots, (PedalType) (1 + i % ((int) PedalType::NumTypes - 1)));
        juce::Thread::sleep (1);
    }

    stop.store (true, std::memory_order_relaxed);
    audio.join();

    CHECK (finite.load());
    CHECK_MSG (allocations.load() == 0, juce::String (allocations.load())
               + " allocations on the audio thread while a slot type was being swapped");
    CHECK_MSG (locks.load() == 0, juce::String (locks.load())
               + " blocking locks on the audio thread while a slot type was being swapped");
}
