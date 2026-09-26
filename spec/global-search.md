# GLOBAL SEARCH AND COMMAND PALETTE SPEC

One box that finds anything in Luthier and takes you there: every
parameter, every place (tab, sub-tab, group, drawer, overlay, Options
page), every command, every preset / guitar / part / riff, every help
topic and every keyboard shortcut. Opened with `Ctrl+K` / `Cmd+K` or a
magnifier in the header. Picking a result goes to the control, runs the
command or loads the item. Typing a value after a parameter name
("gain 7") sets the parameter without leaving the palette.

Added 2026-09-24. Additive to `gui-integration.md`: it adds one header
button, one shortcut and one layer, and moves nothing. It implements
gui-integration 0.2 ("reachable in at most three interactions") in a
way that stays true as features are added. Test ID prefix: **GS-**.

## 0. Ground rules

1. **UI only.** No new parameters, no audio-thread code, nothing in
   presets. The palette reads parameter values through
   `getRawParameterValue` atomics, like every other panel (ui-wiring 4.1).
2. **One action path.** A command run from the palette and the same
   command run from its shortcut or button call the same function. The
   palette never has its own copy of "save" or "panic" (section 5.3).
3. **The index cannot fall out of date without failing a test.** Items
   come from registries that already exist: the APVTS parameter list,
   the workspace tab strip, the Options pages, `AccessibilitySettings`
   shortcuts, `HelpContent`, `PresetManager`, `PartLibrary`. A new tab
   or parameter is indexed without anyone editing a search list. GS-01
   to GS-06 fail if something slips through.
4. **Search moves you. It does not change sound by surprise.** Setting a
   value needs an explicit value typed and Enter (section 4.4). When a
   query could be read as either "go there" or "set a value" with equal
   confidence, it is treated as navigation.
5. **Keyboard first, mouse equal, screen reader complete.** Everything
   the palette does works with the keyboard alone and is announced
   (accessibility.md 1, 2).

## 1. User stories

- A player hears too much top end on the neck pickup with the volume
  down. They type "treble bleed", press Enter and land on the CIRCUIT
  treble-bleed dropdown in Advanced column 2, focused and flashing.
- An Easy-Mode user types "reverb". They get the ROOM wet/dry control,
  the command "Add Reverb pedal" and the Help topic on the rig.
- A power user types "gain 7", presses Enter, and the amp gain is 7.0.
  One undo entry, and the host records the move like a mouse edit.
- A screen-reader user presses Ctrl+K, types "palm", hears "4 results,
  first: Palm mute amount, Techniques, Mute", and presses Enter. Focus
  lands on the control, which then announces its own label and value.
- A user who has forgotten a key types "> slide" and sees "Toggle Slide
  Mode  S".
- A new user presses Ctrl+K with nothing typed and sees their recent
  items plus suggestions for the tab they are on.

## 2. What is indexed

Each entry is a `SearchItem` with a stable id, a kind, a localized
title, keywords, a breadcrumb, an availability and a location.

| Kind | Id form | Source (real code) | Primary action (Enter) |
|---|---|---|---|
| Parameter | `param:<paramId>` | `processor.getParameters()` (APVTS) | Go to its control; set the value if one was typed |
| Choice option | `opt:<paramId>:<index>` | choice parameters' option lists (`Parameters::ampModelNames()` etc.) | Set that option |
| Place | `place:<surface>:<name>` | Section 3.2 place catalogue, checked against `AdvancedPanel::getWorkspaceTabName`, `OptionsPanel::getPageNames`, `Column` section headings, `PracticePanel` tabs | Open it |
| Command | `cmd:<actionId>` | `ActionRegistry` (5.3). Contains every `AccessibilitySettings` shortcut id | Run it |
| Shortcut | `key:<actionId>` | `AccessibilitySettings::getShortcuts()` | Open the rebind row in Options -> ACCESSIBILITY |
| Preset | `preset:<factory|user>/<name>` | `PresetManager::getPreset(i)`, categories | Load (state boundary) |
| Guitar | `guitar:<relativePath>` | `PartLibrary::getFactoryGuitarsFolder/getUserGuitarsFolder` | Load the guitar |
| Part | `part:<type>/<name>` | `PartLibrary::getParts(type)` | Open the Workshop on its category with the card focused |
| Pedal type | `pedal:<typeName>` | `Parameters::pedalTypeNames()` | Add it to the first empty slot of its default chain |
| Snapshot | `snap:<index>` | `processor.getSnapshots()` | Recall it |
| Help topic | `help:<topicId>` | `HelpContent::getTopic(i)`, including aliases | Open Help on that topic |
| Setting | `set:<page>:<key>` | Options page controls that are not parameters (reduced motion, palette, tooltips...) | Open the page and focus the control |
| Riff, Tune, Jam item... | `<provider>:<id>` | Parallel feature specs, via `SearchProvider` (section 8) | Set by the provider |

**Keywords** for each item:
- The English title, so English tutorials work in every locale. Matches
  on it score x0.8 in a non-English locale.
