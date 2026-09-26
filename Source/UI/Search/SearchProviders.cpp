#include "SearchProviders.h"
#include "SearchIndex.h"
#include "SearchCatalog.h"
#include "ParameterVisibility.h"

#include "../../PluginProcessor.h"
#include "../../Parameters.h"
#include "../../Presets/PresetManager.h"
#include "../../DSP/Effects/Pedal.h"
#include "../../Accessibility/Accessibility.h"
#include "../../Accessibility/Localisation.h"
#include "../../Practice/PracticeRoutine.h"
#include "../HelpContent.h"
#include "../../Rhythm/GenreKit.h"
#include "../../Tune/TuneTemplates.h"
#include "../../Tune/TuneSession.h"

namespace luthier::search
{
namespace
{
    using T = LocationStep::Type;

    juce::uint32 hashString (juce::uint32 h, const juce::String& s)
    {
        return h * 31u + (juce::uint32) s.hashCode();
    }

    juce::uint32 localeHash()
    {
        return (juce::uint32) Localisation::get().getLocale().hashCode();
    }

    /** Each pedal type's knob names, built once (message thread). */
    const std::vector<juce::StringArray>& pedalKnobNames()
    {
        static std::vector<juce::StringArray> names = []
        {
            std::vector<juce::StringArray> all;

            for (int t = 0; t < (int) PedalType::NumTypes; ++t)
            {
                juce::StringArray knobs;

                if (auto pedal = Pedal::create ((PedalType) t))
                    for (int i = 0; i < pedal->getNumParameters(); ++i)
                        knobs.add (pedal->getParameterDescriptor (i).name);

                all.push_back (knobs);
            }

            return all;
        }();

        return names;
    }

    int slotTypeIndex (LuthierAudioProcessor& processor, bool post, int slot)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (processor.getState().getParameter (ParamIDs::slotType (post, slot))))
            return c->getIndex();

        return 0;
    }

    juce::RangedAudioParameter* parameterFor (LuthierAudioProcessor& processor, const juce::String& id)
    {
        return processor.getState().getParameter (id);
    }

    juce::String capitalised (const juce::String& upper)
    {
        juce::StringArray words;
        words.addTokens (upper.toLowerCase(), " ", {});

        for (auto& w : words)
            if (w.isNotEmpty())
                w = w.substring (0, 1).toUpperCase() + w.substring (1);

        return words.joinIntoString (" ");
    }

    UiLocation location (std::initializer_list<LocationStep> steps)
    {
        UiLocation l;
        l.steps.assign (steps.begin(), steps.end());
        return l;
    }
}

//==============================================================================
juce::StringArray ParameterText::idWords (const juce::String& parameterId)
{
    juce::StringArray words;
    words.addTokens (parameterId.replaceCharacter ('_', ' '), " ", {});
    words.removeEmptyStrings();

    for (int i = words.size(); --i >= 0;)
        if (words[i].containsOnly ("0123456789") || words[i].length() < 2)
            words.remove (i);

    return words;
}

bool ParameterText::isInertSlotParameter (LuthierAudioProcessor& processor, const juce::String& parameterId)
{
    bool post = false;
    int slot = 0, param = 0;

    if (! ParameterLocations::parseSlotParameter (parameterId, post, slot, param))
        return false;

    const int type = slotTypeIndex (processor, post, slot);

    // An empty slot's bypass and mix do nothing either; its type is how a
    // pedal gets in, so that one always counts.
    if (param < 0)
        return type <= 0 && ! parameterId.endsWith ("_type");

    const auto& knobs = pedalKnobNames();

    return type <= 0 || type >= (int) knobs.size() || param >= knobs[(size_t) type].size();
}

juce::String ParameterText::titleFor (LuthierAudioProcessor& processor, const juce::RangedAudioParameter& p, bool english)
{
    const auto id = p.getParameterID();
    const auto key = "search.title.param:" + id;

    if (! english && SearchCatalog::isLocalised (key))
        return SearchCatalog::text (key);

    if (const auto t = SearchCatalog::english (key); t.isNotEmpty())
        return t;

    bool post = false;
    int slot = 0, param = 0;

    if (ParameterLocations::parseSlotParameter (id, post, slot, param))
    {
        const auto where = juce::String (post ? "Post " : "Pre ") + juce::String (slot + 1);

        if (param >= 0)
        {
            const int type = slotTypeIndex (processor, post, slot);
            const auto& knobs = pedalKnobNames();

            if (type > 0 && type < (int) knobs.size() && param < knobs[(size_t) type].size())
                return knobs[(size_t) type][param] + " (" + where + ")";

            return "Pedal knob " + juce::String (param + 1) + " (" + where + ")";
        }

        const auto tail = id.fromLastOccurrenceOf ("_", false, false);
        return "Pedal " + tail + " (" + where + ")";
    }

    auto name = p.getName (128).trim();
    return name.isNotEmpty() ? name : id;
}

//==============================================================================
ParameterProvider::ParameterProvider (LuthierAudioProcessor& p, SearchServices* s)
    : processor (p), services (s)
{
}

juce::uint32 ParameterProvider::getGeneration() const
{
    juce::uint32 h = localeHash();

    for (int chain = 0; chain < 2; ++chain)
        for (int s = 0; s < EffectsChain::kNumSlots; ++s)
            h = h * 131u + (juce::uint32) slotTypeIndex (processor, chain == 1, s);

    if (services != nullptr)
        h = h * 7u + services->getUiGeneration();

    return h == 0 ? 1u : h;
}

