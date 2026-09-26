#pragma once

/*  The room the guitar is in (environment.md).

    Temperature and humidity, derived from each string's own numbers rather
    than a table: a string's pitch moves with the strain its core carries, so
    thermal detuning is 865.6 * delta-strain / strain, and the thin core of a
    wound string is why wound strings are so sensitive (2.1).

    Everything lags (0.2): string wire in seconds, the neck and body in
    minutes, the wood's moisture in days. The session profiles are analytic
    in time (3.3), so the host timeline can be seeked and a bounce matches
    playback; automation is added by superposition through block-rate
    one-poles with the same time constants.

    Room defaults are a bit-exact no-op (0.3): at 22 C, tuned at 22 C, 45 %
    RH and Static, every delta is exactly zero.

    Environment is state, not wear (0.4): it is never scaled by the character
    amount.

    Threading: inputs are set and advance() runs on the audio thread, once per
    block. The UI reads published atomics. No allocation except toVar/fromVar.
*/

#include "../DSP/Common/DspCommon.h"
#include "../DSP/Noise/FretBuzz.h"
#include "../Model/Guitar/Chambering.h"

#include <array>
#include <atomic>

namespace luthier
{

enum class EnvProfile { staticRoom = 0, stageLights, outdoorEvening, coldCase, airConditioned, humidClub, numProfiles };
enum class EnvClock { hostTimeline = 0, freeRunning, numClocks };

//==============================================================================
/** The model's output (environment.md 4). */
struct EnvironmentState
{
    std::array<double, kMaxStrings> openCents {};
    std::array<double, kMaxStrings> invStrain {};
    std::array<double, kMaxStrings> stringTempC {};

    double reliefDeltaMm = 0.0;
    double actionDeltaMm = 0.0;
    double plateFreqMul = 1.0;
    double plateQMul = 1.0;
    double airFreqMul = 1.0;
    double corrosionRate = 1.0;    ///< k_RH, published to string-aging.md 3.1

    double neckTempC = 22.0;
    double bodyTempC = 22.0;
    double ambientC = 22.0;
    double ambientRh = 45.0;
    double rhAcclimatised = 45.0;
};

//==============================================================================
class EnvironmentModel
{
public:
    // --- constants (2.1, 2.2) --------------------------------------------------
    static constexpr double kCentsPerStrain = 865.6;   ///< 1200/ln2 * 1/2
    static constexpr double kAlphaSteel = 12.0e-6;
    static constexpr double kAlphaNylon = 80.0e-6;
    static constexpr double kAlphaFluorocarbon = 120.0e-6;
    static constexpr double kAlphaNeck = 4.0e-6;
    static constexpr double kTauNeck = 900.0;
    static constexpr double kTauBody = 1200.0;
    static constexpr double kTauMoisture = 86400.0;
    static constexpr double kRoomC = 22.0;
    static constexpr double kReferenceRh = 45.0;
    static constexpr double kReferenceEmc = 8.5;
    static constexpr double kColdCaseKelvin = 17.0;
    static constexpr double kMaxCents = 300.0;

    struct Inputs
    {
        double temperatureC = kRoomC;   ///< env_temperature_c
        double tunedAtC = kRoomC;       ///< env_tuned_at_c
        double humidityPct = kReferenceRh;
        EnvProfile profile = EnvProfile::staticRoom;
        EnvClock clock = EnvClock::hostTimeline;
    };

    struct ProfileShape { double deltaK, deltaRh, tauSeconds; bool coldCase; };
    static ProfileShape getProfile (EnvProfile p) noexcept;

    EnvironmentModel();

    void prepare (double sampleRate) noexcept;

    /** Back to t = 0: the parts at steady state and the reference derived
        from env_tuned_at_c (or the cold case's arrival state). */
    void reset() noexcept;

    void setNumStrings (int n) noexcept;

    /*  One string's physical identity (2.1), from the StringSpec
        refreshStringPhysics has just computed: its core strain, its core's
        expansion coefficient and its diameter (for the wire's time constant). */
    void setStringMaterial (int stringIndex, double strain, double alphaString, double diameterMm) noexcept;

    /** 2.4: the body's chambering sets how far the top rises with moisture. */
    void setChambering (Chambering c) noexcept;
    Chambering getChambering() const noexcept { return chambering; }

    void setInputs (const Inputs& in) noexcept;
    const Inputs& getInputs() const noexcept { return inputs; }

    /*  Once per block. `hostSeconds` is the playhead position when the host is
        playing, or negative when it is not (3.4). */
    void advance (double seconds, double hostSeconds, bool hostPlaying) noexcept;

    /*  3.2: Retune - the reference becomes the parts' current temperatures.
        `displayedTunedAt` is what the UI writes into env_tuned_at_c, so that
        write is not mistaken for a new tuned-at. Any thread: applied at the
        next advance(). */
    void requestRetune (double displayedTunedAt) noexcept;

    const EnvironmentState& getState() const noexcept { return state; }

    /** 2.5: the extra cents fretting at `fret` adds because of the geometry deltas. */
    double fretCents (int stringIndex, double fret) const noexcept;

    /*  The setup the player asked for; the deltas are added to it (4). Returns
        true, and fills `effective`, when the requested setup changed or the
        deltas have moved by more than 0.005 mm since the last call that
        returned true. The fret-stretch table (2.5) is rebuilt then too. */
    bool updateGeometry (const SetupGeometry& requested, SetupGeometry& effective, bool requestedChanged) noexcept;

