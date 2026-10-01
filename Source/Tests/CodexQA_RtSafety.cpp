/*  CODEX-RTSAFETY: audio-thread allocation/lock probes for the paths the static
    review (docs/review/CODEX_RTSAFETY.md) flagged that the existing
    AudioThreadSafetyTests fixture did not exercise:

      - a LIVE part swap (P0): used to rebuild and sort the body's modal bank on
        the audio thread under a blocking lock;
      - the hidden ("Wolf") effect enabled mid-playback (P1): its stereo scratch
        was a thread_local std::vector grown on the first block after enabling;
      - a host block larger than the prepared size (P1): sliced host MIDI into a
        local juce::MidiBuffer and grew the pre/post-pedal scratch in the render;
      - a humanisation change while the band plays (P0, confirmed already fixed):
        the audio thread reads the humanise value from a lock-free TripleBuffer.

    These reuse CircuitTests.cpp's per-thread allocation counter and the
    pthread_mutex_lock interposer installed in AudioThreadSafetyTests.cpp, so a
    blocking lock or allocation on the marked audio thread is counted here too.
    Try-locks are never counted: they do not wait.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../LuthierEngine.h"
#include "../Rhythm/RhythmEngine.h"
#include "../Rhythm/Patterns.h"
#include "../Model/Workshop/PartAcoustics.h"
#include "../Support/ThreadProbe.h"

#include <atomic>
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

    /** Marks this thread as the audio thread for the scope (as AudioThreadSafetyTests). */
    struct AudioThreadScope
    {
        AudioThreadScope()  { ThreadProbe::markAsAudioThread (true); }
        ~AudioThreadScope() { ThreadProbe::markAsAudioThread (false); }
    };

    WorkshopGuitar factoryGuitar (PartLibrary& library, GuitarType type)
    {
        WorkshopGuitar g;
        PartLibrary::LoadReport report;
        library.loadGuitar (PartLibrary::getFactoryGuitarsFolder()
                              .getChildFile (LuthierAudioProcessor::getFactoryGuitarPath (type)), g, report);
        return g;
    }
}

//==============================================================================
/*  P0: a live part swap is built on the message thread and adopted at the audio
    thread's block boundary. The block that adopts it must neither allocate nor
    take a blocking lock. The audio thread renders on its own thread (as a host
    does), marked and probed; the message thread requests the swap once the
    render is warm. */
