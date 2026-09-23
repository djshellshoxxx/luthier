#pragma once

/*  Tuning Engine (engine spec 3).

    Turns (string, fret position, bend) into a target frequency in Hz, taking in
    temperament, per-string detune, intonation error and slow tuning drift.

    Fret position is a double throughout. Nothing here ever quantises to a
    semitone, which is what makes fretless mode and continuous bends possible
    (pitfall 13).
*/

#include "../../DSP/Common/DspCommon.h"
#include <array>

namespace luthier
{

//==============================================================================
enum class Temperament
{
    EqualTemp12,
    JustIntonation,
    Meantone,
    Werckmeister3,
    Kirnberger3,
    Pythagorean,
    Custom,
    NumTemperaments
};

//==============================================================================
enum class TuningPreset
{
    Standard,
    DropD,
    DropC,
    DropB,
    DADGAD,
    OpenG,
    OpenD,
    OpenE,
    OpenC,
    HalfStepDown,
    FullStepDown,
    Nashville,
    SevenString,
    EightString,
    BaritoneB,
    BassStandard,
    BassFiveString,
    Custom,
    NumPresets
};

//==============================================================================
class TuningEngine
{
public:
    struct StringTuning
    {
        double openFrequencyHz    = 110.0;
        double detuneCents        = 0.0;   ///< Deliberate offset, -100..+100.
        double realismDetuneCents = 0.0;   ///< Randomised imperfection, persisted in the preset.
        double driftCents         = 0.0;   ///< Slow drift while playing.

        /*  The character engine's own tuner drift (character-wear 4).

            Kept separate from driftCents rather than sharing it: that one is a
            bounded random walk driven by the Drift toggle, and this one is a
            deterministic per-string LFO derived from the instrument's seed. They
            are different models of different things - a string settling versus a
            machine head slipping - and either writing into the other's field
            would silently cancel it. */
        double characterDriftCents = 0.0;
        double intonationSlope    = 0.30;  ///< Cents of sharpening per fret.
        double fineTuneCents      = 0.0;   ///< Per-string fine tuner.
        int    maxFrets           = 24;
    };

    TuningEngine();

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    //==========================================================================
    void setNumStrings (int n) noexcept;
    int  getNumStrings() const noexcept { return numStrings; }

    void setTuningPreset (TuningPreset preset) noexcept;
    TuningPreset getTuningPreset() const noexcept { return currentPreset; }

    void setStringTuning (int stringIndex, const StringTuning& t) noexcept;
    const StringTuning& getStringTuning (int stringIndex) const noexcept;

    void setOpenFrequency (int stringIndex, double hz) noexcept;
    void setDetuneCents (int stringIndex, double cents) noexcept;
    void setFineTuneCents (int stringIndex, double cents) noexcept;

    /** The character engine's tuner drift, in cents (character-wear 4). */
    void setCharacterDriftCents (int stringIndex, double cents) noexcept;
    void setIntonationSlope (int stringIndex, double centsPerFret) noexcept;
    void setMaxFrets (int stringIndex, int frets) noexcept;

    void setTemperament (Temperament t) noexcept { temperament = t; }
    Temperament getTemperament() const noexcept { return temperament; }

    /** Twelve ratios for Temperament::Custom, relative to the tonic. */
    void setCustomTemperament (const std::array<double, 12>& ratios) noexcept;
    const std::array<double, 12>& getCustomTemperament() const noexcept { return customRatios; }

    /** Which pitch class the non-equal temperaments are centred on. */
    void setTemperamentRoot (int pitchClass) noexcept { temperamentRoot = ((pitchClass % 12) + 12) % 12; }
    int getTemperamentRoot() const noexcept { return temperamentRoot; }

    /** Global reference pitch. */
    void setConcertA (double hz) noexcept;
    double getConcertA() const noexcept { return concertA; }

    //==========================================================================
    /*  The capo (ambiguity-resolutions.md 4.5).

        "Capo raises effective minimum fret to capo_fret. Open strings are the
        capo'd notes." Three consequences, and they are the whole feature:

          - getEffectiveOpenFrequency returns the capo'd note, so everything that
            asks what a string sounds like open - the tuner, the string list, the
            headstock popover, the engine setting up the string - gets the right
            answer without having to know a capo exists.
          - fret positions are measured *from the capo*, so fret 0 is the capo.
          - the neck gets shorter: getHighestPlayableFret is maxFrets - capoFret.

        A capo at 0 is no capo, and every path here reduces to the uncapo'd one.

        The capo is applied as a fret position rather than as a cent offset on the
        open string, which matters under an unequal temperament: the frets are at
        fixed places, so a capo at 5 gives exactly the pitch fret 5 gives, not the
        open string shifted by a tempered fourth. Under equal temperament the two
        are identical, which is why it would have been easy to get wrong.

        A partial capo (4.5) clamps only the strings in its mask, bit n for
        string n (0 = the lowest); the others stay open to the nut and every
        path above answers for them as if there were no capo. */
    void setCapoFret (int fret) noexcept;
    int getCapoFret() const noexcept { return capoFret; }

