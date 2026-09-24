#pragma once

/*  Stock and advanced parameter ranges (advanced-ranges.md).

    Every physical parameter has two ranges: the stock range, which is what
    the real object can do, and the advanced range, which is what the model
    can do. Stock is the default; advanced is opt-in per preset and is always
    marked rather than hidden.

    The mechanism is deliberately small. A `PhysicalRange` is a description,
    not a wrapper: it knows both pairs and can build the JUCE range for either
    mode. `RangeRegistry` maps a parameter id to one. `RangeState` holds which
    families a preset has unlocked and applies that to the APVTS.

    Sparse and additive by design (advanced-ranges.md 3.5). Only `amp` and
    `circuit` have parameters today; a parameter with no entry here is
    non-physical and keeps the single range it was declared with, and a family
    with no members is legal and reads as stock. Each later realism spec adds
    its rows to the table in the .cpp and nothing else changes.
*/

#include <juce_audio_processors/juce_audio_processors.h>

namespace luthier
{

//==============================================================================
/** The seven family keys `file-formats.md` fixes, plus "not physical". */
enum class RangeFamily
{
    amp = 0,
    circuit,
    squeak,
    buzz,
    pick,
    slide,
    modulation,
    strings,        // Phase 2b (DECISIONS "Phase 2b range families"): sustain-and-decay, tuning-stability
    numFamilies,

    /** Returned for a parameter that has no PhysicalRange. */
    none = numFamilies
};

const char* getRangeFamilyName (RangeFamily family) noexcept;

/** Parses a family key from the preset's `ranges.families` object. Returns
    `none` for an unknown key, which `advanced-ranges.md` 4 treats as absent. */
RangeFamily rangeFamilyFromName (const juce::String& name) noexcept;

//==============================================================================
/** One parameter's two ranges (advanced-ranges.md 1). */
struct PhysicalRange
{
    float stockMin = 0.0f, stockMax = 1.0f;
    float advancedMin = 0.0f, advancedMax = 1.0f;
    float defaultValue = 0.0f;

    /*  Skew centre as a fraction of the span, matching `floatParam`'s
        convention, or 1.0f for linear. Applied to whichever range is live so
        a knob keeps its feel when the range widens. */
    float skew = 1.0f;

    RangeFamily family = RangeFamily::none;

    /*  advancedMin <= stockMin < stockMax <= advancedMax, and the default
        inside stock. A failure here is a specification error rather than a
        runtime case, which is why the test sweeps the whole table. */
    bool isValid() const noexcept;

    juce::NormalisableRange<float> makeRange (bool advanced) const;

    bool contains (float value, bool advanced) const noexcept;

    /** True when this value is outside stock and therefore marked, whatever
        the mode is. Marking follows the value, not the mode
        (advanced-ranges.md 6.1). */
    bool isOutsideStock (float value) const noexcept;
};

//==============================================================================
/** The table. Static, built once, sparse. */
class RangeRegistry
{
public:
    /** The PhysicalRange for this parameter, or nullptr if it is not
        physical. */
    static const PhysicalRange* find (const juce::String& parameterId);

    /** Every parameter id in a family, in declaration order. */
    static juce::StringArray idsInFamily (RangeFamily family);

    /** Every parameter id that has a PhysicalRange. */
    static juce::StringArray allIds();

    //==========================================================================
    /*  The declaration check (advanced-ranges.md 1.0).

        `ParameterLayout` records the min and max each float parameter was
        actually declared with. The registry's stock pair has to agree, because
        presets store normalised values and a disagreement would silently
        re-map every preset ever saved.

        This is a recorded fact checked at runtime rather than a `jassert`,
        because the test suite is a Release build and an assertion that
        compiles away is an assertion that never runs. It is also why
        `floatParam` does not simply overwrite the declared range from the
        registry: doing that would make the check compare the registry with
        itself.
    */
    static void noteDeclaration (const juce::String& parameterId, float min, float max);

    /** Every id whose declaration disagrees with its registered stock pair.
        Empty is the only acceptable answer. */
    static juce::StringArray findDeclarationMismatches();
};

//==============================================================================
/*  Which families a preset has unlocked, and the per-control exceptions.

    This is preset state, not user state (advanced-ranges.md 0.4): it travels
    in the preset file so a preset sounds the same on another machine.
*/
class RangeState
{
public:
    RangeState() = default;

    bool isFamilyAdvanced (RangeFamily family) const noexcept;
    void setFamilyAdvanced (RangeFamily family, bool advanced);

    /** True if this parameter is live on its advanced range, whether because
        its family is unlocked or because it is individually unlocked. */
    bool isParameterAdvanced (const juce::String& parameterId) const;

    bool isUnlockedIndividually (const juce::String& parameterId) const;

    /*  There is no per-control *lock* (advanced-ranges.md 4). Unlocking a
        control in an already-advanced family is redundant and is dropped;
        "restrict to stock" removes it from the list and is only offered when
        it is in the list. */
    void setUnlockedIndividually (const juce::String& parameterId, bool unlocked);

    bool isAnythingAdvanced() const;

    void reset();

    //==========================================================================
    /*  Applies this state to the APVTS: for each registered parameter, swaps
        its live NormalisableRange to stock or advanced, preserving the plain
        value across the swap (advanced-ranges.md 1.2, 1.3).

        Returns the number of values that had to be clamped, which is non-zero
        only when narrowing. The caller reports it; a silent clamp is the
        failure ground rule 0.2 exists to prevent.

        Message thread, or the audio thread between blocks. Not concurrently
        with either.
    */
    int applyTo (juce::AudioProcessorValueTreeState& state) const;

    /** The values a narrowing `applyTo` would clamp, without applying it. */
    juce::StringArray findValuesOutsideStock (const juce::AudioProcessorValueTreeState& state) const;

    /*  Bumped by every `applyTo`, from whichever caller.

        A SliderAttachment copies its parameter's range once, when it is made,
        so a control attached before a range swap keeps drawing and dragging
        against the old one. The editor compares this against the last value
        it saw and re-attaches its controls when it moves. A counter rather
        than a broadcaster because the swap happens inside a preset load that
        knows nothing about windows. */
    static juce::uint32 getGeneration() noexcept;

    //==========================================================================
    juce::var toVar() const;

    /*  Reads the `ranges` block. A block that is absent or malformed triggers
        the legacy derivation in `advanced-ranges.md` 4.1: a family is advanced
        if and only if at least one stored value in it is outside stock, which
        preserves the sound of every preset saved before this existed while
        keeping the padlock honest.

        `storedValues` is the preset's `parameters` object, needed for that
        derivation. Pass an empty var when there is nothing to derive from, in
        which case every family reads as stock.
    */
    void fromVar (const juce::var& rangesBlock, const juce::var& storedValues);

    /*  The legacy derivation against live parameters, which is how a caller
        should actually do it (advanced-ranges.md 4.1).

        A preset file stores **normalised** values, and a normalised value
        carries no information about which range it was written against - it is
        always in 0-1 and so always inside whatever range is live. So the
        derivation cannot run on the file. It runs here, after the parameters
        have been set, on the plain values they actually hold.
    */
    void deriveFromCurrentValues (const juce::AudioProcessorValueTreeState& state);

private:
    std::array<bool, (size_t) RangeFamily::numFamilies> families {};
    juce::StringArray perControlUnlocks;
};

} // namespace luthier
