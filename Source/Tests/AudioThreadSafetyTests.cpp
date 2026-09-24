/*  performance-budget.md 0.4 / 0.5 / 10: the audio callback allocates nothing
    and locks nothing.

    Two traps. Allocation: CircuitTests.cpp replaces the global operator new
    and counts per thread (luthier::tests::allocationsOnThisThread). Locks:
    ThreadProbe::noteLock counts a blocking lock on a thread marked as the
    audio thread; ProbedCriticalSection calls it, and on Linux this file also
    interposes pthread_mutex_lock itself, so every blocking lock the test
    binary takes - JUCE's CriticalSection, std::mutex, anything - is seen, not
    only the probed ones. Try-locks are not counted: they never wait.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Practice/Looper.h"
#include "../Support/MidiLearn.h"
#include "../Support/ThreadProbe.h"

#if JUCE_LINUX || JUCE_BSD
 #include <dlfcn.h>
 #include <pthread.h>

namespace
{
    using PthreadLockFn = int (*) (pthread_mutex_t*);
    std::atomic<PthreadLockFn> realPthreadLock { nullptr };
}

extern "C" int pthread_mutex_lock (pthread_mutex_t* mutex)
{
    auto fn = realPthreadLock.load (std::memory_order_acquire);

    if (fn == nullptr)
    {
        fn = (PthreadLockFn) dlsym (RTLD_NEXT, "pthread_mutex_lock");
        realPthreadLock.store (fn, std::memory_order_release);
    }

    luthier::ThreadProbe::noteLock();
    return fn (mutex);
}
 #define LUTHIER_LOCK_INTERPOSED 1
#endif

namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;
}

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    /** Marks this thread as the audio thread for the scope. */
    struct AudioThreadScope
    {
        AudioThreadScope()  { ThreadProbe::markAsAudioThread (true); }
        ~AudioThreadScope() { ThreadProbe::markAsAudioThread (false); }
    };
}

//==============================================================================
LUTHIER_TEST (ThreadProbe, theLockTrapSeesALock)
{
    // The trap itself, so a pass below means "no lock" and not "no trap".
    juce::CriticalSection plain;
    ThreadProbe::ProbedCriticalSection probed;

    const int before = ThreadProbe::audioThreadLocks.load();

    {
        AudioThreadScope audio;
        const ThreadProbe::ProbedScopedLock a (probed);
        const juce::ScopedTryLock tryLock (plain);        // a try-lock never waits
        juce::ignoreUnused (a, tryLock);
    }

    CHECK (ThreadProbe::audioThreadLocks.load() - before >= 1);

   #if LUTHIER_LOCK_INTERPOSED
    const int mid = ThreadProbe::audioThreadLocks.load();

    {
        AudioThreadScope audio;
        const juce::ScopedLock b (plain);
        juce::ignoreUnused (b);
    }

    CHECK_MSG (ThreadProbe::audioThreadLocks.load() - mid == 1, "the pthread interposer did not see a CriticalSection");
   #endif

    // An unmarked thread is never counted.
    const int after = ThreadProbe::audioThreadLocks.load();
    { const ThreadProbe::ProbedScopedLock c (probed); juce::ignoreUnused (c); }
    CHECK (ThreadProbe::audioThreadLocks.load() == after);
}

//==============================================================================
/*  PB-0.4: Looper::captureMidi used MidiMessageSequence::addEvent on the audio
    thread. It now writes a pre-sized FIFO the message thread drains. */
LUTHIER_TEST (PracticeLooper, recordingMidiDoesNotAllocate)
{
    Looper looper;
    looper.prepare (kSr, 10.0);
    looper.press();                           // recording the first layer
    CHECK (looper.getState() == Looper::State::recordingFirst);

    juce::MidiBuffer midi;
    midi.ensureSize (4096);

    long allocations = 0;
    int locks = 0;
    int sent = 0;
    juce::AudioBuffer<float> audio (2, 128);

    for (int block = 0; block < 1000; ++block)
    {
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 40 + block % 24, (juce::uint8) 100), 3);
        midi.addEvent (juce::MidiMessage::noteOff (1, 40 + block % 24), 90);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, block % 128), 100);
        sent += 3;
        audio.clear();

        const auto before = allocationsOnThisThread();
        const int locksBefore = ThreadProbe::audioThreadLocks.load();

        {
            AudioThreadScope scope;
            looper.captureMidi (midi, 128);
            looper.processBlock (audio, 128);
        }

        allocations += allocationsOnThisThread() - before;
        locks += ThreadProbe::audioThreadLocks.load() - locksBefore;

        if (block % 100 == 99)
            looper.drainPendingMidi();        // the processor's timer, in the plugin
    }

    looper.drainPendingMidi();

    CHECK_MSG (allocations == 0, juce::String (allocations) + " allocations while recording MIDI");
    CHECK_MSG (locks == 0, juce::String (locks) + " blocking locks while recording MIDI");
    CHECK_MSG (looper.getLayer (0).getMidi().getNumEvents() == sent,
               juce::String (looper.getLayer (0).getMidi().getNumEvents()) + " of " + juce::String (sent) + " events kept");
}

