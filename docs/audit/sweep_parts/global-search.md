## global-search.md

Nothing of the command palette exists on this checkout: no `Source/UI/Search/`, no `CommandPalette`, `SearchIndex`, `ActionRegistry`, `performAction`, no `search` shortcut and no header magnifier. No `origin/claude/luthier-feat-*` branch is pushed yet (checked after `git fetch origin`), so every requirement is OWNED by the global-search FEAT session. The registries it will build on already exist (`AccessibilitySettings::getShortcuts/findAction`, `HelpContent::findTopic`, `LearnTarget`, `buildParameterContextMenu`, `AdvancedPanel::getWorkspaceTabName/setWorkspaceTabNamed`, `OptionsPanel::getPageNames/showPageNamed`, `Column::getSectionContaining`, `GuitarBodyComponent::showTuningPopover/showWhammyPopover`, `CompactRack::openSlot`, `WorkshopPanel::showCategory`).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| GS-0 (§0) | Ground rules: UI only, one action path, index from registries, no surprise sound changes, keyboard/SR complete — not built | - | - | - | OWNED |
| GS-2a (§2) | Index 13 item kinds (param, opt, place, cmd, key, preset, guitar, part, pedal, snap, help, set, provider) — not built | - (sources exist: APVTS, `Presets/PresetManager`, `Model/Workshop/PartLibrary`, `UI/HelpContent`) | - | - | OWNED |
| GS-2b (§2) | Keywords: English title, `search.syn.<id>` catalog synonyms, spelled-out units, breadcrumb — not built | - | - | - | OWNED |
| GS-2c (§2) | Pedal slot `_pN` titled from loaded pedal descriptor; empty slots out of default results — not built | - | - | - | OWNED |
| GS-2d (§2) | `intentionallyHidden()` moved to shared `UI/Search/ParameterVisibility.h` — still local to `Tests/GuiReachabilityTests.cpp:43` | - | n/a | - | OWNED |
| GS-3.1 (§3.1) | New classes in `Source/UI/Search/` (SearchItem, Matcher, Index, Providers, ActionRegistry, ParameterLocations, LiveControls, Navigator, InlineValue, CommandPalette); index owned per editor — directory absent | - | - | - | OWNED |
| GS-3.2 (§3.2) | Registration: `LearnTarget` ctor/dtor adds to `LiveControls`; `SearchAnchors::tag` place tags on panels; `ParameterLocations` rows for popover controls; place catalogue; `tagSetting` — `LearnTarget` (`UI/Widgets.h:101`) has no registration | - | - | - | OWNED |
| GS-3.3 (§3.3) | `UiLocation` step list (mode, workspaceTab, subTab, column, overlay, optionsPage, drawerTab, popover, workshopCategory) — not built (open functions exist) | - | - | - | OWNED |
| GS-3.4 (§3.4) | Availability enum computed live (needsSlideMode/Bass/Whammy/EmptySlot, modeUnavailable, proLocked, notBuilt) — not built | - | - | - | OWNED |
| GS-4.1 (§4.1) | Matcher: shared normalisation with `HelpContent::findTopic`, token AND, score table, recency/frequency bonuses, 150 floor, 50 cap, deterministic ties, scope prefixes/chips — not built | - | - | - | OWNED |
| GS-4.2 (§4.2) | `SearchNavigator::goTo`: choose target, auto mode switch + notice, unavailable-mode fallback, context gates, overlay dismiss, scroll, focus, `SearchHighlighter` pulse, announce — not built | - | - | - | OWNED |
| GS-4.3 (§4.3) | `ActionRegistry`/`ActionDef`; refactor `keyPressed` chain into `performAction(id)`; keyless commands (open overlays, export MIDI/audio, import MIDI, retune, new tune, snapshots, arm techniques, live, drawer, clear recent); buttons call `performAction` — `PluginEditor.cpp:512 keyPressed` still inline `is(...)` chain | - | - | - | OWNED |
| GS-4.4 (§4.4) | Inline adjustment grammar (numbers+units, relative, %, keywords, option text), two-readings rule, preview row + mini slider, Enter/Shift/Ctrl/Alt keys, gesture-wrapped set, 200 ms nudge merge, clamp/locked-range/mod/refusal rules — not built | - | - | - | OWNED |
| GS-4.5 (§4.5) | Recent items (200) and queries (10) in `UiPreferences` `search.recentItems/recentQueries`, pruned on read, never in state — not built | - | - | - | OWNED |
| GS-5 (§5) | Alt+Enter / right-click secondary actions: param rows reuse `buildParameterContextMenu`/`applyParameterMenuResult`; per-kind menus — not built | - | - | - | OWNED |
| GS-6.1 (§6.1) | Entry points: header magnifier `searchButton` + `onOpenSearch` (overflow <1280), `search` Ctrl/Cmd+K rebindable shortcut, Help-tab Search field opens `?` scope, first-run hint — none present | - | - | - | OWNED |
| GS-6.2 (§6.2) | Palette layer above `OverlayHost` below `MidiLearnArmLayer`, 40% scrim, cancels MIDI Learn; size/rows/empty state/no-results/errors layout — not built | - | - | - | OWNED |
| GS-6.3 (§6.3) | Keyboard & mouse table (nav keys, Tab cycling, Esc focus return, F1, hover/click/wheel/right-click/scrim), field consumes keys, IME, 200-char cap — not built | - | - | - | OWNED |
| GS-7 (§7) | Options > ACCESSIBILITY "Search" group: auto-switch mode, remember recent, clear recent — not present in `UI/OptionsPages.cpp` | - | - | - | OWNED |
| GS-8 (§8) | `SearchProvider` contract, `SearchIndex::addProvider` via `buildSearchProviders()`, 5 ms collect budget; per-feature obligations table — not built | - | - | - | OWNED |
| GS-9 (§9) | No state change; undo classes per action; prefs persistence — not built | - | n/a | - | OWNED |
| GS-10 (§10) | Both editions; Pro items indexed locked, upsell on Enter, inline refused, -100 rank, Free proLocked stand-ins — not built (no edition split exists) | - | - | - | OWNED |
| GS-11 (§11) | Perf budget: 0 audio work, query p95 4 ms/10k, build 30 ms, open 16 ms, navigate 100 ms, 4 MB, 10 Hz refresh; precomputed matcher data — not built | - | n/a | - | OWNED |
| GS-12 (§12) | Interactions (techniques, rhythm kits, tune provider, MIDI export, snapshots, automation, Workshop, Live Mode 44 px rows, preset-load refresh, multi-instance) — not built | - | - | - | OWNED |
| GS-13 (§13) | Failure modes (navigate miss -> ErrorLog, deleted item, host eats Ctrl+K, unparseable value, duplicate provider ids, resize) — not built | - | - | - | OWNED |
| GS-14 (§14) | Accessibility: dialog/editableText/list roles, row titles, open/result announcements, `search.*` catalog keys, RTL mirroring — not built | - | - | - | OWNED |
| GS-T1 (§15 GS-01..06) | Coverage tests: all params, goTo in 4 contexts, places both ways, shortcuts parity, help aliases, presets/guitars/parts — none | - | - | - | OWNED |
| GS-T2 (§15 GS-10..16) | Matcher/ranking unit tests incl. recency clock, scopes, German locale, fuzz — none | - | - | - | OWNED |
| GS-T3 (§15 GS-20..24) | Inline-adjust tests (parse, undo/gesture, clamp, two readings, refusals) — none | - | - | - | OWNED |
| GS-T4 (§15 GS-30..39) | Palette UI xvfb tests (open/close/focus, mode switch, 960 px, confirm row, empty state, did-you-mean, a11y, reduced motion, clicks, Free lock) — none | - | - | - | OWNED |
| GS-T5 (§15 GS-40..45) | Combination/perf tests (audio alloc + bit-identical, perf CI, host preset load, Workshop overlay, fake provider, command parity) — none | - | - | - | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=31 -->
