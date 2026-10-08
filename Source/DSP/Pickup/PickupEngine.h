#pragma once

/*  Pickup Engine (engine spec 7).

    A magnetic pickup does two separate things, and conflating them is the classic
    way to get this wrong:

      1. It samples the string's motion at one point along its length. That is a
         comb filter, and its delay depends on THAT STRING'S length - so it has to
         be applied per string, before the strings are summed. A pickup at 1/N of
         the string nulls the Nth harmonic, which is the whole reason a bridge
         pickup is bright and a neck pickup is warm.

      2. It is an inductor with parasitic resistance and capacitance. That is a
         second-order lowpass with a resonant peak, and it applies once, to the
         summed coil signal, because a real pickup has one coil for all six
         strings.

    Engine spec 1 draws the chain as strings -> body -> pickup. Doing the positional
    comb after summing would give every string the same comb, which is wrong, so
    the positional stage runs per string here and the electrical stage runs on the
    sum. The audible result is the spec's intent; the ordering is the physics.

    A humbucker is modelled as two of these coils at different positions, summed
    with opposite winding sense - the comb between them is what makes it sound
    like a humbucker rather than "a single-coil with extra bass" (pitfall 12).
*/

#include "../Common/DspCommon.h"
#include <array>
#include <vector>

namespace luthier
{

//==============================================================================
enum class PickupType
{
    SingleCoil, Humbucker, P90, Piezo, MagneticSoundhole, InternalMic,
    NumTypes
};

enum class MagnetType
{
    Alnico2, Alnico3, Alnico5, Ceramic, NumMagnets
};

//==============================================================================
/** One pickup's physical and electrical description. */
struct PickupSpec
{
    PickupType type          = PickupType::SingleCoil;
    MagnetType magnet        = MagnetType::Alnico5;

    double position          = 0.13;   ///< Fraction of string length from the bridge.
    double coilSpacingMm     = 17.5;   ///< Humbucker only.
    double resistanceKOhm    = 6.0;
    double inductanceHenries = 2.5;
    double capacitancePf     = 200.0;
    double heightMm          = 2.5;    ///< Distance from the strings.
    double outputTrimDb      = 0.0;

    /*  part-acoustics.md 6: a metal cover's eddy currents cost top end (nickel
        -0.8 dB at 4 kHz), and the pole pieces' do too (steel dulls, ceramic is
        brightest, as a multiplier on the top). */
    double coverLossDbAt4k   = 0.0;
    double poleBrightness    = 1.0;

    bool   coilTapped        = false;  ///< Humbucker split to one coil.
    bool   reverseWound      = false;

    /** Fills in electrically consistent defaults for a pickup type. */
    static PickupSpec makeDefault (PickupType t, double position);
};

//==============================================================================
/** The switch positions a guitar offers. */
enum class PickupSelector
{
    Bridge, BridgeMiddle, Middle, MiddleNeck, Neck, All, BridgeNeck,
    NumSelections
};

//==============================================================================
class PickupEngine
{
public:
    static constexpr int kMaxPickups = 3;

    void prepare (double sampleRate, int numStrings);
    void reset() noexcept;

    //==========================================================================
    void setNumStrings (int n) noexcept;
    void setNumPickups (int n) noexcept;
    int  getNumPickups() const noexcept { return numPickups; }

    void setPickupSpec (int slot, const PickupSpec& spec) noexcept;
    const PickupSpec& getPickupSpec (int slot) const noexcept;

    void setSelector (PickupSelector s) noexcept;
    PickupSelector getSelector() const noexcept { return selector; }

    /** Per-pickup volume, as on a Les Paul. */
    void setPickupVolume (int slot, double linearGain) noexcept;

    /** SPEC-SWEEP: CW-20 - one string's sensitivity in one pickup (the pole
        pieces are never quite level), as a linear gain. 1 is nominal. */
    void setStringBalance (int slot, int stringIndex, double linearGain) noexcept;

    /** Continuous blend between the two outermost active pickups, 0 to 1:
        0 is the bridge-side pickup alone, 1 the neck-side one alone, 0.5 both at
        full level (a centre-detent blend pot). No effect with one pickup on. */
    void setBlend (double blend) noexcept;

    /*  The coil the switch has selected, as the guitar's circuit sees it
        (volume-knob-interaction.md 4). Pickups switched in together are in
        parallel: inductances and resistances combine as parallel impedances,
        capacitances add. `hasCoil` is false when nothing inductive is selected.

        The tone and volume controls, and the coil's loaded resonance, belong to
        GuitarCircuit now. This engine produces the coils' EMF. */
    struct SelectedCoil
    {
        bool hasCoil = false;
        double inductance = 0.0;     ///< H
        double resistance = 0.0;     ///< ohm
        double capacitance = 0.0;    ///< F
    };

    SelectedCoil getSelectedCoil() const noexcept;

    /** 60 Hz hum, present on single coils and cancelled by humbuckers. */
    void setHumAmount (double amount) noexcept;
    void setMainsFrequency (double hz) noexcept;

    /*  noise-floor.md 2.1: the player's position and angle scale the hum, as a
        loop antenna in the room's field. 1 (the default) is the legacy path. */
    void setHumPositionGain (double g) noexcept { humPositionGain = g; }

    /** noise-floor.md 4.1: the share of the active magnetic signal that hears
        hum (single coils, P90s, soundhole pickups, a tapped humbucker). */
    double getSingleCoilShare() const noexcept;

