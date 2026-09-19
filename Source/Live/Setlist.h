#pragma once

/*  Setlists (live-performance.md section 4).

    An ordered list of presets, each with the snapshot it should land on and a
    note for the player. It lives outside the preset system on purpose: a setlist
    that was stored inside a preset would be lost the moment that preset was
    edited, and the whole point of a setlist is that it outlives the rig.

    Walking a setlist has to be gap-free, so the entry after the current one is
    read from disk into `nextEntryData` ahead of time. That is the only expensive
    part of a switch, and doing it early means the switch itself is a parse of
    something already in memory.
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace luthier
{

//==============================================================================
/** One entry (live-performance 4). */
struct SetlistEntry
{
    juce::String presetPath;
    int snapshotIndex = 0;
    juce::String notes;

    /** The name to show, which is the preset's file name unless it is empty. */
    juce::String getDisplayName() const;

    juce::var toVar() const;
    static SetlistEntry fromVar (const juce::var& state);
};

//==============================================================================
class Setlist
{
public:
    static const char* const kFileExtension;

    Setlist();

    void clear();

    const juce::String& getName() const noexcept { return name; }
    void setName (const juce::String& n) { name = n; }

    const juce::String& getNotes() const noexcept { return notes; }
    void setNotes (const juce::String& n) { notes = n; }

    double getDefaultBpm() const noexcept { return defaultBpm; }
    void setDefaultBpm (double bpm) noexcept;

    //==========================================================================
    int getNumEntries() const noexcept { return (int) entries.size(); }
    const SetlistEntry& getEntry (int index) const noexcept;

    void addEntry (const SetlistEntry& entry);
    bool insertEntry (int index, const SetlistEntry& entry);
    bool removeEntry (int index);
    bool moveEntry (int fromIndex, int toIndex);

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& state);

    bool loadFrom (const juce::File& file);
    bool saveTo (const juce::File& file) const;

    static juce::File getUserDirectory();

    /** Every `.luthierset` in the user's setlist folder. */
    static juce::Array<juce::File> findSetlists();

private:
    juce::String name { "Untitled Set" };
    juce::String notes;
    double defaultBpm = 120.0;

    std::vector<SetlistEntry> entries;
};

//==============================================================================
/** Walks a setlist, keeping the next entry warm so a switch has nothing to wait
    for (live-performance 4). */
class SetlistPlayer
{
public:
    SetlistPlayer();

    void setSetlist (const Setlist& setlist);
    const Setlist& getSetlist() const noexcept { return setlist; }

    int getPosition() const noexcept { return position; }

    /** Moves to an entry. Returns false if the index is outside the setlist. */
    bool goTo (int index);

    bool next();
    bool previous();

    const SetlistEntry* getCurrentEntry() const noexcept;
    const SetlistEntry* getPreviousEntry() const noexcept;
    const SetlistEntry* getNextEntry() const noexcept;

    /** The preset data for the current entry, parsed and ready to apply. Empty
        if the entry's file is missing or unreadable. */
    const juce::var& getCurrentEntryData() const noexcept { return currentEntryData; }

    /** Called by the caller after it has applied the current entry. Reads the
        following entry from disk so the next switch does not have to.

        Message thread: it touches the file system. */
    void preloadNext();

    /** What the caller should do when an entry's file has gone missing. */
    juce::String getLastError() const { return lastError; }

private:
    juce::var readEntry (int index);

    Setlist setlist;
    int position = 0;

    juce::var currentEntryData;
    juce::var nextEntryData;
    int preloadedIndex = -1;

    juce::String lastError;

    JUCE_LEAK_DETECTOR (SetlistPlayer)
};

} // namespace luthier
