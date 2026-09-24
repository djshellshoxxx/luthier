#pragma once

/*  The bass step grid (bass-techniques.md 9) - MODEL-GAPS workstream.

    "A step sequencer whose per-step type is a bass technique rather than a
    strum direction - thumb, pop, ghost, fingerstyle, dead. This is how bass
    patterns are actually written, and it is a different grid from the strum
    one rather than a relabelled version."

    A grid of up to 32 steps. Each step is a technique (or a rest), a level,
    and which note of the chord it plays: the root, the fifth or the octave.
    The rhythm engine plays it in place of the strum pattern when the loaded
    guitar is a bass and the grid has anything in it (gui-integration 4.4:
    "the bass step grid when the current guitar family is bass"); an empty
    grid leaves the strum pattern in charge, so nothing that worked before
    changes.

    The note's technique travels on NoteOnEvent::bassTechnique, which the
    slap engine honours (thumb, pop, ghost); fingerstyle notes go through
    the engine's alternation and rest stroke; a dead note is a muted pick.

    Pure data plus its (de)serialisation; message thread builds it, the audio
    thread reads a copy.
*/

#include <juce_core/juce_core.h>
#include <array>

#include "Patterns.h"

namespace luthier
{

/** Per-step technique. Saved as an index: append only. The values from thumb
    to ghost are what NoteOnEvent::bassTechnique and SlapEngine::classify read. */
enum class BassStepType
{
    rest = 0,
    thumb,
    pop,
    ghost,
    finger,
    dead,
    numTypes
};

const char* getBassStepTypeName (BassStepType t) noexcept;

/** Which chord tone a step plays. Saved as an index: append only. */
enum class BassStepNote
{
    root = 0,
    fifth,
    octave,
    numNotes
};

const char* getBassStepNoteName (BassStepNote n) noexcept;

struct BassStep
{
    BassStepType type = BassStepType::rest;
    double level = 0.8;                       ///< 0..1, the note's velocity
    BassStepNote note = BassStepNote::root;

    bool isRest() const noexcept { return type == BassStepType::rest; }

    bool operator== (const BassStep& o) const noexcept
    {
        return type == o.type && level == o.level && note == o.note;
    }
};

class BassStepGrid
{
public:
    static constexpr int kMaxSteps = 32;
    static constexpr int kDefaultSteps = 16;

    BassStepGrid() = default;

    int getLength() const noexcept { return length; }
    void setLength (int steps) noexcept { length = juce::jlimit (1, kMaxSteps, steps); }

    Subdivision getSubdivision() const noexcept { return subdivision; }
    void setSubdivision (Subdivision s) noexcept { subdivision = s; }

    /** How much of a step a note lasts, 0.1..1. */
    double getGate() const noexcept { return gate; }
    void setGate (double g) noexcept { gate = juce::jlimit (0.1, 1.0, g); }

    const BassStep& getStep (int index) const noexcept { return steps[(size_t) juce::jlimit (0, kMaxSteps - 1, index)]; }
    void setStep (int index, const BassStep& step) noexcept;

    /** The next type in the grid's click cycle: rest -> thumb -> pop -> ghost -> finger -> dead -> rest. */
    static BassStepType nextType (BassStepType t) noexcept;

    /** True when no step in the grid's length plays anything. */
    bool isEmpty() const noexcept;

    void clear() noexcept;

    juce::String getName() const { return name; }
    void setName (const juce::String& n) { name = n; }

    juce::var toVar() const;
    static BassStepGrid fromVar (const juce::var& v);

    bool operator== (const BassStepGrid& o) const noexcept;

    //==========================================================================
    /** factory-content.md: the bass kits' grids. */
    enum class Factory { slapFunk = 0, fingerstyleGroove, motownThumb, rootFifthWalk, numFactory };

    static BassStepGrid factory (Factory which);
    static const char* getFactoryName (Factory which) noexcept;

private:
    std::array<BassStep, kMaxSteps> steps {};
    int length = kDefaultSteps;
    Subdivision subdivision = Subdivision::sixteenth;
    double gate = 0.8;
    juce::String name;
};

} // namespace luthier
