#pragma once

/*  Character and wear (character-wear.md).

    The imperfections a real guitar has and a modelled one usually does not:
    dead spots, worn frets, tuners that will not stay put, pots and capacitors
    that have drifted from their marked values, pickups that are not quite level.

    Rule 1 of section 0 is what the whole design turns on: everything here is
    deterministic from one 64-bit seed. Two presets of the same guitar with
    different seeds differ the way two instruments off the same line differ, and
    the same seed produces the same instrument every time it is loaded. That
    means no calls to a global random number generator, and no dependence on the
    order in which anything is constructed - every value is derived from the seed
    and an index, so asking for string 3's dead spots gives the same answer
    whether or not string 2 was asked for first.

    Rule 2 is the other one: none of this modulates per sample. Dead spots and
    fret wear are looked up per note; tuner drift moves over tens of seconds;
    body break-in is applied to coefficients when a preset loads. The only thing
    that runs continuously is the drift, and it runs at control rate.
*/

#include "../DSP/Common/DspCommon.h"

#include <array>
#include <atomic>
#include <vector>

namespace luthier
{

//==============================================================================
/** One dead spot on one string (character-wear 2). */
struct DeadSpot
{
    int fret = 7;
    double depth = 0.3;      ///< 0.1 to 0.7: how much sustain is lost.
    double width = 3.0;      ///< 2 to 5 frets.

    /** How strongly this spot affects a fret, as a Gaussian around its centre. */
    double weightAt (double fretPosition) const noexcept;
};

//==============================================================================
/** Temperature and humidity (character-wear 9). */
enum class Temperature { cold = 0, room, warm, numTemperatures };
enum class Humidity { dry = 0, normal, humid, numHumidities };

const char* getTemperatureName (Temperature t) noexcept;
const char* getHumidityName (Humidity h) noexcept;

//==============================================================================
class CharacterEngine
{
public:
    static constexpr int kMaxFrets = 24;
    static constexpr int kMaxDeadSpotsPerString = 3;

    CharacterEngine();

    void prepare (double sampleRate, int numStrings) noexcept;
    void reset() noexcept;

    void setNumStrings (int n) noexcept;

    /*  string-aging.md 3.1: a per-instance value for another module, from the
        seed. `category` must be one no value of this class uses; the ones
        taken by other specs are listed here. */
    static constexpr int kCategoryStringAge = 40;   // REALISM-A
    double hashedValue (int category, int a, int b = 0) const noexcept { return hashed (category, a, b); }

    //==========================================================================
    // The seed (character-wear 1).

    /** Sets the instance seed and regenerates everything derived from it. */
    void setSeed (uint64_t seed);
    uint64_t getSeed() const noexcept { return seed; }

    /** character-wear 1: a new instrument, saved into the preset. */
    void reroll();

    //==========================================================================
    /** Master intensity. character-wear 0.3: on by default at low intensity, so
        zero is a deliberate choice rather than the ship state. */
    void setAmount (double amount) noexcept;
    double getAmount() const noexcept { return amount.load (std::memory_order_relaxed); }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled.store (shouldBeEnabled, std::memory_order_relaxed); }
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    //==========================================================================
    // Dead spots (character-wear 2).

    int getNumDeadSpots (int stringIndex) const noexcept;
    DeadSpot getDeadSpot (int stringIndex, int index) const noexcept;

    void setDeadSpot (int stringIndex, int index, const DeadSpot& spot);

    /** The loop-gain multiplier for a note, 0 to 1. One means no loss.

        character-wear 2 also asks for the attenuation to interact with the body:
        a dead spot is neck-body coupling, so `bodyResonanceHz` biases how much
        is lost - a note near the body's air resonance loses more. */
    double getSustainMultiplier (int stringIndex, double fretPosition,
                                 double noteHz = 0.0, double bodyResonanceHz = 0.0) const noexcept;

    //==========================================================================
    // Fret wear (character-wear 3).

    double getFretWear (int fret) const noexcept;
    void setFretWear (int fret, double wear);

    /** character-wear 3: a refret, back to new. */
    void refret();

    /** The detune a worn fret adds, in cents. */
    double getFretDetuneCents (double fretPosition) const noexcept;

    /** How much more likely a buzz is at a worn fret, as a multiplier. */
    double getFretBuzzMultiplier (double fretPosition) const noexcept;

    /** Sustain lost to imperfect fret contact. */
    double getFretSustainMultiplier (double fretPosition) const noexcept;

    //==========================================================================
    // Tuner drift (character-wear 4).

    void setTunerLooseness (double percent) noexcept;
    double getTunerLooseness() const noexcept { return tunerLooseness.load (std::memory_order_relaxed); }

    /** Advances the drift. Called once per block with the block's duration. */
    void advance (double secondsElapsed) noexcept;

    /** The drift on a string right now, in cents. */
    double getTunerDriftCents (int stringIndex) const noexcept;

    /** character-wear 9: back to zero, and drifting again from there. */
    void retune() noexcept;

    /** tuning-stability.md 3: Retune string n clears that string's drift only. */
    void retuneString (int stringIndex) noexcept;

    /** How long the instrument has been "played" since the last retune. */
    double getSessionSeconds() const noexcept { return sessionSeconds; }

    //==========================================================================
    // Aged electronics (character-wear 5).

    void setPotLinearityAmount (double amount) noexcept;
    double getPotLinearityAmount() const noexcept { return potLinearity.load (std::memory_order_relaxed); }

    /** Maps a nominal pot position through the aged taper. */
    double applyPotTaper (double position) const noexcept;

    /** The tone capacitor's actual value, as a multiplier on nominal. */
    double getCapacitorDrift() const noexcept { return capacitorDrift; }

    void setCapacitorDriftRange (double fraction) noexcept;

