/*  QA-RTSAFETY (Task Q1): allocation/lock coverage for LuthierEngine paths
    AudioThreadSafetyTests.cpp's five-minute fixture never reaches.

    That fixture only ever sends note on/off, pitch-wheel and CC1, with
    whatever the default preset's E-Bow/Freeze/whammy settings happen to
    be (normally off). This drives E-Bow and Freeze engaged together with a
    Floyd Rose whammy dive - the three features most likely to leave extra
    filter/envelope state live in the feedback path - across:
      - single-sample blocks (the smallest a host can legally send),
      - a block size that divides neither the prepared size nor a nice
        round number (a host's leftover partial buffer),
      - the exact prepared size,
      - a live sample-rate change (prepare() called again mid-session, the
        way a host switching 48k -> 96k would drive it) followed by more of
        all of the above.

    Deliberately NOT exercised here: a host block *larger* than the size
    passed to prepare(). See docs/review/QA_RTSAFETY.md's top finding -
    LuthierEngine::processBlock's oversized-block path allocates a fresh
    juce::MidiBuffer on the audio thread on every call, so a test that
    drove it here would (correctly) fail; it is reported, not encoded as a
    red test on a report-only branch. */

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../Support/ThreadProbe.h"

#include <cmath>
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
    constexpr int kPreparedBlock = 256;

    /** Marks this thread as the audio thread for the scope (see AudioThreadSafetyTests.cpp). */
    struct AudioThreadScope
    {
        AudioThreadScope()  { ThreadProbe::markAsAudioThread (true); }
        ~AudioThreadScope() { ThreadProbe::markAsAudioThread (false); }
    };

    void engageEBowFreezeAndWhammy (LuthierEngine& engine)
    {
        EBowSettings ebow;
        ebow.enabled = true;
        ebow.stringMask = 0;     // any string with a held note
        ebow.intensity = 0.8;
        ebow.harmonic = 2;
        engine.setEBow (ebow);

        // "A new freeze replaces the layer" unconditionally on a rising edge
        // (FreezeOverlay::setEnabled), so drop it first in case it was
        // already on from a previous call in this test (a live sample-rate
        // change does not itself clear FreezeOverlay::enabled - see the
        // report's reset()-completeness finding).
        auto& freeze = engine.getFreezeOverlay();
        freeze.setEnabled (false);
        freeze.setCaptureMs (300.0);
        freeze.setLevelDb (-3.0);
        freeze.setAttackMs (20.0);
        freeze.setReleaseMs (200.0);
        freeze.setEnabled (true);

        engine.getWhammyEngine().setBridgeType (WhammyEngine::BridgeType::FloydRose);
        engine.getWhammyEngine().setRange (3.0, 1.0);
    }
}

//==============================================================================
LUTHIER_TEST (QaRtSafety, eBowFreezeAndWhammyAcrossBlockSizeEdgesAndASampleRateChange)
{
    LuthierEngine engine;
    engine.prepare (kSr, kPreparedBlock);
    engageEBowFreezeAndWhammy (engine);

    long allocations = 0;
    int locks = 0;
    bool finite = true;
    int firstAllocAt = -1, firstLockAt = -1;
    int callIndex = 0;

    auto warmUp = [&]
    {
        juce::AudioBuffer<float> buffer (2, kPreparedBlock);
        buffer.clear();
        juce::MidiBuffer start;
        start.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110), 0);
        start.addEvent (juce::MidiMessage::controllerEvent (1, 2, 24), 1);   // whammy partway down
        engine.processBlock (buffer, start);      // allowed to size/capture things once, off the counted path
    };

    /** Runs `count` blocks of `numSamples`, measuring allocations/locks on
        each processBlock call as AudioThreadSafetyTests.cpp does. */
    auto runBlocks = [&] (int numSamples, int count)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int i = 0; i < count; ++i)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            // A steady wiggle on the whammy and an occasional new note, so
            // the dive/spring-resonance and E-Bow drive-onset paths run
            // repeatedly rather than settling into one steady state.
            if (i % 37 == 0)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 2, (juce::uint8) ((i * 13) % 128)), 0);

            if (i % 211 == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 45 + (i / 211) % 12, (juce::uint8) 100),
                               juce::jmin (numSamples - 1, 0));

            const auto before = allocationsOnThisThread();
            const int locksBefore = ThreadProbe::audioThreadLocks.load();

            {
                AudioThreadScope scope;
                engine.processBlock (buffer, midi);
            }

            const auto a = allocationsOnThisThread() - before;
            const int lk = ThreadProbe::audioThreadLocks.load() - locksBefore;

            if (a > 0 && firstAllocAt < 0) firstAllocAt = callIndex;
            if (lk > 0 && firstLockAt < 0) firstLockAt = callIndex;
            ++callIndex;

            allocations += a;
            locks += lk;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int s = 0; s < numSamples; ++s)
                    finite = finite && std::isfinite (buffer.getSample (ch, s));
        }
    };

    warmUp();

    // The smallest legal buffer size.
    runBlocks (1, 400);

    // A size that divides neither the prepared size nor a power of two - a
    // host's leftover partial buffer.
    runBlocks (37, 250);

    // Exactly what prepare() promised.
    runBlocks (kPreparedBlock, 100);

    // A live sample-rate change. prepare() is not audio-thread work, so it
    // is called from this test's own (unmarked) thread, exactly as the
    // message thread calls prepareToPlay() on a host's rate switch.
    engine.prepare (96000.0, kPreparedBlock);
    engageEBowFreezeAndWhammy (engine);
    warmUp();

    runBlocks (kPreparedBlock, 100);
    runBlocks (1, 200);
    runBlocks (37, 150);

    CHECK (finite);
    CHECK_MSG (allocations == 0, juce::String (allocations)
               + " allocations with E-Bow + Freeze + whammy engaged, first at call " + juce::String (firstAllocAt));
    CHECK_MSG (locks == 0, juce::String (locks)
               + " blocking locks with E-Bow + Freeze + whammy engaged, first at call " + juce::String (firstLockAt));
}
