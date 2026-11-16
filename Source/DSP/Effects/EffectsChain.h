#pragma once

/*  An eight-slot pedal rack (engine spec 10 and 12).

    Slots are ordered, reorderable and independently bypassable. Changing a slot's
    pedal type allocates on the message thread and hands the finished pedal to the
    audio thread through a swap, so processBlock never allocates.
*/

#include "Pedal.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace luthier
{

class EffectsChain
{
public:
    static constexpr int kNumSlots = 8;

    enum class Position { PreAmp, PostAmp };

    EffectsChain();
    ~EffectsChain();

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** reset() for the audio thread (Panic, live-performance 9.2): a try-lock,
        because waiting on the message thread's swapLock (setSlotType, moveSlot)
        would stall the callback. Resets every pedal now when the lock is free,
        else leaves it pending for the next processStereo to carry out. */
    void resetFromAudioThread() noexcept;

    /** True while a resetFromAudioThread is waiting for the next block (tests). */
    bool isResetPending() const noexcept { return resetPending.load (std::memory_order_acquire); }

    void setPosition (Position p) noexcept { position = p; }
    Position getPosition() const noexcept { return position; }

    //==========================================================================
    /** Replaces the pedal in a slot. Allocates; call from the message thread. */
    void setSlotType (int slot, PedalType type);
    PedalType getSlotType (int slot) const noexcept;

    /** The pedal in a slot, or nullptr if the slot is empty. Only safe to touch
        from the message thread between setSlotType calls. */
    /** Audio thread: a slot's bypass, mix and parameters (normalised), under
        the swap lock. A pedal being swapped this instant is skipped; the next
        block applies it. getPedal() is for the message thread, which is the
        one that swaps, never for the audio thread. */
    void applySlotState (int slot, bool bypassed, double mix, const float* normalisedParams, int numParams) noexcept;

    Pedal* getPedal (int slot) noexcept;
    const Pedal* getPedal (int slot) const noexcept;

    void setSlotBypassed (int slot, bool bypassed) noexcept;
    bool isSlotBypassed (int slot) const noexcept;

    void setSlotMix (int slot, double mix) noexcept;

    /** Moves a pedal from one slot to another, shuffling the rest along. */
    void moveSlot (int fromSlot, int toSlot);

    /** Message thread: reclaim pedals after all unlocked readers have left. */
    void reclaimRetired();

    /** Empties every slot. */
    void clear();

    //==========================================================================
    void setTempoBpm (double bpm) noexcept;
    void setExpression (double value) noexcept;
    void setOversamplingFactor (int factor) noexcept { setOversamplingFactor (factor, factor, false); }

    /** cpu-quality-modes 2.2: drive pedals run at `effective` and report
        `nominal`'s latency; a factor change crossfades when asked. */
    void setOversamplingFactor (int effective, int nominal, bool crossfade) noexcept;
    int getEffectiveOversamplingFactor() const noexcept { return effectiveFactor; }

    /** Total latency of the chain, in samples. */
    int getLatencySamples() const noexcept;

    //==========================================================================
    void processBlock (juce::AudioBuffer<float>& buffer) noexcept;

    /** Double-precision path used by the engine and the offline renderer. */
    void processStereo (double* left, double* right, int numSamples) noexcept;

private:
    struct Slot
    {
        std::unique_ptr<Pedal> pedal;
        std::atomic<Pedal*> live { nullptr };
        std::atomic<PedalType> type { PedalType::None };
        std::atomic<bool> bypassed { false };
        std::atomic<double> mix { 1.0 };

        Slot() = default;
        Slot (Slot&& other) noexcept { *this = std::move (other); }
        Slot& operator= (Slot&& other) noexcept
        {
            // moveSlot only moves into an empty owner under swapLock. Readers
            // can observe an intermediate order, but never a destroyed pedal.
            jassert (pedal == nullptr);
            other.live.store (nullptr);
            pedal = std::move (other.pedal);
            type.store (other.type.load());
            bypassed.store (other.bypassed.load());
            mix.store (other.mix.load());
            live.store (pedal.get());
            return *this;
        }
    };

    friend struct EffectsChainLifetimeTestAccess;

    // seq_cst entry, pointer publication and zero observation form one order:
    // readers that could see an outgoing pedal must leave before reclamation.
    struct ReadScope
    {
        explicit ReadScope (const EffectsChain& c) noexcept : chain (c)
        { chain.unlockedReaders.fetch_add (1); }
        ~ReadScope() { chain.unlockedReaders.fetch_sub (1); }
        const EffectsChain& chain;
        ReadScope (const ReadScope&) = delete;
        ReadScope& operator= (const ReadScope&) = delete;
    };
    mutable std::atomic<unsigned> unlockedReaders { 0 };
    static_assert (std::atomic<unsigned>::is_always_lock_free);
    static_assert (std::atomic<double>::is_always_lock_free);
    static_assert (std::atomic<Pedal*>::is_always_lock_free);

    void applyPendingSwaps() noexcept;
    void resetPedalsLocked() noexcept;

    double sr = 44100.0;
    int maxBlock = 512;
    Position position = Position::PreAmp;

    std::atomic<double> tempoBpm { 120.0 };
    std::atomic<double> expression { 0.5 };
    std::atomic<int> oversamplingFactor { 4 };
    std::atomic<int> effectiveFactor { 4 };   // cpu-quality-modes 2.2

    std::array<Slot, kNumSlots> slots;

    // Pedals retired by a type change are parked here and freed on the message
    // thread, never inside processBlock.
    std::vector<std::unique_ptr<Pedal>> retired;
    juce::CriticalSection swapLock;
    std::atomic<bool> resetPending { false };

    std::vector<double> workL, workR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsChain)
};

} // namespace luthier