void ParameterProvider::collect (std::vector<SearchItem>& out) const
{
    for (auto* prm : processor.getParameters())
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (prm);

        if (p == nullptr)
            continue;

        const auto id = p->getParameterID();

        if (ParameterVisibility::isIntentionallyHidden (id))
            continue;

        SearchItem item;
        item.id = "param:" + id;
        item.kind = ItemKind::parameter;
        item.target = id;
        item.title = ParameterText::titleFor (processor, *p, false);
        item.englishTitle = ParameterText::titleFor (processor, *p, true);
        item.synonyms = SearchCatalog::synonyms (item.id);
        item.keywords = ParameterText::idWords (id);
        item.keywords.addArray (SearchCatalog::unitWords (p->getLabel()));
        item.hiddenByDefault = ParameterText::isInertSlotParameter (processor, id);

        if (services != nullptr)
            services->describeParameter (id, item.breadcrumb, item.inEasy, item.inAdvanced);

        out.push_back (std::move (item));
    }
}

Availability ParameterProvider::availabilityOf (const SearchItem& item) const
{
    if (services != nullptr && services->isProLocked (item))
        return Availability::proLocked;

    const auto gated = ParameterLocations::evaluate (ParameterLocations::gateFor (item.target), processor);

    if (gated != Availability::available)
        return gated;

    if (services != nullptr && ! item.inEasy && item.inAdvanced
          && ! services->isAdvancedMode() && ! services->isAdvancedModeAvailable())
        return Availability::modeUnavailable;

    return Availability::available;
}

bool ParameterProvider::activate (const SearchItem& item, ActivationKind kind, SearchContext&)
{
    return services != nullptr && services->goToParameter (item, kind);
}

//==============================================================================
ChoiceOptionProvider::ChoiceOptionProvider (LuthierAudioProcessor& p, SearchServices* s)
    : processor (p), services (s)
{
}

juce::uint32 ChoiceOptionProvider::getGeneration() const
{
    return localeHash() + 1u;
}

void ChoiceOptionProvider::collect (std::vector<SearchItem>& out) const
{
    for (auto* prm : processor.getParameters())
    {
        auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm);

        if (c == nullptr)
            continue;

        const auto id = c->getParameterID();
        bool post = false;
        int slot = 0, param = 0;

        // A pedal slot's type list is the pedal: kind, one row per type (2).
        if (ParameterVisibility::isIntentionallyHidden (id) || ParameterLocations::parseSlotParameter (id, post, slot, param))
            continue;

        const auto title = ParameterText::titleFor (processor, *c, false);
        const auto english = ParameterText::titleFor (processor, *c, true);

        for (int i = 0; i < c->choices.size(); ++i)
        {
            SearchItem item;
            item.id = "opt:" + id + ":" + juce::String (i);
            item.kind = ItemKind::choiceOption;
            item.target = id;
            item.index = i;
            item.title = title + ": " + c->choices[i];
            item.englishTitle = english + ": " + c->choices[i];
            item.synonyms = SearchCatalog::synonyms (item.id);
            item.breadcrumb = title;

            out.push_back (std::move (item));
        }
    }
}

Availability ChoiceOptionProvider::availabilityOf (const SearchItem& item) const
{
    if (services != nullptr && services->isProLocked (item))
        return Availability::proLocked;

    return ParameterLocations::evaluate (ParameterLocations::gateFor (item.target), processor);
}

bool ChoiceOptionProvider::setAsGesture (juce::RangedAudioParameter& p, float normalised)
{
    // The same path as a mouse edit (4.4): the host sees write-mode automation
    // and undo gets one entry.
    p.beginChangeGesture();
    p.setValueNotifyingHost (normalised);
    p.endChangeGesture();
    return true;
}

bool ChoiceOptionProvider::activate (const SearchItem& item, ActivationKind kind, SearchContext& context)
{
    auto* p = parameterFor (processor, item.target);

    if (p == nullptr)
        return false;

    if (kind == ActivationKind::goOnly)
        return services != nullptr && services->goToParameter (item, kind);

    const auto availability = availabilityOf (item);

    if (availability != Availability::available)
    {
        context.showFooterMessage (SearchCatalog::text ("search.noInlineSet", { { "name", item.title } }), true);
        return false;
    }

    setAsGesture (*p, p->convertTo0to1 ((float) item.index));
    context.showFooterMessage (SearchCatalog::text ("search.valueSet", { { "name", ParameterText::titleFor (processor, *p, false) },
                                                                      { "value", p->getCurrentValueAsText() } }), false);
    return true;
}

