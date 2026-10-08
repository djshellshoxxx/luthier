#pragma once

#include <juce_core/juce_core.h>

#include <array>

namespace luthier
{

/**
    SPEC-SWEEP (UW-5 / CB-17): a message-thread -> audio-thread command.

    Plain data only: the audio thread copies it out of the queue and acts on it
    at the top of a block, so the UI never writes engine state the audio thread
    is reading. Anything bigger than a few numbers goes through its own
    TripleBuffer or swap path and is announced here by type alone.
*/
struct EngineCommand
{
    enum class Type : int
    {
        none = 0,
        panic,           ///< release everything (the header's Panic button, the P key)
        stringMute,      ///< index = string, value > 0.5 mutes (choke), else opens
        stringDetune,    ///< index = string, value = cents
        rhythmReset,     ///< restart the rhythm engine's pattern position
        aftertouchTarget,///< index = (int) MidiTarget aftertouch drives (PT-23)
    };

    Type type = Type::none;
    int index = 0;
    float value = 0.0f;

    static EngineCommand make (Type t, int i = 0, float v = 0.0f) noexcept
    {
        EngineCommand c;
        c.type = t;
        c.index = i;
        c.value = v;
        return c;
    }
};

/**
    A fixed-capacity, lock-free single-consumer queue of plain commands.

    Producers are serialised by a SpinLock (they are all on the message thread in
    practice; the lock only guards tests and helpers that post from elsewhere).
    The consumer - the audio thread - never takes that lock: it reads through the
    AbstractFifo's atomics only. A full queue drops the new command and says so.
*/
template <typename Command, int Capacity>
class CommandQueue
{
public:
    bool push (const Command& command) noexcept
    {
        const juce::SpinLock::ScopedLockType sl (writeLock);

        int start1, size1, start2, size2;
        fifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 + size2 < 1)
            return false;

        items[(size_t) (size1 > 0 ? start1 : start2)] = command;
        fifo.finishedWrite (1);
        return true;
    }

    /** Audio thread: hands every queued command to @p handler in order. */
    template <typename Handler>
    int drain (Handler&& handler) noexcept
    {
        const int ready = fifo.getNumReady();

        if (ready <= 0)
            return 0;

        int start1, size1, start2, size2;
        fifo.prepareToRead (ready, start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i)
            handler (items[(size_t) (start1 + i)]);

        for (int i = 0; i < size2; ++i)
            handler (items[(size_t) (start2 + i)]);

        fifo.finishedRead (size1 + size2);
        return size1 + size2;
    }

    int getNumPending() const noexcept { return fifo.getNumReady(); }

private:
    juce::AbstractFifo fifo { Capacity };
    std::array<Command, (size_t) Capacity> items {};
    juce::SpinLock writeLock;
};

} // namespace luthier