//==============================================================================
/*  PB-0.4 / PB-0.5: MIDI Learn took a CriticalSection and posted a lambda
    holding a String from the audio thread while learning. */
LUTHIER_TEST (MidiLearn, learningDoesNotAllocateOrLockOnTheAudioThread)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& learn = processor->getMidiLearn();

    learn.startLearning (ParamIDs::macroDrive);

    juce::MidiBuffer midi;
    midi.ensureSize (1024);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);   // sustain: skipped
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 21, 64), 10);

    const auto before = allocationsOnThisThread();
    const int locksBefore = ThreadProbe::audioThreadLocks.load();

    {
        AudioThreadScope scope;
        learn.processMidi (midi);
    }

    const auto allocations = allocationsOnThisThread() - before;
    const int locks = ThreadProbe::audioThreadLocks.load() - locksBefore;

    CHECK_MSG (allocations == 0, juce::String (allocations) + " allocations while learning");
    CHECK_MSG (locks == 0, juce::String (locks) + " blocking locks while learning");

    // The message thread picks it up.
    learn.servicePendingLearn();
    CHECK (learn.getCcForParameter (ParamIDs::macroDrive) == 21);
    CHECK (! learn.isLearning());

    // Applying a mapping afterwards allocates nothing either.
    juce::MidiBuffer apply;
    apply.ensureSize (1024);
    apply.addEvent (juce::MidiMessage::controllerEvent (1, 21, 100), 0);

    const auto before2 = allocationsOnThisThread();

    {
        AudioThreadScope scope;
        learn.processMidi (apply);
    }

    CHECK_MSG (allocationsOnThisThread() == before2, "applying a learned CC allocated");
}

//==============================================================================
/*  PB-10.4 / PB-10.5: the whole processor, a MIDI fixture, 48 kHz / 128.
    Five minutes of audio in the LUTHIER_PERF=1 run; forty seconds in the
    default run, which covers every path the fixture reaches (the fixture
    cycles in eight seconds). One warm-up block first: the first block may
    size things once, which the spec permits before playback. */
LUTHIER_TEST (Engine, fiveMinutesOfPlaybackNeitherAllocatesNorLocks)
{
    constexpr int kBlock = 128;
    const double seconds = perfRunRequested() ? 300.0 : 40.0;

    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    juce::AudioBuffer<float> buffer (processor->getTotalNumOutputChannels(), kBlock);
    juce::MidiBuffer midi;
    midi.ensureSize (8192);

    // Warm-up.
    buffer.clear();
    processor->processBlock (buffer, midi);

    const int blocks = (int) (seconds * kSr / kBlock);
    const int blocksPerBeat = (int) (0.25 * kSr / kBlock);      // 16ths at 60 bpm
    static constexpr int chord[] = { 40, 47, 52, 55, 59, 64, 45, 52, 57, 60, 64, 69 };

    long allocations = 0;
    int locks = 0;
    int firstAllocBlock = -1, firstLockBlock = -1;
    bool finite = true;

    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();

        if (b % blocksPerBeat == 0)
        {
            const int step = (b / blocksPerBeat) % 32;
            const int note = chord[step % 12];
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (60 + step * 2)), 5);

            if (step % 4 == 3)
                midi.addEvent (juce::MidiMessage::allNotesOff (1), 64);

            if (step % 8 == 0)
                midi.addEvent (juce::MidiMessage::pitchWheel (1, 8192 + (step - 16) * 200), 20);

            midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, step * 4), 30);
        }

        buffer.clear();

        const auto before = allocationsOnThisThread();
        const int locksBefore = ThreadProbe::audioThreadLocks.load();

        {
            AudioThreadScope scope;
            processor->processBlock (buffer, midi);
        }

        const auto a = allocationsOnThisThread() - before;
        const int l = ThreadProbe::audioThreadLocks.load() - locksBefore;

        if (a > 0 && firstAllocBlock < 0) firstAllocBlock = b;
        if (l > 0 && firstLockBlock < 0)  firstLockBlock = b;

        allocations += a;
        locks += l;

        if (b % 64 == 0)
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < kBlock; ++i)
                    finite = finite && std::isfinite (buffer.getSample (ch, i));
    }

    CHECK (finite);
    CHECK_MSG (allocations == 0, juce::String (allocations) + " allocations on the audio thread, first in block "
                                   + juce::String (firstAllocBlock));
    CHECK_MSG (locks == 0, juce::String (locks) + " blocking locks on the audio thread, first in block "
                             + juce::String (firstLockBlock));
}