//==============================================================================
const std::vector<PlaceDef>& PlaceProvider::catalogue()
{
    static const std::vector<PlaceDef> places = []
    {
        std::vector<PlaceDef> v;
        using Gate = ParameterLocations::Gate;

        // gui-integration 4.4: column 4's tabs.
        for (const char* tab : { "WORKSHOP", "MOD", "RHYTHM", "TUNE", "JAM", "LIVE", "ROUTING", "TONE MATCH",   // JAM: FEAT-JAM
                                 "CHARACTER", "PRACTICE", "NOTATION", "MIDI OUT", "CONTROLLERS", "HELP" })
            v.push_back ({ juce::String ("place:tab:") + tab, capitalised (tab) + " tab", "Advanced > Column 4",
                           location ({ LocationStep::make (T::mode, "Advanced"), LocationStep::make (T::workspaceTab, tab) }),
                           tab, Gate::none });

        // 4.1-4.3: the three fixed columns and their sections.
        const char* columnNames[] = { "Instrument", "Signal capture", "Amplification" };

        for (int c = 1; c <= 3; ++c)
            v.push_back ({ "place:column:" + juce::String (c), "Column " + juce::String (c) + ": " + columnNames[c - 1],
                           "Advanced", location ({ LocationStep::make (T::mode, "Advanced"), LocationStep::make (T::column, {}, c) }),
                           {}, Gate::none });

        const struct { int column; const char* heading; } sections[] =
        {
            { 1, "Temperament" }, { 1, "Body" }, { 1, "Strings" }, { 1, "String Set" }, { 1, "Tuning Realism" },
            { 1, "Selected String" }, { 1, "Neck" }, { 1, "Sympathetic" }, { 1, "Bridge" },
            { 2, "Pickups" }, { 2, "Circuit" }, { 2, "Pedalboard (before the amp)" }, { 2, "Playing Hand" },
            { 2, "String Noise" },
            { 3, "Amplifier" }, { 3, "Effects Loop (after the amp)" }, { 3, "Cabinet and Mic" }, { 3, "Room" },
            { 3, "Sustain" }, { 3, "Performance" }, { 3, "Humanise" }, { 3, "Master" },
        };

        for (const auto& s : sections)
            v.push_back ({ juce::String ("place:section:") + juce::String (s.heading).toUpperCase(), s.heading,
                           "Advanced > Column " + juce::String (s.column),
                           location ({ LocationStep::make (T::mode, "Advanced"), LocationStep::make (T::column, {}, s.column),
                                       LocationStep::make (T::subTab, juce::String ("section|") + s.heading) }),
                           s.heading, Gate::none });

        // gui-integration 5: the Options pages.
        for (const char* page : { "AUDIO", "MIDI", "APPEARANCE", "ACCESSIBILITY", "LOCALIZATION", "EXPRESSION",
                                  "RANGES", "UPDATES", "PRIVACY", "DIAGNOSTICS", "FILE LOCATIONS" })
            v.push_back ({ juce::String ("place:options:") + page, "Options: " + capitalised (page), "Options",
                           location ({ LocationStep::make (T::overlay, "options"), LocationStep::make (T::optionsPage, page) }),
                           page, Gate::none });

        // practice-tools 9: the drawer's tabs.
        for (int t = 0; t < (int) PracticeTool::numTools; ++t)
        {
            const juce::String label (getPracticeToolTabLabel ((PracticeTool) t));
            v.push_back ({ "place:drawer:" + label, "Practice drawer: " + capitalised (label), "Practice drawer",
                           location ({ LocationStep::make (T::drawerTab, label) }), "practice", Gate::none });
        }

        // The overlays the editor owns. The hidden effect is not listed: it is
        // an easter egg (include.md), and a search row would give it away.
        const struct { const char* name; const char* title; const char* help; } overlays[] =
        {
            { "options", "Options", "options" }, { "presetBrowser", "Preset browser", "presets" },
            { "help", "Help", "help" }, { "export", "Export audio", "export" },
            { "chords", "Chords and tab", "chords" }, { "debug", "Debug tools", "debug" },
            { "saveAs", "Save preset as", "presets" }, { "workshop", "Workshop", "WORKSHOP" },
        };

        for (const auto& o : overlays)
            v.push_back ({ juce::String ("place:overlay:") + o.name, o.title, "Window",
                           location ({ LocationStep::make (T::overlay, o.name) }), o.help, Gate::none });

        // gui-integration 3: Easy Mode's strips.
        const struct { const char* id; const char* title; } strips[] =
        {
            { "illustration", "Guitar illustration" }, { "playing", "Playing strip" }, { "tone", "Tone strip" },
            { "rhythm", "Rhythm strip" }, { "rig", "Rig strip" },
        };

        for (const auto& s : strips)
            v.push_back ({ juce::String ("place:easy:") + s.id, s.title, "Easy",
                           location ({ LocationStep::make (T::mode, "Easy"), LocationStep::make (T::subTab, juce::String ("easy|") + s.id) }),
                           "easy", Gate::none });

        // Groups inside workspace tabs, tagged by the search bridge.
        const struct { const char* tab; const char* group; const char* title; Gate gate; } groups[] =
        {
            { "CHARACTER", "SETUP", "Setup group", Gate::none },
            { "CHARACTER", "NOISE", "Noise group", Gate::none },
            { "CHARACTER", "SLIDE", "Slide group", Gate::slideMode },
            { "CHARACTER", "SLAP",  "Slap group", Gate::bass },
            { "RHYTHM",    "STRUM", "Strum group", Gate::none },
            { "RHYTHM",    "BASS GRID", "Bass grid", Gate::bass },
            { "CHARACTER", "STRING AGING", "String aging", Gate::none },
            { "CHARACTER", "ENVIRONMENT", "Environment (humidity, temperature)", Gate::none },
            { "CHARACTER", "BODY COUPLING", "Body coupling", Gate::none },
            { "CHARACTER", "NOISE FLOOR", "Noise floor", Gate::none },
            { "CHARACTER", "SUSTAIN SHAPE", "Sustain shape", Gate::none },
            { "CHARACTER", "TUNING STABILITY", "Tuning stability", Gate::none },
            { "CHARACTER", "HARMONICS", "Harmonics", Gate::none },
            { "CHARACTER", "RIGHT HAND", "Right hand", Gate::none },
            { "CHARACTER", "STRING INTERACTION", "String interaction", Gate::none },
        };

        for (const auto& g : groups)
            v.push_back ({ juce::String ("place:group:") + g.tab + ":" + g.group, g.title,
                           "Advanced > " + capitalised (g.tab),
                           location ({ LocationStep::make (T::mode, "Advanced"), LocationStep::make (T::workspaceTab, g.tab),
                                       LocationStep::make (T::subTab, juce::String ("group|") + g.tab + ":" + g.group) }),
                           g.tab, g.gate });

        return v;
    }();

    return places;
}

