#pragma once

/*  global-search.md 2 and 3.1: the built-in providers.

    Each reads a registry that already exists - the APVTS, the choice lists,
    the tab strip and the Options pages (through the place catalogue, checked
    against them by GS-03), AccessibilitySettings, HelpContent, PresetManager,
    PartLibrary, the snapshot bank - so a new parameter, preset or topic is
    indexed without anyone editing a list here.

    Providers that must act on the window (go to a control, open a page) do so
    through SearchServices, which the editor's search bridge implements. With
    no services (the matching unit tests, which have no editor) they still
    collect and rank; activation then does nothing.
*/

#include "SearchProvider.h"
#include "ActionRegistry.h"
#include "ParameterLocations.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include "../../Model/Workshop/Part.h"

namespace luthier
{
class LuthierAudioProcessor;
}

namespace luthier::search
{

//==============================================================================
/** What a provider may ask of the editor. */
class SearchServices
{
public:
    virtual ~SearchServices() = default;

    /** Where a parameter's control is: the breadcrumb, and whether a control
        exists in Easy and in Advanced. */
    virtual void describeParameter (const juce::String& parameterId, juce::String& breadcrumb,
                                    bool& inEasy, bool& inAdvanced) = 0;

    /** Changes when the window's controls do (a pedal card rebuilt, a panel added). */
    virtual juce::uint32 getUiGeneration() = 0;

    /** 3.4: modeUnavailable for an Advanced-only item when Advanced cannot be shown. */
    virtual bool isAdvancedModeAvailable() = 0;
    virtual bool isAdvancedMode() = 0;

    /** 10: the edition split. Always false in a Pro build; the Free build
        (and GS-39) supply the predicate. */
    virtual bool isProLocked (const SearchItem&) = 0;

    virtual bool goToParameter (const SearchItem& item, ActivationKind kind) = 0;
    virtual bool openPlace (const SearchItem& item) = 0;
    virtual bool openHelpTopic (const juce::String& topicId, bool pinnedInTab) = 0;
    virtual bool openWorkshopOn (PartType type, const juce::String& partName) = 0;
    virtual bool openShortcutRow (const juce::String& actionId) = 0;
    virtual bool openSetting (const SearchItem& item) = 0;
    virtual bool showPresetInBrowser (int presetIndex) = 0;

    /** A non-parameter setting in the Options pages (3.2). */
    struct Setting
    {
        juce::String id;        ///< "set:APPEARANCE:reducedMotion"
        juce::String title;
        juce::String page;
    };

    virtual std::vector<Setting> getSettings() = 0;
};

//==============================================================================
/** Titles, breadcrumbs and hiding for parameters, shared by the parameter and
    choice-option providers. */
namespace ParameterText
{
    /** The search title: a catalog override, a pedal slot's descriptor name
        with its slot ("Delay time (Post 3)"), or the parameter's name. */
    juce::String titleFor (LuthierAudioProcessor& processor, const juce::RangedAudioParameter& p, bool english);

    /** An empty slot's _pN, or one past the fitted pedal's knob count (2). */
    bool isInertSlotParameter (LuthierAudioProcessor& processor, const juce::String& parameterId);

    /** Words from the id ("circuit_treble_bleed" -> circuit, treble, bleed). */
    juce::StringArray idWords (const juce::String& parameterId);
}

//==============================================================================
class ParameterProvider : public SearchProvider
{
public:
    ParameterProvider (LuthierAudioProcessor& processor, SearchServices* services);

    juce::String getId() const override { return "param"; }
    juce::uint32 getGeneration() const override;
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override;
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;

private:
    LuthierAudioProcessor& processor;
    SearchServices* services;
};

class ChoiceOptionProvider : public SearchProvider
{
public:
    ChoiceOptionProvider (LuthierAudioProcessor& processor, SearchServices* services);

    juce::String getId() const override { return "opt"; }
    juce::uint32 getGeneration() const override;
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override;
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;

    /** Sets a parameter as a mouse edit would: begin, set, end (4.4). */
    static bool setAsGesture (juce::RangedAudioParameter& p, float normalised);

private:
    LuthierAudioProcessor& processor;
    SearchServices* services;
};

//==============================================================================
/** 3.2: the place catalogue. */
struct PlaceDef
{
    juce::String id;          ///< "place:tab:WORKSHOP"
    juce::String title;
    juce::String breadcrumb;
    UiLocation location;
    juce::String helpAlias;
    ParameterLocations::Gate gate = ParameterLocations::Gate::none;
};

class PlaceProvider : public SearchProvider
{
public:
    PlaceProvider (LuthierAudioProcessor& processor, SearchServices* services);

    /** The catalogue: every workspace tab, column, column section, Options
        page, practice drawer tab, overlay, Easy strip and tagged group. */
    static const std::vector<PlaceDef>& catalogue();

    juce::String getId() const override { return "place"; }
    juce::uint32 getGeneration() const override { return 1; }
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override;
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;

private:
    LuthierAudioProcessor& processor;
    SearchServices* services;
};

//==============================================================================
class CommandProvider : public SearchProvider
{
public:
    explicit CommandProvider (const ActionRegistry& registry);

    juce::String getId() const override { return "cmd"; }
    juce::uint32 getGeneration() const override;
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override;
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;
    juce::StringArray secondaryActions (const SearchItem&) const override;

private:
    const ActionRegistry& registry;
};

class ShortcutProvider : public SearchProvider
{
public:
    explicit ShortcutProvider (SearchServices* services);