    /** The hum this sample carried, for noise-floor.md 4.6's Aux 8 stem. */
    double getLastHumSample() const noexcept { return lastHum; }

    /** Identity rule 4: with every pickup off the instrument is silent and the UI
        must say so. */
    bool isSilent() const noexcept { return activeCount == 0; }

    //==========================================================================
    /** Per-string positional stage.

        @param stringOutputs  one sample per string, from the string engines
        @param delaySamples   each string's current loop length, for the comb
        @param numStrings     how many entries the arrays hold
        @returns              the summed coil signal after the electrical model
    */
    double processStrings (const double* stringOutputs,
                           const double* delaySamples,
                           int numStrings) noexcept;

    /*  string-interaction.md 5: pickup crosstalk. A pole senses its string
        through a Gaussian aperture; a bent string moves off its own pole and
        toward the next. `mmAtFret` is each string's lateral displacement where
        it is fretted, `stopFromSaddleMm` how far that point is from the saddle
        (the displacement falls linearly to the saddle), `bassward` whether it
        is pushed toward the bass side. Unbent strings are sensed with gain 1
        exactly; piezo and internal mic are not affected. Block rate. */
    void setStringLateralOffsets (const double* mmAtFret, const double* stopFromSaddleMm,
                                  const bool* bassward, int n, double scaleLengthMm,
                                  double stringSpacingMm, double apertureScale) noexcept;

    /** The aperture gain in use for a slot and string (1 unbent). */
    double getApertureGain (int slot, int stringIndex) const noexcept
    {
        return apertureGain[(size_t) juce::jlimit (0, kMaxPickups - 1, slot)]
                           [(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)];
    }

    /** The aperture sigma of a pickup type in mm, before the scale (5). */
    static double apertureSigmaMm (PickupType t) noexcept
    {
        return t == PickupType::Humbucker ? 5.0 : t == PickupType::P90 ? 5.5 : 4.0;
    }

    /** Piezo and internal-mic pickups tap the bridge and the body instead of the
        magnetic field, so they get their own inputs. */
    double processPiezo (double bridgeSignal) noexcept;
    double processInternalMic (double bodySignal) noexcept;

    /** Blend between the magnetic/piezo path and the internal mic, 0 to 1. */
    void setPiezoMicBlend (double blend) noexcept;
    double getPiezoMicBlend() const noexcept { return piezoMicBlend.getTarget(); }

    //==========================================================================
    /** Resonant peak of a slot, in Hz. Exposed for the UI and the tests. */
    double getResonantFrequency (int slot) const noexcept;
    double getResonantQ (int slot) const noexcept;

private:
    struct Coil
    {
        /** Per-string comb delay lines: one short buffer per string per coil. */
        std::array<std::vector<double>, kMaxStrings> history;
        std::array<int, kMaxStrings> writeIndex {};

        Biquad magnetEq;      ///< Magnet character.
        Biquad coverEq;       ///< Cover and pole-piece eddy losses.
        double gain = 1.0;
        double positionOffset = 0.0;   ///< Fraction of string length from the slot centre.

        void reset() noexcept;
    };

    void updateCoil (int slot, int coilIndex) noexcept;
    void updateSelection() noexcept;
    static void applyMagnetEq (Biquad& eq, MagnetType m, double sr) noexcept;

    double combSample (Coil& coil, int stringIndex, double input, double delaySamples) noexcept;
    static void writeHistory (Coil& coil, int stringIndex, double input) noexcept;   // SPEC-SWEEP: EN-50

    double sr = 44100.0;
    int numStrings = 6;
    int numPickups = 3;
    int activeCount = 1;

    std::array<PickupSpec, kMaxPickups> specs {};
    std::array<std::array<Coil, 2>, kMaxPickups> coils {};
    std::array<ExpSmoother, kMaxPickups> slotGain {};

    // string-interaction.md 5: 1 unless a string is bent.
    std::array<std::array<double, kMaxStrings>, kMaxPickups> apertureGain = [] {
        std::array<std::array<double, kMaxStrings>, kMaxPickups> a {};
        for (auto& row : a) row.fill (1.0);
        return a; }();
    bool anyApertureGain = false;
    std::array<double, kMaxPickups> userVolume { { 1.0, 1.0, 1.0 } };
    // SPEC-SWEEP: CW-20 - stored as (gain - 1) so zero-initialised is nominal.
    std::array<std::array<double, kMaxStrings>, kMaxPickups> stringBalanceDelta {};

    PickupSelector selector = PickupSelector::Bridge;
    SwitchCrossfade selectorFade;

    ExpSmoother blendAmount;

    /** Which slots the selector has switched in, for getSelectedCoil. */
    std::array<bool, kMaxPickups> slotOn { { true, false, false } };

    // Piezo and internal mic paths.
    Biquad piezoHp, piezoLp, piezoRes;
    Biquad micTilt, micBody;
    ExpSmoother piezoMicBlend;

    // Mains hum.
    double humPhase = 0.0;
    double humIncrement = 0.0;
    ExpSmoother humLevel;
    double humPositionGain = 1.0, lastHum = 0.0;   // noise-floor.md 2.1

    DCBlocker outputDc;

    JUCE_LEAK_DETECTOR (PickupEngine)
};

} // namespace luthier