    /*  The mean over the strings of the steady-state offset per kelvin, both
        string and neck moved (2.1). Negative: heat makes strings go flat.
        For the legacy loader (6). */
    double steadyCentsPerKelvin() const noexcept;

    /** The parts jump to their steady state at the next advance() (a legacy
        load, whose old offset was immediate). */
    void requestSettle() noexcept { settlePending.store (true); }

        /** The time the profile is at, seconds. */
    double getTime() const noexcept { return t; }

    /** True once any input has left the room defaults (the per-block work is skipped until then). */
    bool isActive() const noexcept { return active; }

    //==========================================================================
    // UI readouts (10 Hz). Published after each advance().
    double getPublishedStringTemp (int s) const noexcept { return pubStringTemp[(size_t) juce::jlimit (0, kMaxStrings - 1, s)].load (std::memory_order_relaxed); }
    double getPublishedCents (int s) const noexcept      { return pubCents[(size_t) juce::jlimit (0, kMaxStrings - 1, s)].load (std::memory_order_relaxed); }
    double getPublishedNeckTemp() const noexcept         { return pubNeck.load (std::memory_order_relaxed); }
    double getPublishedBodyTemp() const noexcept         { return pubBody.load (std::memory_order_relaxed); }
    double getPublishedRh() const noexcept               { return pubRh.load (std::memory_order_relaxed); }
    double getPublishedReliefDelta() const noexcept      { return pubRelief.load (std::memory_order_relaxed); }
    double getPublishedActionDelta() const noexcept      { return pubAction.load (std::memory_order_relaxed); }
    juce::uint32 getPublishSerial() const noexcept       { return pubSerial.load (std::memory_order_relaxed); }

    //==========================================================================
    // The physics, pure, for the tests and the UI.

    /** 2.3: equilibrium moisture content, %, at a relative humidity, %. */
    static double emc (double rhPercent) noexcept;

    /** 2.4: k_top by chambering, mm of top rise per % MC. */
    static double topRise (Chambering c) noexcept;

    /** 3.3: a first-order lag's response to T0 + dT (1 - e^-t/tp), minus T0. */
    static double lagResponse (double deltaK, double tauProfile, double tau, double t) noexcept;

    /** The string wire's time constant (2.2). */
    static double wireTau (double diameterMm) noexcept { return 5.0 * juce::jmax (0.05, diameterMm) / 0.25; }

    /** 2.1: cents from the string and neck temperature changes. */
    static double thermalCents (double strain, double alphaString, double dTString, double dTNeck) noexcept;

    /** 2.5: cents at fret n for open-string clearances before and after. */
    static double stretchCents (double hNominalMm, double hEnvironmentMm, double fretPositionMm,
                                double scaleLengthMm, double strain) noexcept;

    //==========================================================================
    /** section 6: {"ref_string_c": [...], "ref_neck_c": x}. Message thread. */
    juce::var toVar() const;
    void fromVar (const juce::var& v);

private:
    void deriveReference() noexcept;
    void computeOutputs() noexcept;
    void restartAutomation() noexcept;

    double sr = 44100.0;
    int numStrings = 6;
    Inputs inputs, lastInputs;
    bool firstInputs = true;
    Chambering chambering = Chambering::solid;

    std::array<double, kMaxStrings> strain {}, alpha {}, tauString {};

    double t = 0.0;
    double lastHostSeconds = -1.0;

    // Automation one-poles (3.3), per part.
    std::array<double, kMaxStrings> autoString {};
    double autoNeck = kRoomC, autoBody = kRoomC;

    // The reference ("tuned at", 3.2).
    std::array<double, kMaxStrings> refString {};
    double refNeck = kRoomC;
    double lastTunedAt = kRoomC;

    std::atomic<bool> retunePending { false };
    std::atomic<bool> settlePending { false };
    std::atomic<double> retuneDisplay { kRoomC };
    std::atomic<bool> refLoaded { false };
    std::array<std::atomic<double>, kMaxStrings> loadedRefString {};
    std::atomic<double> loadedRefNeck { kRoomC };

    EnvironmentState state;
    bool active = false;

    // 2.5's fret stretch, per string, per fret, from the last geometry.
    std::array<std::array<double, SetupGeometry::kMaxFrets + 1>, kMaxStrings> fretTable {};
    // The requested geometry's open-string clearances and their sensitivities
    // to relief and action (clearance is linear in both), and each fret's
    // stretch geometry 1/x + 1/(L - x): rebuilt only when the setup changes.
    std::array<std::array<double, SetupGeometry::kMaxFrets + 1>, kMaxStrings> hNominal {}, dhRelief {}, dhAction {};
    std::array<double, SetupGeometry::kMaxFrets + 1> stretchGeometry {};
    double geometryLength = 648.0;
    bool sensitivitiesValid = false;
    double lastAppliedRelief = 0.0, lastAppliedAction = 0.0;
    bool geometryInitialised = false;

    std::array<std::atomic<double>, kMaxStrings> pubStringTemp {}, pubCents {};
    std::atomic<double> pubNeck { kRoomC }, pubBody { kRoomC }, pubRh { kReferenceRh },
                        pubRelief { 0.0 }, pubAction { 0.0 };
    std::atomic<juce::uint32> pubSerial { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvironmentModel)
};

} // namespace luthier
