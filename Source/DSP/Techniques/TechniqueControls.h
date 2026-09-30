#pragma once

/*  The controllers the techniques read (microtonal-bends.md 1-2,
    slide-technique-controls.md 1).

    Bends and the slide bar are driven by whichever controller the player
    picks: pitch bend, the mod wheel, expression, aftertouch, MPE's Y (CC 74)
    and Z (channel pressure) per channel, any CC, or a drag on the fretboard.
    This holds the latest value of each, read once per block from the MIDI,
    so every technique that listens to one reads the same thing. Values are
    0-1 (unipolar) or -1..1 (pitch bend and the bipolar reading of a CC).

    Audio thread for processMidi and the reads; the on-screen values are
    atomics any thread can write.
*/

#include "../Common/DspCommon.h"
#include <array>
#include <atomic>

namespace luthier
{

/** A control source, as the technique choice lists name them. Append only. */
enum class ControlSource
{
    none = 0,
    modWheel,
    pitchBend,
    mpeY,
    expression,
    customCc,
    fretboard,
    aftertouch,
    mpeZ,
    numSources
};

class TechniqueControls
{
public:
    TechniqueControls() { reset(); }

    void reset() noexcept;

    /** Reads a block's controllers. Only the last value of each matters. */
    void processMidi (const juce::MidiBuffer& midi) noexcept;

    /** -1..1; channel 1-16, or 0 for the last pitch bend on any channel. */
    double pitchBend (int channel = 0) const noexcept;

    /** 0-1; channel 0 for the last value on any channel. */
    double cc (int number, int channel = 0) const noexcept;

    /** 0-1 channel pressure; channel 0 for the last on any. */
    double pressure (int channel = 0) const noexcept;

    /*  A source's value, 0-1 (`bipolar` false) or -1..1. Unipolar controllers
        read bipolar about their centre (64); pitch bend reads unipolar from its
        centre up. `channel` 0 is "any"; MPE sources want the note's own. */
    double read (ControlSource source, int ccNumber, int channel, bool bipolar) const noexcept;

    /** Whether the source has sent anything since reset (a source that has not moved reads its rest value). */
    bool hasValue (ControlSource source, int ccNumber, int channel) const noexcept;

    //==========================================================================
    // The fretboard's drags (any thread). -1 means not dragging.
    void setFretboardSlidePosition (double normalised) noexcept { fretboardSlide.store (normalised); }
    double getFretboardSlidePosition() const noexcept { return fretboardSlide.load(); }

    void setFretboardBend (double bipolar) noexcept { fretboardBend.store (juce::jlimit (-1.0, 1.0, bipolar)); }
    double getFretboardBend() const noexcept { return fretboardBend.load(); }

private:
    std::array<std::array<float, 128>, 17> ccs {};      // [0] = any channel
    std::array<std::array<bool, 128>, 17> ccSeen {};
    std::array<float, 17> bends {};
    std::array<bool, 17> bendSeen {};
    std::array<float, 17> pressures {};
    std::array<bool, 17> pressureSeen {};

    std::atomic<double> fretboardSlide { -1.0 };
    std::atomic<double> fretboardBend { 0.0 };
};

} // namespace luthier