    juce::String getId() const override { return "key"; }
    juce::uint32 getGeneration() const override;
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override { return Availability::available; }
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;

private:
    SearchServices* services;
};

//==============================================================================
class PresetProvider : public SearchProvider
{
public:
    PresetProvider (LuthierAudioProcessor& processor, SearchServices* services);

    juce::String getId() const override { return "preset"; }
    juce::uint32 getGeneration() const override;
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override;
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;
    juce::StringArray secondaryActions (const SearchItem&) const override;

    static juce::String idFor (const juce::String& name, bool factory);

private:
    int findPreset (const SearchItem&) const;

    LuthierAudioProcessor& processor;
    SearchServices* services;
};

class GuitarProvider : public SearchProvider
{
public:
    GuitarProvider (LuthierAudioProcessor& processor, SearchServices* services);

    juce::String getId() const override { return "guitar"; }
    juce::uint32 getGeneration() const override { return generation; }
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override;
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;
    juce::StringArray secondaryActions (const SearchItem&) const override;

    /** Rescans the guitar folders (the provider's cached list, 8: collect
        itself never touches disk). */
    void rescan();

    /** Loads a guitar file the way the header's guitar selector does: a
        factory guitar that a guitar type stands for through the parameter,
        anything else as parts. Undoable. */
    static bool loadGuitar (LuthierAudioProcessor& processor, const juce::File& file, const juce::String& reference);

private:
    struct Entry { juce::String reference; juce::File file; juce::String name, family; bool factory; };

    LuthierAudioProcessor& processor;
    SearchServices* services;
    std::vector<Entry> entries;
    juce::uint32 generation = 1;
};

class PartProvider : public SearchProvider
{
public:
    PartProvider (LuthierAudioProcessor& processor, SearchServices* services);

    juce::String getId() const override { return "part"; }
    juce::uint32 getGeneration() const override;
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override;
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;
    juce::StringArray secondaryActions (const SearchItem&) const override;

private:
    LuthierAudioProcessor& processor;
    SearchServices* services;
};

class PedalTypeProvider : public SearchProvider
{
public:
    explicit PedalTypeProvider (LuthierAudioProcessor& processor);

    juce::String getId() const override { return "pedal"; }
    juce::uint32 getGeneration() const override { return 1; }
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override;
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;

    /** The first empty slot of the pedal's default chain, or -1. */
    static int firstEmptySlot (LuthierAudioProcessor& processor, bool post);
    static bool defaultChainIsPost (int pedalTypeIndex);

    /** Adds the pedal as one undo step (action-and-undo 3.13). */
    static bool addPedal (LuthierAudioProcessor& processor, int pedalTypeIndex);

private:
    LuthierAudioProcessor& processor;
};

class SnapshotProvider : public SearchProvider
{
public:
    explicit SnapshotProvider (LuthierAudioProcessor& processor);

    juce::String getId() const override { return "snap"; }
    juce::uint32 getGeneration() const override;
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override;
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;

private:
    LuthierAudioProcessor& processor;
};

class HelpProvider : public SearchProvider
{
public:
    explicit HelpProvider (SearchServices* services);

    juce::String getId() const override { return "help"; }
    juce::uint32 getGeneration() const override { return 1; }
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override { return Availability::available; }
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;
    juce::StringArray secondaryActions (const SearchItem&) const override;

private:
    SearchServices* services;
};

class SettingProvider : public SearchProvider
{
public:
    explicit SettingProvider (SearchServices* services);

    juce::String getId() const override { return "set"; }
    juce::uint32 getGeneration() const override;
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override { return Availability::available; }
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;

private:
    SearchServices* services;
};

/** rhythm-engine 7 / global-search 12: genre kits ("funk" finds "Genre kit:
    Funk 16th"). Applied as Easy's kit list applies them. */
class GenreKitProvider : public SearchProvider
{
public:
    explicit GenreKitProvider (LuthierAudioProcessor& processor);

    juce::String getId() const override { return "kit"; }
    juce::uint32 getGeneration() const override;
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override { return Availability::available; }
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;

private:
    LuthierAudioProcessor& processor;
};

/** tune-builder / global-search 12: `.luthiertune` files - the factory
    templates and the user's tunes. Enter loads one into the Tune Builder and
    shows the TUNE tab. */
class TuneProvider : public SearchProvider
{
public:
    explicit TuneProvider (LuthierAudioProcessor& processor);

    juce::String getId() const override { return "tune"; }
    juce::uint32 getGeneration() const override { return generation; }
    void collect (std::vector<SearchItem>& out) const override;
    Availability availabilityOf (const SearchItem&) const override { return Availability::available; }
    bool activate (const SearchItem&, ActivationKind, SearchContext&) override;

    /** Rescans the folders (collect never touches disk, 8). */
    void rescan();

private:
    struct Entry { juce::String id, name; juce::File file; bool factory; };

    LuthierAudioProcessor& processor;
    std::vector<Entry> entries;
    juce::uint32 generation = 1;
};

/** The built-in providers, for a processor with (or without) an editor. */
void addDefaultProviders (class SearchIndex& index, LuthierAudioProcessor& processor,
                          SearchServices* services, const ActionRegistry* registry);

} // namespace luthier::search