PlaceProvider::PlaceProvider (LuthierAudioProcessor& p, SearchServices* s)
    : processor (p), services (s)
{
}

void PlaceProvider::collect (std::vector<SearchItem>& out) const
{
    for (const auto& place : catalogue())
    {
        SearchItem item;
        item.id = place.id;
        item.kind = ItemKind::place;
        item.title = place.title;
        item.englishTitle = place.title;
        item.breadcrumb = place.breadcrumb;
        item.synonyms = SearchCatalog::synonyms (place.id);
        item.target = place.helpAlias;
        item.location = place.location;

        for (const auto& step : place.location.steps)
            if (step.type == T::mode)
            {
                item.inEasy = step.name != "Advanced";
                item.inAdvanced = step.name != "Easy";
            }

        out.push_back (std::move (item));
    }
}

Availability PlaceProvider::availabilityOf (const SearchItem& item) const
{
    for (const auto& place : catalogue())
        if (place.id == item.id)
        {
            const auto gated = ParameterLocations::evaluate (place.gate, processor);

            if (gated != Availability::available)
                return gated;

            break;
        }

    if (services != nullptr && ! item.inEasy && ! services->isAdvancedMode() && ! services->isAdvancedModeAvailable())
        return Availability::modeUnavailable;

    return Availability::available;
}

bool PlaceProvider::activate (const SearchItem& item, ActivationKind, SearchContext&)
{
    return services != nullptr && services->openPlace (item);
}

//==============================================================================
CommandProvider::CommandProvider (const ActionRegistry& r) : registry (r) {}

juce::uint32 CommandProvider::getGeneration() const
{
    juce::uint32 h = registry.getGeneration() * 17u + localeHash();

    for (const auto& b : AccessibilitySettings::get().getShortcuts())
        h = hashString (h, b.key.getTextDescription());

    return h;
}

void CommandProvider::collect (std::vector<SearchItem>& out) const
{
    for (const auto& def : registry.getActions())
    {
        SearchItem item;
        item.id = "cmd:" + def.id;
        item.kind = ItemKind::command;
        item.target = def.id;
        item.title = ActionRegistry::titleOf (def);
        item.englishTitle = def.fixedTitle.isNotEmpty() ? def.fixedTitle : SearchCatalog::english (def.titleKey);
        item.synonyms = SearchCatalog::synonyms (item.id);

        if (const auto* binding = AccessibilitySettings::get().findShortcut (def.id))
            if (binding->key.isValid())
                item.keywords.add (binding->key.getTextDescription());

        out.push_back (std::move (item));
    }
}

Availability CommandProvider::availabilityOf (const SearchItem& item) const
{
    return registry.availabilityOf (item.target);
}

juce::StringArray CommandProvider::secondaryActions (const SearchItem&) const
{
    return { SearchCatalog::text ("search.action.run"), SearchCatalog::text ("search.action.showShortcut") };
}

bool CommandProvider::activate (const SearchItem& item, ActivationKind kind, SearchContext& context)
{
    if (kind == ActivationKind::secondary && context.secondaryIndex == 1)
        return context.performAction ("__showShortcut:" + item.target);

    // The one action path (4.3): the palette never has its own copy.
    return context.performAction (item.target);
}

//==============================================================================
ShortcutProvider::ShortcutProvider (SearchServices* s) : services (s) {}

juce::uint32 ShortcutProvider::getGeneration() const
{
    juce::uint32 h = localeHash();

    for (const auto& b : AccessibilitySettings::get().getShortcuts())
        h = hashString (hashString (h, b.id), b.key.getTextDescription());

    return h;
}

void ShortcutProvider::collect (std::vector<SearchItem>& out) const
{
    for (const auto& b : AccessibilitySettings::get().getShortcuts())
    {
        const auto keyText = b.key.isValid() ? b.key.getTextDescription() : juce::String ("(not bound)");
        const auto description = tr (b.descriptionKey) != b.descriptionKey ? tr (b.descriptionKey)
                                                                            : SearchCatalog::text (b.descriptionKey);

        SearchItem item;
        item.id = "key:" + b.id;
        item.kind = ItemKind::shortcut;
        item.target = b.id;
        item.title = (description.isNotEmpty() ? description : b.id) + "  " + keyText;
        item.englishTitle = item.title;
        item.keywords.add ("shortcut");
        item.keywords.add ("key");
        item.keywords.add (keyText);
        item.breadcrumb = "Options > Accessibility > Shortcuts";

        out.push_back (std::move (item));
    }
}

bool ShortcutProvider::activate (const SearchItem& item, ActivationKind, SearchContext&)
{
    return services != nullptr && services->openShortcutRow (item.target);
}

//==============================================================================
PresetProvider::PresetProvider (LuthierAudioProcessor& p, SearchServices* s) : processor (p), services (s) {}

juce::String PresetProvider::idFor (const juce::String& name, bool factory)
{
    return "preset:" + juce::String (factory ? "factory/" : "user/") + name;
}

