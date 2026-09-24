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

    std::vector<double> workL, workR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsChain)
};

} // namespace luthier
