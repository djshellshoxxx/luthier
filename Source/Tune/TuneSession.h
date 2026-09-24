#pragma once

/*  The tune being edited: the Tune, its file, whether it has unsaved changes,
    its undo history, and keeping the player's timeline in step with it.

    It is what the TUNE tab talks to, so the panel only lays out controls and
    forwards what they do. The processor owns one, beside its TunePlayer, so
    the tune outlives the editor window. Everything here is message thread.

    Undo is by snapshot (tune-builder 0.3: "Every edit is undoable"): an edit
    stores the Tune as it was, and undo puts it back. action-and-undo.md sets
    the rules the stack follows - 200 entries at most (2), the redo stack
    cleared by a new edit (2), same-class edits on the same target within
    200 ms merged into one (0.3; 3.9 names the Tune Builder's classes), and a
    tune load as a state boundary (3.9), which here clears the history: the
    stack belongs to one tune.

    Every change rebuilds the timeline and hands it to the player, so what
    plays is always what is shown; the player takes it in at the next bar line.
*/

#include "TunePlayer.h"
#include "TuneFile.h"
#include "../Rhythm/GenreKit.h"

#include <functional>
#include <vector>

namespace luthier
{

class TuneSession
{
public:
    static constexpr int kMaxUndo = 200;
    static constexpr double kGroupWindowMs = 200.0;

    TuneSession();

    const Tune& getTune() const noexcept { return tune; }

    //==========================================================================
    /** Applies an edit. `change` returns whether it changed anything; nothing
        is recorded when it did not. `target` identifies what was edited (a
        section, a chord cell, a note) for 200 ms grouping; -1 never groups. */
    bool edit (TuneEditClass editClass, const juce::String& description,
               const std::function<bool (Tune&)>& change, int target = -1);

    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    bool undo();
    bool redo();
    juce::String getUndoDescription() const;
    juce::String getRedoDescription() const;
    int getNumUndoSteps() const noexcept { return (int) undoStack.size(); }

    //==========================================================================
    /** A new tune (from a template): a state boundary, no file, not dirty. */
    void newTune (const Tune& from);

    /** Loads a file. On failure nothing changes and `error` says why. */
    bool load (const juce::File& file, juce::String& error);

    /** Saves to the current file. False, with a reason, when there is none. */
    bool save (juce::String& error);
    bool saveAs (const juce::File& file, juce::String& error);

    juce::File getFile() const { return file; }
    bool isDirty() const noexcept { return dirty; }

    /** The title, with a mark when there are unsaved changes. */
    juce::String getDisplayTitle() const;

    /** The tune, its file and the dirty flag, for the plugin's state (15:
        "save a tune, close, relaunch, the last tune loads"). */
    juce::var toState() const;
    bool restoreState (const juce::var& state);

    //==========================================================================
    int getSelectedSection() const noexcept { return selectedSection; }
    void setSelectedSection (int index);

    //==========================================================================
    // Playback

    /** The player this session drives. The current timeline is handed over at
        once, and again after every change. */
    void attachPlayer (TunePlayer* player);
    TunePlayer* getPlayer() const noexcept { return player; }

    /** Where service() applies the player's rhythm changes (3.5, 8). */
    void attachRhythm (RhythmEngine* engine, const GenreKitLibrary* kits, const PatternLibrary* patterns);

    TuneMidiOptions& getMidiOptions() noexcept { return midiOptions; }
    void rebuildTimeline();

    /** A message-thread timer's work, at the UI rate: builds the next pass of
        an improvised tune ahead of the audio thread, frees retired timelines,
        applies the rhythm settings of the section that just started, and
        gathers a take being recorded. */
    void service();

    /** Puts a section's rhythm into the rhythm engine (3.5, 8). Kit first
        (GenreKitLibrary::apply sets voicing, humanise and its first pattern),
        then the section's own pattern over it, then on/off, feel and strum.
        Feel scales the kit's humanise amount (0.5 is the kit as written) and
        strum scales its strum duration the same way. Message thread. */
    static void applyRhythmChange (const TuneRhythmChange& change, RhythmEngine& engine,
                                   const GenreKitLibrary& kits, const PatternLibrary& patterns);

    //==========================================================================
    // Record (4.3). Notes arrive from the player's capture of MIDI in while
    // the tune plays (service() gathers them), or through recordNote,
    // positioned in section-relative beats; finishing quantises them into the
    // section.

    void beginRecording (int sectionIndex);
    bool isRecording() const noexcept { return recording; }
    int getRecordingSection() const noexcept { return recordingSection; }
    void recordNote (int pitch, int velocity, double startBeat, double endBeat);
    int getNumRecordedNotes() const noexcept { return (int) recorded.size(); }
    bool finishRecording (QuantiseGrid grid, bool followChordChanges, bool snapToKey);
    void cancelRecording();

    //==========================================================================
    /** Called after any change (edit, undo, load, selection), for the panel. */
    std::function<void()> onChanged;

    /** For tests: where "now" comes from, in milliseconds. */
    void setClock (std::function<double()> clockMs) { clock = std::move (clockMs); }

private:
    struct UndoEntry
    {
        Tune before;
        juce::String description;
        TuneEditClass editClass = TuneEditClass::other;
        int target = -1;
        double timeMs = 0.0;
    };

    void changed();
    void drainRecording();

    Tune tune;
    juce::File file;
    bool dirty = false;
    int selectedSection = 0;

    std::vector<UndoEntry> undoStack, redoStack;

    TunePlayer* player = nullptr;
    RhythmEngine* rhythmEngine = nullptr;
    const GenreKitLibrary* genreKits = nullptr;
    const PatternLibrary* patternLibrary = nullptr;
    TuneMidiOptions midiOptions;

    bool recording = false;
    int recordingSection = -1;
    std::vector<RecordedNote> recorded;
    std::vector<TuneSpan> recordingSpans;               ///< The play order the take is placed by.
    std::array<std::pair<double, int>, 128> openNotes;  ///< Start beat and velocity; start < 0 when not held.

    std::function<double()> clock;
};

} // namespace luthier
