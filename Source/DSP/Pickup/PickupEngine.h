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

    /** Continuous blend between the two outermost active pickups, 0 to 1.
        Used by the Easy-mode blend knob. */
    void setBlend (double blend) noexcept;

    /** Guitar tone control: a passive treble roll-off, not a digital EQ. */
    void setToneControl (double amount) noexcept;   ///< 1 = wide open, 0 = fully rolled off.
    void setVolumeControl (double amount) noexcept;

    /** 60 Hz hum, present on single coils and cancelled by humbuckers. */
    void setHumAmount (double amount) noexcept;
    void setMainsFrequency (double hz) noexcept;

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

        Biquad tank;          ///< LCR resonance.
        Biquad magnetEq;      ///< Magnet character.
        double gain = 1.0;
        double positionOffset = 0.0;   ///< Fraction of string length from the slot centre.

        void reset() noexcept;
    };

    void updateCoil (int slot, int coilIndex) noexcept;
    void updateSelection() noexcept;
    static void applyMagnetEq (Biquad& eq, MagnetType m, double sr) noexcept;

    double combSample (Coil& coil, int stringIndex, double input, double delaySamples) noexcept;

    double sr = 44100.0;
    int numStrings = 6;
    int numPickups = 3;
    int activeCount = 1;

    std::array<PickupSpec, kMaxPickups> specs {};
    std::array<std::array<Coil, 2>, kMaxPickups> coils {};
    std::array<ExpSmoother, kMaxPickups> slotGain {};
    std::array<double, kMaxPickups> userVolume { { 1.0, 1.0, 1.0 } };

    PickupSelector selector = PickupSelector::Bridge;
    SwitchCrossfade selectorFade;

    // Guitar tone/volume: a passive RC roll-off whose corner moves with the pot.
    OnePoleLP toneFilter;
    ExpSmoother toneAmount, volumeAmount, blendAmount;

    // Piezo and internal mic paths.
    Biquad piezoHp, piezoLp, piezoRes;
    Biquad micTilt, micBody;
    ExpSmoother piezoMicBlend;

    // Mains hum.
    double humPhase = 0.0;
    double humIncrement = 0.0;
    ExpSmoother humLevel;

    DCBlocker outputDc;

    JUCE_LEAK_DETECTOR (PickupEngine)
};

} // namespace luthier
