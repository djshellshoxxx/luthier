// OWNER: WP2 (interface frozen by WP1, I-3: the public section below changes only via errata)
#pragma once

// Facade of the reference pedalboard, owned by LuthierEngine::referencePedals.
// Experimental, independently routed pedal engine: the legacy
// EffectsChain/PedalType and its factory presets are untouched, and with the
// board off or empty processStereo() returns before it touches a buffer
// (bit-identical legacy path, T-LEGACY).
//
// This header stays cheap on purpose: only light headers, BoardModel and
// MidiRouter are forward-declared and all heap state is behind a pimpl.
//
// Interim (WP1): Board.cpp still runs the original three prototype models
// through the legacy shims so the prototype UI and tests keep working. WP2
// replaces the body with BoardEngine; the public interface stays.

#include <memory>
#include <juce_audio_basics/juce_audio_basics.h>
#include "BoardTypes.h"
#include "Midi/ExtendedHostAutomation.h"
#include "LegacyModel.h"

namespace luthier::reference
{

class BoardModel;
class IAssetStore;
namespace midi { class MidiRouter; }

class Board final
{
public:
    static constexpr int slotCount = kMaxSlots;     // legacy name (prototype had 8)
    static constexpr int modelCount = kLegacyModelCount;

    Board();
    ~Board();
    Board (const Board&) = delete;
    Board& operator= (const Board&) = delete;

    //==========================================================================
    // Message thread
    void prepare (double sampleRate, int maxBlock = 4096);   // allocates pool, delay lines, scratch; idempotent
    void releaseResources();
    juce::String getPreparationError() const; // E69: off-audio concrete failure, empty after recovery
    // E59: before prepare or after stopped release; false if prepared/runtimes retained.
    // Null explicitly disables persistent storage; the default is lazy app-data storage.
    bool setAssetStore (std::shared_ptr<IAssetStore>);
    void reset() noexcept;                                   // clears DSP state (legacy name; prepare() calls it)
    BoardModel& model() noexcept;                            // single truth; edits via model().apply(BoardCommand)
    midi::MidiRouter& midi() noexcept;
    int reclaim() noexcept;                                  // 30 Hz UI timer / after prepare: frees retired topologies and DSP
    PedalMeters readPedalMeters (int slot) const noexcept; // E47: atomics, bounded retries
    BoardMeters readMeters() const noexcept;                 // atomics

    //==========================================================================
    // Audio thread, per processSlice, in this order
    // E65: measured Luthier callback load, not host-global or board-only load.
    void setProcessorLoad (double fraction, bool measuredWindow, double blockAudioSeconds) noexcept;
    void setAuxOutputs (const BoardAuxOutputs&) noexcept; // E54: borrowed, pre-cleared float buffers
    void setSidechain (const float* left, const float* right, int samples) noexcept; // audio-only borrowed block input
    void setExtendedHostAutomation (const ExtendedHostAutomationSnapshot&) noexcept;
    void setHostAutomation (const HostAutomationSnapshot&) noexcept; // E44: audio thread, before beginBlock
    void beginBlock (const juce::MidiBuffer& midi, const HostTransport& host, int numSamples) noexcept;
    void processStereo (double* left, double* right, int n, Placement p) noexcept;   // up to 3x per block; early-out BEFORE touching buffers when !isActive()
    int drainMidiOut (MidiEvent*, int capacity) noexcept; // E52: caller-owned fixed buffer; audio thread
    void endBlock (juce::MidiBuffer& midiOut) noexcept;      // meters, SPSC drain, RC-5 clock out
    int getLatencySamples() const noexcept;                  // L_board (0 when inactive)
    bool isActive() const noexcept;                          // enabled && >= 1 non-empty pedal

    //==========================================================================
    // Legacy shims (prototype API). Thin forwards; deleted after the tests migrate.
    void setBoardEnabled (bool) noexcept;
    bool isBoardEnabled() const noexcept;
    void setModel (int slot, Model) noexcept;
    Model getModel (int slot) const noexcept;
    void setSlotEnabled (int slot, bool) noexcept;
    bool isSlotEnabled (int slot) const noexcept;
    void setPlacement (int slot, Placement) noexcept;
    Placement getPlacement (int slot) const noexcept;
    void setKnob (int slot, int knob, double) noexcept;
    double getKnob (int slot, int knob) const noexcept;
    void move (int from, int to) noexcept;
    int getIdAtPosition (int position) const noexcept;
    static const char* modelName (Model model) noexcept { return legacyModelName (model); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace luthier::reference