- Localized synonyms from the catalog key `search.syn.<itemId>`, written
  as `|`-separated values. For example, `search.syn.param:amp_gain` =
  `drive|distortion|overdrive|dirt`. English ships a curated set for
  every parameter and place. Other locales fall back to it.
- The unit, spelled out: dB -> "decibel", Hz -> "hertz frequency",
  ms -> "time milliseconds", st -> "semitone".
- The breadcrumb words ("amp", "column 3").

**Pedal slot parameters** (`pre3_p2` etc., `Parameters.cpp` 697-703) get
their title from the loaded pedal: `Pedal::getParameterDescriptor(i).name`
plus the slot, e.g. "Delay time (Post 3)". An empty slot's `_pN`
parameters are left out of the default results, because they do nothing
(gui-integration 0.7). GS-01 still counts them as indexed.

**Parameters hidden on purpose.** The list in
`GuiReachabilityTests.cpp::intentionallyHidden()` moves to a shared
`Source/UI/Search/ParameterVisibility.h`, used by both that test and
the index. Those parameters are not indexed. It is the only exemption.

## 3. Registry design

### 3.1 Classes (new, `Source/UI/Search/`)

```
SearchItem.h        struct SearchItem, enum class ItemKind, enum class Availability,
                    struct UiLocation (vector<LocationStep>)
SearchMatcher.h/.cpp  normalise(), tokenise(), score(); no JUCE GUI dependency
SearchIndex.h/.cpp    owns providers, flat item vector, query(), recent-use store
SearchProviders.h/.cpp  ParameterProvider, ChoiceOptionProvider, PlaceProvider,
                    CommandProvider, ShortcutProvider, PresetProvider, GuitarProvider,
                    PartProvider, PedalTypeProvider, SnapshotProvider, HelpProvider,
                    SettingProvider
ActionRegistry.h/.cpp   every command: id, title key, availability, perform()
ParameterLocations.h/.cpp  declarative rules for controls that exist only when opened
LiveControls.h/.cpp     process-wide weak set of LearnTarget* (message thread)
SearchNavigator.h/.cpp  runs a UiLocation, scrolls, focuses, SearchHighlighter
InlineValue.h/.cpp      parses "7", "+1", "50%", "442 Hz", "on", "max", option text
CommandPalette.h/.cpp   the component (section 6)
```

`SearchIndex` is owned by `LuthierAudioProcessorEditor`, so each plugin
instance has its own. It holds non-owning references to the processor's
managers. It lives on the message thread only.

### 3.2 How controls register

- **Parameters: automatically.** `ParameterProvider` enumerates the
  APVTS. Nothing registers by hand.
- **Where a parameter's control is: found live, with a table as
  backup.** `LearnTarget` (Widgets.h) is the base of `LuthierKnob`,
  `LuthierChoice`, `LuthierToggle` and `LuthierSlider`. It gains a
  constructor and destructor that add and remove `this` from
  `LiveControls`. This is the single insertion point, and it covers
  every widget, including the rebuilt pedal-card knobs. To navigate to a
  parameter, the navigator asks `LiveControls` for the targets that have
  that `getLearnParameterId()` and that sit inside this editor
  (`editor.isParentOf`). It then picks one (section 4.2) and derives the
  path by walking up the parents and reading their **place tags**.
- **Place tags.** `SearchAnchors::tag (juce::Component&, placeId)` sets
  the component property `luthier.place`. The following call it once
  when they build:
  - `AdvancedPanel::buildWorkspace` (`tab:<NAME>` on each panel) and
    `buildColumn1..3` (`column:<n>`).
  - `EasyPanel` (`easy:<strip>`).
  - `OptionsPanel` (`options:<PAGE>`).
  - `PracticePanel` (`drawer:<TAB>`).
  - `LiveStrip`, `HeaderBar`, `WorkshopOverlay`, `HelpPanel`,
    `PresetBrowserPanel`, `ExportPanel`, `ChordAndTabPanel`.
  - Sub-tab and group hosts: the CHARACTER groups, the TECHNIQUES
    sub-tabs and the RHYTHM STRUM group.

  `Column::getSectionContaining` supplies the section heading
  ("CIRCUIT") for column controls. Sections are not components.
- **Controls that exist only when opened.** These are popovers: the
  headstock `TuningPopover`, the `WhammyPopover`, `CompactRack` slot
  popovers in Easy. For them, `ParameterLocations.cpp` holds rows of the
  form `{ idPattern, UiLocation, availabilityRule }`, for example
  `{ "tuning_*", Easy / illustration / popover "headstock" }`. The
  navigator runs the steps, which creates the control, then looks the
  control up in `LiveControls`.
- **Places** are a static catalogue in `SearchProviders.cpp`: title
  key, synonyms, `UiLocation` and help alias. GS-03 checks the
  catalogue against the real tab strip, page list, column headings and
  drawer tabs, in both directions.
- **Non-parameter settings** (Options toggles) register with
  `SearchAnchors::tagSetting (component, "set:APPEARANCE:reducedMotion",
  titleKey)`.

