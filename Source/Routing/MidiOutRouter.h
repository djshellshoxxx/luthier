#pragma once

/*  MIDI output (routing-io 6).

    An instrument plugin's MidiBuffer is an in-out parameter: it arrives holding
    what the host sent and it leaves holding what the plugin sends. The engine
    reads it and does not clear it, so by the time the block is over the original
    contents are still there but are no longer what we want to emit.

    So the order is: snapshot the incoming buffer before the engine touches it,
    let the engine play, then rebuild the buffer from the sources the user turned
    on. That also makes pass-through exact - the echoed events are the original
    messages at their original timestamps, not a reconstruction.

    Real-time safety: every buffer here is sized once in prepare(). MidiBuffer
    keeps its capacity across clear(), so the steady state never allocates. The
    one case that could - a block carrying more MIDI than we reserved for - is
    handled by dropping the overflow and counting it, rather than by growing.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include "RoutingMatrix.h"

#include <array>
#include <atomic>

namespace luthier
{

//==============================================================================
/** One string starting or stopping ringing, at a known sample offset.

    The engine fills these as it fires scheduled events, so a strum spread over
    40 ms produces six events at six different offsets rather than six events
    stacked on sample zero. */
struct StringActivityEvent
{
    int  sampleOffset = 0;
    int  stringIndex = 0;
    int  midiNote = 0;
    float velocity = 0.0f;
    bool isNoteOn = true;

    /*  notation-export 6.1: a bass technique the engine resolved for a note
        (bass-techniques / string-slap-technique) rides the same queue, in the
        same block, so the capture's BASS_TECH track comes from the same place
        the note did. MIDI out skips these; only `note` records become notes. */
    enum class Kind : juce::uint8 { note = 0, bassTechnique };

    Kind kind = Kind::note;
    juce::uint8 code = 0;         ///< bassTechnique: a SlapType index
    juce::uint8 flags = 0;        ///< bassTechnique: bit 0 ghost, bit 1 the double thump's up-stroke
    float position = 0.5f;        ///< bassTechnique: the contact point, as a fraction of the vibrating length

    static constexpr juce::uint8 kGhost = 1;
    static constexpr juce::uint8 kRebound = 2;
};

//==============================================================================
/** A fixed-capacity, single-producer queue of string activity for one block. */
class StringActivityQueue
{
public:
    static constexpr int kCapacity = 128;

    void clear() noexcept { count = 0; dropped = 0; }

    void push (const StringActivityEvent& e) noexcept
    {
        if (count < kCapacity)
            events[(size_t) count++] = e;
        else
            ++dropped;
    }

    int size() const noexcept { return count; }
    int getDroppedCount() const noexcept { return dropped; }

    const StringActivityEvent& operator[] (int i) const noexcept
    {
        return events[(size_t) juce::jlimit (0, kCapacity - 1, i)];
    }

private:
    std::array<StringActivityEvent, kCapacity> events {};
    int count = 0;
    int dropped = 0;
};

//==============================================================================
class MidiOutRouter
{
public:
    MidiOutRouter();

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** Takes a copy of what the host sent, before the engine consumes it. */
    void captureInput (const juce::MidiBuffer& incoming) noexcept;

    /** The rhythm engine's generated events for this block. Cleared each block;
        empty until rhythm-engine.md is wired in. */
    juce::MidiBuffer& getRhythmBuffer() noexcept { return rhythm; }

    /** Macro values, 0..1, for the CC broadcast. Only a change is transmitted,
        so a static macro costs no bandwidth. */
    void setMacroValue (int macroIndex, float value) noexcept;

    /** Rebuilds `midiMessages` as the plugin's MIDI output. Call once, after the
        engine has run, at the end of processBlock. */
    void emit (juce::MidiBuffer& midiMessages,
               const MidiOutConfig& cfg,
               const StringActivityQueue& stringActivity,
               int numSamples) noexcept;

    /** Events that did not fit this block's reservation. Diagnostics only. */
    int getOverflowCount() const noexcept { return overflow.load (std::memory_order_relaxed); }

private:
    juce::MidiBuffer captured;
    juce::MidiBuffer rhythm;

    std::array<std::atomic<float>, 6> macroValues;
    std::array<float, 6> lastSentMacro {};

    std::atomic<int> overflow { 0 };

    double sr = 44100.0;
    int maxBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiOutRouter)
};

} // namespace luthier