    /** character-wear 5: off by default, because it will surprise people. */
    void setJackIntermittentEnabled (bool shouldBeEnabled) noexcept
    {
        jackIntermittent.store (shouldBeEnabled, std::memory_order_relaxed);
    }

    bool isJackIntermittentEnabled() const noexcept
    {
        return jackIntermittent.load (std::memory_order_relaxed);
    }

    /** The jack's gain right now: 1 normally, dipping during a dropout. */
    double getJackGain() const noexcept { return jackGain; }

    //==========================================================================
    // Balance (character-wear 5, 6 and 7).

    /** Per-string, per-pickup output trim in dB. */
    double getPickupBalanceDb (int stringIndex, int pickupSlot) const noexcept;

    /** Per-saddle piezo balance in dB. */
    double getSaddleBalanceDb (int stringIndex) const noexcept;

    /** character-wear 7: nut slot wear dampens the open string. */
    double getNutDamping (int stringIndex) const noexcept;

    /** character-wear 7: saddle height variation, in millimetres. */
    double getSaddleHeightOffsetMm (int stringIndex) const noexcept;

    void setBoneNut (bool bone) noexcept { boneNut.store (bone, std::memory_order_relaxed); }
    bool isBoneNut() const noexcept { return boneNut.load (std::memory_order_relaxed); }

    /** The high-frequency damping the nut material implies, as a multiplier. */
    double getNutMaterialDamping() const noexcept;

    //==========================================================================
    // Body break-in (character-wear 8).

    void setBodyAge (double age) noexcept;
    double getBodyAge() const noexcept { return bodyAge.load (std::memory_order_relaxed); }

    /** Multipliers the body engine applies to its coefficients on preset load. */
    double getBodyQMultiplier() const noexcept;
    double getAirResonanceMultiplier() const noexcept;
    double getBodyHfDampingMultiplier() const noexcept;

    //==========================================================================
    /*  Environment (character-wear 9) is environment.md's EnvironmentModel now:
        a physical, lagged model that is state rather than wear. What is left
        here is the legacy three-step choice, read from presets saved before it
        (environment.md 6) so the loader can convert it; it no longer reaches
        the audio and is no longer saved. */

    void setTemperature (Temperature t) noexcept;
    Temperature getTemperature() const noexcept { return (Temperature) temperature.load (std::memory_order_relaxed); }

    void setHumidity (Humidity h) noexcept;
    Humidity getHumidity() const noexcept { return (Humidity) humidity.load (std::memory_order_relaxed); }

    /** environment.md 6: the old model's offset for the legacy choice,
        +-2.5 cents x amount, which the loader converts to a temperature. */
    static double legacyTemperatureOffsetCents (Temperature t, double amount) noexcept
    {
        return t == Temperature::cold ? 2.5 * amount : (t == Temperature::warm ? -2.5 * amount : 0.0);
    }

    //==========================================================================
    // Presets (character-wear 10).

    /** Everything to zero: a machine-perfect instrument. */
    void setAllFresh();

    /** Everything near maximum: a well-used one. */
    void setAllOld();

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& state);

private:
    /** Regenerates every per-instance value from the seed. */
    void generate();

    /** A deterministic value in 0..1 from the seed and a pair of indices.

        This is what makes rule 1 work. Rather than running one generator through
        every value in order - which would make string 3's dead spots depend on
        how many the earlier strings happened to get - each value is hashed
        independently from the seed and its own coordinates. */
    double hashed (int category, int a, int b = 0) const noexcept;

    uint64_t seed = 0x5EEDC0DEull;

    int numStrings = 6;
    double sr = 44100.0;

    std::atomic<bool> enabled { true };
    std::atomic<double> amount { 0.25 };      ///< character-wear 0.3: low by default.

    // --- dead spots -----------------------------------------------------------------
    std::array<std::array<DeadSpot, kMaxDeadSpotsPerString>, kMaxStrings> deadSpots {};
    std::array<int, kMaxStrings> numDeadSpots {};

    // --- fret wear -------------------------------------------------------------------
    std::array<double, kMaxFrets + 1> fretWear {};

    // --- tuner drift ------------------------------------------------------------------
    std::atomic<double> tunerLooseness { 15.0 };
    std::array<double, kMaxStrings> driftPhase {};
    std::array<double, kMaxStrings> driftRate {};
    std::array<double, kMaxStrings> driftAmplitude {};
    std::array<double, kMaxStrings> driftCents {};

    double sessionSeconds = 0.0;

    // --- electronics -------------------------------------------------------------------
    std::atomic<double> potLinearity { 0.35 };
    double capacitorDrift = 1.0;
    double capacitorDriftRange = 0.05;

    std::atomic<bool> jackIntermittent { false };
    double jackGain = 1.0;
    double jackDropoutRemaining = 0.0;
    double jackSecondsToNext = 60.0;

    // --- balance ---------------------------------------------------------------------
    std::array<std::array<double, 3>, kMaxStrings> pickupBalanceDb {};
    std::array<double, kMaxStrings> saddleBalanceDb {};
    std::array<double, kMaxStrings> nutDamping {};
    std::array<double, kMaxStrings> saddleHeightMm {};

    std::atomic<bool> boneNut { true };

    // --- body -----------------------------------------------------------------------
    std::atomic<double> bodyAge { 20.0 };

    // --- environment -------------------------------------------------------------------
    std::atomic<int> temperature { (int) Temperature::room };
    std::atomic<int> humidity { (int) Humidity::normal };

    /** The drift's own generator, seeded from the instance seed, so the
        intermittent jack is deterministic too. */
    RtRandom rng { 0x5EEDC0DEull };

    JUCE_LEAK_DETECTOR (CharacterEngine)
};

} // namespace luthier