juce::uint32 PresetProvider::getGeneration() const
{
    auto& presets = processor.getPresetManager();
    juce::uint32 h = (juce::uint32) presets.getNumPresets() + 1u;

    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (const auto* info = presets.getPreset (i))
            h = hashString (h, info->name + info->category);

    return h;
}

void PresetProvider::collect (std::vector<SearchItem>& out) const
{
    auto& presets = processor.getPresetManager();

    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        const auto* info = presets.getPreset (i);

        if (info == nullptr)
            continue;

        SearchItem item;
        item.id = idFor (info->name, info->isFactory);
        item.kind = ItemKind::preset;
        item.title = info->name;
        item.englishTitle = info->name;
        item.target = info->name;
        item.index = i;
        item.breadcrumb = juce::String (info->isFactory ? "Factory preset" : "User preset")
                            + (info->category.isNotEmpty() ? " > " + info->category : juce::String());
        item.keywords = info->tags;
        item.keywords.add ("preset");

        if (info->author.isNotEmpty())
            item.keywords.add (info->author);

        out.push_back (std::move (item));
    }
}

int PresetProvider::findPreset (const SearchItem& item) const
{
    auto& presets = processor.getPresetManager();

    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (const auto* info = presets.getPreset (i))
            if (idFor (info->name, info->isFactory) == item.id)
                return i;

    return -1;
}

Availability PresetProvider::availabilityOf (const SearchItem& item) const
{
    if (services != nullptr && services->isProLocked (item))
        return Availability::proLocked;

    return Availability::available;
}

juce::StringArray PresetProvider::secondaryActions (const SearchItem&) const
{
    return { SearchCatalog::text ("search.action.load"), SearchCatalog::text ("search.action.showInBrowser"),
             SearchCatalog::text ("search.action.reveal") };
}

bool PresetProvider::activate (const SearchItem& item, ActivationKind kind, SearchContext& context)
{
    const int index = findPreset (item);

    if (index < 0)
    {
        // 13: "{name} no longer exists", and the recent entry goes.
        context.showFooterMessage (SearchCatalog::text ("search.noLongerExists", { { "name", item.title } }), true);
        RecentStore::get().remove (item.id);
        return false;
    }

    const int action = kind == ActivationKind::secondary ? context.secondaryIndex : 0;

    if (action == 1)
        return services != nullptr && services->showPresetInBrowser (index);

    if (action == 2)
    {
        if (const auto* info = processor.getPresetManager().getPreset (index); info != nullptr && info->file.exists())
        {
            info->file.revealToUser();
            return true;
        }

        return false;
    }

    // The browser's load path (a state boundary, action-and-undo 5).
    processor.pushUndoState ("Load preset");
    processor.getPresetManager().loadPreset (index);
    processor.getParameterBridge().applyAllNow();
    return true;
}

//==============================================================================
GuitarProvider::GuitarProvider (LuthierAudioProcessor& p, SearchServices* s) : processor (p), services (s)
{
    rescan();
}

void GuitarProvider::rescan()
{
    const auto previous = entries;
    entries.clear();

    auto scan = [this] (const juce::File& root, bool factory)
    {
        if (! root.isDirectory())
            return;

        for (const auto& f : root.findChildFiles (juce::File::findFiles, true, "*.luthierguitar"))
        {
            Entry e;
            e.file = f;
            e.factory = factory;
            e.name = f.getFileNameWithoutExtension();
            e.family = f.getParentDirectory() == root ? juce::String() : f.getParentDirectory().getFileName();
            e.reference = juce::String (factory ? "Factory/" : "User/") + f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
            entries.push_back (e);
        }
    };

    scan (PartLibrary::getFactoryGuitarsFolder(), true);
    scan (PartLibrary::getUserGuitarsFolder(), false);

    std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b) { return a.reference < b.reference; });

    bool changed = previous.size() != entries.size();

    for (size_t i = 0; ! changed && i < entries.size(); ++i)
        changed = previous[i].reference != entries[i].reference;

    if (changed)
        ++generation;
}

void GuitarProvider::collect (std::vector<SearchItem>& out) const
{
    for (const auto& e : entries)
    {
        SearchItem item;
        item.id = "guitar:" + e.reference;
        item.kind = ItemKind::guitar;
        item.title = e.name;
        item.englishTitle = e.name;
        item.target = e.reference;
        item.breadcrumb = juce::String (e.factory ? "Factory guitar" : "User guitar")
                            + (e.family.isNotEmpty() ? " > " + e.family : juce::String());
        item.keywords.add ("guitar");

        if (e.family.isNotEmpty())
            item.keywords.add (e.family);

        out.push_back (std::move (item));
    }
}

Availability GuitarProvider::availabilityOf (const SearchItem& item) const
{
    if (services != nullptr && services->isProLocked (item))
        return Availability::proLocked;

    return Availability::available;
}

juce::StringArray GuitarProvider::secondaryActions (const SearchItem&) const
{
    return { SearchCatalog::text ("search.action.load"), SearchCatalog::text ("search.action.reveal") };
}

bool GuitarProvider::loadGuitar (LuthierAudioProcessor& processor, const juce::File& file, const juce::String& reference)
{
    // A guitar a guitar-type stands for goes through the parameter, as the
    // header's selector does (one undoable edit, and the type follows).
    if (auto* type = dynamic_cast<juce::AudioParameterChoice*> (processor.getState().getParameter (ParamIDs::guitarType)))
        for (int t = 0; t < type->choices.size(); ++t)
            if (const auto path = LuthierAudioProcessor::getFactoryGuitarPath ((GuitarType) t);
                path.isNotEmpty() && reference == "Factory/" + path)
                return ChoiceOptionProvider::setAsGesture (*type, type->convertTo0to1 ((float) t));

    WorkshopGuitar guitar;
    PartLibrary::LoadReport report;

    if (! processor.getPartLibrary().loadGuitar (file, guitar, report))
        return false;

    processor.pushUndoState ("Load guitar");
    processor.applyEditedGuitar (guitar);
    return true;
}

