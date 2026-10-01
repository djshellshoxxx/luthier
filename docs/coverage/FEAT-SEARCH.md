# FEAT-SEARCH coverage: global search and command palette

Spec: `spec/global-search.md` (GS-). Code: `Source/UI/Search/`, plus small,
marked hooks in `PluginEditor.*`, `Widgets.h` (`LearnTarget`), `HeaderBar.*`,
`AdvancedPanel.*`, `OptionsPages.*`, `HelpTab.*`, `HelpContent.cpp`,
`Accessibility.cpp` / `Localisation.cpp` (the `search` shortcut) and
`GuiReachabilityTests.cpp` (the shared hidden list). Tests:
`Source/Tests/SearchTests.cpp` (no editor) and
`Source/Tests/SearchEditorTests.cpp` (the real editor, under xvfb).
Integration guide for other features: `docs/SEARCH_INTEGRATION.md`.

No parameters are added (ground rule 0.1); presets, snapshots and host state
are unchanged (GS-30 checks `getStateInformation` byte for byte).

## (a) Coverage

| ID | Spec | Implementation | Verification | Status |
|---|---|---|---|---|
| GS-R0.1 | 0.1 UI only, no parameters, no audio code | Everything in `Source/UI/Search`; values read from the APVTS | GS-30 (state byte-identical), GS-40 (render bit-identical, 0 allocations) | verified |
| GS-R0.2 | 0.2 one action path | `LuthierAudioProcessorEditor::performAction` (keyPressed's chain moved as-is); header buttons, shortcuts and palette call it | GS-04, GS-45 | verified |
| GS-R0.3 | 0.3 index cannot fall out of date | Providers read the APVTS, tab strip, Options pages, shortcuts, HelpContent, PresetManager, PartLibrary; LearnTarget registers controls | GS-01..06 | verified |
| GS-R0.4 | 0.4 no sound change by surprise; ties are navigation | `InlineValue::read` (value reading only if strictly higher) | GS-23 | verified |
| GS-R0.5 | 0.5 keyboard first, announced | `CommandPalette::handleKey`, announcements | GS-36, keyboardAndLiveRows | verified |
| GS-R2.1 | 2 parameter items `param:<id>` | `ParameterProvider` | GS-01 | verified |
| GS-R2.2 | 2 choice options `opt:<id>:<i>` | `ChoiceOptionProvider` (pedal slot types are the `pedal:` kind instead) | GS-20 ("treble bleed off"), GS-32 | verified |
| GS-R2.3 | 2 places `place:` | `PlaceProvider::catalogue` | GS-03 | verified |
| GS-R2.4 | 2 commands `cmd:` incl. every shortcut | `CommandProvider` over `ActionRegistry` | GS-04, GS-14 | verified |
| GS-R2.5 | 2 shortcuts `key:` -> rebind row | `ShortcutProvider`, `SearchNavigator::openShortcutRow` | GS-04 | verified |
| GS-R2.6 | 2 presets `preset:<factory\|user>/<name>` | `PresetProvider` (load = browser's path, undo boundary) | GS-06 | verified |
| GS-R2.7 | 2 guitars `guitar:<path>` | `GuitarProvider` (a type's guitar through the parameter, else as parts) | GS-06 | verified |
| GS-R2.8 | 2 parts `part:<type>/<name>` -> Workshop category | `PartProvider`, `openWorkshopOn` | GS-06, GS-43 | verified |
| GS-R2.9 | 2 pedal types -> first empty slot of default chain | `PedalTypeProvider::addPedal` (ScopedUndoAction, 3.13) | GS-11 ("reverb" -> pedal:Reverb) | verified |
| GS-R2.10 | 2 snapshots `snap:` recall | `SnapshotProvider` | GS-23 | verified |
| GS-R2.11 | 2 help topics incl. aliases | `HelpProvider` (aliases are synonyms) | GS-05 | verified |
| GS-R2.12 | 2 settings `set:<page>:<key>` | `SettingProvider` (tagged + discovered), `SearchAnchors::tagSetting` | entryPointsAndOptions | verified |
| GS-R2.13 | 2 provider items | `SearchProvider` contract; genre kits (`kit:`) and tunes (`tune:`) built in | GS-44, GS-06 | verified |
| GS-R2.14 | 2 keywords: English title x0.8, synonyms `search.syn.*`, units spelled out, breadcrumb | `SearchMatcher::scoreItem`, `SearchCatalog` | GS-15, GS-11 | verified |
| GS-R2.15 | 2 curated English synonyms for every parameter and place | Curated `search.syn.*` for the main controls and all places; every parameter also carries its id words and unit words | GS-11 | partial: see decision D6 |
| GS-R2.16 | 2 pedal slot titles from the fitted pedal; empty slot knobs hidden by default | `ParameterText::titleFor`, `isInertSlotParameter` | GS-01, GS-02 ("pedals" context) | verified |
| GS-R2.17 | 2 hidden list shared with GuiReach | `ParameterVisibility.h` | GS-01 | verified |
| GS-R3.1 | 3.1 classes in `Source/UI/Search` | SearchItem, SearchMatcher, SearchIndex, SearchProviders, ActionRegistry, ParameterLocations, LiveControls (+SearchAnchors), SearchNavigator (+SearchHighlighter), InlineValue, CommandPalette | build | verified |
| GS-R3.2a | 3.2 LearnTarget registers with LiveControls | `LearnTarget::LearnTarget/~LearnTarget` in LiveControls.cpp | GS-02 | verified |
| GS-R3.2b | 3.2 place tags | `SearchAnchors::tag`: AdvancedPanel columns; navigator tags tabs, Options pages, overlays, drawer tabs, racks, groups | GS-03 | verified |
| GS-R3.2c | 3.2 popover-only controls | `ParameterLocations` rows (headstock, bridge, rack slots) | runs in the app; see D9 | pending: needs a desktop peer to test |
| GS-R3.3 | 3.3 UiLocation steps with named open functions | `LocationStep`, `SearchNavigator::runStep` | GS-03 | verified |
| GS-R3.4 | 3.4 availability computed when asked | providers' `availabilityOf`, `ParameterLocations::evaluate` | GS-02, GS-24, GS-32 | verified |
| GS-R4.1 | 4.1 normalisation (shared with HelpContent), tokens, score table, bonuses, cut-off, cap, ties, scopes | `SearchMatcher`, `SearchIndex::query`, `HelpContent::normalise` | GS-10..16 | verified |
| GS-R4.2 | 4.2 navigation steps 1-9 | `SearchNavigator::goToParameterControl / finishOnControl / scrollIntoView / focusAndHighlight` | GS-02, GS-31, GS-43 | verified |
| GS-R4.2b | 4.2 step 2 auto mode switch + notice | `switchModeFor` | GS-31 | verified |
| GS-R4.2c | 4.2 step 3 mode unavailable (width, Live Mode) | `isAdvancedModeAvailable`, `modeUnavailable` | GS-32 | verified |
| GS-R4.2d | 4.2 step 4 context gates | `needsSlideMode` confirm then `HeaderBar::toggleSlideMode`; `needsBass/Whammy` -> Workshop | GS-02, GS-24 | verified |
| GS-R4.2e | 4.2 step 8 highlighter | `SearchHighlighter` (2 px accent, 4 px outset, 3 pulses / 900 ms; static 1500 ms) | GS-02, GS-37 | verified |
| GS-R4.2f | 4.2 step 9 announce | `SearchNavigator::announce` | GS-36 | verified |
| GS-R4.3 | 4.3 ActionRegistry; keyPressed -> performAction; commands without keys | `ActionRegistry`, `registerActions`, `performExtendedAction` (Workshop, export audio/MIDI, import MIDI, retune all, new tune, chords, recall/save snapshot 1-16, arm slap/scrape, clear recent) | GS-04, GS-45 | verified |
| GS-R4.4 | 4.4 inline grammar, units, %, keywords, options, two readings | `InlineValue` | GS-20, GS-23 | verified |
| GS-R4.4b | 4.4 preview "Set to X (now Y)" + 44 x 8 slider | `CommandPalette::paintRow`, `getSubtitle` | GS-20 (text), manual | verified |
| GS-R4.4c | 4.4 Enter / Shift+Enter / Ctrl+Enter / Alt+arrows / drag | `handleKey`, `nudge`, RowComponent drag | GS-21, GS-36 | verified |
| GS-R4.4d | 4.4 gestures, one undo entry, nudges merge in 200 ms | `setAsGesture`, `nudge` | GS-21 | verified |
| GS-R4.4e | 4.4 clamp text, stock-locked edge | `InlineValue::resolve` | GS-22 | verified |
| GS-R4.4f | 4.4 refusals (needs*, proLocked; modeUnavailable allowed) | `SearchNavigator::applyValue` | GS-24, GS-32 | verified |
| GS-R4.5 | 4.5 recent items/queries in UiPreferences, pruned | `RecentStore` | GS-13, GS-06, GS-34 | verified |
| GS-R5 | 5 secondary actions; parameter rows = `buildParameterContextMenu` | `CommandPalette::buildSecondaryMenu / runSecondary`; providers' `secondaryActions` | GS-38 | verified |
| GS-R6.1a | 6.1 header magnifier; File menu below 1280 | `HeaderBar::searchButton` (`MagnifierButton`), File -> Search... | GS-30 | verified |
| GS-R6.1b | 6.1 Ctrl/Cmd+K rebindable; again closes | `add ("search", ...)` in `buildDefaultShortcuts` | GS-30 | verified |
| GS-R6.1c | 6.1 HELP tab Search field -> `?` scope | `HelpSearchField` in `HelpTab` | entryPointsAndOptions | verified |
| GS-R6.1d | 6.1 first-run empty-state hint (onboarding) | - | - | deferred: D10 |
| GS-R6.2 | 6.2 layer, scrim, size, rows, empty state, no results, errors | `CommandPalette` | GS-34, GS-35, GS-38, keyboardAndLiveRows | verified |
| GS-R6.3 | 6.3 keys and mouse | `handleKey`, RowComponent | GS-36, GS-38, keyboardAndLiveRows | verified |
| GS-R7 | 7 Options -> ACCESSIBILITY Search group | `SearchOptionsGroup` in `AccessibilityPage` | entryPointsAndOptions, GS-33, GS-34 | verified |
| GS-R8 | 8 provider contract; `buildSearchProviders` | `SearchProvider.h`, `LuthierAudioProcessorEditor::buildSearchProviders` | GS-44 | verified |
| GS-R9 | 9 undo classes, no state change | `ActionDef::undo`; commands keep their own | GS-21, GS-30, GS-45 | verified |
| GS-R10 | 10 edition split: locked shown, -100, Enter refuses | `SearchNavigator::proLockPredicate` | GS-39, GS-24 | verified (predicate); upsell panel: D8 |
| GS-R11 | 11 performance budget | precomputed text, character masks, allocation-free scoring | GS-41 (build ~12 ms, p95 ~2-3 ms), GS-40 | verified |
| GS-R12 | 12 interactions (kits, tunes, preset load while open, Workshop, Live Mode, multi-instance) | `GenreKitProvider`, `TuneProvider`, preset change listener, 44 px rows | GS-06, GS-42, GS-43, keyboardAndLiveRows | verified |
| GS-R13 | 13 failure modes | navigate miss -> footer + `ErrorLog` `SEARCH_NAVIGATE_MISS`; "no longer exists" + prune; duplicates dropped/logged; resize re-evaluates | GS-06, GS-44, GS-02 | verified |
| GS-R14 | 14 accessibility: roles, row titles, announcements, catalog strings, RTL | `CommandPalette::createAccessibilityHandler` (dialogWindow), `getAccessibleRowTitle`, `SearchCatalog` | GS-36 | verified |
| GS-01 | 15 | `SearchEditorTests.cpp` | `SearchEditor.GS01_everyParameterIndexed` | verified |
| GS-02 | 15 | | `SearchEditor.GS02_everyParameterNavigates` (5 contexts) | verified; see D11 |
| GS-03 | 15 | | `SearchEditor.GS03_everyPlaceIndexedAndReal` | verified |
| GS-04 | 15 | | `SearchEditor.GS04_shortcutsAreCommands` | verified |
| GS-05 | 15 | | `SearchEditor.GS05_helpTopicsAndAliases` | verified (help scope; D12) |
| GS-06 | 15 | | `SearchEditor.GS06_contentIndexedAndFresh` | verified |
| GS-10 | 15 | `SearchTests.cpp` | `Search.GS10_topResults` | verified |
| GS-11 | 15 | | `Search.GS11_inTopThree` | verified |
| GS-12 | 15 | | `Search.GS12_deterministicOrderAndTieBreaks` | verified |
| GS-13 | 15 | | `Search.GS13_recentUseAndDecay` | verified |
| GS-14 | 15 | | `Search.GS14_scopes` | verified |
| GS-15 | 15 | | `Search.GS15_germanLocaleAndDiacritics` | verified |
| GS-16 | 15 | | `Search.GS16_fuzz` | verified |
| GS-20 | 15 | | `Search.GS20_inlineValues` | verified |
| GS-21 | 15 | | `SearchEditor.GS21_inlineSetUndoAndGestures` | verified |
| GS-22 | 15 | | `Search.GS22_clamping` | verified |
| GS-23 | 15 | | `Search.GS23_navigationReadings` | verified |
| GS-24 | 15 | | `SearchEditor.GS24_inlineSetRefusals` | verified |
| GS-30 | 15 | | `SearchEditor.GS30_openCloseFocusAndState` | verified (focus as requested: D9) |
| GS-31 | 15 | | `SearchEditor.GS31_modeSwitching` | verified |
| GS-32 | 15 | | `SearchEditor.GS32_advancedUnavailable` | verified |
| GS-33 | 15 | | `SearchEditor.GS33_autoSwitchOffConfirms` | verified |
| GS-34 | 15 | | `SearchEditor.GS34_emptyState` | verified |
| GS-35 | 15 | | `SearchEditor.GS35_didYouMean` | verified |
| GS-36 | 15 | | `SearchEditor.GS36_accessibility` | verified |
| GS-37 | 15 | | `SearchEditor.GS37_reducedMotionHighlight` | verified |
| GS-38 | 15 | | `SearchEditor.GS38_mouse` | verified |
| GS-39 | 15 | | `SearchEditor.GS39_proLocked` | verified |
| GS-40 | 15 | | `SearchEditor.GS40_noAudioThreadEffect` | verified |
| GS-41 | 15 | | `Search.GS41_performance` | verified |
| GS-42 | 15 | | `SearchEditor.GS42_presetLoadWhileOpen` | verified |
| GS-43 | 15 | | `SearchEditor.GS43_workshopOverlay` | verified |
| GS-44 | 15 | | `Search.GS44_providerContract` | verified |
| GS-45 | 15 | | `SearchEditor.GS45_commandsMatchTheirButtons` | verified |

## (b) Decisions

- D1. Unitless 0..1 parameters (amp-face knobs: gain, bass, treble...) are shown and typed on the 0-10 dial ("gain 7" = 0.7, reads "7.0"; "gain 99" -> "Clamped to 10.0 (max)"): the spec's examples assume the printed knob scale, the parameters are normalised; a percentage stays the normalised fraction.
- D2. A synonym equal to the whole query scores 600 (a title word), not 450: at 450 "reverb" lost the room's wet/dry to every preset titled "... Reverb", so user story 1 and GS-11 could not hold. Per-token synonym matches keep 450/400.
- D3. Titles that do not stand alone out of their panel get a search title from the catalog (`search.title.param:amp_gain` = "Amp gain", room_blend = "Room"); GS-13 needs "gain" not to be an exact title, and "room 50%" (GS-20) needs the room's wet/dry to be "Room".
- D4. Ties sort by the normalised title (code points), then id: allocation-free (11); case and accents do not reorder.
- D5. Normalisation also treats brackets, colons, commas, quotes, dashes and the ellipsis as spaces (so "Delay time (Post 3)" has a word "post"). HelpContent::findTopic now shares it.
- D6. Curated English synonyms cover the main controls and every place/command group; every other parameter is keyworded by its id words and spelled-out unit instead of a hand-written list (~450 parameters). Translators add `search.syn.*` keys; English applies where a locale has none.
- D7. Search strings live in `SearchCatalog.cpp` (keys `search.*`), consulted after the loaded locale catalog, so `Localisation.cpp` (shared) only gained the one shortcut description.
- D8. The Free edition and its upsell panel are not built (no edition code exists): `proLocked` is produced by `SearchNavigator::proLockPredicate`, which a Free build (and GS-24/GS-39) sets; Enter on a locked row states "Available in Luthier Pro" instead of opening an upsell.
- D9. Under xvfb the editor cannot be put on the desktop (the X server rejects the window's atoms), so nothing holds real keyboard focus in tests: search routes focus through `SearchNavigator::requestFocus`, and tests check the request. Popovers are CallOutBoxes (desktop windows), so the popover route is used only when the editor is showing; without a peer the canonical Advanced control is used.
- D10. The first-run empty-state hint (6.1, "coordinated with onboarding.md") is deferred: the first-run notice set belongs to onboarding (Source/WIP/FirstRun, not integrated) and a notice at editor start would change EditorTests' startup layout.
- D11. GS-02 reports, and does not fail on, parameters GuiReachabilityTests already fails: 48 with no control anywhere at the last merge (scrape_*, slap_* arming/trigger, rh_* style/string tools, noise_player_*, *_style, tune_feel_mod, macro_assign_a/b, ...) and 9 laid out at zero size by their own panel (RHYTHM's STRUM group rows, ROUTING's Aux 1 pre-circuit: "hidden control only"). Those are panel gaps in other workstreams, not routes; the navigator reaches everything that has a size.
- D12. GS-05 checks each alias in the Help scope ("? alias", the Help chip). Unscoped, a control or place of the same name may rightly rank first (148 of 210 aliases still put their topic in the top 3 unscoped).
- D13. Pedal slot type choices are not choice options (16 slots x 22 types would crowd every pedal name); the `pedal:` kind adds a pedal instead. Empty slots' bypass and mix are hidden by default like their knobs.
- D14. The palette's dialog role is JUCE's `AccessibilityRole::dialogWindow` (JUCE 8 has no `dialog`).
- D15. Workspace panels that are not the selected tab are detached from the component tree (the viewport holds one); the navigator treats them as Advanced and in the editor, and their tab tag opens them.
- D16. `SearchNavigator` is a friend of the editor rather than a set of new public editor methods; the editor's new public surface is `performAction`, `getSearch` and `buildSearchProviders`.
- D17. The secret overlay (include.md's hidden effect) is tagged but has no place row, so search does not give the easter egg away (GS-03 exempts it).
- D18. `settingsRoundTrip` (AccessibilityTests) rebinds panic to Ctrl+J: it used Ctrl+K as a free key, which is now Search.
- D19. GS-41's index build is timed as the best of five builds (a shared CI machine's noise); query p95/p99 are measured over 200 queries.
- D20. A Help topic's aliases are its other names (HelpContent::findTopic treats them so): one equal to the whole query scores as the title (1000), via `SearchItem::synonymsAreNames`. Without it the six "Techniques: ..." topics outranked "Playing Techniques" for its own alias "techniques".
- D21. The realism groups merged from REALISM-A/B/C (String aging, Environment, Body coupling, Noise floor, Sustain shape, Tuning stability, Harmonics, Right hand, String interaction) are tagged places in CHARACTER; GS-02 navigated 1401 parameters at the last merge.
- D22. GS-45 compares Retune all from the palette with pressing the CHARACTER panel's Retune button (same session reset and the same undo-depth change), since REALISM-A made the button also retune the room.
- D23. "New tune" is the TUNE-HELP-ONBOARDING shortcut (Ctrl+T, `openNewTune`) in performAction; search indexes it as that shortcut's command rather than a keyless one of its own.
