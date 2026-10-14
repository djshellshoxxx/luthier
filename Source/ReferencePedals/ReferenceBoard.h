#pragma once

/*  The reference pedalboard: a user-editable, ordered chain of reference pedals
    (spec/reference-pedalboard/ARCHITECTURE.md), separate from the classic racks.

    Threading
      message thread : addPedal, removePedal, movePedal, swapModel, setPlacement,
                       setControl, setEnabled, fromVar/toVar, getters for the UI
      audio thread   : processPre / processLoop only
    Structure changes build and prepare the new processor on the message thread,
    then publish it under a try-locked swap (the same scheme as EffectsChain):
    if the audio thread is mid-block it passes through unchanged for that block.
    Control values and bypass are lock-free atomics read once per block.
*/

#include "ReferencePedalModel.h"

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace luthier::refpedals
{

constexpr int kMaxInstances = 16;

/** A saved instance, kept even when its model is unknown (state is preserved). */
struct InstanceInfo
{
    juce::String instanceId;     ///< stable UUID, survives reorder and swap
    juce::String modelId;
    Placement placement = Placement::Pre;
    bool enabled = true;         ///< footswitch: true = effect engaged
    bool modelMissing = false;   ///< unknown model: shown as disabled, state kept
    std::array<float, kMaxControls> controls {};   ///< normalised
};

class ReferenceBoard
{
public:
    ReferenceBoard();
    ~ReferenceBoard();

    // --- lifecycle (message thread) -----------------------------------------
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // --- board state (message thread) ---------------------------------------
    bool isBoardEnabled() const noexcept { return boardEnabled.load(); }
    void setBoardEnabled (bool on) noexcept;

    int getNumInstances() const noexcept;
    InstanceInfo getInstance (int index) const;                 ///< copy, in chain order
    int indexOfInstance (const juce::String& instanceId) const;

    /** Appends a pedal built from the registry. Returns its instanceId, or an
        empty string if the board is full or the model unknown. */
    juce::String addPedal (const juce::String& modelId, Placement placement);
    bool removePedal (const juce::String& instanceId);
    bool movePedal (const juce::String& instanceId, int newIndex);     ///< reorder
    bool swapModel (const juce::String& instanceId, const juce::String& newModelId);
    juce::String duplicatePedal (const juce::String& instanceId);
    bool setPlacement (const juce::String& instanceId, Placement);
    bool setEnabled (const juce::String& instanceId, bool engaged);
    bool setControl (const juce::String& instanceId, int controlIndex, double normalised);
    void resetControls (const juce::String& instanceId);
    void clear();

    /** Bumped on every structural or control change; the UI polls it. */
    uint32_t getRevision() const noexcept { return revision.load(); }

    // --- persistence --------------------------------------------------------
    /** Versioned var: {schema:1, enabled, instances:[{id, model, placement,
        enabled, controls:{<controlId>:norm}}]}. Unknown keys and unknown
        models round-trip. */
    juce::var toVar() const;
    /** A void var, or anything not an object, restores the built default
        (empty, disabled). Never throws. */
    void fromVar (const juce::var&);
    bool isDefault() const;      ///< true when toVar need not be written

    // --- audio thread -------------------------------------------------------
    /** Runs every enabled Pre / Loop instance in order, in place. A no-op (and
        bit-exact) while the board is disabled or empty. */
    void processPre  (double* left, double* right, int numSamples) noexcept;
    void processLoop (double* left, double* right, int numSamples) noexcept;

    int getLatencySamples() const noexcept;      ///< Pre-stage latency, nominal

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<bool> boardEnabled { false };
    std::atomic<uint32_t> revision { 0 };
};

//==============================================================================
/** The catalogue. Only models present here appear in the UI. */
struct ModelEntry
{
    const ModelDescriptor* descriptor;
    std::unique_ptr<ReferencePedalProcessor> (*create)();
};

const std::vector<ModelEntry>& getModelRegistry();
const ModelEntry* findModel (const juce::String& modelId);

} // namespace luthier::refpedals