bool GuitarProvider::activate (const SearchItem& item, ActivationKind kind, SearchContext& context)
{
    const Entry* entry = nullptr;

    for (const auto& e : entries)
        if (e.reference == item.target)
            entry = &e;

    if (entry == nullptr || ! entry->file.existsAsFile())
    {
        context.showFooterMessage (SearchCatalog::text ("search.noLongerExists", { { "name", item.title } }), true);
        RecentStore::get().remove (item.id);
        rescan();
        return false;
    }

    if (kind == ActivationKind::secondary && context.secondaryIndex == 1)
    {
        entry->file.revealToUser();
        return true;
    }

    return loadGuitar (processor, entry->file, entry->reference);
}

//==============================================================================
PartProvider::PartProvider (LuthierAudioProcessor& p, SearchServices* s) : processor (p), services (s) {}

juce::uint32 PartProvider::getGeneration() const
{
    auto& library = processor.getPartLibrary();
    juce::uint32 h = (juce::uint32) library.getNumParts() + 3u;

    for (int t = 0; t < (int) PartType::numTypes; ++t)
    {
        const auto parts = library.getParts ((PartType) t);
        h = h * 29u + (juce::uint32) parts.size();

        if (! parts.isEmpty())
            h = h * 13u + (juce::uint32) (juce::pointer_sized_uint) parts.getFirst().get();
    }

    return h;
}

void PartProvider::collect (std::vector<SearchItem>& out) const
{
    auto& library = processor.getPartLibrary();

    for (int t = 0; t < (int) PartType::numTypes; ++t)
    {
        const auto type = (PartType) t;

        for (const auto& part : library.getParts (type))
        {
            if (part == nullptr)
                continue;

            SearchItem item;
            item.id = "part:" + juce::String (getPartTypeId (type)) + "/" + part->name;
            item.kind = ItemKind::part;
            item.title = part->name;
            item.englishTitle = part->name;
            item.target = getPartTypeId (type);
            item.index = t;
            item.breadcrumb = juce::String ("Parts > ") + getPartCategoryFolder (type);
            item.keywords = part->tags;
            item.keywords.add (getPartTypeId (type));
            item.keywords.add ("part");

            if (part->author.isNotEmpty())
                item.keywords.add (part->author);

            out.push_back (std::move (item));
        }
    }
}

Availability PartProvider::availabilityOf (const SearchItem& item) const
{
    if (services != nullptr && services->isProLocked (item))
        return Availability::proLocked;

    return Availability::available;
}

juce::StringArray PartProvider::secondaryActions (const SearchItem&) const
{
    return { SearchCatalog::text ("search.action.showInWorkshop"), SearchCatalog::text ("search.action.fit"),
             SearchCatalog::text ("search.action.reveal") };
}

bool PartProvider::activate (const SearchItem& item, ActivationKind kind, SearchContext& context)
{
    const auto type = partTypeFromId (item.target);
    const auto name = item.title;
    auto part = processor.getPartLibrary().find (type, name);

    if (part == nullptr)
    {
        context.showFooterMessage (SearchCatalog::text ("search.noLongerExists", { { "name", item.title } }), true);
        RecentStore::get().remove (item.id);
        return false;
    }

    const int action = kind == ActivationKind::secondary ? context.secondaryIndex : 0;

    if (action == 1)
    {
        // Fit to this guitar: the bench's undoable part swap (action-and-undo 3.4).
        auto& bench = processor.getBench();

        if (type == PartType::pick || type == PartType::slide || type == PartType::capo)
            return bench.fitAccessory (part);

        for (int s = 0; s < kNumGuitarSlots; ++s)
            if (getSlotPartType ((GuitarSlot) s) == type)
                return bench.fit ((GuitarSlot) s, part);

        return false;
    }

    if (action == 2)
    {
        if (part->file.exists())
        {
            part->file.revealToUser();
            return true;
        }

        return false;
    }

    return services != nullptr && services->openWorkshopOn (type, name);
}

//==============================================================================
PedalTypeProvider::PedalTypeProvider (LuthierAudioProcessor& p) : processor (p) {}

bool PedalTypeProvider::defaultChainIsPost (int pedalTypeIndex)
{
    return ! Pedal::isPreAmpPedal ((PedalType) pedalTypeIndex);
}

int PedalTypeProvider::firstEmptySlot (LuthierAudioProcessor& processor, bool post)
{
    for (int s = 0; s < EffectsChain::kNumSlots; ++s)
        if (slotTypeIndex (processor, post, s) == 0)
            return s;

    return -1;
}

bool PedalTypeProvider::addPedal (LuthierAudioProcessor& processor, int pedalTypeIndex)
{
    const bool post = defaultChainIsPost (pedalTypeIndex);
    const int slot = firstEmptySlot (processor, post);
    auto* type = parameterFor (processor, ParamIDs::slotType (post, slot));

    if (slot < 0 || type == nullptr)
        return false;

    // action-and-undo 3.13: one entry for the add.
    LuthierAudioProcessor::ScopedUndoAction undo (processor, "Add " + juce::String (Pedal::getTypeName ((PedalType) pedalTypeIndex)) + " pedal");
    ChoiceOptionProvider::setAsGesture (*type, type->convertTo0to1 ((float) pedalTypeIndex));
    return true;
}