### 3.3 `UiLocation`

This is an ordered list of steps. Each step is one of:
- `mode` (Easy / Advanced / Either)
- `workspaceTab` (name, via `AdvancedPanel::setWorkspaceTabNamed`)
- `subTab` (host place id + name)
- `column` (1-3)
- `overlay` (options / presetBrowser / workshop / help / export / chords)
- `optionsPage` (`OptionsPanel::showPageNamed`)
- `drawerTab` (`PracticePanel::setOpen` + `showTab`)
- `popover` (headstock -> `GuitarBodyComponent::showTuningPopover`,
  bridge -> `showWhammyPopover`, `rack:<pre|post>:<slot>` ->
  `CompactRack::openSlot`)
- `workshopCategory` (`WorkshopPanel::showCategory`)

Each step names its "open" function. There are no string-switch
dispatches.

### 3.4 Availability

Every item carries one of:
- `available`
- `needsSlideMode` (SLIDE group, gui-integration 7)
- `needsBass` (SLAP / bass grid)
- `needsWhammy` (WHAMMY panel)
- `needsEmptySlot` (pedal add)
- `modeUnavailable` (Advanced below 1000 px, or locked by Live Mode)
- `proLocked` (Free edition, section 10)
- `notBuilt`

A provider computes this when asked, never caching it, because
guitar, mode and width change under it.

## 4. Behaviour

### 4.1 Matching and ranking (`SearchMatcher`)

**Normalisation.** Case fold, fold Latin diacritics, treat `-`, `_`,
`/` and `›` as spaces, and collapse whitespace. This is the same rule as
`HelpContent::findTopic`, factored out and shared by both. For CJK
scripts every character counts as a word start.

**Tokens.** The query splits on spaces. Every token must match (AND),
anywhere in title, keywords or breadcrumb. Per token, the best of these
scores is used:

| Match | Score |
|---|---|
| Whole title equals query | 1000 |
| Title starts with query | 800 |
| Token is a word-start prefix in the title | 600 (+50 if tokens are in title order) |
| Acronym of title words ("tb" -> Treble Bleed) | 500 |
| Synonym exact / prefix | 450 / 400 |
| Breadcrumb word-start | 300 |
| Substring in title | 250 |
| Typo within Damerau-Levenshtein 1 (token >= 4 chars) or 2 (>= 8 chars) | 200 |
| Ordered subsequence (fuzzy), minus 10 per gap, floor 100 | 100-240 |

**Item score.** The mean of the token scores, plus these bonuses:
- Recent use: `150 * 0.5^(days since last use / 7)`.
- Frequency: `20 * log2(1 + uses)`, capped at 100.
- Visible in the current mode without switching: +40.
- Available: +30. Locked or `notBuilt`: -100.

**Filtering.** Items below 150 are dropped. Results are capped at 50.

**Ties** are broken by kind order (Parameter, Place, Command, Preset,
Guitar, Part, Pedal, Snapshot, Help, Shortcut, Setting, provider kinds),
then by title, then by id. The ordering is fully deterministic.

**Scopes.** A leading character limits the kinds searched: `>` commands
and shortcuts, `?` help, `#` content (presets, guitars, parts, pedals,
riffs, tunes), `@` places, `=` parameters only. The same scopes are
chips under the field: All, Controls, Places, Commands, Content, Help.

### 4.2 Navigation (`SearchNavigator::goTo`)

1. **Choose a target control.** Prefer a control that is reachable in
   the current mode, whether it is the canonical control or a mirror
   (gui-integration 0.1). Only if there is none, use the canonical
   location: the `ParameterLocations` row, or else the Advanced control.
2. **Mode (decision: switch automatically; do not ask).** If the target
   is only in the other mode, the navigator calls the editor's
   `setAdvancedMode`. It also posts the `InlineNotice` "Switched to
   Advanced Mode to show {name}. {key:toggleAdvanced} returns." Reason:
   a mode switch costs nothing, is reversible with one key, and is not
   undoable (action-and-undo 3.17). An "ask" dialog would add a fourth
   interaction and break gui-integration 0.2. Users who prefer to be
   asked can turn this off (section 7). Then the row reads "Opens in
   Advanced Mode", and the first Enter confirms it inline.
3. **If the target mode is unavailable** (window under 1000 px,
   `isAdvancedModeAvailable()` false, or Live Mode locking the toggle),
   do not switch. The palette stays open on that row with the inline
   adjuster (4.4) and the text "Advanced Mode is unavailable at this
   width. Adjust here, or widen the window." A place result just shows
   the notice.
4. **Context gates.**
   - `needsSlideMode`: the row reads "Slide Mode is off. Enter turns it
     on and shows the control." A second Enter calls
     `HeaderBar::toggleSlideMode`, which is undoable (action-and-undo
     3.3), and then navigates.
   - `needsBass` / `needsWhammy`: navigation is refused with "Appears on
     guitars with {part}. Open Workshop -> {category}?". Enter opens the
     Workshop on the Bridge (whammy) or Body (family) category.
