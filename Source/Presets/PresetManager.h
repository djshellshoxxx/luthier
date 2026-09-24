#pragma once

/*  Preset system (build spec, "Preset system").

    Format: JSON, extension .luthierpreset. The file carries every automatable
    parameter plus the non-automatable extras (MIDI mappings, per-string custom
    gauges and detune, tags, notes), and a schema version so that later releases
    can migrate old files rather than refusing them.

    Locations:
      Factory  - Resources/Presets/Factory/<Category>/
      User     - <Documents>/Luthier/Presets/User/
      Extra    - any folders the user registers in Preferences
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Parameters.h"
#include "../PhysicalRange.h"

namespace luthier
{

//==============================================================================
struct PresetInfo
{
    juce::String name;
    juce::String category;
    juce::String author;
    juce::String description;
    juce::StringArray tags;
    juce::File file;
    bool isFactory = false;

    // preset-browser-previews.md 5.6 (FEAT-BROWSER): filled in scanFolder.
    juce::String uid;              ///< the file's uid, or "factory:<name>" for a factory preset
    juce::String guitarName, family, ampName;
    juce::Time modified;
};

//==============================================================================
class PresetManager : public juce::ChangeBroadcaster
{
public:
    static constexpr int kSchemaVersion = 1;
    static const char* const kFileExtension;

    /*  file-formats.md 0.5: every file carries a magic marker, so a JSON file
        that is not one of ours is refused rather than half-loaded.

        The canonical marker is `magic`. Files written before file-formats.md
        existed carry `format` instead, with a different spelling, so both are
        accepted on load; `magic` is what gets written. */
    static const char* const kMagic;
    static const char* const kLegacyMagic;

    /** How long a superseded file is kept in the backup folder (file-formats 13). */
    static constexpr int kBackupRetentionDays = 30;

    /** file-formats 13.4: copies the file being replaced into the dated backup
        folder before the new one lands on top of it. */
    static void backupBeforeOverwrite (const juce::File& target);

    /*  guitar-workshop.md 9: pickup position and height were parameters and are
        placements now. A preset saved before stores them in `parameters`; the
        load keeps them here, in engine slot order, until the guitar that
        loads next takes them. */
    struct LegacyPlacement { bool present = false; double positionFraction = 0.13; double heightMm = 2.5; };
    std::array<LegacyPlacement, 3> takeLegacyPickupPlacements();
    bool hasLegacyPickupPlacements() const noexcept
    {
        return std::any_of (legacyPlacements.begin(), legacyPlacements.end(),
                            [] (const LegacyPlacement& p) { return p.present; });
    }

    /*  file-formats.md 2 / guitar-workshop.md 8: the preset's `guitar` block,
        `{ "reference": ..., "override": ... }`. The guitar is the processor's,
        so it supplies the block on save and is handed it at the end of every
        load, after the parameters (void for a preset saved before the
        Workshop). Message thread. */
    std::function<juce::var()> captureGuitarBlock;
    std::function<void (const juce::var&)> onGuitarBlockLoaded;

    /** After a load has written its pedal types and their parameters, so the
        pedals can be built with the loaded settings rather than their defaults
        (ParameterBridge::adoptPedalTypesFromParameters). */
    std::function<void()> onPedalTypesLoaded;

    /*  file-formats.md 2 (MODEL-GAPS, TODO 2k): a preset the load had to migrate
        - the legacy `format` magic, no `ranges` block (schema 1, pre-M42), a
        pre-Workshop `guitar.name`, or the retired pickup-placement parameters -
        has its original kept as Backup/<yyyy-mm-dd>/<name>-v<schema>.luthierpreset
        beside it. Once per file and schema: loading it again finds the backup
        already there (in any dated folder) and does not file another. */
    static bool needsMigration (const juce::var& data);
    static juce::File backupMigratedOriginal (const juce::File& original, int schema);

    /** Where the last load filed its migration backup; empty when it made none. */
    juce::File getLastMigrationBackup() const { return lastMigrationBackup; }

    /** Deletes backups older than kBackupRetentionDays. Called once on startup. */
    static void pruneOldBackups();

    /*  `ranges` is the processor's RangeState (advanced-ranges.md). It is
        passed by reference rather than reached through the processor because
        this class deliberately holds a generic juce::AudioProcessor& - it
        knows about parameters and the engine, not about Luthier's processor
        type, and coupling it to that for one field would be a step backwards. */
    PresetManager (juce::AudioProcessor& processor,
                   juce::AudioProcessorValueTreeState& state,
                   LuthierEngine& engine,
                   RangeState& ranges);
    ~PresetManager() override;

    //==========================================================================
    /** Rescans every registered folder. Safe to call from the message thread. */
    void refresh();

    int getNumPresets() const noexcept { return presets.size(); }
    const PresetInfo* getPreset (int index) const noexcept;

    /** The index of the preset with this name, or -1. Case-insensitive, and it
        prefers a factory preset when a user preset shares the name, so "New
        preset" cannot be redefined by saving a user preset called Init. */
    int indexOfPreset (const juce::String& name) const noexcept;

    /** Indices of the presets in one category, or all of them if empty. */
    juce::Array<int> getPresetsInCategory (const juce::String& category) const;
    juce::StringArray getCategories() const;

    int getCurrentPresetIndex() const noexcept { return currentIndex; }
    juce::String getCurrentPresetName() const { return currentName; }

    /*  The file the current preset was loaded from, or a file that does not exist
        if the session has never loaded one.

        GAPS.md A5 said this was why "Reveal preset file" could not be built -
        "PresetManager tracks the current preset's name and index but not its file
        path". The index half was true and not enough: `loadPreset (File)`, which
        is what the header's Open dialog calls, does not set an index at all, so
        for a preset opened from anywhere but the library there was nothing to map
        back. Recording the file on load covers both. */
    juce::File getCurrentPresetFile() const { return currentFile; }
    bool isCurrentPresetModified() const noexcept { return modified; }
    void markModified() noexcept;

    //==========================================================================
    bool loadPreset (int index);
    bool loadPreset (const juce::File& file);
    bool loadNext();
    bool loadPrevious();

    /*  Why the last load failed, in a sentence a user can read, or empty if the
        last one worked.

        Every failure path already writes to the error log, which is right and is
        not enough: ground rule 0.2 is that degradation is never silent, and most
        callers here discard the bool. The header's Open dialog is the clearest
        case - a preset that will not load currently does nothing at all, with no
        message anywhere the user can see.

        This is what gui-integration 15's "preset load error" banner reads. It is
        a value the window polls rather than a callback the loader fires, because
        presets are loaded from five places including a host program change, and
        five call sites each remembering to report would be five chances to
        forget. Cleared by the next load that succeeds. */
    juce::String getLastLoadError() const { return lastLoadError; }

    /** Saves over the current user preset, or falls back to Save As behaviour if
        the current preset is a factory one. */
    bool saveCurrent();
    bool saveAs (const juce::String& name, const juce::String& category,
                 const juce::String& description = {}, const juce::StringArray& tags = {});

    bool deletePreset (int index);

    // ==== BEGIN FEAT-BROWSER (preset-browser-previews.md 5.4) ====
    /** The current preset's uid (written on the first save of a user preset)
        and its author-chosen preview phrase. Both round-trip. */
    juce::String getCurrentUid() const { return currentUid; }
    juce::String getCurrentPreviewPhrase() const { return currentPreviewPhrase; }
    void setCurrentPreviewPhrase (const juce::String& phraseId) { currentPreviewPhrase = phraseId; }

    /** 2: called after saveAs / saveCurrent's atomic write, with the file. */
    std::function<void (const juce::File&)> onPresetSaved;
    // ==== END FEAT-BROWSER ====

    /** Import copies the file into the user folder; export writes it anywhere. */
    bool importPreset (const juce::File& source);
    bool exportPreset (const juce::File& destination);

    //==========================================================================
    /** Serialises the current state. Used by presets and by host state. */
    juce::var toVar (const juce::String& name = {},
                     const juce::String& category = {},
                     const juce::String& description = {},
                     const juce::StringArray& tags = {}) const;

    /** Applies a serialised state. Returns false if the data is unusable. */
    bool fromVar (const juce::var& data);

    //==========================================================================
    // Extra state the preset carries beyond the automatable parameters.

    struct ExtraState
    {
        std::array<double, kMaxStrings> customGaugeInches {};
        std::array<double, kMaxStrings> detuneCents {};
        std::array<double, kMaxStrings> realismDetuneCents {};
        std::array<double, kMaxStrings> fineTuneCents {};
        std::array<double, kMaxStrings> openFrequencyHz {};
        std::array<bool, kMaxStrings> stringMuted {};
        std::array<double, 12> customTemperament {};
        int numStrings = 6;
        bool useCustomTuning = false;
    };

    ExtraState& getExtraState() noexcept { return extra; }
    const ExtraState& getExtraState() const noexcept { return extra; }

    /** Sets the extra state to its defaults (does not push it). */
    void resetExtraState();

    /** Pushes the extra state into the engine. */
    void applyExtraState();

    /** Reads the engine's current per-string state back into the extra state. */
    void captureExtraState();

    /** True once the per-string state holds something real: a preset or a
        session read it, or it was captured from the engine. A fresh
        instance's is only defaults, and applying it would undo the guitar. */
    bool hasExtraState() const noexcept { return extraStateValid; }

    //==========================================================================
    static juce::File getUserPresetFolder();

    /** Where the factory bank is written. The folder inside the installed bundle
        when that can be written to, and a folder under Documents when it cannot -
        which is the normal case for a plugin installed under Program Files. */
    static juce::File getFactoryPresetFolder();

    /** The folder inside the installed bundle, writable or not, or an invalid File
        if there is none. Scanned as well as the folder above, so a bank installed
        by hand beside the plugin is still found. */
    static juce::File getShippedPresetFolder();

    static juce::File getRenderFolder();

    void addSearchFolder (const juce::File& folder);
    void removeSearchFolder (const juce::File& folder);
    juce::Array<juce::File> getSearchFolders() const { return searchFolders; }

    /** Writes the factory bank to disk if it is not already there. Called once at
        startup so a fresh install has presets without needing an installer step. */
    void ensureFactoryPresetsInstalled();

    //==========================================================================
    /** Restores every parameter to its default. The "Reset" button. */
    void resetToDefaults();

    /** Randomises the parameters, honouring locks. Each press starts from the
        defaults so the results do not compound.

        `respectStockRanges` keeps a physical parameter inside its stock range
        even when its family is unlocked (advanced-ranges.md 5). */
    void randomise (uint64_t seed, const juce::StringArray& lockedParameters,
                    bool respectStockRanges = true);

    /** Parameters excluded from randomisation because randomising them produces
        something unusable rather than something interesting. */
    static bool isRandomisable (const juce::String& paramId);