void PedalTypeProvider::collect (std::vector<SearchItem>& out) const
{
    for (int t = 1; t < (int) PedalType::NumTypes; ++t)
    {
        const juce::String name (Pedal::getTypeName ((PedalType) t));

        SearchItem item;
        item.id = "pedal:" + name;
        item.kind = ItemKind::pedal;
        item.title = name;
        item.englishTitle = name;
        item.index = t;
        item.target = name;
        item.breadcrumb = defaultChainIsPost (t) ? "Pedal > after the amp" : "Pedal > before the amp";
        item.keywords.add ("pedal");
        item.keywords.add ("effect");
        item.synonyms = SearchCatalog::synonyms (item.id);

        out.push_back (std::move (item));
    }
}

Availability PedalTypeProvider::availabilityOf (const SearchItem& item) const
{
    return firstEmptySlot (processor, defaultChainIsPost (item.index)) < 0 ? Availability::needsEmptySlot
                                                                           : Availability::available;
}

bool PedalTypeProvider::activate (const SearchItem& item, ActivationKind, SearchContext& context)
{
    if (availabilityOf (item) != Availability::available)
    {
        context.showFooterMessage (SearchCatalog::text ("search.needsEmptySlot"), true);
        return false;
    }

    if (! addPedal (processor, item.index))
        return false;

    context.showFooterMessage (SearchCatalog::text ("search.cmd.addPedal", { { "pedal", item.title } }), false);
    return true;
}

//==============================================================================
SnapshotProvider::SnapshotProvider (LuthierAudioProcessor& p) : processor (p) {}

juce::uint32 SnapshotProvider::getGeneration() const
{
    auto& bank = processor.getSnapshots();
    juce::uint32 h = (juce::uint32) bank.getNumSnapshots() + 5u;

    for (int i = 0; i < bank.getNumSnapshots(); ++i)
        h = hashString (h, bank.getSnapshot (i).label);

    return h;
}

void SnapshotProvider::collect (std::vector<SearchItem>& out) const
{
    auto& bank = processor.getSnapshots();

    for (int i = 0; i < bank.getNumSnapshots(); ++i)
    {
        const auto& snap = bank.getSnapshot (i);

        SearchItem item;
        item.id = "snap:" + juce::String (i);
        item.kind = ItemKind::snapshot;
        item.index = i;
        item.title = "Snapshot " + juce::String (i + 1) + (snap.label.isNotEmpty() ? ": " + snap.label : juce::String());
        item.englishTitle = item.title;
        item.breadcrumb = "Live > Snapshots";
        item.keywords.add ("snapshot");

        out.push_back (std::move (item));
    }
}

Availability SnapshotProvider::availabilityOf (const SearchItem& item) const
{
    return juce::isPositiveAndBelow (item.index, processor.getSnapshots().getNumSnapshots())
             ? Availability::available : Availability::notBuilt;
}

bool SnapshotProvider::activate (const SearchItem& item, ActivationKind, SearchContext&)
{
    // action-and-undo 3.7, through the same call the digit keys use.
    return processor.recallSnapshot (item.index);
}

//==============================================================================
HelpProvider::HelpProvider (SearchServices* s) : services (s) {}

void HelpProvider::collect (std::vector<SearchItem>& out) const
{
    for (int i = 0; i < HelpContent::getNumTopics(); ++i)
    {
        const auto& topic = HelpContent::getTopic (i);

        SearchItem item;
        item.id = juce::String ("help:") + topic.id;
        item.kind = ItemKind::help;
        item.target = topic.id;
        item.index = i;
        // The row's "?" glyph and "Help" breadcrumb say what it is; the title
        // stays the topic's own, so "help" does not tie every topic (GS-05).
        item.title = topic.title;
        item.englishTitle = item.title;
        item.breadcrumb = "Help";
        item.synonyms = HelpContent::getAliases (topic);   // an alias is another name (2)
        item.synonymsAreNames = true;
        item.keywords.add (topic.id);

        out.push_back (std::move (item));
    }
}

juce::StringArray HelpProvider::secondaryActions (const SearchItem&) const
{
    return { SearchCatalog::text ("search.action.open"), SearchCatalog::text ("search.action.openPinned") };
}

bool HelpProvider::activate (const SearchItem& item, ActivationKind kind, SearchContext& context)
{
    const bool pinned = kind == ActivationKind::secondary && context.secondaryIndex == 1;
    return services != nullptr && services->openHelpTopic (item.target, pinned);
}

//==============================================================================
SettingProvider::SettingProvider (SearchServices* s) : services (s) {}

juce::uint32 SettingProvider::getGeneration() const
{
    return (services != nullptr ? services->getUiGeneration() : 0u) + localeHash() + 11u;
}

void SettingProvider::collect (std::vector<SearchItem>& out) const
{
    if (services == nullptr)
        return;

    for (const auto& s : services->getSettings())
    {
        SearchItem item;
        item.id = s.id;
        item.kind = ItemKind::setting;
        item.title = s.title;
        item.englishTitle = s.title;
        item.target = s.page;
        item.breadcrumb = "Options > " + capitalised (s.page);
        item.synonyms = SearchCatalog::synonyms (s.id);
        item.keywords.add ("setting");
        item.keywords.add ("option");

        out.push_back (std::move (item));
    }
}

