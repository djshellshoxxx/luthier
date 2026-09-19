#pragma once

/*  Controller profiles (controllers.md).

    Every supported controller is a named profile rather than a branch in the
    MIDI code. A profile says what MIDI dialect the hardware speaks - standard,
    MPE, or one channel per string - which channel carries which string, how far
    its pitch bend travels, what its continuous controllers mean, how late its
    notes arrive, and how much of its pitch output is noise rather than
    intention.

    That is a deliberate split: the interpreter knows how to play a guitar, and
    the profile knows what a particular box on the floor sends. Adding support
    for new hardware is then a JSON file, not a code change, which is what
    controllers.md section 0 asks for.
*/

#include "../DSP/Common/DspCommon.h"
#include "../Model/Playing/MidiInterpreter.h"

#include <array>
#include <vector>

namespace luthier
{

//==============================================================================
/** What MIDI dialect a controller speaks (controllers.md 0.2). */
enum class ControllerMode
{
    standard = 0,   ///< One channel, ordinary MIDI.
    mpe,            ///< MPE zone: a master channel and per-note member channels.
    perChannel,     ///< One channel per string, as hex pickups send.
    numModes
};

const char* getControllerModeName (ControllerMode mode) noexcept;
ControllerMode controllerModeFromName (const juce::String& name) noexcept;

//==============================================================================
/** One profile (controllers.md 2). */
struct ControllerProfile
{
    juce::String id;
    juce::String displayName;
    ControllerMode mode = ControllerMode::standard;

    /** Per-string routing, for `perChannel` mode. */
    struct StringRouting
    {
        int channel = 0;              ///< 1-based; 0 means this string is unrouted.
        double pitchBendSemis = 2.0;
    };

    std::array<StringRouting, kMaxStrings> perString {};

    /** True when any string carries a channel, which is what makes the profile
        usable in per-channel mode. */
    bool hasPerStringRouting() const noexcept;

    //==========================================================================
    // MPE and standard settings.

    int mpeMasterChannel = 1;
    int mpeFirstMemberChannel = 2;
    int mpeLastMemberChannel = 16;

    double pitchBendSemis = 2.0;          ///< Master / standard bend range.
    double memberPitchBendSemis = 48.0;   ///< MPE member-channel bend range.

    //==========================================================================
    /** CC number -> what it drives. Only the entries the profile names. */
    std::array<MidiTarget, 128> ccMap {};

    //==========================================================================
    // Latency (controllers.md 3).

    /** What the hardware typically costs, before the user measures it. */
    double latencyMsDefault = 0.0;

    /** What the latency wizard actually measured, or a negative number if it
        never has been. */
    double latencyMsMeasured = -1.0;

    /** The one that should be used: the measurement if there is one. */
    double getEffectiveLatencyMs() const noexcept
    {
        return latencyMsMeasured >= 0.0 ? latencyMsMeasured : latencyMsDefault;
    }

    //==========================================================================
    // Calibration (controllers.md 5).

    /** Sustained notes on some controllers wobble around their pitch. Bends
        smaller than this are treated as noise. */
    double pitchDeadZoneCents = 5.0;

    /** Some controllers send lazy note-offs; a note lasts at least this long. */
    double minimumNoteDurationMs = 0.0;

    /** LinnStrument's "guitar mode": rows map to strings. */
    bool rowsAsStrings = false;

    /** Osmose's pitch response is not linear, so the profile carries a curve.
        Empty means linear. Values are output-per-input over 0..1. */
    std::vector<double> pitchCurve;

    juce::String notes;

    //==========================================================================
    bool isValid() const noexcept { return id.isNotEmpty(); }

    /** Maps a normalised bend, -1..1, through the profile's pitch curve. */
    double applyPitchCurve (double normalised) const noexcept;

    /** Which string a channel drives, or -1. Only meaningful in per-channel
        mode. */
    int stringForChannel (int channel) const noexcept;

    juce::var toVar() const;
    static ControllerProfile fromVar (const juce::var& state);

    bool loadFrom (const juce::File& file);
    bool saveTo (const juce::File& file) const;
};

//==============================================================================
/** The profiles the plugin knows about (controllers.md 1).

    Built in code so the plugin always has them, and overridable from disk so a
    user can correct one or add their own. A profile in the user's folder beats a
    factory one of the same id, which is rule 4 of section 0. */
class ControllerProfileLibrary
{
public:
    ControllerProfileLibrary();