    static constexpr juce::uint32 kAllStrings = 0xFFFFFFFFu;
    void setCapoStringMask (juce::uint32 mask) noexcept { capoMask = mask; }
    juce::uint32 getCapoStringMask() const noexcept     { return capoMask; }

    /** The capo fret this string sees: capoFret if the capo clamps it, else 0. */
    int getCapoFretFor (int stringIndex) const noexcept
    {
        const bool clamped = stringIndex >= 0 && stringIndex < 32
                          && ((getCapoStringMask() >> stringIndex) & 1u) != 0;
        return clamped ? capoFret : 0;
    }

    /** The highest fret still reachable, counting from the capo. Zero when a capo
        has been put past the end of the neck, which is legal and silly. */
    int getHighestPlayableFret (int stringIndex) const noexcept;

    //==========================================================================
    /** Re-randomises every string's realism detune within +/- `maxCents`.
        Called on preset load and from the "retune" button. Persisted so a preset
        sounds the same every time it is opened. */
    void randomiseRealismDetune (double maxCents, uint64_t seed) noexcept;

    /** Enables the slow drift that models a guitar going out of tune. */
    void setDriftEnabled (bool enabled, double maxCents = 5.0) noexcept;
    bool isDriftEnabled() const noexcept { return driftEnabled; }

    /** Advances drift. Call once per block with the block's length in samples. */
    void advanceDrift (int numSamples) noexcept;

    //==========================================================================
    /** The core conversion.

        @param stringIndex   which string
        @param fretPosition  0.0 = open, fractional values allowed
        @param bendCents     extra offset in cents (bend, vibrato, whammy)
        @returns             target frequency in Hz
    */
    double computeFrequency (int stringIndex, double fretPosition, double bendCents = 0.0) const noexcept;

    /** The open pitch including detune, drift and fine tuning but no fret. */
    double getEffectiveOpenFrequency (int stringIndex) const noexcept;

    /** Inverse of computeFrequency: the fret position that would produce `hz`.
        Returns a negative value if the pitch is below the open string. */
    double frequencyToFretPosition (int stringIndex, double hz) const noexcept;

    /** True if `hz` can be played on this string within its fret range. */
    bool canPlay (int stringIndex, double hz) const noexcept;

    //==========================================================================
    /** Human-readable note name for a frequency, e.g. "A2 +3c". */
    static juce::String describeFrequency (double hz, double concertAHz = 440.0);

    /** Note name for a pitch class + octave, e.g. "E2". */
    static juce::String noteName (int midiNote);

    /** Parses "E2", "Eb3", "F#1" into a MIDI note number. Returns -1 on failure. */
    static int parseNoteName (const juce::String& name);

    static const char* getTuningPresetName (TuningPreset p) noexcept;
    static const char* getTemperamentName (Temperament t) noexcept;

    /** Default open frequencies for a preset, in Hz, string 0 = highest. */
    static int getPresetStringCount (TuningPreset p) noexcept;
    static void getPresetFrequencies (TuningPreset p, double* dest, int maxCount) noexcept;

private:
    double temperamentRatio (double semitonesFromRoot) const noexcept;

    /** The open string with no capo on it: detune, drift and fine tuning, and
        nothing else. The capo is a fret, so it belongs on the fret side. */
    double getOpenFrequencyBeforeCapo (int stringIndex) const noexcept;

    int numStrings = 6;
    int capoFret = 0;
    juce::uint32 capoMask = kAllStrings;
    std::array<StringTuning, kMaxStrings> strings {};

    Temperament temperament = Temperament::EqualTemp12;
    std::array<double, 12> customRatios {};
    int temperamentRoot = 4;   // E, the usual guitar tonic

    double concertA = 440.0;
    double sr = 44100.0;

    TuningPreset currentPreset = TuningPreset::Standard;

    bool   driftEnabled = false;
    double driftMaxCents = 5.0;
    int    driftCounter = 0;
    int    driftIntervalSamples = 44100 * 30;
    std::array<double, kMaxStrings> driftTargets {};
    RtRandom driftRng { 0xD817F7Aull };
};

} // namespace luthier