bool SettingProvider::activate (const SearchItem& item, ActivationKind, SearchContext&)
{
    return services != nullptr && services->openSetting (item);
}

//==============================================================================
GenreKitProvider::GenreKitProvider (LuthierAudioProcessor& p) : processor (p) {}

juce::uint32 GenreKitProvider::getGeneration() const
{
    juce::uint32 h = 17u + (juce::uint32) processor.getGenreKits().getNumKits();

    for (const auto& name : processor.getGenreKits().getNames())
        h = hashString (h, name);

    return h;
}

void GenreKitProvider::collect (std::vector<SearchItem>& out) const
{
    auto& kits = processor.getGenreKits();

    for (int i = 0; i < kits.getNumKits(); ++i)
    {
        const auto& kit = kits.getKit (i);

        SearchItem item;
        item.id = "kit:" + kit.name;
        item.kind = ItemKind::provider;
        item.index = i;
        item.target = kit.name;
        item.title = "Genre kit: " + kit.name;
        item.englishTitle = item.title;
        item.breadcrumb = "Rhythm";
        item.keywords = kit.tags;
        item.keywords.add ("rhythm");
        item.keywords.add ("style");

        out.push_back (std::move (item));
    }
}

bool GenreKitProvider::activate (const SearchItem& item, ActivationKind, SearchContext& context)
{
    const int index = processor.getGenreKits().indexOf (item.target);

    if (index < 0)
    {
        context.showFooterMessage (SearchCatalog::text ("search.noLongerExists", { { "name", item.title } }), true);
        RecentStore::get().remove (item.id);
        return false;
    }

    // As Easy's kit list does: choosing a style is a request to hear it.
    processor.applyGenreKit (index);
    processor.getEngine().getRhythmEngine().setEnabled (true);
    return true;
}

//==============================================================================
TuneProvider::TuneProvider (LuthierAudioProcessor& p) : processor (p)
{
    rescan();
}

void TuneProvider::rescan()
{
    std::vector<Entry> found;

    auto scan = [&found] (const juce::File& folder, bool factory)
    {
        if (! folder.isDirectory())
            return;

        for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, juce::String ("*") + TuneFile::kFileExtension))
            found.push_back ({ juce::String ("tune:") + (factory ? "factory/" : "user/")
                                 + f.getRelativePathFrom (folder).replaceCharacter ('\\', '/'),
                               f.getFileNameWithoutExtension(), f, factory });
    };

    scan (TuneTemplateLibrary::getFactoryDirectory(), true);
    scan (TuneFile::getUserDirectory(), false);

    std::sort (found.begin(), found.end(), [] (const Entry& a, const Entry& b) { return a.id < b.id; });

    bool changed = found.size() != entries.size();

    for (size_t i = 0; ! changed && i < found.size(); ++i)
        changed = found[i].id != entries[i].id;

    if (changed)
    {
        entries = std::move (found);
        ++generation;
    }
}

void TuneProvider::collect (std::vector<SearchItem>& out) const
{
    for (const auto& e : entries)
    {
        SearchItem item;
        item.id = e.id;
        item.kind = ItemKind::provider;
        item.target = e.file.getFullPathName();
        item.title = e.name;
        item.englishTitle = e.name;
        item.breadcrumb = e.factory ? "Tune template" : "Tune";
        item.keywords.add ("tune");
        item.keywords.add ("song");

        out.push_back (std::move (item));
    }
}

bool TuneProvider::activate (const SearchItem& item, ActivationKind, SearchContext& context)
{
    const juce::File file (item.target);
    juce::String error;

    if (! file.existsAsFile())
    {
        context.showFooterMessage (SearchCatalog::text ("search.noLongerExists", { { "name", item.title } }), true);
        RecentStore::get().remove (item.id);
        rescan();
        return false;
    }

    if (! processor.getTuneSession().load (file, error))
    {
        context.showFooterMessage (error, true);
        return false;
    }

    UiLocation tuneTab;
    tuneTab.steps.push_back (LocationStep::make (LocationStep::Type::mode, "Advanced"));
    tuneTab.steps.push_back (LocationStep::make (LocationStep::Type::workspaceTab, "TUNE"));
    context.openLocation (tuneTab, item.title);
    return true;
}

//==============================================================================
void addDefaultProviders (SearchIndex& index, LuthierAudioProcessor& processor,
                          SearchServices* services, const ActionRegistry* registry)
{
    index.addProvider (std::make_unique<ParameterProvider> (processor, services));
    index.addProvider (std::make_unique<ChoiceOptionProvider> (processor, services));
    index.addProvider (std::make_unique<PlaceProvider> (processor, services));

    if (registry != nullptr)
        index.addProvider (std::make_unique<CommandProvider> (*registry));

    index.addProvider (std::make_unique<ShortcutProvider> (services));
    index.addProvider (std::make_unique<PresetProvider> (processor, services));
    index.addProvider (std::make_unique<GuitarProvider> (processor, services));
    index.addProvider (std::make_unique<PartProvider> (processor, services));
    index.addProvider (std::make_unique<PedalTypeProvider> (processor));
    index.addProvider (std::make_unique<SnapshotProvider> (processor));
    index.addProvider (std::make_unique<HelpProvider> (services));
    index.addProvider (std::make_unique<SettingProvider> (services));
    index.addProvider (std::make_unique<GenreKitProvider> (processor));
    index.addProvider (std::make_unique<TuneProvider> (processor));
}

} // namespace luthier::search