    void refresh();

    int getNumProfiles() const noexcept { return (int) profiles.size(); }
    const ControllerProfile& getProfile (int index) const noexcept;

    int indexOf (const juce::String& id) const;

    juce::StringArray getDisplayNames() const;

    /** Adds or replaces a profile by id, and writes it to the user directory. */
    bool save (const ControllerProfile& profile);

    static juce::File getFactoryDirectory();
    static juce::File getUserDirectory();

    /** Pushes a profile's settings into an interpreter. This is the whole point
        of a profile: everything it knows becomes interpreter state here, and
        nowhere else in the plugin has to know which controller is plugged in. */
    static void apply (const ControllerProfile& profile, MidiInterpreter& interpreter);

private:
    void addFactoryProfiles();
    void scanDirectory (const juce::File& directory);

    std::vector<ControllerProfile> profiles;
};

//==============================================================================
/** The latency wizard (controllers.md 3).

    Plays a click, waits for the player to play along, and measures the gap. One
    measurement is worthless - a human playing along to a click is scattered by
    tens of milliseconds - so it takes several and reports both the middle of
    them and how scattered they were. A profile is only worth writing when the
    scatter is small enough to trust. */
class LatencyWizard
{
public:
    /** controllers.md 7: ten runs, and a sigma inside half a millisecond. */
    static constexpr int kDefaultRuns = 10;
    static constexpr int kMaxRuns = 64;

    void begin (int runs = kDefaultRuns) noexcept;

    /** Records one click-to-note gap, in milliseconds. Returns true once enough
        runs have been collected. */
    bool addMeasurement (double millisecondsAfterClick) noexcept;

    bool isRunning() const noexcept { return running; }
    int getNumMeasurements() const noexcept { return numMeasurements; }
    int getRunsWanted() const noexcept { return runsWanted; }

    /** The median of the measurements, which is what should go into a profile. */
    double getMeasuredLatencyMs() const noexcept;

    /** How scattered the measurements were. A large sigma means the player was
        inconsistent, not that the controller is. */
    double getSigmaMs() const noexcept;

    /** True when the scatter is small enough for the result to mean something. */
    bool isReliable (double maximumSigmaMs = 0.5) const noexcept
    {
        return numMeasurements >= runsWanted && getSigmaMs() <= maximumSigmaMs;
    }

    void cancel() noexcept { running = false; numMeasurements = 0; }

private:
    std::array<double, kMaxRuns> measurements {};
    int numMeasurements = 0;
    int runsWanted = kDefaultRuns;
    bool running = false;
};

//==============================================================================
/** Merging two controllers at once (controllers.md 6).

    When a player has an MPE keyboard and a GK pickup plugged in together, both
    can ask for the same string. The rule is simple and stated in the spec: the
    most recent event wins, per string, and a string already sounding from one
    source keeps its state until its note-off.

    This tracks who owns what, and says whether an incoming event should be
    allowed through. It does not route anything itself. */
class ControllerMerge
{
public:
    static constexpr int kMaxSources = 4;

    void reset() noexcept;

    /** Asks whether `source` may take `stringIndex` now. Returns true and
        records the claim if so.

        A string nobody holds is always free. A string held by another source is
        taken over, because the spec says the most recent event wins - but the
        takeover is counted, so the UI can warn that two controllers are
        fighting. */
    bool claim (int stringIndex, int source, int64_t timestamp) noexcept;

    /** Releases a string, if `source` is the one holding it. */
    void release (int stringIndex, int source) noexcept;

    int getOwner (int stringIndex) const noexcept;

    /** controllers.md 6: how many times a source has taken a string from another
        while it was still sounding. Persistent contention is worth telling the
        user about. */
    int getContentionCount() const noexcept { return contention; }
    void clearContentionCount() noexcept { contention = 0; }

private:
    std::array<int, kMaxStrings> owner {};
    std::array<int64_t, kMaxStrings> claimedAt {};
    std::array<bool, kMaxStrings> sounding {};

    int contention = 0;
};

} // namespace luthier
