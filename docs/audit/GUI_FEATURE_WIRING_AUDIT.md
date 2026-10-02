# GUI / feature wiring audit

Branch `claude/luthier-gui-audit`, from `codex/luthier-beta`. Every claim below was checked against `Source/`, not the older docs.
The runtime check is `GuiReach` (`Source/Tests/GuiReachabilityTests.cpp`): it builds the real editor, walks 438 views
(Easy, Advanced and every tab, Techniques sub-tabs, Practice, Live, Slide, every overlay, every Options page) in several guitar
contexts, operates each control through its attachment, and records which parameter each one wrote.

## Summary

| Area | Result |
|---|---|
| Host-automatable parameters | 656 |
| ...with a visible, reachable control | 579 (was reported as 578; see finding 1) |
| ...hidden-only or no control, not exempt | 0 |
| Intentionally hidden (`ParameterVisibility.h`, each with a reason) | 10 |
| Pedal-slot `preN_pK` / `postN_pK` params without a knob | 67 (by design: a pedal with four knobs leaves p4-p9 unused) |
| Controls operated by the walk | 4870, all wrote the parameter they name (575 params operated OK) |
| Dead raw controls (no handler / attachment / read) | 0 (static scan of Source/UI headers + sources) |
| Controls bound to a nonexistent parameter id | 0 (literal-id scan; `ParamIDs::` references fail to compile otherwise) |
| Visible but never operated by the walk | 4 (`bend_armed`, `scrape_armed`, `slap_armed`, `tap_armed`: the technique pill row, a custom component the walk has no operator for) |
| Spec rows (93 parts, 3418 rows) | 2486 DONE, 458 PARTIAL, 474 MISSING per `docs/audit/sweep_parts`; about 18 of ~45 spot-checked GUI rows are STALE (implemented since) |
| Findings fixed in this PR | 2 (test hole, missing Workshop button) |

**The "known lead" of about 20 param ids with no control is stale.** Every one of them has a control today, so nothing needed wiring:

| Param(s) | Where the control is |
|---|---|
| `squeak_style`, `noise_floor_style`, `sustain_style` | style combo boxes (`NoiseGroups.cpp`, `RealismGroupsC.cpp`: NoiseFloorGroup, SustainShapeGroup, DecayRow), CHARACTER tab. They write through `applyNoiseFloorStyle` / `applySustainStyle` / the squeak style action, so they are not `LearnTarget`s |
| `setup_style` | SetupGroup style box, CHARACTER tab |
| `rh_style`, `rh_string_tool_1..6` | RightHandGroup style box + `StringToolCell` row, CHARACTER tab |
| `noise_player_angle`, `noise_player_distance` | `PositionPad` (press = distance, wheel = angle), CHARACTER tab |
| `ebow_string_mask`, `scrape_string_mask`, `slap_string_mask` | `StringMaskSelector` (MOD tab, TECHNIQUES SCRAPE and SLAP pages) |
| `jam_intensity` | intensity slider in JamPanel and JamWidgets |
| `aa_enabled`, `aa_rules` | PerformanceAssistUi (Easy AUTO pill, RHYTHM tab); `aa_rules` is non-automatable in Free (Pro suffix), still has its control |
| `preset_morph_position` | morph slider in the preset browser (A/B slots) |

None of them is intentionally internal. Only the 10 ids in `ParameterVisibility.h` are (all superseded or legacy-migrated params, plus `tune_feel_mod` / `tune_tempo_drift`, which are mod-matrix destinations).

## Findings (most severe first)

