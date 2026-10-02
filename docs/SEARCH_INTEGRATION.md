# Making a feature findable by search

`spec/global-search.md` (FEAT-SEARCH) gives Luthier one search palette
(Ctrl/Cmd+K, the header magnifier, the HELP tab's Search field). Its index is
built from registries that already exist, so most features are found without
any search code. This page says what a feature owes the index, and how to
register the rest. The code is in `Source/UI/Search/`.

## What you get for free

| You add | Search finds it because | You also add |
|---|---|---|
| A parameter whose control is a `LuthierKnob`, `LuthierChoice`, `LuthierToggle`, `LuthierSlider` or `StandardValueChoice` (any `LearnTarget`) | `ParameterProvider` enumerates the APVTS; `LearnTarget`'s constructor registers the control in `LiveControls`, so the navigator finds it and derives its route by walking up its parents | Optional: synonyms in `SearchCatalog.cpp` (`search.syn.param:<id>`, `\|`-separated), and a `search.title.param:<id>` when the parameter's short name does not stand alone ("Gain" -> "Amp gain") |
| A choice parameter | `ChoiceOptionProvider` indexes every option as `opt:<id>:<index>` | Nothing |
| A command with a shortcut | Every `AccessibilitySettings` shortcut is a `cmd:` and a `key:` item, run through `LuthierAudioProcessorEditor::performAction` | Nothing, if the shortcut's branch is in `performAction` |
| A preset, factory guitar, part, genre kit, `.luthiertune`, snapshot or Help topic | The preset manager, guitar folders, `PartLibrary`, `GenreKitLibrary`, tune folders, `SnapshotBank`, `HelpContent` | Nothing |
| A labelled toggle, combo box or slider on an Options page | `SettingProvider` discovers it (`set:<PAGE>:<title>`) | Optional: `SearchAnchors::tagSetting (component, "set:PAGE:key", "catalog.key")` for a stable id and a catalog title |

A control inside a component that only exists while a popover is open (the
headstock `TuningPopover`, the `WhammyPopover`, Easy's rack slots) needs a row
in `ParameterLocations.cpp`. So does a parameter that does nothing unless Slide
Mode is on, a bass is loaded or a whammy is fitted (its *gate*).

## New places: tabs, sub-tabs, groups, drawer tabs, overlays

GS-03 fails until a new surface is both **tagged** and **catalogued**:

1. Tag the host component once, when it is built, with the function that brings
   it on screen:

   ```cpp
   #include "Search/LiveControls.h"
   search::SearchAnchors::tag (myGroup, "group:RHYTHM:JAM", [this] { showJamGroup(); }, "Jam");
   ```

   Column 4 tabs, Options pages, drawer tabs and the editor's overlays are
   tagged by `SearchNavigator::tagSurfaces()` already; a new tab added to
   `AdvancedPanel::buildWorkspace` is tagged automatically.

2. Add a row to `PlaceProvider::catalogue()` in `SearchProviders.cpp` (id,
   title, breadcrumb, `UiLocation`, help alias, gate) and synonyms under
   `search.syn.place:<...>` in `SearchCatalog.cpp`.

## New commands without a key

Register an `ActionDef` in `SearchNavigator::registerActions()` whose `perform`
calls `editor.performAction (id)`, and put the command's body in
`SearchNavigator::performExtendedAction` (or in `performAction` if it belongs
with the editor's own commands). Give it a title key (`search.cmd.<id>` in
`SearchCatalog.cpp`). The button that does the same thing should call
`performAction (id)` too, so there is one code path. A command whose feature
is not built is not registered.

## Your own content: a `SearchProvider`

For riffs, jam styles, preset-browser entries and anything else with its own
list, implement `search::SearchProvider` (`Source/UI/Search/SearchProvider.h`):

```cpp
struct RiffProvider : search::SearchProvider
{
    explicit RiffProvider (RiffLibrary& l) : library (l) {}

    juce::String getId() const override               { return "riff"; }
    juce::uint32 getGeneration() const override       { return library.getGeneration(); }   // bump when the list changes

    void collect (std::vector<search::SearchItem>& out) const override   // message thread, <= 5 ms / 2,000 items, no disk
    {
        for (auto& riff : library.getCachedRiffs())
        {
            search::SearchItem item;
            item.id = "riff:" + riff.id;              // stable: recent items are stored by id
            item.kind = search::ItemKind::provider;   // sorts after the built-in kinds; "#" scope
            item.title = riff.name;
            item.englishTitle = riff.name;
            item.breadcrumb = "Riffs > " + riff.key + " > " + juce::String (riff.tempo) + " bpm";
            item.keywords = riff.tags;
            out.push_back (std::move (item));
        }
    }

    search::Availability availabilityOf (const search::SearchItem&) const override
    {
        return search::Availability::available;       // asked on every query; never cache it
    }

    bool activate (const search::SearchItem& item, search::ActivationKind kind, search::SearchContext& context) override
    {
        if (kind == search::ActivationKind::secondary)
            return context.secondaryIndex == 0 ? audition (item) : insertIntoTune (item);

        return openInLibrary (item, context);         // context.openLocation (...) to show your place
    }

    juce::StringArray secondaryActions (const search::SearchItem&) const override
    {
        return { "Audition", "Insert into tune" };    // Alt+Enter / right-click
    }

    RiffLibrary& library;
};
```

Register it in `LuthierAudioProcessorEditor::buildSearchProviders()`
(`PluginEditor.cpp`), after the defaults:

```cpp
void LuthierAudioProcessorEditor::buildSearchProviders()
{
    searchNav->initialise();
    searchNav->getIndex().addProvider (std::make_unique<RiffProvider> (processor.getRiffLibrary()));   // riff-library
}
```

A provider with the same id replaces the earlier one. Duplicate item ids are
dropped (the later one) and logged; a debug build asserts.

`SearchContext` gives an activated provider the editor's services without the
editor: `openLocation` (run a `UiLocation`: mode, workspace tab, overlay,
Options page, drawer tab, popover, Workshop category), `performAction` (the one
action path), `showFooterMessage`, `postNotice` and `closePalette`.

### Per feature (global-search.md 8)

| Feature | Owes the index |
|---|---|
| Riff library | A `riff:` provider (title, key, tempo, tags); Enter opens the riff in the library; secondary actions Audition, Insert into tune |
| Preset browser (new) | Replace `PresetProvider`'s activation with "open browser at entry": register a provider with id `preset` (it replaces the built-in one) and keep the `preset:<factory\|user>/<name>` ids so recent items survive |
| Jam mode | Commands (start / stop jam, change key, change style) and its places |
| Mic placement | Its parameters are automatic. Add the place for the placement view and the command "Reset mic placement" |
| Auto-articulation | Its toggle parameter is automatic. Add an `ActionDef` "Toggle auto-articulation" |
| Animated strings | A `set:` setting on its Options toggle (`SearchAnchors::tagSetting`) |
| Output normalization, CPU quality modes | Parameters automatic; tag any new Options group's controls with `tagSetting` for stable ids |

## Tests that will tell you

- **GS-01** every automatable parameter is indexed (the only exemption is
  `ParameterVisibility::intentionallyHidden()`, shared with GuiReachabilityTests).
- **GS-02** every parameter navigates to an on-screen control of its own.
- **GS-03** every tab, Options page, column section, drawer tab, owned overlay
  and tagged group has a place row, and every row names a real surface.
- **GS-04** every shortcut is a command and a key item, and the palette and
  the key do the same thing.
- **GS-44** the provider contract (generation, duplicates, activation).