LUTHIER_TEST (CodexRtSafety, aLivePartSwapAllocatesAndLocksNothing)
{
    constexpr int kBlock = 256;

    PartLibrary library;
    library.refresh();

    const auto before = factoryGuitar (library, GuitarType::LesPaul);
    auto after = before;

    // A different bridge pickup: keeps the structure (string count, family), so
    // the swap is taken live rather than parked.
    for (const auto& part : library.getParts (PartType::pickup))
        if (part->name != before.get (GuitarSlot::pickupBridge)->name)
            { after.parts[(size_t) GuitarSlot::pickupBridge] = part; break; }

    const auto derivedAfter = mapSpec (after);

    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.applyWorkshopGuitar (mapSpec (before), GuitarType::LesPaul);
    CHECK (engine.partSwapKeepsStructure (derivedAfter));

    std::atomic<int> blocksRendered { 0 };
    std::atomic<bool> measuring { false };
    std::atomic<bool> stop { false };
    std::atomic<long> allocDuringMeasure { 0 };
    std::atomic<int>  lockDuringMeasure { 0 };
    std::atomic<bool> finite { true };

    std::thread audio ([&]
    {
        AudioThreadScope scope;

        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;
        midi.ensureSize (1024);
        bool firstBlock = true;

        while (! stop.load (std::memory_order_acquire))
        {
            midi.clear();

            if (firstBlock)
            {
                for (int note : { 45, 52, 57 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);
                firstBlock = false;
            }

            buffer.clear();

            const bool measure = measuring.load (std::memory_order_acquire);
            const auto allocBefore = allocationsOnThisThread();
            const int  lockBefore  = ThreadProbe::audioThreadLocks.load();

            engine.processBlock (buffer, midi);

            if (measure)
            {
                allocDuringMeasure.fetch_add (allocationsOnThisThread() - allocBefore);
                lockDuringMeasure.fetch_add (ThreadProbe::audioThreadLocks.load() - lockBefore);
            }

            for (int i = 0; i < kBlock; ++i)
                if (! std::isfinite (buffer.getSample (0, i)))
                    finite.store (false);

            blocksRendered.fetch_add (1, std::memory_order_release);
            std::this_thread::yield();
        }
    });

    // Let the render warm up (first-block sizing is allowed), then measure.
    while (blocksRendered.load (std::memory_order_acquire) < 64)
        std::this_thread::yield();

    measuring.store (true, std::memory_order_release);

    // A couple of measured blocks with nothing happening first.
    const int base = blocksRendered.load();
    while (blocksRendered.load() < base + 4)
        std::this_thread::yield();

    bool taken = false;
    for (int attempt = 0; attempt < 50 && ! taken; ++attempt)
    {
        taken = engine.swapPartsAtBlockBoundary (derivedAfter, GuitarType::LesPaul);
        if (! taken)
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }

    // Let the swap-adopting block and several after it be measured.
    const int afterSwap = blocksRendered.load();
    while (blocksRendered.load() < afterSwap + 16)
        std::this_thread::yield();

    stop.store (true, std::memory_order_release);
    audio.join();

    CHECK_MSG (taken, "the swap was not taken live at a block boundary");
    CHECK (engine.getLivePartSwapCount() == 1);
    CHECK (finite.load());
    CHECK_MSG (allocDuringMeasure.load() == 0,
               juce::String (allocDuringMeasure.load()) + " allocations on the audio thread across the live swap");
    CHECK_MSG (lockDuringMeasure.load() == 0,
               juce::String (lockDuringMeasure.load()) + " blocking locks on the audio thread across the live swap");
}

//==============================================================================
/*  P1: the hidden "Wolf" effect's stereo scratch is engine-owned and sized in
    prepare, so the first block after it is enabled mid-playback allocates
    nothing. The effect is enabled directly on the engine (its scratch was a
    thread_local std::vector grown lazily on that first enabled block, which the
    operator-new counter sees). */
LUTHIER_TEST (CodexRtSafety, enablingTheHiddenEffectMidPlaybackDoesNotAllocate)
{
    constexpr int kBlock = 128;

    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.setGuitarType (GuitarType::LesPaul);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;
    midi.ensureSize (1024);

    // Warm up with the effect OFF, so its scratch is never sized before measuring.
    for (int b = 0; b < 16; ++b)
    {
        midi.clear();
        if (b == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
        buffer.clear();
        AudioThreadScope scope;
        engine.processBlock (buffer, midi);
    }

    // Enable the hidden effect (as a parameter write would, mid-playback).
    engine.getSecretEffect().setEnabled (true);
    CHECK (engine.getSecretEffect().isEnabled());

    long allocations = 0;
    int locks = 0;
    bool finite = true;

    for (int b = 0; b < 16; ++b)
    {
        midi.clear();
        buffer.clear();

        const auto allocBefore = allocationsOnThisThread();
        const int  lockBefore  = ThreadProbe::audioThreadLocks.load();

        {
            AudioThreadScope scope;
            engine.processBlock (buffer, midi);
        }

        allocations += allocationsOnThisThread() - allocBefore;
        locks += ThreadProbe::audioThreadLocks.load() - lockBefore;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < kBlock; ++i)
                finite = finite && std::isfinite (buffer.getSample (ch, i));
    }

    CHECK (finite);
    CHECK_MSG (allocations == 0,
               juce::String (allocations) + " allocations after enabling the hidden effect");
    CHECK_MSG (locks == 0,
               juce::String (locks) + " blocking locks after enabling the hidden effect");
}

//==============================================================================
/*  P1: the pre/post-pedal stereo scratch is sized to maxBlock in prepare. The
    old thread_local std::vector grew whenever a block exceeded its running
    high-water mark, so a block larger than the warm-up size reallocated inside
    the render. Here the engine is prepared for a large block, warmed at a small
    one, then driven at the large one: the member scratch must not reallocate. */
LUTHIER_TEST (CodexRtSafety, aBlockLargerThanTheWarmUpDoesNotGrowScratch)
{
    constexpr int kMaxBlock = 512;
    constexpr int kSmall    = 96;

    LuthierEngine engine;
    engine.prepare (kSr, kMaxBlock);
    engine.setGuitarType (GuitarType::LesPaul);

    juce::AudioBuffer<float> small (2, kSmall);
    juce::AudioBuffer<float> large (2, kMaxBlock);
    juce::MidiBuffer midi;
    midi.ensureSize (1024);

    // Warm up at the small size (sizes the old thread_local scratch to kSmall).
    for (int b = 0; b < 16; ++b)
    {
        midi.clear();
        if (b == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
        small.clear();
        AudioThreadScope scope;
        engine.processBlock (small, midi);
    }

    long allocations = 0;
    int locks = 0;
    bool finite = true;

    // Now render larger blocks: the old scratch would grow kSmall -> kMaxBlock.
    for (int b = 0; b < 16; ++b)
    {
        midi.clear();
        large.clear();

        const auto allocBefore = allocationsOnThisThread();
        const int  lockBefore  = ThreadProbe::audioThreadLocks.load();

        {
            AudioThreadScope scope;
            engine.processBlock (large, midi);
        }

        allocations += allocationsOnThisThread() - allocBefore;
        locks += ThreadProbe::audioThreadLocks.load() - lockBefore;

        for (int ch = 0; ch < large.getNumChannels(); ++ch)
            for (int i = 0; i < kMaxBlock; ++i)
                finite = finite && std::isfinite (large.getSample (ch, i));
    }

    CHECK (finite);
    CHECK_MSG (allocations == 0,
               juce::String (allocations) + " allocations when the block grew past the warm-up size");
    CHECK_MSG (locks == 0,
               juce::String (locks) + " blocking locks when the block grew past the warm-up size");
}

//==============================================================================
/*  P1: a host block larger than the prepared size is split into prepared-size
    slices. The MIDI slice buffers (directSlice, sliceMidi) are members reserved
    in prepare rather than a local created in the callback. juce::MidiBuffer
    allocates through juce's own heap, which this counter does not see, so the
    slice-buffer fix is correct by construction; this test guards the split path
    against std::vector allocations, blocking locks and non-finite output under
    dense MIDI. */
LUTHIER_TEST (CodexRtSafety, anOversizedHostBlockAllocatesAndLocksNothing)
{
    constexpr int kPrepared = 128;
    constexpr int kOversized = 1024;   // eight prepared slices

    // The engine's oversized path is where the slice MIDI buffers and the pre/post
    // pedal scratch live, so drive it directly to isolate that fix.
    LuthierEngine engine;
    engine.prepare (kSr, kPrepared);
    engine.setGuitarType (GuitarType::LesPaul);

    juce::AudioBuffer<float> buffer (2, kOversized);
    juce::MidiBuffer midi;
    midi.ensureSize (8192);

    auto fillDenseMidi = [&] (bool withNote)
    {
        midi.clear();
        if (withNote)
            midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
        // Many events spread across the whole oversized block, so every slice
        // carries some and addEvent is exercised on each slice buffer.
        for (int i = 0; i < 128; ++i)
        {
            const int pos = (i * kOversized) / 128;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, i % 128), pos);
            midi.addEvent (juce::MidiMessage::pitchWheel (1, 8192 + (i % 32) * 100), pos);
        }
    };

    // Warm up (first oversized block may size things once).
    fillDenseMidi (true);
    buffer.clear();
    { AudioThreadScope scope; engine.processBlock (buffer, midi); }

    long allocations = 0;
    int locks = 0;
    bool finite = true;

    for (int b = 0; b < 16; ++b)
    {
        fillDenseMidi (false);
        buffer.clear();

        const auto allocBefore = allocationsOnThisThread();
        const int  lockBefore  = ThreadProbe::audioThreadLocks.load();

        {
            AudioThreadScope scope;
            engine.processBlock (buffer, midi);
        }

        allocations += allocationsOnThisThread() - allocBefore;
        locks += ThreadProbe::audioThreadLocks.load() - lockBefore;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < kOversized; ++i)
                finite = finite && std::isfinite (buffer.getSample (ch, i));
    }

    CHECK (finite);
    CHECK_MSG (allocations == 0,
               juce::String (allocations) + " allocations on an oversized host block");
    CHECK_MSG (locks == 0,
               juce::String (locks) + " blocking locks on an oversized host block");
}

