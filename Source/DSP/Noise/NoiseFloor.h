#pragma once

/*  The rig's steady noise sources (noise-floor.md).

    A drone, not an event: these run continuously, so they are not in the
    NoiseEngine pool (whose rule is "nothing here drones"). Each is injected
    where it enters a real chain (2), so the volume knob, the pickup type and
    the amp gain act on it with no special case:

        strings -> pickups (+hum, +fluorescent) -> [+passive hiss] -> GuitarCircuit
                -> [+cable movement] -> DI (Aux 1) -> pre-FX
                -> [+ground loop, +radio, +amp hiss, +microphonics] -> AmpEngine

    The single-coil hum itself stays in PickupEngine (1); this module only
    supplies its position gain. Every source here defaults to zero, and with
    all of them at zero isIdle() is true and the engine skips the module, so a
    preset that never touches them renders bit-identically (NF-01).

    beginBlock renders the whole block of every source into four tap buffers,
    because every input it needs is known at the start of the block: the
    microphonic path reads the previous block's amp output, one block late,
    exactly as FeedbackLoop's air path does.
*/

#include "../Common/DspCommon.h"
#include "../Circuit/GuitarCircuit.h"
#include <array>
#include <atomic>
#include <vector>

namespace luthier
{

//==============================================================================
struct NoiseFloorSettings
{
    double mainsHz = 60.0;               ///< noise_mains_hz: 60 or 50
    double angleDegrees = 0.0;           ///< noise_player_angle
    double distanceMetres = 1.0;         ///< noise_player_distance

    double fluorescent = 0.0;            ///< 2.2
    double passiveHiss = 0.0;            ///< 2.3, 1 = physical
    double cableMovement = 0.0;          ///< 2.4
    double radio = 0.0;                  ///< 2.6
    double groundLoop = 0.0;             ///< 2.5
    double ampHiss = 0.0;                ///< 2.7
    double microphonics = 0.0;           ///< 2.8

    bool toAux8 = false;                 ///< 4.6: an identification stem on Aux 8

    bool anySourceOn() const noexcept
    {
        return fluorescent > 0.0 || passiveHiss > 0.0 || cableMovement > 0.0 || radio > 0.0
            || groundLoop > 0.0 || ampHiss > 0.0 || microphonics > 0.0;
    }
};

//==============================================================================
class NoiseFloor
{
public:
    NoiseFloor() = default;

    //==========================================================================
    /*  The reference pluck (0.4): open low E, velocity 100, bridge single coil,
        volume and tone at 10, 3 m standard cable. This is its measured peak at
        Aux 1 in engine units (NoiseFloorTests logs the live value against it),
        and every target below is stated relative to it. */
    static constexpr double kReferencePluckPeak = 1.08;

    /** 2.3: a real pickup's reference pluck is about 0.3 V at the EMF. */
    static constexpr double kEmfVoltsPerUnit = 0.3 / kReferencePluckPeak;

    /** Boltzmann's constant and the room temperature the hiss is quoted at. */
    static constexpr double kBoltzmann = 1.380649e-23;
    static constexpr double kRoomKelvin = 300.0;
    static constexpr double kNoiseBandwidthHz = 20000.0;

    /** 2.1: g_pos = g_angle x g_dist. Exactly 1 at (0 deg, 1 m). */
    static double positionGain (double angleDegrees, double distanceMetres) noexcept;

    /** 2.3: sqrt (4 k T R B), in volts RMS, over B (20 kHz by default). */
    static double johnsonVoltsRms (double ohms, double bandwidthHz = kNoiseBandwidthHz) noexcept;

    /** 2.3: the resistance the hiss sees - the coil plus the volume pot's
        wiper-to-ground section. */
    static double hissResistance (const CircuitComponents& parts) noexcept;

    //==========================================================================
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** The character seed (character-wear.md 1). Rebuilds the ground-loop
        wavetable when it changes. Allocation-free. */
    void setSeed (uint64_t seed) noexcept;

    void setSettings (const NoiseFloorSettings& s) noexcept { settings = s; }
    const NoiseFloorSettings& getSettings() const noexcept { return settings; }

    /** 0.5: every new source at zero and nothing still sounding. */
    bool isIdle() const noexcept;

    /*  Renders the block. `singleCoilShare` is the magnetic share that hears
        hum (PickupEngine::getSingleCoilShare); `parts` the live circuit;
        `anyRinging` whether a string sounds (the cable's weight-shift rolls);
        `separateHead` whether the cabinet implies a head off the cab (2.8). */
    void beginBlock (int numSamples, double singleCoilShare, const CircuitComponents& parts,
                     bool anyRinging, bool separateHead) noexcept;

    // --- the four taps (4), valid for the block beginBlock rendered ----------
    double pickupSample (int i) const noexcept   { return pickupBuf[(size_t) i]; }
    double circuitInSample (int i) const noexcept{ return circuitInBuf[(size_t) i]; }
    double diSample (int i) const noexcept       { return diBuf[(size_t) i]; }
    double ampInSample (int i) const noexcept    { return ampInBuf[(size_t) i]; }

    /** The dry sum of every new source, for the Aux 8 stem (4.6). */
    double stemSample (int i) const noexcept
    {
        return pickupBuf[(size_t) i] + circuitInBuf[(size_t) i] + diBuf[(size_t) i] + ampInBuf[(size_t) i];
    }

