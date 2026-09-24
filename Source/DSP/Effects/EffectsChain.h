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

    /** Reset that never blocks: for panic / reset on the audio thread, where
        waiting on the message thread's swapLock (setSlotType, moveSlot) would
        stall the callback. Resets every pedal now when the lock is free, else
        leaves it pending for the next processStereo to carry out. */
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
    Pedal* getPedal (int slot) noexcept;
    const Pedal* getPedal (int slot) const noexcept;

    void setSlotBypassed (int slot, bool bypassed) noexcept;
    bool isSlotBypassed (int slot) const noexcept;

    void setSlotMix (int slot, double mix) noexcept;

    /** One slot's per-block controls, as the parameter layer polls them. */
    struct SlotControls
    {
        bool bypassed = false;
        double mix = 1.0;
        std::array<float, Pedal::kMaxParams> normalised {};
    };

    /*  Pushes every slot's bypass, mix and pedal parameters from the audio
        thread. Takes the swap lock with a try-lock, so a slot being replaced
        on the message thread makes this block's push wait for the next one
        rather than touching a pedal that is being freed. False when skipped;
        the values are polled again next block, so nothing is lost. */
    bool applyControls (const std::array<SlotControls, kNumSlots>& controls) noexcept;

    /** Moves a pedal from one slot to another, shuffling the rest along. */
    void moveSlot (int fromSlot, int toSlot);

    /** Empties every slot. */
    void clear();

    //==========================================================================
    void setTempoBpm (double bpm) noexcept;
    void setExpression (double value) noexcept;
    void setOversamplingFactor (int factor) noexcept;

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
        PedalType type = PedalType::None;
        bool bypassed = false;
        double mix = 1.0;
    };

    void applyPendingSwaps() noexcept;

    double sr = 44100.0;
    int maxBlock = 512;
    Position position = Position::PreAmp;

    double tempoBpm = 120.0;
    double expression = 0.5;
    int oversamplingFactor = 4;

    std::array<Slot, kNumSlots> slots;

    // Pedals retired by a type change are parked here and freed on the message
    // thread, never inside processBlock.
    std::vector<std::unique_ptr<Pedal>> retired;
    juce::CriticalSection swapLock;
    std::atomic<bool> resetPending { false };

    /** Every pedal's reset; the caller holds swapLock. */
    void resetPedalsLocked() noexcept;

    std::vector<double> workL, workR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsChain)
};

} // namespace luthier