| # | Category | File:line | Issue | Severity | One-line fix | Status |
|---|---|---|---|---|---|---|
| 1 | Test hole (wiring unverified) | `Source/Tests/GuiReachabilityTests.cpp` (pedal wildcard, morph credit) | `"pre*_p*"` also matches `preset_morph_position`, so that param was silently filed under "pedal slot params without a knob" and never checked. Once the wildcard was tightened, the check failed for two further reasons: the walk never turned Morph on (the slider is hidden until then; a toggle state change does not run `onClick`), and the morph slider writes far more than 3 ids, so the walk did not credit it. The control itself is fine. | Med | Use `pre?_p?` / `post?_p?`; enable morph before the browser opens; credit a slider whose first write is `preset_morph_position` | FIXED |
| 2 | Missing control | `Source/UI/AdvancedPanel.cpp` buildColumn1 | GUITAR column had no "Open in Workshop" button (gui-integration 4.1); the comment claimed the Workshop "does not exist", but the WORKSHOP tab does | Med | Add a TextButton calling `setWorkspaceTabNamed ("WORKSHOP")` | FIXED |
| 3 | Missing feature (engine-adjacent, RT) | `UI/FretboardComponent.cpp:119`, `PluginProcessor.cpp:2888` | String mute is not an APVTS param; the UI calls `setStringMuted` from the message thread on audio-thread state (CB-6/CB-17) | High | Command FIFO + `string_mute_*` params | LEFT: engine/RT-safety, other worker's domain |
| 4 | Missing control | `UI/Widgets.h:11` | No "Assign to macro" item in the knob context menu (GI-95); `macro_assign_a/b` exist | Med | Add the menu item to `LuthierKnob` | LEFT: touches every knob, needs design |
| 5 | Missing control | `Source/PluginEditor.cpp:354` | No Save/Discard/Cancel prompt before a preset or guitar load with unsaved edits (SM-44) | Med | Dirty flag + 3-button alert | LEFT: needs a PresetManager dirty-state decision |
| 6 | Missing control | `UI/HeaderBar.cpp` | No input/output meter in the header (GD-4/5, GI-16) | Med | Reuse `LevelMeter` | LEFT |
| 7 | Missing control | `UI/TunePianoRoll.cpp`, `PianoRollStrip.cpp` | Piano roll has no horizontal scroll (TB-28) | Med | Wrap in a Viewport | LEFT |
| 8 | Missing control | `UI/PracticePanel.cpp:2406` | Practice TAB tab is a static editor; no scrolling tab view with playhead (PT-41/42) | Med | Embed `RiffTabView` | LEFT |
| 9 | Missing control | `UI/EasyPanel.cpp` | No Easy 4-way Mute button (MR-16); `MuteGroup` exists elsewhere | Med | Reuse `MuteGroup` | LEFT |
| 10 | Missing param + control | `Parameters.h` (none) | No global bend source selector (MB-3) | Med | New `bend_source` param (append-only) + combo | LEFT: new host param changes the count in IntegrationTests |
| 11 | Missing visual | `UI/SetupGroup.cpp` | Buzz heatmap is not drawn on the guitar illustration (WU-9) | Med | Overlay layer in GuitarRenderer | LEFT: large |
| 12 | Test blind spot | `GuiReachabilityTests.cpp operate()` | Technique pill row (`bend_armed`, `scrape_armed`, `slap_armed`, `tap_armed`) is visible but the walk has no operator for it, so it is never operated | Low | Give the pill a public toggle and operate it | LEFT |
| 13 | Docs stale | `docs/audit/REMAINING.md`, `sweep_parts/` | About 120 GUI rows (TECHNIQUES tab, tap/bend/slide, mic view, RIFFS, preview, new preset browser, macros 7/8, pickup_blend) are listed MISSING but exist | Low | Regenerate the sweep | LEFT |
| 14 | Docs stale | `spec/TODO.md` | FirstRun / MuteGroup / Workshop family-switch / preset-thumbnail bullets are done | Low | Close them | LEFT: another worker owns TODO closures |
| 15 | Small gaps | `OptionsPages.cpp:2113`, `ToneMatchPanel.cpp`, `TunePanel` | Crash-report viewer (UT-17), default preset on load (SP-124), recent IR list (TM-16), artist/time-signature fields (TB-6) | Low | See A3 | LEFT |

No miswired control (bound to the wrong param) and no dead control was found. Label/behaviour mismatches were looked for by
checking that each control, when operated, wrote the parameter it names (575 did); tooltip wording was not audited line by line.

## `slap_rebound_gap`, host clock, B-*/RT docs, TODO closures, ON27 goldens

Not touched, as agreed with the other workers.

## Part A: spec completeness (verified)

