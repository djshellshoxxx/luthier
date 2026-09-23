#pragma once

/*  The rest of the PRACTICE tab's model (practice-tools.md 11.2 and 11.4):
    per-tool defaults, the session recorder's setup, the library of saved
    files, and the empty-state text.

    Everything here is "before and after" (11.1). None of it starts or stops a
    tool: defaults are loaded into the tools' settings and the session setup
    sizes the recorder's buffer, but the transports stay in the drawer (11.3).
*/

#include "PracticeRoutine.h"

namespace luthier
{

//==============================================================================
/** 11.2 "Defaults - per-tool starting settings". Saved to
    `~/Documents/Luthier/Practice/defaults.json`. */
struct PracticeDefaults
{
    static constexpr int kSchemaVersion = 1;
    static constexpr const char* kMagic = "luthier.practicedefaults";

    // --- metronome: "default tempo, subdivision, accent pattern, click sample choice"
    double metronomeTempo = 120.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    ClickSubdivision subdivision = ClickSubdivision::quarter;
    ClickSound sound = ClickSound::woodBlock;
    std::vector<BeatAccent> accents { BeatAccent::accent, BeatAccent::normal, BeatAccent::normal, BeatAccent::normal };

    // --- looper: "default length, count-in, overdub mode"
    double loopLengthSeconds = 0.0;   ///< 0: the first recording sets the length.
    int loopCountInBars = 1;
    LayerMode overdubMode = LayerMode::overdub;

    // --- trainers: "default key, scale set, range, question count"
    int trainerKey = 0;               ///< Pitch class.
    std::vector<ScaleType> scaleSet { ScaleType::ionian, ScaleType::aeolian, ScaleType::minorPentatonic };
    int rangeLowNote = 40;            ///< E2
    int rangeHighNote = 76;           ///< E5
    int questionCount = 20;

    // --- backing track: "default folder, shuffle, level"
    juce::String backingFolder;
    bool shuffle = false;
    double backingLevelDb = -6.0;

    juce::NamedValueSet extra;

    /** Loads the defaults into the tools, through their own setters: the
        metronome's tempo, signature, subdivision, sound and accents; the
        active loop layer's mode; the scale trainer's key and first scale; the
        backing track's level. Starts nothing. Returns what could not be
        applied (see the TODO hooks in the .cpp for the settings no tool takes
        yet). */
    juce::StringArray applyTo (const PracticeTargets& targets) const;

    juce::var toVar() const;
    static bool fromVar (const juce::var& state, PracticeDefaults& result, juce::String& error);

    bool save (const juce::File& file, juce::String& error) const;
    static bool load (const juce::File& file, PracticeDefaults& result, juce::String& error);

    /** `~/Documents/Luthier/Practice/defaults.json`. */
    static juce::File getDefaultsFile();

    bool operator== (const PracticeDefaults& other) const;
    bool operator!= (const PracticeDefaults& other) const { return ! (*this == other); }
};

//==============================================================================
/** 11.2 "Session recorder setup - the settings, not the transport". */
struct SessionRecorderSetup
{
    double ringMinutes = SessionRecorder::kDefaultMinutes;
    bool recordAudio = true;
    bool recordMidi = true;
    bool autoSaveOnStop = false;

    /** Bytes the ring takes: minutes of stereo 32-bit float (practice-tools 8:
        "60 minutes at 48 kHz stereo float32 = ~1.4 GB"). */
    static juce::int64 estimateBytes (double minutes, double sampleRate = 48000.0, int channels = 2);

    /** "1.4 GB", "350 MB": decimal units, as the spec quotes them. */
    static juce::String describeBytes (juce::int64 bytes);

    /** The plain-words size warning the tab shows beside the control
        (11.2, performance-budget 3). */
    juce::String getSizeWarning (double sampleRate = 48000.0) const;

    /** Sizes the recorder's ring. Message thread: this allocates, which is why
        it is setup and not transport. Returns false when the recorder could
        not get the memory. */
    bool applyTo (SessionRecorder& recorder, double sampleRate) const;

    juce::var toVar() const;
    static SessionRecorderSetup fromVar (const juce::var& state);

    bool operator== (const SessionRecorderSetup& o) const noexcept
    {
        return ringMinutes == o.ringMinutes && recordAudio == o.recordAudio
            && recordMidi == o.recordMidi && autoSaveOnStop == o.autoSaveOnStop;
    }
};

//==============================================================================
/** 11.2 "Library - the files". Lists what is on disk; the tab adds the play,
    delete and reveal buttons. */
class PracticeLibrary
{
public:
    static constexpr int kMaxRecentTabs = 10;

    struct Item
    {
        juce::String name;
        juce::File file;          ///< A loop's folder, or a session's WAV.
        juce::Time modified;
        juce::int64 sizeBytes = 0;
    };

    /** Saved loops: the folders Looper::save writes (a `loop.json` beside the
        layer WAVs), newest first. */
    static std::vector<Item> listLoops (const juce::File& directory = Looper::getUserDirectory());

    /** Saved sessions: the WAVs SessionRecorder::saveLastTake writes, newest
        first. The ring's own temp folder is not listed. */
    static std::vector<Item> listSessions (const juce::File& directory = SessionRecorder::getSessionDirectory());

    /** Deletes a saved loop's folder. Refuses anything that is not a loop
        folder, so a mistaken path cannot take a user directory with it. */
    static bool deleteLoop (const Item& loop);

    //==========================================================================
    /** "Tab files opened recently", newest first, at most kMaxRecentTabs. */
    void noteTabOpened (const juce::File& file);
    const juce::Array<juce::File>& getRecentTabs() const noexcept { return recentTabs; }

    /** Drops recent files that no longer exist. */
    void pruneMissing();

    juce::var toVar() const;
    void fromVar (const juce::var& state);

    bool save (const juce::File& file, juce::String& error) const;
    bool load (const juce::File& file);

    /** `~/Documents/Luthier/Practice/library.json`. */
    static juce::File getLibraryFile();

private:
    juce::Array<juce::File> recentTabs;
};

//==============================================================================
/** 11.4's empty states, word for word. */
namespace PracticeEmptyStates
{
    inline constexpr const char* noStats =
        "No practice recorded yet. The metronome, looper and trainers all count time once you start them.";

    /** The three factory routines are always present, so only the user's own
        section can be empty. */
    inline constexpr const char* noUserRoutines = "Your own routines appear here.";

    inline constexpr const char* noLoops = "Saved loops appear here";

    /** 11.4 names one string for "no saved loops or sessions"; the sessions
        list uses the same wording for its own files. */
    inline constexpr const char* noSessions = "Saved sessions appear here";
}

} // namespace luthier