private:
    void scanFolder (const juce::File& folder, bool factory);
    bool writeToFile (const juce::File& file, const juce::var& data) const;

    /** Set on every load failure beside the error-log line, cleared on success. */
    juce::String lastLoadError;

    /** Where the current preset came from. Empty until something is loaded. */
    juce::File currentFile;


    /*  file-formats 0.3: fields this build does not understand are kept on load
        and written back on save.

        Without this, opening a preset written by a newer version and re-saving it
        would silently delete whatever that version added - which is a data-loss
        path that only shows up once two versions are in use. */
    juce::var unknownFields;
    juce::File lastMigrationBackup;   // MODEL-GAPS

    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    /** advanced-ranges.md: the processor's range state, applied on load and
        written on save. */
    RangeState& ranges;
    std::array<LegacyPlacement, 3> legacyPlacements {};
    LuthierEngine& engine;

    juce::Array<PresetInfo> presets;
    juce::Array<juce::File> searchFolders;

    int currentIndex = -1;
    juce::String currentName { "Init" };
    juce::String currentCategory { "User" };
    bool modified = false;

    ExtraState extra;

    bool extraStateValid = false;

    juce::String currentUid, currentPreviewPhrase;   // FEAT-BROWSER (5.4)
    std::unique_ptr<class PresetFeatureReader> featureReader;   // FEAT-BROWSER (5.6)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace luthier