The status column is derived from the existing sweep (`docs/audit/sweep_parts`), then corrected by a code spot-check of about 45
high-impact GUI rows. Rows tagged STALE in the notes (auto-articulation, global-search, gui-techniques-updates, mic-placement,
microtonal-bends, muting-rhythm, preset-browser-previews, riff-library, slide-technique-controls, two-hand-tapping) are
shown as MISSING by the sweep but are implemented; read those rows as PARTIAL or better.

### A1. Per-spec status

| spec | status | #done | #missing | #partial | note |
|---|---|---|---|---|---|
| CLAUDE_CODE_BRIEF | PARTIAL | 11 | 2 | 13 |  |
| DECISIONS | PARTIAL | 67 | 2 | 7 |  |
| GAPS | PARTIAL | 48 | 1 | 2 |  |
| INDEX | PARTIAL | 14 | 1 | 6 |  |
| JUCE_CLAUDE_GUIDELINES | PARTIAL | 23 | 2 | 5 |  |
| PROGRESS | PARTIAL | 40 | 1 | 3 |  |
| README | DONE | 32 | 0 | 0 |  |
| REVIEW | PARTIAL | 15 | 0 | 1 |  |
| TODO | PARTIAL | 45 | 4 | 8 |  |
| accessibility | PARTIAL | 27 | 7 | 14 |  |
| action-and-undo | PARTIAL | 35 | 0 | 2 |  |
| advanced-ranges | PARTIAL | 31 | 3 | 4 |  |
| ambiguity-resolutions | PARTIAL | 34 | 0 | 2 |  |
| animated-strings | DONE | 23 | 0 | 0 |  |
| auto-articulation | MISSING | 0 | 31 | 0 | STALE: AssistPill/AssistStyleBox in EasyPanel |
| bass-techniques | PARTIAL | 33 | 0 | 5 |  |
| body-coupling | PARTIAL | 32 | 2 | 1 |  |
| character-wear | PARTIAL | 31 | 0 | 2 |  |
| controllers | PARTIAL | 23 | 1 | 3 |  |
| cpu-quality-modes | DONE | 22 | 0 | 0 |  |
| docs-KEYBOARD_SHORTCUTS | DONE | 18 | 0 | 0 |  |
| docs-MIDI_EXPORT_LUTHIER_PROFILE | DONE | 21 | 0 | 0 |  |
| docs-PLAYING_TECHNIQUES | PARTIAL | 26 | 0 | 1 |  |
| docs-PRESET_FORMAT | DONE | 25 | 0 | 0 |  |
| docs-TROUBLESHOOTING | DONE | 18 | 0 | 0 |  |
| docs-USER_MANUAL | DONE | 51 | 0 | 0 |  |
| engine-technique-layer | PARTIAL | 5 | 11 | 9 |  |
| engine | PARTIAL | 68 | 2 | 6 |  |
| environment | PARTIAL | 34 | 1 | 1 |  |
| error-recovery | PARTIAL | 35 | 22 | 21 |  |
| factory-content | PARTIAL | 7 | 10 | 5 | content gaps real (presets, setlists, backing tracks) |
| file-formats | PARTIAL | 30 | 0 | 12 |  |
| fingerstyle-attack | PARTIAL | 40 | 2 | 1 |  |
| fret-buzz | PARTIAL | 30 | 1 | 1 |  |
| global-search | MISSING | 0 | 31 | 0 |  |
| gui-engine-dataflow | PARTIAL | 22 | 3 | 10 |  |
| gui-integration | PARTIAL | 89 | 10 | 38 | partly stale (macros 7/8, pickup_blend) |
| gui-techniques-updates | MISSING | 0 | 18 | 6 | STALE: TECHNIQUES tab, pill row, sub-tabs, mirrors now exist (UI/Techniques/*) |
| guitar-illustration | PARTIAL | 38 | 6 | 17 |  |
| guitar-workshop | PARTIAL | 23 | 0 | 4 |  |
| harmonic-realism | PARTIAL | 43 | 1 | 0 |  |
| host-integration | PARTIAL | 33 | 9 | 8 |  |
| include | PARTIAL | 22 | 0 | 5 |  |
| input-routing | PARTIAL | 14 | 4 | 7 |  |
| installer | PARTIAL | 16 | 10 | 13 |  |
| issues | PARTIAL | 10 | 1 | 1 |  |
| jam-mode | PARTIAL | 29 | 0 | 3 |  |
| licensing | MISSING | 0 | 0 | 3 |  |
| live-performance | PARTIAL | 41 | 0 | 4 |  |
| mic-placement | PARTIAL | 1 | 23 | 0 | STALE: MicPlacementView/Editor + Easy mic cards exist |
| microtonal-bends | MISSING | 0 | 25 | 3 | STALE: BendEngine + bend_armed exist |
| midi-export | PARTIAL | 33 | 0 | 5 |  |
| modulation-matrix | PARTIAL | 33 | 5 | 8 |  |
| muting-rhythm | MISSING | 0 | 24 | 2 | STALE: UI/MuteGroup + Techniques MUTE page exist |
| noise-floor | PARTIAL | 40 | 0 | 2 |  |
| notation-export | PARTIAL | 32 | 3 | 6 |  |
| onboarding | PARTIAL | 31 | 1 | 4 |  |
| output-normalization | PARTIAL | 26 | 0 | 4 |  |
| part-acoustics | PARTIAL | 39 | 7 | 7 |  |
| performance-budget | PARTIAL | 20 | 6 | 2 |  |
| piano-roll-chord-display | DONE | 32 | 0 | 0 |  |
| pick-noise | PARTIAL | 24 | 0 | 4 |  |
| practice-tools | PARTIAL | 37 | 7 | 6 |  |
| preset-browser-previews | PARTIAL | 1 | 36 | 1 | STALE: Presets/Preview/* + PresetBrowser/* (sidebar, rows, hover 300ms) exist |
| proposals-visual-polish | PARTIAL | 27 | 1 | 0 |  |
| qa-polish | PARTIAL | 46 | 10 | 19 |  |
| rhythm-engine | PARTIAL | 41 | 0 | 4 |  |
| riff-library | MISSING | 0 | 29 | 0 | STALE: RIFFS tab + RiffBrowser/RiffTabView exist |
| routing-io | PARTIAL | 30 | 1 | 1 |  |
| slide-guitar | PARTIAL | 30 | 2 | 1 |  |
| slide-technique-controls | PARTIAL | 1 | 17 | 5 | STALE: slide_pos_* params + SlideEngine settings exist |
| spec | PARTIAL | 83 | 11 | 32 |  |
| state-model | PARTIAL | 39 | 8 | 14 |  |
| string-aging | PARTIAL | 35 | 1 | 2 |  |
| string-interaction | PARTIAL | 21 | 2 | 2 |  |
| string-scraping | PARTIAL | 18 | 1 | 1 |  |
| string-slap-technique | PARTIAL | 17 | 1 | 4 |  |
| string-squeak | PARTIAL | 38 | 0 | 2 |  |
| strum-dynamics | PARTIAL | 31 | 1 | 1 |  |
| sustain-and-decay | PARTIAL | 33 | 0 | 1 |  |
| technique-cascade | PARTIAL | 2 | 13 | 2 |  |
| theme | PARTIAL | 8 | 1 | 5 |  |
| tone-match | PARTIAL | 27 | 5 | 5 |  |
| tune-builder | PARTIAL | 76 | 1 | 8 |  |
| tuning-stability | PARTIAL | 44 | 1 | 1 |  |
| two-hand-tapping | MISSING | 0 | 21 | 3 | STALE: TapEngine + tap_* params + TapTests exist |
| ui-wiring | PARTIAL | 30 | 7 | 21 |  |
| updates-telemetry | PARTIAL | 20 | 2 | 6 |  |
| volume-knob-interaction | PARTIAL | 31 | 1 | 2 |  |
| workshop-ui | PARTIAL | 35 | 1 | 8 |  |

### A2. Verified rows (sweep says vs code)

| spec REQ | doc says | code says | evidence | sev | fix |
|---|---|---|---|---|---|
| gui-techniques-updates GT-2/GT-3, DEC-43, CB-24 | TECHNIQUES tab/sub-tab rail not built | STALE: tab registered, 7 sub-tabs + CASCADE | UI/AdvancedPanel.cpp:1285,1303; UI/Techniques/TechniquesPanel.cpp:47-50 | Low (doc) | flip rows to DONE after one visual pass |
| GT-12/13/14 Easy pills, right-click opens sub-tab | absent | STALE: TechniquePillRow in Easy, onOpenSubTab wired | UI/EasyPanel.cpp:195,770,802; PluginEditor.cpp:141-147; TechniquePillRow.cpp:132 | Low | doc refresh |
| GT-16 technique fretboard overlays | absent | STALE: TechniqueOverlay exists | UI/Techniques/TechniqueOverlay.h:32 | Low | doc refresh; check reduced motion (GT-22) |
| GT-19 Uses Techniques filter chip | absent | STALE: sidebar filter | UI/PresetBrowser/PresetFilterSidebar.cpp:114,152 | Low | doc |
| GT-18, MR-15/16/17 Mute row/sub-tab | absent | PARTIAL-STALE: MuteGroup built, used in RhythmPanel and Techniques pages; Easy 4-way Mute button not found | UI/MuteGroup.cpp; UI/RhythmPanel.cpp:4; EasyPanel has no mute control | Med | add 4-way mute button to EasyPanel playing strip (MR-16) |
| TH-5/11/12/13/14/16 tapping | no TapEngine/tap_* params | STALE: engine and params exist | DSP/Techniques/TapEngine.h; Parameters.h:582-584; Tests/TapTests.cpp | Low | doc; check TH-14 square markers, TH-16 presets |
| MB-3/13/14/15 bends | no BendEngine | STALE: engine, bend_armed, source stack exist; no global PB/expr/CC source selector param (only StringBendSource enum) | DSP/Techniques/BendEngine.h:13,38; Parameters.h:594 | Med | add bend_source param + combo in Bend page (UI/Techniques/TechniquePages.cpp) |
| SL-1/11/12/13/15 slide control | no slide_pos_* params | STALE: params exist | Parameters.h:615-617; DSP/Slide/SlideEngine.h; UI/Techniques/TechniqueMirrors.cpp | Low | doc; verify presets SL-15 |
| MP-6.1/6.2 mic view/editor | absent | STALE | UI/AdvancedPanel.cpp:944,1238; UI/MicPlacementView.cpp, MicPlacementEditor.cpp | Low | doc |
| MP-6.3 Easy mic card | pad absent | PARTIAL-STALE: mic1/mic2/blend knobs in Easy | UI/EasyPanel.cpp:414-416 | Low | mic bright<->warm pad optional |
| RL-7.1-7.4 Riffs tab/save riff | not implemented | STALE: tab between TUNE and LIVE, R shortcut, Save-as-riff dialog | UI/AdvancedPanel.cpp:1294; PluginEditor.cpp:881-886; UI/RiffBrowser.cpp:1510 | Low | doc; check Riffs folder option, Easy drawer |
| PB-3/PB-4 preview render + player | none | STALE: renderer, service, player, gate, processBlock mix | Presets/Preview/*.cpp; PluginProcessor.cpp:2104-2106 | Low | doc |
| PB-7.1/7.2/7.4, PB-4.4 browser UI | missing | STALE: sidebar, rows with waveform, hover 300 ms | UI/PresetBrowser/PresetBrowserPanel.cpp:14,164; PresetRow.cpp:64,151 | Low | doc; PB-8 Options group exists (PresetBrowserOptions) |
| AA-7.1 Easy AUTO pill + style | not built | STALE | UI/EasyPanel.cpp:188-193 | Low | doc; AA-7.2/7.3 unverified |
| GI-1/GI-3 scrape_/slap_/macro_assign_a/b attachments | no UI | STALE for macro (knobs attached), scrape/slap pages exist | UI/ModMatrixPanel.cpp:878-886; Techniques pages | Low | doc |
| GI-117 / MM-28 macros 7/8 have no control | gap | STALE for 7/8 knobs; still no names/host-param uniformity | UI/ModMatrixPanel.cpp:885-886 | Low | doc |
| GI-36/SP-17 pickup_blend unattached | gap | STALE: knob attached | UI/AdvancedPanel.cpp:707 | Low | doc; per-pickup phase still absent |
| WU-9 / GI-23 buzz heatmap | missing | PARTIAL: SetupGroup draws a heatmap grid; no illustration overlay layer found | UI/SetupGroup.cpp:13,51,92; no heat layer in UI/Guitar* | Med | overlay layer in GuitarRenderer when Setup focused |
| CB-6/CB-17/SP-95 string mute not param-backed | gap | CONFIRMED: no string_mute_* param; UI calls processor.setStringMuted directly | UI/FretboardComponent.cpp:119-127; PluginProcessor.cpp:2888; Parameters.h none | High (RT) | SPSC command FIFO + params (CB-17 recipe) |
| GD-4/GD-5/GI-16 header input/output meter | LED only | CONFIRMED: no meter in HeaderBar | rg Meter UI/HeaderBar.* empty; LevelMeter only in Widgets/EasyPanel | Med | add LevelMeter to HeaderBar fed by existing output peak |
| GI-15 header snapshot strip | only in Live strip | CONFIRMED | SnapshotStrip in UI/LiveStrip.cpp:60-121 only | Low | reuse SnapshotStrip in header (Live mode) |
| GI-17/GD-25 header tap/kill/MIDI-learn | Live strip only | CONFIRMED: header has panic only | UI/HeaderBar.cpp:78-80 | Low | add tap LED to header |
| GI-32 Open in Workshop / library list | missing | WAS a gap: only a comment. **FIXED in this PR** (button added, test `openInWorkshopButtonShowsTheWorkshopTab`); the instrument-library list half is an Easy-mode surface, still open | UI/AdvancedPanel.cpp (buildColumn1) | Med | done |
| GI-95 Assign to macro menu | absent | CONFIRMED: enum entry only, no menu | UI/Widgets.h:11 | Med | add item in LuthierKnob context menu |
| GI-97 panel right-click | absent | CONFIRMED: no panel menu | UI/AdvancedPanel.cpp:187-197 is a different menu | Low | skip / later |
| CT-19 bend-range calibration UI | missing | CONFIRMED: only generic Calibrate in controller wizard | UI/OptionsPages.h:326; Controllers/ControllerProfile.cpp:407 | Low | extend wizard |
| PT-41/42/43 tab view scroll/cursor/speed trainer in TAB tab | static text editor | CONFIRMED: TAB tab in PracticePanel; RiffTabView has playhead but not wired to Practice TAB | UI/PracticePanel.cpp:2406; UI/RiffTabView.h:33-35 | Med | embed RiffTabView in PracticePanel TAB |
| PT-32 playlist UI | none | CONFIRMED: only backing folder/shuffle setup | Practice/PracticeRoutineSetup.cpp:112 | Low | list component in TRACK tab |
| TB-6 artist / time-signature in TUNE header | missing | CONFIRMED: no artist/timeSig in Tune UI | rg empty in UI/Tune* | Low | add two fields to TunePanel header |
| TB-9 variations A/B no UI | model only | PARTIAL-STALE: createSectionVariation wired in section strip | UI/TuneSectionStripEditing.cpp:52 | Low | A/B toggle + playback still absent |
| TB-28 piano roll horizontal scroll | missing | CONFIRMED: no Viewport/scroll in roll | rg empty UI/TunePianoRoll.cpp, PianoRollStrip.cpp | Med | wrap in Viewport |
| TM-16 recent IR list | missing | CONFIRMED | rg empty UI/ToneMatchPanel.cpp | Low | store 8 recent paths in UiPreferences |
| UT-17 crash-report viewer | missing | CONFIRMED: only upload toggle | UI/OptionsPages.cpp:2113-2129 | Low | button calling Telemetry describePendingCrashReport |
| SP-124 default-preset-on-load | missing | CONFIRMED | no pref in OptionsPages / PresetLibraryPrefs | Low | Options > Presets combo |
| SM-44 unsaved-bench Save/Discard/Cancel before load | missing | CONFIRMED for guitar load: only Save-guitar AlertWindow exists | PluginEditor.cpp:354 | Med | dirty flag + 3-button alert before preset/guitar load |
| IR-19 root file drop by type | .mid only | CONFIRMED | PluginEditor.cpp:1489-1501 | Low | route via Support/FileOpenRouter |
| IR-18/GI-13 drag part cards onto illustration | click-to-fit only | CONFIRMED: DragAndDropTarget only in Widgets.h:158,352; none in Workshop/Guitar | UI/Widgets.h | Med | large; later |
| GI-26 pickup pulse layer | missing | CONFIRMED | rg pulse empty in UI/Guitar* | Low | cosmetic |
| GI-70 glide target in tuning popover | missing | CONFIRMED | no glide in UI | Low | cosmetic |
| ER-6 unrecoverable modal | no modal | CONFIRMED: banners only | UI/Notifications.h:5,23 | Low | AlertWindow path |
| MP-6.5/PB-8 Options toggles | missing | PB-8 STALE (PresetBrowserOptions.cpp exists); MP-6.5 unverified | UI/PresetBrowser/PresetBrowserOptions.cpp | Low | doc |
| TC-9 conflict red-slash pill | absent | STALE: conflictFor + message in pill | UI/Techniques/TechniqueUi.cpp:87-103,263 | Low | doc |
| NE-27 notation export beside Save last take | partial | STALE-ish: PracticePanel has format box + export | UI/PracticePanel.cpp:1752,1863 | Low | doc |
| VK-24 CHARACTER CIRCUIT group | missing | CONFIRMED | rg empty UI/CharacterPanel.cpp | Low | mirror CircuitPanel |
| FC-2/9/10/15/16 factory content | missing | not re-verified; content tasks, not GUI | n/a | Med | content pass |

TODO.md Remaining bullets: Source/WIP FirstRun/MuteGroup is STALE (UI/FirstRun.cpp, FirstEncounterHint.cpp, MuteGroup.cpp now in UI/). G illustration "family-switch UI in Workshop drawer": STALE, family cards exist (UI/WorkshopPanel.cpp:1045,1378). "preset-browser thumbnails": thumbnails exist (Overlays GuitarThumbnails). 2k notation chordSymbol hook and part-acoustics 2.1 not re-verified (engine, out of scope).

### A3. GUI gaps worth wiring now (smallest first)
1. SM-44 Save/Discard/Cancel prompt before preset/guitar load: PluginEditor.cpp (~354 AlertWindow pattern), dirty flag from PresetManager.
2. GI-32 "Open in Workshop" button in GUITAR column: UI/AdvancedPanel.cpp ~425, use setWorkspaceTabNamed.
3. GI-95 "Assign to macro" item in knob context menu: UI/Widgets.h/.cpp (LuthierKnob menu), macro_assign_a/b exist.
4. GD-4/5, GI-16 header output (and input) meter: UI/HeaderBar.cpp reuse LevelMeter (UI/Widgets.h).
5. MR-16 Easy 4-way Mute button: UI/EasyPanel.cpp, reuse UI/MuteGroup.
6. TB-28 piano roll horizontal scroll: UI/TunePianoRoll.cpp, PianoRollStrip.cpp.
7. PT-41/42 tab view in Practice TAB: embed UI/RiffTabView in UI/PracticePanel.cpp (TAB at ~2406), highlight fret via FretboardComponent.
8. MB-3 bend source selector: Parameters.h, UI/Techniques/TechniquePages.cpp Bend page.
9. UT-17 crash viewer + SP-124 default preset pref + TM-16 recent IRs: UI/OptionsPages.cpp, UI/ToneMatchPanel.cpp.
10. WU-9 buzz heatmap layer on illustration: UI/SetupGroup.cpp data to UI/Guitar renderer.
11. CB-6/17 string_mute params + FIFO (High, engine-adjacent): Parameters.cpp, PluginProcessor.cpp:2888, UI/FretboardComponent.cpp:119, UI/AdvancedPanel.cpp StringRow.
Also: refresh docs/audit/sweep_parts for ~120 stale GUI rows (technique/riff/preview/mic/tap/bend/slide) so REMAINING.md stops overstating.