//==============================================================================
/*  P0 (confirmed already fixed): the audio thread reads the humanise value from
    a lock-free TripleBuffer, so a message-thread humanisation change while the
    band is strumming never makes the audio thread take a blocking lock. The
    audio thread renders the rhythm engine; the message thread hammers
    setHumanise throughout. */
LUTHIER_TEST (CodexRtSafety, changingHumanisationWhileTheBandPlaysTakesNoAudioThreadLock)
{
    constexpr int kBlock = 128;

    TuningEngine tuning;
    tuning.prepare (kSr);
    tuning.setNumStrings (6);
    tuning.setTuningPreset (TuningPreset::Standard);

    RubricVoicer voicer;
    voicer.prepare (&tuning, 6);

    RhythmEngine engine;
    engine.prepare (kSr, kBlock, &tuning, &voicer);
    engine.setNumStrings (6);
    engine.setEnabled (true);
    engine.setStrumDurationMs (8.0);

    // A sixteenth-note strum pattern, so strokes schedule every block and the
    // audio path reads the humanise buffer repeatedly.
    RhythmPattern pattern;
    pattern.setName ("RT Strums");
    pattern.setKind (RhythmPattern::Kind::strum);
    pattern.setSubdivision (Subdivision::sixteenth);
    pattern.setLength (16);
    for (int i = 0; i < 16; ++i)
    {
        StrumStep step;
        step.type = (i % 2 == 0) ? StrumType::down : StrumType::up;
        step.dynamic = 1.0;
        step.stringMask = 0x0FFF;
        pattern.setStrumStep (i, step);
    }
    engine.setPattern (pattern);

    // Hold a chord so the engine has something to voice.
    {
        juce::MidiBuffer held;
        for (int note : { 40, 47, 52, 56, 59, 64 })
            held.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
        engine.handleMidi (held, 0);
    }

    std::atomic<int> blocksRendered { 0 };
    std::atomic<bool> stop { false };
    std::atomic<int> lockDuringMeasure { 0 };
    std::atomic<bool> measuring { false };

    std::thread audio ([&]
    {
        AudioThreadScope scope;
        PlayEventQueue out;
        int64_t sample = 0;

        while (! stop.load (std::memory_order_acquire))
        {
            RhythmTransport transport;
            transport.bpm = 120.0;
            transport.isPlaying = true;
            transport.ppqPosition = (double) sample / (60.0 / 120.0 * kSr);

            const bool measure = measuring.load (std::memory_order_acquire);
            const int lockBefore = ThreadProbe::audioThreadLocks.load();

            out.clear();
            engine.processBlock (kBlock, transport, out);

            if (measure)
                lockDuringMeasure.fetch_add (ThreadProbe::audioThreadLocks.load() - lockBefore);

            sample += kBlock;
            blocksRendered.fetch_add (1, std::memory_order_release);
            std::this_thread::yield();
        }
    });

    while (blocksRendered.load (std::memory_order_acquire) < 16)
        std::this_thread::yield();

    measuring.store (true, std::memory_order_release);

    // Hammer humanise changes from the message thread while audio renders.
    for (int i = 0; i < 400; ++i)
    {
        RhythmHumanise h;
        h.timingMs = 1.0 + (i % 10);
        h.velocityPercent = (double) (i % 30);
        h.missPercent = (double) (i % 5);
        h.ghostPercent = (double) (i % 7);
        h.amount = (i % 100) / 100.0;
        engine.setHumanise (h);
        std::this_thread::yield();
    }

    const int base = blocksRendered.load();
    while (blocksRendered.load() < base + 16)
        std::this_thread::yield();

    stop.store (true, std::memory_order_release);
    audio.join();

    CHECK_MSG (lockDuringMeasure.load() == 0,
               juce::String (lockDuringMeasure.load())
                 + " blocking locks on the audio thread while humanisation changed");
}
