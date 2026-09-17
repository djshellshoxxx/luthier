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
};

//==============================================================================
class PresetManager : public juce::ChangeBroadcaster
{
public:
    static constexpr int kSchemaVersion = 1;
    static const char* const kFileExtension;

    PresetManager (juce::AudioProcessor& processor,
                   juce::AudioProcessorValueTreeState& state,
                   LuthierEngine& engine);
    ~PresetManager() override;

    //==========================================================================
    /** Rescans every registered folder. Safe to call from the message thread. */
    void refresh();

    int getNumPresets() const noexcept { return presets.size(); }
    const PresetInfo* getPreset (int index) const noexcept;

    /** Indices of the presets in one category, or all of them if empty. */
    juce::Array<int> getPresetsInCategory (const juce::String& category) const;
    juce::StringArray getCategories() const;

    int getCurrentPresetIndex() const noexcept { return currentIndex; }
    juce::String getCurrentPresetName() const { return currentName; }
    bool isCurrentPresetModified() const noexcept { return modified; }
    void markModified() noexcept;

    //==========================================================================
    bool loadPreset (int index);
    bool loadPreset (const juce::File& file);
    bool loadNext();
    bool loadPrevious();

    /** Saves over the current user preset, or falls back to Save As behaviour if
        the current preset is a factory one. */
    bool saveCurrent();
    bool saveAs (const juce::String& name, const juce::String& category,
                 const juce::String& description = {}, const juce::StringArray& tags = {});

    bool deletePreset (int index);

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

    /** Pushes the extra state into the engine. */
    void applyExtraState();

    /** Reads the engine's current per-string state back into the extra state. */
    void captureExtraState();

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
        defaults so the results do not compound. */
    void randomise (uint64_t seed, const juce::StringArray& lockedParameters);

    /** Parameters excluded from randomisation because randomising them produces
        something unusable rather than something interesting. */
    static bool isRandomisable (const juce::String& paramId);

private:
    void scanFolder (const juce::File& folder, bool factory);
    bool writeToFile (const juce::File& file, const juce::var& data) const;

    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    LuthierEngine& engine;

    juce::Array<PresetInfo> presets;
    juce::Array<juce::File> searchFolders;

    int currentIndex = -1;
    juce::String currentName { "Init" };
    juce::String currentCategory { "User" };
    bool modified = false;

    ExtraState extra;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace luthier
