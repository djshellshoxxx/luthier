#pragma once

/*
    SPEC-SWEEP (input-routing IR-11, live-performance LP-34): expression pedal
    calibration on the audio thread.

    ExpressionCalibrationSet lives on the message thread (it saves to disk and
    the Options page edits it). This stage is its audio-thread face:

      - map: a 128 x 128 table of calibrated values, published through a
        TripleBuffer whenever the set changes, rewrites each calibrated CC at the
        front of the input chain, so every consumer after it - MIDI Learn, the
        interpreter, the modulation matrix - sees the pedal's real travel as a
        clean 0..127.
      - observe: while the calibration wizard listens to a CC, its raw values go
        into a small FIFO the message thread drains into the wizard. They used
        never to reach it, so the wizard only ever saw the defaults.
*/

#include "LiveControls.h"
#include "../Support/TripleBuffer.h"

#include <array>
#include <atomic>

namespace luthier
{

class ExpressionStage
{
public:
    ExpressionStage();

    //==========================================================================
    // Message thread

    /** Rebuilds the table from @p set if its version moved. */
    void update (const ExpressionCalibrationSet& set);

    /** The CC the calibration wizard is listening to, or -1. */
    void setObservedCc (int ccNumber) noexcept { observedCc.store (ccNumber, std::memory_order_relaxed); }

    /** Hands every raw value the audio thread observed to @p set's wizard.
        Returns how many there were. */
    int drainObserved (ExpressionCalibrationSet& set);

    //==========================================================================
    // Audio thread

    /** Records wizard values and remaps calibrated CCs in place (rebuilding the
        buffer through @p scratch, which must be pre-sized). Real-time safe. */
    void process (juce::MidiBuffer& midi, juce::MidiBuffer& scratch) noexcept;

private:
    struct Table
    {
        std::array<bool, 128> active {};
        std::array<std::array<juce::uint8, 128>, 128> value {};
        bool any = false;
    };

    TripleBuffer<Table> tables;
    std::atomic<juce::uint32> publishedGeneration { 0 };
    juce::uint32 appliedGeneration = 0;
    juce::uint32 builtFromVersion = 0xffffffffu;
    const Table* live = nullptr;

    std::atomic<int> observedCc { -1 };

    static constexpr int kObservedCapacity = 256;
    juce::AbstractFifo observedFifo { kObservedCapacity };
    std::array<juce::int16, kObservedCapacity> observed {};
};

} // namespace luthier