    /** 2.4: a note-on rolls for a cable event. */
    void onNoteOn (double velocity) noexcept;

    /*  2.8: what actually entered the amp this block (the guitar plus this
        module's amp-input sources) and what came out of it. The output is the
        microphonic path's input next block; the pair measures the amp's gain
        at the resonance, so the loop gain is what the parameter says. */
    void recordAmpInput (int i, double x) noexcept { if (i < (int) ampTotalIn.size()) ampTotalIn[(size_t) i] = x; }
    void pushAmpOutput (const double* data, int numSamples) noexcept;

    /** What entered the amp this block (valid while not idle), for the tests. */
    const double* getAmpInputRecord() const noexcept { return ampTotalIn.data(); }

    // --- state for the UI and the tests ---------------------------------------
    double getPositionGain() const noexcept { return posGain; }

    /** Summed noise-floor RMS in dB re the reference-pluck peak, last block. */
    double getMeterDb() const noexcept { return meterDb.load (std::memory_order_relaxed); }

    /** Blocks the meter was updated in; it stops moving while idle. */
    juce::uint32 getMeterUpdates() const noexcept { return meterUpdates.load (std::memory_order_relaxed); }

    /** Monotonic count of cable events started, for the UI and NF-10. */
    int getCableEventCount() const noexcept { return cableEvents; }

    /** The microphonic resonator's frequency and Q for this instance (2.8). */
    double getMicrophonicHz() const noexcept { return micHz; }
    double getMicrophonicQ() const noexcept  { return micQ; }

    /** The amp's measured gain at the microphonic resonance (2.8). */
    double getMicrophonicAmpGain() const noexcept { return micAmpGain; }

    /** The hum's RMS in engine units at a level and share, for the meter. */
    void setHumForMeter (double humAmount) noexcept { humForMeter = humAmount; }

    // --- calibration (noise-floor.md 2, targets re the reference pluck) --------
    static double fluorescentGain() noexcept;
    static double groundLoopGain() noexcept;
    static double radioGain() noexcept;
    static double ampHissGain() noexcept;
    static double cableGain() noexcept;

private:
    void buildGroundLoopTable (uint32_t seed) noexcept;
    void startCableEvent (juce::uint32 hash) noexcept;
    double nextRadio() noexcept;
    double nextAmpHiss() noexcept;
    double nextCable() noexcept;

    double sr = 48000.0;
    int maxBlock = 512;
    NoiseFloorSettings settings;

    uint64_t seed64 = 0;
    juce::uint32 seed = 0x9E3779B9u;
    bool seeded = false;

    // --- block taps ----------------------------------------------------------
    std::vector<double> pickupBuf, circuitInBuf, diBuf, ampInBuf;
    std::vector<double> prevAmpOut, ampTotalIn;
    int prevAmpOutCount = 0;

    // --- mains (2) -----------------------------------------------------------
    double mainsPhase = 0.0;
    double posGain = 1.0;

    // --- fluorescent (2.2) ---------------------------------------------------
    Biquad fluorBand, fluorBody;
    juce::uint32 fluorIndex = 0;

    // --- ground loop (2.5) ---------------------------------------------------
    static constexpr int kTableSize = 2048;
    std::array<double, kTableSize + 1> groundTable {};

    // --- passive hiss (2.3) --------------------------------------------------
    RtRandom hissRng { 1 };

    // --- cable (2.4) ----------------------------------------------------------
    struct CableVoice
    {
        bool active = false;
        double level = 0.0, env = 0.0, decay = 1.0, crackleProb = 0.0;
        int samplesLeft = 0;
        juce::uint32 age = 0;
        Biquad thump, crackle;
        RtRandom rng { 1 };
    };

    std::array<CableVoice, 4> cable;
    juce::uint32 rollIndex = 0, voiceAge = 0;
    double ringSeconds = 0.0;
    int cableEvents = 0;
    double cableLevelNow = 0.0;

    // --- radio (2.6) ------------------------------------------------------------
    RtRandom radioRng { 1 };
    Biquad radioHp, radioLp;
    double radioEnv = 0.0, radioEnvTarget = 0.0, radioSyllablePhase = 0.0, radioSyllableRate = 4.0;
    int radioSegmentLeft = 0;
    bool radioInPhrase = false;
    double whistlePhase = 0.0, whistleLfoPhase = 0.0;
    double radioLevelNow = 0.0;

    // --- amp hiss (2.7) -------------------------------------------------------
    RtRandom ampRng { 1 };
    double pink0 = 0.0, pink1 = 0.0, pink2 = 0.0;
    double pinkA0 = 0.99765, pinkA1 = 0.963, pinkA2 = 0.57;

    // --- microphonics (2.8) ---------------------------------------------------
    Biquad micRes, micResIn, micResOut;
    DCBlocker micDc;
    double micHz = 4000.0, micQ = 30.0;
    double micAmpGain = 1.0;
    double micState = 0.0;

    // Unit-RMS normalisers, measured once in prepare() so every source's level
    // is its target at any sample rate (engine.md 0.5).
    double fluorNorm = 1.0, radioNorm = 1.0, hissNorm = 1.0;

    // --- meter --------------------------------------------------------------------
    double humForMeter = 0.0, shareNow = 0.0;
    std::atomic<double> meterDb { -240.0 };
    std::atomic<juce::uint32> meterUpdates { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseFloor)
};

} // namespace luthier