5. **Run the location steps outer to inner.** If an overlay other than
   the target's is up, dismiss it first (`OverlayHost::dismiss`).
6. **Scroll into view.** For every `juce::Viewport` ancestor (Advanced
   column viewports, `workspaceViewport`, Options page viewports), set
   the view position so the control's bounds plus a 24 px margin are
   visible, with the least movement.
7. **Focus.** Call `grabKeyboardFocus` on the inner control (the slider,
   box or button, as the reachability walk's `innerControl` does). Arrow
   keys then adjust it (accessibility 2).
8. **Highlight.** `SearchHighlighter` is a mouse-transparent top layer.
   It draws a 2 px accent ring (`theme.md` accent, 4 px outset, 4 px
   corner radius) that pulses 3 times over 900 ms. Under reduced motion
   it is a static ring for 1500 ms. It holds a `SafePointer`, so a
   control deleted mid-flash just ends the flash.
9. **Announce** "{name}, {breadcrumb}" politely via
   `juce::AccessibilityHandler::postAnnouncement`. The focused control
   then announces its own label and value.

**Places** open with steps 5-9. Focus goes to the place's first
interactive child, as for overlays (accessibility 1).

### 4.3 Commands (`ActionRegistry`)

`ActionDef { id, titleKey, synonymsKey, std::function<Availability()>
available, std::function<bool()> perform, UndoClass undo }`.

The body of `LuthierAudioProcessorEditor::keyPressed` is refactored
into `bool performAction (const juce::String& actionId)`, which is the
chain of `is(...)` branches that exist today, moved as-is. `keyPressed`
becomes `performAction (AccessibilitySettings::get().findAction (key))`
plus the Escape and digit special cases. Every shortcut id is therefore
an `ActionDef`.

Commands that have no key register the same way:
- Open Workshop / Options / preset browser / Help / Export
- Export MIDI (MIDI OUT tab's export)
- Export audio, Import MIDI (the existing `importMidiFile` chooser)
- Retune all (`CharacterEngine::retune`, the CHARACTER "Retune" button)
- New tune
- Recall snapshot n, Save snapshot to slot n
- Arm / disarm each technique (TECHNIQUES pills, gui-techniques-updates)
- Toggle Live Mode, Toggle practice drawer
- Clear recent searches

The buttons that do these things call `performAction` too. A command
whose feature is not built is not registered. It is absent, not
present and dead (the rule `buildDefaultShortcuts` already follows).

### 4.4 Inline adjustment

**Grammar**, applied to the last token (or the last two, for "442 Hz"):
- A number, optionally with a unit: `7`, `-3 dB`, `442 Hz`,
  `0.442 kHz`, `20 ms`, `0.02 s`, `+50 cents`
- A relative step: `+1`, `-2`
- A percentage: `50%`, meaning that fraction of the live normalised range
- A keyword: `on`, `off`, `toggle`, `min`, `max`, `default`, `reset`
- The text of a choice option: "amp model plexi"

Units are converted to the parameter's own unit. The value then goes
through the parameter's own `getValueForText`, which is the same text
function used by right-click "Enter value" (ui-wiring 1).

**Two readings.** The navigator scores both readings of the query: the
whole query as a name, and name-part plus value. The value reading is
used only if the name-part's best match is a Parameter or Choice option
scoring at least 400, and it scores higher than the whole-query
reading. So "snapshot 3" stays the command "Recall snapshot 3", and
"pickup 2 volume" stays a name.

**Preview.** The selected row shows "Set to 7.0 (now 5.0)" and a
44 x 8 px inline slider.

**Keys:**
- `Enter` applies and closes.
- `Shift+Enter` applies and keeps the palette open with the value
  selected.
- `Ctrl+Enter` goes to the control without applying.
- `Alt+Left` / `Alt+Right` nudge the selected parameter by the
  control's arrow-key step. `Alt+Shift` gives the fine step.
- Dragging the inline slider adjusts it.

**Applying.** The palette calls `beginChangeGesture`,
`setValueNotifyingHost`, then `endChangeGesture`: the same path as a
mouse edit. The host sees it as automation in write mode, and undo gets
one class 3.1 entry, "Change Amp Gain from 5.0 to 7.0". Repeated nudges
on the same parameter within 200 ms merge (action-and-undo 4).

**Limits:**
- Values out of range clamp: "Clamped to 10.0 (max)".
- A parameter locked to its stock range clamps at the stock edge and
  shows the same text as `showLockedRangeNoticeIfAtEdge`
  (advanced-ranges 6.3).
- A parameter under modulation has its base value set, as a knob drag
  would.
- Inline adjustment is refused for `needs*`, `proLocked` and
  `modeUnavailable`, except that `modeUnavailable` allows it (that is
  the fallback in 4.2 step 3).

### 4.5 Recent items and recent searches

After each activation, `SearchIndex` records `{ itemId, lastUsed, uses }`
(at most 200 items, the oldest dropped) and the committed query text
(the last 10 distinct). They are stored in `UiPreferences` keys
`search.recentItems` and `search.recentQueries` as JSON strings. They
are user-global. They are never in presets, host state or telemetry
(updates-telemetry.md). An item id that no longer resolves is pruned
when read.

## 5. Other actions on a result (`Alt+Enter`, or right-click a row)

- **Parameter rows** show exactly `buildParameterContextMenu (processor,
  id)` (Widgets.h), and `applyParameterMenuResult` runs the choice. That
  gives Enter value, Reset, Copy / Paste, MIDI Learn, Assign to macro,
  Modulate (the `kModulateMenuBase` range), and the range-unlock items.
  These are the same 13 items as gui-integration 16, with no new code
  path.
- **Other kinds:**
  - Preset: Load, Show in browser, Reveal file.
  - Part: Show in Workshop, Fit to this guitar (undoable part swap,
    action-and-undo 3.4), Reveal file.
  - Command: Run, Show shortcut.
  - Help: Open, Open pinned in HELP tab.
  - Every kind: Copy id, Remove from recent.

## 6. UI

### 6.1 Entry points

- **Header.** A magnifier `TextButton searchButton` in the Overflow
  region (gui-integration 2), before the gear, with the tooltip
  "Search everything ({key:search})". `HeaderBar` gains
  `std::function<void()> onOpenSearch`. Below 1280 px, it goes into the
  three-dot menu as "Search… Ctrl+K".
- **Shortcut.** `add ("search", "accessibility.shortcut.search", KP ('k',
  cmd, 0))` in `buildDefaultShortcuts`. It is rebindable. While the
  palette is open, pressing it again closes the palette. `Ctrl+K` is
  unbound in gui-integration 17 and in the registry.
- **Help tab.** A "Search" field at the top of the HELP tab opens the
  palette with the `?` scope.
- **Empty-state hint.** The `InlineNotice` in the existing
  first-run notice set (coordinated with onboarding.md).

### 6.2 Layer and layout

`CommandPalette` is a child of the editor, above `OverlayHost` and
below `MidiLearnArmLayer`, like the arm layer. It is not an
`OverlayPanel`, so it can open over the Options overlay without
dismissing it. It has a 40% scrim; clicking the scrim closes the
palette. Opening it cancels an armed MIDI Learn.

```
+--------------------------------------------------------------+
| [magnifier] treble bl|                               [Esc]   |  field 40 px
| All  Controls  Places  Commands  Content  Help               |  chips 24 px
+--------------------------------------------------------------+
| [knob] Treble bleed        Adv › Col 2 › CIRCUIT    Off    > |  rows 36 px
| [knob] Treble bleed R      Adv › Col 2 › CIRCUIT  150k       |
| [cmd ] Toggle Slide Mode                               S     |
| [?   ] Help: Guitar circuit                                  |
+--------------------------------------------------------------+
| Enter go · Alt+Enter actions · type a value to set · Esc     |  footer 20 px
+--------------------------------------------------------------+
```

- **Size.** Width is `min(640, window - 32)`. The top sits 48 px below
  the header. Height grows with the results up to 60% of the window.
  All sizes scale with the UI scale (accessibility 4). In Live Mode
  rows are 44 px (gui-integration 9).
- **Rows.** Each row has:
  - a kind glyph;
  - the title, with matched characters in the accent colour and bold
    (never colour alone, accessibility 0.2);
  - a dimmed breadcrumb;
  - on the right: the current value for parameters (refreshed at 10 Hz,
    only for visible rows), a shortcut chip for commands, and a lock
    glyph plus "Pro" when locked;
  - "Opens in Advanced" or "Needs Slide Mode" as a subtitle when
    relevant.
- **Empty state (no query).**
  - "Recent": the last 8 items.
  - "Recent searches": 3 query texts.
  - "Suggestions": 3 items from the current tab or strip, plus Load a
    preset, Open Workshop, Tuning, Show all shortcuts.
  - The hint "Type a control, place, command or preset. > commands,
    ? help, # content".
  - With "Remember recent" off, the Recent sections are not shown.
- **No results.**
  - "No matches for '{query}'."
  - "Did you mean '{best typo candidate}'?" as a row.
  - "Search Help for '{query}'", which opens the HELP tab filtered.
- **Errors.** Shown as the footer line in the warning colour, e.g.
  "Couldn't show Treble bleed. Opened its panel instead." They are
  never modal.

### 6.3 Keyboard and mouse

| Input | Effect |
|---|---|
| typing | Filters instantly, with no debounce |
| Up / Down, PageUp / PageDown, Ctrl+Home / Ctrl+End | Move the selection; wraps at the ends |
| Enter / Shift+Enter / Ctrl+Enter / Alt+Enter | 4.4 and 5 |
| Alt+Left / Alt+Right | Nudge the selected parameter |
| Tab / Shift+Tab | Move focus between the field, the chip row and the list. Left / Right choose a chip |
| Escape | Close, whatever the query, and return focus to the component focused before opening (accessibility 1) |
| F1 | Help topic "Search" |
| Click a row | Same as Enter |
| Hover | Selects |
| Wheel | Scrolls |
| Right-click | Same as Alt+Enter |
| Click the scrim | Close |

The field consumes keys like every text field (input-routing 3.1), so
single-key shortcuts (P, S, W...) cannot fire while typing. IME
composition is honoured (input-routing 3.3). Queries are capped at 200
characters.

## 7. Options

Options -> ACCESSIBILITY gains a "Search" group:
- **Switch mode automatically to show a result** (default on). Stored
  as `search.autoSwitchMode`.
- **Remember recent searches** (default on). Stored as
  `search.rememberRecent`. Turning it off clears both stores.
- **Clear recent searches** (button, also the command
  `cmd:clearRecentSearches`).

The shortcut is rebound in the existing table. These are
`UiPreferences` values, saved immediately. They are not parameters and
not preset data.

## 8. Provider contract for other features

Features specified in parallel (jam mode, riff library, the new preset
browser, mic placement, auto-articulation, animated strings) register
their items without touching the search code:

```cpp
struct SearchProvider
{
    virtual ~SearchProvider() = default;
    virtual juce::String getId() const = 0;              // "riffs"
    virtual juce::uint32 getGeneration() const = 0;      // bump to re-collect
    virtual void collect (std::vector<SearchItem>& out) const = 0;   // message thread
    virtual Availability availabilityOf (const SearchItem&) const = 0;
    virtual bool activate (const SearchItem&, ActivationKind, SearchContext&) = 0;
    virtual juce::StringArray secondaryActions (const SearchItem&) const { return {}; }
};
```

They register with `SearchIndex::addProvider` in
`LuthierAudioProcessorEditor::buildSearchProviders()`. `collect` must
finish in 5 ms for 2,000 items and must not touch disk (use the
manager's cached list).

What each feature owes the index:

| Feature | Must register |
|---|---|
| Any new tab, sub-tab, group, drawer tab or overlay | `SearchAnchors::tag` on its host component, plus a place-catalogue row with synonyms (GS-03 fails otherwise) |
| Any new parameter | Nothing, if its control is a `LearnTarget` that exists after the editor is built. A `ParameterLocations` row if the control only exists when a popover opens. The `search.syn.param:<id>` catalog key |
| Any new command or button action | An `ActionDef`. If it has a shortcut, the `AccessibilitySettings` id and the `ActionDef` id are the same |
| Riff library | A `riff:` provider (title, key, tempo, tags as keywords). Enter opens the riff in the library; secondary actions Audition, Insert into tune |
| Preset browser (new spec) | Replaces `PresetProvider`'s activation with "open browser at entry". It keeps the `preset:` ids so recent items survive |
| Jam mode | Commands (start / stop jam, change key, change style) and its places |
| Mic placement | Its parameters are covered automatically. Adds the place for the placement view and the command "Reset mic placement" |
| Auto-articulation | Its toggle parameter is covered automatically. Adds an `ActionDef` "Toggle auto-articulation" |
| Animated strings | A `set:` setting on its Options toggle |

## 9. Undo, state and serialization

- **Parameters:** none. No `getStateInformation` change. Presets,
  snapshots and host sessions are byte-identical with or without use of
  the palette (GS-30).
- **Undo:**
  - Navigation, opening places, mode switches and the palette itself
    are not undoable (action-and-undo 3.17).
  - Inline sets are class 3.1.
  - Commands keep their own classes: a preset load is a boundary (5),
    a Slide toggle is 3.3, a part fit is 3.4, a pedal add is 3.13, a
    snapshot recall is 3.7.
  - Panic and tap tempo skip the stack (7).
- **Persistence:** `UiPreferences` keys (4.5 and 7), plus the `search`
  binding in the `AccessibilitySettings` config.

## 10. Edition split

Search is in **both** editions, in full. It is navigation and
accessibility (editions 0.3). Pro features stay indexed in Free
(editions 4.1: locked is never hidden):
- Their rows show the lock and "Available in Luthier Pro".
- Enter opens the upsell panel (editions 4.2).
- Inline adjustment is refused.
- Pro-only parameters inside Free panels navigate to that panel's
  "More in Luthier Pro" row.

The Free `ActionRegistry` omits commands whose code is not in the
binary (editions 7.2). It indexes a `proLocked` stand-in instead, so
"export midi" still explains itself. Locked items get the -100 rank
penalty (4.1), so they never outrank a working result.

## 11. Performance budget

| Measure | Budget (baseline CPU, performance-budget.md) |
|---|---|
| Audio thread | 0 work, 0 allocations, 0 locks |
| Query over 10,000 items | p95 <= 4 ms, p99 <= 8 ms, message thread |
| Full index build (10,000 items) | <= 30 ms. Runs lazily on first open after a generation change; per provider when possible |
| Palette open (index warm) | <= 16 ms to first painted frame |
| Navigate + highlight | <= 100 ms to focus, including a mode switch |
| Memory | <= 4 MB for index and recent stores |
| Value refresh | 10 Hz, visible rows only |

The matcher pre-computes the normalised title, word-start offsets and
the acronym for each item at build time, so a query allocates only the
result vector (reserved to 50).

## 12. Interactions

- **Techniques.** Arm commands per technique, sub-tab places and all
  technique parameters. Free: locked (H3).
- **Rhythm engine.** Genre kits are choice options ("funk" finds
  "Genre kit: Funk 16th"). The STRUM group is a place.
- **Tune builder.** New tune command, TUNE places, `.luthiertune` files
  as a `tune:` provider. Space still belongs to the tune transport. The
  palette field consumes it while open.
- **MIDI export.** The "Export MIDI" command opens the MIDI OUT tab's
  export. Drag-out stays on that tab, because a palette row is not a
  drag source.
- **Snapshots.** Recall and save commands follow Free limits
  (`edition::limits.snapshotsPerBank`).
- **Host automation.** Inline sets go through gestures (4.4). The host
  records them in write mode, and they never fire while a host is
  playing back automation for that parameter.
- **MIDI Learn / modulation / advanced ranges.** Through the shared
  right-click menu (section 5).
- **Workshop.** Part results open `WorkshopPanel::showCategory`. In
  Easy Mode they open the `WorkshopOverlay`.
- **Live Mode.** Allowed. Uses 44 px rows. Never switches to Advanced
  (4.2 step 3).
- **Preset load from host or program change while open.** The
  `PresetManager` change message bumps the generation, and the results
  refresh keeping the selection by id.
- **Multi-instance.** Each editor has its own index and highlighter.
  Recent items are shared (user-global).

## 13. Failure modes

| Failure | Response |
|---|---|
| Target control not found after running the steps (a bug) | Open the enclosing place, write a footer line, log `search.navigate.miss <id>` to `ErrorLog`. GS-02 makes this a test failure |
| Item no longer exists (preset deleted, part removed) | "{name} no longer exists". Drop it from recent and re-collect that provider |
| Host swallows Ctrl+K (plugin window not focused) | The header button always works. Documented in host-integration quirks |
| A text field elsewhere has focus | Ctrl+K is typed into it (input-routing 3.1). The header button works |
| Unparseable value ("gain seven") | No value reading. Treated as a name query |
| Locale has no synonyms | English synonyms apply |
| Provider returns duplicate ids | The later one is dropped and logged. A debug build asserts |
| Editor resized below 1000 px while palette open | Rows re-evaluate `modeUnavailable` |

## 14. Accessibility

- The palette handler role is `dialog`, named "Search". The field is
  `editableText` with the label "Search everything". The list is a
  `list` with `listItem` rows.
- Each row's accessible title is "{title}, {kind}, {breadcrumb},
  {value}, {lock}". At verbosity `minimal`, only title and value.
- On open: "Search. Type to find controls, places, commands and
  presets."
- 400 ms after typing stops: "{n} results, first: {title}", or "No
  results".
- Every inline value change announces the new value text.
- Everything in 6.3 works without a mouse. Nothing uses colour alone.
  The highlight ring is also a shape.
- Every string comes from the locale catalog, under keys `search.*`,
  with named placeholders (accessibility 6). Right-to-left layout
  mirrors the row (glyph right, value left).

## 15. Tests (LuthierTests; GUI tests run under xvfb like EditorTests.cpp)

Coverage (the "nothing added later is missed" tests):
- **GS-01** For every parameter in `processor.getParameters()` that is
  not in `ParameterVisibility::intentionallyHidden()`, the index holds
  exactly one `param:<id>` item with a non-empty localized title. It
  fails naming each missing id.
- **GS-02** Editor at 1600x1000. For every GS-01 item, `goTo` in four
  contexts: default guitar, bass, whammy bridge, Slide Mode on (the
  ComboHarness contexts GuiReachabilityTests uses). Within 3 message
  loop iterations, an on-screen `LearnTarget` with that id has keyboard
  focus and the highlighter is on its bounds. An item whose
  availability is a `needs*` value for that context passes only if its
  availability says so.
- **GS-03** Every `AdvancedPanel::getWorkspaceTabName(i)`, every
  `OptionsPanel::getPageNames()` entry, every `Column` section heading,
  every practice drawer tab, every `OverlayPanel` the editor owns, and
  every tagged sub-tab and group has a `place:` item. Navigating to it
  makes it visible. Every place-catalogue row matches a real surface
  (no dangling places).
- **GS-04** Every `AccessibilitySettings::getShortcuts()` id is a
  `cmd:` and a `key:` item. Starting from identical states,
  `performAction(id)` from the palette and the key press give identical
  processor state and undo depth. This is checked for panic, undo,
  toggleSlideMode, newPreset, toggleAdvanced and togglePractice.
- **GS-05** Every `HelpContent` topic is indexed. Searching any alias
  returns that topic in the top 3.
- **GS-06** Every preset, factory guitar and `PartLibrary` part is
  indexed. Writing a new user preset and calling
  `PresetManager::refresh` makes it appear on the next query. Deleting
  it gives the "no longer exists" response and prunes recent items.

Matching and ranking (unit tests, no editor):
- **GS-10** Top result for each query:
  - "treble bleed" -> `treble_bleed`
  - "gain" -> the amp gain
  - "trebel bleed" -> `treble_bleed`
  - "concert" -> `concert_a`
- **GS-11** Must appear in the top 3:
  - "tb" -> `treble_bleed`
  - "reverb" -> room wet/dry and `pedal:Reverb`
  - "drive" -> amp gain
- **GS-12** The same query twice gives the same order. Tie-breaks follow
  4.1 exactly on a synthetic corpus of equal titles.
- **GS-13** After activating "Input Gain" 3 times, "gain" ranks it
  first. With a clock mock at +28 days, the bonus is 150/16 ±1.
- **GS-14** Scopes: "> slide" returns only commands and shortcuts, and
  "? tone" only help topics.
- **GS-15** German locale: titles are the catalog translations. "gain"
  (English) still finds the amp gain. Diacritic folding: "ubersicht"
  matches "Übersicht".
- **GS-16** Fuzz: 10,000 random Unicode queries up to 200 characters.
  No crash, at most 50 results, every score at least 150.

Inline adjustment:
- **GS-20** Each input and its result:
  - "gain 7" + Enter -> amp gain reads 7.0
  - "gain +1" -> 8.0
  - "concert a 442 hz" -> 442
  - "concert a 0.442 khz" -> 442
  - "room 50%" -> normalised 0.5
  - "treble bleed off" -> the "Off" option
- **GS-21** One inline set gives one undo entry. Undo restores the old
  value. Five `Alt+Right` nudges within 200 ms give one entry. The host
  listener sees exactly one begin/end gesture pair per set.
- **GS-22** "gain 99" clamps to the maximum and says so. A
  stock-locked physical parameter clamps at its stock edge.
- **GS-23** "snapshot 3" runs Recall snapshot 3 and changes no
  parameter. "pickup 2 volume" navigates and changes nothing.
- **GS-24** No inline set for whammy parameters on a hardtail, for
  `proLocked` items in a Free build, or for slide parameters with Slide
  Mode off. The parameter value is unchanged.

Palette UI (xvfb):
- **GS-30** Ctrl+K opens the palette with focus in the field. Ctrl+K
  again or Escape closes it, and focus returns to the component that
  had it before. The header magnifier opens it. The state from
  `getStateInformation` is byte-identical after 50 searches and
  navigations.
- **GS-31** Easy Mode, select a parameter that exists only in Advanced:
  the mode becomes Advanced, the right tab shows, the control is
  focused, and the notice is up. Easy Mode, select the amp gain: the
  mode stays Easy (mirror).
- **GS-32** Window at 960 px: an Advanced-only result does not switch
  modes, shows the fallback text, and an inline set still works. The
  same holds in Live Mode.
- **GS-33** With auto-switch off, the first Enter shows the confirm row
  and the second switches.
- **GS-34** Empty state: recent items and suggestions are shown. With
  "Remember recent" off, both stores are empty and no recent section is
  shown.
- **GS-35** No results shows the did-you-mean row. Activating it
  replaces the query.
- **GS-36** Accessibility:
  - The handler roles are as in section 14.
  - Every row has a non-empty accessible title.
  - The announcements (captured through the test hook
    `CommandPalette::onAnnouncement`) fire on open and 400 ms after
    typing.
  - A keyboard-only run (type, arrows, Enter) reaches the target
    without a mouse event.
- **GS-37** Reduced motion: the highlighter produces no intermediate
  alpha frames.
- **GS-38** Clicking a row activates it. Clicking the scrim closes the
  palette. Right-clicking a parameter row shows the same items as
  `buildParameterContextMenu` for that id.
- **GS-39** In a Free build, a Pro item shows the lock, Enter opens the
  upsell panel, and it ranks below any available item with an equal
  match.

Combination and performance:
- **GS-40** A palette query every 16 ms during a 30 s render of the
  Rock preset gives 0 audio-thread allocations (heap hook), and the
  output is bit-identical to the same render without the palette.
- **GS-41** Query p95 at most 4 ms and index build at most 30 ms on a
  synthetic 10,000-item corpus (performance-budget CI job).
- **GS-42** A host preset load while the palette is open refreshes the
  results and keeps the selection by id.
- **GS-43** A Workshop overlay is open in Easy Mode. Selecting a CAB
  parameter dismisses the overlay and navigates. Selecting a part opens
  the Workshop on its category with the card focused.
- **GS-44** A fake `SearchProvider` registered in a test adds items.
  Bumping its generation re-collects. `activate` receives the chosen
  item and the activation kind. Duplicate ids are dropped and logged.
- **GS-45** Retune all, Export MIDI and Toggle Slide Mode run from the
  palette with the same effects and undo classes as their buttons and
  shortcuts.
