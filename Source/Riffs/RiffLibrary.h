#pragma once

/*  The riff library's index (riff-library.md 3, 4).

    Owns the metadata of every factory and user riff: the factory
    catalog.json plus a scan of the user folder, loaded on a juce::ThreadPool
    job when an editor first opens the RIFFS tab or the drawer - never at
    plugin construction (performance-budget 5). Full riffs load lazily into
    an LRU cache of 32.

    Search and filter run on the message thread, over precomputed
    std::bitset<64> masks for technique, genre, type and instrument, and a
    lower-cased name / tag / genre string: text search is a case-insensitive
    substring match, AND between words.

    User-global data - favourites, the last 50 recents and play counts - lives
    in ~/Documents/Luthier/Riffs/library.json. It is not a preset.
*/

#include "Riff.h"
#include "RiffTransposer.h"

#include <bitset>
#include <list>
#include <map>
#include <memory>

namespace luthier
{

struct RiffIndexEntry
{
    juce::String id, name, type, genre, instrument, tuningName;
    juce::String keyRoot, scale, feel, author, origin;
    juce::StringArray tags, techniques;
    std::vector<int> tuning;
    int capo = 0;
    double tempoBpm = 120.0, lengthBeats = 4.0;
    int meterNumerator = 4, meterDenominator = 4;
    int difficulty = 1;
    int numNotes = 0;
    bool free = false;
    bool factory = true;
    juce::File file;

    int genreIndex = -1, typeIndex = -1, instrumentIndex = -1, rootPitchClass = -1;
    std::bitset<64> techniqueMask;
    juce::String searchText;   ///< lower case: name, tags, genre (id and display name)

    bool isBass() const { return RiffVocabulary::isBassInstrument (instrument); }

    /** "Delta Turnaround 3, lick, Blues, A minor pentatonic, 84 bpm, difficulty
        2 of 5, bend, vibrato, slide" (accessibility 10), without the favourite. */
    juce::String describe() const;

    static RiffIndexEntry fromRiff (const Riff& riff, const juce::File& file, bool factory);
    void finalise();
};

struct RiffQuery
{
    juce::String text;
    std::bitset<64> genres, types, instruments;   ///< none set = any
    std::bitset<64> techniques;                    ///< every one set must be present
    int minDifficulty = 1, maxDifficulty = 5;
    double minTempo = 0.0, maxTempo = 1000.0;
    int keyRoot = -1;                              ///< pitch class, -1 any
    bool favouritesOnly = false;
    bool userOnly = false;

    /** "Fits this instrument" (default on): hides riffs that would change
        family (bass on a guitar or the reverse) or need more strings. */
    bool fitsInstrument = false;
    GuitarSpecSummary instrument;

    enum class Sort { genreThenName = 0, name, tempo, difficulty, key, recent };
    Sort sort = Sort::genreThenName;

    juce::var toVar() const;
    static RiffQuery fromVar (const juce::var& v);
};

class RiffLibrary
{
public:
    static constexpr int kCacheSize = 32;
    static constexpr int kMaxRecents = 50;

    RiffLibrary();
    ~RiffLibrary();

    //==========================================================================
    // Folders (riff-library 3; Options -> FILE LOCATIONS "Riffs folder").
    static juce::File getDefaultFactoryFolder();
    static juce::File getRiffsRoot();              ///< ~/Documents/Luthier/Riffs
    static juce::File getDefaultUserFolder();      ///< .../Riffs/User
    static juce::File getDragFolder();             ///< .../Riffs/Drag
    static juce::File getDefaultGlobalFile();      ///< .../Riffs/library.json

    /** Where to look. Set before loading; tests point these at fixtures. */
    void setFolders (const juce::File& factory, const juce::File& user, const juce::File& globalFile);
    juce::File getFactoryFolder() const { return factoryFolder; }
    juce::File getUserFolder() const { return userFolder; }

    //==========================================================================
    // The index.

    /** Loads the index on `pool` and applies it on the message thread; the
        change callback fires when it lands. Does nothing if loaded or loading. */
    void loadIndexAsync (juce::ThreadPool& pool);

    /** Loads the index now, on this thread. */
    void loadIndexNow();

    bool isIndexLoaded() const noexcept { return loaded; }
    bool isLoading() const noexcept { return loading; }

    std::function<void()> onIndexChanged;

    int getNumEntries() const noexcept { return (int) entries.size(); }
    const RiffIndexEntry* getEntry (int index) const noexcept;
    const RiffIndexEntry* findEntry (const juce::String& id) const;
    int indexOf (const juce::String& id) const;
    int getNumFactory() const noexcept;

    /** The entries that match, in the query's order. */
    std::vector<int> query (const RiffQuery& q) const;

    /** The brute-force reference query() is checked against (RL-23). */
    bool matches (const RiffIndexEntry& e, const RiffQuery& q) const;

    //==========================================================================
    // Problems (riff-library 7.5, 15).
    bool isFactoryMissing() const noexcept { return factoryMissing; }
    const juce::StringArray& getUnreadableFiles() const noexcept { return unreadable; }
    const juce::StringArray& getDiagnostics() const noexcept { return diagnostics; }

    //==========================================================================
    // Full riffs.

    /** The riff, from the cache or its file; nullptr if unreadable. */
    std::shared_ptr<const Riff> getRiff (const juce::String& id);
    int getCacheSize() const noexcept { return (int) cache.size(); }

    //==========================================================================
    // User riffs (7.4).

    static juce::String makeUserId (const juce::String& type);

    /** Writes the riff atomically into the user folder and indexes it. Fills
        in id, author, origin, dates and version when missing. */
    juce::Result saveUserRiff (Riff& riff, juce::File* written = nullptr);

    /** Moves a user riff's file to the trash and forgets it. Factory riffs refuse. */
    juce::Result deleteUserRiff (const juce::String& id);

    //==========================================================================
    // User-global data (library.json).
    bool isFavourite (const juce::String& id) const;
    void setFavourite (const juce::String& id, bool favourite);
    void notePlayed (const juce::String& id);
    const juce::StringArray& getRecents() const noexcept { return recents; }
    int getPlayCount (const juce::String& id) const;

    void loadGlobal();
    bool saveGlobal() const;

    /** Builds an index directly, for tests (RL-23's 1000 generated entries). */
    void setEntriesForTesting (std::vector<RiffIndexEntry> newEntries);

private:
    struct IndexData
    {
        std::vector<RiffIndexEntry> entries;
        bool factoryMissing = false;
        juce::StringArray unreadable, diagnostics;
    };

    static IndexData buildIndex (const juce::File& factory, const juce::File& user);
    void applyIndex (IndexData data);
    void rebuildLookup();

    juce::File factoryFolder, userFolder, globalFile;

    std::vector<RiffIndexEntry> entries;
    std::map<juce::String, int> byId;
    bool loaded = false, loading = false, factoryMissing = false;
    juce::StringArray unreadable, diagnostics;

    std::list<std::pair<juce::String, std::shared_ptr<const Riff>>> cache;

    juce::StringArray favourites, recents;
    std::map<juce::String, int> playCounts;
    bool globalLoaded = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (RiffLibrary)
    JUCE_DECLARE_NON_COPYABLE (RiffLibrary)
};

} // namespace luthier
