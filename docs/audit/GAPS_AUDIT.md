# Spec gap audit

Auditor branch `claude/luthier-audit`. This refresh audits the tree after
realism-a/b/c, techniques' predecessors (model-gaps), tune-help, visual,
release and review landed on the integration branch
`claude/luthier-cloud-session-5lzlix` (groups A-I at `243c7f7`), plus the
nine feature specs that arrived with the feat-* branches (group J, at
`e4dee39`: feat-strings, feat-cpu, feat-jam merged; feat-assist, -browser,
-mic, -riffs, -search and -normalize still open). The only original helper
branch not landed is `claude/luthier-techniques`; items implemented only
there, or only on an open feat-* branch, are marked "in progress on
<branch>".

Method: every Markdown file under `spec/` and `spec/proposals/` was read in full
and split into actionable requirements (controls, behaviours, UI locations,
file fields, defaults, shortcuts, and every item in its Tests section). Each was
checked against `Source/`, `Resources/`, `Tools/`, `CMakeLists.txt` and
`scripts/`. A class existing is not "yes" unless something calls it; a
parameter is not GUI-reachable unless a visible control is attached to it.
GUI reachability was then checked mechanically by `GuiReach.*` (see
BETA_TEST_REPORT.md B-11), and that result overrides a desk check where they
disagree. Counts below are recounted from the per-requirement tables in
"Detail by spec file".

## Summary by spec file

"Partial" includes "in progress on <branch>". The previous audit (before the
helpers landed) stood at 1379 yes / 707 partial / 827 no of 2913 rows (47%).

| Spec file | Group | Yes | Partial / in progress | No | Rows | % done |
|---|---|---|---|---|---|---|
| spec.md | A | 128 | 48 | 10 | 186 | 69% |
| engine.md | A | 78 | 19 | 2 | 99 | 79% |
| include.md | A | 23 | 3 | 0 | 26 | 88% |
| theme.md | A | 19 | 9 | 6 | 34 | 56% |
| issues.md (user bug list) | A | 8 | 2 | 2 | 12 | 67% |
| README.md (spec/README) | A | 18 | 3 | 1 | 22 | 82% |
| TODO.md | A | 22 | 6 | 2 | 30 | 73% |
| CLAUDE_CODE_BRIEF.md | A | 8 | 10 | 4 | 22 | 36% |
| gui-integration.md | B | 87 | 31 | 14 | 132 | 66% |
| ui-wiring.md | B | 28 | 18 | 8 | 54 | 52% |
| gui-engine-dataflow.md | B | 15 | 17 | 3 | 35 | 43% |
| gui-techniques-updates.md | B | 2 | 4 | 25 | 31 | 6% |
| workshop-ui.md | B | 44 | 10 | 0 | 54 | 81% |
| proposals/visual-polish.md | B | 25 | 2 | 0 | 27 | 93% |
| onboarding.md | B | 33 | 4 | 1 | 38 | 87% |
| accessibility.md | B | 18 | 18 | 11 | 47 | 38% |
| piano-roll-chord-display.md | B | 29 | 3 | 0 | 32 | 91% |
| guitar-illustration.md | C | 60 | 30 | 11 | 101 | 59% |
| guitar-workshop.md | C | 35 | 9 | 1 | 45 | 78% |
| part-acoustics.md | C | 40 | 10 | 9 | 59 | 68% |
| advanced-ranges.md | C | 39 | 6 | 4 | 49 | 80% |
| tune-builder.md | C | 81 | 6 | 1 | 88 | 92% |
| routing-io.md | D | 22 | 3 | 1 | 26 | 85% |
| modulation-matrix.md | D | 28 | 9 | 3 | 40 | 70% |
| rhythm-engine.md | D | 25 | 3 | 0 | 28 | 89% |
| live-performance.md | D | 24 | 10 | 4 | 38 | 63% |
| controllers.md | D | 8 | 9 | 4 | 21 | 38% |
| practice-tools.md | D | 40 | 13 | 6 | 59 | 68% |
| tone-match.md | D | 18 | 5 | 6 | 29 | 62% |
| notation-export.md | D | 24 | 3 | 2 | 29 | 83% |
| character-wear.md | D | 22 | 8 | 0 | 30 | 73% |
| updates-telemetry.md | D | 13 | 6 | 2 | 21 | 62% |
| midi-export.md | D | 22 | 5 | 0 | 27 | 81% |
| input-routing.md | D | 8 | 10 | 7 | 25 | 32% |
| volume-knob-interaction.md | E | 21 | 3 | 1 | 25 | 84% |
| pick-noise.md | E | 20 | 6 | 1 | 27 | 74% |
| string-squeak.md | E | 21 | 2 | 2 | 25 | 84% |
| fret-buzz.md | E | 21 | 2 | 3 | 26 | 81% |
| slide-guitar.md | E | 22 | 2 | 3 | 27 | 81% |
| bass-techniques.md | E | 24 | 4 | 1 | 29 | 83% |
| string-aging.md | E | 21 | 0 | 2 | 23 | 91% |
| environment.md | E | 20 | 0 | 1 | 21 | 95% |
| body-coupling.md | E | 16 | 0 | 2 | 18 | 89% |
| harmonic-realism.md | F | 35 | 1 | 2 | 38 | 92% |
| string-interaction.md | F | 29 | 1 | 2 | 32 | 91% |
| fingerstyle-attack.md | F | 41 | 2 | 2 | 45 | 91% |
| noise-floor.md | F | 42 | 1 | 0 | 43 | 98% |
| sustain-and-decay.md | F | 36 | 1 | 1 | 38 | 95% |
| tuning-stability.md | F | 42 | 0 | 1 | 43 | 98% |
| strum-dynamics.md | F | 25 | 3 | 1 | 29 | 86% |
| string-scraping.md | G | 19 | 4 | 1 | 24 | 79% |
| slide-technique-controls.md | G | 1 | 0 | 17 | 18 | 6% |
| string-slap-technique.md | G | 18 | 6 | 1 | 25 | 72% |
| muting-rhythm.md | G | 0 | 4 | 16 | 20 | 0% |
| two-hand-tapping.md | G | 0 | 1 | 18 | 19 | 0% |
| microtonal-bends.md | G | 0 | 4 | 16 | 20 | 0% |
| technique-cascade.md | G | 1 | 3 | 7 | 11 | 9% |
| engine-technique-layer.md | G | 7 | 5 | 8 | 20 | 35% |
| ambiguity-resolutions.md | G | 22 | 2 | 0 | 24 | 92% |
| qa-polish.md | G | 30 | 13 | 7 | 50 | 60% |
| performance-budget.md | G | 18 | 7 | 2 | 27 | 67% |
| installer.md | G | 19 | 10 | 5 | 34 | 56% |
| file-formats.md | H | 20 | 25 | 8 | 53 | 38% |
| factory-content.md | H | 11 | 16 | 7 | 34 | 32% |
| error-recovery.md | H | 18 | 35 | 28 | 81 | 22% |
| state-model.md | H | 31 | 25 | 17 | 73 | 42% |
| host-integration.md | H | 32 | 19 | 8 | 59 | 54% |
| action-and-undo.md | H | 45 | 4 | 1 | 50 | 90% |
| licensing.md | H | 4 | 8 | 28 | 40 | 10% |
| editions.md | H | 3 | 1 | 30 | 34 | 9% |
| DECISIONS.md | I | 81 | 10 | 2 | 93 | 87% |
| GAPS.md | I | 32 | 2 | 3 | 37 | 86% |
| PROGRESS.md | I | 24 | 4 | 4 | 32 | 75% |
| INDEX.md | I | 13 | 6 | 2 | 21 | 62% |
| REVIEW.md | I | 14 | 2 | 0 | 16 | 88% |
| JUCE_CLAUDE_GUIDELINES.md | I | 23 | 12 | 2 | 37 | 62% |
| animated-strings.md | J | 74 | 13 | 1 | 88 | 84% |
| auto-articulation.md | J | 0 | 87 | 3 | 90 | 0% |
| cpu-quality-modes.md | J | 66 | 16 | 1 | 83 | 80% |
| global-search.md | J | 0 | 74 | 7 | 81 | 0% |
| jam-mode.md | J | 97 | 10 | 1 | 108 | 90% |
| mic-placement.md | J | 0 | 89 | 2 | 91 | 0% |
| output-normalization.md | J | 75 | 13 | 0 | 88 | 85% |
| preset-browser-previews.md | J | 0 | 84 | 5 | 89 | 0% |
| riff-library.md | J | 0 | 88 | 5 | 93 | 0% |
| **Total** | | **2328** | **1082** | **438** | **3848** | **60%** |

Like for like, groups A-I stand at **2016 yes / 608 partial / 413 no of 3037
rows (66%)**, up from 47%; the specs of the landed helpers (groups E and F)
went from 0-3% to 83-98%. Group J adds 811 rows from nine new feature specs:
the four whose branches landed are 80-90% done, and the five still on open
feat-* branches (auto-articulation, global-search, mic-placement,
preset-browser-previews, riff-library) count as "in progress" (0% on HEAD,
near complete on their branches), which is what pulls the overall figure
down to 60%.

## Helper branch status

| Branch | Landed on integration? | Covers | Notes |
|---|---|---|---|
| realism-a | yes | string-aging, environment, body-coupling | 91-95% done; body coupling 0 in every factory preset |
| realism-b | yes | harmonic-realism, string-interaction, fingerstyle-attack | 91-92% |
| realism-c | yes | noise-floor, sustain-and-decay, tuning-stability | 95-98%; every realism feature ships off in factory presets |
| model-gaps | yes | bass techniques, notation capture, part fields | bass-techniques 83% |
| tune-help | yes | TUNE, onboarding, HELP | tune-builder 92%, onboarding 87% |
| visual | yes | workspace panels, illustration, drag and drop | visual-polish 93% |
| release | yes | installers, CI, licences | installer 56% (unsigned without secrets) |
| review | yes | review fixes | |
| feat-strings, feat-cpu, feat-jam | yes | animated-strings, cpu-quality-modes, jam-mode | 80-90%; animated strings ignore the CPU relief level (J top gap 2) |
| feat-normalize | yes (1 commit ahead) | output-normalization | 85% |
| **techniques** | **no** (12 ahead; 21 conflicting files per group G) | TECHNIQUES tab, scrape/slap controls, mute, tap, bend, cascade | gui-techniques-updates 6%, muting/tapping/bends 0% on HEAD |
| feat-assist | no | auto-articulation | complete on branch; conflicts with riffs (`NoteOnEvent::explicitArticulation`) |
| feat-browser | no | preset-browser-previews | previews bypass output-normalization |
| feat-mic | no | mic-placement | rewrites `CabinetEngine::processBlock` that feat-cpu changed |
| feat-riffs | no | riff-library | claims the same tab slot as JAM |
| feat-search | no | global-search | its fixed tab list lacks JAM and RIFFS (GS-03) |
| spec-sweep | no (25 ahead) | spec text only | |

## Top gaps across the product

Ranked by what a buyer meets first. B-numbers refer to BETA_TEST_REPORT.md.

1. **Techniques still not on HEAD.** 27 automatable parameters (14 `scrape_*`, 13 slap arm/trigger) have no control, so `GuiReach.everyAutomatableParameterHasAVisibleControl` fails; no TECHNIQUES tab, mute, tap, bend or cascade. Help and global search already describe the missing tab. In progress on techniques.
2. **Nothing stops everything** (issues.md 9, B-14): Panic and Reset leave looper, backing track, tune player, progression looper, metronome and rhythm running.
3. **Realism ships switched off.** Sustain style Legacy, noise 0, stability 0 and body coupling 0 in every factory preset: a buyer hears none of the realism work without opening CHARACTER.
4. **Idle CPU about equal to playing CPU** (B-13); cpu-quality-modes landed but there is still no silence short-circuit.
5. **Presets lose state**: a `.luthierpreset` drops the modulation matrix, snapshots, MIDI Learn, rhythm, character and tone-match state; setlist snapshot recall reads a stale bank.
6. **No MIDI output in VST3/AU** (`NEEDS_MIDI_OUTPUT FALSE` while `producesMidi()` is true).
7. **Dead or unreachable controls**: `pickup_blend` (never read), Macro 7/8, `tune_feel_mod`/`tune_tempo_drift`, the Advanced "Sustain" knob (`sustain_scale`, deferred by the spec to the factory re-voicing pass), backing-track pitch/tempo, expression calibration, controller-profile MPE/bend overwritten every block, character "aged electronics" and body age.
8. **B-15: "Old" strings silence a bass above about A3** (P-Bass Flatwound).
9. **Scroll UX and the RHYTHM tab**: wheel over a knob moves the knob; the STRUM group's lower rows never get height (B-11).
10. **Guitar illustration and Workshop**: a body swap never redraws the outline; no finish/colour editor; four Workshop slots cannot be changed.
11. **Five finished feature specs wait on unmerged branches** with known merge conflicts (tab slot, `explicitArticulation`, parameter-order tests, `CabinetEngine::processBlock`).
12. **Localisation English only**; no `Resources/i18n`.
13. **Licensing/editions**: placeholder activation, no Options page, no Free/Pro code (licensing 10%, editions 9%).
14. **Error recovery thin** (22%): most specified banners, fallbacks and repair paths missing; no crash minidump writer.
15. **Window contract**: 940x560 minimum with locked aspect; spec says 1280x800-2560x1600 with reflow.
16. **Threading**: the UI thread calls `engine.panic()`, string mute and detune directly; the audio thread takes a CriticalSection on a live part swap.
17. **Release blockers**: `.example` support links by default, placeholder EULA, unsigned installers without CI secrets, pluginval thread-safety test over its default timeout (B-12).
18. **Stale live data**: the environment SysEx/MIDI-export still sends the legacy temperature/humidity.

## Unspecified gaps (no spec covers them) with proposed spec outlines

Things a buyer of a $200 guitar instrument expects that no spec file asks
for. Each outline is a starting point for the coordinator to turn into a
spec file.

### U-1 Chromatic tuner (`spec/tuner.md`)
- Where: header button opening an overlay, plus Live Mode strip; shortcut `U`.
- What it measures: the plugin's own output (string-activity pitch, per string), and optionally the sidechain input (a real guitar or a backing track).
- Display: needle and strobe modes, cents to 0.1, note name, per-string row for the current tuning and capo; reference follows `concert_a` and temperament.
- Tuner-mute (already named in spec.md): mutes output while the tuner is open, option.
- Tests: a 440 Hz string reads A4 +-0.5 cent; a +10 cent realism detune reads +10 +-1; mute silences output within one block.

### U-2 Noise gate on the amp path (`spec/noise-gate.md`)
- A NoiseGate pedal type exists but no factory high-gain preset loads one, and the idle floor reaches -22 dBFS at high gain (B-04).
- Specify: a gate in the MASTER/amp section (pre-amp, keyed from the guitar signal), threshold/release/range, a "Gate" toggle on the Easy tone strip, on by default in every high-gain factory preset, and an idle-floor ceiling (e.g. -60 dBFS with the gate on) as a test.

### U-3 Preset browser: search, tags, favourites, ratings (`spec/preset-browser.md`)
- Status: partly specified since by `global-search.md` and `preset-browser-previews.md` (both in progress on feat-search / feat-browser); favourites, ratings and stable factory program numbers are still unspecified.
- Text search across name/author/tags; tag filter chips; favourite star and a Favourites view; stable factory program numbers for hosts (user presets appended after, not interleaved); unsaved-changes prompt before loading another preset; "restore factory content" button.
- Tests: search latency < 50 ms with 2000 presets; favourites persist; saving a user preset does not renumber factory programs.

### U-4 Undo history list (`spec/undo-history.md`)
- A menu/panel listing named steps (the descriptions already recorded), click to jump; routing, modulation, snapshot and Workshop edits all on the stack; tune edits on their own stack.

### U-5 On-screen keyboard and technique audition (`spec/audition.md`)
- Status: the playable keyboard is now built (piano-roll-chord-display, 91%); technique demo phrases and the keyswitch legend are still unspecified.
- Playable keyboard (overlaps piano-roll spec section 3; merge there), per-technique demo phrases (scrape, slap, tap, bend, harmonics, palm mute), keyswitch legend showing what KS 12-21 and articulation keys do, a harmonic trigger reachable without CC controllers.

### U-6 Left-handed mode (`spec/left-handed.md`)
- Mirror the illustration, fretboard and Workshop bench; strum direction semantics unchanged; saved per user.

### U-7 Out-of-range note policy (`spec/note-range.md`)
- Notes below the lowest open string (after capo) or above the last fret: options Drop (with a one-time notice), Octave-fold (default), Nearest string; notice in the status line. Today they are silently dropped (B-09).

### U-8 Quality / CPU mode and meter (`spec/quality-modes.md`)
- Status: now specified by `cpu-quality-modes.md` (landed, 80%); the idle short-circuit is still missing (B-13).
- Eco / Realtime / Render quality switch in Easy mode (oversampling, body IR length, noise pool size), a CPU and voice meter in the header, an overload indicator tied to performance-budget section 8's relief mechanisms, and an idle short-circuit (silent strings, silent input -> skip body/amp/cab/room after their tails; B-13).

### U-9 Deterministic render (`spec/determinism.md`)
- Status: B-08 is fixed and `Combo.renderIsDeterministicAfterReset` / `renderDoesNotDependOnWhatWasPlayedBefore` guard it; an offline render flag and a spec are still missing.
- Every random source (humanise, noise pools, character wear, realism detune) seeded from state, so the same session renders bit-identically in any instance and after reset; offline render mode flag. Test: B-08's harness at zero difference.

### U-10 Parts and guitar library management (`spec/library-management.md`)
- Rename, duplicate, delete, import/export `.luthierguitar` bundles and parts, reveal folder, rescan; a preset bundle (preset + guitar + IRs) for sharing.

### U-11 DI / re-amp input (`spec/reamp.md`)
- Process a real guitar through the rig (amp, cab, room, pedals) from the sidechain input, with the modelled guitar muted; include.md hints at an effects version.

### U-12 MIDI monitor (`spec/midi-monitor.md`)
- A view showing each incoming event and which consumer took it (input-routing's order), for "why isn't MIDI Learn seeing my CC".

### U-13 Drag-out clips from TUNE (`spec/tune-dragout.md`)
- Drag a section or the whole tune as MIDI or audio straight into the DAW.

### U-14 Factory realism voicing (`spec/factory-voicing.md`)
- Every realism feature (sustain style, noise floor, stability, body coupling) and the Advanced Sustain knob wait on a listening pass nobody owns. Specify per-preset targets and a render-based acceptance test so the realism work is heard by default.

### U-15 Merge-order contract for feature branches (`spec/branch-merge-order.md`)
- Tab slots, parameter-order tests and shared structs (`NoteOnEvent`) collide between feat-* branches. Specify tab-slot allocation, append-only parameter blocks per feature and an owner per shared struct.

## Small glue candidates

Engine features that lack only an attachment, a call, or a preset field. Each
group's own "Small glue candidates" table in the detail section has the full
list with exact IDs.

| Item | IDs / API | Where | Owner / status |
|---|---|---|---|
| Macro 7/8 knobs | `macro_assign_a`, `macro_assign_b` | MOD tab macro card; Easy strip | open |
| Pickup blend | `pickup_blend`; `PickupEngine` must read `blendAmount` | Col 2 PICKUPS | open |
| Tune feel/tempo drift | `tune_feel_mod`, `tune_tempo_drift` (mod-only?) | TUNE or `intentionallyHidden` | open (decide) |
| Legacy string age | `string_age` into GuiReach `intentionallyHidden` (superseded by `string_age_hours`) | Tests | open |
| RHYTHM panel height | `RhythmPanel::preferredHeight()` into `AdvancedPanel::resized` | AdvancedPanel | open |
| Stop everything | looper, backing track, tune player, metronome, progression, rhythm into `LuthierAudioProcessor::panic()` | PluginProcessor | done (PR #2 port; `ResetStop.*`) |
| Backing track pitch/tempo | read `pitchSemis`/`tempoRatio` in the renderer | BackingTrack | open |
| Controller profile MPE/bend | stop `ParameterBridge::applyToEngine` overwriting them each block | Parameters.cpp | open |
| Environment SysEx | send `env_temperature_c` / `env_humidity_pct` instead of the legacy enum | PluginProcessor | open |
| Chambering air mode / gloss | route through `BodyEngine::setRuntimeScaling` | LuthierEngine | open |
| Animated strings follow CPU relief | call `StringAnimator::setReliefLevel`; register with `AnimationPolicy` | feat-strings / feat-cpu | open |
| Root file drop | pass every Luthier type to `FileOpenRouter::open` | PluginEditor | open |
| MIDI output flag | `NEEDS_MIDI_OUTPUT TRUE` | CMake | open (host decision) |
| "Amp Buzz" label | rename to "Single-coil Hum" | AdvancedPanel.cpp:815 | open |
| Done on this branch | engine lock (B-01), preset-morph in state (B-10), declick (B-06), parts guitars (B-05), release damping (B-03), compiled guitar after restart (B-17) | | fixed |

# Detail by spec file

Groups: A core (spec, engine, include, theme, issues, README, TODO, brief);
B GUI (plus piano-roll-chord-display); C illustration, Workshop, parts,
ranges, tune builder; D phase-1 extensions; E phase-2 realism; F extended
realism; G techniques, QA, performance, installer; H deep integration
(plus licensing, editions); I meta files; J feature specs from the feat-*
branches.

---

## Group A: core

Audited: local `claude/luthier-audit` at 243c7f7. The previous audit was at f63a7f7. Since then 200 commits have landed, including the merges of realism-a, realism-b, realism-c, model-gaps, tune-help, visual, release and review. The only unmerged branch is `origin/claude/luthier-techniques` at 1b8bf87, read with `git show`. Items that exist only there are marked "in progress on techniques". None of the eight group-A spec files changed between the two commits, so the rows are the same and only their status was re-checked.

Abbreviations: AP = `Source/UI/AdvancedPanel.cpp`, EP = `Source/UI/EasyPanel.cpp`, HB = `Source/UI/HeaderBar.cpp`, PB = `ParameterBridge::applyToEngine` in `Source/Parameters.cpp`, LE = `Source/LuthierEngine.cpp`, PP = `Source/PluginProcessor.cpp`, CHAR = the CHARACTER workspace tab (`CharacterPanel`), Col1/2/3 = the Advanced columns, "tab X" = a column-4 workspace tab. Test names are `Suite.test` from `Source/Tests/*`, which now holds 1198 tests (743 before). Tests were not run, because a build is running. Pass/fail notes come from `docs/audit/BETA_TEST_REPORT.md`.

**Parameter reachability** was re-checked mechanically. Every `ParamIDs::` constant and its ID string was grepped against `Source/UI/**` and `PluginEditor*.cpp`, and the results were cross-checked with `GuiReach.everyAutomatableParameterHasAVisibleControl`, which lists the parameters that are deliberately hidden. **Still no attached control on HEAD:**

- `macro_assign_a`, `macro_assign_b`.
- `pickup_blend`, which is also still dead: `PickupEngine::blendAmount` is set but never read in process.
- All 14 `scrape_*`.
- 13 of the 25 slap parameters: `slap_armed`, `slap_type`, `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`, `slap_ghost_cc`, `slap_force`, `slap_palm_position_mm`, `slap_string_mask`, `slap_ghost_mode`, `slap_rebound_gap`, `slap_snap_back`, `slap_body_part`. The other 12 now have controls in `SlapGroup` on CHAR, shown for bass only. The techniques branch attaches both sets (`TechniqueUi.cpp`, `TechniquePages.cpp`).
- New since the previous audit: `tune_feel_mod` and `tune_tempo_drift`, which are automatable and have no control.
- `string_age`, which is now inert and was replaced by `string_age_hours`, but is not in `intentionallyHidden()`.
- `noise_floor_style` and `sustain_style`, which are written by the style combo boxes through `applyNoiseFloorStyle` and similar, not through an attachment.

The legacy parameters `fret_action`, `strum_speed`, `feedback_on/threshold/speed` and `doubler_on/amount` are documented as intentionally hidden.

---

### spec.md

**Summary:** 186 requirements. **128 yes / 48 partial / 10 no** (was 123 / 47 / 16).

What moved to yes:
- Pluginval L10 in CI.
- The AU target, macOS only.
- 50/60 Hz mains hum.
- Two-stage decay.
- Artificial-harmonic trigger (CC103).
- Per-string pick/finger tool (`rh_string_tool_N`).

What moved from no to partial:
- Per-string material and gauge, now through Workshop string overrides.
- Pickup handling noise, now through `noise_microphonics` and `noise_cable_movement`.
- Signed installers: the scripts sign when secrets are present.
- Audio drag-out, from the Practice take and MIDI OUT.
- Easy live notes, through the piano-roll strip.

Still open:
- Per-string fretless, custom frets and per-fret offsets.
- A clickable fretboard in Easy mode.
- `pickup_blend`.
- Custom amp controls.
- A tuner or tuner-mute.
- Stop-everything.
- Pole spacing.
- Chord fingering editing.
- Humanise vibrato variation.
- The two preference items.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Waveguide per string, up to 12 strings | 1 String | yes | DSP/String/StringEngine; LE numStrings | n/a | StringEngine.pluckProducesCorrectPitch; GuitarLibrary.twelveStringCoursesAreOctavePaired | |
| Scale length 500-750 mm | 1 | partial | GuitarSpec.scaleLengthMm, Workshop neck part | Workshop tab (part swap / inspector) | WorkshopBench.* | Still no direct scale control |
| Tension from pitch, mass, length | 1 | yes | StringMaterials; LE getStringTensionNewtons | Col1 string rows (display) | StringPhysics.standardSetsLandInTheUsualTensionRange | |
| Linear density from gauge and material | 1 | yes | StringMaterials | Col1 String Set | StringPhysics.thickerStringsAreHeavierAndTighter | |
| Young's modulus / stiffness | 1 | yes | StringMaterials | no (derived) | StringPhysics.woundStringsAreLessStiff… | |
| Frequency-dependent damping | 1 | yes | StringEngine loop filter | Col1 Sustain `sustain_scale` | StringEngine.higherNotesDecayFaster | |
| Continuous fretting position 0-24 | 1 | yes | TuningEngine fretPosition | n/a | Tuning.fretPositionIsContinuous | |
| Pluck position | 1 | yes | `pluck_position` → Excitation | Col2 Playing Hand | — | |
| Pluck strength = velocity | 1 | yes | Excitation velocity | n/a | StringEngine.harderPluckIsBrighterNotJustLouder | |
| Pluck material pick/thumb/nail/thumbpick/brush | 1 | yes | Excitation::Material; RightHand.h RhTool | Col2 "Pick / Finger"; CHAR RIGHT HAND | FingerstyleAttack.FA10_perStringTools | |
| Pick 2-4 ms bright asymmetric excitation | 1 | yes | Excitation::specFor | n/a | FingerstyleAttack.FA05_thePickPathIsUntouched | |
| Finger 5-8 ms, thumb low-passed, nail vs pad | 1 | yes | Excitation; `nail_vs_flesh`, `finger_flesh/nail_release_ms` | Col2; CHAR RIGHT HAND | FingerstyleAttack.FA01_FA02…, FA03… | |
| Inharmonicity B per string, wound > plain | 1 | yes | StringEngine dispersion allpass | no | StringEngine.dispersionStretchesPartialsSharp | |
| Sympathetic NxN coupling matrix | 1 | yes | DSP/Coupling/CouplingMatrix, plus BodyCouplingBank (new) | Col1 `coupling_amount`; CHAR `body_coupling_*`, `coupling_air_amount` | Coupling.*; BodyCoupling.BC07_sympatheticRingThroughTheBody | |
| Two-stage decay (fast HF 200 ms, then slow) | 1 | yes | StringEngine two-stage knee; `sustain_fast_share`, `sustain_fast_ratio` | CHAR SUSTAIN group (RealismGroupsC) | SustainDecay.twoStageKnee, SustainDecay.kneeDepth | Merged from realism-c |
| Sustain per string (Advanced) | 1 | partial | `sustain_scale` global; string aging per string (restring one) | Col1 (global); CHAR STRING AGING per-string restring | StringAging.SA09_restringOneMakesOnlyThatStringNew | Still no per-string sustain control |
| Body: convolution with IR | 2 Body | yes | BodyEngine convolution | Col1 Body Mode | Engine.everyGuitarTypeLoadsAndSounds | |
| Body IR library for every type | 2 | yes | Resources/BodyIRs (216) | n/a | IrReload.theFirstNoteAfterALoadIsEveryNote | Synthesised |
| Modal bank 20-40 modes | 2 | yes | BodyEngine resonators | Body Mode = Modal | Body.modalBankReproducesTheAirResonance | |
| Modes shift with dimensions | 2 | yes | BodyModels; `body_width/depth` | Col1 Body | Body.dimensionsMoveTheModes | |
| Wood type (8 woods) with modal profile | 2 | yes | Wood enum | Col1 top/back wood | — | |
| Body size small…parlor | 2 | partial | BodyShape via Workshop body part | Workshop tab | — | No size selector |
| Body depth | 2 | yes | `body_depth` | Col1 Body | — | |
| Top thickness | 2 | yes | `body_top_thickness` | Col1 Body | — | |
| Bracing incl. solid/semi/hollow | 2 | yes | `body_bracing` | Col1 Body | — | |
| Sound-hole size | 2 | yes | `body_soundhole` | Col1 Body | — | |
| Air resonance frequency | 2 | partial | `body_air_gain`; `body_mode_freq_scale` (all modes); EnvironmentModel moves the air mode | Col1 Body; CHAR BODY COUPLING / ENVIRONMENT | Environment.ENV04_theAirModeFollowsTheSpeedOfSound | Still no direct air-mode frequency control |
| Age simulation | 2 | yes | `body_age` | Col1 Body | — | |
| Pickup types (SC, HB, P90, piezo, soundhole, mic) | 3 Pickup | yes | PickupEngine; `pickupN_type` | Col2 Pickups | Pickup.* | |
| Blended piezo + mic balance | 3 | yes | `piezo_mic_blend` | Col2 Pickups | — | |
| Position along string | 3 | yes | Workshop placement | Workshop bench drag | Pickup.positionCombNullsTheExpectedHarmonic | |
| Coil count / spacing | 3 | partial | HB 2 coils; `pickup_aperture_scale` (new) | Workshop inspector; CHAR | — | |
| Coil R / L / C | 3 | partial | Part fields → GuitarCircuit | Workshop inspector (RealismUi shows derived figures) | Pickup.resonantFrequencyMatchesTheLcrValues; RealismUi.theWorkshopInspectorShowsTheDerivedFigures | Raw part fields only |
| Magnet type EQ | 3 | yes | `pickupN_magnet` | Col2 Pickups | — | |
| Pole spacing | 3 | no | — | no | — | |
| Height above strings | 3 | yes | Workshop placement height | Workshop bench | — | |
| Pickup selector positions | 3 | yes | `pickup_selector` | Col2; illustration switch | — | |
| Continuous blend knob | 3 | no | `pickup_blend` → `PickupEngine::setBlend`; `blendAmount` never read in process | no | — | Still dead (BETA B-11) |
| Coil-tap | 3 | yes | `coil_tap` | Col2 Pickups | — | |
| Fretted mode snaps to fret | 4 Fret | yes | TuningEngine / MidiInterpreter | n/a | Engine.chromaticScalePlaysAtTheRightPitch | |
| 24 frets default, 12-27 per type | 4 | yes | GuitarSpec.maxFrets | n/a | GuitarLibrary.everyEntryIsInternallyConsistent | |
| Temperaments (just, meantone, well, custom) | 4 | partial | Temperament enum; `customTemperament` preset field | Col1; headstock popover | Tuning.temperamentsDifferButStayInRange | No custom-ratio editor |
| Fret noise | 4 | yes | `noise_fret` | Col2 String Noise | NoiseTests | |
| Fret buzz physical | 4 | yes | FretBuzz; setup geometry | CHAR SETUP | Buzz.*; Environment.ENV08_aDryNeckBuzzesMore | |
| Fret action height | 4 | yes | `setup_action_treble/bass` | CHAR SETUP; Workshop | Buzz.* | |
| Fretless mode continuous | 4 | yes | `fretless` | Col1 Neck | Engine.fretlessModeIsGenuinelyContinuous | Global only |
| Fretless: no buzz, softer attack, less sustain | 4 | partial | Technique fretless glide | n/a | Technique.fretlessTurnsLegatoIntoGlide | Buzz/attack differences not tested |
| Bend (cents, multi-string) | 4 Tech | yes | pitch bend / MPE per string | n/a | StringEngine.bendIsSmoothAndReachesTarget | Microtonal bend controls in progress on techniques |
| Pre-bend | 4 | partial | Only works as a bend sent before note-on | no | — | `bend_prebend_*` in progress on techniques (BendTests.cpp) |
| Vibrato rate/depth | 4 | yes | `vibrato_rate/depth` | Col3 Performance | — | |
| Vibrato shapes incl. classical / blues | 4 | partial | `vibrato_shape`: Sine, Tri, Square, Saw, Random, Finger | Col3 | — | Classical and blues still missing |
| Slide legato vs picked | 4 | yes | TechniqueEngine Slide | n/a | Technique.slideDurationScalesWithDistance | |
| Hammer-on / pull-off | 4 | yes | TechniqueEngine | n/a | Technique.legatoBecomesHammerOnAndPullOff | |
| Palm mute, depth | 4 | yes | CC67 → Damping::PalmMute; `palm_mute_spread`, `thumb_palm_mute` (new) | CC only for the mute itself; CHAR STRING INTERACTION spread | StringEngine.palmMuteShortensAndDarkens; StringInteraction.SI04_SI05_palmSpreadCoversAndLifts; BassTechniques.aBassPalmMuteIsShorterAndDarker | MUTE pill/page (`mute_*`) in progress on techniques |
| Natural harmonic | 4 | yes | Excitation Harmonic; Harmonics.h touch model | CC73 trigger; CHAR PICK › HARMONICS touch params | HarmonicRealism.HR01…HR08 | |
| Pinch harmonic | 4 | yes | CC72; `pinch_thumb_offset_mm` | CC; CHAR HARMONICS | HarmonicRealism.HR10_pinchHarmonic | |
| Artificial harmonic | 4 | yes | MidiTarget::ArtificialHarmonic on CC103; `artificial_harmonic_offset` | CC trigger; CHAR HARMONICS offset | HarmonicRealism.HR09_artificialHarmonics | Was partial |
| Tapping | 4 | yes | CC74 / Tap; `tapped_harmonic_offset` | CC only | TechniqueTriggers.*; HarmonicRealism.HR11… | TAP page (`tap_*`) in progress on techniques |
| Slide guitar (bottleneck) toggle | 4 | yes | SlideEngine; `slide_guitar` | Header Slide; CHAR SLIDE; Workshop slide accessory | Slide.*; WorkshopAccessories.theSlideTurnsOnTheBench… | |
| Whammy vintage / locking / transposing, dive | 4 | yes | WhammyEngine; `bridge_type` | Col1 Bridge | Whammy.transTremPreservesChordIntervals | |
| String / pick scrape | 4 | partial | ScrapeEngine wired in PB | **no** on HEAD (14 params unattached) | Scrape.* | TECHNIQUES › SCRAPE in progress on techniques |
| Muted picking | 4 | yes | Technique MutedPick; CC71 | CC only | — | |
| Pick or fingers, per string or per note | 5 Right hand | yes | `use_fingers`; `rh_string_tool_N`, `rh_style`, `rh_stroke` (RightHand.h) | Col2; CHAR RIGHT HAND string cells; Easy Tool selector | FingerstyleAttack.FA10_perStringTools, FA16_styleWritesOnce | Per string, not per note (per-note possible via CC triggers FA15) |
| Pick material (6) | 5 | yes | Excitation materials | Col2 | — | |
| Pick thickness | 5 | yes | `pick_thickness` | Col2; CHAR PICK | PickNoise.* | |
| Pick angle | 5 | yes | `pick_angle` | Col2; Workshop pick accessory | WorkshopAccessories.thePickIsDraggedAndTurnedOnTheBench | |
| Pick position in mm | 5 | partial | `pluck_position` normalised; `thumb_position_offset` | Col2; Workshop pick drag | — | Not in mm |
| Fingers thumb…little, finger-to-string assignment | 5 | partial | RhTool per string; Rhythm patterns Finger | CHAR RIGHT HAND; RHYTHM fingerpick grid | FingerstyleAttack.FA09_patternFingersReachTheString | Tool per string, not per finger |
| Nail vs flesh | 5 | yes | `nail_vs_flesh` | Col2; CHAR RIGHT HAND mirror | FingerstyleAttack.FA01_FA02… | |
| Strum direction and delay 2-15 ms | 5 | yes | `strum_direction`, `strum_crossing_sps` | Col3; RHYTHM STRUM | StrumDynamics.* | |
| Strum speed | 5 | yes | `strum_crossing_sps` | Col3; STRUM | StrumDynamics.* | Lower STRUM rows can get zero height (B-11) |
| Finger slide squeak (speed, wound) | 6 Noise | yes | NoiseEngine squeak | CHAR STRING NOISE | Squeak.*; StringAging.SA13_squeakReconciliation | |
| Pick attack transient | 6 | yes | pick click/chirp | CHAR PICK; Col2 | PickNoise.* | |
| Fret noise | 6 | yes | `noise_fret` | Col2 | — | |
| Release noise | 6 | yes | `noise_release` | Col2 | — | |
| Body knock | 6 | yes | `noise_body_knock` | Col2 | — | |
| Pickup handling noise | 6 | partial | NoiseFloor `noise_microphonics`, `noise_cable_movement` | CHAR NOISE FLOOR | NoiseFloor.microphonicsIsBounded; NoiseFloor.cableMovementRollsAndScales | Was no; no hand-on-pickup event |
| Amp buzz 60 Hz for single-coils | 6 | yes | `noise_amp_buzz`; `noise_mains_hz` 50/60 | Col2; CHAR NOISE FLOOR; Options › Audio mains region | NoiseFloor.regionSetsTheHumFrequency | |
| Global and per-type mechanical volume | 6 | partial | per-type knobs; `noise_floor_style`; macro_character | Col2; CHAR | NoiseUi.*; NoiseFloor.styleOffIsInert | Still no single global mechanical-noise level |
| 11 electric + 8 acoustic + 5 bass + custom types | Guitar types | yes | GuitarLibrary (24 + Custom) | Header; thumbnails | Engine.everyGuitarTypeLoadsAndSounds; Thumbnails.everyFactoryPresetShowsItsGuitar | |
| Custom guitar from scratch; save as user preset | Guitar types | yes | Workshop parts; Save As Guitar | tab WORKSHOP | WorkshopPresets.* | |
| 13 string materials | Strings | partial | StringMaterial (12); `string_coating` (new) | Col1 String Set | StringPhysics.* | "Roundwound" not a separate entry |
| Gauges XL…Heavy + custom per string | Strings | partial | StringGauge; per-string `StringOverride.gaugeIn` | Col1 (set); Workshop per-string override | WorkshopStrings.aPerStringOverrideIsSeenReadHeardAndOneEntry | Custom per-string gauge is now editable in Workshop only |
| String age fresh / broken-in / old | Strings | yes | `string_age_hours` (+ accrual, coating, corrosivity); StringAging | Col1 "Age (h)"; CHAR STRING AGING | StringAging.SA01…SA16 | `string_age` is inert. BETA B-15 reports Old nearly silencing a bass above E3, status not re-verified |
| Material/gauge per string selectable | Strings | partial | PartLibrary `StringOverride` (gauge, wound, material) | Workshop (drag a string card onto a string) | WorkshopStrings.aCardOntoAStringOverridesIt…; WorkshopStrings.anOverriddenStringIsDrawnInItsOwnMaterial | Was no; not in the Advanced string rows |
| Standard + 11 alternate tunings | Tuning | yes | TuningPreset | Header; headstock popover | Tuning.everyPresetProducesSaneFrequencies | |
| Custom per-string tuning (any note) | Tuning | partial | `openFrequencyHz` / `useCustomTuning` preset fields | no note picker; detune ±100 only | — | |
| Per-string detune ±100 c | Tuning | partial | TuningEngine.detuneCents | Headstock popover sliders (with stability offsets) | Editor.theHeadstockPopoverEditsPerStringTuning; RealismUi.theHeadstockPopoverShowsOffsetsAndRetunes | Not a parameter; written from the message thread (GuitarBodyComponent.cpp:597) |
| Realism detune 0-20 c, refreshed on load / request | Tuning | partial | `realism_detune`; PB fixed seed 0x9E3779B9 (Parameters.cpp:1904) | Col1 | — | Still no re-roll and a fixed pattern |
| Intonation error scales with fret | Tuning | yes | `intonation_error`; aging adds sharpness per string | Col1 | Tuning.intonationErrorGoesSharpUpTheNeck | |
| Fine tuner per string | Tuning | partial | TuningEngine.fineTuneCents set by StringAging and StabilityModel | Retune-all button and offset strip (CHAR TUNING STABILITY) | RealismUi.theOffsetStripRetunesTheStringItIsClickedOn; RealismUi.theRetuneAllButtonClearsEveryOffset | Retune only; no fine-tune knob |
| Mono mode with legato | Playing modes | yes | MidiInterpreter Mono | EP Mode | Technique.* | |
| Poly / chord voicing, detection, strum | Playing modes | yes | ChordVoicer, RubricVoicer | EP Mode | ChordVoicer.*; Engine.aChordVoicesAcrossStrings | |
| Guitar controller: channel = string, MPE | Playing modes | yes | MidiInterpreter; ControllerProfile | EP Mode; tab CONTROLLERS | Controllers.*; ReviewRegression.aControllerNoteOffReleasesItsOwnString | |
| Auto technique: bend, AT vibrato, mod wheel, sustain, sostenuto | Auto detect | yes | MidiInterpreter ccMap; pedal-up release (CC64/66) now implemented | n/a | Combo.liftingTheSustainPedalReleasesItsNotes | |
| Mod wheel → vibrato OR whammy (user-mapped) | Auto detect | partial | `setCcTarget` from preset `midiMap` or ControllerProfile only | no remap UI | — | |
| High velocity → pinch (user-mappable) | Auto detect | partial | Velocity trigger only for natural harmonics, never enabled from a parameter | no | — | |
| CC for palm mute, pick position, slide | Auto detect | yes | default ccMap | n/a | FingerstyleAttack.FA15_ccTriggers | |
| Signal chain order | Effects | yes | LE process | Options › Diagnostics audio-path view | Diagnostics.theAudioPathShowsWhatIsSoundingAndTheFlags | |
| Pre pedals: comp, wah, env, octaver, pitch, OD, dist, fuzz | Effects | yes | PedalsDrive | Col2 Pedalboard; Easy rack | Effects.everyPedalTypeRunsCleanly; ReviewRegression.thePitchShifterShiftsTheWayItSays | |
| Tuner-mute pedal | Effects | no | — | no | — | No tuner at all |
| 14 amp models | Amp | yes | AmpModel (13 + Custom) | Col3; Easy amp card | Amp.gainSweepIsMonotonicAt1kHz | |
| Gain / master / bright / mid boost / presence / standby | Amp | yes | AmpFacePanel | Col3; Easy | Amp.standbyIsSilentAndWarmsUp; ModelGapsUi.standbyAndBypassReachTheFacesOnThePanels | |
| Cabinet sizes incl. open / closed back | Cab | yes | CabinetType | Col3; Easy | — | |
| Speaker types; speaker age | Cab | yes | SpeakerType; `cab_speaker_age` | Col3 | — | |
| Mic types, position, distance, dual-mic blend | Mic | yes | MicType; `mic_*` | Col3; Easy | — | |
| Room size, material, mic-to-room blend | Room | yes | RoomEngine | Col3; Easy; room light | Room.biggerRoomsRingLonger; StageTouches.theRoomLightFollowsSizeAndWet | |
| Stereo via dual cab / stereo fx / dual amp | Stereo | partial | `mic_width`; stereo pedals | Col3 | Engine.monoCompatibility | No dual amp |
| MIDI capture 60 s; Save last take | Additional | yes | MidiCapture (64-bit counters) | File menu | MidiCapture.capturesAndWritesAFile | |
| Chord library, searchable | Additional | yes | Overlays chord panel | Chord overlay | — | |
| Chord library: edit fingerings | Additional | no | — | no | — | |
| Scale / mode overlay on fretboard | Additional | partial | FretboardComponent scale overlay | Advanced fretboard right-click only | — | Not in Easy |
| Real-time tab display, exportable | Additional | yes | NotationPanel; capture now receives chords and techniques | tab NOTATION; fretboard tab dots | ModelGapsUi.theCaptureHearsTechniquesAndChordsFromTheEngine; ModelGapsUi.theCurrentBarIsDrawnOnTheFretboardAsTabDots | |
| Practice: metronome, progression looper, backing track | Additional | yes | Source/Practice/*; session recorder | Practice drawer; tab PRACTICE | PracticeGaps.*; ReviewRegression.theMetronomeStartsOnBeatOne | |
| Freeze / E-Bow | Additional | yes | FreezeOverlay, EBowDriver | Col3 Sustain | Sustain.*; EBow.* | |
| Doubler | Additional | yes | Doubler pedal | Post rack | Doubler.*; ModelGapsUi.theDoublerDefaultsAreTheClassicAdt | Legacy migration runs on every load (B-07) |
| Feedback simulation | Additional | yes | FeedbackLoop (+ chambering term) | Col3 Sustain | Feedback.*; ModelGaps.chamberingFeedsTheFeedbackCoupling | |
| Humanize: timing, velocity, detune, attack, noise | Humanize | yes | MidiInterpreter::Humanisation | Col3 Humanise | — | |
| Humanize: vibrato timing / depth variation | Humanize | no | — | no | — | |
| Rounded window with cutaway | GUI window | yes | PluginEditor paint | n/a | Editor.itLaysOutAndPaints… | |
| 1200x720, resizable, aspect locked | GUI window | yes | PluginEditor constrainer | n/a | Reflow.noControlHangsOutsideItsParentAtAnyWidthOrScale | |
| Header items 1-9 | Header | yes | HeaderBar | header | Editor.* | |
| Easy band 1: guitar image with switch and knobs | Easy | yes | GuitarBodyComponent / GuitarRenderer | Easy | GuitarIllustration.*; Headstocks.everyLayoutIsDrawable… | TODO G remainders landed |
| Easy band 1: 24-fret clickable fretboard, live notes | Easy | partial | Easy PianoRollStrip plays and mirrors; note dots on the illustration | Easy piano roll | PianoRoll.clickingAKeyPlaysTheGuitar; NoteDots.theDotAppearsWithin60ms… | Was no; still no clickable fretboard in Easy |
| Six LARGE macro knobs with dice and lock | Easy band 2 | partial | EP macros `Size::Small` (EasyPanel.h:121-128) | Easy playing strip | EasyLayout.* | Size still wrong |
| Style dropdown, mode selector, MIDI blink, Audition | Easy band 3 | yes | EP | Easy | — | |
| Export / drag-out handle | Easy band 3 | partial | Export button; external drag in PracticePanel.cpp:1588 and MidiOutPanel.cpp:144 | Easy Export; Practice Save drag | PracticeGaps.theSaveButtonDragsTheSavedTakeOut | No drag handle in Easy band 3 |
| Advanced col 1: per-string rows | Advanced | partial | StringRow note, tension, mute; DecayRow; offset strip | Col1 | — | Material, gauge and age are per string only in Workshop and CHAR; mute is engine state written from the UI (AP:127) |
| Col 2: selected-string detail editing | Advanced | partial | `stringInfoLabel` read-only | Col1 | — | Per-string editing is in the Workshop bench |
| Per-string fretless toggle | Advanced | no | global `fretless` | no | — | |
| Custom fret positions / scalloping per string | Advanced | no | — | no | — | |
| Per-fret tuning offset editor | Advanced | no | — | no | — | |
| Col 3 body/pickup/hand/noise sections | Advanced | yes | AP buildColumn1/2 | Col1/Col2 | GuiReach.* | |
| Col 4 amp/cab/mic/room/fx/humanise | Advanced | yes | AP buildColumn3 | Col3 | — | |
| Drag-and-drop pedal reorder | Advanced | yes | PedalRack::reorder | racks | Effects.chainReordersWithoutGlitching; Undo.movingAPedalIsOneEntry | |
| Knob drag, double-click, context menu | Interaction | yes | Widgets showParameterContextMenu | all knobs | ContextMenu.everyParameterShowsItsAutomationId…; DragToModulate.* | |
| Fretboard click plays; right-click mute / capo / mark | Interaction | partial | FretboardComponent::mouseDown / menu (FretboardComponent.cpp:640) | Advanced strip | — | Still no "mark" |
| MIDI Learn via right-click | Interaction | yes | MidiLearn (learn finished on the message thread) | all knobs | MidiLearn.learningDoesNotAllocateOrLockOnTheAudioThread | |
| Overlays: Escape, click-outside, close, one at a time | Interaction | yes | OverlayPanel | overlays | Editor.everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt | Test covers Escape only |
| `.luthierpreset` JSON with state, MIDI map, tags | Presets | yes | PresetManager | File menu; browser | Presets.everyFactoryPresetRoundTripsToTheUlp; Presets.everyByteOfAFactoryPresetFlipped… | |
| Factory presets by category in bundle | Presets | partial | FactoryPresets (36, factoryRevision 3, carry guitar parts) | Browser | Combo.factoryPresetsAndResetUseTheGuitarsOwnParts | Still no Custom/User category; written at runtime |
| User folders; extra folders via Preferences | Presets | yes | FileLocationsPage add + list | Options › File locations | — | Still no remove button |
| Quick export phrase to WAV | Audio export | yes | AudioExporter | Easy Export; File | ReviewRegression.autoTrimTrimsTheTailWithoutLeadingSilence | |
| Export As: format, depth, rate, length, normalise, name | Audio export | yes | ExportPanel; TuneExportDialog | Export overlay; TUNE export | TuneIntegration.theExportDialogWritesEachDestinationFromOneScreen | |
| MIDI capture export `.mid` | Audio export | yes | File menu | header | MidiCapture.* | |
| Identity 1: tension range, warn on impossible | Identity | partial | Validator::checkTension clamps | StringRow tension colour | Validator.* | No warning banner |
| Identity 2: fret range reject / transpose | Identity | yes | Validator::checkFretRange; voicer drops out-of-range notes | n/a | Combo.notesBelowTheRangeAreDroppedAndInRangeNotesSound | No notice to the user (B-09) |
| Identity 3: body always, "no body" experimental | Identity | yes | `body_mode` | Col1 | — | |
| Identity 4: all pickups off → silence and warning | Identity | partial | Validator::checkPickupOutput | no warning UI | Pickup.silenceWhenEverythingIsOff | |
| Identity 5: velocity changes brightness | Identity | yes | Excitation | n/a | StringEngine.harderPluckIsBrighter… | |
| Identity 6: higher notes decay faster | Identity | yes | StringEngine | n/a | StringEngine.higherNotesDecayFaster | |
| Identity 7: coupling cannot be disabled | Identity | yes | coupling floor | Col1 | Coupling.* | |
| Identity 8: slide noise present by default | Identity | yes | `noise_slide` default | Col2 | — | |
| Identity 9: playable chord voicings | Identity | yes | ChordVoicer | n/a | ChordVoicer.impossibleChordDegradesGracefully | |
| Validator 5 stages, nudge or reject, log | Validator | partial | Validator.h (+ harmonic fallback count) | Debug panel | Validator.*; HarmonicRealism.HR15_fallbackIsCounted | Check 5 tests level only |
| RT-safe audio, denormals off, NaN guards | Threading | partial | ScopedNoDenormals; sanitise; engine try-lock (B-01) | n/a | Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks; Combo.structuralChangesWhileAudioRuns | The UI thread still calls `engine.panic()` (PP:2189), `setDamping` (AP:127, FretboardComponent.cpp:110) and `setDetuneCents` (GuitarBodyComponent.cpp:597) |
| Partitioned FFT body convolution | Threading | yes | BodyEngine | n/a | Latency.anImpulseArrivesWhenReported | |
| Worker threads for IR / preset / coupling recalculation | Threading | partial | async IR; thumbnails worker; notation export on a worker | n/a | ModelGapsUi.notationExportRunsOnAWorkerThread; Thumbnails.workerRendersCachesAndEvictsAt200 | Coupling recalculation still inline |
| SR / block agnostic | Threading | yes | prepare() | n/a | ReviewRegression.aBlockBiggerThanPreparedIsRenderedWhole; PerfBudget.sampleRateScaling | |
| 20 ms parameter smoothing | Threading | yes | kParamSmoothSeconds | n/a | ReviewRegression.integerParametersSwitchRatherThanBlend | |
| CC for all continuous params | MIDI | yes | MidiLearn | right-click | MidiLearn.*; Stress.midiLearnArmDisarmHundredTimes | |
| MPE pitch bend, pressure, timbre | MIDI | yes | MidiInterpreter MPE | Col3 | Controllers.* | |
| Per-string bend range (controller mode) | MIDI | partial | ControllerProfile only | tab CONTROLLERS | Controllers.* | `bend_global_range` / `bend_string_*` in progress on techniques |
| Program change recalls presets | MIDI | yes | PP | n/a | — | |
| Prefs: default preset on load | Prefs | no | — | no | — | TUNE relaunch loads the last tune, but not a preset |
| Prefs: preset folders add / remove | Prefs | partial | add + list | Options | — | No remove |
| Prefs: MIDI mapping global defaults | Prefs | partial | MidiPage "clear all" | Options › MIDI | — | |
| Prefs: export defaults | Prefs | partial | MIDI export defaults; TuneExportDialog | MIDI OUT; TUNE | — | |
| Prefs: oversampling / FFT block / max polyphony | Prefs | partial | `oversampling`; CPU relief opt-out | Options › Audio / Diagnostics | CpuRelief.stepSevenIsOptOut | No FFT block or polyphony setting |
| Prefs: realism defaults | Prefs | partial | Mains region (user-global) | Options › Audio | NoiseFloor.regionSetsTheHumFrequency | Was no |
| Prefs: appearance | Prefs | yes | AppearancePage (+ accent, data stream) | Options | Accent.*; Theme.* | |
| Windows VST3 + macOS VST3/AU | Deliverables | yes | CMakeLists.txt:35-38 AU on APPLE; CLAP if extensions present | n/a | — | Was partial; AU and macOS unbuilt here (platform-deferred) |
| Source layout DSP/Model/UI/Presets | Deliverables | partial | Source/* | n/a | — | UI is still flat |
| 100+ body IRs, 50+ speaker IRs | Deliverables | yes | 720 wav | n/a | — | |
| Docs | Deliverables | yes | docs/*.md (+ RELEASING, CHANGELOG, updated TROUBLESHOOTING) | n/a | — | |
| Unit tests per DSP module | Tests | yes | Source/Tests | n/a | 1198 tests | |
| String tension tests | Tests | yes | StringPhysics.* | n/a | | |
| Chord voicing tests | Tests | yes | ChordVoicer.*, RubricVoicer.* | n/a | | |
| Pluginval level 10 in CI | Tests | yes | .github/workflows/ci.yml → scripts/pluginval.sh (strictness 10) | n/a | BETA report: pass with `--timeout-ms 900000` | Was no; the default timeout may hit B-12 in CI |
| UI overlay dismissal tests | Tests | partial | EditorTests | n/a | Editor.everyOverlayShortcut… | Escape only |
| Fuzz 10,000 states | Tests | yes | | n/a | Parameters.fuzzAcrossHundredThousandStates | Now 100k |
| Latency reporting test | Tests | yes | | n/a | Latency.dspLatencyIsWithinBudget; Latency.anImpulseArrivesWhenReported | |
| Signed and notarised installers | Deliverables | partial | packaging/windows/Luthier.iss; scripts/package_macos.sh / package_windows.ps1 / package_linux.sh; release.yml | n/a | InstallLayout.* | Was no; signing only when secrets are present; Windows/macOS not verified |
| CLI batch renderer | Deliverables | yes | Tools/RenderCli.cpp | n/a | — | |
| Ship gates 1-9 | Ship | partial | tests above | n/a | Combo.*, IrReload.*, Effects.toggleIsClickFree | Pluginval recorded in the BETA report; overlay test is Escape only |
| Ship gates 10-12 (7-host soak, GK/MPE hardware, blind A/B) | Ship | no | — | n/a | — | Manual; no record |

---

### engine.md

**Summary:** 99 requirements. **78 yes / 19 partial / 2 no** (was 75 / 20 / 4).

What moved to yes:
- Rule 0.2 no-alloc, now tested with an operator-new counter plus a lock probe.
- Round trip of every factory preset.
- Pluginval in CI.

Still open:
- Custom amp controls, per-string whammy, the aftertouch target and interpolation choice (all still have no caller).
- 8 s standby.
- Mono-compatibility per preset.
- Direct UI-to-engine calls.
- Idle CPU (B-13).
- Host matrix.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Double-precision DSP | 0.1 | yes | DSP/* | n/a | — | |
| No allocation or I/O in processBlock | 0.2 | yes | prepare-allocated buffers | n/a | Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks; NoiseFloor.noAllocationOnTheAudioPath; PianoRoll.publishingTheSnapshotDoesNotAllocate | Was partial. BETA B-01 notes that `ReverbPedal::rebuildLines` can still allocate on a Size change |
| DC blocker + NaN guard on every recursive stage | 0.3 | partial | StringEngine dcBlocker + sanitise; others sanitise | n/a | Common.dcBlockerRemovesOffset; Robustness Stress.* | No per-stage DC blockers found |
| 20 ms / 30 ms / 5 ms smoothing and crossfades | 0.4 | yes | DspCommon | n/a | Effects.toggleIsClickFree; ReviewRegression.aPedalMixMoveToFullWetIsSmoothed | |
| No hardcoded SR | 0.5 | partial | header defaults 44100 | n/a | PerfBudget.sampleRateScaling | Cosmetic |
| Times in s / Hz | 0.6 | yes | | n/a | — | |
| ScopedNoDenormals | 0.7 | yes | PP, LE | n/a | Combo.* subnormal check | |
| reset() everywhere | 0.8 | yes | *.reset (StringEngine now also resets termination state) | n/a | Combo.renderDoesNotDependOnWhatWasPlayedBefore; Combo.renderIsDeterministicAfterReset | |
| Modules testable in isolation | 0.9 | yes | Tests | n/a | many | |
| Oversampling 4x default, 2x / 8x | 0.10 | yes | `oversampling` | Options › Audio; Col3 | ReviewRegression.theOversamplerDelaysWhatItReports; Engine.oversamplingDowngradesAbove96k | |
| Pipeline order | 1 | yes | LE process | Diagnostics audio path | Diagnostics.theAudioPathShowsWhatIsSounding… | |
| Only back-flow is coupling | 1 | partial | Also FeedbackLoop (amp → strings); BodyCouplingBank (string ↔ body) | n/a | Feedback.*; BodyCoupling.BC01_theBankIsPassive | Deliberate |
| Typed events | 2 | yes | PlayingEvents.h | n/a | — | |
| Mode A / B / C | 2 | yes | MidiInterpreter | EP Mode | Controllers.* | |
| String assignment algorithm | 2 | yes | ChordVoicer / RubricVoicer | n/a | ChordVoicer.singleNotesStayNearTheHand | |
| Default CC map | 2 | yes | resetCcMapToDefaults (+ CC103 artificial) | n/a | FingerstyleAttack.FA15_ccTriggers | |
| CC map user-remappable | 2 | partial | setCcTarget via preset / ControllerProfile | no editor | — | |
| Aftertouch → vibrato or bend | 2 | partial | setAftertouchTarget | no | — | Still no caller outside tests |
| TuningEngine formula | 3 | yes | TuningEngine | n/a | Tuning.* | |
| Temperaments incl. custom | 3 | partial | Temperament; `customTemperament` | Col1 choice | Tuning.temperamentsDiffer… | No editor |
| Alternate tunings | 3 | yes | TuningPreset | header | Tuning.everyPreset… | |
| Custom tuning array in preset | 3 | partial | `openFrequencyHz` | no | ReviewRegression.aPresetWithoutAStringsBlockClearsThePreviousDetune | |
| Manual detune ±100 | 3 | partial | detuneCents | headstock popover | Editor.theHeadstockPopover… | Not automatable |
| Realism detune ±20 persisted | 3 | yes | `realismDetuneCents` | Col1 | — | Fixed seed |
| Drift every 30 s, off by default | 3 | yes | TuningEngine drift → TUNING STABILITY / EnvironmentModel | CHAR TUNING STABILITY / ENVIRONMENT | TuningStability.*; Environment.* | Now a much richer model |
| Intonation slope 0.3 c/fret per string | 3 | partial | global slope; per-string aging sharpness | Col1 | Tuning.intonationError… | Not per string by setting |
| Technique set | 4 | yes | TechniqueEngine | n/a | Technique.* | |
| Detection logic order | 4 | yes | TechniqueEngine.cpp | n/a | Technique.controllersTakePriorityOverInference | Cascade tests in progress on techniques |
| HammerOn / PullOff re-excite | 4 | yes | Excitation | n/a | StringEngine.legatoDoesNotRetriggerTheAttack | |
| Slide ramp + noise | 4 | yes | Slide | n/a | Technique.slideDurationScalesWithDistance | |
| Palm mute 5 kHz → 800 Hz | 4 | yes | Damping::PalmMute | CC67 | StringEngine.palmMute… | |
| Harmonics, pinch, tap | 4 | yes | Excitation + Harmonics.h | CC; CHAR HARMONICS | HarmonicRealism.* | |
| SlideGuitar softer attack + vibrato | 4 | yes | SlideEngine | header Slide | Slide.* | |
| Strum offsets, up-strums lighter | 4 | yes | StrumGesture | STRUM | StrumDynamics.* | |
| EKS delay line + loop filter + loss + DC + NaN | 5.1 | yes | StringEngine | n/a | StringEngine.* | |
| Fractional delay: 3 options | 5.2 | partial | FractionalDelayLine::setInterpolation | no | — | Still not selectable |
| Buffer size for 40 Hz at 96 kHz | 5.2 | yes | | n/a | StringEngine.survivesExtremeParameters | |
| Delay length smoothed ~2 ms | 5.2 | yes | | n/a | StringEngine.bendIsSmooth… | |
| Loop filter by material | 5.3 | yes | | n/a | — | |
| Palm 0.6 / muted pick 0.75 | 5.3 | yes | | n/a | | |
| Loss allpass inharmonicity | 5.4 | yes | | n/a | StringEngine.dispersionStretches… | |
| Triangle excitation, comb, material filters | 5.5 | yes | Excitation | n/a | FingerstyleAttack.FA04_defaultsReproduceTheTable | |
| Random attack 0.5-2 ms | 5.5 | partial | `hum_attack` | Col3 | — | |
| Coupling bandpass, symmetric, cap | 5.6 | yes | CouplingMatrix | Col1 | Coupling.*; ReviewRegression.aUnisonPairDecays | |
| Muted string still receives coupling | 5.6 | yes | | n/a | — | |
| reset() clears state | 5.7 | yes | | n/a | Combo.renderDoesNotDependOnWhatWasPlayedBefore | |
| Voice steal 5 ms | 5.8 | yes | | n/a | — | |
| Body convolution, latency reported | 6.1 | yes | BodyEngine | n/a | Latency.* | |
| IR path, 100+ | 6.1 | yes | Resources/BodyIRs | n/a | IrReload.* | |
| Modal 30-50 modes + air mode | 6.2 | yes | BodyEngine | Col1; CHAR | Body.*; BodyCoupling.* | |
| Modal params per body in JSON | 6.2 | partial | compiled tables + part JSON; `body_mode_*_scale` | n/a | PartAcoustics.* | |
| Pickup types and params | 7 | yes | PickupEngine | Col2 / Workshop | Pickup.* | |
| Position comb | 7.1 | yes | | Workshop | Pickup.positionComb… | |
| LCR resonance | 7.2 | yes | GuitarCircuit | Col2 Circuit | Circuit.* | |
| Magnet EQ | 7.3 | yes | | Col2 | — | |
| Humbucker coils; coil tap | 7.4 | yes | | Col2 | NoiseFloor.aHumbuckerCancelsHumButNotAGroundLoop | |
| Piezo filters | 7.5 | yes | | Col2 | — | |
| Internal mic tilt | 7.6 | yes | | Col2 | — | |
| Selector, volumes, 5 ms crossfade | 7.7 | yes | | Col2; illustration | — | |
| Whammy modes | 8 | yes | WhammyEngine | Col1 Bridge | Whammy.*; ReviewRegression.aHardtailIgnoresTheWhammyRanges | |
| Floyd spring burst | 8.2 | yes | `whammy_springs` | Col1 | — | |
| TransTrem | 8.3 | yes | `transpose_lock` | Col1 | Whammy.transTrem… | |
| 5 ms whammy smoothing | 8 | yes | | n/a | — | |
| Whammy on CC2 / MPE Y | 8 | yes | | n/a | — | |
| Per-string whammy toggle | 8 | no | `WhammyEngine::setPerStringEnabled` has no caller | no | — | Unchanged |
| CableSim | 9 | yes | GuitarCircuit cable | Col2 Circuit | Circuit.* | |
| Compressor opt / FET | 10 | yes | PedalsDrive | racks | Effects.* | |
| Wah, env, octaver, pitch shifter | 10 | yes | | racks | ReviewRegression.thePitchShifterShiftsTheWayItSays | |
| OD / Dist / Fuzz / Boost / Volume | 10 | yes | | racks | EffectsQa | |
| 8 slots, reorder, 10 ms bypass fade | 10 | yes | EffectsChain | racks | Effects.zeroMixIsABypassWithinMinus80; Effects.toggleIsClickFree | |
| 4x oversampling on drive | 10 | yes | | n/a | Common.oversampling… | |
| Amp model list incl. Custom | 11.1 | partial | AmpModel | Col3 | — | Custom still has no controls (setCustom* uncalled) |
| Preamp stages | 11.2 | yes | AmpEngine | n/a | Amp.gainSweepIsMonotonicAt1kHz | |
| Tone stack + presence | 11.3 | yes | ToneStack | amp face | Amp.neutralToneStackIsFlatWithin1dB; ReviewRegression.theToneStackIsPassiveAtEverySetting | |
| Phase inverter | 11.4 | yes | | n/a | — | |
| Power amp + sag | 11.5 | yes | | n/a | — | |
| Output transformer | 11.6 | yes | | n/a | — | |
| Standby with 30 s warm-up | 11.7 | partial | warmupGain 8 s (AmpEngine.cpp:100) | amp face | Amp.standbyIsSilentAndWarmsUp; Amp.coldStartHasNoTransient | Still 8 s |
| Post fx | 12 | yes | PedalsMod | post rack | ReviewRegression.theReverbPedalTailSurvives…; ReviewRegression.aSyncedLfoWithTheTransportStoppedStillCycles | |
| Cab IR 200+ | 13.1 | yes | CabinetEngine | Col3 | Cabinet.* | |
| Mic selector swaps IR | 13.2 | yes | | Col3 | — | |
| Dual mic blend | 13.3 | yes | | Col3 | Engine.monoCompatibility | |
| Async IR with fallback | 13.4 | yes | | n/a | IrReload.throughThePluginAGuitarChangeLeavesNoOnsetDifference | |
| Room ER + FDN | 14 | yes | RoomEngine | Col3 | Room.*; ReviewRegression.theRoomTailSurvivesRepeatedDecaySends | |
| Master gain, limiter, DC | 15 | yes | MasterBus | Col3; Easy | Master.limiterHoldsTheCeiling; ReviewRegression.reEnablingTheLimiterReplaysNothingStale | |
| Metering peak / RMS / LUFS | 15 | yes | MasterBus | LevelMeter; LUFS in the Debug overlay (Overlays.cpp:343); VU | Master.meteringTracksTheSignal; StageTouches.theVuNeedle… | |
| Per-preset state | 16 | yes | PresetManager | n/a | Presets.*; Combo.everyParameterSurvivesTheSessionStateRoundTrip | |
| Host state incl. UI state | 16 | yes | PP uiState | n/a | HostState.*; StateModel.* | |
| Threading: serial strings, workers | 17 | yes | | n/a | — | |
| Communication only via FIFO / atomics | 17 | partial | engine lock (B-01) | n/a | Combo.structuralChangesWhileAudioRuns | UI still calls engine setters (panic, mute, detune, restring requests) |
| Latency sum | 18 | yes | PP | n/a | Latency.oversamplerReportsItsGroupDelay | |
| Unit tests per module | 19 | partial | Tests | n/a | see Tests | Tuning to 0.1 c for all tunings still not asserted |
| Integration: chromatic, bend, fast slide | 19 | yes | | n/a | Engine.chromaticScale…; Engine.fastSlides… | |
| 30 s at 96 kHz all fx < 15 % CPU | 19 | partial | PerfBudget.* | n/a | PerfBudget.everyModuleWithinBudget, scenarioTotals, sampleRateScaling; Combo.cpuPerFactoryPreset | Idle CPU 6-12x over budget (B-13) |
| Round-trip every factory preset within 0.01 dB | 19 | yes | | n/a | Presets.everyFactoryPresetRoundTripsToTheUlp; Combo.sessionStateRoundTripReproducesAudio | Was partial |
| Pluginval L10 CI | 19 | yes | ci.yml; build.yml (5 on push, 10 nightly) | n/a | BETA external validators | Was no |
| Host matrix / controller testing | 19 | no | — | n/a | — | Manual |
| Mono-compatibility of every factory preset | 20.19 | partial | Engine.monoCompatibility (one rig) | n/a | yes | Not per preset |
| Perf: < 8 % CPU, 16 instances, preset load < 500 ms, MIDI < 2 ms | 22 | partial | PerfBudgetTests; RobustnessTests | n/a | Workshop.guitarLoadStaysUnder300ms; Stress.thirtyTwoInstancesRenderInTurn; Boot.coldAndWarmInstantiationStayInBudget | Was no; MIDI-to-sound latency and reference-CPU figures not measured |

---

### include.md

**Summary:** 26 requirements. **23 yes / 3 partial / 0 no** (was 20 / 5 / 1). MIDI import is done: File › Import MIDI and drop-on-window, into the session, the tune or the looper. `THIRD_PARTY_LICENCES.txt` exists and is tested. The CLAP target and Linux packaging exist. The bug bash exists as the beta harness. Still partial:
- Support links are still `.example` placeholders. They can be overridden at build time, but nothing enforces that on release.
- Reset does not stop the players.
- Standalone device settings are still only a note.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| In-plugin help: every feature, workflow, GUI | Help | yes | HelpContent, HelpTab, PanelHelpButton, tour | tab HELP; F1; panel `?`; onboarding tour | HelpTab.theContentCoversWhatIncludeMdAsksFor; Onboarding.everyPanelsHelpIconOpensItsOwnTopic | |
| Version, licence | Help | yes | HelpContent "about" | HELP | — | |
| GitHub / homepage / support email links | Help | partial | Support/SupportLinks.h:30-38 `.example` defaults, `-DLUTHIER_*` overrides, `areConfigured()` | HELP (HelpTab.cpp:188 hides or flags them) | Onboarding test line 617 | Placeholders still ship by default; release.yml does not require them |
| Install / uninstall troubleshooting | Help | yes | HelpContent; docs/TROUBLESHOOTING.md; packaging uninstall scripts | HELP | — | |
| Debug button in help | Help | yes | help.onOpenDebug | HELP → Debug | — | |
| Unique app icon | Icon | yes | Resources/icon.png | n/a | — | |
| Preset bank with descriptive names | Presets | yes | FactoryPresets (36) | browser; thumbnails | Combo.everyFactoryPresetPlaysEveryPhrase | |
| Reset button to defaults | Reset | partial | PP::resetEverything (PP:2197) | Easy; File; Ctrl+Shift+R | Undo.resetEverythingIsOneEntry; Combo.factoryPresetsAndResetUseTheGuitarsOwnParts | Still doesn't stop the looper, backing track, tune, rhythm or metronome (B-14) |
| File dropdown | Menus | yes | HB showFileMenu | header | — | |
| Options: tooltips on/off | Menus | yes | AppearancePage | Options | — | |
| Options: MIDI and audio card settings | Menus | partial | AudioPage note button | Options | — | Unchanged |
| "Open location in explorer" | Menus | yes | File menu; FileLocationsPage (guitars / parts folders added) | header; Options | — | |
| Export audio to WAV with quality options | Export | yes | ExportPanel; TuneExportDialog | Easy / File / TUNE | TuneIntegration.audioExportWritesTheMixAndEveryAuxStem | |
| Success popup | Export | yes | Overlays | — | — | |
| Import / export MIDI | MIDI | yes | Export/MidiImportTargets; HB menu item 14 "Import MIDI..." (HB:329); PluginEditor drop handler (PluginEditor.cpp:1258) | File menu; drag onto window | MidiImport.bothProfilesGoIntoTheSession, theTuneBuilderGetsANewTune, theLooperGetsARenderedLayer, aDropOnTheWindowImports | Was partial |
| Right-click: MIDI map, reset, set value | Right click | yes | Widgets | all knobs | MidiLearn.*; ContextMenu.* | |
| Drag-and-drop when importing samples | DnD | yes | IrSlotEditor filesDropped; `.mid` drop | TONE MATCH; window | MidiImport.aDropOnTheWindowImports | |
| Hover tooltips | Tooltips | yes | TooltipWindow 400 ms | everywhere | Onboarding.theRandomiseTooltipShowsOnTheFirstHoverOnly | Not in the locale catalog |
| Randomise | Randomise | yes | PresetManager::randomise | File; Easy; Ctrl+R | Presets.randomiseRespectsLocks | |
| Triple-test / logic-error pass / bug bash | Build notes | yes | docs/audit/BETA_TEST_REPORT.md; Combo harness; docs/review/FINDINGS.md | n/a | Combo.*; GuiReach.*; ReviewRegression.* | Was no; B-07, B-09, B-11…B-15 still open |
| VST3 + standalone; future CLAP / Linux | Build notes | yes | CMake CLAP via clap-juce-extensions; packaging/linux (deb, .desktop, mime) | n/a | InstallLayout.*; FileOpen.* | clap-validator 18/18 (BETA) |
| Debug window: raw data live | Extra | yes | DebugPanel; footer data stream | Debug overlay; footer | DataStream.itKeeps200StopsAfter500ms… | |
| "Create log on crash" off at every load | Extra | yes | Diagnostics | Debug | Telemetry.* | |
| Hard reset + clear caches | Extra | yes | PP::hardResetAndClearCaches | Debug | — | |
| Export troubleshooting file | Extra | yes | Diagnostics | Debug | — | |
| Easter egg pixel → hidden tab | Extra | yes | SecretPanel | notch pixel | Effects.secretEffectIsStable… | |

---

### theme.md

**Summary:** 34 requirements. **19 yes / 9 partial / 6 no** (was 16 / 10 / 8).

What moved to yes:
- The scrolling data stream is now built. It is a one-line footer at `PluginEditor.cpp:63` that stops after 500 ms, has faded rows and follows reduced motion.
- The tab strip is readable at 1280. Tracked text now fits its button.

Still breaking the theme:
- The guitar-shop palette and fonts, and its skeuomorphism.
- The 80 ms value animation. `Metrics::animationMs` is still unused.
- Hover brighten and the vertical-resize cursor on knobs.
- Colourful scrollbars. `drawScrollbar` still uses `edgeBright` on `panelSunken`.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Core palette neutrals (#0E1116 etc.) | Colour | no | Theme.h Palette (guitar-shop) | — | Theme.theDefaultIsTheGuitarShop | Unchanged |
| One accent re-tint allowed | Colour | yes | Palette::accent + accent choices following the guitar | Options › Appearance | Accent.everyChoiceMeetsContrastOnEveryPalette; Accent.theWindowTakesTheAccentAndFollowsTheGuitar | |
| Shadow 0.55, 8 px, y+2 | Colour | partial | — | — | — | Not verified |
| Inter / JetBrains Mono fonts | Type | no | Fonts::ui Lato, display Bebas | — | Theme.theBundledFontsLoad | |
| Labels 11 px uppercase tracked; values 13-14 px | Type | partial | Fonts::drawTrackedText (now drops tracking and shrinks to fit, ≥ 7.5 pt) | — | Screenshots.everyPanelInEveryPalette | |
| Knob 48 / 36 / 64 px, flat gradient | Knobs | partial | Metrics knob sizes | — | Theme.controlsRender… | Skeuomorphic bell knobs |
| 2 px indicator, 270° 3 px arc, 4 px gap | Knobs | yes | LookAndFeel; range warning arc | — | RangeMarking.theWarningArcAppearsPastStock… | |
| Centre dot active / default | Knobs | partial | — | — | — | Not verified |
| Double-click reset; right-click value entry | Knobs | yes | Widgets | — | ContextMenu.* | |
| Value above on hover / drag | Knobs | partial | LuthierKnob | — | — | |
| Slider track 4 px, 16x24 thumb, ticks | Sliders | partial | LookAndFeel | — | — | Brass fader caps |
| Buttons 4 px radius, 28 px, states, 100 ms flash | Buttons | partial | Metrics::buttonHeight 28; accent overlay only while down (Theme.cpp:904) | — | — | No timed flash |
| Meter gradient, peak hold, readout | Meters | yes | LevelMeter; VU (StageTouches) | Easy | StageTouches.theVuNeedleHasBallistics… | |
| 8 px grid | Layout | yes | Metrics::grid | — | Reflow.noControlHangsOutsideItsParent… | |
| 1 px separators, no boxes-in-boxes | Layout | partial | Column rules | — | — | Walnut panels |
| Section headers with 2x12 accent bar | Layout | yes | drawSectionHeader | — | — | |
| Corner radii 6 / 4 / 2 | Layout | yes | Metrics | — | — | |
| 16 px min edge padding | Layout | yes | windowPadding | — | — | |
| No skeuomorphism / faux wood / metal | Layout | no | guitar-shop theme (+ VU needle, room light) | — | — | Direct violation, proposal-driven |
| 80 ms ease-out value animation | Feel | no | `Metrics::animationMs` (Theme.h:118) unused | — | — | |
| Hover brighten 8 %; vertical-resize cursor | Feel | no | no knob cursor | — | — | Resize cursors exist only on the piano roll and tune pills |
| Shift coarse / Ctrl ultra-fine drag | Feel | yes | KnobSlider::mouseDrag | — | — | |
| Tooltip pill after 400 ms | Feel | yes | tooltipDelayMs 400 | — | — | |
| Header strip height | Header | yes | headerHeight 48 (spec.md wins) | — | — | |
| Header: name, gear, preset selector, A/B | Header | partial | HB (File menu instead of gear) | header | — | |
| Signature diagonal notch | Signature | yes | drawSignatureNotch | — | — | |
| Version in 9 px mono footer | Signature | yes | PluginEditor footer | — | — | Footer also carries the data stream |
| Output LED | Animation | yes | OutputLed | header | — | |
| Scrolling matrix data stream in empty space | Animation | yes | DataStreamDisplay in the PluginEditor footer (PluginEditor.cpp:63, :493); toggle in Options › Appearance (on by default) | footer | DataStream.itKeeps200StopsAfter500msAndHonoursReducedMotion | Was no; one line in the footer, not the "empty space" |
| Stream stops when idle, faded edges, green | Animation | yes | Widgets.cpp:1367-1450 (500 ms stop, alpha fades, `Palette::dataStream`); suspended under CPU relief | footer | same | Was no |
| Colourful, obvious scrollbars (issues.md) | — | no | Theme.cpp:1145 drawScrollbar | — | — | Unchanged |
| Palette actually applied | — | yes | Palette::remap; createSliderTextBox; table header colours | Options › Appearance | Theme.aPaletteChangeReaches…; Reflow.overlayValueBoxesUseThePaletteTextColour | |
| Contrast AA on palettes | — | yes | — | — | Accessibility.palettesMeetContrast…; Accent.everyChoiceMeetsContrast… | |
| Tab strip readable at 1280 | — | yes | AP resized (tab label fit); drawTrackedText fit | tab strip | Screenshots.everyPanelInEveryPalette | Was partial (visual merged) |

---

### issues.md (user bug list)

**Summary:** 12 issues. **8 fixed / 2 partial / 2 open** (was 3 / 6 / 3).

Now fixed:
- Effects, with QA tests.
- The pedalboard.
- The illustration: every TODO G remainder landed.
- MIDI import.
- The live piano roll: `PianoRollStrip` mirrors the sounding strings in Easy and Advanced and plays the guitar.

Partial:
- Pickup audibility. B-04/B-05 fixed the factory presets and Reset so they load the guitar's own pickups, but `pickup_blend` is still dead.
- Squished panels. They now fill the viewport, but RHYTHM is not laid out at its preferred height, so the lower STRUM rows are never on screen (B-11).

Still open:
- Scrollbars and the wheel conflict.
- Stop everything.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Effects must audibly work | 1 | yes | EffectsChain; preset pedal adoption | racks | PresetPedals.*; Effects.zeroMixIsABypassWithinMinus80; Effects.toggleIsClickFree; Combo.pairwiseAcrossMajorSettings | No listening sign-off recorded |
| Pickup changes should be audible | 2 | partial | PickupEngine / GuitarCircuit; factory presets now carry guitar parts | Col2; illustration | Combo.factoryPresetsAndResetUseTheGuitarsOwnParts; NoiseFloor.positionScalesTheHum | `pickup_blend` still dead |
| Pre-amp pedals / pedalboard functional | 3 | yes | as 1 | Col2 rack; Easy rack | PresetPedals.*; GuiReach.everySlotTypeBypassAndMixHasAControl | |
| Fingers vs pick makes a difference | 4 | yes | PB; RightHand tools | Col2; CHAR RIGHT HAND | FingerstyleAttack.FA05…, FA10… | |
| Chords (two notes at once) | 5 | yes | Poly default; chord window | EP Mode | Engine.aChordVoicesAcrossStrings; ReviewRegression.aNoteReleasedInsideTheChordWindowIsReleased | |
| Guitar picture ugly | 6 | yes | GuitarRenderer; HeadstockOutlines; GuitarThumbnails; IllustrationMotion | Easy / Advanced / browser | IllustrationRemainder (8); GuitarIllustration.* | Needs the user's verdict |
| Scrollbars colourful + arrows + tooltip; wheel scrolls not knob | 7 | no | drawScrollbar unchanged; no `setScrollWheelEnabled(false)` | — | — | Open |
| MOD / RHYTHM squished; LIVE tiny boxes; tooltips | 8 | partial | AP resized (AdvancedPanel.cpp:1468): fills viewport; preferred height for TUNE / MIDI OUT / NOTATION / PRACTICE only | tabs | GuiReach (hidden-only: `strum_acceleration`, `strum_up_velocity_ratio`, `strum_tilt`, `strum_miss_probability`, `chuck_amount`, `chuck_damping`) | RhythmPanel.preferredHeight() is not used by AP |
| "Reset and stop" that stops everything | 9 | no | PP::panic (PP:2186) = audition + engine + preview MIDI | header Panic | Combo.unisonStringsNeverGrowAndPanicSilencesThem (engine only) | Players keep running (B-14) |
| MIDI import working | 10 | yes | MidiImportTargets; File › Import MIDI; drop | File; window | MidiImport.* | Was partial |
| Basic internal sequencer | 10 | yes | Tune Builder (+ editing, setlists, examples) | tab TUNE | TuneEditing.*; TuneIntegration.* | |
| Small piano roll mirroring plucked strings and vice versa | 11 | yes | UI/PianoRollStrip, PianoRollModel; Support/SoundingNotes(Publisher) | Easy (under illustration); Advanced strip (collapsible) | PianoRoll.aChordLightsItsSoundingKeys…, PianoRoll.clickingAKeyPlaysTheGuitar, PianoRoll.latchedKeysPlayAsOneStrummedChord | Was no |

---

### README.md (spec/README)

**Summary:** 22 claims. **18 true / 3 partial / 1 false** (was 15 / 4 / 3).

Now true:
- `THIRD_PARTY_LICENCES.txt` exists and is tested.
- "The scrolling data stream is unchanged".
- The `Luthier_AU` target exists on APPLE.

Still false: `Source/DSP/Cable/`.

Still partial:
- "All three interpolations selectable".
- Pluginval. It is now in CI and in the BETA report, but only passes with a long timeout.

The README does not mention the CLAP target, the installers or the 1198 tests.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Targets VST3, Standalone, AU, Tests, Render | Build | yes | CMakeLists.txt:35-38 | n/a | — | Was partial; AU on macOS only; CLAP not documented |
| Install copies Resources beside artefacts | Build | yes | CMake luthier_copy_resources (incl. CLAP) | n/a | IrLibrary.candidatesIncludeTheInstallerLayout | |
| 25 instruments, 17 tunings, 7 temperaments | Box | yes | enums | header | — | |
| 12 materials, 11 gauges | Box | yes | StringMaterials | Col1 | — | |
| 21 pedals, two 8-slot racks | Box | yes | PedalType | racks | — | |
| 13 amps, 10 cabs, 8 speakers, 7 mics | Box | yes | enums | Col3 | — | |
| 720 IRs | Box | yes | Resources (720 wav) | n/a | — | |
| 36 factory presets | Box | yes | FactoryPresets | browser | — | |
| Architecture diagram modules | Arch | partial | Source/* | n/a | — | Diagram still says CableSim; new modules (NoiseFloor, BodyCouplingBank, StabilityModel, EnvironmentModel, StringAging) not listed |
| Source layout incl. `DSP/Cable/` | Arch | no | Source/DSP has no Cable (GuitarCircuit under DSP/Circuit) | n/a | — | Stale doc |
| DSP rules 1-10 | Rules | yes | see engine.md | n/a | Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks | Was partial (rule 2 now tested) |
| Render CLI flags | CLI | yes | Tools/RenderCli.cpp | n/a | — | |
| Tests: fuzz, preset round trip, mono check | Tests | yes | — | n/a | Parameters.fuzzAcrossHundredThousandStates; Presets.everyFactoryPresetRoundTripsToTheUlp; Engine.monoCompatibility | |
| Pluginval 1.0.3 L10 passes | Tests | partial | scripts/pluginval.sh; ci.yml | n/a | BETA: pass with `--timeout-ms 900000`, times out at the default (B-12) | |
| IR scripts deterministic | IRs | yes | scripts/make_irs.py | n/a | — | |
| Runtime folders + Options buttons | Runtime | yes | FileLocationsPage | Options | — | |
| docs/* list | Docs | yes | docs/ | n/a | — | README list lacks RELEASING, CHANGELOG, audit, coverage |
| Lagrange default, "all three selectable" | Deviations | partial | FractionalDelayLine | no | — | Still not user-selectable |
| Cutaway drawn, not clipped | Deviations | yes | PluginEditor | — | — | |
| Theme structure unchanged incl. "scrolling data stream" | Deviations | yes | DataStreamDisplay in footer | footer | DataStream.* | Was false |
| `THIRD_PARTY_LICENCES.txt` | Licence | yes | /THIRD_PARTY_LICENCES.txt (287 lines) | — | Legal.thirdPartyLicencesNameEveryBundledDependency | Was false |
| Help › About licence | Licence | yes | HelpContent | HELP | — | |

---

### TODO.md

**Summary:** 30 rows. **22 yes / 6 partial / 2 no** (was 2 / 11 / 17). `spec/TODO.md` has not been updated since f63a7f7, so its "In progress" and "Remaining" lists are now mostly stale. The merged branches closed:
- G, V, 2h, 2k (all five), 2d, 6e, 7, 8 (grid), 9, 10, 11, 12, 13 (help icons and docs), 14 and 14b, 14c and FirstRun.

What remains:
- The support links (13).
- Scrape and the rest of slap, 5b and 13b: in progress on techniques.
- Muting: in progress on techniques.
- The platform installers and a full polish or bug-bash pass (15-17).

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Scrape / strum / slap integration green | In progress [x] | partial | ScrapeEngine, SlapEngine in PB; SlapGroup (bass) | Slap 12/25 on CHAR (bass only); scrape none | Scrape.*, Slap.*, BassTechniques.theSlapGroupIsShownOnlyOnABass | Rest in progress on techniques (TechniqueUi / TechniquePages) |
| Remaining WIP: Muting + MuteGroup | In progress | no | Source/WIP/Rhythm/Muting, WIP/UI (not built on HEAD) | no | — | In progress on techniques (moves to Source/UI/MuteGroup, MutingTests) |
| WIP: FirstRun + FirstEncounterHint | In progress | yes | Source/UI/FirstRun*.cpp, FirstEncounterHint.cpp | Options › Diagnostics; first launch | FirstRun.* (5); FirstEncounterHint.* | Merged |
| 13c phase-2b realism | In progress | yes | NoiseFloor, SustainDecay, StabilityModel, StringAging, EnvironmentModel, BodyCouplingBank, RightHand, Harmonics | CHAR NOISE FLOOR / SUSTAIN / TUNING STABILITY / STRING AGING / ENVIRONMENT / RIGHT HAND / HARMONICS / STRING INTERACTION | RealismUi.everyRealismCParameterHasAControlOnTheCharacterTab; *Tests per spec | Merged with UI |
| G realistic illustration remainder | In progress | yes | GuitarRenderer, HeadstockOutlines, GuitarThumbnails, IllustrationMotion | Easy / Advanced / browser | FamilySwitch.*; Thumbnails.*; NoteDots.*; ReducedMotion.*; BenchZoom.*; Headstocks.* | |
| C spec-coverage.md | In progress | yes | docs/spec-coverage.md; docs/coverage/*; docs/audit/GAPS_AUDIT.md | — | — | "In progress" is stale |
| 6e off-thread part swap | In progress | yes | WorkshopBench live swap at a block boundary | Workshop | PartSwap.aSwapKeepingTheStructureIsTakenAtABlockBoundaryWithoutSilence (3) | |
| 5b slide-technique-controls | In progress | no | — | no TECHNIQUES tab on HEAD | — | In progress on techniques (SlideTechniqueTests) |
| V visual appeal | Remaining | yes | Theme accent choices; StageTouches (VU, room light); screenshot harness | Options › Appearance | Accent.*; StageTouches.*; Screenshots.everyPanelInEveryPalette | Theme.md deviations remain on purpose |
| 2k chambering feedback term | Remaining | yes | Chambering.h; FeedbackLoop::bodyCouplingFor | n/a | ModelGaps.chamberingFeedsTheFeedbackCoupling | |
| 2k capture chord / technique → NOTATION history | Remaining | yes | PerformanceCapture hooks from the engine | NOTATION | ModelGapsUi.theCaptureHearsTechniquesAndChordsFromTheEngine | |
| 2k allocation counter | Remaining | yes | global operator new counter (CircuitTests) + ThreadProbe | — | Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks | Old `LUTHIER_ALLOCATION_COUNTER` blocks remain undefined |
| 2k FeedbackLed 20 Hz vs 30 | Remaining | yes | FeedbackLed | Col3 | ModelGapsUi.theFeedbackLedDrainsAtThirtyHertz | |
| 2k migrate-on-load backup | Remaining | yes | PresetManager | — | ModelGapsUi.aMigratedPresetKeepsItsOriginal; Editor.aMigratedPresetRaisesOneInfoBanner | |
| 2k notation export on worker | Remaining | yes | NotationExport | NOTATION | ModelGapsUi.notationExportRunsOnAWorkerThread | |
| 2h Easy amp card cramped | Remaining | yes | AmpFace.cpp; EP rig strip | Easy | Screenshots.*; EasyLayout knob-size test | |
| 2d bass pattern; crossing velocity; fb / freeze / E-Bow mod dest; Aux 1 toggle | Remaining | yes | RhythmEngine::setBassPattern; ModSources; `aux1_pre_circuit` | RHYTHM Bass voicing; MOD; ROUTING | ModelGapsUi.theSustainControlsAreModulationDestinations; ModelGapsUi.auxOneTapsBeforeOrAfterTheCircuit | |
| 7 Workshop remaining | Remaining | yes | WorkshopPanel (per-string strings, nut, accessories, padlock, wrench, SR summary) | WORKSHOP | WorkshopStrings.*; WorkshopNut.*; WorkshopAccessories.*; WorkshopRanges.*; WorkshopEditor.*; WorkshopSpectrum.theSummaryIsAnnounced… | |
| 8 StrumGesture then BassTechniques SLAP group + bass step grid | Remaining | partial | SlapGroup, BassGridGroup, BassStepGrid | CHAR (bass); RHYTHM BASS GRID | BassTechniques.* (16) | 13 slap params still unattached (techniques) |
| 9 capture techniques, marked region, tab dots, Mono chord extraction | Remaining | yes | PerformanceCapture; CaptureRanges; FretboardComponent tab dots; offline chords | NOTATION; fretboard | CaptureRanges.*; ModelGaps.aMonoTakeGetsItsChordsOffline | |
| 10 MIDI import UI, marked ranges, CHARACTER / env events, practice drag | Remaining | yes | MidiImportTargets; CaptureRanges; ModSources events; PracticePanel drag | File; NOTATION; Practice | MidiImport.*; ModelGapsUi.characterSeedAndEnvironmentGoOut…; PracticeGaps.theSaveButtonDragsTheSavedTakeOut | |
| 11 SessionRecorder audio / MIDI / autosave; looper / trainer hooks | Remaining | yes | PracticeRoutineSetup; Looper; Trainers | Practice | PracticeGaps.* (7) | |
| 12 Tune: popovers, drag, Vary, note editing, bass / layer editors, export, hum, Ctrl+T, 15-07…15-10 | Remaining | yes | TuneChordEditor, TuneChordPillsEditing, TunePianoRoll, TuneLayersStrip, TuneExportDialog, TuneHumCapture, TuneVary, TuneSetlistStrip | TUNE | TuneEditing.* (8); TuneIntegration.* (13); HumCapture.* | |
| 13 support links placeholders; per-panel ? icons; stale docs | Remaining | partial | SupportLinks.h (placeholders by default); PanelHelpButton; docs updated | HELP; panel `?` | Onboarding.everyPanelsHelpIconOpensItsOwnTopic | **Links are still a release blocker** |
| 13b phase 5b technique specs + TECHNIQUES tab | Remaining | partial | engines exist | no TECHNIQUES tab on HEAD | Scrape.*, Slap.* | In progress on techniques (TechniquesPanel, Bend/Tap/SlideTechnique/Cascade/TechniquesUi tests) |
| 14 audit ui-wiring / onboarding / perf / qa / installer | Remaining | yes | docs/coverage/VISUAL-WORKSHOP-QA.md; GuiReach | — | GuiReach.*; Onboarding.*; PerfBudget.*; InstallLayout.* | Several items deferred (PB-8.3/8.5, QA-2.1, QA-5.2x, platform installers) |
| 14b action-and-undo audit | Remaining | yes | UndoHistory; UndoHistoryPanel | File › Undo history | Undo.*; UndoCoverage.* | AU-8b family-switch warning merged via wip/vwq-undo |
| 14c Restore first-run clears ranges flag | Remaining | yes | FirstRun restore | Options › Diagnostics | FirstRun.restoreClearsTheSettingsAndTheOneTimeFlagsAndKeepsTheLibraries | |
| 15-17 polish / perf / onboarding / installer / bug bash | Remaining | partial | Onboarding tour; PerfBudget; packaging; BETA report | — | Onboarding.*; PerfBudget.*; Combo.* | Idle CPU (B-13); Windows and macOS installers unverified; open B-items |
| 18 plugin targets build clean | Remaining | partial | VST3 / Standalone / CLAP (Linux); AU on APPLE | — | CI build.yml matrix (ubuntu, windows, macos) | No recorded Windows or macOS green run in the repo |

---

### CLAUDE_CODE_BRIEF.md

**Summary:** 22 rules and steps. **8 yes / 10 partial / 4 no** (was 4 / 10 / 8).

What moved to yes:
- Realism wiring.
- Ambiguity resolutions (2d).
- The onboarding pass.
- The audits.

Still violated:
- Threading contract: the UI thread calls `engine.panic()`, `getString().setDamping()` in AP:127 and FretboardComponent.cpp:110, and `TuningEngine::setDetuneCents` in GuitarBodyComponent.cpp:597.
- Every parameter in the APVTS: per-string detune, fine tune, custom tuning and string mute are still engine state.
- Locale catalog: AdvancedPanel, EasyPanel, HeaderBar, Overlays and TunePanel still have zero `tr(` calls.
- No READY TO SHIP marker.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Surface every engine feature in the GUI | Problem 1 | partial | GuiReach enforces it | — | GuiReach.everyAutomatableParameterHasAVisibleControl (expected to fail on HEAD) | Still missing: scrape (14), slap (13), macros 7/8, `pickup_blend`, `tune_feel_mod`, `tune_tempo_drift`; custom amp, per-string whammy, CC map and interpolation APIs |
| Realism specs landed | Problem 2 | partial | realism-a/b/c merged with UI | CHAR | Realism*Tests | Phase 5b in progress on techniques |
| Step 2: audit against gui-integration 19 | Order | yes | docs/audit/GAPS_AUDIT.md; docs/spec-coverage.md | — | GuiReach.* | |
| Step 3: panels via attachments, theme, a11y, tooltip, docs, PhysicalRange | Order | partial | LuthierKnob::attachTo; RangesUi | — | ScreenReader.everyAttachedControlHasAName; RangeMarking.* | Headstock detune and style boxes bypass attachments |
| Step 4: ambiguity-resolutions applied | Order | yes | MODEL-GAPS 2d items | — | ModelGapsUi.* | |
| Step 5: realism modules wired | Order | yes | NoiseEngine, NoiseFloor, GuitarCircuit, SlideEngine, StringAging, Stability, Environment, BodyCoupling | CHAR | many | |
| Step 6: Tune Builder MIDI-only; MIDI export round trip | Order | yes | TunePlayer; MidiExport | TUNE; MIDI OUT | TuneIntegration.aLuthierProfileMidiExportReimportsToTheSameAudio | |
| Step 7: polish pass (qa-polish 4, 5) | Order | partial | EffectsQa, PresetQa, WorkshopQa, RealismQa, Screenshots | — | Effects.*; Presets.*; Screenshots.* | QA-2.1 golden renders and QA-5.21-5.29 deferred |
| Step 8: performance pass | Order | partial | PerfBudgetTests; CpuRelief ladder | Options › Diagnostics | PerfBudget.*; CpuRelief.*; Memory.* | Idle CPU over budget (B-13); PB-8.3/8.5 deferred |
| Step 9: onboarding pass / tour | Order | yes | UI/Onboarding (12-stop tour, banner, discovery week, new-feature dots) | first launch; HELP | Onboarding.* (12); NewDots.* | |
| Step 10: installer pass | Order | partial | packaging/*, scripts/package_*.sh/.ps1, release.yml, docs/RELEASING.md | — | InstallLayout.*; FileOpen.* | Linux verified; Windows and macOS platform-deferred |
| Step 11-12: bug bash, human check | Order | partial | docs/audit/BETA_TEST_REPORT.md | — | Combo.*; GuiReach.* | Human check not recorded |
| Do not skip or drop spec features | Rules | partial | — | — | — | See Top gaps |
| No features outside spec (proposal first) | Rules | partial | visual-polish proposal theme | — | — | Unchanged |
| Column-4 tab order WORKSHOP … HELP | Rules | yes | AP buildWorkspace | tab strip | Editor.everyWorkspaceTabSelectsAndPaints | TECHNIQUES tab in progress on techniques |
| Threading contract | Rules | no | PP::panic from HeaderBar.cpp:77 / PluginEditor.cpp:792; AP:127; FretboardComponent.cpp:110; GuitarBodyComponent.cpp:597 | — | — | Engine try-lock (B-01) narrows but does not remove these |
| Every parameter in APVTS | Rules | no | TuningEngine per-string state; string mute | — | — | Workshop string overrides are part state (acceptable as structural) |
| Structural state via command queue | Rules | yes | Workshop live swap at block boundary; IR installer; engine lock | — | PartSwap.*; Combo.structuralChangesWhileAudioRuns | Was partial |
| Every string in locale catalog | Rules | no | Localisation exists; panels hard-code | — | Localisation.* | Violation |
| Every feature has tests from its spec's Tests section | Rules | partial | 1198 tests; many spec IDs (FA01…, HR01…, SA01…, BC01…, ENV01…) | — | — | QA-5.2x, PB-2.1 and golden renders missing |
| PROGRESS.md updated per milestone | Rules | yes | spec/PROGRESS.md | — | — | |
| Done: READY TO SHIP marker and sign-off | Done | no | none | — | — | |

---

### Top gaps (group A)

Ranked by what a user would hit first.

1. **Nothing stops everything** (issues.md 9, BETA B-14). `PP::panic` covers only the audition, the engine and the preview MIDI. The looper, backing track, progression looper, tune player, rhythm engine and metronome keep playing after Panic and after Reset.
2. **Scroll UX is still broken** (issues.md 7). Scrollbars are low-contrast, with no arrows or hint. The mouse wheel over a knob moves the knob instead of scrolling the column. No code change since the last audit.
3. **Techniques still have no controls on HEAD.** All 14 `scrape_*` and 13 slap parameters are unattached, and `slap_armed` and `scrape_armed` default off. There is no TECHNIQUES tab, and pre-bend and the mute, tap and bend controls are missing too. All of this is in progress on techniques, which has not been merged.
4. **RHYTHM tab clips the STRUM group** (issues.md 8, B-11). `AdvancedPanel::resized` gives preferred height only to TUNE, MIDI OUT, NOTATION and PRACTICE, not to `RhythmPanel::preferredHeight()`, so 6 strum/chuck controls are never on screen.
5. **Other dead or unreachable controls:**
   - `pickup_blend` is still never read by the engine.
   - `macro_assign_a/b` have no knobs.
   - `tune_feel_mod` and `tune_tempo_drift` are new, automatable and have no control.
   - `string_age` is inert but not listed as hidden, so GuiReach will fail on these.
6. **Threading violations remain.** The UI thread calls `engine.panic()`, `setDamping` (string mute, in both the StringRow and the fretboard) and `setDetuneCents` (headstock).
7. **Support links ship as `.example` placeholders by default.** Nothing in release.yml forces the `-DLUTHIER_*` overrides, and this is a release blocker.
8. **Easy mode still has no clickable fretboard,** and its macros are still `Size::Small`. The piano-roll strip partly makes up for it.
9. **The Advanced per-string editing that spec.md promises is still missing:** fretless, custom frets, per-fret offsets, a note picker for custom tuning, and per-string sustain. Material and gauge are now per string, but only in the Workshop.
10. **Idle CPU is about equal to playing CPU** (B-13), 6-12x over the idle budget. There is no silence short-circuit, and host soak tests and mono checks per preset are still absent.
11. **Engine APIs with no caller:** custom amp (`setCustomStages/PowerTube/ToneStackStyle`), `WhammyEngine::setPerStringEnabled`, `FractionalDelayLine::setInterpolation` (README says "selectable"), `setAftertouchTarget`, and a CC-map editor.
12. **Locale-catalog rule broken** across all major panels (zero `tr(` calls).
13. **Theme deviations:** guitar-shop palette and fonts, skeuomorphism, no 80 ms animation, no hover brighten or resize cursor.
14. **No tuner and no tuner-mute pedal.**
15. **Realism detune uses a fixed seed** (Parameters.cpp:1904) and has no re-roll button.
16. **Validator warnings never reach the user.** Impossible tension, all pickups off and notes below range (B-09) are clamped or dropped silently.
17. **Missing vibrato variants:** classical and blues vibrato shapes, and humanise vibrato variation.
18. **Amp standby warm-up is 8 s** (spec: 30 s).
19. **Preferences gaps:** no default preset on load, no remove-folder button, no MIDI-map defaults, no FFT or polyphony settings.
20. **Platform deliverables unverified:** the Windows and macOS installers, AU and notarisation have no recorded green run. spec/README still lists `DSP/Cable/` and does not list CLAP or the installers.

### Unspecified gaps noticed

- **No chromatic tuner display.** Every commercial guitar plugin has one, and the new stability, aging and environment drift makes one more useful.
- **`NEEDS_MIDI_OUTPUT FALSE`** (CMakeLists.txt:49), although the MIDI OUT tab offers live MIDI out and routing.
- **No CPU or voice meter in the header.** CPU relief shows only a banner at step 7, and the Diagnostics audio-path view is buried.
- **The legacy doubler migration runs on every load** (B-07). A session with hidden `doubler_on` changes its own rig when reopened.
- **`string_age` is inert but still automatable**, so hosts show a dead "String Age" parameter. It should be hidden or marked legacy, like `fret_action`.
- **The style combo boxes (`noise_floor_style`, `sustain_style`) are not parameter attachments.** Host automation of them does not update the box, and GuiReach may count them as unreachable.
- **Two header constants share one ID** (`restStroke` and `bassRestStroke` = `rest_stroke`). This is harmless, but the merge left both.
- **No favourites or stars in the preset browser.** Thumbnails now exist.
- **No DI / re-amp input path.** There is a Sidechain input bus, but nothing plays a real guitar through the rig.
- **ReverbPedal `rebuildLines` may still allocate on the audio thread** when Size grows (B-01 follow-up).

### Small glue candidates

Each item below has an engine feature, API or parameter that exists and lacks only a UI attachment, a preset field or one call.

| Item | Exact IDs / API | Where it should go | Effort |
|---|---|---|---|
| Stop-everything panic | `Looper`, `BackingTrackPlayer`, `ProgressionLooper`, `TunePlayer`, `Metronome`, rhythm engine stop; `EffectsChain::reset` | `LuthierAudioProcessor::panic()` (PP:2186), posted to the audio thread | ~15 lines |
| RHYTHM preferred height | `RhythmPanel::preferredHeight()` | one more `dynamic_cast<RhythmPanel*>` branch in AdvancedPanel.cpp:1476 | 1 line |
| Wheel scroll vs knob; visible scrollbars | `juce::Slider::setScrollWheelEnabled(false)` on knobs inside column viewports; accent thumb in `LuthierLookAndFeel::drawScrollbar` (Theme.cpp:1145) | Widgets.cpp LuthierKnob; Theme.cpp | small |
| User macros 7/8 knobs | `macro_assign_a`, `macro_assign_b` | tab MOD header row; Easy strip | 2 attachTo |
| Pickup blend | `pickup_blend`; read `blendAmount` in `PickupEngine::process` | Col2 PICKUPS; illustration | 1 knob + ~5 lines DSP |
| Tune timeline params | `tune_feel_mod`, `tune_tempo_drift` | TUNE tab transport row | 2 attachTo |
| Hide legacy `string_age` | `ParamIDs::stringAge` | add to `intentionallyHidden()` and mark "(legacy)" | naming |
| Scrape / remaining slap | 14 `scrape_*`; 13 `slap_*` listed above | merge `claude/luthier-techniques` (TechniquesPanel) | merge |
| Per-string whammy | `WhammyEngine::setPerStringEnabled` | new bool param in Col1 Bridge | 1 param + toggle |
| Custom amp controls | `AmpEngine::setCustomStages/setCustomPowerTube/setCustomToneStackStyle` | 3 params shown for `amp_model` = Custom | 3 params |
| CC map / aftertouch editor | `MidiInterpreter::setCcTarget`, `setAftertouchTarget` (preset `midiMap` saved) | Options › MIDI page table | UI only |
| Harmonic / pinch velocity trigger | `TechniqueEngine::setHarmonicVelocityThreshold`, `setHarmonicVelocityTriggerEnabled` | CHAR HARMONICS row | params + knobs |
| Realism-detune re-roll | `TuningEngine::randomiseRealismDetune(max, seed)` with a fresh seed stored in `realismDetuneCents` | button beside `realism_detune` in Col1 | one button |
| Interpolation choice | `FractionalDelayLine::setInterpolation` | Options › Audio | 1 param |
| Custom tuning / temperament editors | `openFrequencyHz` + `useCustomTuning`, `customTemperament`; `TuningEngine::setOpenFrequency`, `setCustomTemperament` | headstock TuningPopover note picker; Temperament = Custom editor | UI only |
| Easy macro size | EP `LuthierKnob::Size::Small` → `Size::Large` (EasyPanel.h:121-128) | Easy playing strip | layout |
| Easy fretboard | existing `FretboardComponent` (+ `onGhostDots` already wired to the illustration) | Easy band 1 under the illustration | layout |
| Support-link guard | `SupportLinks::areConfigured()` | release.yml step that fails a tagged build while it returns false | CI step |
| Validator notices | `Validator::getRecentRecords` / `getSummary` | editor info banner (the migrated-preset banner already exists) | small |

---

## Group B: GUI

Audited at `243c7f7` (HEAD of `claude/luthier-audit`). This tree contains the merged realism-a, realism-b, realism-c, model-gaps, tune-help, visual (including `wip/vwq-qa` and `wip/vwq-undo`), release and review branches. The one branch not merged is `origin/claude/luthier-techniques`, which I inspected with `git diff HEAD...origin/claude/luthier-techniques` and its `docs/coverage/TECHNIQUES.md`. Rows it would close say **in progress on techniques** and are counted as `no`. Paths are relative to `Source/` unless stated. `Source/WIP/*` (MuteGroup, Muting) is still not compiled.

I checked each row against the code. The workstream coverage docs (`docs/coverage/*.md`) were only used to find symbols and tests. Test names are `Suite.test`. I did not run anything, because a build is in progress.

### gui-integration.md

The GUI changed a lot since the last audit. These are now built:
- panel `?` icons and NEW dots (the dots have an empty table for 1.0)
- drag-to-modulate, and right-click items 12-13 (Automation ID, Show in Shortcuts)
- the WORKSHOP padlock and W for the Workshop
- Ctrl+T, which opens the TUNE New menu
- MIDI import from the File menu and by drag-drop
- the bass SLAP group and the bass step grid
- the Diagnostics audio-path view and feature-flag mirror
- the Guitars/ and Parts/ folder buttons
- the full set of APPEARANCE switches
- the data stream (it is in the footer, not the main area)
- pick, slide and capo on the bench
- the reduced-motion noise count
- UI scale that is actually applied
- 200 ms undo grouping with boundaries
- the 1000-op undo random walk
- the new §19 rows for noise floor, sustain shape and tuning stability

These are still missing:
- the window size contract (940x560 minimum, fixed aspect ratio)
- in the header: snapshot strip, input and output meters, tap, and the gear/dice/reset icons
- collapse chevrons, per-source mod arcs, macro knobs and "Assign to macro"
- the CHARACTER circuit mirror and the CC-map editor
- most empty-state texts
- Live-mode tooltip suppression, and the footer voice count and status line

Of the automatable parameters, **33 still have no control** (listed in Top gaps 1). 27 of them (`scrape_*` and the `slap_*` trigger/arm set) are in progress on techniques.

Counts: **yes 87 / partial 31 / no 14**.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Every automatable param has exactly one canonical UI control | 0.1 | partial | Parameters.h vs UI/* | no for 33 params | GuiReach.everyAutomatableParameterHasAVisibleControl (fails today) | Still no control: `scrape_*` (14) and `slap_armed/type/trigger/velocity_zone/trigger_cc/ghost_cc/force/palm_position_mm/string_mask/ghost_mode/rebound_gap/snap_back/body_part` (13), all in progress on techniques; `macro_assign_a/b`, `pickup_blend`, `string_age` (superseded by `string_age_hours` but not in `intentionallyHidden`), `tune_feel_mod`, `tune_tempo_drift`. Slap/pop/ghost/double-thump amounts and all realism-a/b/c params now have controls |
| Every feature within 3 interactions | 0.2 | partial | — | — | Onboarding.theTourWalksEveryStopAtEverySize (indirect) | Scrape and slap arming are unreachable until techniques lands |
| Easy never hides audible behaviour | 0.3 | partial | EasyPanel | Easy | EasyLayout tests | Slap or scrape armed via preset/MIDI has no Easy summary; the technique pills are in progress on techniques |
| Nothing discoverable only by right-click | 0.4 | partial | LiveStrip SnapshotStrip::showSlotMenu; ModMatrixPanel | — | none | Snapshot rename/colour on the Live strip is right-click only (the LIVE tab has buttons). Modulate is reachable by right-click, drag or the MOD tab |
| Panels fixed (no dock/drag) | 0.5 | yes | AdvancedPanel | — | — | |
| Panel header: uppercase + accent bar, status line, collapse chevron, per-preset collapse memory | 0.6 | partial | AdvancedPanel::Column::addSection, PanelHelpButton | Adv | none | Heading and `?` exist. No chevron, status line or collapse state (VWQ doc lists it as deferred) |
| Panels present/absent, never greyed | 0.7 | yes | SlapGroup::preferredHeight (0 on non-bass), EasyPanel whammy, SlideGroup | CHARACTER | BassTechniques.theSlapGroupIsShownOnlyOnABass | The Workshop Slide category is greyed on purpose (workshop-ui 9) |
| Min window 1280x800, cap 2560x1600 | 0.8 | no | PluginEditor.h minimumWidth=940/minimumHeight=560; PluginEditor.cpp:180 setSizeLimits(...,2400,1440) + setFixedAspectRatio | — | Reflow.noControlHangsOutsideItsParentAtAnyWidthOrScale | Unchanged |
| Advanced-range arc in warning colour + `*` suffix | 0.9 / 21 | yes | Widgets LuthierKnob, RangesUi | all knobs | RangeMarking.theWarningArcAppearsPastStockAndGoesWhenReturned | |
| Padlock at top of tabs holding advanced values | 0.9 / 21 | yes | AdvancedPanel RangeTabButton (CHARACTER, WORKSHOP) | tabs | WorkshopRanges.theTabCarriesAPadlockAndHeightsStopAtStock, NoiseUi | Was partial |
| Header 32 px; footer 16 px | 1 | yes | Metrics::headerHeight/footerHeight | — | — | |
| Brand + signature notch Easter egg | 2 | yes | HeaderBar (display face + brass mark), PluginEditor getSecretPixelBounds | header | Screenshots.everyPanelInEveryPalette | |
| Preset prev/name/next/save/A-B | 2 | yes | HeaderBar presetPrev/Name/Next, compareA/B, File>Save | header | Undo.aPresetLoadIsOneNamedEntry | Save is a File-menu item |
| Easy/Advanced pill, wrench, slide glyph | 2 | yes | HeaderBar modeButton/workshopButton/slideButton | header | WorkshopEditor.theWrenchOpensTheBenchInEasyModeAndTheTabInAdvanced | |
| Snapshot strip (8 buttons + prev/next + bank) in header | 2 / 8 | no | SnapshotStrip is only in LiveStrip | Live strip only | — | Unchanged |
| Input meter, output meter, output LED | 2 | partial | HeaderBar OutputLed | header LED only | none | No input meter and no header output meter (Easy has LevelMeter and VuMeter) |
| Utility: Panic, MIDI Learn, tap tempo, kill (Live) | 2 | partial | HeaderBar panicButton/midiLearnButton | header | MidiLearn tests | Tap and kill are only on the Live strip |
| Overflow: gear, help, dice, reset | 2 | partial | HeaderBar helpButton; File menu Options/Randomise/Reset | File menu | — | The tour's "gear" stop points at File (onboarding decision) |
| Header collapse below 1280 (numeric snapshot, 3-dot menu) | 2 | partial | HeaderBar::resized `compact` (<1280) | header | Screenshots | Controls shrink; no numeric snapshot and no 3-dot menu |
| Every header control has tooltip + documented shortcut | 2 | partial | HeaderBar setTooltip | — | — | Guitar and tuning selectors have no shortcut |
| A/B compare transient, not serialized | 2 | yes | PluginProcessor slotB | header | Undo.doesNotMoveTheViewOrTheTune | |
| Range-lock padlock next to preset name | 2 | yes | HeaderBar rangePadlock | header | RangesUi | |
| Easy layout: illustration + rig + playing/tone/rhythm strips | 3 | yes | EasyPanel (+ VuMeter/RoomLight, piano roll) | Easy | EasyLayout theWindowMatchesSection3 | |
| Headstock click: tuning popover (per-string, capo, temperament) | 3.1 | yes | GuitarBodyComponent TuningPopover (+ stability offset/Retune) | Easy | Editor theHeadstockPopoverEditsPerStringTuning, UndoCoverage.headstockDetuneIsOneGroupedEntryPerString | Per-string detune is still engine state, not a parameter |
| Pickup click selects active pickup | 3.1 | yes | GuitarBodyComponent onPickupSelected | Easy | Editor everyHitRegion... | |
| Bridge click: whammy popover only if fitted | 3.1 | yes | WhammyPopover::isWhammyFitted | Easy | Editor theBridgePopoverAppearsOnly... | |
| Fretboard shows played notes live | 3.1 | yes | GuitarOverlay, IllustrationMotion NoteDots | Easy | NoteDots.theDotAppearsWithin60msAndFadesOver60ms | |
| Rig strip: circuit card (vol, tone, visualiser) | 3.2 | yes | EasyPanel circuitView | Easy | — | |
| Rig: pre/post racks 8 slots, click opens pedal popover | 3.2 | yes | CompactRack | Easy | FacesIntegration | |
| Rig: amp (model + 6 knobs) | 3.2 | yes | EasyPanel AmpFacePanel | Easy | FacesIntegration theEasyAmpCard... | |
| Rig: cab model, mic1, mic2, blend | 3.2 | yes | EasyPanel | Easy | — | |
| Rig: room size, wet/dry | 3.2 | yes | EasyPanel roomSize/roomMix, RoomLight | Easy | StageTouches.theRoomLightFollowsSizeAndWet | |
| Playing strip: mode, humanize, character macro, whammy if fitted | 3.3 | yes | EasyPanel | Easy | EasyLayout | Mode is Mono/Poly/Controller, not Mono/Poly/Chord |
| Tone strip: input, output, wet/dry, width | 3.4 | yes | EasyPanel | Easy | EasyLayout theToneStripIsHeard | |
| Rhythm strip: kit + dice, feel, enable, chord + next-strum readout | 3.5 | yes | EasyPanel rhythm* | Easy | — | |
| Advanced 4 columns, each scrollable | 4 | yes | AdvancedPanel | Adv | Editor | |
| Col1 GUITAR: library, tuning, capo, temperament, scale readout, "Open in Workshop" | 4.1 | partial | AdvancedPanel::buildColumn1 (comment at :385 still says "the Workshop does not exist") | Adv col1 | — | No library list and no Open-in-Workshop button. The stale comment should be updated |
| Col1 BODY: model, dims, woods, bracing, air readout | 4.1 | yes | AdvancedPanel body* | Adv col1 | — | No link to CHARACTER |
| Col1 STRINGS: material/gauge per string, age, tension readout, DECAY row | 4.1 / 19 | partial | String Set section, stringAgeHours, DecayRow | Adv col1 | RealismUi tests | Per-string material only on the bench (Ctrl-click / inspector). DECAY row is new (realism-c) |
| Col1 WHAMMY: absent if none | 4.1 | partial | AdvancedPanel "Bridge" section (:599) | Adv col1 | — | Still always shown on a hardtail |
| Col2 PICKUPS: selector, per-pickup gain/phase, "Edit in Workshop" | 4.2 | partial | pickupSelector/pickupVolume | Adv col2 | — | No phase param, no Edit-in-Workshop, `pickup_blend` unattached |
| Col2 CIRCUIT incl. visualiser | 4.2 | yes | CircuitPanel CircuitResponseView | Adv col2 | LiveDisplays.theFretboardDrawsTheSlideBarAndTheCircuitCurveFollowsTheVolume | |
| Col2 PRE-FX rack: drag reorder, right-click bypass/delete | 4.2 | yes | PedalRack | Adv col2 | Undo.movingAPedalIsOneEntry | |
| Col3 AMP: model, tone, sag, bright, bias, master | 4.3 | partial | AmpFacePanel | Adv col3 | FacesIntegration | No sag or bias parameters |
| Col3 POST-FX, CAB, ROOM | 4.3 | yes | AdvancedPanel | Adv col3 | — | |
| Col3 SUSTAIN: freeze, e-bow, feedback LED | 4.3 | yes | AdvancedPanel, FeedbackLed (30 Hz) | Adv col3 | ModelGapsUi.theFeedbackLedDrainsAtThirtyHertz | |
| Col4 tab strip, fixed order of 13 | 4.4 | yes | AdvancedPanel tabs (wrap) | Adv col4 | Editor everyWorkspaceTabSelectsAndPaints | TECHNIQUES (14th) is in progress on techniques |
| WORKSHOP tab takes over cols 3+4 | 4.4 / 6 | yes | AdvancedPanel::resized | Adv | WorkshopPanel itPaintsAndTheWorkshopTabTakesOver... | |
| MOD tab: LFO/ENV/STEP/FOLLOW/MACROS/RAND cards + route table | 4.4 | partial | ModMatrixPanel ModSourceCard (SourceKind::plain for macros) | Adv MOD | Modulation, UndoCoverage.modPanelRouteEditsAreEntries | Macro card has no knobs; macros 7/8 have no control anywhere |
| RHYTHM tab: voicer, strum + fingerpick editors, feel, STRUM group | 4.4 | partial | RhythmPanel, StrumGroup | Adv RHYTHM | UndoCoverage.rhythmSettingsAndPatternEditsAreEntries | AdvancedPanel::resized (:1468) sizes RhythmPanel to the viewport and ignores RhythmPanel::preferredHeight(), so STRUM rows can get zero height (beta report B-11) |
| RHYTHM: bass step grid for bass family | 4.4 | yes | BassGridGroup (RhythmPanel.cpp:610), RhythmEngine::processBassGrid | Adv RHYTHM (bass) | BassTechniques.theStepGridPlaysItsTechniquesOnABass | Was no |
| TUNE tab | 4.4 | yes | TunePanel | Adv TUNE | TunePanel tests | |
| LIVE tab | 4.4 | yes | LivePanel | Adv LIVE | UndoCoverage.snapshotRenameColourAndDeleteAreEntries | |
| ROUTING tab incl. Aux 8 noise bus, Aux 1 pre-circuit, noise-floor to Aux 8 | 4.4 / 19 | yes | RoutingPanel noiseFloorToAux8, aux1 toggle | Adv ROUTING | ModelGapsUi.auxOneTapsBeforeOrAfterTheCircuit | |
| TONE MATCH tab | 4.4 | yes | ToneMatchPanel | Adv | ToneMatch tests | |
| CHARACTER: dead spots, wear, drift, electronics, body age, environment | 4.4 | yes | CharacterPanel (+ StringAging/Environment/BodyCoupling groups) | Adv CHARACTER | RealismUi tests | |
| CHARACTER > STRING NOISE / PICK / SETUP / SLIDE groups | 4.4 | yes | NoiseGroups, SetupGroup, SlideGroup | Adv CHARACTER | NoiseUi, WorkshopPanel aSlideNeedsSlideMode | |
| CHARACTER > SLAP group (bass only) | 4.4 | yes | SlapGroup | Adv CHARACTER (bass) | BassTechniques.theSlapGroupIsShownOnlyOnABass | New |
| CHARACTER > NOISE FLOOR / SUSTAIN SHAPE / TUNING STABILITY | 19 (new rows) | yes | RealismGroupsC NoiseFloorGroup/SustainShapeGroup/TuningStabilityGroup | Adv CHARACTER | RealismUi.everyRealismCParameterHasAControl... | New §19 rows. Style boxes write through RealismStyleActions, not an attachment |
| CHARACTER > CIRCUIT mirror | 4.4 | no | — | — | none | Unchanged |
| PRACTICE / NOTATION / MIDI OUT / CONTROLLERS / HELP tabs | 4.4 | yes | respective panels | Adv col4 | respective tests | |
| CONTROLLERS: custom CC-map editor, multi-controller merge display | 4.4 | no | ControllerMerge used only by ControllerProfile + tests | — | Controller tests | Unchanged |
| Last-used tab persists globally | 4.4 | yes | UiPreferences | — | Editor theWorkspaceTabWraps... | |
| Column widths; stack <1280; Adv unavailable <1000 | 4.5 / 13 | yes | AdvancedPanel::resized | — | Editor advancedModeIsRefusedBelow... | |
| Options overlay, Ctrl+, , 11 tabs, Escape | 5 | yes | OptionsPanel | File menu / Ctrl+, | Editor everyOptionsPageSelectsAndPaints | No header gear |
| APPEARANCE: accent tint, data-stream toggle, noise-strip toggle | 5 | yes | OptionsPages AppearancePage accentBox + switches | Options | Accent.theWindowTakesTheAccentAndFollowsTheGuitar, NoiseStrip.reducedMotionShowsAStaticCountAndAppearanceHidesIt | Was no. Also adds the Visual aids section (piano-roll spec) |
| DIAGNOSTICS: feature-flag mirror | 5 | yes | OptionsPages.cpp:2281 AudioPathView | Options DIAGNOSTICS | Diagnostics.theAudioPathShowsWhatIsSoundingAndTheFlags | Was no |
| FILE LOCATIONS incl. Guitars/ and Parts/ | 5 | yes | FileLocationsPage openGuitarsFolder/openPartsFolder | Options | review only | Was partial |
| Workshop bench layout | 6 | yes | WorkshopPanel | Adv WORKSHOP / Easy overlay | WorkshopPanel tests | |
| Workshop direct manipulation (pickup, height, saddles, nut, pick, slide, capo) | 6 | yes | BenchIllustration Drag::{pickup,saddle,nut,pick,pickRotate,slide,slideRotate,capo} | bench | WorkshopNut.*, WorkshopAccessories.* | Fret-wear brush still missing (see workshop-ui) |
| Bench A/B 8 slots | 6 | yes | WorkshopPanel slotButtons | bench | WorkshopBench abRecallRoundTrips | |
| Alt-hover audition on shadow spec | 6 | yes | WorkshopBench::beginAudition | bench | auditionNeverCommits | Refused under CPU relief step 6 |
| Easy wrench opens Workshop overlay; Esc closes | 6 | yes | WorkshopOverlay | Easy | WorkshopEditor.theWrench... | |
| Slide Mode header toggle + S | 7 | yes | HeaderBar::toggleSlideMode | header | Stress.slideAndAdvancedRangeTogglesMidPlay | |
| Slide: fretboard bar at position + slant | 7 | yes | FretboardComponent | fretboard | LiveDisplays.theFretboardDrawsTheSlideBar... | |
| Slide: Workshop illustration draws same bar (slant) | 7 | yes | GuitarOverlay slideFret/slideSlantDeg/slideColour | bench | WorkshopAccessories.theSlideTurnsOnTheBench... | Was partial |
| Slide: tuning popover shows glide target | 7 | no | — | — | none | Unchanged |
| Snapshot click load; Shift-click write; right-click rename/colour/clear | 8 | partial | LiveStrip SnapshotStrip::mouseDown | Live strip | UndoCoverage snapshot tests | Still no Shift-click write: a click on an empty slot captures |
| Snapshot colour tag, 12-char label, accent outline | 8 | yes | SnapshotStrip paint | Live strip | — | |
| Live strip contents | 9 | yes | LiveStrip | Live Mode | Live tests | |
| Live Mode: 44 px targets, lock Advanced toggle | 9 | yes | LiveStrip, HeaderBar | — | — | |
| Live Mode suppresses non-critical tooltips | 9 | no | PluginEditor.cpp:603 tooltip delay follows tooltipsEnabled only | — | none | Unchanged |
| Practice drawer 32–360 px, 8 tabs, open state restored | 10 | yes | PracticePanel, UiState::practiceDrawerOpen | drawer | ReturningUser.thePracticeDrawerComesBackAsItWasLeft | |
| Mod arcs: per-source colour, segmented | 11.1 | partial | Widgets.cpp:645 (single Palette::secondary arc) | knobs | none | Unchanged |
| Drag source card onto control creates 25% route | 11.2 | yes | Widgets kModSourceDragPrefix, addModulationFromDrop | knobs | DragToModulate.aDroppedSourceRoutesAt25PercentAsOneEntry | Was no |
| Right-click "Modulate ->" submenu | 11.2 | yes | Widgets showParameterMenu | knobs | Undo.rightClickModulationIsUndoable | |
| Footer: version, status line, CPU %, voice count | 12 | partial | PluginEditor::paint (:423) | footer | — | Version, CPU, latency, optional undo depth. No status line or voice count |
| Scrolling data stream | 12 | partial | PluginEditor dataStream (DataStreamDisplay) in the footer | footer | DataStream.itKeeps200StopsAfter500msAndHonoursReducedMotion | Built, but in the footer, not the "empty main area" |
| Reflow: Easy <900 rig becomes tab | 13 | no | — | — | none | Unreachable while the minimum width is 940 |
| Scale >125% auto-picks smaller scale + notice | 13 | no | setScaleFactor applied, no fit check | — | none | |
| Empty-state hints (7 texts) | 14 | partial | SlideEngine::kLowActionMessage (SlideGroup), RangesUi::kLockedNoticeText (Widgets.cpp:411), SlapGroup::kInactiveMessage (never displayed; the group hides) | — | BassTechniques (constant only) | 2 of 7 are shown with the spec wording. Missing: empty snapshot, no mod routes, empty setlist (different text), no backing track |
| Notifications: banner, 32 px, auto-dismiss | 15 | yes | NotificationCentre | — | Editor notificationBannersQueue... | |
| Triggers (preset error, IR, part, guitar, SR, update, policy, crash, licence, migration, CPU limit) | 15 | yes | PluginEditor pollForNotifications, CpuReliefUi | — | Editor.aMigratedPresetRaisesOneInfoBanner, CpuReliefUi.theBannerComesOncePerEpisodeAndGoes | |
| Trigger: advanced range clamped on preset save | 15 | partial | Widgets.cpp:318 bubble on per-control restrict | — | RangesUi | No banner on preset save |
| Control right-click: value, reset, copy/paste, MIDI Learn, Modulate | 16 | yes | Widgets showParameterMenu | knobs | Editor rightClick... | |
| Control right-click: Assign to macro (8) | 16 | no | Widgets.h:11 mentions it; not in the menu | — | none | Unchanged |
| Control right-click: unlock/restrict range | 16 | yes | kUnlockRangeMenuId/kRestrictRangeMenuId | knobs | RangesUi | |
| Control right-click: Automation ID; Show in Options>Shortcuts | 16 | yes | Widgets shortcutActionForParameter/showShortcutInOptions | knobs | ContextMenu.everyParameterShowsItsAutomationIdAndBoundOnesTheirShortcut | Was no |
| Panel right-click: collapse, reset panel, screenshot, docs | 16 | partial | AdvancedPanel::Column::mouseDown ("Docs: <section>") | Adv columns | none | Only Docs |
| Shortcuts all rebindable (registry) | 17 | yes | Accessibility.cpp buildDefaultShortcuts | Options | Accessibility.shortcutDefaultsMatchTheCanonicalTable | |
| Canonical shortcut list (P, \, [ ], 1-9, Tab, S, L, D, Ctrl+, ...) | 17 | yes | PluginEditor::keyPressed | — | Editor overlay tests | Kill toggles rather than holds |
| W toggles Workshop | 17 | yes | Accessibility.cpp:576 toggleWorkshop | — | WorkshopEditor | Was partial |
| Ctrl+T new tune | 17 | yes | Accessibility.cpp:633 newTune, editor openNewTune | — | TuneIntegration.ctrlTIsInTheShortcutRegistryAndOpensTheTuneTab | Was no |
| Space play/pause tune | 17 | partial | TunePanel::keyPressed (focused); global Space = audition | TUNE | — | Unchanged |
| Ctrl+E context-aware export | 17 | partial | PluginEditor.cpp:659 always `exportPanel` | — | — | TunePanel handles Ctrl+E only when it has focus |
| Undo stack of 64, groups within 200 ms | 18 | partial | UndoHistory::push (cap 200, 200 ms merge, injectable clock) | header, File > Undo history | Undo.gesturesGroupWithin200ms, Undo.overflowDropsTheOldest | Grouping done; depth is 200, not 64 |
| State boundaries need a modifier to undo across | 18 | yes | UndoHistory::canUndo boundaries; `undoAcrossBoundary` Ctrl-Alt-Z | header | Undo.aPresetLoadIsABoundary | Ctrl-Alt-Z rather than Shift |
| Feature-to-location index rows | 19 | partial | see rows | — | none | Missing: guided build, right-click part to drawer, technique rows (techniques) |
| Import MIDI (File menu / drag onto window) | 19 | yes | HeaderBar menu id 14, editor FileDragAndDropTarget importMidiFile | File menu | MidiImport.aDropOnTheWindowImports | Was no |
| Bass slap/pop/ghost/LH slap/double thump UI | 19 | partial | SlapGroup (amounts), BassGridGroup | CHARACTER (bass), RHYTHM | BassTechniques.* | Arming/trigger params (`slap_armed` and 12 others) have no UI: in progress on techniques |
| Guided build (templates) rail in Workshop | 19 | no | — | — | none | |
| Right-click illustration part -> parts drawer | 19 | no | GuitarBodyComponent.cpp:440 right-click pickup = parameter menu | — | none | |
| `?` icon on panels opening pinned Help | 20 | yes | PanelHelpButton on sections, workspace, Easy strips | everywhere | Onboarding.everyPanelsHelpIconOpensItsOwnTopic | Was no |
| NEW dot for one week | 20 | yes | NewFeatureDots (+ Onboarding DiscoveryLayer newDot) | tabs | NewDots.anEntryPointIsMarkedForItsFirstWeekOnly | Both tables are empty for 1.0. Two parallel mechanisms (see Unspecified) |
| Diagnostics "What's on the audio path" diagram | 20 | yes | AudioPathView | Options DIAGNOSTICS | Diagnostics.theAudioPathShows... | Was no |
| First-unlock explainer, once, re-armable | 20 | yes | RangesUi kExplainerText; FirstRun::restoreFirstRunExperience | — | FirstRun.theRangeExplainerSaysSectionSevensWords | |
| Slide bar overlay 6 px, material colour, 80 ms ease | 21 | yes | FretboardComponent | fretboard | LiveDisplays | No ease under reduced motion |
| Pick overlay on illustration with handles | 21 | yes | GuitarOverlay pickPositionMm/pickAngleDeg; BenchIllustration Drag::pickRotate | bench | WorkshopAccessories.thePickIsDraggedAndTurnedOnTheBench | Was no. The Easy illustration draws no pick |
| Buzz heatmap + dot glyph | 21 | yes | BuzzHeatmap | CHARACTER SETUP | — | |
| Circuit visualiser live | 21 | yes | CircuitResponseView | col2, Easy | LiveDisplays | |
| Noise event strip; static count under reduced motion | 21 | yes | NoiseEventStrip | CHARACTER | NoiseStrip.reducedMotionShowsAStaticCount... | Was partial |
| Pressure-state readout | 21 | yes | SlideGroup | CHARACTER SLIDE | — | |
| Test: every §19 entry resolves to a component | 22 | partial | — | — | GuiReach.* (params only) | GuiReach covers parameters, not the §19 feature list |
| Test: automation ID -> one focused control | 22 | no | — | — | none | |
| Test: tab-order walk | 22 | no | — | — | none | |
| Test: reflow at 6 widths x 6 scales | 22 | partial | — | — | Reflow.noControlHangsOutsideItsParentAtAnyWidthOrScale | 6 widths; scales only checked as applied, not laid out |
| Test: empty-state hints | 22 | no | — | — | none | |
| Test: 13 right-click items per param | 22 | yes | — | — | Editor rightClick..., ContextMenu.everyParameterShowsItsAutomationId..., RangesUi rightClick | Assign-to-macro item missing, so 12 of 13 items |
| Test: 1000-op undo random walk | 22 | yes | — | — | Undo.randomWalkUndoesBackToTheStart, Stress.undoRedoThousandTimes (gated) | Was no |
| Test: 10 000 random workshop clicks | 22 | yes | — | — | GuitarRenderer hit test (10 000 clicks per guitar) | |
| Test: Slide toggle 100x during playback | 22 | yes | — | — | Stress.slideAndAdvancedRangeTogglesMidPlay | Was no |
| Test: advanced-range arc marking | 22 | yes | — | — | RangeMarking.* | |

### ui-wiring.md

The plumbing is still built differently from this spec. There is no general command queue, no `uiState` ValueTree, no panel base API and no locale broadcast. These parts have filled in:
- a real block-boundary part swap (`LuthierEngine::swapPartsAtBlockBoundary`)
- lock-free MIDI Learn
- the data stream and reduced-motion noise strip
- drag-to-modulate
- a slide/range toggle stress test
- the 1000-op undo walk
- screen-reader names on every attached control

Counts: **yes 28 / partial 18 / no 8**.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Single APVTS, no side-channel audible state | 0.1 | partial | PluginProcessor apvts | n/a | Combo.everyParameterSurvivesTheSessionStateRoundTrip | Per-string detune, stability offsets, rhythm density/hand position, bass step grid and character maps are engine state (now in undo, still not params) |
| Attach via APVTS attachments only | 0.2 | partial | Widgets attachTo | n/a | ScreenReader.everyAttachedControlHasAName | EasyPanel rhythmFeelSlider and RhythmPanel densitySlider write the engine; RealismGroupsC style boxes write via RealismStyleActions |
| uiState ValueTree saved with preset | 0.3 | partial | PluginProcessor::UiState struct (`ui` block in session state) | n/a | VisualAids.theOptionsPersistAndStayOutOfPresets | A struct, now a session layer that undo and presets skip |
| Listeners removed in destructors | 0.4 | yes | e.g. HeaderBar dtor | n/a | none | No leak test |
| Structural state via command queue + atomic swap | 0.6 / 4.3 | partial | LuthierEngine::swapPartsAtBlockBoundary; ParameterBridge::getEngineLock try-lock; snapshot crossfade on timer | n/a | PartSwap.*, Combo.structuralChangesWhileAudioRuns | No LoadPreset/ShadowAudition/AddModRoute command queue. Was no |
| Params declared once | 1 | yes | Parameters.h/.cpp | n/a | Parameters.* | |
| Display name translated via i18n | 1 | no | Parameters.cpp literal names | n/a | — | |
| PhysicalRange stock/advanced pair | 1 | yes | PhysicalRange.cpp (+ strings family) | n/a | RangeTests | |
| Unit enum + category tag per param | 1 | no | — | n/a | — | |
| AttachedKnob: arc, mod arc, warning arc, sizes, right-click menu | 2 | yes | LuthierKnob | everywhere | RangeMarking, ContextMenu | Mod arc is one colour |
| AttachedSwitch momentary vs latching | 2 | partial | LuthierToggle | — | — | Latching only |
| AttachedCombo rebuilds on metadata change | 2 | no | LuthierChoice | — | — | |
| PartSlotWidget | 2 | partial | WorkshopPanel cards + BenchIllustration | Workshop | WorkshopPanel tests | No shared widget class |
| Panel base API (getPanelId, collapse, VT listener) | 3 | no | — | — | — | |
| Panels never own DSP state | 3 | yes | — | — | — | |
| APVTS atomics read on message thread | 4.1 | yes | — | — | — | |
| Display FIFO per subsystem with drain rates | 4.2 | partial | NoiseEngine ring, SoundingNotes double buffer, MidiLearn pendingLearnCc, Looper MIDI FIFO | — | NoiseStrip, PianoRoll.publishingTheSnapshotDoesNotAllocate | Meters and chord readout still poll atomics |
| Preset load at block boundary, ranges applied | 5 | partial | PresetManager::loadPreset + 5 ms fade-out/in (class 2) | — | Combo.presetSwitchUnderARingingNoteDoesNotClick | Message-thread apply behind a fade, not a posted command |
| Snapshot never changes ranges | 5 | yes | — | — | — | |
| LoadGuitar swap + part-acoustics refill | 6.1 | yes | processor applyGuitar / mapSpec | Workshop | WorkshopPreset, Workshop.guitarLoadStaysUnder300ms | |
| Part swap with 5 ms crossfade | 6.2 | yes | LuthierEngine::swapPartsAtBlockBoundary, applyPartSwapLive | Workshop | PartSwap.aSwapKeepingTheStructureIsTakenAtABlockBoundaryWithoutSilence | Block-boundary swap with a no-click check, not a literal 5 ms crossfade. Structural changes still park. Was partial |
| Shadow audition, 30 ms crossfade back | 6.3 | yes | WorkshopBench::beginAudition | Workshop | WorkshopBench.hundredAuditionsStayInBudget | 30 ms return not measured |
| Spectrum delta on worker, 40 ms budget | 6.4 | yes | Workshop/SpectrumDelta | Workshop | WorkshopSpectrum.hundredShadowRendersStayUnder40ms | |
| Ranges per family; clamp; ClampNotification banner | 7 | partial | RangesUi::changeRanges | Options RANGES | Undo.clampToStockIsOneEntry | No banner |
| Per-control unlock stored in preset | 7 | yes | lockedParameters | right-click | RangesUi | |
| MIDI Learn arm, next control | 8 | yes | MidiLearnArmLayer, dispatchPendingLearn | header | MidiLearn.learningDoesNotAllocateOrLockOnTheAudioThread | |
| MIDI Learn "Save as global" | 8 | no | — | — | none | |
| Meters: peak hold UI-side | 9 | partial | LevelMeter, VuMeter | Easy | StageTouches.theVuNeedle... | No header meters |
| Buzz heatmap | 9 | yes | BuzzHeatmap | CHARACTER | — | |
| Noise event strip; reduced-motion static count | 10 | yes | NoiseEventStrip | CHARACTER | NoiseStrip.reducedMotionShowsAStaticCount... | Was partial |
| LogStream data stream (200 lines, fade, stop 500 ms) | 11 | yes | DataStreamDisplay (footer) | footer | DataStream.itKeeps200StopsAfter500ms... | Was no |
| Mod arcs from snapshot at 30 Hz | 12 | partial | LuthierKnob paint | — | none | |
| Drag source -> AddModRoute 0.25 | 12 | yes | addModulationFromDrop | knobs | DragToModulate.* | Was no |
| GuitarIllustration, one class, interactionMode | 13 | partial | GuitarRenderer shared by GuitarBodyComponent and BenchIllustration | Easy + Workshop | GuitarIllustration tests | Pick overlay now drawn; still two components |
| NoiseEngine pool | 14 | yes | DSP/Noise | — | Squeak tests | |
| GuitarCircuit control-rate updates | 15 | yes | DSP/Circuit | — | Circuit tests | |
| SlideEngine pressure states | 16 | yes | DSP/Slide | CHARACTER SLIDE | Slide tests | |
| Full state serialisation (incl. bench slots, setlist, piano latch) | 17 | yes | PluginProcessor get/setStateInformation | — | Undo.benchSlotsAndSetlistAreInTheState..., HostState tests | |
| Missing parts fall back with a banner | 17 | yes | takeGuitarNotices | — | GuitarMigration | |
| Undo: every param change, compound structural | 18 | yes | UndoHistory, ScopedUndoAction | header, File > Undo history | Undo.*, UndoCoverage.* | |
| Workshop undo strings in real units | 18 | yes | WorkshopBench | — | WorkshopNut.aSlotDragIsOneEntryInRealUnits... | |
| Threading contract; ThreadPool of 2 | 19 | partial | — | — | Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks | No shared pool |
| LocaleCatalog; no literals; LocaleChanged -> refreshStrings | 20 | no | `tr (` about 15 uses in UI/PluginEditor vs about 300 literal setText/drawText/button texts | — | Localisation catalogCoversTheUi | No live refresh |
| AccessibilityHandler everywhere; custom handlers | 21 | partial | labelForScreenReaders on attach; CircuitResponseView and PianoRollStrip handlers | — | ScreenReader.everyAttachedControlHasAName | Fretboard, snapshot strip and heatmap have no custom handler |
| Test: attachment leak x100 | 23 | no | — | — | none | |
| Test: 60 s cross-thread invariant | 23 | yes | ThreadProbe lock trap + pthread interposer | — | Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks | Was partial |
| Test: preset load determinism | 23 | yes | — | — | Combo.renderIsDeterministicAfterReset, Combo.renderDoesNotDependOnWhatWasPlayedBefore | |
| Test: 1000 random undo/redo | 23 | yes | — | — | Undo.randomWalkUndoesBackToTheStart | Was no |
| Test: MIDI Learn 128 CCs | 23 | partial | — | — | MidiLearn.mapsAndUnmapsCleanly, Stress.midiLearnArmDisarmHundredTimes | |
| Test: display FIFO overflow | 23 | no | — | — | none | Capture ring overflow is tested, display FIFOs are not |
| Test: shadow audition 100 parts byte-identical | 23 | partial | — | — | WorkshopBench.hundredAuditionsStayInBudget (timing) | |
| Test: PhysicalRange toggle 100x no alloc | 23 | partial | — | — | Stress.slideAndAdvancedRangeTogglesMidPlay | No allocation counter in it |
| Test: part swap crossfade click threshold | 23 | yes | — | — | PartSwap.aSwapKeepingTheStructure... | Was no |
| Test: spectrum delta vs offline 0.2 dB | 23 | partial | — | — | WorkshopSpectrum null/real change | VWQ lists it as deferred |

### gui-engine-dataflow.md

These live elements are new or improved:
- data stream, VU meter (stale grey), noise-floor meter stale state, feedback LED at 30 Hz
- reduced-motion noise count
- preset-browser thumbnails (worker, 200-entry LRU)
- bench pick overlay
- piano-roll SoundingNotes snapshot with a 250 ms stale rule

Still missing: header input/output meters, header tap LED, the kill parameter, the pickup pulse, voice count, stale states on most elements, a `Tests/Ui/Dataflow` suite and the debug overlay. Counts: **yes 15 / partial 17 / no 3**.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| One source per element; no busy-wait; UI triggers no audio work | 0 | yes | — | — | Engine.fiveMinutesOfPlayback... | By inspection |
| Stale rule defined per element | 0.3 | partial | BuzzHeatmap/NoiseEventStrip isStale, AmpFace driveStale, VuMeter, RealismGroupsC kStaleSeconds, PianoRollModel (250 ms) | — | StageTouches.theVuNeedle...GreysWhenStale, RealismUi.theNoiseMeterReadsAndGoesStale | Missing on LED, chord readout, slide bar, footer |
| Input meter (header) | 2 | no | — | — | none | |
| Output meter (header) | 2 | partial | OutputLed in header; Easy LevelMeter/VuMeter | Easy | StageTouches | |
| Aux bus meters (ROUTING) | 2 | yes | RoutingPanel AuxStrip | ROUTING | Routing | No stale rule |
| Per-string activity: fretboard 60 Hz, compact strip 30 Hz | 2 | partial | GuitarOverlay, NoteDots; PianoRollStrip keys | Easy/Adv | NoteDots, PianoRoll.aChordLightsItsSoundingKeys... | No separate compact strip (the piano roll partly serves) |
| Output LED colour map, red hold 400 ms, stale | 3 | partial | OutputLed (Widgets.cpp ~1127, 30 Hz) | header | none | No hold or stale state |
| Chord readout 10 Hz, dim after 3 s | 4 | partial | EasyPanel chordLabel | Easy | — | ChordNameOverlay fades on the guitar (piano-roll spec), but the Easy readout does not dim |
| Next-strum arrow, hide after 500 ms | 5 | partial | EasyPanel rhythmReadout | Easy | — | Text only |
| Fretboard played-notes layer (60 ms decay) | 6.1 | yes | IllustrationMotion NoteDots | Easy/Workshop | NoteDots.theDotAppearsWithin60msAndFadesOver60ms | |
| Scale highlight layer | 6.2 | yes | FretboardComponent | drawer | PracticeTrainers | |
| Buzz heatmap layer, fade when stale | 6.3 | yes | BuzzHeatmap | CHARACTER | — | Not on the bench |
| Slide bar overlay, stale alpha | 6.4 | partial | FretboardComponent | fretboard | LiveDisplays | No stale alpha |
| Pick overlay (position, angle) | 6.5 | partial | GuitarOverlay pickPositionMm/pickAngleDeg (bench) | bench | WorkshopAccessories.thePick... | Parameter-driven on the bench only. No live overlay on the Easy illustration or fretboard. Was no |
| Pickup pulse layer | 6.6 | no | highlightedPickup = hover | — | none | |
| Mod arcs from contribution; stale desaturation | 7 | partial | LuthierKnob paint | knobs | none | |
| Circuit visualiser within 100 ms | 8 | yes | CircuitResponseView | col2/Easy | LiveDisplays | |
| Spectrum delta pane: two traces, stale after 30 s | 9 | partial | WorkshopPanel paintSpectrum | Workshop | WorkshopSpectrum.* (incl. theSummaryIsAnnouncedForScreenReaders) | Delta trace only; no 30 s stale text |
| Noise event strip 15 Hz, reduced-motion count | 10 | yes | NoiseEventStrip | CHARACTER | NoiseStrip.reducedMotionShowsAStaticCount... | Refreshes at 30 Hz (every other tick under CPU relief) |
| Footer voice count + CPU at 4 Hz, "-" when stale | 11 | partial | PluginEditor::paint | footer | none | No voice count |
| Scrolling data stream | 12 | yes | DataStreamDisplay | footer | DataStream.* | Was no |
| Snapshot strip on change | 13 | yes | SnapshotStrip::refresh | Live strip | — | |
| Setlist triptych | 14 | yes | SetlistTriptych | Live strip | Live tests | |
| Tap-tempo LED in header | 15 | partial | LiveStrip tap pad | Live strip | LiveTapTempo | Not in header |
| Kill pill from `kill_switch_active` param | 16 | partial | LiveStrip killButton / KillSwitch | Live strip | Live tests | Not a parameter |
| Illustration static cache + live layers | 17/25 | yes | GuitarRenderer, SceneCrossfade | Easy/Workshop | ReducedMotion.aGuitarChangeCrossfades..., GuitarIllustration fullRenderIsFastEnough | |
| Preset browser thumbnail (worker, 200 cache) | 18 | yes | UI/Guitar/GuitarThumbnails | preset browser | Thumbnails.workerRendersCachesAndEvictsAt200 | Was no |
| A/B compare highlight | 19 | yes | HeaderBar | header | — | |
| MIDI Learn button + target pulse at 1 Hz | 20 | partial | Widgets.cpp:697 (static outline on the learning control) | knobs | none | Static outline, not a 1 Hz pulse. Header button shows toggle state only. Was no |
| Practice loop LED + 4 Hz pulse | 21 | partial | PracticePanel Looper state strip | drawer | PracticeDrawer | |
| Feedback LED 30 Hz | 22 | yes | FeedbackLed::kRefreshHz | col3 | ModelGapsUi.theFeedbackLedDrainsAtThirtyHertz | |
| Session recorder buffer bar | 23 | partial | PracticePanel capacity text | drawer | PracticeGaps.* | Text, not a bar |
| Tune playhead 30 Hz | 24 | yes | TuneSectionStrip | TUNE | TunePanel | |
| Tests/Ui/Dataflow (latency, stale, reduced motion per element) | 26 | partial | — | — | Per-element tests exist (DataStream, NoiseStrip, VuMeter, FeedbackLed, NoteDots, PianoRoll) | No systematic suite |
| Diagnostics "Show data-flow overlay" | 27 | no | — | — | none | |

### gui-techniques-updates.md

**In this tree**, three things are new since the last audit:
- the bass-technique amounts (slap/pop/ghost/double thump, rest stroke, alternation) are on a CHARACTER SLAP group, not a TECHNIQUES sub-tab
- CHARACTER has a Right Hand group (RightHandGroup, fingerstyle attack)
- the tour can include a TECHNIQUES stop once the host reports that the tab exists

Nothing else is built here. The rest of the spec is **in progress on techniques** (`origin/claude/luthier-techniques`, not merged). That branch adds:
- TechniquesPanel with a SCRAPE/SLIDE/SLAP/MUTE/TAP/BEND/CASCADE rail
- the TechniquePillRow (Easy), TechniqueOverlay and TechniqueMirrors
- a RHYTHM Mute Row and the preset-browser chip
- the Tap/Bend/Mute/Cascade engines and 64 parameters
- a move of the MUTE code out of `Source/WIP`

Counts: **yes 2 / partial 4 / no 25**.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Additive only (no locations removed) | 0.1 | yes | — | — | — | |
| TECHNIQUES tab between CONTROLLERS and HELP | 0.2 / 1 | no | AdvancedPanel tabs (13) | no | none | In progress on techniques (TechniquesPanel, TechniquesUi.theTabAndItsRail) |
| Easy Techniques pill row | 0.3 / 2 | no | — | no | none | In progress on techniques (TechniquePillRow) |
| Vertical sub-tab rail | 0.4 / 1 / 10 | no | — | no | none | In progress on techniques |
| SCRAPE sub-tab | 1 | no | params scrape_* + ScrapeEngine (engine only) | no | Scrape tests (engine) | In progress on techniques (ScrapePage). No control in this tree |
| SLIDE sub-tab (slide-technique-controls 1) | 1 | partial | SlideGroup in CHARACTER | CHARACTER SLIDE | WorkshopPanel aSlideNeedsSlideMode | Source/range/scripting controls (and their params) in progress on techniques |
| SLAP sub-tab | 1 | partial | SlapGroup (bass amounts) in CHARACTER | CHARACTER (bass only) | BassTechniques.theSlapGroupIsShownOnlyOnABass | slap_armed/type/trigger/... (13 params) have no control: in progress on techniques (SlapPage). Was partial with no UI at all |
| MUTE sub-tab | 1 | no | WIP/UI/MuteGroup, WIP/Rhythm/Muting (not compiled) | no | WIP/Tests/MutingTests (not built) | In progress on techniques (moved out of WIP) |
| TAP sub-tab | 1 | no | — | no | none | In progress on techniques (TapEngine, TapPage) |
| BEND sub-tab | 1 | no | — | no | none | In progress on techniques (BendEngine, BendPage) |
| CASCADE overview grid + conflicts | 1 / 10 | no | — | no | none | In progress on techniques (CascadeResolver, CascadeView) |
| Arm pill per sub-tab | 1 | no | scrape_armed/slap_armed params only | no | none | In progress on techniques |
| Pill tap toggles arm | 2 | no | — | no | none | In progress on techniques |
| Pill hold -> popover, Esc closes | 2 | no | — | no | none | In progress on techniques |
| Pill right-click -> Advanced sub-tab | 2 | no | — | no | none | In progress on techniques |
| Pill armed fill + firing dot | 2 | no | — | no | none | In progress on techniques |
| Header unchanged | 3 | yes | HeaderBar | — | — | |
| Scrape trail overlay | 4 | no | — | — | none | In progress on techniques (TechniqueOverlay) |
| Tap markers overlay | 4 | no | — | — | none | In progress on techniques |
| Mute-zone shading | 4 | no | — | — | none | In progress on techniques |
| Bend arc + cents badge | 4 | no | — | — | none | In progress on techniques |
| Slap impact flash | 4 | no | — | — | none | In progress on techniques |
| Overlays toggleable within 2 ms | 4 | no | — | — | none | In progress on techniques (TechniquesUi.theOverlaysDrawInsideTheirBudget) |
| CHARACTER Right Hand > Tapping section | 5 | partial | RightHandGroup exists (CharacterPanel.cpp:323), no Tapping section | CHARACTER | RealismBUi tests | Tapping mirror in progress on techniques (TechniqueMirrors, at the foot of CHARACTER rather than inside the group) |
| CHARACTER PLAYING > Microtonal section | 5 | no | — | — | none | In progress on techniques |
| RHYTHM Mute Row | 6 | no | RhythmPanel | — | none | In progress on techniques |
| Preset browser "Uses Techniques" chip | 7 | no | PresetBrowserPanel | — | none | In progress on techniques |
| Onboarding tour stop at Techniques | 8 | partial | Onboarding::getTourSteps(includeTechniques), PluginEditorOnboarding.cpp:130 | — | Onboarding.theTourHasTwelveStopsInTheSpecsOrder | The hook exists. The stop appears only once the tab exists (techniques branch anchors `onboarding.techniques`) |
| Pill screen-reader labels, keyboard, non-colour state | 9 | no | — | — | none | In progress on techniques |
| Reduced motion makes overlays instant | 9 | no | — | — | none | In progress on techniques |
| Tests | 12 | no | — | — | none | In progress on techniques (TechniquesUiTests.cpp, 668 lines) |

### workshop-ui.md

All of these are now done:
- nut-slot drag
- pick, slide and capo drags and overlays
- per-string overrides with inspector fields
- narrow-width fallbacks (inspector collapses below 900, category box below 700)
- a screen-reader summary of the spectrum
- the audition body tint
- Ctrl-scroll zoom

Still missing:
- the fret-wear brush
- screw-handle height and tilt
- the buzz-heatmap overlay on the bench
- a keyboard path for the saddles
- inspector rows as real components
- the 30 ms return check

Counts: **yes 44 / partial 10 / no 0**.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Bench is not modal | 0.6 | yes | WorkshopPanel | Workshop | — | |
| Three feedbacks per interaction | 0.3 | partial | WorkshopBench, SpectrumDelta | Workshop | WorkshopBench aMovedPickupIsSeenReadAndHeard, WorkshopStrings.aPerStringOverrideIsSeenReadHeardAndOneEntry | Pickup and string override only; nut and accessories have no audible check |
| Layout: header, illustration, inspector, drawer, setup, spectrum | 1 | yes | WorkshopPanel::resized | Adv / Easy overlay | WorkshopPanel | |
| Header: name (modified), Save As Guitar, A/B | 1 | yes | WorkshopPanel | Workshop | — | |
| Below 900: inspector collapses to a drawer | 1 | yes | WorkshopPanel kWideBench | Workshop | WorkshopLayout.theBenchCollapsesItsInspectorAndDrawerWhenNarrow | Was no |
| Below 700: parts drawer becomes a dropdown | 1 | yes | kNarrowBench category box | Workshop | same | Was no |
| Easy overlay works down to window minimum | 1 | yes | WorkshopOverlay | Easy | WorkshopEditor | |
| Procedural render from GuitarSpec | 2 | yes | GuitarRenderer | Workshop | GuitarIllustration, Headstocks.* | |
| Ruler under pickup rail | 2 | yes | BenchIllustration paint | Workshop | WorkshopPanel aPickupDrag... | |
| Hit-region outlines on hover/selection | 2 / 3.1 | yes | GuitarRenderer overlay | Workshop | WorkshopPanel hoverDoesNotSelect | |
| Live overlays: pick, slide at slant, capo, buzz heatmap when setup focused | 2 | partial | GuitarOverlay pick*/slide*/capo*; BenchIllustration::currentOverlay | Workshop | WorkshopAccessories.* | Heatmap overlay missing. Was no |
| Repaint budget 8 ms full / 2 ms overlay | 2 | partial | cached scene + paintOverlay | — | GuitarIllustration fullRenderIsFastEnough | Overlay budget untested |
| Hover tooltip | 3.1 | yes | BenchIllustration | Workshop | Editor everyHitRegion... | |
| Hover doesn't select | 3.1 | yes | — | Workshop | WorkshopPanel hoverDoesNotSelect | |
| Click selects | 3.1 | yes | BenchIllustration::select | Workshop | — | |
| Alt-hover audition; delta; 30 ms back | 3.2 | yes | WorkshopPanel::hoverCard, setAuditionTint | drawer | WorkshopPanel auditionFromTheDrawerNeverCommits, WorkshopSpectrum.theBodyTintsWhileAuditioning... | 30 ms unmeasured |
| Per-string select shows set + override fields; colour | 3.3 | yes | PartLibrary StringOverride, inspector `#string.` fields, Ctrl-click | Workshop | WorkshopStrings.* | Was partial |
| Hit regions from drawing geometry | 4 | yes | GuitarScene hits | — | GuitarIllustration hitTestingFindsThePartOnTop | |
| Pickup drag, snaps, collision stop | 4 | yes | Drag::pickup | Workshop | WorkshopBench snap/collision tests | |
| Pickup height via screws or scroll | 4 | partial | BenchIllustration::mouseWheelMove (:863) | Workshop | WorkshopRanges.theTabCarriesAPadlockAndHeightsStopAtStock | Scroll only; stops at 0.8 mm stock |
| Pickup tilt by one screw | 4 | partial | Shift/Alt wheel | Workshop | — | |
| Bridge saddles drag ±6 mm | 4 | yes | Drag::saddle | Workshop | — | |
| Nut slots drag per string 0.05 mm | 4 | yes | Drag::nut, WorkshopBench::setNutSlotDepth | Workshop | WorkshopNut.aSlotDragIsOneEntryInRealUnitsAndReachesTheEngine | Was partial |
| Frets click select, brush wear | 4 | partial | select only | Workshop / CHARACTER FretWearMap | — | No brush |
| Pick drag along string, rotate at corner | 4 | yes | Drag::pick/pickRotate | Workshop | WorkshopAccessories.thePickIsDraggedAndTurnedOnTheBench | Was no |
| Slide drag + rotate slant | 4 | yes | Drag::slide/slideRotate | Workshop | WorkshopAccessories.theSlideTurnsOnTheBench... | Was no |
| Capo drag 0–12 | 4 | yes | Drag::capo | Workshop | WorkshopAccessories.theCapoIsDrawnAndDraggedByFrets | Was no |
| Live value follows pointer in inspector | 4 | yes | onPickupDragged | Workshop | WorkshopPanel | |
| Comb notches live | 4 / 6 | yes | SpectrumDelta::combNotches | Workshop | WorkshopSpectrum | |
| Inspector: name, origin, fields, compatibility | 5 | yes | refreshInspector | Workshop | WorkshopPanel theInspectorShows... | |
| Factory part edit -> user copy + Save as user part | 5 | yes | editInspectorField | Workshop | WorkshopPanel editingAFieldMakesAUserCopy | |
| Swap control | 5 | yes | swapButton | Workshop | — | |
| Revert control | 5 | yes | revertButton | Workshop | — | |
| Fields get tooltips/accessibility | 5 | partial | — | — | — | Rows are painted |
| Plain clamps from part-acoustics | 5 | yes | PartAcoustics | — | — | |
| Fixture render committed vs candidate | 6 | yes | SpectrumDelta | Workshop | WorkshopSpectrum | |
| Flat 0 dB shown plainly | 6 | yes | — | — | WorkshopSpectrum aNullChangeIsFlat | |
| ±12 dB axis, auto-zoom | 6 | yes | autoZoomToggle | Workshop | — | |
| Worker, 40 ms, coalesced | 6 | yes | SpectrumDelta | — | WorkshopSpectrum.hundredShadowRendersStayUnder40ms | |
| A/B 8 slots | 7 | yes | slotButtons, UiState::benchSlots | Workshop | Undo.benchSlotsAndSetlistAreInTheState... | |
| One undo entry per commit, real units | 8 | yes | WorkshopBench | — | WorkshopNut, WorkshopBench | |
| Empty user-parts text | 9 | yes | WorkshopPanel | drawer | — | |
| Slide category greyed with "Turn on Slide Mode (S)" | 9 | yes | WorkshopPanel | drawer | WorkshopPanel aSlideNeedsSlideMode | |
| Single-option part type still shown | 9 | yes | drawer | drawer | — | |
| Incompatible part warning, audition works | 9 | yes | paintDrawer | drawer | — | |
| Hit regions focusable, builder order | 10 | yes | builderOrder | Workshop | WorkshopPanel keyboardNudgesMatchADrag | Not separate accessibility children |
| Arrow nudge by snap; Shift fine | 10 | yes | BenchIllustration::keyPressed (pickup, height, nut, pick, slide, capo) | Workshop | WorkshopNut (keyboard parity) | |
| Spectrum announced as a sentence | 10 | yes | takeSpectrum postAnnouncement | — | WorkshopSpectrum.theSummaryIsAnnouncedForScreenReaders | Was partial |
| Everything by drag reachable by keyboard | 10 | partial | — | — | — | Saddles have no key path |
| Test: every part hit-testable | 11 | yes | — | — | WorkshopPanel everyFittedPartIsReachable... | |
| Test: hover does not select | 11 | yes | — | — | WorkshopPanel hoverDoesNotSelect | |
| Test: audition does not commit (+30 ms return) | 11 | partial | — | — | WorkshopBench auditionNeverCommits | No audio-return check |
| Test: drag one entry / snap / three feedbacks / null delta / 40 ms / A-B / keyboard parity | 11 | yes | — | — | WorkshopBench, WorkshopSpectrum, WorkshopPanel, WorkshopNut | |
| Test: nothing on audio thread during bench interaction | 11 | partial | — | — | ThreadProbe mapSpecCalls, Engine.fiveMinutesOfPlayback... | Not driven through bench gestures |

### proposals/visual-polish.md

The proposal is now almost fully built. This round added the VU meter, the room light, six accent choices plus follow-the-guitar, the display-face header name and the screenshot harness. Still open: focus-ring styling is only the palette outline colours (unverified), and there is no performance-budget test with every face visible. Counts: **yes 25 / partial 2 / no 0**.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Section 6 theme replaces theme.md | 0.1 | yes | UI/Theme.h Palette | all | Theme theDefaultIsTheGuitarShop | |
| 4.5:1 contrast on all palettes; High contrast untextured | 0.2 | yes | Palette::textured | all | Theme contrast tests, highContrastIsFlat | |
| Cached textures, no per-frame work | 0.3 | yes | Faces cached images | — | Faces facesRenderIdenticallyTwice | |
| No animation beyond live elements | 0.4 | yes | — | — | — | |
| Guitar key light + diffuse + edge highlight | 1 | yes | GuitarRenderer | Easy/Workshop | GuitarIllustration contactSheet | |
| Lacquer sheen by gloss | 1 | yes | GuitarRenderer | — | — | |
| Metal reflection + hot highlight | 1 | yes | GuitarRenderer metalFill | — | — | |
| Drop shadows | 1 | yes | GuitarRenderer shadow() | — | — | |
| Test: High contrast render has no gradient | 1 | yes | — | — | GuitarIllustration highContrastHasNoLighting | |
| Amp head face | 2 | yes | UI/Faces/AmpFace | Adv AMP, Easy | Faces, ModelGapsUi.standbyAndBypassReachTheFacesOnThePanels | |
| Pedal faces | 2 | yes | UI/Faces/PedalFace | racks | Faces | |
| Same knobs and params | 2 | yes | — | — | FacesIntegration | |
| Model-specific knob caps | 3 | yes | UI/Faces/KnobCaps | faces | Faces everyKnobCapRendersAndKeepsTheArc | |
| Tube glow follows drive; grey when stale | 4 | yes | AmpFace | Adv AMP | FacesIntegration | |
| Optional VU needle meter | 4 | yes | UI/StageTouches VuMeter (300 ms ballistics, stale grey), Appearance switch | Easy | StageTouches.theVuNeedleHasBallisticsAndGreysWhenStale | Was no |
| Room light | 4 | yes | UI/StageTouches RoomLight | Easy ROOM | StageTouches.theRoomLightFollowsSizeAndWet | Was no |
| Default/High contrast/Light palettes | 5 | yes | PaletteId | Options APPEARANCE | Theme / Accessibility | |
| User accent choice (brass + 5), each ≥4.5:1 | 5 | yes | AccessibilitySettings accentFor/accentContrast; AppearancePage accentBox | Options APPEARANCE | Accent.everyChoiceMeetsContrastOnEveryPalette | Was no |
| Follow-the-guitar accent | 5 | yes | kFollowGuitar; editor feeds finish.colourA | Options APPEARANCE | Accent.theWindowTakesTheAccentAndFollowsTheGuitar | Was no |
| Palette values | 6.1 | yes | Theme.h | — | Theme | |
| Light palette | 6.1 | yes | PaletteId::light | — | Screenshots | |
| Display heading + warm sans + tabular numbers | 6.2 | yes | Resources/Fonts, Fonts::display/mono | — | Theme theBundledFontsLoad | Header name now in the display face |
| Engraved-plate headers | 6.2 | yes | Palette plate | — | — | |
| Bell knob, mini toggles, brass fader, screws | 6.3 | yes | Theme | — | Theme controlsRender... | |
| Brass inlaid headstock mark | 6.4 | yes | Theme.cpp, HeaderBar | header | Screenshots | |
| Focus rings restyled and visible | 6.5 | partial | Theme.cpp:345/363 focusedOutlineColourId = accent | — | none | Only ComboBox/TextEditor outlines; knobs/buttons unverified |
| Tests: twice-identical, HC flat, contrast, pilot/LED, palettes, perf budget | 7 | partial | — | — | Faces, Theme, Accent, Screenshots.everyPanelInEveryPalette | No perf-budget test with every face visible |

### onboarding.md

Onboarding went from "almost entirely missing" to largely built:
- the welcome banner (3 offers, Don't ask again, upgrade line)
- the 12-stop tour from the banner and from Help
- first-week pulses, tab dots and the dice tooltip
- NEW dots (empty table)
- FirstRun OS defaults (Windows reads only)
- first-encounter hints for TUNE and Workshop
- Restore first-run experience
- drawer-state restore
- example tunes, MIDI clips and setlists
- the fresh preset ("Single-Cut Crunch" by the trademark decision)

Still open: backing tracks (deferred), the realism defaults, an explicit last-preset restore for the standalone, and macOS/Linux OS reads. Counts: **yes 33 / partial 4 / no 1**.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Great sound <30 s, no modal on first run | 0.1–0.2 | yes | applyFirstRunPreset; non-modal banner | — | Onboarding.aFreshInstallStartsWhereSectionOneSays | Was partial |
| First run does no network | 0.5 | yes | Telemetry/updates opt-in | — | Telemetry tests | |
| Fresh preset `Factory/Rock/Modern Overdrive` | 1 | yes | Onboarding::kFirstRunPreset = "Single-Cut Crunch" | — | Onboarding.aFreshInstallStarts... | Renamed by the trademark sweep (TUNE-HELP decision). Was no |
| Fresh guitar `Les Paul Standard` | 1 | yes | the preset's single-cut guitar | — | same | Trademark-free name |
| Easy, Live off, drawer collapsed, Workshop closed, Slide off, ranges stock | 1 | yes | UiState defaults | — | same | |
| Realism defaults: squeak 25%, player-friendly setup | 1 | partial | squeak_amount default 0.25 | — | — | Deferred: taken from the preset bank |
| Welcome banner (Yes / Maybe later x3 / Don't ask again) | 2 | yes | UI/Onboarding WelcomeBanner, PluginEditorOnboarding | under header | Onboarding.theWelcomeBannerOffersTheTourUpToThreeTimes | Was no |
| 12-step tour with Next/Back/Skip, Esc | 3 | yes | TourOverlay, Onboarding::getTourSteps | — | Onboarding.theTourHasTwelveStopsInTheSpecsOrder, theTourWalksEveryStopAtEverySize, skippingTheTourLeavesThePluginPlayable | Was no |
| Help -> Take the tour | 2 | yes | HelpTab | HELP | Onboarding.theBannerAndHelpBothStartTheTour | Was no |
| Post-tour discoverability (7 days) | 4 | yes | DiscoveryLayer, DiscoveryTooltip | window | Onboarding.theFirstWeekMarksTheWrench..., theRandomiseTooltipShowsOnTheFirstHoverOnly, theDiscoveryWeekIsSevenLaunchesOrSevenDays | Was no |
| First-run defaults: sidechain, recorder, updates, telemetry, crash, beta off | 5 | yes | defaults | — | — | |
| Reduced motion, high contrast, DPI >150% -> 125%, locale from OS | 5 | partial | UI/FirstRun, FirstRunOs.cpp | — | FirstRun.theOsPreferencesMapToSectionFivesDefaults, appliesOnceOnAFreshInstallAndNeverAgain | Contrast and motion are read on Windows only (macOS/Linux return false). No locale catalogs ship, so locale-follow is moot. Was partial (branch) |
| Range mode stock, Slide off, Live off | 5 | yes | defaults | — | — | |
| 36 factory presets | 6 | yes | FactoryPresets (36) | browser | Presets tests | |
| 12 factory guitars | 6 | yes | Resources/Guitars | Workshop | GuitarMigration | |
| 60+ factory parts | 6 | yes | Resources/Parts (148) | drawer | — | |
| 12 tune templates | 6 | partial | Resources/Tunes/Templates (10) | TUNE | TuneBuilder.theTenTemplatesLoadInOrderAndAreValid | Ten by DECISIONS C-16 |
| 6 example tunes | 6 | yes | Resources/Tunes/Examples (6), TuneExamples | TUNE New menu | SampleContent.theSixExampleTunesAreValidAndShipAsBuilt | Was no |
| 12 example MIDI clips | 6 | yes | Resources/Examples/*.mid (12) | import | SampleContent.theTwelveMidiClipsShipAndPlay | Was no |
| 6 backing tracks | 6 | no | — | — | none | Deferred (size) |
| 10 example setlists | 6 | yes | TuneExamples::installExampleSetlists (first run) | LIVE | SampleContent.theTenExampleSetlistsInstallOnce... | Was no |
| Tour as reusable walkthrough | 6 | yes | Help "Take the tour" | HELP | Onboarding.theBannerAndHelpBothStartTheTour | Was no |
| Advanced-range explainer, once, exact text | 7 | yes | RangesUi | knobs | FirstRun.theRangeExplainerSaysSectionSevensWords | |
| Re-trigger via Diagnostics Restore | 7 / 12 | yes | DiagnosticsPage restoreFirstRun | Options DIAGNOSTICS | FirstRun.theDiagnosticsPageRestores | Was partial (branch) |
| TUNE first-session hint | 8 | yes | FirstEncounterHint in TunePanel | TUNE | FirstEncounterHint.theTuneTabAndTheBenchCarryTheirHints | Was partial (branch) |
| Workshop first-session hint | 9 | yes | WorkshopPanel/WorkshopOverlay | Workshop | same | Was partial (branch) |
| Paths A/B/C documented | 10 | yes | Help "First Steps and the Tour", docs/USER_MANUAL.md | HELP | HelpLinks.otherWorkstreamsPanelsPinToTheirOwnTopics | Videos not audited |
| Restore last preset if closed clean | 11 | partial | host state restore | — | HostState tests | No standalone "last preset" memory outside host state |
| Restore window size, mode, tab | 11 | yes | UiState, UiPreferences | — | Editor | |
| Restore practice drawer state | 11 | yes | UiState::practiceDrawerOpen | — | ReturningUser.thePracticeDrawerComesBackAsItWasLeft | Was partial |
| Restore Slide Mode, last tune | 11 | yes | slide_guitar; tune in state | — | TuneIntegration.aRelaunchLoadsTheLastTuneAndPlaysIt | |
| Skip welcome unless armed; update banner | 11 | yes | Onboarding::getWelcomeDue | — | Onboarding.anUpgradeWelcomesOnce... | Was partial |
| Reset to first-run (modal, keeps libraries) | 12 | yes | FirstRun::restoreFirstRunExperience | Options DIAGNOSTICS | FirstRun.restoreClearsTheSettings...KeepsTheLibraries | Was partial |
| "What's new" upgrade banner | 13 | yes | WelcomeBanner upgrade kind | — | Onboarding.anUpgradeWelcomesOnceAndKeepsTheUsersData | Was no |
| NEW dot on new features | 13 | yes | NewFeatureDots / DiscoveryLayer newDot | tabs | NewDots.anEntryPointIsMarkedForItsFirstWeekOnly | Empty table for 1.0. Was no |
| Changelog one click away | 13 | yes | banner "What's new?" -> What's New topic | banner | — | Was partial |
| Forward-compatible formats; migrate + dated Backup | 13 | yes | PresetManager::backupMigratedOriginal, backupFolderFor | — | ModelGapsUi.aMigratedPresetKeepsItsOriginal, Presets.backupsGoToThePresetsRootBackupFolder | Was partial |
| Tests (fresh, tour, skip, OS, upgrade, restore, first-encounter once) | 14 | yes | — | — | Onboarding.*, FirstRun.*, FirstEncounterHint.*, ReturningUser.* | Was no |

### accessibility.md

The UI scale is now applied (`setScaleFactor`), and reduced motion is honoured by the illustration, note dots, slide bar, data stream, noise strip, chord name and discovery pulses. Every attached control gets a screen-reader name. The piano roll and the circuit view have handlers.

Unchanged:
- `configureMeter` and `announceOverlayOpened` are never called
- the fretboard and snapshot strip have no accessible children
- focus returns to the editor, not to the launcher
- no Tab order is defined or tested
- no scale-aware minimum or fallback
- no translation catalogs ship, and only about 15 `tr()` calls exist against about 300 literals

Counts: **yes 18 / partial 18 / no 11**.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Every control has an accessible label, role and value | 0.1 / 1 | partial | Widgets.cpp:181 labelForScreenReaders; AccessibleSetup::configure* | — | ScreenReader.everyAttachedControlHasAName | Painted widgets (SnapshotStrip, maps, inspector rows) still unnamed |
| Nothing conveyed by colour alone | 0.2 | partial | SnapshotStrip border; heatmap glyph | — | Accessibility colourblind tests | Mod arc and output LED are colour-only |
| Shortcuts rebindable + printable | 0.3 | yes | AccessibilitySettings | Options, HELP | Accessibility.shortcutsRebindAndRefuseClashes | |
| All UI strings in a catalog | 0.4 / 6 | partial | Localisation getBuiltInEnglish | — | Localisation catalogCoversTheUi | Most strings are literals |
| Font sizes scale 75–200% without breaking layout | 0.5 / 4 | partial | PluginEditor.cpp:42/563 setScaleFactor | Options APPEARANCE | Reflow.noControlHangs... (asserts the scale is applied) | Now applied; layout at each scale is not checked. Was no |
| AccessibleValueInterface on knobs etc. | 1 | yes | JUCE handlers | — | — | |
| Meters report peak dBFS | 1 | no | AccessibleSetup::configureMeter (Accessibility.cpp:846) is never called | — | none | |
| Fretboard per-fret child elements | 1 | no | FretboardComponent (no handler) | — | none | |
| Snapshot buttons expose names | 1 | no | SnapshotStrip painted | — | none | |
| Overlays announce on open, focus first element | 1 | partial | OverlayHost::show grabs focus | — | none | announceOverlayOpened never called |
| Escape returns focus to launcher | 1 | partial | OverlayHost::dismiss -> parent focus | — | Editor overlay tests | Focus goes to the editor |
| NVDA/VoiceOver/Orca regression | 1 / 10 | no | — | — | none | |
| Tab/Shift-Tab order | 2 | partial | JUCE default; bench builderOrder; PianoRollStrip focusable | — | VisualAids.everyControlIsReachableFocusableAndNamed (piano roll only) | No order for the main window |
| Arrows adjust (Shift fine, Ctrl coarse) | 2 | partial | JUCE slider keys; Widgets.cpp:773 (mouse modifiers) | — | none | Keyboard step sizes not modifier-aware |
| Enter opens dropdowns / confirms | 2 | yes | JUCE default | — | — | |
| Esc cancels / dismisses | 2 | yes | PluginEditor::keyPressed | — | Editor | |
| F1 context help | 2 | yes | getHelpContext | — | HelpTab | |
| All actions by shortcut | 2 | partial | registry (now incl. W, Ctrl+T, Ctrl-Alt-Z, Ctrl-Y) | — | Accessibility.shortcutDefaultsMatchTheCanonicalTable | Ctrl+E not context-aware; Space is audition only |
| Show-all-shortcuts overlay with search | 2 | yes | AccessibilityPage searchBox | Options | HelpTab theSearchFiltersTheSheet | |
| Palettes: Default, Deut, Prot, Trit, HC, Light | 3 | yes | PaletteId | Options APPEARANCE | Accessibility palette tests | |
| Palettes ship as Resources/Themes/*.json | 3 | partial | JSON round trip; built-in | — | Accessibility palettesRoundTripThroughJson | No Resources/Themes |
| Meters use shape | 3 | no | LevelMeter | — | none | |
| Scale steps 75–200 | 4 | yes | AccessibilitySettings::kScales | Options | uiScaleStepsAndFontFloor | Applied now |
| Min 10 px font at 100% | 4 | yes | font floor | — | uiScaleStepsAndFontFloor | |
| Reflow when scale increases | 4 | partial | whole-window transform | — | Reflow.* | The window scales as a unit; nothing reflows |
| Window minimum grows with scale; fallback + warn once | 4 | no | — | — | none | |
| Reduced-motion toggle | 5 | yes | AppearancePage reducedMotionToggle | Options | Accessibility reducedMotionRemovesAnimation | |
| Reduced motion disables data stream, 80 ms ease, pulses | 5 | yes | DataStreamDisplay, FretboardComponent, NoiseEventStrip, IllustrationMotion, ChordNameFader, DiscoveryLayer | — | DataStream.*, NoiseStrip.*, ReducedMotion.*, ChordName.reducedMotionHasNoFades | Was partial |
| Catalog `Resources/i18n/<locale>.json` | 6 | partial | Localisation::loadCatalog | — | Localisation tests | No Resources/i18n; English only |
| 15 ship locales offered | 6 | yes | Localisation::getShipLocales | Options LOCALIZATION | Localisation everyShipLocaleIsOffered | No catalogs, so a switch is refused |
| Locale switch without restart | 6 | partial | LocalizationPage | Options | — | No refresh broadcast |
| Missing key -> en; log | 6 | yes | Localisation fallback | — | Localisation | |
| Named placeholders; no concatenation | 6 | partial | tr (key, {{name}}) | — | Localisation | Heavy concatenation in UI |
| RTL-safe layout + bidi test | 6 | no | — | — | none | |
| Help in current locale | 7 | partial | HelpContent (English) | HELP | HelpTab | |
| Shortcut panel is live view | 7 | yes | HelpTab | HELP | HelpTab aRebindShowsUpInTheCheatSheet | |
| Honour system fonts | 8 | partial | fontOverride | Options | — | |
| Numeric readouts in tabular mono | 8 | yes | Fonts::mono | — | — | |
| CJK/Arabic fallback font stack | 8 | partial | LocaleInfo needsCjk | — | none | |
| Options > Accessibility page | 9 | partial | AccessibilityPage | Options | Editor everyOptionsPage... | Scale/palette/motion on APPEARANCE |
| Options > Localization page | 9 | yes | LocalizationPage | Options | — | |
| Test: NVDA smoke | 10 | no | — | — | none | |
| Test: keyboard Tab walk | 10 | no | — | — | none | |
| Test: contrast on 3 palettes; no clipping | 10 | yes | — | — | Theme / Accessibility / Accent contrast tests, Screenshots | |
| Test: every locale renders every panel at 100/150% | 10 | no | — | — | none | |
| Test: reduced motion no animation frames | 10 | yes | — | — | ChordName.reducedMotionHasNoFades, ReducedMotion.*, DataStream.*, NoiseStrip.* | Was partial |
| Test: CJK fallback no tofu | 10 | no | — | — | none | |

### piano-roll-chord-display.md

New spec (added 2026-09-24), not in the previous report. It is built and wired end to end:
- `SoundingNotesPublisher::publish` is called every slice from PluginProcessor.cpp:1505
- PianoRollStrip is in AdvancedPanel (under the fretboard in the top guitar strip) and in EasyPanel (56 px)
- keys play through the processor's preview MIDI
- Show fingering uses a private RubricVoicer
- ChordNameOverlay/ChordNameFader draw in GuitarBodyComponent's live pass
- Options -> Appearance (the page with "Show tooltips") has a Visual aids section

Every PR/CD/OP test in §8 has a matching test. Gaps: the Advanced placement spans the top strip rather than "column 2", the computer-keyboard path is untested, and the 2 ms overlay budget is not measured. Counts: **yes 29 / partial 3 / no 0**.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Display-only reads never allocate/lock on audio thread | 0.1 | yes | Support/SoundingNotes (double buffer, atomic seq) | — | PianoRoll.publishingTheSnapshotDoesNotAllocate (PR-07) | |
| Playing from keys uses the on-screen keyboard's MIDI path | 0.1 / 3 | yes | PluginProcessor::playKeyboardNote/releaseKeyboardNote/playKeyboardChord -> previewMidi (ch 1) | roll keys | PianoRoll.clickingAKeyPlaysTheGuitar (PR-03) | Message side takes `previewLock`; audio side try-locks (per VWQ doc) |
| What sounds is what shows (engine string activity, incl. voicer/rhythm notes) | 0.2 / 2 | yes | SoundingNotesPublisher::publish (PluginProcessor.cpp:1505) | roll | PianoRoll.aChordLightsItsSoundingKeysInTheStringsColours (PR-01) | |
| Toggles live in Options next to tooltips, per user | 0.3 / 5 | yes | UI/VisualAids (UiPreferences `visualAids.*`); OptionsPages.cpp:845-860 | Options APPEARANCE | VisualAids.theOptionsPersistAndStayOutOfPresets (OP-01) | "General" page is the Appearance page here |
| Keyboard 18–28 px + roll 40–80 px, 4 s scrolling | 1 | yes | PianoRollModel, PianoRollStrip | Easy/Adv | PR-01/PR-02 | |
| Range follows tuning/capo, padded to octaves; out-of-range keys dimmed but work | 1 | yes | PianoRollModel::rangeFor | roll | PianoRoll.theRangeFollowsTuningAndCapo (PR-06) | |
| Key colour = string colour at 85%, 150 ms release fade; bar at 60% | 1 | yes | PianoRollModel, StringColours::forString | roll | PR-01, PR-02 | |
| Octave labels, middle C marked | 1 | yes | PianoRollStrip paint | roll | — | Not asserted by a test |
| Advanced: collapsible strip under the fretboard in column 2, 72 px, drag 40–140, state in UiState | 1 | partial | AdvancedPanel.cpp:304/1340 (top guitar strip, full width), PianoRollStrip collapseButton + handle, UiState pianoRollExpanded/pianoRollHeight | Adv | VisualAids.everyControlIsReachableFocusableAndNamed | Sits under the fretboard across the window, not in column 2 |
| Easy: 56 px under the guitar | 1 | yes | EasyPanel.cpp:645 PianoRollStrip::kEasyHeight | Easy | OP-02 | |
| Strip header: ROLL/KEYS, Latch, Show fingering | 1 | yes | PianoRollStrip (+ Play, Clear) | roll | OP-02 | |
| SoundingNotes snapshot: 128-bit set + per-string note/start; double-buffered; audio only stores | 2 | yes | Support/SoundingNotes | — | PR-07 | Also packs bend cents |
| UI drained at 30 Hz | 2 | yes | PianoRollStrip timer | — | PR-01 (within 2 frames) | |
| Stale after 250 ms -> no keys lit | 2 | yes | PianoRollModel | — | PR-02 | |
| Slide/bend moves the key at the semitone; bend tick >20 cents | 2 | yes | SoundingNotesPublisher (nearest semitone, cents), PianoRollModel | roll | — | No dedicated bend test |
| Click/drag glissando; velocity 40–110 by position | 3 | yes | PianoRollStrip mouse handlers | roll | PR-03 | Glissando not tested separately |
| Latch: toggle held set, Play/Enter sends one strummed chord; Clear/Escape | 3 | yes | PianoRollStrip latch, playKeyboardChord | roll | PianoRoll.latchedKeysPlayAsOneStrummedChord (PR-04) | Sent as simultaneous note-ons; the engine/rhythm engine strums them |
| Show fingering: RubricVoicer ghost dots before playing; x + "below this guitar's range" | 3 | yes | PianoRollStrip voicer; FretboardComponent/GuitarBodyComponent setGhostDots | fretboard | PianoRoll.showFingeringDrawsTheVoicersShapeBeforeItSounds (PR-05) | |
| Computer keyboard (A–L, W–P, Z/X) only while focused | 3 | yes | PianoRollStrip::keyPressed (:511) | roll (focused) | none | Untested (no key-state source in the harness) |
| Chord naming: 1 pc note name, spelling from tune key else sharps except Bb/Eb | 4 | yes | UI/ChordNaming, ChordNameOverlay.cpp:20 (tune preferFlats) | guitar | ChordName.singleNotesPowerChordsAndChordsAreNamed (CD-01) | |
| Two pcs: power chord "E5" else both notes; 3+: symbol, below confidence floor -> pcs | 4 | yes | ChordNaming.cpp:74 (kConfidenceFloor) | guitar | CD-01 | |
| Placement: lower bout, display face, 12% height clamped 28–96, text colour, 0.35 peak | 4 | yes | ChordNameOverlay::lowerBout, GuitarBodyComponent.cpp:252 | Easy guitar | ChordName.sizedFromTheIllustrationAndAnnouncedPolitely | Drawn wherever GuitarBodyComponent is, which is Easy only |
| Timing: 60 ms in, 30 ms burst merge, hold ≤1.2 s, 0.8 s out, 60 ms crossfade, legato in place | 4 | yes | ChordNameFader | guitar | ChordName.fadesInHoldsAndFadesOut (CD-02), ChordName.aStrumIsOneName (CD-03) | |
| Reduced motion: no fades | 4 | yes | ChordNameFader | — | ChordName.reducedMotionHasNoFades (CD-05) | |
| Screen reader: polite announcement, ≤1 per 1.5 s, option-gated | 4 | yes | ChordNameOverlay postAnnouncement | — | ChordName.sizedFromTheIllustrationAndAnnouncedPolitely | |
| Options: chord names (on), announce (off, enabled only with names), roll per mode, Keys/Keys+Roll | 5 | yes | OptionsPages chordNamesToggle/announceChordsToggle/pianoRollAdvancedToggle/pianoRollEasyToggle/pianoRollShowsBox | Options APPEARANCE | OP-01 | |
| Not preset data, not parameters | 5 | yes | UiPreferences | — | OP-01 | |
| UiState pianoRollExpanded/Height/Latch/ShowFingering; latched set in session state, not presets; no undo | 6 | yes | UiState (+ pianoLatchedNotes) in the `ui` session block | — | OP-01 | |
| Audio publish ≤12 stores + 1 increment | 7 | partial | SoundingNotesPublisher | — | PR-07 (no-allocation only) | 14 relaxed stores + 1 release store (VWQ doc), a little over the spec's 12 |
| Roll repaints dirty region at 30 Hz within 2 ms at 1920x1080 | 7 | partial | PianoRollStrip | — | none | Budget not measured |
| Chord name drawn in live overlay pass, not cached scene | 7 | yes | GuitarBodyComponent live pass | — | — | |
| Tests PR-01..07, CD-01..05, OP-01..02 | 8 | yes | Tests/PianoRollTests.cpp | — | 15 tests, one per ID | CD-04: ChordName.offMeansNoDrawingAndNoDetection |

### Top gaps (group B)

Ranked by user impact.

1. **33 automatable parameters still have no control**, so `GuiReach.everyAutomatableParameterHasAVisibleControl` fails.
   - 27 are in progress on techniques: `scrape_*` (14) and the `slap_*` arming/trigger set (13).
   - `macro_assign_a/b` (Macro 7/8) have no control.
   - `pickup_blend` has no control.
   - `tune_feel_mod` and `tune_tempo_drift` are mod-only by design but are not listed in `intentionallyHidden`.
   - `string_age` is superseded by `string_age_hours` but is not listed as hidden.
   - The RHYTHM tab also still ignores `RhythmPanel::preferredHeight()`, so the STRUM group's lower rows can collapse (beta report B-11).
2. **The TECHNIQUES tab, Easy pills, technique overlays, Mute Row, preset chip and the TAP/BEND/MUTE/CASCADE engines exist only on `claude/luthier-techniques`.** Until it merges, MUTE is uncompiled WIP, and TAP and BEND do not exist in this tree.
3. **The window contract is wrong.** The minimum is 940x560, the cap 2400x1440, and the aspect ratio is locked; the spec says 1280x800 to 2560x1600. The UI scale is a whole-window transform with no reflow, no scale-aware minimum and no fallback-with-notice.
4. **The header is still missing** the snapshot strip, input/output meters, tap, and the gear/dice/reset icons. It shrinks below 1280 but has no numeric snapshot or 3-dot overflow. Snapshots are only visible in Live Mode.
5. **Localisation is English-only.** No `Resources/i18n` catalogs ship, so choosing a locale is refused. There are about 15 `tr()` calls against about 300 literal UI strings, and no refresh broadcast.
6. **Modulation UX is incomplete.** The mod arc is one colour (no per-source colour or segments), the MOD tab's macro card has no knobs, macros 7/8 have no control, and "Assign to macro" is missing from right-click.
7. **Screen-reader gaps.** `configureMeter` and `announceOverlayOpened` are never called. The fretboard and snapshot strip have no accessible children, focus does not return to the launcher, and there is no defined or tested Tab order.
8. **Only 2 of the 7 §14 empty-state texts are displayed.** The bass text is a constant that is never shown, and the snapshot, mod-route, setlist and backing-track hints are missing.
9. **Panel chrome is incomplete.** There is no collapse chevron, status line or per-preset collapse memory. The panel right-click menu has only "Docs" (no collapse, reset panel or screenshot).
10. **Missing surfaces:** the CHARACTER → CIRCUIT mirror, the CONTROLLERS CC-map editor and multi-controller merge display, the guided-build rail, right-click part → parts drawer, the Col 1 library with "Open in Workshop", and "Edit in Workshop" in PICKUPS.
11. **Data-flow gaps.** There are no header meters, no header tap LED, no kill parameter, no pickup pulse, no footer voice count or status line, no stale state on the LED/chord readout/slide bar, no 1 Hz MIDI Learn pulse, no `Tests/Ui/Dataflow` suite, and no data-flow debug overlay.
12. **Workshop remainder.** There is no fret-wear brush, no screw-handle height or tilt, no buzz-heatmap overlay on the bench, and no saddle keyboard path. Inspector rows are painted rather than being accessible components.
13. **Shortcuts.** Ctrl+E always opens audio export (it is not context-aware). Space is audition, not tune play/pause, unless TUNE has focus. Kill toggles instead of holding. Live Mode does not suppress tooltips.
14. **Snapshot semantics.** There is no Shift-click write: a click on an empty slot captures, and rename/colour on the strip is right-click only.
15. **Undo depth is 200, not 64** (the grouping and boundaries are now correct). The clamp-on-preset-save banner is missing.
16. **Col 1/2/3 details.** The whammy section shows on a hardtail. There is no per-pickup phase and no amp sag/bias. A stale comment at AdvancedPanel.cpp:385 says "the Workshop does not exist".
17. **Onboarding remainder.** Backing tracks are deferred, the realism defaults are not applied, and the OS reads (contrast, motion) work only on Windows.

### Unspecified gaps noticed

- **Two NEW-dot mechanisms.** `NewFeatureDots` (PluginEditor.cpp:45/580, Theme.cpp:935) and `Onboarding::getNewFeatures`/DiscoveryLayer newDot both exist, both with empty tables. A future entry could be added to one and not the other.
- **The chord name is shown only in Easy mode.** Advanced has no GuitarBodyComponent, so a guitarist in Advanced never sees it; the fretboard there could carry it.
- **No preset favourites, rating or search** in the preset browser (the thumbnails are new). The "Uses Techniques" chip is on techniques.
- **No layout presets** (S/M/L window sizes or fit-to-screen). The aspect ratio is locked.
- **No visible CPU/quality mode switch in Easy.** The CPU-relief banner and Diagnostics opt-out exist, but cpu-quality-modes.md (a new spec) has no Easy control yet.
- **No tuner display.**
- **No user guitar/part library manager** (rename, delete, import/export) outside the Workshop drawer.
- **The `ContentPackage` updater has no UI entry point** (VWQ doc, item 49).

### Small glue candidates

| Param IDs / feature | Put it on | Notes |
|---|---|---|
| `pickup_blend` | Adv Col 2 PICKUPS next to Selector; Easy pickup popover | Also check `PickupEngine::setBlend` is used in process (beta report B-11 says it is not) |
| `macro_assign_a`, `macro_assign_b` (+ macros 1–6 knobs) | MOD tab: give the `SourceKind::plain` macro card eight LuthierKnobs | ModMatrixPanel.cpp:10–21 already classifies the macros |
| `tune_feel_mod`, `tune_tempo_drift`, `string_age` | Add to `intentionallyHidden()` in GuiReachabilityTests.cpp with reasons (mod-only; superseded by `string_age_hours`), or add small controls to TUNE / Col 1 STRINGS | Makes GuiReach honest |
| RHYTHM tab height | AdvancedPanel.cpp:1468 layout: add `else if (auto* p = dynamic_cast<RhythmPanel*> (panel)) height = jmax (visible, p->preferredHeight());` | Fixes the hidden STRUM rows (B-11) |
| `scrape_*`, `slap_armed` and the slap trigger set | Merge `origin/claude/luthier-techniques` (ScrapePage, SlapPage, pills) | Done on the branch |
| SlapGroup empty-state text | Show `SlapGroup::kInactiveMessage` as a one-line label when not a bass, rather than height 0 | The constant exists and is tested; gui-integration 14 wants it shown |
| Empty-state hints | LiveStrip empty slot tooltip/text; ModMatrixPanel route table when empty; LivePanel setlistEmptyLabel wording; PracticePanel backing-track drop text | Four string changes plus labels |
| Meter accessibility | Call `AccessibleSetup::configureMeter` on OutputLed / LevelMeter / VuMeter / AuxStrip | Helper exists, unused |
| Overlay announcement | Call `AccessibleSetup::announceOverlayOpened` in `OverlayHost::show` | Helper exists, unused |
| Focus return to launcher | Remember `Component::getCurrentlyFocusedComponent()` in `OverlayHost::show`, restore in `dismiss` | Replaces `parent->grabKeyboardFocus()` |
| CHARACTER → CIRCUIT mirror | CharacterPanel: a second `CircuitResponseView` + `guitar_volume`/`guitar_tone` knobs | Pieces exist |
| "Open in Workshop" / "Edit in Workshop" | Col 1 GUITAR section, Col 2 PICKUPS: button -> `setWorkspaceTabNamed ("WORKSHOP")` | Also fix the stale comment at AdvancedPanel.cpp:385 |
| Header snapshot strip / tap | Reuse `SnapshotStrip` and the Live strip tap pad in HeaderBar | Components exist |
| Whammy section on hardtail | AdvancedPanel Bridge section: hide the whammy knobs when `!WhammyPopover::isWhammyFitted` | The same test Easy uses |
| Live Mode tooltips | PluginEditor.cpp:603: also disable when `liveMode` is on | One condition |
| Ctrl+E context-aware | PluginEditor.cpp:659: route to TuneExportDialog when the TUNE tab is showing, NotationPanel export on NOTATION | Both dialogs exist |
| Unify NEW dots | Drop one of NewFeatureDots / Onboarding::getNewFeatures | Both tables are empty |
| Undo depth | `UndoHistory` cap 200 -> 64 (or amend the spec) | One constant |

---

## Group C: illustration, Workshop, parts, ranges, tune

This audit was checked against `HEAD` = `961cd55` on `claude/luthier-audit`. The helper branches realism-a, realism-b, realism-c, model-gaps, tune-help, visual, release and review are all merged into that tree. Only `origin/claude/luthier-techniques` is still unmerged; where it matters, a row says "in progress on techniques". The previous audit was taken at `91f946f`. None of the five spec files in this group has changed since then (`git diff 91f946f..HEAD -- spec/<file>` is empty), so no rows were added. Group C's spec list has no new files: piano-roll-chord-display.md, licensing.md and editions.md belong to other groups.

The techniques branch touches this group in one file only, `PhysicalRange.cpp`. It adds rows for `slideSpeedLimit` (slide), `tapMaxConcurrent` (pick) and `bendGlobalRange` (the first **modulation** family row). It edits `return table[index]` and `constexpr int kNumEntries = 32 + 3`, and HEAD has since replaced both with `allEntries(count)` and a counted `kNumEntries`. **Merging techniques will therefore conflict in `PhysicalRange.cpp`.** The fix is to drop the hard-coded count and keep the three rows.

Places things live:
- **WS tab**: Advanced mode, column 4, WORKSHOP tab.
- **WS overlay**: Easy mode's wrench, which opens `WorkshopOverlay`.
- **Illus**: `GuitarBodyComponent`.
- **TUNE**: the column 4 TUNE tab.
- **Opt→Ranges**: `OptionsPages.cpp:RangesPage`.
- **Opt→Diag**: Options → Diagnostics (`AudioPathView`).

---

### guitar-illustration.md

**Summary:** 101 requirements. **yes 60 / partial 30 / no 11** (previously 46 / 43 / 12).
- **Now done:** everything that was in progress on `visual` is merged and verified. That covers:
  - the thumbnail worker and 200-entry LRU, used by the preset browser in `Overlays.h:324`
  - the 250 ms `SceneCrossfade`
  - `NoteDots`
  - per-string override drawing
  - pick, slide and capo overlays and drags
  - nut-slot drag
  - the audition tint
  - family amp defaults
  - reduced motion
  - Shift+click to reach the part below
- **Still open:**
  - **Ground rule 5.** None of the 20 body parts has `illustration.body_style` and none of the 15 necks has `illustration.headstock`, so a body swap still never changes the outline, and 14 of the 34 outlines are still unreachable.
  - **Finish and hardware colour.** There is still no editor.
  - **Drawer.** There is still no drag-and-drop from it.
  - **Overlays.** There is still no buzz heatmap or pickup pulse.
  - **Families.** There is still no `extended` family.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Procedurally drawn; same renderer for Easy, Workshop and thumbnails | 0.1 | yes | UI/Guitar/GuitarRenderer.cpp:build; GuitarThumbnails::render | Illus; WS; preset browser | GuitarIllustration.*; Thumbnails.everyFactoryPresetShowsItsGuitar | Thumbnails now merged |
| Flat visual language (single gradient, no textures) | 0.2 | partial | GuitarRenderer:buildLighting | Illus | GuitarIllustration.highContrastHasNoLighting | Deliberate deviation (DECISIONS "Guitar illustration materials") |
| Every part is a first-class visual (swap → redraw) | 0.3 | partial | resolveStyle (GuitarRenderer.cpp:446); buildHeadstock (:2295) | WS | theKeyChangesWithEveryVisibleChange | Code reads `illustration.body_style`/`headstock`, but 0/20 bodies and 0/15 necks set them; inlay follows body style, not the fretboard part |
| Family switch fully rebuilds | 0.4 | yes | PartLibrary::switchFamily; Processor::switchGuitarFamily | WS drawer "Guitar" | aFamilySwitchGivesTheTargetFamilysGuitar | |
| Illustration authoritative to the ear | 0.5 | partial | resolveStyle vs mapSpec | — | — | Body outline and body part can still disagree |
| Three feedbacks (visual, inspector, audible) | 0.6 | yes | WorkshopPanel inspector + SpectrumDelta + audition | WS | WorkshopBench.aMovedPickupIsSeenReadAndHeard | Spectrum is also announced (WorkshopSpectrum.theSummaryIsAnnounced…) |
| Full repaint <8 ms, overlay <2 ms | 0.7 | partial | — | — | fullRenderIsFastEnough (120 ms bound) | Spec budget not asserted |
| mm coords, origin at saddle | 1 | yes | GuitarScene; SceneBuilder | — | — | |
| Fit zoom shows whole guitar | 1 | yes | GuitarRenderer::fitTransform | Illus/WS | everyFactoryGuitarRendersWithoutClipping | |
| Ctrl-scroll zoom to 4x with pan | 1 | yes | BenchIllustration::mouseWheelMove; Drag::pan | WS | BenchZoom.ctrlScrollZoomsToFourTimesAboutThePointer | |
| Static scene cached by spec hash | 2.1 | yes | GuitarRenderer::keyFor | — | theKeyChangesWithEveryVisibleChange | |
| Live overlays from display FIFO each frame | 2.2 | partial | GuitarBodyComponent::updateLiveOverlay | Illus | NoteDots.theDotAppearsWithin60ms… | Still polls `engine.getStringLevel`/tuning engine, not a FIFO. `SoundingNotes` exists but feeds only the piano roll and chord name |
| Hover/selection outlines | 2.2 | yes | GuitarRenderer::paintOverlay | Illus/WS | WorkshopPanel.hoverDoesNotSelect | |
| Drag ghost for part cards | 2.2 / 5.32 | no | — | no | — | No DragAndDropContainer anywhere in UI/ |
| Preset-browser thumbnails on worker, hash cache | 2.3 / 15 | yes | UI/Guitar/GuitarThumbnails (worker, LRU 200); Overlays.h:324 | preset browser | Thumbnails.workerRendersCachesAndEvictsAt200 | 256x128, not the 128x256 the spec gives |
| Families electric/acoustic/classical/bass/resonator | 3 | yes | PartLibrary::getFamilyTemplate | WS "Guitar" | aFamilySwitch… | |
| `extended` family (7/8, fanned) | 3 | no | — | no | — | getFamilyTemplate has no "extended"; FamilyDefaults.cpp:40 comment mentions it but maps nothing |
| Incompatible parts → family defaults + banner | 3 / 12.1 | yes | PartLibrary::switchFamily | WS | aFamilySwitch… | |
| Electric bodies (12 styles) | 4.1 | partial | BodyOutlines.h | by guitar only | everyFactoryGuitarHasItsParts | flying_v, reverse_firebird, multiscale unreachable |
| Acoustic bodies (8, 12-string, Selmer) | 4.2 | partial | BodyOutlines.h | by guitar only | — | om, 000, GA-cutaway, grande-bouche unreachable |
| Classical / flamenca / cutaway classical | 4.3 | partial | BodyOutlines.h | by guitar only | — | Flamenca guitar draws the classical outline |
| Bass bodies (8) | 4.4 | partial | BodyOutlines.h | by guitar only | — | P-Style Bass draws bass_offset; 4 styles unreachable |
| Resonator steel/wood/square-neck | 4.5 | partial | BodyOutlines.h | by guitar only | — | Square-neck unreachable |
| Body part carries outline + attachment points | 4 / 18 | partial | Part::illustration read in resolveStyle; custom `outline` (:596) | — | — | Resources/Parts unchanged since the last audit: 0/20 set it |
| Z-order layers 1-25 | 5 | yes | SceneBuilder::build* | Illus | everyFactoryGuitarHasItsParts | |
| Grain per wood | 5.6 | yes | buildGrain | Illus | — | |
| Pickguard tortoise / pearloid swirl | 5.7 | partial | buildPickguard | Illus | — | No pearloid |
| Bracing shadow through soundhole | 5.9 | yes | SceneBuilder | Illus | — | |
| Played-notes overlay (60 ms dots) | 5.26 | yes | IllustrationMotion.h:NoteDots | Illus | NoteDots.theDotAppearsWithin60msAndFadesOver60ms | |
| Buzz heatmap overlay | 5.27 | no | — | no | — | Not on any branch |
| Slide bar overlay | 5.28 | yes | GuitarRenderer::slidePath; paintOverlay | Illus/WS | WorkshopAccessories.theSlideTurnsOnTheBench… | Slant + material colour |
| Pick overlay | 5.29 | yes | GuitarRenderer::pickPath (:3023); WorkshopPanel currentOverlay | WS | WorkshopAccessories.thePickIsDraggedAndTurnedOnTheBench | Only on the bench: Illus leaves `pickPositionMm` < 0 |
| Pickup pulse layer | 5.30 | no | — | no | — | |
| Bolt/set/through neck | 6 | yes | buildNeck | Illus | — | |
| Fretboard radius shading | 6 | partial | GuitarRenderer.cpp:2163 radius_mm | Illus | — | No compound gradient |
| Fretboard woods + binding | 6 | yes | woodColour; buildBinding | Illus | — | |
| Inlays dot/block/trapezoid/sharktooth/vine | 6 | partial | buildNeck | Illus | — | Chosen by body style; no vine |
| Headstocks incl. reverse | 6 | partial | HeadstockOutlines.h (reverse inline-6 added) | Illus | Headstocks.everyLayoutIsDrawable… | Neck parts still carry no headstock id |
| Truss cover + "L" | 6 | yes | buildHeadstock | Illus | — | |
| Bridges (TOM, trem, Floyd, wrap, Bigsby) | 7 | yes | buildBridge | Illus | — | |
| Hardtail ferrules on back | 7 | no | — | — | — | Back not drawn |
| Pin / pinless / floating / moustache / tie-block | 7 | partial | pinBridge, floatingBridge, tieBlock | Illus | — | No pinless |
| Resonator biscuit / spider | 7 | yes | buildBridge | Illus | — | |
| Bass vintage / high-mass / mutes | 7 | partial | bassBridge | Illus | — | No mutes |
| Pickup draw variants | 8 | yes | SceneBuilder::pickup | Illus | — | |
| Piezo indicator; soundhole magnetic | 8 | partial | pickup() (GuitarRenderer.cpp:1895) | Illus | — | Piezo indicator unclear |
| `pole_spacing_mm` drives pole spacing | 8 | no | — | — | — | Field never read |
| Cover colour and rings | 8 | yes | pickup() | Illus | — | |
| Pickguard colours (9) | 9 | partial | pickguardColour | **no** | — | Still no Pickguard drawer category |
| Per-body default pickguard | 9 | yes | BodyStyle.pickguard | Illus | — | |
| String colours per material | 10 | yes | stringColour | Illus | stringColoursFollowSection10 | |
| String counts | 10 | yes | buildStrings | Illus | everyFactoryGuitarHasItsParts | |
| Per-string material override rendered | 10 | yes | PartLibrary StringOverride; GuitarRenderer per-string colour | WS (Ctrl-click string; inspector `#string.`) | WorkshopStrings.anOverriddenStringIsDrawnInItsOwnMaterial | Merged from visual |
| Finish fields | 11 | yes | GuitarFinish; round trip | **no** | — | Still no finish editor (only the Appearance accent *reads* finish.colourA) |
| Named solid palette (18) | 11.1 | no | — | no | — | |
| Bursts (named) | 11.2 | partial | finish "burst" | no | — | Named bursts only via hex |
| Transparent | 11.3 | yes | finish "transparent" | no | — | |
| Natural | 11.4 | yes | — | no | — | |
| Metallic (one stroke) | 11.5 | partial | finish "metallic" | no | — | Gradient rather than a stroke |
| Sparkle 3% | 11.6 | partial | finish "sparkle" | no | — | 30-35% alpha |
| Aging | 11.7 | yes | buildAging | no | agingIsSeededAndStable | No belt-buckle back wear |
| Hardware colour table | 11.8 | yes | GuitarRenderer::hardwareColour | **no** | hardwareColourIsSilent | No UI |
| Family selector = first drawer category | 12.1 | yes | kCategories "Guitar" (WorkshopPanel.cpp:1044) | WS | theGuitarCategorySwitchesFamily | |
| One confirmation per session | 12.1 | yes | WorkshopPanel::switchFamily familyConfirmedThisSession | WS | — | Undo warning added (d4643d2) |
| 250 ms crossfade | 12.1 | yes | IllustrationMotion.h:SceneCrossfade; GuitarBodyComponent fade | Illus/WS | ReducedMotion.aGuitarChangeCrossfadesOrIsStaticWithAnOutline | |
| Banner listing replaced parts | 12.1 | yes | switchFamily `replaced` | WS | aFamilySwitch… | |
| Family change via template | 12.2 | yes | PartLibrary::switchFamily | WS | aFamilySwitch… | |
| Per-family template files | 12.2 | partial | getFamilyTemplate maps to factory guitars | — | — | No template files; no extended |
| 12.3 changes | 12.3 | yes | template + writeGuitarParameters | WS | — | |
| Amp defaults follow family | 12.3 | yes | Workshop/FamilyDefaults.cpp:ampModelFor/applyAmpDefaults (+room for acoustic) | WS | FamilySwitch.theAmpFollowsTheFamilyOnlyWhenItDoesNotSuit | |
| Family → bass mode in rhythm engine | 12.3 | yes | retargetStrumDefaults | — | TunePlayer.theBassGoesToTheEngineOnlyForABass | |
| Slide-friendly hint on a high nut | 12.3 | no | — | — | — | |
| MIDI-out profile default per family | 12.3 | no | — | — | — | FamilyDefaults sets amp and room only |
| Preserve name, FX, amp, … on switch | 12.4 | yes | switchGuitarFamily | — | WorkshopFamily.aFamilySwitchKeepsWhatSection12_4Keeps | 961cd55 keeps host values |
| Hit regions, topmost wins | 13.1 | yes | GuitarRenderer::hitTest | WS | hitTestingFindsThePartOnTop | |
| Alt+click = audition | 13.1 | partial | hoverCard(altDown) | WS drawer | auditionFromTheDrawerNeverCommits | Alt on the illustration is still "bass side" |
| Shift+click targets the region below | 13.1 | yes | BenchIllustration::mouseDown (WorkshopPanel.cpp:673) | WS | — | New since the last audit; untested |
| Ctrl+click string → override target | 13.1 | yes | bench Ctrl-click → string override | WS | WorkshopStrings.aCardOntoAStringOverridesIt… | |
| Drag cards onto the illustration | 13.2 | no | click-to-fit (clickCard → targetSlot) | WS (click) | clickingACardFitsItAsOneUndoEntry | No drag-and-drop |
| Capo card onto fret / headstock removes | 13.2 | partial | Drag::capo; fitAccessory | WS | WorkshopAccessories.theCapoIsDrawnAndDraggedByFrets | Capo drags by fret; no drag-to-remove |
| Drag pickup along axis, ruler snap | 13.3 | yes | BenchIllustration::mouseDrag | WS | aPickupDragIsOneEntry… | |
| Screw handles → height per side | 13.3 | yes | mouseWheelMove | WS | heightsAndSetupEditsAreOneEntryEach | |
| Drag saddle → intonation | 13.3 | yes | Drag::saddle | WS | — | |
| Drag nut slot → depth | 13.3 | yes | Drag::nut; WorkshopBench::setNutSlotDepth | WS | WorkshopNut.aSlotDragIsOneEntryInRealUnits… | |
| Drag capo / pick / slide | 13.3 | yes | Drag::pick/pickRotate/slide/slideRotate/capo | WS | WorkshopAccessories.* | |
| Snap modifiers per spec | 13.3 | partial | mouseDrag | WS | snapIsOneMillimetreFineWithShiftFreeWithAlt | Mapping still differs (Shift fine, Alt free, no Ctrl ultra-fine) |
| Audition tint 500 ms | 14 | yes | setAuditionTint/endAuditionTint | WS | WorkshopSpectrum.theBodyTintsWhileAuditioningAndFadesIn500ms | |
| Thumbnail reduced detail | 15 | yes | Options.thumbnail via GuitarThumbnails | preset browser | Thumbnails.* | Now has a caller |
| Accessible child per part | 16 | partial | BenchIllustration setDescription; scene.stringDescriptions | WS | accessibleDescriptionsNameTheParts | Still one component with a changing description |
| Tab walks; Enter → inspector; announce | 16 | partial | BenchIllustration::keyPressed | WS | keyboardNudgesMatchADrag | No Enter handler; arrows nudge |
| Reduced motion | 16 | yes | SceneCrossfade static outline; NoteDots; tint suppressed | Illus/WS | ReducedMotion.* | |
| Perf budgets | 17 | partial | — | — | fullRenderIsFastEnough; WorkshopSpectrum.hundredShadowRendersStayUnder40ms | Overlay 2 ms not timed |
| New shapes as data | 18 | partial | illustration.body_style/headstock/outline | — | — | Bridge and pickup shapes still chosen by name heuristics |
| T: render 128…3072 | 19 | yes | — | — | everyFactoryGuitarRendersWithoutClipping | |
| T: hash on swap/move | 19 | yes | — | — | theKeyChangesWithEveryVisibleChange | |
| T: family → family | 19 | yes | — | — | aFamilySwitchGivesTheTargetFamilysGuitar | |
| T: 10 000 clicks | 19 | yes | — | — | hitTestingFindsThePartOnTop | |
| T: drag bounds (height ≥0.8 mm) | 19 | yes | — | — | WorkshopRanges.theTabCarriesAPadlockAndHeightsStopAtStock; aPickupStopsBeforeItOverlaps… | |
| T: burst vs reference SVG | 19 | no | — | — | — | |
| T: live dot within 60 ms | 19 | yes | — | — | NoteDots.theDotAppearsWithin60ms… | |
| T: reduced motion / thumbnail cache | 19 | yes | — | — | ReducedMotion.*; Thumbnails.workerRendersCachesAndEvictsAt200 | |

---

### guitar-workshop.md

**Summary:** 45 requirements. **yes 35 / partial 9 / no 1** (unchanged).
- **Changed since the last audit:**
  - Model-gaps' block-boundary live swap is merged (`PartSwap.*`).
  - Byte-flip robustness and audio round-trip tests are added (`WorkshopQaTests.cpp`).
  - "Open Parts folder" and "Open Guitars folder" are in File Locations.
  - Per-string overrides are added.
- **Still open:**
  - **4 of 15 slots (top, fretboard, tailpiece, pickguard) still have no drawer category** (`kCategories`, WorkshopPanel.cpp:1044).
  - The string-count excess still goes to a notice or the error log, not to the inspector.
  - The folder is still not rescanned when it changes.
  - The missing-part banner still has no jump action.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| A guitar is its parts | 0.1 | yes | WorkshopGuitar; mapSpec | WS | — | |
| Parts are files; factory RO, user folder | 0.2 | yes | PartLibrary folders | Options File Locations "Open Parts folder" | theFactoryLibraryIsThere | |
| Compatibility advisory | 0.4 | yes | getCompatibilityWarnings | WS | incompatiblePartsFitWithAWarning | |
| Spec owned by audio thread, atomic swap | 0.5 | yes | applyWorkshopGuitar; live block-boundary swap | — | WorkshopSwap.*; PartSwap.aSwapKeepingTheStructureIsTakenAtABlockBoundaryWithoutSilence | model-gaps merged |
| 25 enum guitars ship as files | 0.6 | yes | Resources/Guitars + migration.json | Guitar type combo | GuitarMigration.* | |
| Slot body | 1 | yes | GuitarSlot::body | WS "Body" | — | |
| Slot top | 1 | partial | GuitarSlot::top | **no** | — | No drawer category |
| Slot neck | 1 | yes | | WS "Neck" | — | |
| Slot fretboard | 1 | partial | | **no** | everyFittedPartIsReachableAndNothingElseIs (hit only) | Selectable on the illustration; cannot be swapped |
| Slot frets / nut / bridge / tuners | 1 | yes | | WS categories | — | Tuners now audible (tuning-stability) |
| Slot tailpiece | 1 | partial | | **no** | — | |
| Pickup slots | 1 | yes | | WS "Pickups" | — | |
| Wiring / strings | 1 | yes | | WS "Wiring"/"Preamp"/"Strings" | — | Per-string override added |
| Slot pickguard | 1 | partial | | **no** | — | |
| hardware_color / finish / setup / seed | 1 | partial | WorkshopGuitar | setup strip only | — | No finish or hardware UI |
| Zero-pickup guitar legal | 1 | yes | mapSpec | — | — | |
| Part types + field sets | 2 | yes | PartType; Part::fields | WS inspector | — | |
| Accessories slide/pick/capo | 2 | yes | fitAccessory; Processor setSlidePart | WS | WorkshopAccessories.*; WorkshopCapo.* | Slide material now played |
| GuitarSpec model | 3 | yes | WorkshopGuitar | — | — | |
| Table as fallback | 3.1 | yes | mapSpec baseTypeFor | — | aMissingGuitarFileFallsBackToItsType | |
| DerivedAcoustics cached; 5 ms crossfade | 3.2 | partial | mapSpec once per swap; PartSwap live path | — | aSwapMapsOnceNotPerBlock; PartSwap.* | Several derived fields still unread (see part-acoustics) |
| Factory/user layout | 4 | yes | PartLibrary | — | — | |
| Scanned at startup and on folder change | 4 | partial | PartLibrary::refresh | — | — | No watcher |
| User part wins | 4 | yes | PartLibrary::find | — | aUserPartBeatsTheFactoryOne | |
| Missing ref → default | 4.1 | yes | PartLibrary::resolve | — | aMissingPartFallsBackAndSaysSo | |
| Missing-part notification with jump-to-Workshop | 4.1 | partial | PluginEditor.cpp:1142 "missing-part" | banner | — | No action; stale comment at PluginEditor.cpp:950 |
| Missing part in error log | 4.1 | yes | ErrorLog | — | aMissingPartFallsBackAndSaysSo | |
| Compatibility warning text | 5 | yes | PartLibrary; inspector | WS | incompatiblePartsFitWithAWarning | |
| String count = min(neck, bridge) | 5.1 | yes | getStringCount | — | aStringCountMismatchClamps | |
| Excess reported in the inspector | 5.1 | no | PluginProcessor.cpp:938 notice; ErrorLog | banner only | — | Not in the inspector |
| Tuning resize | 5.1 | yes | writeGuitarParameters | — | choosingATypeGivesItsStringCount | |
| Save As Guitar, Ctrl+G | 6 | yes | saveGuitarAs; registry (Accessibility.cpp:620) | WS button, Ctrl+G | saveAsGuitarWritesAFile… | Stale comment Accessibility.cpp:560 |
| By reference; bundle option | 6 | yes | saveGuitarAs(bundleParts) | dialog | — | |
| Preset reference updated | 6 | yes | guitarReference | — | saveAsGuitar… | |
| Save As Part | 7 | yes | savePartAs | WS | saveAsPartMakesAUserPartAndFitsIt | |
| Editing a factory part → user copy | 7 | yes | WorkshopBench::editField | WS | editingAFieldMakesAUserCopy | |
| Preset reference + override | 8 | yes | guitarOverride | — | anEditedGuitarTravelsWholeInTheState | |
| Workshop adds no parameters | 9 | yes | — | — | — | |
| Pickup placement params retired | 9 | yes | PresetManager | — | oldPickupPlacementParametersBecomeTheGuitars | 6 IDs, not 9 |
| T: factory round trip | 10 | yes | — | — | everyFactoryGuitarLoadsAndRoundTrips; Workshop.everyFactoryGuitarRoundTripsInAudio; everyByteOfAFactoryGuitarFlipped… | |
| T: part swap changes spectrum; swap back −80 dB | 10 | partial | — | — | everyMappedFieldMovesSomething; SpectrumDelta | Swap-and-back is not rendered (the audio round-trip covers file reload only) |
| T: swap click-free | 10 | yes | — | — | aPartSwapDuringANoteIsClickFree | |
| T: missing / user-wins / incompatible / clamp | 10 | yes | — | — | Workshop.* | |
| T: no FS on audio thread; mapped once | 10 | yes | — | — | noFileIsTouched…; aSwapMapsOnce… | Plus hundredRandomPartSwapsStayUnder50ms |
| T: override self-contained | 10 | yes | — | — | anEmbeddedGuitarNeedsNoPartFiles | |

---

### part-acoustics.md

**Summary:** 59 requirements. **yes 40 / partial 10 / no 9** (previously 38 / 11 / 10).
- **Now done:**
  - `feedbackGain` reaches `FeedbackLoop::setBodyCoupling`, so chambering reaches the feedback loop.
  - `nut.friction`, and all the tuner fields (ratio, stability, locking), now feed `StabilityModel` through `LuthierEngine::partsNutFriction` and `partsTuner*` (realism-c).
  - Bridge mass and coupling also drive the new `BodyCouplingBank` (realism-a).
  - Per-string overrides are mapped.
- **Still computed and never read:** **`airResonanceHz/Q`, `bodyGainDb` and `finishDampingDb`**. Realism-a added `BodyEngine::setRuntimeScaling(…, airFreqMul, …, airQMul)`, a ready hook, but nothing passes the derived air mode into it.
- **Still recomputed separately:** `windingPitchPerMm` (`PlayingNoise.cpp:19`).
- **Still unmapped:** `fretboard.radius_mm` (drawn only), neck `profile` (text only) and `wood`, `strings.core`, `tension_kg`, `coil_turns`, `wiring.switching` (drawn only) and `bridge.piezo`.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| One function mapSpec, once per swap | 0.1 | yes | Model/Workshop/PartAcoustics.cpp:mapSpec | n/a | aSwapMapsOnceNotPerBlock | |
| Physical units in, DSP out | 0.2 | yes | mapSpec | n/a | — | |
| Monotonic and continuous | 0.3 | yes | — | n/a | theMappingIsMonotonic | |
| Constants named and sourced | 0.5 | partial | comments | n/a | — | Some fitted constants loosely sourced |
| Wood table | 1 | yes | lookUpWood | inspector | — | |
| Density/E → mode frequency | 1.1 | partial | resonanceTrim (PartAcoustics.cpp:394) | — | monotonic | E unused |
| tanδ → mode Q | 1.1 | no | — | — | — | Only the fretboard uses lossTangent (:445) |
| Neck/fretboard density → coupling, dead spots | 1.1 | no | — | — | — | |
| body.wood / density | 2 | yes | mapSpec body | WS inspector | everyMappedFieldMovesSomething | |
| body.thickness_mm | 2 | yes | scaleDepth | inspector | moves-something | |
| body.area_cm2 | 2 | yes | scaleWidth | inspector | moves-something | |
| body.chambering → modes | 2.1 | yes | shapeFor; Model/Guitar/Chambering.h (env + body coupling) | inspector | BodyCoupling.BC02/BC03 | |
| Chambering air resonance | 2.1 | partial | d.airResonanceHz/Q (:410, :423) | — | chamberingPutsTheAirModeInItsRange | Still unused by the engine; BodyEngine::setRuntimeScaling is the hook |
| Chambering mode gain dB | 2.1 | partial | d.bodyGainDb | — | — | Not consumed |
| Chambering sustain | 2.1 | yes | sustainScale | — | — | |
| Chambering feedback coupling | 2.1 | yes | LuthierEngine.cpp:387 FeedbackLoop::setBodyCoupling(bodyCouplingFor(d.feedbackGain)) | — | — | model-gaps merged; no dedicated test |
| body.bracing | 2 | yes | engineBracing | — | — | |
| scale_length → tension | 3 | yes | stringTensionNewtons | inspector | scaleLengthSetsTension | |
| neck.profile (mass) | 3 | no | — | — | — | Only in the illustration's description text |
| neck.joint coupling | 3 | yes | jointCoupling | inspector | couplingsMultiply | |
| fretboard.wood → brightness | 3 | yes | fretBrightness × tanδ | — | — | Fretboard not swappable in the UI |
| fretboard.radius_mm → buzz | 3 | no | — | — | — | Drawn only (GuitarRenderer.cpp:2163); DECISIONS defers |
| frets.material brightness | 4 | yes | fretMaterialBrightness | inspector | aReferenceGuitarSoundsLikeTheEngineDefault | |
| frets.height | 4 | yes | setup.fretHeight | setup | moves-something | |
| frets.width | 4 | yes | fretBrightness | — | moves-something | |
| frets.count | 4 | yes | spec.maxFrets | — | — | |
| nut.material | 4 | yes | nutBrightnessFactor | — | — | |
| nut.slot_depths | 4 | yes | setup.nutDepth | WS nut drag | WorkshopNut.* | |
| nut.friction → bend stability | 4 | yes | d.nutFriction → LuthierEngine::partsNutFriction → StabilityModel | WS inspector line (RealismGroupsC.cpp:866) | — | realism-c; no part-acoustics-level test |
| bridge.mass_g | 5 | yes | terminationMassG; partsBridge.massKg → BodyCouplingBank | inspector | monotonic; BodyCoupling.BC08_aHeavyBridgeCouplesLess | |
| bridge.coupling | 5 | yes | couplingFraction | inspector | couplingsMultiply | |
| bridge.type defaults table | 5 | partial | part files | — | — | |
| has_tremolo / tremolo_type | 5 | yes | d.spec.bridge | — | — | |
| bridge.piezo | 5 | no | — | — | — | |
| tailpiece.mass_g | 5 | yes | terminationMassG | — | — | |
| tailpiece.break_angle | 5 | yes | fretBrightness × angle | — | — | Tailpiece not swappable |
| Pickup L, R, C | 6 | yes | PickupSpec | inspector | moves-something | |
| magnet | 6 / 6.2 | yes | lookUpMagnet | inspector | magnetPullShortensSustainAndPullsFlat | |
| coil_turns | 6 | no | — | — | — | |
| pole_piece_material | 6 | yes | poleBrightness | — | — | |
| cover loss | 6 | yes | coverLossDbAt4k | — | aCoverCostsTopEnd | |
| position_mm → comb | 6.1 | yes | spec.position | WS drag | pickupPositionSetsTheComb; WorkshopSpectrum.combNotchesSitWhereThePickupIsANode | |
| height → level and damping | 6 | yes | heightMm | WS wheel | — | |
| wiring pots/cap/taper/bleed/active | 7 | yes | d.wiring | Circuit panel | — | |
| wiring.switching | 7 | no | — | — | — | Drawn only (GuitarRenderer.cpp:1360) |
| strings gauges → μ, T | 8 | yes | computeSpec | — | — | Per-string gauge override added |
| winding type | 8 | yes | materialFromIds | — | — | |
| winding_material | 8 | yes | materialFromIds | WS per-string | WorkshopStrings.* | |
| core round/hex | 8 | no | — | — | — | |
| winding_pitch_per_mm → squeak | 8 | partial | d.windingPitchPerMm | — | — | Still not passed; PlayingNoise.cpp:19 recomputes |
| tension_kg override | 8 | no | — | — | — | |
| Inharmonicity B | 8 | yes | StringMaterials::computeSpec | — | — | |
| pickguard.mass_g → top damping | 9 | partial | terminationMassG | — | moves-something | |
| hardware_color silent | 9 | yes | — | — | hardwareColourIsSilent | |
| finish.gloss → −0.5 dB, Q −8% | 9 | partial | d.finishDampingDb (:466) | — | — | Never consumed |
| finish.aging → break-in | 9 | yes | body.age | — | — | |
| Composition rules | 10 | partial | — | — | couplingsMultiply | Loss-domain Q sum absent |
| T: every numeric field moves the spectrum | 11 | partial | — | — | everyMappedFieldMovesSomething | Checks the derived struct only |
| T: named tests | 11 | yes | — | — | PartAcoustics.* | Chambering test still reads a struct field |

---

### advanced-ranges.md

**Summary:** 49 requirements. **yes 39 / partial 6 / no 4** (previously 36 / 8 / 5).
- **Now done:**
  - The `strings`, `environment` and `body` families, with their rows, are merged (realism-a/c).
  - The WORKSHOP tab padlock covers buzz, pick and slide, with a 0.8 mm height stop.
  - The Diagnostics mirror is in `AudioPathView.cpp:73-84`, listing the advanced families.
  - The first-unlock explainer now has a test.
- **Still open:**
  - The **modulation family's setter clamps are missing**: `ModSources.h:109` still has a fixed `jlimit(0.01, 40)`. Techniques adds the first `modulation` registry row (`bendGlobalRange`), which is a parameter row and not a mod-rate clamp.
  - **Telemetry `advanced_ranges_used`** is missing.
  - Snapshots store normalised values and are untested.
  - There is no test that undo restores a clamp.
  - Squeak probability/moisture/pressure, pick bevel/wear and slide pressure still have no rows.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Stock default everywhere | 0.1 | yes | RangeState | — | theRangesBlockRoundTrips… | |
| Advanced marked, never hidden | 0.2 | yes | RangesUi::tagSlider/markReadout | all knobs | markingFollowsTheValueNotTheMode | |
| Widening silent; narrowing clamps, undoable, announced | 0.3 | yes | RangeState::applyTo; changeRanges | Opt→Ranges | wideningPreserves…; narrowingClamps… | applyTo now skips unchanged ranges (961cd55) |
| Mode in preset; prefs per user | 0.4 | yes | PresetManager; UiPreferences | — | — | |
| Only physical params | 0.5 | yes | RangeRegistry | — | — | |
| No parameter count change | 0.6 | yes | — | — | — | |
| PhysicalRange invariants | 1 | yes | PhysicalRange::isValid | — | everyPhysicalRangeIsValid | Table is now counted (allEntries) |
| Stock = declared range | 1.0 | yes | findDeclarationMismatches | — | stockMatchesTheDeclaredRange | |
| Normalised against live range | 1.1 | yes | makeRange | — | normalisationFollowsTheLiveRange | |
| Mode change via command queue | 1.1 | partial | Processor::changeRanges | — | — | Direct call |
| Switch to advanced preserves values | 1.2 | yes | applyTo | — | wideningPreservesEveryPlainValue | |
| Switch to stock clamps + banner | 1.3 | yes | RangesUi::apply | Opt→Ranges | narrowingClampsAndReportsTheCount | |
| Mod routes sweep live range | 1.4 | yes | ParameterBridge | — | — | |
| Family keys | 2 | yes | RangeFamily (+strings, environment, body) | — | — | 10 families now |
| Per-control unlock via right-click | 2 / 4 | yes | Widgets showParameterContextMenu | right-click | rightClickUnlocksAndRestrictsOneControl | |
| Modulation family = setter clamps | 2.1 / 3.4 | **no** | ModSources.h:109 fixed jlimit(0.01, 40) | no | — | Techniques adds a `modulation` row for bendGlobalRange only |
| amp rows | 3.1 | yes | PhysicalRange.cpp | Amp face | — | Plus noise ground-loop, hiss and microphonics (realism-c) |
| circuit rows | 3.2 | yes | PhysicalRange.cpp | Circuit panel | — | Plus noise rows and pickupApertureScale |
| squeak rows | 3.3 | partial | squeakAmount, squeakMinTravel, adjacentMuteAmount | — | — | probability, moisture and pressure still missing |
| pick rows | 3.3 | partial | + harmonic touch, finger release, thumb, rest stroke, palm spread | — | — | bevel and wear missing; tapMaxConcurrent in progress on techniques |
| buzz rows | 3.3 | yes | action, relief, nut depths, fret height | WS setup | WorkshopRanges.* | |
| slide rows | 3.3 | partial | slant, noise, clank | — | — | pressure missing; slideSpeedLimit in progress on techniques |
| strings / environment / body families | — | yes | PhysicalRange.cpp REALISM-A/C blocks | CHARACTER groups | — | Merged; no family-specific test |
| Sparse registry | 3.5 | yes | RangeRegistry | — | — | |
| `ranges` block schema | 4 | yes | RangeState::toVar/fromVar | — | theRangesBlockRoundTrips… | |
| Redundant unlock dropped | 4 | yes | setUnlockedIndividually | — | — | |
| Restrict only when listed | 4 | yes | buildParameterContextMenu | right-click | rightClickUnlocksAndRestricts… | |
| Legacy derivation | 4.1 | yes | deriveFromCurrentValues | — | theRangesBlockRoundTripsAndDerivesWhenAbsent | |
| Malformed block = absent | 4.1 | yes | fromVar | — | same | |
| Snapshots don't carry mode; recall clamps | 5 | partial | Live/Snapshots.cpp:186/262 store getValue() (normalised) | — | — | Re-maps rather than clamps |
| A/B slots own ranges | 5 | yes | full state | header A/B | — | |
| Randomise respects stock | 5 | yes | presets.randomise | Opt→Ranges | randomiseStaysInStockUnlessToldOtherwise | |
| Reset never changes mode | 5 | yes | — | — | — | |
| MIDI Learn over live range | 5 | yes | normalised | — | — | |
| Warning-colour arc | 6.1 | yes | LookAndFeel | knobs | RangeMarking.theWarningArcAppearsPastStockAndGoesWhenReturned | |
| `*` suffix | 6.1 | yes | RangesUi::markReadout | knobs | markingFollows… | |
| Tab padlock | 6.1 | yes | RangeTabButton on CHARACTER and WORKSHOP; header PadlockButton | tabs, header | WorkshopRanges.theTabCarriesAPadlock…; theHeaderPadlockShowsOnly… | |
| Opt→Ranges master toggle + preview | 6.2 | yes | RangesPage | Opt→Ranges | theRangesPageListsLocksAndClamps | |
| Always show warning colour | 6.2 | yes | kWarningColourKey | Opt→Ranges | — | |
| Out-of-stock list | 6.2 | yes | RangesPage::clampOne | Opt→Ranges | same | |
| Locked notice; drag stops | 6.3 | yes | showLockedRangeNoticeIfAtEdge | knobs | — | |
| First-unlock explainer, once | 6.4 | yes | showExplainerIfFirstTime; Restore re-arms | popover | FirstRun.theRangeExplainerSaysSectionSevensWords | |
| Undo `ranges-toggle`, grouped, restores clamps | 7 | partial | pushUndoState | Ctrl+Z | generic Undo tests | No ranges-specific test |
| Telemetry `advanced_ranges_used` | 8 | **no** | — | — | — | Telemetry.cpp has only category switches |
| Opt→Diagnostics mirrors it | 8 | yes | UI/AudioPathView.cpp:73-84 | Opt→Diag | Diagnostics.theAudioPathShowsWhatIsSoundingAndTheFlags | Lists advanced families; per-control unlocks not listed |
| Mode read on change only | 9 | yes | applyTo | — | — | |
| T: invariants … randomise | 10 | yes | — | — | Ranges.*, RangesUi.* | |
| T: undo restores a clamp | 10 | no | — | — | — | |
| T: snapshots do not carry mode | 10 | no | — | — | — | |

---

### tune-builder.md

**Summary:** 88 requirements. **yes 81 / partial 6 / no 1** (previously 49 / 34 / 5).
- **Now done:** the whole tune-help remainder is merged and verified in `TuneEditingTests.cpp` and `TuneIntegrationTests.cpp`:
  - the export dialog: audio with stems, MIDI profiles, notation and project bundle
  - the chord popover, pill drag and pill menu
  - the piano roll's menu, multi-select, clipboard and nudge
  - section drag, Vary and the setlist strip
  - the BASS and LAYERS rows
  - the TOOLS menu: palette, suggest next, reharmonize, ±N transpose and modal shift with Follow mode
  - STYLE and FOLLOW
  - SING hum capture
  - timeline modulation (`tune_feel_mod`, `tune_tempo_drift` as MOD destinations)
  - snapshot sections and TO LOOPER
  - the Ctrl+T registry entry
  - the offline and re-import tests
- **Still open:**
  - **MP3** is refused by decision.
  - **Variations (A/B)** exist in the model only.
  - `melody_range` and `string_hint` have no control.
  - The piano roll does not scroll horizontally.
  - There is no companion-instance bass routing.
  - The standalone does not arm MIDI or audio input.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Control-only module | 0.1 | yes | TunePlayer → engine MIDI | — | theMelodySoundsWhileTheRhythmEngineStrums | |
| Save/reload .luthiertune | 0.2 | yes | TuneFile | SAVE/LOAD | aHundredRandomTunesRoundTrip…; unknownFieldsAreKept… | |
| Every edit undoable; locked notes survive | 0.3 | yes | TuneSession::edit | TUNE | regenerateLeavesLocked…; TuneIntegration.theTunesUndoStackIsSeparateFromThePlugins | |
| Works without a DAW | 0.4 | yes | Processor owns TuneSession | TUNE | — | |
| Export audio/MIDI/notation from one score | 0.5 | yes | Support/TuneExport.cpp; UI/TuneExportDialog.cpp | EXPORT / Ctrl+E | theExportDialogWritesEachDestinationFromOneScreen | |
| Meta fields | 1 | yes | TuneMeta | title/tempo/key/mode; swing via KIT TEMPO | — | No direct control for swing, time signature or artist |
| Sections | 1 | yes | TuneSection | TUNE | — | |
| Setlist with repeats | 1 | yes | TuneArrangement | TuneSetlistStrip | sectionsReorderVaryAndArrangeInTheSetlist | |
| Variations (A/B arrangements) | 1 | partial | TuneVariation (TuneModel.cpp:1006) | **no** | — | Model only; nothing in UI/ references it |
| ChordCell fields | 1.1 | yes | ChordCell | TuneChordEditor popover | theChordPopoverEditsEveryFieldOfTheCell | |
| Hold last cell | 1.1 | yes | resolveChordSpans | — | chordSpansRepeat…Hold… | |
| MelodyTrack string_hint, articulation default | 1.2 | partial | MelodyTrack.stringHint | no | — | No control; per-note articulation is in the roll menu |
| MelodyNote fields | 1.2 | yes | MelodyNote | roll right-click menu | thePianoRollSelectsNudgesCopiesAndEditsNotes | |
| Absolute/relative pitch | 1.2 | yes | resolveMelodyPitch | — | relativePitchesFollowTheChord… | |
| Genre kit with tempo/feel/palette | 2.1 | yes | TuneExamples TuneKits (28); kitBox, PALETTE, KIT TEMPO | TUNE rhythm row | TuneIntegration.aGenreKitBringsItsTempoFeelAndPalette; SampleContent.everyGenreKit… | Kit names still differ from the spec's examples |
| Progression text field | 2.2 | yes | parseProgression | TUNE | shorthandParses… | |
| Melody Auto | 2.3 | yes | generateMelody | AUTO | autoMelodyIsByteIdentical… | |
| Melody Draw | 2.3 | yes | TunePianoRoll | DRAW | thePianoRollDrawsSnappedLockedNotes… | |
| Record from MIDI | 2.3 / 4.3 | yes | finishRecording | RECORD | recordingQuantisesATake… | |
| Improvise + Freeze | 2.3 / 4.4 | yes | generateImprovisedPass | IMPROVISE/FREEZE | improviseVaries… | |
| Play/loop; bar-boundary edits | 2.5 | yes | TunePlayer | transport | anEditWhilePlayingWaitsForTheBarLine… | |
| Ctrl+S / Ctrl+E | 2.6 | yes | TunePanel::keyPressed | shortcut | — | |
| Export dialog one screen | 2.6 | yes | TuneExportDialog | EXPORT | theExportDialogWritesEachDestinationFromOneScreen | |
| TUNE tab between RHYTHM and LIVE | 3 | yes | AdvancedPanel.cpp:1148 | tab | — | |
| Header | 3.1 | yes | buildHeader | TUNE | theHeaderEditsTitleTempoKeyAndSaves… | |
| Section strip | 3.1 / 3.3 | yes | TuneSectionStrip | TUNE | theSectionStripsMenu… | |
| Rhythm strip | 3.1 / 3.5 | yes | buildRhythm | TUNE | theRhythmStripSets… | |
| Piano-roll strip, scrollable, shrinks <1280 | 3.1 | partial | TunePianoRoll | TUNE | — | Still no horizontal scroll or viewport in TunePianoRoll.cpp |
| Transport | 3.1 / 3.6 | yes | buildTransport | TUNE | theTransportAndSpaceDriveThePlayer | |
| Pills coloured by function | 3.2 | yes | colourForDegree | TUNE | diatonicPaletteAndFunctionsFollowTheKey | |
| Click cell → popover | 3.2 | yes | UI/TuneChordEditor.cpp (CallOutBox) | TUNE | theChordPopoverEditsEveryFieldOfTheCell | |
| Drag edge = duration; reorder | 3.2 | yes | UI/TuneChordPillsEditing.cpp | TUNE | dragsResizeAndReorderChordPills | |
| Typing updates pills live | 3.2 | yes | progressionTextChanged | TUNE | theProgressionFieldWrites… | |
| Pill right-click menu | 3.2 | yes | TuneChordPills::buildMenu | TUNE | theChordPillMenuInsertsDuplicatesDeletes… | |
| Section menu | 3.3 | yes | TuneSectionStrip::buildMenu | right-click | theSectionStripsMenuRenames… | |
| Drag sections to reorder | 3.3 | yes | UI/TuneSectionStripEditing.cpp | TUNE | sectionsReorderVaryAndArrangeInTheSetlist | |
| "Vary" sibling | 3.3 | yes | Tune/TuneVary.cpp | section menu | same | |
| Setlist timeline | 3.3 | yes | UI/TuneSetlistStrip.cpp | TUNE | same | |
| Roll bar lines, scale shading | 3.4 | yes | TunePianoRoll::paint | TUNE | rendersWithTheLookAndFeel | |
| Snap to key; C toggles chromatic | 3.4 | yes | TunePanel.cpp:1407 | key C | — | |
| Note right-click menu | 3.4 | yes | TunePianoRoll::buildNoteMenu | TUNE | thePianoRollSelectsNudgesCopiesAndEditsNotes | |
| Drag-box multi-select, shift-click | 3.4 | yes | selectInBox/selectNote | TUNE | same | |
| Cut/copy/paste; nudge | 3.4 | yes | cutSelected/paste/nudgeSelected | TUNE | same | |
| Generators on current section | 3.4 | yes | generateMelody | — | regenerateLeavesLocked… | |
| Rhythm edits section; Link rhythm | 3.5 | yes | rhythmLinkedTo | right-click | theSectionStripsMenu…Links… | |
| Host transport wins | 3.6 | yes | followingHost | — | theHostWinsWhenItPlays… | |
| Space / Shift+Space | 3.6 | yes | keyPressed | TUNE | theTransportAndSpaceDrive… | |
| Auto generator rules | 4.1 | yes | generateAutoMelody | AUTO | autoMelodyStaysInRangeRests… | |
| melody_range / density "wider on request" | 4.1 | partial | MelodyTrack.rangeLow/High, density | density follows kit (TunePanel.cpp:807) | — | No range or density control |
| Regenerate increments seed | 4.1 | yes | regenerateMelody | AUTO | — | |
| Quantise grids | 4.3 | yes | QuantiseGrid | Quantise combo | — | |
| Follow chord changes toggle | 4.3 | yes | followChordsToggle | FOLLOW | theChordToolsStyleAndFollowWorkFromTheTab | |
| Style transfer | 4.5 | yes | applyMelodyStyle; styleBox | STYLE | same; styleTransferChangesPhrasing… | |
| Diatonic palette | 5 | yes | TuneToolsMenu palette; PALETTE | TOOLS | same | |
| Suggest next chord | 5 | yes | TuneToolsMenu.cpp:36 suggestNextChords | TOOLS | same; suggestNextChordOffersThree… | |
| Reharmonize with undo | 5 | yes | TuneToolsMenu.cpp:77 via session.edit | TOOLS | same | |
| Transpose; melodies follow | 5 | yes | transposeTune | TOOLS ±12; Key combo | transposeMovesChords… | |
| Modal shift + Follow mode | 5 | yes | TuneToolsMenu followModeItem → shiftMode(follow) | TOOLS | same | |
| Bass modes | 6 | yes | generateBassLine; TuneLayersStrip BASS row; roll EDIT=Bass | TUNE | theBassAndLayerRowsEditTheSection; theRollEditsTheBass… | |
| Bass through bass engine, else separate MIDI | 6 | partial | setBassToEngine; MIDI out | — | theBassGoesToTheEngineOnlyForABass | No companion-instance routing |
| Layers pad/arp/counter/perc | 7 | yes | TuneLayer; TuneLayersStrip | TUNE | countermelodyStaysUnder…; theBassAndLayerRows… | |
| Layer on/off, volume, pan | 7 | yes | TuneLayersStrip | TUNE | theBassAndLayerRowsEditTheSection | |
| Playback through realism engine | 8 | yes | engine MIDI | — | theMelodySounds… | |
| State boundary resets | 8 | yes | processBlock | section menu | TuneIntegration.aStateBoundarySectionResetsAtItsStart | |
| Loop plays setlist | 8 | yes | TunePlayer loop | Loop | loopingWrapsOnTheSample… | |
| Audio export WAV/FLAC/MP3, depth, SR, stems, tail | 9.1 | partial | TuneExport::renderAudio/exportAudio | Export dialog | audioExportWritesTheMixAndEveryAuxStem | WAV/AIFF/FLAC only; MP3 refused by decision (no encoder; LAME licence) |
| MIDI export profiles, splits, realism/plain | 9.2 | yes | TuneExport::exportMidi via MidiProfiles | Export dialog | aLuthierProfileMidiExportReimportsToTheSameAudio | |
| Notation MusicXML/GP/ASCII | 9.3 | yes | TuneExport::exportNotation | Export dialog | notationAndProjectExportKeepSectionsChords… | |
| Project export with bundle | 9.4 | yes | TuneExport::exportProject | Export dialog | same | |
| 10 templates | 10 | yes | TuneTemplates | NEW menu | theTenTemplatesLoadInOrder… | Plus 6 example tunes (NEW → Example tunes) |
| JSON schema | 11 | yes | TuneFile | — | templateFilesAreInCanonicalForm… | |
| Standalone: last tune loads | 12 | yes | tune in plugin state | — | TuneIntegration.aRelaunchLoadsTheLastTuneAndPlaysIt | |
| Standalone: MIDI in / audio in armed | 12 | no | — | — | — | SING reads the sidechain bus when present; nothing arms inputs |
| Sung/hummed capture | 13 | yes | Tune/TuneHumCapture; DSP/Common/PitchTracker | SING | HumCapture.* | |
| Mod routes over tune timeline | 14 | yes | params tune_feel_mod / tune_tempo_drift (Parameters.cpp:897) | MOD destination list, host | theTunesTimelineParametersDriftTempoAndFeel | No dedicated knob; reached through the MOD matrix |
| Live snapshots / footswitch sections | 14 | yes | PluginProcessorTune.cpp captureTuneSnapshotState / applyPendingTuneSection | LIVE snapshots | aSnapshotRecallsTheTunesSection | |
| Looper captures a Tune render | 14 | yes | Looper::importLayer | TO LOOPER | theLooperCapturesAWholeTuneRender | |
| Ctrl+T new tune (registry) | gui | yes | Accessibility.cpp:633 newTune | Ctrl+T | ctrlTIsInTheShortcutRegistryAndOpensTheTuneTab | Stale comment at Accessibility.cpp:560 |
| T: 100 shorthand strings | 15 | yes | — | — | shorthandParses… | |
| T: auto determinism ×1000 | 15 | yes | — | — | autoMelodyIsByteIdentical… | |
| T: locked notes | 15 | yes | — | — | regenerateLeavesLocked… | |
| T: 1000 reorders | 15 | yes | — | — | aThousandSectionReorders… | |
| T: 60 s loop no drift | 15 | yes | — | — | aLoopedTuneRunsSixtySecondsWithoutDrift | |
| T: 100 random files | 15 | yes | — | — | aHundredRandomTunesRoundTripByteIdentical | |
| T: offline render nulls live −80 dBFS | 15 | yes | — | — | theOfflineRenderNullsAgainstTheLiveOne | |
| T: Luthier-profile MIDI re-import | 15 | yes | — | — | aLuthierProfileMidiExportReimportsToTheSameAudio | −60 dBFS bar, per midi-export 12 (decision) |
| T: sung capture 95% | 15 | yes | — | — | HumCapture.aHummedMelodyIsTranscribed… | |
| T: standalone reload | 15 | yes | — | — | aRelaunchLoadsTheLastTuneAndPlaysIt | |

---

### Top gaps (group C)

Ranked by user impact.

1. **A body swap still does not redraw the body** (illustration 0.3/0.5, ground rule 5). No body part carries `illustration.body_style` and no neck carries `illustration.headstock`, and `Resources/Parts` is unchanged since the last audit. 14 of 34 outlines are unreachable. This is data-only work.
2. **No finish or hardware-colour editor** (illustration 11, workshop 1). A user still cannot change a guitar's colour, even though the renderer, the file format and now the accent colour (Appearance "follow the guitar") all use it.
3. **Four Workshop slots have no drawer category** (top, fretboard, tailpiece, pickguard; WorkshopPanel.cpp:1044). 21 shipped parts cannot be fitted, so fretboard wood and inlays, tailpiece break angle and pickguard colour cannot be changed in the UI.
4. **Chambering air mode, mode gain and gloss damping are computed and discarded** (part-acoustics 2.1/9). Semi-hollow and hollow bodies still lack their defining air mode. The hook now exists (`BodyEngine::setRuntimeScaling` air multipliers), so the fix is small.
5. **The modulation range family is missing** (advanced-ranges 2.1/3.4). LFO rate is hard-clamped to 0.01-40 Hz and envelope, sequencer and follower have no clamps. The techniques branch adds only a parameter row.
6. **Drawer-to-illustration drag-and-drop and the drag ghost are absent** (13.2, 5.32). Click-to-fit plus on-bench drags cover most of the uses.
7. **The `extended` family (7/8-string, fanned) is missing** from families and templates.
8. **Part fields that still change nothing**: `fretboard.radius_mm`, neck profile and wood, `strings.core`, `tension_kg`, `coil_turns`, `wiring.switching`, `bridge.piezo` and `pole_spacing_mm`. Wood tanδ does not set body Q, and `windingPitchPerMm` is recomputed rather than passed. Tuners and nut friction are now fixed.
9. **Live overlays are incomplete**: there is no buzz heatmap (27) and no pickup pulse (30). The Easy illustration polls the engine rather than a FIFO, and it does not show the pick.
10. **TUNE variations (A/B arrangements)** exist in the model with no UI. The piano roll has no horizontal scroll for long sections.
11. **Snapshots re-map rather than clamp after a range lock**, and snapshots and ranges-undo are both untested (advanced-ranges 5, 7, 10).
12. **Telemetry `advanced_ranges_used`** is missing. The Diagnostics mirror is now present.
13. **Illustration accessibility**: there are no per-part accessible children, no Enter → inspector, and the arrows nudge rather than navigate.
14. **The missing-part banner has no jump-to-Workshop action**, and the string-count excess is not in the inspector (workshop 4.1, 5.1).
15. **The parts folder is not watched** (workshop 4). Parts dropped in by hand appear only after a save or a restart.
16. **Standalone does not arm MIDI or audio input** (tune 12), and there is no companion-instance bass routing (tune 6).
17. **Squeak, pick and slide registry rows are still missing**: squeak probability, moisture and pressure; pick bevel and wear; slide pressure.
18. **Merge hazard**: techniques' `PhysicalRange.cpp` hunk hard-codes `kNumEntries = 32 + 3` and `return table[index]` against a table that is now counted and has grown to about 80 rows.

### Unspecified gaps noticed

- **Left-handed guitars.** There is still no mirror option for the illustration, the fretboard or the bench.
- **Parts library management.** There is still no rename, delete, duplicate or import from the drawer. File Locations can now open the Parts folder, but without a rescan the result is not seen until a save or a restart.
- **Drag MIDI or audio from TUNE into the DAW.** Export is by file chooser only.
- **Drums or a click track in TUNE.** A full arrangement with no drums has no documented decision behind it.
- **Tempo or time-signature changes within a tune.**
- **Chord voicing preview in TUNE.** The piano-roll strip's Show fingering could be reused for a pill hover.
- **Finish undo.** It is moot until an editor exists.
- **Stale meta comments** (still present):
  - `PluginEditor.cpp:950` says "missing part" is unreachable; it is posted at :1142.
  - `Accessibility.cpp:560` says Ctrl+G and Ctrl+T are absent; they are registered at :620 and :633.
  - `spec/GAPS.md:197` says "TUNE not built".
  - `spec/GAPS.md:301` says "No RANGES".
- **Thumbnail size.** `GuitarThumbnails` renders 256x128 (landscape). The spec says 128x256.
- **Easy illustration never shows the pick.** `pickPositionMm` is set only on the bench.

### Small glue candidates

| Item | Exists at | Glue needed | Panel |
|---|---|---|---|
| Top / Fretboard / Tailpiece / Pickguard drawer categories | PartType::top/fretboard/tailpiece/pickguard; 21 parts | 4 rows in `kCategories` (WorkshopPanel.cpp:1044); `summaryOf` cases (:1083) | WS drawer |
| Body style per body part | resolveStyle reads `illustration.body_style` (GuitarRenderer.cpp:446) | Add the field to 20 body JSONs; add body parts for the 14 unused outlines | WS "Body" |
| Headstock per neck part | buildHeadstock reads `illustration.headstock` (:2295) | Add the field to 15 neck JSONs | WS "Neck" |
| Air mode / body gain / gloss into the engine | DerivedAcoustics airResonanceHz/Q, bodyGainDb, finishDampingDb; BodyEngine::setRuntimeScaling | Read in LuthierEngine applyWorkshopGuitar (:370-400), convert to air-frequency/Q multipliers and a body gain | engine |
| Winding pitch into noise | d.windingPitchPerMm | Pass it into PlayingNoise/ScrapeEngine StringInfo instead of recomputing (PlayingNoise.cpp:19) | engine |
| Finish editor | WorkshopGuitar.finish, hardwareColour | Inspector rows when Body is selected, plus a WorkshopBench setter (one undo entry) | WS |
| Extended family | Electric/7-String Modern.luthierguitar | Add "extended" to getFamilyTemplate (PartLibrary.cpp:283) and to the Guitar category list; ampModelFor already treats it as electric | WS "Guitar" |
| Modulation range family | RangeFamily::modulation; RangeState | Clamp ModSources::setRateHz (ModSources.h:109) and the env/seq/follower setters from RangeState | MOD + Opt→Ranges |
| Telemetry flag | RangeState families (as AudioPathView reads them) | Add `advanced_ranges_used` to the usage payload in Telemetry.cpp | — |
| Squeak/pick/slide registry rows | ParamIDs squeak_probability, squeak_finger_moisture, squeak_finger_pressure, pick_bevel, pick_wear, slide_pressure | Rows in PhysicalRange.cpp allEntries | knobs |
| String-count excess in inspector | LoadReport.stringExcess; d.stringExcess | One inspector line | WS inspector |
| Missing-part banner action | Notification "missing-part" (PluginEditor.cpp:1142) | Add an action that opens the WORKSHOP tab or overlay | banner |
| Melody range control | MelodyTrack.rangeLow/High | Two small controls, or a TOOLS submenu | TUNE melody row |
| Variations | Tune::addVariation (TuneModel.cpp:1006) | A/B switch next to the setlist strip | TUNE |
| Easy illustration pick | GuitarRenderer pick overlay | Set overlay.pickPositionMm from pluck_position in GuitarBodyComponent::updateLiveOverlay | Illus |
| Parts rescan | PartLibrary::refresh | Call it on WorkshopPanel visibility or focus, or add a 2 s timestamp poll | WS |
| Techniques merge fix | origin/claude/luthier-techniques PhysicalRange.cpp | Keep the 3 rows and drop the `kNumEntries = 32 + 3` / `table[index]` edits | — |

---

## Group D: phase-1 extensions

Scope: routing-io, modulation-matrix, rhythm-engine, live-performance, controllers, practice-tools, tone-match, notation-export, character-wear, updates-telemetry, midi-export, input-routing. None of these 12 spec files changed since the previous audit (git diff 2ede79c..HEAD -- spec/<file> is empty), so the requirement rows are the same. Every row was re-checked against the current Source/.
Since the previous audit (2ede79c), realism-a, realism-b, realism-c, model-gaps, tune-help, visual, release and review are merged (167 commits, 335 files under Source/). Every item that was "in progress on <branch>" for one of these branches was checked again in the current tree. Only `claude/luthier-techniques` is still unmerged. Its only group-D touch is a Mute Row on the RHYTHM tab and a `pendingMute` step field in RhythmEngine. Both belong to muting-rhythm, not to rhythm-engine.md, so no row here is marked "in progress on techniques".
Rows that changed carry **[changed]** in Notes. "Engine-only" means the class or setter exists but nothing in the UI or processor calls it. The other spec files named as examples in the task (piano-roll-chord-display, licensing, editions) belong to other groups; licensing overlaps updates-telemetry 5 and is noted there.

### routing-io.md

The routing work is largely done. All four bus layouts are advertised and tested, Aux 1-7 plus a new Aux 8 noise bus are distributed, and 12 per-string buses work. The ROUTING tab exists in Column 4. Sidechain-to-amp, MIDI-out sources, macro-to-CC mapping, the latency readout and the preset state all work. Gaps: there is no "sidechain compressor" pedal (the effect is absent from PedalType). The layout "selector" is a read-only label. Since the last audit: the Aux 1 pre/post-circuit toggle has merged, routing edits are undoable, and Latency::anImpulseArrivesWhenReported measures one path with an impulse. The per-aux latency is still only ordering-checked. **Counts (recounted from the table): yes 22 / partial 3 / no 1.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Layouts A-D advertised via BusesProperties | 0.1, 1 | yes | PluginProcessor::buildBusesProperties / isBusesLayoutSupported | n/a | Routing::everyLayoutRendersCleanly, PluginBuses::* | also Aux 8 noise bus appended last |
| Runs correctly at every layout | 0.2 | yes | processSlice + RoutingMatrix::distribute | n/a | Routing::everyLayoutRendersCleanly | |
| Aux 1-7 tap assignments (DI, pre-cab, mic1, mic2, room, wet, monitor) | 2 | yes | TapBuffers, AuxBus, RoutingMatrix::distribute/writeMonitorBus | n/a | Routing::diTapNullsAgainstReappliedAmp | **[changed]** Aux1 pre/post-circuit toggle merged: `aux1_pre_circuit` param, ROUTING "AUX 1 PRE-CIRCUIT" LuthierToggle; ModelGapsUi::auxOneTapsBeforeOrAfterTheCircuit, Routing::diPreCircuitBypassesTheCircuit |
| Per-aux gain trim | 2 | yes | RoutingMatrix::setAuxGainDb (smoothed) | ROUTING tab AuxStrip | Routing::stateRoundTrips, UndoCoverage::routingGainAndMuteAreEntries | **[changed]** gain/mute/solo are now undo entries |
| Muted aux skips its render | 2 | yes | RoutingMatrix::updateWantedTaps | n/a | Routing::muteAndSoloResolveTogether | |
| 12 mono per-string buses, post-body pre-pickup, silence if unused | 3 | yes | kNumPerStringBuses, distribute | ROUTING PerStringStrip (C/D only) | Routing::perStringOutputsSumToPreBody | |
| Per-string latency = engine-only | 3, 7 | yes | LuthierEngine::getPerStringLatencySamples | readout | Routing::perOutputLatencyIsConsistent | |
| Sidechain -> SidechainEnvFollower mod source | 4 | yes | ModEnvelopeFollower::Source::sidechain; ModBlockContext.sidechainPeak | MOD tab follower card | Modulation::envelopeFollowerTracksLevel | |
| Sidechain -> "sidechain compressor" pedal ducking | 4 | no | - | no | - | no such pedal in PedalType |
| Sidechain -> practice backing-track/monitor summing | 4 | yes | MonitorMix (Live/LiveControls) | LiveStrip monitor level | LiveMonitor::idleWhenNothingToMonitorAndSumsWhenThereIs | |
| Sidechain never sums into main unless consumed | 4 | yes | processSlice (sidechainCopy only to consumers) | n/a | Routing::sidechainToAmpReplacesTheInstrument | |
| Sidechain meter in routing panel | 4 | yes | RoutingMatrix::meterSidechain | ROUTING tab | - | |
| Internal re-amp "sidechain to amp" toggle (off by default) | 5 | yes | engine.setSidechainToAmp | ROUTING tab toggle | Routing::sidechainToAmpReplacesTheInstrument | |
| MIDI out: note pass-through, same timestamps | 6 | yes | MidiOutRouter::captureInput | ROUTING / MIDI OUT | Routing::midiOutPassThroughIsSampleExact (10k) | |
| MIDI out: rhythm engine stream | 6 | yes | MidiOutRouter | ROUTING / MIDI OUT | MidiExport::liveMidiOutKeepsTenThousandEventsOnTheirSample | |
| MIDI out: string activity NoteOn/Off | 6 | yes | StringActivityQueue | ROUTING / MIDI OUT | Routing::stringActivityIsSampleAccurate | |
| MIDI out: CC broadcast per macro, CC# assignable | 6 | yes | MidiOutConfig::macroCc | ROUTING macro CC combos | MidiOutPanel::liveSwitchesAreTheRoutingPanelsSwitches | |
| Silently no-ops on hosts without MIDI out | 6 | yes | standard MidiBuffer | n/a | - | |
| Per-output latency reporting | 7 | partial | updateRoutingLatencyReport | ROUTING latency label | Routing::perOutputLatencyIsConsistent | JUCE reports one value (main); per-output only shown in the UI (spec allows this) |
| ROUTING panel in Column 4 tab strip | 8 | yes | AdvancedPanel::buildWorkspace "ROUTING" | Col 4 ROUTING | GuiReachability | |
| Layout selector shows advertised layouts | 8 | partial | RoutingPanel layoutLabel | read-only label | - | shows the negotiated layout only; no list of supported layouts |
| Aux strips: mute, solo, gain, meter | 8 | yes | AuxStrip | ROUTING | Routing::muteAndSoloResolveTogether, NoiseFloor::aux8IsOptIn | 8 strips; Aux 8 now also carries the noise floor via the "AUX 8: + NOISE FLOOR" toggle (realism-c) |
| MIDI-out enable + source checkboxes + CC table | 8 | yes | RoutingPanel midiOut* toggles | ROUTING (also mirrored on MIDI OUT) | MidiOutPanel::liveSwitches... | |
| Latency readout per active output | 8 | yes | RoutingPanel latencyLabel | ROUTING | - | |
| Preset: aux mute/solo/gain, MIDI-out, sc-to-amp stored; layout not stored | 9 | yes | RoutingMatrix::toVar/fromVar | n/a | Routing::stateRoundTrips, loadingClearsPreviousState | |
| Test: latency measured with an impulse, within 1 sample | 10 | partial | - | n/a | Latency::anImpulseArrivesWhenReported, perOutputLatencyIsConsistent | **[changed]** an impulse is now measured through the sidechain-to-amp path (main out minus DI tap). Aux 2-7 and per-string outputs are still only ordering-checked |

### modulation-matrix.md

The engine side is complete: 8 LFOs, 4 envelopes, 2 step sequencers, 2 followers, note/CC/14-bit/random sources, compiled lock-free routes, the 8-per-destination limit, and discrete destinations. All five listed tests exist. The UI is thinner than the engine. Missing from the UI: the per-route **offset** column, LFO phase and custom breakpoints, envelope stage curves and loop mode, and the step sequencer's per-step grid. There are no per-source colour tags. Snapshot morph position is not a modulation destination. The macros are fixed-function with fixed names. Since the last audit: drag-to-modulate and the clamp fix have merged, and the MOD card sliders are now labelled. Offset, phase, curves, loop mode, step grid and colour tags are still missing. **Counts (recounted from the table): yes 28 / partial 9 / no 3.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Control rate = block/32, min 128 | 0.1 | yes | ModMatrix::prepare | n/a | - | |
| Linear interpolation between ticks | 0.2 | yes | ModMatrix offsets ramp | n/a | Modulation::routeModulatesItsDestination | |
| Additive over base, clamped | 0.3 | yes | ModMatrix::apply (finiteOrZero, clamps to param range) | n/a | routeModulatesItsDestination, ModelGapsUi::theSustainControlsAreModulationDestinations | **[changed]** the +-4 clamp bug fix is merged |
| Up to 8 sources per destination | 0.4 | yes | kMaxRoutesPerDestination | Modulate menu shows limit | destinationAcceptsEightSourcesAndNoMore | |
| Per-route depth and offset +-100% | 0.4, 3 | partial | ModRoute.depth/offset | depth typed in table; **no offset column** | presetRoundTripIsExact | offset reachable only through preset/state |
| Sources reset on preset load / snapshot / transport start | 0.5 | partial | ModMatrix::reset, resetEnvelopes; LFO Retrigger::onTransportStart | n/a | - | no explicit reset on snapshot recall |
| Free-running LFO option | 0.5 | yes | ModLfo::Retrigger::freeRun | MOD card retrigger box | ReviewRegression::aSyncedLfoWithTheTransportStoppedStillCycles | **[changed]** synced LFO with the transport stopped now free-runs (was frozen) |
| Serialized as compact route array under `modulation` | 0.6, 6 | yes | ModMatrix::toVar; processor "modulation" | n/a | presetRoundTripIsExact | |
| LFO x8, 8 shapes incl S&H, random smooth | 1.1 | yes | ModLfo | MOD card shape box | lfoFrequencyIsAccurate, sampleAndHoldHoldsForAWholeCycle | |
| LFO custom 8-point breakpoint editor | 1.1 | partial | ModLfo::setBreakpoint | no editor | - | engine-only |
| LFO rate 0.01-40 Hz / tempo sync incl dotted/triplet | 1.1 | yes | ModSyncDivision | MOD card rate/division/sync | syncedLfoFollowsTheHost | |
| LFO phase offset 0-360 | 1.1 | partial | ModLfo::setPhaseOffsetDegrees | no control | - | engine-only |
| LFO depth, symmetry, smoothing, uni/bipolar, retrigger modes | 1.1 | yes | ModLfo | MOD card | - | |
| Envelope x4 DAHDSR 0-30 s, sustain | 1.2 | yes | ModEnvelope | MOD card sliders | envelopeStageTimesAreAccurate | |
| Envelope curve per stage | 1.2 | partial | ModEnvelope::setStageCurve | no control | - | engine-only |
| Envelope retrigger legato/always/one-shot | 1.2 | yes | ModEnvelope::Retrigger | MOD card retrigger | - | |
| Envelope loop mode off/D-S/D-R | 1.2 | partial | ModEnvelope::setLoopMode | no control | - | engine-only |
| Step seq x2: length, grid, direction, swing, sync | 1.3 | yes | ModStepSequencer | MOD card length/swing/direction/division | stepSequencerWalksItsSteps | |
| Step seq per-step value/gate/slide/probability | 1.3 | partial | ModStepSequencer::setStep | no step grid | stepSequencerWalksItsSteps | engine-only; the user cannot draw steps |
| Envelope followers x2: source, attack, release, detection, threshold | 1.4 | yes | ModEnvelopeFollower | MOD card | envelopeFollowerTracksLevel | |
| Note pitch/velocity/trigger/held/AT/poly-AT sources | 1.5 | yes | ModSourceSlots 16-21 | selectable | sourceIdsResolveBothWays | |
| CC 0-127, 14-bit pairs, PB, mod wheel, ch pressure | 1.6 | yes | ModSourceSlots cc/cc14 | selectable | sourceIdsResolveBothWays | |
| Macros x8, user-named 32-char, host params "Macro 1..8" | 1.7 | partial | macro_attack..macro_assign_b | Easy/Advanced knobs | - | 6 of 8 are fixed-function (Attack, Body...); no naming |
| Macros modulatable by LFO | 1.7 | yes | macros are APVTS destinations | Modulate menu | - | |
| Random source per-note/per-bar/smooth, seeded | 1.8 | yes | ModRandomSource | selectable | randomSourcesAreDeterministic | |
| Every automatable param is a destination | 2 | yes | ModMatrix destination = param index | right-click Modulate | routeModulatesItsDestination | |
| Snapshot morph position as destination | 2 | no | SnapshotBank::setMorphPosition is not a parameter | no | - | only `preset_morph_position` exists |
| Discrete destinations: threshold rule, bypass at 0.5 | 4 | yes | ModMatrix discrete handling | n/a | discreteDestinationsStepAtBoundaries | |
| Curves Linear/Exp/Log/S/Custom | 3 | yes | ModCurve | table click cycles | curvesPreserveSignAndFixedPoints | Custom(name) not user-definable |
| Enabled toggle per route | 3 | yes | ModRoute.enabled | table "On" | UndoCoverage::modPanelRouteEditsAreEntries | **[changed]** route edits are undo entries |
| MOD tab in Column 4 | 5 | yes | ModMatrixPanel | Col 4 MOD | GuiReachability | stacked, not 1/3-2/3 (documented) |
| Routing table columns incl offset + delete | 5 | partial | ModRouteTable | MOD | - | no Offset column |
| Add-route button | 5 | yes | ModMatrixPanel addButton | MOD | - | |
| Drag source card onto control creates route | 5 | yes | ModSourceCard::mouseDrag / dragDescriptionFor; knob DragAndDropTarget | MOD card header -> any knob | DragToModulate::aDroppedSourceRoutesAt25PercentAsOneEntry | **[changed]** merged from visual; one undo entry |
| Right-click "Modulate" submenu | 5 | yes | Widgets.cpp kModulateMenuBase | any knob | Editor::rightClickOffersModulationAndBuildsTheRoute | |
| Depth arc around modulated controls | 5, 7 | yes | Widgets paint "modulation arc" | all AttachedKnobs | - | single colour (Palette::secondary) |
| Per-source user colour tags, routes/arcs inherit | 5 | no | - | no | - | |
| Snapshot "includes modulation" vs preset-level flag | 6 | no | captureSnapshot always stores modMatrix | no | - | always included |
| Import validates destination IDs, warns | 6 | yes | ModMatrix::fromVar unknown list | - | unknownDestinationsAreReportedNotFatal | no user-visible banner verified |
| Tests: source accuracy, 1000-route stress, round trip, discrete, determinism | 8 | yes | - | n/a | Modulation::* (17 tests) | |

### rhythm-engine.md

The rhythm engine is complete in function. It has the chord detector (84 templates), the voicer with 9 styles, strum and fingerpick scheduling, humanisation, bypass, free-run, all 29 GenreKits and the 7 fingerpick patterns. The RHYTHM tab and the Easy strip exist, and all six listed tests are present. Deviations: the factory patterns and kits are built in code, not stored as `Resources/Rhythm` / `Resources/Genres` JSON. The `rhythm_engine.enabled/state` values live in a preset blob, not as host parameters, so the rhythm on/off cannot be automated. Merged since the last audit: the bass kits with a bass step grid, and undo entries for rhythm edits. **Counts (recounted from the table): yes 25 / partial 3 / no 0.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| MIDI transformer between Interpreter and TechniqueEngine | 0.1, 1, 9 | yes | Rhythm/RhythmEngine | n/a | GenreKits::everyKitSoundsWhenApplied | |
| No allocation in callback (pre-sized buffers) | 0.2 | yes | RhythmEngine | n/a | - | not separately tested |
| Sample-accurate to host transport | 0.3 | yes | RhythmEngine + playhead | n/a | RhythmPatterns::strumSchedulingIsSampleAccurate | |
| Silent when stopped unless Free-run | 0.4 | yes | RhythmEngine free-run | RHYTHM FREE-RUN | RhythmPatterns::silentWhenStoppedUnlessFreeRunning | |
| Quantized on read, humanized on write (HumanizeMatrix) | 0.5 | yes | RhythmEngine | n/a | humanisationIsDeterministic | |
| Own bypass, reverts within one block | 0.6 | yes | RhythmEngine::setEnabled | RHYTHM enable, Easy switch | bypassIsCleanAndImmediate | |
| ChordDetector: 84 templates, bass/slash, confidence<0.6 Unknown, 30 ms burst | 2 | yes | ChordDetector | indicator | Rhythm::chordDetectorRoundTrips..., lowConfidence..., burstWindow... | |
| ChordVoicer constraints + score + 9 styles | 3 | yes | ChordVoicer | style box | voicerHandlesEveryChordOnEveryGuitar, voicingStylesProduceDifferentVoicings | |
| voicing_density, hand_position_hint updated from previous chord | 3 | yes | RhythmEngine | density/hand sliders | - | |
| Strum types Down/Up/mutes/Rake/Rasgueado | 4 | yes | StrumType | strum grid | everyFactoryStrumPatternSounds | |
| Per-string dynamic falloff by strum_evenness | 4 | yes | RhythmEngine/StrumGesture | STRUM group | - | |
| string_mask per step | 4 | yes | Pattern step mask | grid right-click Mask | factoryMasksSelectStringsASixStringHas | |
| 16/32 steps, triplet/dotted | 4 | yes | Patterns | grid | patternsRoundTripThroughJson | |
| Humanize timing/velocity/miss/ghost | 4 | yes | RhythmEngine | feel sliders | humanisationIsDeterministic | |
| Fingerpick p/i/m/a/e with right-hand profiles | 5 | yes | Patterns fingerpick | FingerpickGrid | - | |
| 7 factory fingerpick patterns | 5 | yes | PatternLibrary::addFactoryPatterns | pattern list | factoryPatternsAreWellFormed | |
| .luthierpattern JSON, factory in Resources/Rhythm, user in ~/Documents/Luthier/Rhythm | 6 | partial | PatternLibrary | LOAD/SAVE/EXPORT | patternsRoundTripThroughJson | factory patterns are in code; Resources/Rhythm does not exist |
| 29 GenreKits (listed) | 7 | yes | GenreKitLibrary (29 + bass kits) | genre box | factoryKitsAreWellFormed, BassTechniques::theBassKitsInstallTheirGrids, SampleContent::everyGenreKitSuggestsATempoAFeelAndAPalette | **[changed]** bass kits with BassStepGrid (BassGridGroup on RHYTHM, bass only) merged |
| Kits as JSON under Resources/Genres/ | 7 | partial | in code; user dir scanned | - | kitsRoundTripThroughJson | |
| Kit rig as soft reference | 7 | yes | rigHintLabel | RHYTHM | applyingAKitDoesNotLoadItsRig | |
| UI: genre + dice, voicing, capo +/- | 8 | yes | RhythmPanel | Col 4 RHYTHM | GenreKits::capoRemovesFrets... | |
| Strum editor 16/32 grid, right-click dynamic/mask/delete | 8 | yes | StrumGrid | RHYTHM | - | |
| Fingerpick editor 5 rows | 8 | yes | FingerpickGrid | RHYTHM | - | |
| Feel: swing, timing, velocity, miss, ghost | 8 | yes | RhythmPanel sliders | RHYTHM | - | |
| Pattern browser tag filter/load/save/export | 8 | yes | patternList, tagFilterBox | RHYTHM | - | |
| Live indicators: chord, fretboard dots, next-strum light | 8 | yes | RhythmIndicators | RHYTHM | - | |
| Easy strip: kit, feel knob, on/off, Mono hint | 8 | yes | EasyPanel::buildRhythmStrip | Easy | EasyLayout tests | |
| Preset params `rhythm_engine.enabled` / `.state` | 9 | partial | "rhythm" blob in state | - | UndoCoverage::rhythmSettingsAndPatternEditsAreEntries | still not host-automatable parameters (no rhythm entry in Parameters.h). The Mute Row on the RHYTHM grid is in progress on techniques (muting-rhythm, not this spec) |

### live-performance.md

The engines are complete and every listed test exists: snapshot bank, crossfade, morph with curves and exclusions, setlist with preload, tap tempo, kill switch and monitor mix. The keyboard map is done ([ ] in Live Mode, 1-9 and Shift+digit, PageUp/PageDown, \, P, T). The main gaps:
- **Expression calibration does not work.** The heel/toe wizard has UI, but `ExpressionCalibrationSet::observe()` is never fed incoming CCs, and `map()` is never applied to incoming CCs.
- Live actions (next/previous snapshot, snapshot by CC, kill, tap, setlist navigation) cannot be assigned to a CC. MIDI Learn is parameter-only.
- Snapshot morph cannot be driven by a pedal CC, LFO or mod wheel, because it is not a parameter.
- Morph exclusions and Bezier points have no UI.
- Monitor pan and EQ have no UI.
- There is no per-preset or global MIDI-learn scope.

New since the last audit: snapshot recall is applied from the 30 Hz message timer (review fix), so crossfades are stepped. Snapshot and setlist edits are undoable. None of the gaps above were closed. **Counts (recounted from the table): yes 24 / partial 10 / no 4.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Every live control reachable without a mouse (key, CC, PC) | 0.1 | partial | keyPressed; handleLiveMidi | keys yes; CC no | - | only PC/CC0 hard-wired; no CC-assign for live actions |
| Glitch-free snapshot switching | 0.2, 1 | yes | SnapshotBank crossfade | n/a | LiveSnapshots::thousandRecallsNeverJumpAParameter, ReviewRegression::integerParametersSwitchRatherThanBlend | **[changed]** (review merge) the recall work moved to the message thread. The audio thread only calls noteAudioTime, and the 30 Hz timer calls advancePending -> applyBlend. The crossfade therefore advances in ~33 ms steps (the default 30 ms fade is about one step, relying on parameter smoothing), and a PC recall waits for the next timer tick or a stalled message thread. This is a thread-safety fix, but it weakens the spec 0.2 timing |
| Kill within one block, 3 ms fades | 0.4, 6 | yes | KillSwitch | LiveStrip KILL pill, key \ | LiveKillSwitch::* (3) | key toggles (not momentary), documented |
| No modal dialogs on live surface | 0.5 | yes | LiveStrip | - | - | rename AlertWindow is on the LIVE setup tab only |
| 128 snapshots: params, mod, bypasses, rhythm, 32-char label, 16 colours | 1 | yes | Snapshots.h kMaxSnapshots/kMaxLabelLength/kNumColourTags; captureSnapshot | LIVE tab grid | captureAndRecallRoundTrip, bankRoundTripsThroughJson | |
| Crossfade `snapshot_xfade_ms` 0-500, default 30 | 1 | yes | SnapshotBank::setCrossfadeMs | LIVE crossfade slider | recallCrossfadesContinuousAndStepsDiscrete | |
| Bypass at midpoint; tails preserved; coupling on worker | 1 | partial | SnapshotBank | n/a | - | discrete switching tested; tail double-buffer / coupling worker not verified |
| Snapshots stored in preset `snapshots` | 1, 11 | yes | processor state "snapshots" | n/a | bankRoundTripsThroughJson | |
| PC = snapshot index; CC0 selects preset | 2 | yes | handleLiveMidi | n/a | programChangeMapsAcrossAllOneTwentyEight | |
| CC assign: next/previous/by-value snapshot | 2 | no | - | no | - | MidiLearn maps CC -> parameter only |
| `[` `]` step, 1-9, Shift+digit | 2 | yes | PluginEditor::keyPressed | keys | Undo::snapshotSaveAndRecallAreEntries | [ ] step snapshots only in Live Mode; **[changed]** capture/recall/rename/delete now go through *AsUserAction (undoable) |
| On-screen snapshot strip (8 + prev/next) | 2, 10 | yes | SnapshotStrip | LiveStrip | Editor::theLiveTab... | |
| Foot controller via MIDI Learn | 2 | partial | MidiLearnManager | right-click Learn | - | params only |
| Morph A/B, knob, enable | 3 | yes | SnapshotBank morph | LiveStrip MORPH/A/B/knob | morphFollowsItsCurve | |
| Morph curve linear/S/exp/Bezier | 3 | partial | MorphCurve; setBezierControlPoints | LIVE morphCurveBox | morphFollowsItsCurve | Bezier points have no UI |
| Discrete params switch at 0.5; per-param exclusion | 3 | partial | SnapshotBank::setParameterExcludedFromMorph | no exclusion UI | morphHonoursExclusionsAndDiscreteSwitching | exclusions engine-only |
| Morph driven by pedal CC / LFO / mod wheel / sidechain env | 3 | no | - | no | - | snapshot morph is not a parameter or mod destination |
| Setlist `.luthierset` in ~/Documents/Luthier/Setlists | 4 | yes | Setlist; FileOpenRouter (standalone open) | LIVE setlist list, triptych | roundTripsThroughJson, UndoCoverage::setlistEditsAreEntries, SampleContent::theTenExampleSetlistsInstallOnceOverTheFactoryBank | **[changed]** 10 example setlists ship; setlist edits undoable |
| PageUp/PageDown, CC navigation | 4 | partial | keyPressed setlistPrevious/Next | keys | - | no CC assignment |
| Header triptych prev/current/next | 4 | yes | SetlistTriptych | LiveStrip | reportsPreviousCurrentAndNext | |
| Preload next entry | 4 | yes | SetlistPlayer::preloadNext | n/a | walksForwardsAndBackwardsWithoutGrowing | |
| Tap: 4 taps/3 s, median, outliers, 20-300, snap | 5 | yes | TapTempo | TapPad, key T | LiveTapTempo::* (4) | |
| Tap drives rhythm (host stopped), synced LFO, synced delay | 5 | partial | blockTempo = tapTempo.getEffectiveBpm | - | respectsRangeSnapAndHostPriority | metronome does not follow tap (see practice) |
| Beat LED blink in tap accent | 5 | yes | TapPad blink | LiveStrip | - | the pad blinks; the header LED does not |
| Host tempo wins unless "internal tempo" forced | 5 | yes | TapTempo::getEffectiveBpm | Options | respectsRangeSnapAndHostPriority | |
| Kill default key `\`, red pill | 6 | yes | Accessibility "killSwitch" | LiveStrip | - | |
| Monitor: main + sidechain sum, level | 7 | yes | MonitorMix | LiveStrip monitor level | LiveMonitor::idle... | |
| Monitor pan + 3-band EQ | 7 | partial | MonitorMix::setPan/setEq*Db | no controls | - | engine-only |
| Click bus to monitor only | 7 | yes | processSlice clickBuffer | Practice METRO "main out" toggle | - | |
| Monitor to 2nd output pair; bypass when idle | 7 | yes | RoutingMatrix::writeMonitorBus (Aux 7) | ROUTING | LiveMonitor::idle... | |
| Expression calibration wizard heel/toe, dead-zones, curve | 8 | partial | ExpressionCalibrationSet; ExpressionPage | Options -> EXPRESSION | LiveExpression::* (4) | **observe() never called: the wizard never sees the pedal** |
| Calibration applied (0-1 remap) to incoming CC | 8 | no | map() never called on MIDI path | - | - | calibration has no effect |
| Calibrations global in config/expression.json | 8, 11 | yes | getConfigFile | - | calibrationSetRoundTrips | |
| Panic: notes off, tails, DC, coupling; keeps preset | 9 | yes | LuthierAudioProcessor::panic -> engine.panic | header, key P | - | |
| Live strip contents | 10 | yes | LiveStrip | Live Mode | Editor::theLiveTab... | |
| Live Mode: 44 px targets, suppress tooltips, lock Advanced | 10 | partial | LiveStrip kTouchTargetHeight; updateLiveStripVisibility | header LIVE | - | Advanced locked (forces Easy); tooltip suppression not found |
| Live-mode on/off per preset | 11 | yes | state "liveMode" | - | - | |
| MIDI-learn per-preset default, "global" flag | 11 | no | MidiLearn stored in plugin state only | - | - | |

### controllers.md

The profile model, the JSON format, 9 built-in profiles, user overrides, the per-channel mapping, dead-zone and minimum note duration are implemented. The CONTROLLERS UI exists (Col 4, moved from Options). Much of it goes nowhere:
- **Latency compensation is never applied** to MIDI.
- The latency wizard never receives notes: `addMeasurement` is never called outside tests, so it never finishes.
- The LinnStrument rows-as-strings and Osmose pitch-curve settings are never applied.
- `ControllerMerge` is unused.
- **Bug:** `ParameterBridge::applyToEngine` rewrites `setMpeEnabled` and `setPitchBendRange` from the `mpe_enabled`/`bend_range` params every block. This overwrites an MPE profile's MPE flag and 48-semitone bend range.
- The profile selection is not persisted.

**No change since the last audit.** Source/Controllers/ is untouched, and Parameters.cpp:1339-1340 (`interp.setMpeEnabled` / `setPitchBendRange` in applyToEngine) still overwrites the profile. **Counts (recounted from the table): yes 8 / partial 9 / no 4.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Profiles from Resources/Controllers/*.json | 0.1 | partial | ControllerProfileLibrary::addFactoryProfiles | - | everyShipProfileIsWellFormed | built in code; the folder does not exist |
| User picks profile, default Generic MIDI | 0.1 | partial | ControllersPage::applySelectedProfile | Col 4 CONTROLLERS (and Options) | applyingAProfileConfiguresTheInterpreter | selection not saved in state; resets to item 1 |
| Profile declares mode, bend, latency, cc map | 0.2 | yes | ControllerProfile | readout | parsesTheSpecExampleProfile | |
| Latency compensation on MIDI input path | 0.3, 3 | no | getEffectiveLatencyMs only displayed | slider (no effect) | measuredLatencyOverridesTheBudget (data only) | |
| User overrides in ~/Documents/Luthier/Controllers win | 0.4 | yes | scanDirectory/save | SAVE | savingAProfileReplacesTheOneOfTheSameId | |
| 9 ship profiles (Generic, MPE, Seaboard, Linn, Osmose, GK, TriplePlay, Jamstik, EZ-EG) | 1 | yes | addFactoryProfiles | profile box | everyShipProfileIsWellFormed | |
| Seaboard CC74->whammy, AT->vibrato | 1 | yes | ccMap | - | everyCcMappingResolvesToARealTarget | |
| LinnStrument "Guitar mode" rows->strings | 1 | partial | rowsAsStrings stored | toggle | - | never applied to the interpreter |
| Osmose pitch-curve LUT | 1 | partial | applyPitchCurve | - | osmosePitchCurveIsMonotonicAndPinned | never called in the MIDI path |
| JSON file format | 2 | yes | toVar/fromVar | - | profilesRoundTripThroughJson | |
| Latency wizard (click, play along, store measured) | 3 | partial | LatencyWizard; ControllersPage::runWizardStep | "Measure latency" | latencyWizardIsStableAndReportsItsScatter | addMeasurement never fed notes; cannot complete |
| >1 block interpolate, >2 blocks warn | 3 | no | - | - | - | |
| Per-channel: channel->string, bend per string, pressure->vibrato | 4 | yes | MidiInterpreter channel map | - | perChannelRoutingSendsEachChannelToItsString | |
| Unreachable pitch dropped with diagnostic log | 4 | partial | - | - | - | not verified |
| MPE: master notes ignored, sticky string per member channel | 4 | partial | MidiInterpreter | - | - | no stickiness logic or test found |
| Profile MPE flag / member bend range take effect | 1, 4 | no | ControllerProfileLibrary::apply | - | applyingAProfile... (interpreter alone) | **overwritten every block by ParameterBridge::applyToEngine (now Parameters.cpp:1339-1340)** |
| Calibration: bend range confirm via UI | 5 | no | - | - | - | |
| Calibration: minimum note duration | 5 | yes | setMinimumNoteDurationMs | slider | minimumNoteDurationSurvivesAnEarlyNoteOff | |
| Calibration: pitch dead-zone (default 5) | 5 | yes | setPitchDeadZoneCents | slider | pitchDeadZoneRejectsTrackingNoiseButNotRealBends | |
| Multi-controller merge, source tagging, contention warning | 6 | partial | ControllerMerge | - | multiControllerMergeNeverLosesAString | class unused in processBlock; no source tagging |
| Test: MPE stickiness | 7 | partial | - | - | - | missing |

### practice-tools.md

The drawer (8 tabs, 32-360 px, strip readouts, practice level, tap, panic) and the PRACTICE setup tab (progress, routines, defaults, library, session setup) are complete and well tested. Main gaps:
- **Backing-track pitch shift and tempo shift do nothing.** Their sliders only store values that the renderer never reads.
- MP3 is not enabled.
- The **scale-trainer quiz cannot be answered by playing**: `ScaleTrainer::answer` is never fed MIDI.
- The tab reader reads ASCII and MusicXML only (no GP5/GP/PTB) and shows static text: no cursor, tempo, count-in, fretboard highlight or scoring.
- The metronome ignores host and tap tempo.
- The click WAVs are absent (the clicks are synthesized).
- Looper and backing-track low/high cut and backing-track pan have no UI.
- Loop re-render on tone change is not implemented.

Closed since the last audit: the session-recorder audio/MIDI/auto-save setup, recorder MIDI capture with drag-out, looper default length, trainer ranges, and undo on practice edits. Everything in the list above is still open. **Counts (recounted from the table): yes 40 / partial 13 / no 6.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Isolated panel, zero CPU when closed | 0.1 | yes | processSlice `practicePanelOpen` gate | drawer | - | |
| Click to monitor by default, optional main | 0.2 | yes | setClickToMain | METRO toggle | - | |
| Looper re-renders stored MIDI through new tone | 0.3 | partial | Looper::captureMidi -> lock-free FIFO -> drainPendingMidi | - | PracticeLooper::recordingMidiDoesNotAllocate | MIDI is now captured without allocating, but still nothing re-renders it (Looper.h:12 says "the re-render is the caller's", and there is no caller). The Looper can now import a rendered layer (loadLayerAudio / importLayer: MIDI import, whole-tune capture) |
| Backing tracks stream from disk | 0.4 | yes | BackingTrackPlayer (BufferingAudioSource) | TRACK | - | |
| Metronome time sigs + custom | 1 | yes | Metronome::setTimeSignature | METRO | settingsRoundTrip | |
| Tempo 20-300, follows tap + host tempo | 1 | partial | Metronome::setTempo | METRO slider | ReviewRegression::theMetronomeStartsOnBeatOne | still never follows tap or host tempo (setTempo is called only by routines and the Options reset). **[changed]** the beat-one skip bug is fixed |
| Accent map, 3 levels (+silent) | 1 | yes | setBeatAccent | METRO beat buttons | accentPatternIsObeyed | |
| Subdivisions with own gain | 1 | yes | setSubdivision/setSubdivisionLevelDb | METRO | - | |
| 6 click sounds | 1 | yes | ClickSound | METRO sound box | everyClickSoundIsAudibleAndFinite | |
| 16 click WAVs in Resources/Practice/Clicks | 1, 10 | no | synthesized | - | - | folder does not exist |
| Silent bars every N | 1 | yes | setSilentBarPeriod | METRO | silentBarsMuteTheClick... | |
| Progressive tempo A->B over N bars | 1 | yes | startProgressiveTempo | METRO | progressiveTempoRampsAndStops | |
| 4-dot visual | 1 | yes | beat indicator paint | METRO | - | |
| Loop length 1-240 s, bar-quantized | 2 | yes | Looper | LOOP | loopLengthQuantisesToBars | |
| 8 layers, MIDI + audio | 2 | yes | Looper kMaxLayers | LOOP | recordsClosesAndOverdubs | |
| Undo/redo per layer | 2 | yes | LoopLayer undo (depth 1); LooperTab::editLayer | LOOP | layerUndoAndRedo, Undo::aClearedLoopLayerComesBack, UndoCoverage::practiceScaleAndLooperLayerAreEntries | **[changed]** a clear is restorable; layer edits go on the global undo stack |
| Reverse / half-speed per layer | 2 | yes | LoopLayer | LOOP | reverseAndHalfSpeedDoNotAlterTheRecording | |
| Overdub / replace / play-once modes | 2 | yes | LayerMode | LOOP mode box | - | |
| Per-layer volume, pan | 2 | yes | LoopLayer | LOOP | mutedLayersAreSilent | |
| Per-layer low-cut / high-cut | 2 | partial | LoopLayer::setLowCutHz/setHighCutHz | no controls | - | engine-only |
| Export mixdown / stems | 2 | yes | exportMixdown/exportStems | LOOP | - | |
| .luthierloop save/load | 2 | yes | Looper::save/load | LOOP | - | |
| Temp-folder audio snapshots moved on save | 2 | partial | - | - | - | not verified |
| Formats WAV/AIFF/FLAC/MP3 (dr_mp3) | 3 | partial | registerBasicFormats | TRACK Load | - | MP3 still not enabled. The TRACK chooser (now PracticePanel.cpp:694) still offers *.mp3. A track load is now an undo entry |
| 4-s ring per file | 3 | yes | kRingBufferSeconds | - | - | |
| Volume, mono | 3 | yes | setLevelDb/setMonoSum | TRACK | - | |
| Pan, low-cut, high-cut | 3 | partial | setPan/setLowCutHz/setHighCutHz | no controls | - | engine-only |
| Loop points, zero-crossing snap | 3 | yes | setLoopSeconds/snapToZeroCrossing | TRACK set start/end | - | |
| Pitch shift +-12 st, no tempo change | 3 | no | setPitchShiftSemitones stores only | TRACK slider (no effect) | - | the renderer never reads pitchSemis |
| Tempo 25-200 %, no pitch change | 3 | no | setTempoRatio stores only | TRACK slider (no effect) | - | the renderer never reads tempoRatio (routines also set it) |
| Section markers, jump hotkeys, names | 3 | partial | addMarker/jumpToMarker | TRACK add/jump | - | no hotkeys; markers are unnamed |
| Auto-detect tempo on load | 3 | yes | estimateTempo | TRACK readout | - | |
| Playlist, gapless | 3 | partial | setPlaylist | no UI | - | not gapless; nothing sets it |
| Scale trainer Explore + overlays | 4 | yes | ScaleTrainer | SCALE | scaleTrainerKnowsItsScales | |
| Quiz: detects played note via MIDI, scores | 4 | partial | ScaleTrainer::answer | SCALE | scaleQuizScoresAnswers | **answer() never fed played MIDI** |
| Interval / chord-tone trainer modes | 4 | partial | Mode enum | SCALE mode box | - | the same missing MIDI input |
| All diatonic modes, harmonic/melodic minor, pentatonic, blues, custom | 4 | yes | ScaleType, setCustomIntervals | SCALE | - | custom interval UI not verified |
| Ear training intervals/chords/progressions, adaptive, stats.json | 5 | yes | EarTrainer | EAR | earTrainer* (3) | |
| Uses Luthier's own guitar sound | 5 | yes | audition | EAR | - | |
| Tab reader formats GP5/GP/ASCII/MusicXML/PTB | 6 | partial | NotationImporter (ASCII, MusicXML) | TAB Open | Notation::importerIsHonestAboutWhatItReads | GP5/GP/PTB refused |
| Scrolling tab view, cursor, tempo, section loop, count-in | 6 | partial | TabReaderTab (static TextEditor) | TAB | - | no cursor, playback or count-in |
| Highlight current fret on fretboard | 6 | no | - | - | - | |
| Speed trainer | 6 | partial | SpeedTrainer (routines) | not in TAB | theSpeedTrainerClimbsUntilAPassMissesNotes | |
| Play-along note detection + scoring | 6 | no | - | - | - | |
| Chord progression looper (symbols, voiced, strummed) | 7 | yes | ProgressionLooper | PROG | progressionParsing | |
| Session recorder ring, default 60 min | 8 | yes | SessionRecorder | SESSION | ringBufferNeverGrows | |
| Save last take WAV + MIDI | 8 | yes | SessionRecorder::saveLastTake, drainMidi (lock-free FIFO) | SESSION Save (drag-out, Alt = Generic) | PracticeGaps::theSessionRecorderTakesMidiWithoutAllocating, theSaveButtonDragsTheSavedTakeOut | **[changed]** recorder MIDI capture merged |
| Temp dir, 24 h cleanup, off by default, enabled in Options | 8 | yes | cleanUpOldTempFiles; OptionsPages recorderToggle | Options | disabledByDefault | |
| Drawer 32-360 px resizable, collapsed strip readouts | 9 | yes | PracticePanel | drawer | PracticeDrawer::* | |
| Tabs METRO/LOOP/TRACK/SCALE/EAR/TAB/PROG/SESSION | 9 | yes | PracticePanel tabs | drawer | PracticeRoutine::toolsAreInTheDrawersTabOrder | |
| Drawer master volume, tap, panic | 9 | yes | practiceLevel, tapButton, panicButton | drawer | - | |
| 11.2 Progress (90 days, per tool, accuracy, tempo, streak, CSV, clear) | 11 | yes | PracticeStats; PracticeSetupPanel | Col 4 PRACTICE | progressReportsDaysStreaksAccuracyAndTempo, clearHistory... | |
| 11.2 Routines (3 factory, drive drawer, saved JSON) | 11 | yes | PracticeRoutineLibrary/Runner | PRACTICE | theThreeFactoryRoutines..., aRoutineDrivesTheDrawer... | |
| 11.2 Defaults per tool | 11 | yes | PracticeDefaults; Looper::setDefaultLengthSamples; trainer ranges | PRACTICE | defaultsSurviveAReopen..., PracticeGaps::theLooperClosesItsFirstLoopAtTheDefaultLength, theTrainersKeepToTheirRangeAndSessionLength | **[changed]** merged |
| 11.2 Library (loops, sessions, backing folder, recent tabs) | 11 | yes | PracticeLibrary | PRACTICE | libraryListsLoadsAndDeletes... | |
| 11.2 Session setup (ring length, audio/MIDI, auto-save, size warning) | 11 | yes | SessionRecorderSetup; SessionRecorder::setRecordAudio/setRecordMidi/setAutoSaveOnStop/stop | PRACTICE | sessionSetupStatesTheRingSize..., PracticeGaps::theSessionRecorderRecordsWhatItIsToldTo, stoppingTheRecorderSavesWhenAutoSaveIsOn | **[changed]** partial -> yes |
| 11.3 No transport on tab / 11.4 empty states | 11 | yes | - | - | theTabExposesNoTransport, emptyStatesReadAsTheSpecWritesThem | |
| Tests: metronome accuracy; session no-alloc | 12 | yes | - | - | interClickInterval..., ringBufferNeverGrows | |
| Tests: looper -80 dB null; 60-min stream memory; 50 GP files | 12 | no | - | - | - | still missing. Memory::sixtyMinuteSessionNoMonotonicGrowth covers the engine, not backing-track streaming |

### tone-match.md

The IR slots (body plus 2 cab), windowed-sinc resampling, cab-match deconvolution with three test signals, EQ-match fitting, the capture utility, the library folders, sidecar metadata, relative preset paths and the missing-IR banner are all implemented. The TONE MATCH tab exists. Gaps:
- **The EQ-match result is loaded into cab IR slot 2**, replacing mic 2, instead of being inserted as a filter at pre-amp / post-amp / post-master.
- The match analysis runs on the message thread (spec: worker thread).
- The capture sources are main and sidechain only.
- IR start/end trim has no UI.
- There is no recent-IRs list and no library search.
- Three of the five listed tests are missing.

**No functional change since the last audit.** EqMatch still loads into `getCabIrSlot (1)` (ToneMatchPanel.cpp:590), and `Capture::Source` is still {mainOut, sidechain}. Only undo, auto-trim and installer-path fixes landed. **Counts (recounted from the table): yes 18 / partial 5 / no 6.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| IR loaded on message thread, lock-free swap | 0.1 | yes | IrSlot::load | - | anEmptySlotLeavesTheAudioAlone | |
| Windowed-sinc resample, >=512 taps | 0.2 | yes | IrSlot::resample | - | - | no resampling-accuracy test |
| Truncate at max_ir_seconds (4 s) | 0.3 | yes | IrSlot::setMaxSeconds | - | - | |
| No extra latency for user IRs | 0.4 | yes | - | - | - | not tested |
| Match analysis on worker thread | 0.5 | no | MatchWizard step handler (message thread) | - | - | EqMatch::fit / deconvolve run inline |
| Body IR slot + 2 cab slots | 1 | yes | processor getBodyIrSlot/getCabIrSlot | TONE MATCH slot cards | irSlotSettingsRoundTrip | |
| Slot: file, channel/auto-sum, gain +-24, predelay, reverse, mix | 1 | yes | IrSlotEditor (pushIrEdit) | TONE MATCH | irSlotSettingsRoundTrip, UndoCoverage::irSlotSettingsAreEntries, IrReload::* | **[changed]** slot edits undoable; IR reload onset fix |
| Slot: start/end trim | 1 | partial | IrSlot::setStartTrim/setEndTrim | no controls | irSlotSettingsRoundTrip | engine-only |
| Browser at ~/Documents/Luthier/IRs + OS drag-drop | 1 | yes | IrSlotEditor FileDragAndDropTarget | slot card | - | |
| Recent IRs (last 20) | 1 | no | - | - | - | |
| Cab match workflow, sweep/MLS/burst signals | 2 | yes | CabMatch | TONE MATCH wizard | everyTestSignalIsWellFormed | |
| Deconvolve, trim, Hann fade, save to IRs/Cab Match, auto-load | 2 | yes | CabMatch::saveIr; MatchWizard | wizard | sweepDeconvolutionRecoversTheSourceIr | |
| Progress, tail length, null result | 2 | yes | MatchWizard resultLabel | wizard | nullMeasurementIsCorrect | |
| EQ match: reference by file drag or sidechain loop | 3 | partial | MatchWizard (sidechain capture) | wizard | - | dragging a reference file onto EQ Match not found |
| EQ match: min-phase FIR 256/1024/4096 | 3 | yes | EqMatch::fit | length box | everyFilterLengthProducesAFilter | |
| Aggressiveness, preserve dynamics | 3 | yes | EqMatch::Options | wizard | eqMatchRespectsItsBandAndOptions | |
| Match band selection | 3 | partial | Options.lowHz/highHz | no control | eqMatchRespectsItsBandAndOptions | engine-only |
| Output filter at pre-amp/post-amp/post-master, saved per preset | 3 | no | loads into cab slot 2 | - | - | overwrites the mic-2 IR slot instead |
| Labelled "not a substitute for cab match" | 3 | yes | EqMatch::getDescription | wizard | eqMatchSaysWhatItCannotDo | |
| Capture from main/DI/sidechain/per-string | 4 | partial | Capture::Source {mainOut, sidechain} | capture pane | captureRecordsAndStops | no DI/per-string source |
| Capture 100 ms-60 s, WAV 32f to Captures/, autotrim | 4 | yes | Capture | capture pane | captureAutoTrimsSilence, ReviewRegression::autoTrimTrimsTheTailWithoutLeadingSilence | **[changed]** tail trim without leading silence fixed |
| IR library folder convention | 5 | yes | IrLibraryPaths; IrLibrary::getCandidateFolders | - | IrLibrary::candidatesIncludeTheInstallerLayout | **[changed]** installer factory paths searched |
| Sidecar .json, filename fallback | 5 | yes | IrMetadata | - | metadataRoundTripsAndFallsBackToTheFilename | |
| TONE MATCH tab: slots, wizards, capture, library with tag filter + search | 6 | partial | ToneMatchPanel | Col 4 TONE MATCH | - | tag filter yes; search no |
| Preset IR paths relative/absolute; missing-IR banner | 7 | yes | IrLibraryPaths; PluginEditor "ir-missing" | header banner | presetPathsAreRelativeUnderTheLibraryRoot | |
| Test: 100 IRs resampled within 0.5 dB | 8 | no | - | - | - | |
| Test: EQ fits shelf/bell/notch within 1 dB | 8 | yes | - | - | eqMatchFitsKnownCurves | |
| Test: 60 s capture nulls vs offline render | 8 | no | - | - | - | |
| Test: sample-rate change during IR playback | 8 | no | - | - | - | |

### notation-export.md

`PerformanceScore`, `PerformanceCapture` (8192-slot lock-free ring, drop-oldest, off/rolling/armed states, quarter-note or seconds timing) and the MusicXML / GP8 / ASCII / MIDI writers are implemented. The NOTATION tab has capture state, live tab, export and preview; the TAB drawer tab and File-menu export also exist. **[changed]** Everything that was in progress on model-gaps is now merged and tested:
- chord symbols from the detector (`<harmony>`)
- technique, BASS_TECH and slide events fed into the capture
- Mono-mode offline chord extraction
- current-bar fretboard dots
- marked-region and current-section ranges
- export on a worker thread
 Still missing: a Guitar Pro importer (so no GP round trip), grace notes, the 100-progression chord test, and a MuseScore fixture. **Counts (recounted from the table): yes 24 / partial 3 / no 2.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Export offline on worker thread | 0.1 | yes | NotationPanel / HeaderBar -> writeAsync | NOTATION, File menu | ModelGapsUi::notationExportRunsOnAWorkerThread | **[changed]** merged |
| Guitar-aware string/fret | 0.2 | yes | PerformanceScore, ScoreNote | - | Notation::stringAndFretAreNotDerivedFromPitch | |
| Technique metadata preserved | 0.3 | yes | LuthierEngine::captureBlockState -> PerformanceCapture | - | Capture::techniquesBendsChordsAndMetersReachTheScore, ModelGapsUi::theCaptureHearsTechniquesAndChordsFromTheEngine | **[changed]** merged |
| Round-trip per format | 0.4 | partial | NotationImporter | - | musicXmlRoundTrips, asciiTabRoundTrips | GP cannot be re-imported |
| PerformanceScore data model | 1 | yes | Notation/PerformanceScore | - | captureBuildsMeasures | |
| Capture last N minutes (default 10) | 1 | yes | PerformanceCapture rolling | NOTATION rollingMinutes | Capture::rollingKeepsTheLastMinutes | |
| MusicXML 4.0 technical elements | 2.1 | yes | NotationExporter | format box | musicXmlIsWellFormedAndGuitarAware | |
| MusicXML chord symbols (Poly) | 2.1, 4 | yes | NotationExport.cpp `<harmony>` from the capture chord track | - | ModelGapsUi::theCaptureHearsTechniquesAndChordsFromTheEngine | **[changed]** merged |
| Grace notes / multi-voice | 2.1 | partial | NotationExport `<voice>` | - | - | multi-voice written; no `<grace>` anywhere in NotationExport.cpp |
| Whammy as text directions | 2.1 | yes | formatsDeclareTheirLosses | - | formatsDeclareTheirLosses | |
| Guitar Pro 8 .gp bundle, techniques, chord diagrams | 2.2 | yes | NotationExporter GPIF zip | format box + option | guitarProBundleIsAValidZip | fidelity checked only for validity |
| ASCII tab: 6 lines, ruler, symbols, width 80, headings | 2.3 | yes | renderAsciiTab | lineWidth slider | asciiTabColumnsAlign, asciiTabUsesTheSpecifiedSymbols | |
| MIDI per-string tracks, RPN hints, bend, CC68 | 2.4 | yes | NotationExporter MIDI | format box | midiExportIsPerString | |
| Live TAB view in practice panel, last N beats | 3 | yes | TabReaderTab / NotationPanel tabView | drawer TAB, Col 4 NOTATION | Notation::liveTabWindowRendersASlice, Capture::theLiveTabShowsWhatWasPlayed | |
| Current bar as fretboard tab dots | 3 | yes | FretboardComponent::refreshTabDots | fretboard | ModelGapsUi::theCurrentBarIsDrawnOnTheFretboardAsTabDots | **[changed]** merged |
| Controls: show/hide, scroll speed, bars 1-8, density | 3 | yes | NotationPanel speedBox/barsBox/densityBox | NOTATION | NotationTab::stateButtonsLiveTabAndPreview | |
| Poly chord symbols at change points | 4 | yes | capture chord track (ChordDetector) | - | ModelGapsUi::theCaptureHearsTechniquesAndChordsFromTheEngine | **[changed]** merged |
| Mono offline chord extraction | 4 | yes | PerformanceCapture::extractChordsWhenMissing | - | ModelGaps::aMonoTakeGetsItsChordsOffline | **[changed]** merged |
| File menu Export -> Notation | 5 | yes | HeaderBar File menu | header | - | |
| Dialog: format, range, options, destination, preview | 5 | yes | NotationPanel; UI/CaptureRanges | NOTATION | NotationTab::exportsEveryFormat, CaptureRanges::theMarkedRegionIsWhatWasPlayedBetweenTheMarks, theCurrentSectionIsTheTunesSelectedSection | **[changed]** marked-region and current-section ranges merged; the tune export dialog also writes notation (TuneIntegration::notationAndProjectExportKeepSectionsChordsAndTheBundle) |
| "Export to Notation" beside Save last take | 5 | yes | HeaderBar (notation-export 5 comment) | header | - | |
| Capture records voiced notes, not MIDI | 6.1 | yes | PerformanceCapture | - | Capture::recordsVoicedNotesNotMidi | |
| Chord / tempo / BASS_TECH / slide tracks | 6.1 | yes | CaptureRing / PerformanceCapture (chord, bassTech, slideBar) | - | BassTechniques::aStrikeIsCapturedAsBassTech, ModelGapsUi::theCaptureHearsTechniques... | **[changed]** merged |
| 8192 lock-free ring, 10 Hz drain, drop-oldest counter | 6.2 | yes | CaptureRing | overflow count shown | ringOverflowDropsTheOldestAndCountsExactly, capturingTenThousandNotesDoesNotAllocate | **[changed]** allocation counter merged |
| States off/rolling (default)/armed | 6.3 | yes | PerformanceCapture | NOTATION state buttons | offWritesNothing..., armedStartsCleanFromTheNextNote | |
| Timing: quarter notes with transport, seconds free | 6.4 | yes | CaptureClock | quantise box | transportTimingIsInQuarterNotes, freePlayIsInSeconds... | |
| Test: MusicXML via MuseScore fixture | 7 | partial | - | - | musicXmlRoundTrips (own importer) | |
| Test: Guitar Pro round trip via fixture parser | 7 | no | - | - | - | zip validity only |
| Test: chord extraction on 100 progressions >95 % | 7 | no | - | - | - | |

### character-wear.md

The CharacterEngine is complete and deterministic, and the CHARACTER tab has every listed control. But **many of its outputs are never consumed by the DSP.** Only these reach the audio: dead spots, fret-wear sustain and detune, tuner drift, nut damping and nut material (LuthierEngine.cpp:1321-1354, 2364), plus the seed, which now also seeds aging, noise, stability and scrape.
Computed but unused:
- fret-wear buzz multiplier
- pot taper, cap drift, jack drop
- piezo saddle balance, per-string pickup balance, saddle height
- body-age Q, air and HF multipliers
- ~~temperature and humidity multipliers~~ (replaced by EnvironmentModel, which is audible)

**[changed]** Temperature and humidity now work through EnvironmentModel (realism-a merged). The pot-linearity, cap-drift, dodgy-jack and body-age controls still have **no audible effect**. None of `applyPotTaper`, `getCapacitorDrift`, `getJackGain`, `getPickupBalanceDb`, `getSaddleBalanceDb`, `getSaddleHeightOffsetMm`, `getBodyQMultiplier`, `getAirResonanceMultiplier` or `getFretBuzzMultiplier` has a caller outside tests. **New bug:** the live CHARACTER "environment" SysEx (PluginProcessor.cpp:3173-3182) still reads the legacy `character.getTemperature()` and `getHumidity()`. Those are no longer set, so the live MIDI-out environment never changes. **Counts (recounted from the table): yes 22 / partial 8 / no 0.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Deterministic per seed | 0.1 | yes | CharacterEngine | - | fixedSeedIsByteIdentical, valuesDoNotDependOnAccessOrder | |
| Per-note/region, not per-sample | 0.2 | yes | LuthierEngine::triggerNote use | - | - | |
| On by default at low intensity | 0.3 | yes | amount 0.25, enabled true | CHARACTER amount | - | |
| 64-bit seed in preset; New Character reroll | 1 | yes | toVar "seed"; reroll | CHARACTER seed + button | stateRoundTrips | seed field is a read-only label |
| Different seeds -> measurably different | 1 | yes | - | - | differentSeedsProduceDifferentInstruments | |
| Dead spots 0-3/string, beta 6-12, depth/width, Gaussian loop-gain cut | 2 | yes | getSustainMultiplier (LuthierEngine.cpp:785) | DeadSpotMap with sliders | deadSpotsAreInRangeAndClusterCorrectly, deadSpotsReduceSustainWhereTheyAre | |
| Dead spots weighted by body air resonance | 2 | yes | bodyResonance argument | - | - | |
| Fret wear map + real wear pattern | 3 | yes | getFretWear/setFretWear | FretWearMap click-drag | fretWearFollowsRealWearPatterns | |
| Worn fret: reduced sustain, detune | 3 | yes | getFretSustainMultiplier, getFretDetuneCents | - | wornFretsBehaveAsDescribed | |
| Worn fret: more buzz at low action | 3 | partial | getFretBuzzMultiplier | - | wornFretsBehaveAsDescribed (value only) | not used by the buzz engine |
| Refret button (Options -> Character) | 3 | yes | refret | CHARACTER tab (not Options) | - | |
| Tuner drift LFO, looseness default 15 % | 4 | yes | getTunerDriftCents (LuthierEngine.cpp:1540) | CHARACTER looseness | tunerDriftStaysWithinItsStatedAmplitude | |
| Environmental envelope over minutes | 4 | yes | Character/EnvironmentModel (lagged wood/air model) | CHARACTER tab ENVIRONMENT group (RealismGroups) | Environment::ENV03_coldCaseOvershootsThenSettles, ENV06_woodIsSlow, ENV12_aTemperatureStepIsASlew | **[changed]** realism-a merged |
| Reroll reseeds drift | 4 | yes | reroll | New Character | - | |
| Aged pot linearity | 5 | partial | applyPotTaper | slider | agedPotTaperIsMonotonicAndPinned | not used by the circuit |
| Tone cap drift +-5 % | 5 | partial | getCapacitorDrift | slider | capacitorDriftIsInRangeAndDeterministic | not used by the circuit |
| Jack intermittent drop (off by default) | 5 | partial | getJackGain | "Dodgy jack" toggle | intermittentJackIsOffByDefault | toggle has no audible effect |
| Piezo saddle balance +-2 dB | 5 | partial | getSaddleBalanceDb | - | - | unused |
| Per-string per-pickup balance +-1.5 dB, pole height | 6 | partial | getPickupBalanceDb | - | - | unused |
| Nut slot wear on open strings | 7 | yes | getNutDamping | - | - | |
| Saddle height variation (intonation) | 7 | partial | getSaddleHeightOffsetMm | - | - | unused |
| Bone vs synthetic nut | 7 | yes | getNutMaterialDamping | "Bone nut" | nutMaterialChangesDamping | |
| Body break-in: Q up, air -3..8 %, HF damping down | 8 | partial | getBodyQMultiplier etc. | body age slider | bodyBreakInMovesInTheRightDirection | never applied to BodyEngine |
| Temperature -> tension/tuning | 9 | yes | EnvironmentModel (`env_temperature_c`, `env_tuned_at_c`); CharacterEngine only keeps legacyTemperatureOffsetCents for migration | ENVIRONMENT group sliders (attached params) | Environment::ENV02_steadySlopeFromTheStringsOwnNumbers, Character::temperatureIsTheEnvironmentsNow, ENV11_legacyTemperatureAndHumidityMigrate | **[changed]** the character temperature box was removed; temperature lives in environment.md |
| Humidity -> body Q / compliance | 9 | yes | EnvironmentModel (`env_humidity_pct`: geometry + plate) | ENVIRONMENT group | Environment::ENV05_humidityMovesTheGeometryAndThePlate, ENV08_aDryNeckBuzzesMore | **[changed]** the old getHumidityQMultiplier was deleted |
| Session time drift + Retune | 9 | yes | retune; retuneString | Retune, headstock popover | retuneResetsTheDrift, Environment::ENV10_retuneZeroesThenDriftsAgain, RealismUi::theRetuneAllButtonClearsEveryOffset | **[changed]** per-string retune added |
| CHARACTER tab contents incl All fresh / All old | 10 | yes | CharacterPanel (+ realism groups) | Col 4 CHARACTER | allFreshAndAllOldPresets, UndoCoverage::characterEditsAreGroupedEntries | **[changed]** temperature/humidity boxes replaced by the ENVIRONMENT group; edits undoable |
| Stacks with humanize | 11 | yes | - | - | - | |
| Test: temperature step measured in the tuning engine output | 12 | yes | - | - | Environment::ENV12_aTemperatureStepIsASlew, ENV02_... | **[changed]** measured through the environment model |
| Test: zero-character bitwise identical | 12 | yes | - | - | zeroCharacterIsExactlyNeutral | |

### updates-telemetry.md

Everything is off by default. The update check (manifest, semver, 24 h throttle, worker thread, banner, beta toggle), the local outbound log, the policy file with its "Managed by policy" banner, and the privacy dashboard with its paranoia button are done and tested. Gaps:
- **Usage and diagnostics telemetry never record anything.** `Telemetry::record` and `sendPending` have no callers outside tests.
- **No crash handler writes minidumps.** Existing dumps are detected, but nothing ever creates one.
- The License class exists but has no activation, offline-challenge or deactivate UI.
- Delta updates exist only as an unwired content-package path.

Still true: `Telemetry::record` and `sendPending` have no callers outside Updates/Telemetry.cpp, and no crash handler is installed. **Counts (recounted from the table): yes 13 / partial 6 / no 2.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| All opt-in, default off | 0.1 | yes | Telemetry | Options UPDATES/PRIVACY | everythingIsOffByDefault | |
| No PII | 0.2 | yes | - | - | - | |
| Every outbound call logged locally | 0.3 | yes | Transport logging | - | everyOutboundCallIsLogged | |
| Update check on worker thread | 0.4 | yes | PluginEditor thread launch | - | - | |
| Never auto-install | 0.5 | yes | - | - | - | |
| Opt-in toggle, on-load throttled 24 h, Check now | 1 | yes | UpdatesPage; PluginEditor | Options -> UPDATES | updateCheckIsThrottled | |
| Manifest JSON, semver compare | 1 | yes | checkForUpdate | - | updateCheckReadsTheManifest, versionComparison | |
| Non-modal banner with What's new / Download | 1 | yes | NotificationCentre; UpdateDownloader | header, Options -> UPDATES | Updates::theDownloadLandsInDownloadsUnderItsOwnName | **[changed]** Download now fetches the installer into ~/Downloads (still never installs) |
| Beta channel toggle | 1 | yes | betaToggle | UPDATES | - | |
| Delta updates (bsdiff/rsync) | 2 | partial | Updates/ContentPackage::applyDelta (signed content delta, rollback) | no | ContentPackage::aBadHashRollsBackAndOffersTheFullDownload | **[changed]** content-package deltas exist, but ContentPackage has no caller outside tests, and there is no binary (plugin) delta |
| Usage telemetry (panels, presets, CPU, daily send) | 3A | partial | Telemetry::record/sendPending | toggle | recordsAreLoggedLocallyAndSentWhenAllowed | **no call sites; nothing is ever recorded** |
| Diagnostics telemetry (errors, host info) | 3B | partial | same | toggle | same | no call sites |
| Log at Diagnostics/telemetry-yyyymm.log | 3 | yes | Telemetry | PRIVACY | recordsAreLogged... | |
| Crash minidump written on crash | 4 | no | - | - | - | no crash handler installed |
| Next-launch prompt with diff viewer; 3-attempt upload | 4 | partial | hasPendingCrashReport; banner "Review" | PRIVACY | crashReportDescribesItself | no dumps are ever produced |
| License activate / revalidate 30 d / 14 d grace | 5 | partial | License | no activation UI | licenceActivationAndGrace, revalidationCountdown... | grace banner + Help readout only (HelpTab.cpp:170); unchanged, see licensing.md |
| Offline challenge/response, one-click deactivate | 5 | no | License::getOfflineChallenge, deactivate | no UI | - | engine-only |
| Privacy dashboard: explanations, toggles, view log, clear, endpoints, paranoia | 6 | yes | PrivacyPage | Options -> PRIVACY | turnEverythingOffDeletesAndDisables | |
| Enterprise policy file + "Managed by policy" | 7 | yes | Policy::getPolicyFile | banner | policyOverridesTheUser | |
| Tests: no-network, defaults, policy | 8 | yes | - | - | noNetworkIsSilentRatherThanAnError etc. | |
| Tests: crash dump privacy grep; delta SHA | 8 | partial | - | - | crashReportDescribesItself | delta test absent |

### midi-export.md

The codec is thorough: 18 event classes, text and SysEx redundancy, Generic and Luthier profiles, PPQ 96-3840, track splits, `.midprofile`, strip-identifiers, and a corrupt-file sweep. Every listed test exists, including the null tests against all factory presets. Gaps:
- Capture-based export writes NOTE, BEND and SLIDE, BASS_TECH/SLIDE_BAR (merged), and VIBRATO/WHAMMY only when they are tagged as score techniques (MidiPerformance.cpp kTechniques). It never writes STRUM, PICK, SQUEAK, BUZZ, CLANK or RASGUEADO: those only go out as live SysEx.
- The default MIDI options live on the MIDI OUT tab, not in Options -> MIDI (the MidiPage there holds only the chord window and learn clearing).
- MIDI OUT export and drag-out write on the message thread.
- macOS does not register `.midprofile`.

**[changed]** Merged since the last audit: MIDI import (menu, drop, three targets), SESSION drag-out, the tune export dialog, capture ranges, and `.midprofile` registration on Windows and Linux. New partial: the live environment SysEx is stale (see character-wear). **Counts (recounted from the table): yes 22 / partial 5 / no 0.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Luthier profile lossless for every event class | intro, 2.1 | partial | LuthierEvents codec (18 classes) | - | everyEventClassRoundTripsWithEveryField | the capture never produces STRUM/PICK/SQUEAK/BUZZ/CLANK/VIBRATO/WHAMMY/RASGUEADO |
| Generic profile with LUTHIER: text metas | intro, 3 | yes | MidiProfiles | profile toggle | genericProfileIsPlainMidi | |
| Export on worker thread | 0.1 | partial | - | - | - | notation export is now on a worker. MIDI OUT export and drag-out still write on the message thread (no thread in MidiOutPanel.cpp) |
| Import parses to PerformanceScore / Tune | 0.2 | yes | MidiProfiles::importFromFile | - | aScoreSurvivesBothProfiles | |
| Self-describing schema version | 0.3 | yes | LUTHIER-BEGIN class schema | - | importWarnsOfAdvancedRangesAndNewerSchemas | |
| Sample-accurate, 960 PPQ default | 0.4 | yes | - | PPQ box | luthierProfileIsSampleExactAtEveryPpqAndSplit | |
| SMF format 1, meta track, per-instrument/per-string tracks | 1 | yes | MidiProfiles | split box | trackSplitsNameTheirTracks | |
| LUTHIER chunk, BEGIN/END, SysEx + text redundancy | 2, 2.3 | yes | LuthierMidiEvents | SysEx toggle | eitherCopyOfAnEventIsEnough | |
| Encoding documented in docs/MIDI_EXPORT_LUTHIER_PROFILE.md | 2.1 | yes | docs/ | - | - | |
| Luthier round trip -60 dB null | 2.2 | yes | - | - | luthierRoundTripNullsEveryFactoryPreset | |
| Unknown classes preserved as opaque blobs | 2.2 | yes | LuthierEventClass::unknown | - | - | not separately tested |
| Generic: CC1/11/64/74, bend-range RPN per track | 3 | yes | MidiProfiles | - | genericProfileIsPlainMidi | |
| Export dialog from MIDI OUT tab | 4.1 | yes | MidiOutPanel | Col 4 MIDI OUT | MidiOutPanel::exportWritesTheCaptureInTheChosenProfile | |
| Export from tune-builder dialog / File -> Export -> MIDI | 4.1 | yes | UI/TuneExportDialog (Destination::midi); HeaderBar "Save last MIDI take..." | TUNE tab export, File menu | TuneIntegration::theExportDialogWritesEachDestinationFromOneScreen, aLuthierProfileMidiExportReimportsToTheSameAudio | **[changed]** merged from tune-help. The File menu item is "Save last MIDI take", not an Export -> MIDI submenu |
| Fields: profile, range, split, realism, PPQ, preview | 4.1 | yes | MidiOutPanel; CaptureRanges | MIDI OUT | previewDescribesTheOpeningBar, CaptureRanges::* | **[changed]** marked-region/current-section ranges merged |
| Drag-out from session recorder Save (Alt = Generic) | 4.2 | yes | SessionTab::SaveButton::filesToDrag (isAltDown -> Generic) | Practice SESSION Save | dragOutWritesAValidMidiFile, PracticeGaps::theSaveButtonDragsTheSavedTakeOut | **[changed]** merged |
| Import: File -> Import -> MIDI, drop .mid | 5 | yes | HeaderBar item 14 "Import MIDI..."; LuthierAudioProcessorEditor::filesDropped / importMidiFile | File menu, drop on window | MidiImport::aDropOnTheWindowImports | **[changed]** no -> yes |
| Auto-detect profile by header | 5 | yes | MidiProfiles | - | headerStrippedFileLoadsAsGenericWithoutWarning | |
| Import targets: session / tune / looper | 5 | yes | Export/MidiImportTargets::importFile (popup target menu) | popup after drop/menu | MidiImport::bothProfilesGoIntoTheSession, theTuneBuilderGetsANewTune, theLooperGetsARenderedLayer | **[changed]** merged |
| Live MIDI-out 7 sources incl Character/noise SysEx, workshop | 6 | partial | MidiOutRouter; sysExOut (PluginProcessor.cpp:3160-3260) | ROUTING + MIDI OUT | liveEventsAndWorkshopChangesGoOutAsLuthierSysEx, ModelGapsUi::characterSeedAndEnvironmentGoOutAsTheyChange | **[changed]** seed/environment events merged, but the environment event reads the legacy CharacterEngine temperature/humidity, which EnvironmentModel replaced. It never reflects `env_temperature_c` / `env_humidity_pct` |
| .midprofile save/load | 7 | yes | MidiExportDefaults | SAVE/LOAD PROFILE | midprofileSavesAndLoads | |
| Installer registers .midprofile | 7 | partial | packaging/windows/Luthier.iss:134, packaging/linux/luthier-mime.xml; FileOpenRouter::Target::midiProfile | - | FileOpen::everyAssociationRoutesToItsLoader | **[changed]** Windows + Linux register it and the standalone opens it. packaging/macos has no document-type declaration |
| Options -> MIDI defaults (profile, PPQ, split, realism, SysEx) | 8 | partial | MidiExportDefaults | MIDI OUT tab, not Options -> MIDI | profileEditsAreTheExportDefaults | placement differs from spec |
| Session recorder writes .mid alongside WAV | 9 | yes | saveLastTake | SESSION | - | |
| Macro CCs as CC broadcast | 9 | yes | macroCc | ROUTING/MIDI OUT | - | |
| Advanced-range annotation warns on import; strip identifiers | 9, 11 | yes | - | toggle | importWarnsOfAdvancedRanges..., stripIdentifiersLeavesNoNamesOrSeeds | |
| Tests (all 10 listed) | 12 | yes | - | - | MidiExport::* (25 tests) | |

### input-routing.md

This is the least implemented spec in the group. The MIDI chain order is roughly followed: pass-through capture first, then MIDI Learn, then PC/CC0. Deviations and gaps:
- MIDI Learn does **not consume** the event it learns (`processMidi` takes a const buffer), and it learns CCs only.
- The controller profile is not a separate remapping stage.
- The practice trainers receive no MIDI.
- Expression calibration is never applied.
- Incoming Luthier SysEx is not dispatched.
- MIDI clock, Start, Stop and SPP are ignored.
- The root file-drop handler takes only `.mid`/`.midi` (PluginEditor.cpp:1255). The other Luthier types are opened only by the standalone's command line through FileOpenRouter.
- There is no Diagnostics fixture injection and no `Tests/InputRouting/` suite.

Keyboard handling works: shortcuts go through the rebind registry with clash detection, and JUCE gives text fields focus.

**[changed]** The window now accepts `.mid` drops (root drop: no -> partial), and MIDI Learn is realtime-safe. Clock/SPP, SysEx-in dispatch, fixture injection and a `Tests/InputRouting/` suite are still absent. **Counts (recounted from the table): yes 8 / partial 10 / no 7.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| MIDI consumer order (export -> learn -> profile -> interpreter -> technique -> rhythm -> tune -> practice -> strings) | 1 | partial | PluginProcessor::processSlice | n/a | - | tune capture before learn; profile applied inside the interpreter; practice not fed |
| MIDI-out pass-through first | 1 | yes | midiOutRouter.captureInput | n/a | Routing::midiOutPassThroughIsSampleExact | |
| MidiLearn consumes the first non-note event | 1.1 | partial | MidiLearnManager::processMidi (const buffer) | right-click / header arm | ReviewRegression::midiLearnLearnsAppliesAndSurvivesAClear, MidiLearn::learningDoesNotAllocateOrLockOnTheAudioThread | **[changed]** now lock-free (LookupEntry table + learnedCc handed to a timer) and undoable, but the learned CC is still not consumed |
| Learn accepts CC/PC/AT/ch pressure; note learn opt-in | 1.1 | no | CC only | - | - | |
| RhythmEngine consumes chord notes | 1.1 | yes | RhythmEngine | - | GenreKits::everyKitSoundsWhenApplied | |
| TechniqueEngine tags, does not consume | 1.1 | yes | TechniqueEngine | - | - | |
| Macro CC sources and learn mappings pass through | 1.2 | yes | feedModulationSources, MidiLearn | - | - | |
| Expression calibration remaps CC before consumers | 1.2 | no | ExpressionCalibrationSet::map unused | - | - | |
| CCs never reach string/rhythm engine directly | 1.2 | partial | MidiInterpreter ccMap (e.g. CC74 -> Tap) | - | - | the interpreter acts on CCs itself |
| PB / AT -> per-note bend / vibrato | 1.3 | yes | MidiInterpreter | - | - | |
| PC -> snapshot; "Bank + PC" routing option | 1.4 | partial | handleLiveMidi | - | programChangeMapsAcrossAllOneTwentyEight | CC0 always selects a preset; no option |
| Luthier SysEx in dispatched to engines | 1.5 | no | decodeSysEx used only for file import | - | liveSysExIsDroppedByOtherHostsAndReadByLuthier (decode only) | |
| MIDI clock / Start / Stop / SPP -> tap and rhythm | 1.6 | no | - | - | - | |
| Mouse z-order: overlays consume, popovers eat outside click | 2, 2.1 | partial | OverlayHost | - | - | JUCE default behaviour; untested |
| Drag consumers (parts, pedals, snapshots, presets->setlist, mod sources) | 2.2 | partial | ModSourceCard drag; Workshop card-onto-string | - | DragToModulate::..., WorkshopStrings::aCardOntoAStringOverridesItAndASetClearsOverrides | **[changed]** mod-source and part/string-set drags merged; preset->setlist and snapshot drags not found |
| Root file-drop by extension (10 types) + unknown-type banner + batch | 4 | partial | PluginEditor::isInterestedInFileDrag/filesDropped (.mid/.midi only); IrSlotEditor (audio); FileOpenRouter (6 types, standalone command line only) | window drop | MidiImport::aDropOnTheWindowImports, FileOpen::* | **[changed]** no -> partial. The window accepts only MIDI. Presets, guitars, tunes, loops, setlists and profiles dropped on the window are ignored, although FileOpenRouter::open could route them. No unknown-type banner, no batch |
| Text field consumes keys; Escape passes | 3, 3.1 | yes | JUCE focus + PluginEditor::keyPressed | - | - | |
| Global shortcut table, rebinding in Options | 3 | yes | AccessibilitySettings | Options -> ACCESSIBILITY | Accessibility tests | |
| Rebind conflict detection | 3.2 | yes | OptionsPages clash hint | Options | - | inline hint rather than a modal |
| IME honoured | 3.3 | partial | JUCE TextEditor | - | - | untested |
| Host transport -> rhythm, tune, tap, metronome, recorder | 5 | partial | processSlice playhead | - | RhythmPatterns::silentWhenStopped... | metronome is not transport-synced |
| Sidechain consumers (followers, compressor, sc-to-amp, EQ/cab match) | 6 | partial | - | - | - | no sidechain compressor |
| Standalone audio input: practice trainer input | 7 | no | - | - | - | |
| Options -> Diagnostics "Inject fixture MIDI/audio" | 8 | no | - | - | - | Diagnostics now has an audio-path view (AudioPathView; Diagnostics::theAudioPathShowsWhatIsSoundingAndTheFlags), but no injection |
| Tests/InputRouting/ suite + 60 s integration | 9 | no | - | - | - | missing |

### Top gaps (group D)

Items 1-15 were all open in the previous audit and are still open. Closed since then: `.mid` import/drop (was #10, now partial), character temperature/humidity (part of #5), and drag-to-route (part of #15). New items are marked NEW.

1. **Backing-track pitch shift and tempo shift are dead controls.** `BackingTrackPlayer::setPitchShiftSemitones` and `setTempoRatio` only store `pitchSemis` and `tempoRatio`, and nothing reads them (BackingTrack.cpp:241-248). The TRACK sliders (PracticePanel.cpp:754/759) and routine `tempo_ratio` / `pitch_semitones` do nothing.
2. **Expression-pedal calibration is non-functional.** `ExpressionCalibrationSet::observe` and `map` have no caller outside Live/LiveControls.cpp. The Options -> EXPRESSION wizard never sees the pedal, and incoming CCs are never remapped.
3. **Controller profile MPE and bend settings are overwritten every block.** `ParameterBridge::applyToEngine` does this at Parameters.cpp:1339-1340. The Seaboard, Linn and Osmose profiles lose MPE and their 48-semitone range.
4. **Controller latency compensation is never applied.** `getEffectiveLatencyMs` is display-only, `LatencyWizard::addMeasurement` has no caller, the profile selection is not persisted, `rowsAsStrings` and `applyPitchCurve` are never applied, and `ControllerMerge` is unused.
5. **Character "aged electronics" and body age are inaudible.** `applyPotTaper`, `getCapacitorDrift`, `getJackGain`, `getPickupBalanceDb`, `getSaddleBalanceDb`, `getSaddleHeightOffsetMm`, `getBody*`/`getAir*` multipliers and `getFretBuzzMultiplier` have no callers. The Pot linearity, Cap drift, Dodgy jack and Body age controls do nothing.
6. **The scale trainer's quiz, interval and chord-tone modes cannot be answered by playing.** Answers come only from on-screen buttons, and no note-on reaches `ScaleTrainer`.
7. **Live actions cannot be put on a footswitch or CC:** next/previous snapshot, snapshot-by-CC, tap, kill and setlist navigation. MIDI Learn is CC -> parameter only, it does not consume the learned CC, and there is no per-preset or global learn scope.
8. **The EQ-match result overwrites cab IR slot 2** (ToneMatchPanel.cpp:590) instead of being an insert filter at a chosen position, and cab/EQ analysis runs on the message thread.
9. **Snapshot morph cannot be driven by an expression pedal, LFO or mod wheel,** because it is not a parameter (only `preset_morph_position` is). Morph exclusions and Bezier points have no UI.
10. NEW: **Snapshot recall now runs at the message timer's rate.** After the review fix, `SnapshotBank::advance` and `applyBlend` run from the 30 Hz timer (`advancePending`), and the audio thread only accumulates time. The default 30 ms crossfade becomes about one step, and a PC-triggered recall waits for the timer, or stalls with the message thread. This is safe, but it does not match live-performance 0.2 / 1. A lock-free per-parameter ramp on the audio side is needed.
11. **The metronome ignores host and tap tempo** and is not transport-synced (`Metronome::setTempo` is called only by routines and the Options reset).
12. **The tab reader is thin:** no GP5/GP/PTB import, static text only, no cursor or playback, no fretboard highlight, no play-along scoring. There is also no `<grace>` in the MusicXML export.
13. **Usage and diagnostics telemetry and crash reporting are shells.** `Telemetry::record` and `sendPending` have no callers, and no crash handler is installed. The License class has no activation UI. ContentPackage delta/apply has no caller.
14. NEW: **The live CHARACTER environment SysEx is stale.** PluginProcessor.cpp:3173-3182 sends the legacy `CharacterEngine::getTemperature/getHumidity`. EnvironmentModel replaced both and the loader no longer sets them. Live MIDI-out, and any capture of it, never reports `env_temperature_c` / `env_humidity_pct`.
15. **The root file drop takes only `.mid`.** Presets, guitars, tunes, loops, setlists and `.midprofile` dropped on the window are ignored, although `FileOpenRouter::open` already routes all six. There is no unknown-type banner and no batch drop.
16. **MP3 backing tracks are unsupported,** but the chooser still offers `*.mp3` (PracticePanel.cpp:694).
17. **The MOD tab lacks several editors that exist in the engine:** route offset, LFO phase and custom shape, envelope curves and loop mode, and the step-sequencer grid. There are also no per-source colours.
18. **MIDI input path gaps:** incoming Luthier SysEx is not dispatched, MIDI clock/Start/Stop/SPP are ignored, and there is no fixture injection or `Tests/InputRouting/` suite.
19. **No sidechain-compressor pedal** exists, although routing-io 4 and input-routing 6 list it as a sidechain consumer.
20. **The looper does not re-render through a new tone.** MIDI is now captured lock-free, but nothing renders it. The Capture utility has no DI or per-string source, and three tone-match tests are still missing.

### Unspecified gaps noticed

- A **built-in tuner** (strobe/needle) for tuning against the backing track, or for checking Character and Environment drift. The headstock popover shows offsets, but there is no pitch display of the input or output.
- **Host automation of routing, monitor and practice levels.** These are still not parameters.
- **A MIDI-monitor / activity view** showing which consumer took an event. Diagnostics' new AudioPathView shows audio flags, not MIDI.
- **A per-controller setup check** ("play each string") that confirms the channel-to-string mapping for a hex pickup.
- **The standalone opens six Luthier file types from the command line** (FileOpenRouter), but the plugin window cannot open them by drop. Either route drops through FileOpenRouter, or document the difference.
- **Stale documentation:** spec/GAPS.md (lines 202-205, 241-250) still says PRACTICE, NOTATION and MIDI OUT are unbuilt, and it predates everything merged here.
- Resolved since the last audit: undo for routing and modulation edits (UndoCoverage::routingGainAndMuteAreEntries, modPanelRouteEditsAreEntries).

### Small glue candidates

These engine features exist and only need a control, a call site, or a preset field. All rows from the previous audit are still valid. Two are new, and one is closed.

| Feature / id | Engine hook | Where the control should go | Effort |
|---|---|---|---|
| Mod route offset | `ModMatrix` route `offset` | MOD tab ModRouteTable: "Offset" column beside Depth (pushUndoAction like depth) | small |
| LFO phase offset | `ModLfo::setPhaseOffsetDegrees` | MOD ModSourceCard (LFO): one more `sliderRow()` | trivial |
| Envelope stage curve, loop mode | `ModEnvelope::setStageCurve`, `setLoopMode` | MOD ModSourceCard (Envelope): combo boxes | small |
| Step-sequencer steps | `ModStepSequencer::setStep` | MOD card mini grid; could reuse MuteGridEditor-style cells from techniques | medium |
| Morph exclusions | `SnapshotBank::setParameterExcludedFromMorph` | LIVE tab list, or knob right-click "Exclude from morph" | small |
| Morph Bezier points | `SnapshotBank::setBezierControlPoints` | LIVE tab beside morphCurveBox | small |
| Monitor pan + 3-band EQ | `MonitorMix::setPan`, `setEq*Db` | LIVE tab monitor section | trivial |
| Snapshot morph as parameter | new `snapshot_morph_position` APVTS param -> `SnapshotBank::setMorphPosition` | LiveStrip knob attaches; becomes learnable and a mod destination | small |
| Expression calibration feed and apply | `processor.getExpression().observe(cc,val)` + `map()` in processSlice before `midiLearn.processMidi` (PluginProcessor.cpp:1408) | Options -> EXPRESSION (UI exists) | small |
| Controller latency wizard feed | `LatencyWizard::addMeasurement(ms)` on note-on vs click | CONTROLLERS page (UI exists) | small |
| Controller profile vs params | on apply, write `mpe_enabled`/`bend_range` params (Parameters.cpp:1339-1340); persist the profile id in state | CONTROLLERS page | small |
| Backing-track pan / low-cut / high-cut | `BackingTrackPlayer::setPan`, `setLowCutHz`, `setHighCutHz` | TRACK tab | trivial |
| Backing-track playlist | `BackingTrackPlayer::setPlaylist` | TRACK tab | small |
| Looper per-layer low/high cut | `LoopLayer::setLowCutHz`, `setHighCutHz` | LOOP layer row (via LooperTab::editLayer for undo) | trivial |
| IR start/end trim | `IrSlot::setStartTrim`, `setEndTrim` | IrSlotEditor (pushIrEdit for undo) | trivial |
| EQ-match band | `EqMatch::Options.lowHz/highHz` | EQ wizard: two sliders | trivial |
| Capture source DI / per-string | extend `Capture::Source`; taps exist in TapBuffers | TONE MATCH capture pane | small |
| Scale quiz from played notes | forward note-ons to `ScaleTrainer::answer (midiNote)` (Trainers.h:89) when SCALE is in quiz mode | Practice drawer SCALE | small |
| Metronome follows tap/host | `metronome.setTempo(blockTempo)` near PluginProcessor.cpp:1580 when "Follow" is on | METRO | trivial |
| Character pickup/saddle balance, cap drift, pot taper, jack gain, body age | apply in PickupEngine / circuit / output / BodyEngine | CHARACTER tab (controls exist) | small each |
| LinnStrument rows-as-strings / Osmose curve | apply `rowsAsStrings` / `applyPitchCurve` in `ControllerProfileLibrary::apply` / MidiInterpreter | CONTROLLERS | small |
| Rhythm on/off as a parameter | `rhythm_enabled` APVTS bool -> `RhythmEngine::setEnabled` | Easy rhythm strip + RHYTHM toggle | trivial |
| NEW: Environment SysEx from EnvironmentModel | in PluginProcessor.cpp:3173-3182, read `env_temperature_c` / `env_humidity_pct` (or `engine.getEnvironment()`) instead of the legacy character enums | none (MIDI out) | trivial |
| NEW: Window drop for all Luthier types | in `filesDropped`, call `FileOpenRouter::open` for non-MIDI files, and post a banner for unknown types | whole window | trivial |
| MP3 chooser honesty | drop `*.mp3` from the TRACK chooser filter (PracticePanel.cpp:694), or enable JUCE_USE_MP3AUDIOFORMAT | TRACK | trivial |
| ~~Mod drag-to-route~~ | closed (visual merged) | - | - |

---

## Group E: phase-2 realism

Audited: `/home/user/luthier` at HEAD `961cd55` (`claude/luthier-audit`). realism-a, realism-b, realism-c, model-gaps, tune-help, visual, release and review are merged into this tree. Only `origin/claude/luthier-techniques` is unmerged; it was inspected with `git diff HEAD...origin/claude/luthier-techniques` and `git show`. The audit was static. Nothing was built or run, so "Test covering it" means the test exists, not that it passes.

The previous audit was at `2ede79c`. Since then only three of these specs changed, and only as "amended in the build" notes from realism-a:
- `body-coupling.md`: bridge wave = loop return; budget 0.1 units; BC-06/07/10/11 measurement changes.
- `environment.md`: humidity is acclimatised directly; ENV-05 0.01 mm; ENV-12 0.1 c/block.
- `string-aging.md`: in-loop slide-noise table; synchronous detune write.

None of these amendments adds a requirement. No new spec file belongs to this group.

Legend: **yes** = implemented and wired on HEAD. **partial** = wired, but it departs from the spec or is incomplete. **no** = absent on HEAD. "in progress on techniques" = present only on `claude/luthier-techniques`.

---

### volume-knob-interaction.md

**Summary:** Unchanged since the last audit, apart from one new QA test (`Circuit.atVolumeFiveKinmanHoldsTheTiltNoneLosesIt`) and a UI render test of the curve (`LiveDisplays.theFretboardDrawsTheSlideBarAndTheCircuitCurveFollowsTheVolume`). Still open:
- The CHARACTER tab has no CIRCUIT mirror.
- The custom bleed R/C editor is always visible.
- The network is re-solved on the audio thread.
- `macro_tone` rescales the tone wiper.

Counts: **yes 20 / partial 4 / no 1**.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Solve pickup+pots+cap+bleed+cable+amp in as one network | 0.1, 1.1 | yes | DSP/Circuit/GuitarCircuit.cpp: `buildPassiveNetlist`, `rebuild` | n/a | Circuit.theAudioPathMatchesTheResponse | |
| CableSim removed | intro | yes | – | n/a | – | |
| Coefficients on the message thread; the audio thread runs a fixed filter | 0.5 | partial | LuthierEngine.cpp:2757 `circuit.setComponents (getLiveCircuitComponents())` inside processBlock | n/a | Circuit.sweepingEveryControlDoesNotAllocate | No allocation, but the matrix inverse runs at block rate on the audio thread |
| Ls/Rs/Cp from the pickup part | 1, 4 | yes | LuthierEngine.cpp:`getLiveCircuitComponents` | n/a | Circuit.theEngineRunsThroughTheCircuit | |
| Volume = wiper (level + load + cable decoupling) | 1.2 | yes | `taperFraction`, netlist | Adv col 2 CIRCUIT; Easy "Guitar" card; illustration | Circuit.turningDownDarkensAsWellAsQuietens | |
| Treble bleed None / Kinman / Fender / Custom | 1.3 | yes | Parameters.cpp:`trebleBleedNames` | Adv col 2 | Circuit.aKinmanBleedKeepsTheTop; Circuit.atVolumeFiveKinmanHoldsTheTiltNoneLosesIt | Trademark-safe names |
| Custom bleed R, C, series/parallel | 1.3 | yes | `circuit_bleed_r/_c/_mode` | Adv col 2 (AdvancedPanel.cpp:756-758) | Circuit.everyCornerOfTheAdvancedRangeIsStable | |
| Active mode | 1.4 | yes | `buildActivePickupNetlist` | Adv col 2 | Circuit.activeModeRemovesTheLoading | |
| Cable pF/m table | 2 | yes | `cableCapacitancePerMetre` | Adv col 2 | Circuit.cableCapacitanceMovesTheResonance | |
| `cable_on` off = zero-length cable | 2 | yes | – | Adv col 2 | Circuit.bypassIsNeutral | |
| 50s wiring topology | 3.1 | yes | `PotTaper::fiftiesWiring` | Adv col 2 taper | – | Still no dedicated test |
| 15 params, IDs and defaults | 3 | yes | Parameters.cpp circuit block | – | Parameters count | |
| Advanced ranges (circuit family) | 3 | yes | PhysicalRange.cpp | padlock | Circuit.everyCorner… | |
| Feedback level taken after the circuit | 4 | yes | FeedbackLoop.cpp | n/a | Feedback.theVolumeKnobLowersTheLoopByTheCircuitsAttenuation | |
| Chain String→Pickup→Circuit→PreFX | 4 | yes | LuthierEngine.cpp:2799 | n/a | Circuit.theEngineRunsThroughTheCircuit; ModelGapsUi.auxOneTapsBeforeOrAfterTheCircuit | |
| Large volume/tone knobs, Adv col 2 | 5 | yes | AdvancedPanel.cpp | Adv col 2 | GuiReach | |
| Pot/cap dropdowns with a custom value | 5 | yes | UI/CircuitPanel.h:`StandardValueChoice` | Adv col 2 | GuiReach | |
| Custom R/C editor only for Custom | 5 | partial | AdvancedPanel.cpp:753-758 | always shown | – | No visibility toggle on `circuit_treble_bleed` |
| Live H(s) visualiser with a marked peak | 5 | yes | UI/CircuitPanel.cpp:`CircuitResponseView` | Adv col 2, Easy | LiveDisplays.theFretboardDrawsTheSlideBarAndTheCircuitCurveFollowsTheVolume (AppearanceTests.cpp:672) | Newly covered |
| CHARACTER tab CIRCUIT group (full-size mirror) | 5; gui-int 4.4 | no | CharacterPanel.cpp has no circuit group (not on techniques either) | no | – | |
| Easy compact card | 5 | yes | EasyPanel.cpp:312-319, 587-592 | Easy | – | |
| Test: tone monotonic, never >unity | 6 | yes | – | – | Circuit.theToneControlSweepsDownAndNeverBoosts | |
| Test: pot value moves the resonance | 6 | yes | – | – | Circuit.potValueMovesTheResonance | |
| Test: stability 44.1-192 kHz | 6 | yes | – | – | Circuit.everyCornerOfTheAdvancedRangeIsStable | |
| `guitar_tone` is the tone wiper only | 3 | partial | Parameters.cpp:1484 `guitarTone * (0.45 + macroTone*1.1)` | – | – | The Easy tone macro still rescales the physical wiper |

---

### pick-noise.md

**Summary:** Two rows moved forward:
- **Pool degradation is wired.** `CpuRelief` step 4 calls `setDegraded` (LuthierEngine.cpp:3036), tested by CpuReliefTests.
- **The pick is drawn on the Workshop bench.** It is at true size, rotated by angle, and has an 8 px handle (visual merge). The handle turns the angle instead of resizing thickness, and the colour is fixed.

Still open:
- Three materials are missing (Ultex, Tortex, Stone/horn).
- The default thickness decodes to 1.07 mm, not 0.73.
- The rake has no UI trigger. `ScrapeEngine::requestRake` has no non-test caller on HEAD or on techniques.
- The `scrape_*` controls are only on techniques.

Counts: **yes 19 / partial 7 / no 1**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Pools 16/16/16/8/16/8, pre-allocated | 1 | yes | DSP/Noise/NoiseEngine.h:`kPoolSizes` | n/a | NoisePool.aFullPoolStealsTheOldest | |
| Steal the oldest | 1 | yes | `NoiseEngine::trigger` | n/a | NoisePool.aFullPoolStealsTheOldest | |
| Degradation step 4 halves pools | 1 | yes | LuthierEngine.cpp:3036 `setDegraded (reliefStep >= CpuRelief::halveNoisePools)`; Support/CpuRelief.h | n/a | CpuReliefTests.cpp:118-131; NoiseTests.cpp:81 | **New since the last audit** |
| Per-material textures synthesised once | 1 | partial | `synthesiseTexture` (5 shared textures) | n/a | NoisePool.aSeedRepeatsExactly | One per texture class, not one per material |
| Generator excitation→resonators→envelope | 1.1 | yes | `NoiseGenerator` | n/a | – | |
| Injection points | 1.2 | yes | NoiseEngine | n/a | – | |
| Aux 8 carries the noise sum | 1.3 | yes | RoutingMatrix | ROUTING | PluginBuses.aux8CarriesThePlayingNoiseAndObeysItsStrip; RealismUi.theAux8SwitchIsMirroredOnRouting | |
| `pick_material` + 8 materials | 2.1 | partial | PlayingNoise.h:88 (comment admits Ultex, Tortex and stone are missing) | CHARACTER → PICK | PickNoise.clickPitchTracksMaterialAndThickness | 5 of 8 |
| `pick_thickness` 0.38-3 mm, default 0.73 | 2, 7 | partial | Parameters.cpp:494 default 0.5 → 1.07 mm (`pickThicknessMm`, :403) | CHARACTER → PICK; Workshop pick | – | Guitar default still wrong. The bass default 1.14 mm now applies (BassDefaults.cpp:73-79) |
| `pick_tip_radius`, `pick_bevel`, `pick_wear` | 2 | yes | Parameters.cpp | CHARACTER → PICK | – | |
| `pick_angle` 0-60° | 2 | yes | `pickAngleDegrees` | CHARACTER → PICK; Workshop handle | PickNoise.angleTradesClickForChirp | |
| Click level formula, −30 dB | 3 | yes | `PlayingNoise::makeClick` | – | PickNoise.clickScalesWithVelocityToThePower0_7; PickNoise.aClickSitsAboutThirtyDecibelsUnderTheNote | |
| Click fundamental, decay, wear sub-resonance | 3 | yes | PlayingNoise.cpp:150-158 | – | PickNoise.clickPitchTracksMaterialAndThickness | |
| Chirp: wound only | 4 | yes | `makeChirp` | – | PickNoise.plainStringsNeverChirp | |
| Scrape/rake: `pick_scrape` MIDI class or Easy rake gesture | 5 | partial | Keyswitches 13/14 → `ScrapeEngine::takeRake` → LuthierEngine.cpp:2486 `triggerPickScrape` | no | ScrapeTests.cpp:984 (`requestRake` from the test) | The keyswitch works only while `scrape_armed`, which has no control on HEAD. The SCRAPE sub-tab is in progress on techniques. There is **no Easy rake gesture anywhere**: `requestRake` has no UI caller on techniques either, whose "Scrape now" fires a scrape, not a rake |
| Rake sweeps; 8-voice pool | 5 | yes | `PlayingNoise::startScrape` | – | – | |
| `use_fingers` silences pick noise | 6 | yes | – | Adv RIGHT HAND; Easy tool selector | PickNoise.fingersNeitherClickNorChirp | |
| Nail/flesh fingertip noise | 6 | yes | `makeFingertipNoise` | Adv | PickNoise.fingersNeitherClickNorChirp | |
| Amount params with advanced range 0-4 | 7 | yes | PhysicalRange pick | CHARACTER → PICK | Ranges | |
| PICK group | 8 | yes | UI/NoiseGroups.cpp | CHARACTER → PICK | NoiseUi.* | |
| Pick illustration: true size, angle-rotated, 8 px handles; drag = angle, handles = thickness | 8 | partial | GuitarRenderer.cpp:2860-2880 `pickPath`/`pickHandle`; WorkshopPanel.cpp:649, 809, 826 | WORKSHOP bench | WorkshopAccessories.thePickIsDraggedAndTurnedOnTheBench | **New.** Dragging the body moves `pluck_position`, and the handle turns the angle. No handle resizes thickness. Always drawn tortoiseshell, whatever the material. Not on CHARACTER |
| Workshop pick part drives the pick params | 8 | yes | WorkshopBench | Workshop | – | |
| Test: noise differs through two bodies; amount 0 bit-identical | 9 | no | – | – | – | |
| Test: zero is free | 9 | yes | – | – | NoisePool.zeroIsFree | |
| Test: 20 clicks into 16 | 9 | yes | – | – | NoisePool.aFullPoolStealsTheOldest | |
| Test: no audio-thread allocation | 9 | partial | – | – | Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks (default patch plucks, so clicks fire) | Indirect only. No noise-specific test |
| Test: deterministic seed | 9 | yes | – | – | NoisePool.aSeedRepeatsExactly | |

---

### string-squeak.md

**Summary:** Two rows moved to yes:
- The bass default pressure of 0.35 is in, via `BassFamilyDefaults`.
- Age roughness is now continuous, per string, from `StringAging` (`StringNoiseInfo::fromSpec(..., aging.getFactors(s).roughness, squeakCentroid)`).

Still open:
- Only `Technique::Slide` shifts squeak. PerformanceScore and ChordVoicer revoicing do not.
- Fret wear does not raise squeak.
- The winding selector still edits `string_material` (its comment says "until the Workshop exists").
- There is no allocation test.

Counts: **yes 21 / partial 2 / no 2**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Wound strings only | 0.1 | yes | `PlayingNoise::makeSqueak` | – | Squeak.flatwoundIsNearlySilentAndPlainIsSilent | |
| No "squeak now" control | 0.2 | yes | – | – | – | |
| f = speed × winding pitch, with glide | 1, 3 | yes | `makeSqueak` | – | Squeak.pitchTracksSpeedAndWinding | |
| Trigger: legato slide | 2 | yes | LuthierEngine.cpp:1543 `onShift` | – | Squeak.aLegatoSlideInTheEngineSqueaksAndABendDoesNot | |
| Trigger: PerformanceScore position change / ChordVoicer revoice | 2 | partial | only `Technique::Slide && slideFromFret >= 0` | – | – | A voicer revoice or a tab position shift never squeaks |
| Not a trigger: pluck, bend, vibrato | 2 | yes | `onShift` gate | – | Squeak.aLegatoSlide…ABendDoesNot | No vibrato test |
| `squeak_min_travel` | 2.1 | yes | – | CHARACTER → STRING NOISE | Squeak.theMinimumTravelIsRespected | |
| Level formula | 3 | yes | – | – | Squeak.aShiftSitsTwentyToThirtyDecibelsUnderTheNote | |
| Winding brightness/texture table | 4 | yes | – | – | Squeak.flatwound… | |
| Injection pre-body + Aux 8 | 5 | yes | – | – | PluginBuses.aux8… | |
| `squeak_probability` deterministic | 6 | yes | `onShift(shiftIndex)` | CHARACTER | Squeak.theProbabilityRollIsDeterministic; Squeak.aThousandRunsAreByteIdentical | New QA test |
| Moisture | 6 | yes | – | CHARACTER | – | |
| Pressure | 7 | yes | – | CHARACTER | – | |
| Bass default pressure 0.35 | 7 | yes | Model/Guitar/BassDefaults.cpp:16, 68; PluginProcessor.cpp:918 | – | BassTechniques.bassDefaultsApplyOnLoad; BassTechniques.aUserSettingSurvivesTheFamilyChange | **New** |
| Style presets + "(modified)" | 8 | yes | NoiseGroups.cpp | CHARACTER | NoiseUi.squeakStylesApplyAndReadModified | |
| 6 params, advanced ranges | 9 | yes | – | CHARACTER | Ranges | |
| STRING NOISE group | 9 | yes | UI/NoiseGroups.cpp | CHARACTER | NoiseUi.* | |
| Noise-event strip | 9.1 | yes | `NoiseEventStrip` | CHARACTER | NoiseUi.theEventStripShowsWhatTheEngineTriggered | |
| Winding selector mirrors the Workshop string part | 9.1 | partial | NoiseGroups.cpp:243-245 attaches `string_material` | CHARACTER | – | The Workshop exists, but there is no jump to it and no part edit |
| No ducking with fret buzz | 10 | yes | separate pools | – | – | |
| Slide Mode suppresses squeak on contacted strings | 10 | yes | LuthierEngine.cpp:1538 `! slide.isUnderBar(s)` | – | Slide.squeakStopsUnderTheBarButNotBesideIt | |
| Fret wear raises squeak | 10 | no | – | – | – | |
| Age multiplies roughness | 10 | yes | PlayingNoise.cpp:29-32, 241 via `aging.getFactors(s).roughness` | Adv col 1 Age (h); CHARACTER STRING AGING | StringAging.SA13_squeakReconciliation | **New:** continuous and per string |
| MIDI export SQUEAK | 11 | yes | Export/LuthierMidiEvents.cpp | MIDI OUT | MidiExport.everyEventClassRoundTripsWithEveryField | |
| Test: no audio-thread allocation | 13 | no | – | – | – | Engine.fiveMinutes… plays no legato shift, so squeak never fires there |

---

### fret-buzz.md

**Summary:** Two changes:
- The bass Factory-low default is in.
- The environment now moves the buzz geometry: `EnvironmentModel::updateGeometry` feeds `fretBuzzModel.setGeometry` at LuthierEngine.cpp:1983, covered by ENV-08.

Still open:
- Fret material does not set buzz brightness. Brightness comes only from fret height (FretBuzz.cpp:227).
- Fret wear does not move buzz.
- Bends do not change clearance.
- There is no module budget test.

Counts: **yes 21 / partial 2 / no 3**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Sensed, never scheduled | 0.1, 3.1 | yes | DSP/Noise/FretBuzz.cpp:`sense`, `process` | – | Buzz.lowActionBuzzesAndHighActionDoesNot | |
| Geometry params | 1, 7 | yes | Parameters.cpp | CHARACTER → SETUP; Workshop setup strip | – | |
| Clearance: nut→12th + relief parabola | 1 | yes | `SetupGeometry::clearanceMm` | – | Buzz.reliefMovesWhereItBuzzes | |
| Only frets past the finger | 1 | yes | – | – | Buzz.onlyFretsAheadOfTheFingerBuzz | |
| Modal amplitude at block rate | 2 | yes | `displacementMm` | – | – | |
| Threshold trim | 3.2 | yes | – | CHARACTER | Buzz.theThresholdIsATrimNotAMute; Buzz.oneDecibelUnderTheThresholdIsCleanThreeOverBuzzes | New QA test |
| Bursts at f0 | 4 | yes | – | – | Buzz.buzzStopsAsTheNoteDecays | |
| Spectrum 3-6 kHz rising with fret | 4 | yes | – | – | – | |
| Fret material sets brightness | 4 | no | FretBuzz.cpp:227 brightness from `fretHeight` only | – | – | |
| Level min(1, excess/0.3) × height | 4 | yes | `levelFor` | – | Buzz.fretHeightChangesLevelNotPosition | |
| Envelope | 4 | yes | – | – | Buzz.buzzStopsAsTheNoteDecays | |
| Injection + Aux 8 | 4 | yes | – | – | – | |
| Sitar mode | 5 | yes | – | CHARACTER | Buzz.sitarModeIsContinuous | |
| Setup styles (6), Player-friendly default | 6.1 | yes | SetupGroup.cpp | CHARACTER → SETUP | BuzzUi.setupStylesApplyAsOneStepAndReadModified | |
| Heatmap | 6.2 | yes | `BuzzHeatmap` | CHARACTER → SETUP | BuzzUi.heatmapCellsReadInMonochromeTerms | |
| Params + advanced ranges | 7 | yes | PhysicalRange buzz | padlock (CHARACTER, WORKSHOP) | Ranges | |
| Fret wear moves buzz | 8 | no | – | – | – | |
| Slide style suggested in Slide Mode | 8 | yes | SlideGroup.cpp | CHARACTER → SLIDE | SlideUi.* | |
| Bass default Factory low | 8 | yes | BassDefaults.cpp:84-92 | – | BassTechniques.bassDefaultsApplyOnLoad | **New** |
| Bends reduce buzz at the fret and raise it higher up | 8 | no | `FretBuzz::process` has no bend input | – | – | |
| Environment moves relief/action (env 2.4, 4) | env 4 | yes | LuthierEngine.cpp:1981-1984 | CHARACTER → ENVIRONMENT | Environment.ENV08_aDryNeckBuzzesMore | **New** cross-spec row |
| Old in-loop clipper uses the same setup | – | yes | LuthierEngine.cpp:530-547 | – | – | |
| Test: heatmap matches audio | 9 | yes | – | – | Buzz.theHeatmapAgreesWithTheGenerator | |
| Test: Player-friendly only when attacked hard | 6.1 | yes | – | – | Buzz.playerFriendlyBuzzesOnlyWhenAttackedHard | |
| Test: block-rate sensing within 0.2 units | 9 | partial | – | – | PerfBudget.scenarioTotals "realism" scenario (buzz threshold 0) | Scenario total only, no FretBuzz module row |
| Test: no audio-thread allocation | 9 | partial | – | – | Engine.fiveMinutes… (default patch) | Indirect |

---

### slide-guitar.md

**Summary:** **The Workshop slide part is now audible.** `LuthierAudioProcessor::setSlidePart` → `slideBarFor` → `LuthierEngine::setSlideBar` → `SlideEngine::setBar` (PluginProcessor.cpp:520-553), tested by WorkshopAccessories.theSlideTurnsOnTheBenchAndItsMaterialIsPlayed. Two more rows closed:
- Live capture of bar movement is in (LuthierEngine.cpp:1714).
- The low-action test now asserts that the setup is unchanged.

Still open:
- The tuning popover has no Slide-Mode branch.
- There is no high-pressure choke.
- The ID is still `slide_guitar`.
- There is no allocation test.
- The slide technique controls exist only on techniques.

Counts: **yes 22 / partial 2 / no 3**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Modes bottleneck/lap/dobro/hybrid | 1 | yes | DSP/Slide/SlideEngine.h:`SlideMode` | CHARACTER → SLIDE | Slide.* | |
| Hybrid routing | 1 | yes | `isUnderBar` | – | Slide.squeakStopsUnderTheBarButNotBesideIt | |
| Bar = Workshop part (material/mass/length/diameter) | 2 | yes | PluginProcessor.cpp:520 `slideBarFor`, :542 `setSlidePart`; LuthierEngine.h:224 `setSlideBar` | WORKSHOP slide accessory (needs Slide Mode) | WorkshopAccessories.theSlideTurnsOnTheBenchAndItsMaterialIsPlayed | **New** (visual merge) |
| Material table (7) | 2.1 | yes | `getSlideMaterial` | via part | Slide.theBarClanksWhenItLands | |
| Continuous pitch | 3 | yes | `contactFret` | – | Slide.pitchIsContinuous | |
| Damping behind the bar | 3 | yes | `sustainScale` | CHARACTER | Slide.theSegmentBehindIsDamped | |
| Contact loss by material and mass | 3 | yes | `sustainScale` | mass via Workshop part | Slide.aHeavierBarSustainsLonger | Mass now reachable |
| Pressure: too light rattles, too heavy chokes | 3 | partial | SlideEngine.cpp:194 (0.85 + 0.15·p, monotonic), :209 rattle | CHARACTER | SlideUi.pressureSaysWhatItMeans | No choke at high pressure |
| Slant | 3.1 | yes | – | CHARACTER | Slide.slantGivesEachStringItsOwnInterval | |
| Slide vibrato in mm | 3.2 | yes | `vibratoCents` | Adv vibrato | – | |
| Intonation assist, 120 ms | 4 | yes | `assist` | CHARACTER | Slide.theAssistPullsToPitch; Slide.theBarArrivesWithinTwoCents | New QA test |
| Friction noise | 5.1 | yes | LuthierEngine.cpp:1528 | CHARACTER | – | Scaled by the per-string `aging…squeakScale` |
| Clank + rattle | 5.2 | yes | `makeClank` | CHARACTER | Slide.theBarClanksWhenItLands | |
| Low-action message; setup not changed | 6 | yes | `kLowActionMessage` | CHARACTER → SLIDE | SlideUi.theSlideGroupAppearsWithSlideModeAndTheTabFitsIt (asserts `setup_action_bass` unchanged) | Test gap closed |
| Buzz active under a slide | 6, 8 | yes | – | – | – | |
| 8 params | 7 | partial | Parameters.h:126 `slide_guitar`; :196 comment "slide_enabled is the existing slide_guitar switch" | CHARACTER, header, Adv | – | ID differs from the spec (deliberate, documented) |
| Advanced ranges (slide) | 7 | yes | PhysicalRange | padlock CHARACTER + WORKSHOP | Ranges | |
| SLIDE group only in Slide Mode | 7 | yes | CharacterPanel.cpp:348 `addChildComponent` | CHARACTER | SlideUi.theSlideGroupAppears… | |
| Header toggle + `S` | 7 | yes | HeaderBar.cpp | Header | – | |
| Workshop Slide category only in Slide Mode | 7 | yes | WorkshopPanel.cpp:1540 | Workshop | WorkshopPanel.aSlideNeedsSlideMode | |
| Fretboard bar overlay | 7 | yes | GuitarRenderer.cpp:2851-2858 | Fretboard / illustration | LiveDisplays.theFretboardDrawsTheSlideBarAndTheCircuitCurveFollowsTheVolume | Newly tested |
| Tuning popover shows continuous pitch / glide target | 7 | no | GuitarBodyComponent.h:49 `TuningPopover` (no slide branch) | no | – | |
| Squeak suppressed on contacted strings only | 8 | yes | – | – | Slide.squeakStopsUnderTheBarButNotBesideIt | |
| MIDI export SLIDE_BAR / pitch bend | 8 | yes | LuthierEngine.cpp:1714 `perfCapture->slideBar`; Capture/PerformanceCapture.cpp:325 | MIDI OUT | Capture.bassAndSlideEventsBecomeLuthierEvents | Live capture **new**. The test is at capture level, not engine level |
| Test: mode switch mid-note clean | 9 | yes | – | – | Slide.switchingModeMidNoteIsClean | |
| Test: no audio-thread allocation | 9 | no | – | – | – | Engine.fiveMinutes… never enables Slide Mode |
| Slide technique controls (sources, gestures) | beyond spec | no | – | no | – | In progress on techniques (`SlideControlSettings`, `SlideGesture`, `SlidePage`, SlideTechniqueTests.cpp) |

---

### bass-techniques.md

**Summary:** This spec changed the most. The model-gaps merge brought the following, with the `BassTechniqueTests` suite (16 tests):
- `SlapGroup`, the bass step grid on RHYTHM, bass genre kits.
- `BassFingerstyle` (alternation, rest stroke), `BassFamilyDefaults`, the `PalmMuteBass` profile.
- Live BASS_TECH capture.

The rest_stroke / finger_alternation_variation ID clash with realism-b was resolved to one declaration: `bassRestStroke` aliases `"rest_stroke"`, Parameters.h:426.

Remaining:
- The 13 slap arming/trigger parameters (`slap_armed`…`slap_body_part`) still have no control. They are in progress on techniques (SLAP sub-tab).
- The fixed empty-state message is a constant that is never displayed.
- Slap/pop position ranges are 5-400 mm (spec 20-200 / 10-150) and not in the `buzz` family.
- Double-thump spacing uses `slap_rebound_gap`, not `1/(2·strum_crossing_sps)`.

Counts: **yes 24 / partial 4 / no 1**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Family detection from the spec | 1 | yes | `SlapGroup::isBassLoaded`; `spec.category == Bass` | – | TunePlayer.theBassGoesToTheEngineOnlyForABass; BassTechniques.inertOnAGuitar | |
| Inert on a guitar + fixed empty-state message | 0.1 | partial | UI/SlapGroup.h:45 `kInactiveMessage` | Group hidden on a guitar | BassTechniques.inertOnAGuitar; BassTechniqueTests.cpp:936 (string check only) | The message is never drawn anywhere (grep finds only the constant and the test) |
| Slap collision via the buzz generator | 2.1 | yes | SlapEngine.cpp:`shapeExcitation` | – | Slap.theClackIsTheFretBuzzGenerator; SlapWiring.theClackComesFromTheBuzzGenerator | |
| `slap_strength/position_mm/thumb_hardness/fret_contact` | 2.2 | partial | Parameters.cpp:771 (position 5-400) | **CHARACTER → SLAP (bass only)** | SlapPresets.everySlapFieldRoundTrips; BassTechniques.aSlapIsNotALoudPluck | GUI **new**. Range 5-400 vs spec 20-200. Not in the `buzz` PhysicalRange family (spec 11) |
| Pop: brighter and shorter | 3 | yes | SlapEngine pop | CHARACTER → SLAP | BassTechniques.aPopIsBrighterAndShorterThanASlap | |
| `pop_strength`, `pop_position_mm` | 3 | partial | Parameters.cpp:775 (5-400) | CHARACTER → SLAP | – | Range vs spec 10-150; not in the buzz family |
| Double thump up ×0.65, brighter | 4 | yes | SlapEngine.cpp | CHARACTER → SLAP | Slap.theUpStrokeComesAtItsGapAndItsRatio | |
| Double-thump spacing = 1/(2·strum_crossing_sps) | 4 | partial | SlapEngine.cpp:498 `reboundGapMs` | `slap_rebound_gap` has no control (techniques) | SlapWiring.theDoubleThumpComesBackAtItsGap | Not tied to the strum crossing speed |
| Ghost: damping before strike | 5 | yes | `SlapEngine::applyGhostDamping` | – | SlapWiring.aGhostIsAThumpWithNoPitch | |
| `ghost_level/damping/auto/velocity_threshold` | 5 | yes | Parameters.cpp | CHARACTER → SLAP | BassTechniques.autoGhostingFiresBelowTheThresholdOnly | GUI **new** |
| BASS_TECH triggers a ghost | 5 | yes | Export classes | – | MidiExport.everyEventClassRoundTripsWithEveryField | |
| Finger alternation (`finger_alternation_variation`) | 6 | yes | DSP/Slap/BassFingerstyle.h; LuthierEngine.cpp:1199-1203 | CHARACTER → SLAP "Fingerstyle" | BassTechniques.fingerAlternationVaries | **New**. The value is also read by realism-b's RightHand (Parameters.cpp:1711) |
| Rest stroke damps the next-lower string | 6 | yes | LuthierEngine.cpp:1489-1494 (BassFingerstyle) | CHARACTER → SLAP | BassTechniques.theRestStrokeDampsTheNextLowerString | **New**. See the unspecified gaps: realism-b's RightHand also forces a rest stroke on a bass finger (LuthierEngineRealismB.cpp:514, 693), so both paths may damp |
| Bass pluck position 0.12 | 6 | yes | BassDefaults.cpp:19, 69 | – | BassTechniques.bassDefaultsApplyOnLoad | **New** |
| Pick bass 1.14 mm, more click | 7 | yes | BassDefaults.cpp:73-79 | – | BassTechniques.bassDefaultsApplyOnLoad (BassTechniqueTests.cpp:678) | **New** |
| Bass palm-mute profile | 7 | yes | StringEngine.h:94 `Damping::PalmMuteBass`; LuthierEngine.cpp:1372 | – | BassTechniques.aBassPalmMuteIsShorterAndDarker | **New** |
| Bass strum 100 sps / 0.01 | 8 | yes | `retargetStrumDefaults` | RHYTHM | StrumDynamics.bassDefaultsApply | |
| Bass squeak 0.35 | 8 | yes | BassDefaults | – | BassTechniques.bassDefaultsApplyOnLoad | **New** |
| Bass setup Factory low | 8 | yes | BassDefaults.cpp:84-92 | – | same | **New** |
| Bass 864 mm scale | 8 | yes | factory bass neck parts | – | BassTechniqueTests.cpp:683 (engine spec 864) | Now engine-side, not just rendering |
| Bass compressor on, 2:1 | 8 | yes | BassDefaults.cpp:96-125 (pre slot 1) | Pedal rack | BassTechniques.bassDefaultsApplyOnLoad | **New** |
| SLAP group on CHARACTER (bass only) | 9 | yes | UI/SlapGroup.cpp; CharacterPanel.cpp:353 | CHARACTER | BassTechniques.theSlapGroupIsShownOnlyOnABass | **New** |
| Bass step grid on RHYTHM | 9 | yes | Rhythm/BassStepGrid.*; UI/BassGridGroup.*; RhythmPanel.cpp:610 | RHYTHM (bass only) | BassTechniques.theStepGridPlaysItsTechniquesOnABass; …RoundTripsAndEmptyLeavesThePattern; …aGridTechniqueIsAStrikeOnABassOnly | **New** |
| Genre kits gain bass kits | 9 | yes | Rhythm/GenreKit.cpp:372-379 | RHYTHM kit browser | BassTechniques.theBassKitsInstallTheirGrids | **New** |
| MIDI export BASS_TECH | 10 | yes | Export/LuthierMidiEvents.cpp | MIDI OUT | Capture.bassAndSlideEventsBecomeLuthierEvents | |
| Live capture of BASS_TECH | 10 | yes | LuthierEngine.cpp:1664-1669 `captureBassTechnique` | – | BassTechniques.aStrikeIsCapturedAsBassTech | **New** |
| Tests: not a loud pluck, headroom, alternation, rest stroke, defaults on load | 12 | yes | – | – | BassTechniqueTests.cpp (16 tests) | **New** |
| Tests: slap uses FretBuzz, double thump, ghosts pitchless | 12 | yes | – | – | SlapTests.cpp | |
| Slap arming/trigger controls (`slap_armed`, `slap_type`, `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`, `slap_ghost_cc`, `slap_force`, `slap_palm_position_mm`, `slap_string_mask`, `slap_ghost_mode`, `slap_rebound_gap`, `slap_snap_back`, `slap_body_part`) | beyond spec (slap trigger spec) | no | Parameters.h:337-349; engine-wired | **no control on HEAD** | – | In progress on techniques (`SlapPage` in UI/Techniques/TechniquePages.*). BETA_TEST_REPORT B-11 lists these as GuiReach failures |

---

### string-aging.md

**Summary:** Fully merged from realism-a:
- The `StringAging` model: per-string state, coating, curves, legacy blend, detune and intonation slope.
- Squeak reconciliation, the five parameters and the `strings` range family.
- Restring one/all with one undo, accrual, the Adv col 1 "Age (h)" knob, the CHARACTER → STRING AGING group, serialization and SA-01..16.

Still missing:
- The Workshop strings-inspector mirror.
- False beating, which the spec defers.

The legacy `string_age` is now inert with no control. `GuiReach.everyAutomatableParameterHasAVisibleControl` will likely report it as "no control", because it is not in `intentionallyHidden()` (not run). BETA_TEST_REPORT B-15 is still OPEN: legacy "Old" on a P-Bass silences notes above about A3. SA-02 requires the legacy anchors to null against the pre-spec path, so the new model reproduces that behaviour.

Counts: **yes 21 / partial 0 / no 2**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Legacy `string_age` inert after load, kept for migration | 1 | yes | Parameters.cpp:481 (declared); Parameters.cpp:1754 (no longer structural); PresetManager.cpp:679 migration | no control (by design) | StringAging.SA02_aLegacyPresetNullsAgainstThePreSpecPath | Not in GuiReach's `intentionallyHidden()`, so that test probably fails on it |
| Age roughness into squeak/chirp (+ centroid) | 1, 3.5 | yes | PlayingNoise.cpp:29-32 | – | StringAging.SA13_squeakReconciliation | |
| `StringAging` class | 5 | yes | DSP/String/StringAging.* | n/a | SA-01..16 | |
| Effective hours per string (weights, seed jitter) | 3.1 | yes | LuthierEngine.cpp:832-869 | – | StringAging.SA07_woundStringsDullFaster; SA10_theSeedIsDeterministic | |
| Coating rate | 3.1-3.2 | yes | `setStringInfo(…, Coated)` | CHARACTER | StringAging.SA08_coatingIsARate | |
| Physical curves B/S/D/P/C | 3.2 | yes | StringAging.cpp | – | SA03, SA04, SA05, SA06 | |
| Legacy blend by `string_age_detail` | 3.3 | yes | StringAging.cpp:8 | CHARACTER | SA01_legacyAnchorsAreExact | |
| Open-string detune + intonation slope | 3.4 | yes | `TuningEngine::setAgingIntonation`; LuthierEngine.cpp:838 | – | SA05 / SA16 | |
| Squeak level/centroid | 3.5 | yes | – | – | SA13 | |
| 5 params | 4 | yes | Parameters.cpp (string_age_hours, corrosivity, detail, coating, accrual) | CHARACTER → STRING AGING (RealismGroups.cpp:59-67) | GuiReach | |
| `strings` range family | 4 | yes | PhysicalRange.h `RangeFamily::strings` | CHARACTER padlock | SA15_theStringsRangeFamily | |
| `StringEngine::setAgingFactors` | 5 | yes | StringEngine.h:185 | – | – | |
| Block-rate advance | 5 | yes | LuthierEngine.cpp:1964 `aging.advance` | – | SA11_accrualIsPlayedTime | |
| Restring one / all (one undo) | 6 | yes | RealismGroups.cpp:92-100 | CHARACTER | RealismUi.restringAllZeroesTheHoursAndTheState; SA09 | |
| Accrual of played time | 6 | yes | `aging.advance(levels)` | CHARACTER toggle | SA11 | |
| Adv col 1 age slider = hours | 7 | yes | AdvancedPanel.cpp:514-520 | Adv col 1 "Age (h)" | – | |
| CHARACTER → STRING AGING group, per-string rows, 4 Hz | 7 | yes | UI/RealismGroups.cpp `StringAgingGroup` (startTimerHz 4) | CHARACTER | RealismUi.theCharacterPanelCarriesTheGroups | |
| Workshop strings-inspector mirror | 7 | no | WorkshopPanel has no aging readout | no | – | |
| Padlock covers `strings` | 7 | yes | AdvancedPanel.cpp:1171 | CHARACTER tab padlock | SA15 | Not on the WORKSHOP padlock |
| `character.aging` block + legacy migration | 8 | yes | PluginProcessor.cpp:2761, 2803; PresetManager.cpp:676-684 | – | SA02 | |
| Hours smoothing, budget | 9 | yes | – | – | SA14_automatingHoursDoesNotClick; SA16_budgetAndSafety | |
| Tests SA-01..16 | 10 | yes | Tests/StringAgingTests.cpp | – | all 16 present | |
| False beating | 3.6 | no | – | – | – | Deferred by the spec |

---

### environment.md

**Summary:** Merged from realism-a:
- `Character/EnvironmentModel.*`: thermal detune from each string's strain and alpha, the four lags, EMC, geometry deltas into FretBuzz, fretting stretch, body scaling (shared with coupling), the corrosion hook.
- The six profiles, the host/free-running clock, Retune (one undo) and the five params with their range family.
- The CHARACTER → ENVIRONMENT group (10 Hz readouts, sparkline, convolution note).
- Legacy migration and ENV-01..15.

The old CHARACTER temperature/humidity combo boxes are gone. `getTemperatureOffsetCents` has no caller.

Still missing: the SETUP secondary "+x mm (humidity)" line. Also, the Luthier-events MIDI "environment" message still reports the legacy `CharacterEngine` enum (see the unspecified gaps).

Counts: **yes 20 / partial 0 / no 1**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Replace the three-step temperature placeholder | intro | yes | CharacterPanel has no temperature box; the legacy value is only migrated | – | Environment.ENV11_legacyTemperatureAndHumidityMigrate | The `CharacterEngine::Temperature` enum survives for load and MIDI export only |
| Replace the dead humidity multipliers | intro | yes | PluginProcessor.cpp:2838-2848 (legacy → 45 % RH, logged) | – | ENV11 | |
| `EnvironmentModel` | 4 | yes | Character/EnvironmentModel.*; LuthierEngine.cpp:75, 1951 | – | ENV01_theRoomIsANoOp | |
| Thermal detuning from strain | 2.1 | yes | LuthierEngine.cpp:840-853 | – | ENV02_steadySlopeFromTheStringsOwnNumbers | |
| Lags wire/neck/body/wood | 2.2 | yes | – | – | ENV03_coldCaseOvershootsThenSettles; ENV06_woodIsSlow | |
| EMC; relief/top/action deltas → FretBuzz | 2.3-2.4, 4 | yes | LuthierEngine.cpp:1981-1984 | – | ENV05; ENV08_aDryNeckBuzzesMore | |
| Fretting-stretch intonation | 2.5 | yes | LuthierEngine.cpp:2043 `environment.fretCents` | – | ENV07 | |
| Body plate/air/Q multipliers → BodyEngine + bank | 2.6 | yes | LuthierEngine.cpp:1967-1971 | – | ENV04_theAirModeFollowsTheSpeedOfSound; BodyCoupling.ENV13_theWolfFollowsTheEnvironment | |
| k_RH corrosion hook | 2.7 | yes | LuthierEngine.cpp:1956 | – | StringAging.SA12_humidityDrivesCorrosion | |
| Session profiles (6) | 3.1 | yes | Parameters.cpp:213 `envProfileNames` | CHARACTER → ENVIRONMENT | ENV09 | |
| Tuned-at + Retune (one undo) | 3.2 | yes | RealismGroups.cpp:321-324 | CHARACTER | RealismUi.retuneWritesTunedAtAndZeroesTheOffsets; ENV10 | |
| Closed form, seekable host clock / free-running | 3.3-3.4 | yes | `environment.advance(seconds, hostTime, playing)` | CHARACTER "Profile Clock" | ENV09_seekingIsDeterministic | |
| Runs with character disabled | 0.4, 4 | yes | – | – | ENV14_independentOfCharacter | |
| 5 params | 5 | yes | Parameters.cpp:810-814 | CHARACTER (RealismGroups.cpp:271-279) | GuiReach | |
| `environment` range family | 5 | yes | PhysicalRange.h | CHARACTER padlock | ENV15 | |
| Serialization + legacy conversion | 6 | yes | PluginProcessor.cpp (REALISM-A state) | – | ENV11 | |
| ENVIRONMENT group, 10 Hz readouts, 60 s sparkline | 7 | yes | RealismGroups.cpp `EnvironmentGroup`, `EnvironmentSparkline` | CHARACTER | RealismUi.theCharacterPanelCarriesTheGroups | |
| Convolution-mode note | 7 | yes | RealismGroups.cpp:295, 373 | CHARACTER | – | |
| SETUP secondary "+x mm (humidity)" line | 7 | no | SetupGroup.cpp has no humidity text | no | – | Deferred on realism-a, still missing |
| NaN guards, clamps, budget | 8 | yes | – | – | ENV15_budgetSafetyAndCorners | |
| Tests ENV-01..15 | 9 | yes | EnvironmentTests.cpp (14) + BodyCouplingTests.cpp (ENV13) | – | | |

---

### body-coupling.md

**Summary:** Merged from realism-a:
- `DSP/Coupling/BodyCouplingBank.*`, designed off-thread from `waveImpedance` and staged (LuthierEngine.cpp:655-665).
- Driven per sample from `getBridgeWave()` (:2638-2648).
- Tap injection from the slap body part and the Tap button (`driveDirect`, :1619, :1978).
- Shared scaling with the environment, the five params, the `body` family, the Adv col 1 BODY "Coupling" knob, and the BODY COUPLING group (scales, mode list, wolf map, Tap).
- Legacy load writes 0, and BC-01..13.

Still missing:
- The Workshop body-inspector mirror.
- Factory re-voicing. FactoryPresets.cpp:765-779 strips `body_coupling_amount`, so every factory preset loads with coupling 0 while Init has 1.0.

Counts: **yes 16 / partial 0 / no 2**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Bridge admittance bank from BodyModels | 2.1-2.2 | yes | DSP/Coupling/BodyCouplingBank.* | – | BC01_theBankIsPassive; BC02_aWolfOnAnAcoustic | |
| Loaded passive discrete form, cap | 2.3 | yes | `design` | – | BC01; BC10 | |
| Tap tones | 2.4 | yes | LuthierEngine.cpp:1619, 1978 `driveDirect` | Tap button; slap body part | BC06_BC09_aTapRingsTheStringsNearAMode | |
| `waveImpedance`, `getBridgeWave` | 3 | yes | StringEngine.h:47, 208 | – | – | Loop-return variant per the amended spec |
| Per-sample insertion | 3 | yes | LuthierEngine.cpp:2638-2648 | – | BC07_sympatheticRingThroughTheBody | |
| Redesign on rebuild / refresh / part swap | 3 | yes | LuthierEngine.cpp:665 `stage(design(...))` | – | BC05_theWolfMovesWithTheBody | |
| Shared scaling with BodyEngine | 3 | yes | :1967-1971 `setScaling` | – | ENV13 | |
| 5 params | 4 | yes | Parameters.cpp:816ff | Adv col 1 + CHARACTER | GuiReach | |
| `body` range family | 4 | yes | PhysicalRange.h | CHARACTER padlock | BC10 | |
| Adv col 1 BODY → Coupling knob | 5 | yes | AdvancedPanel.cpp:471 | Adv col 1 | – | |
| BODY COUPLING group: scales, mode list | 5 | yes | RealismGroups.cpp:502-507 `BodyCouplingGroup`, `BodyModeList` | CHARACTER | RealismUi.theCharacterPanelCarriesTheGroups | |
| Wolf map | 5 | yes | `WolfMap`, `computeWolfMap` | CHARACTER | BC11_theWolfMapIsHonest | |
| Tap button | 5 | yes | RealismGroups.cpp:520 | CHARACTER | – | |
| Workshop body-inspector mirror | 5 | no | – | no | – | |
| Legacy load writes 0 | 6 | yes | PresetManager.cpp:688-689 | – | BC12_legacyLoadIsOff | |
| Factory presets re-voiced with coupling | 6 | no | FactoryPresets.cpp:765-779 removes the key → 0 | – | – | Deferred to a listening pass. The feature is off in every factory sound |
| Smoothing | 7 | yes | – | – | BC04_offIsBitIdentical | |
| Tests BC-01..13 | 8 | yes | BodyCouplingTests.cpp | – | 13 tests (BC-06/09 combined) | |

---

### Top gaps (group E)

Ranked by user impact.

1. **The slap arming/trigger family and all `scrape_*` controls have no control on HEAD.** That is 13 slap params (`slap_armed`, `slap_type`, `slap_trigger`, `slap_rebound_gap`…) and 14 scrape params. The engine is fully wired, but a user cannot arm a slap or a scrape, and `GuiReach.everyAutomatableParameterHasAVisibleControl` fails on them (BETA B-11). This is in progress on techniques (`SlapPage`, `ScrapePage`).
2. **Body coupling is off in every factory preset.** The factory builder strips `body_coupling_amount`, so wolf notes and sympathetic body ring exist only in Init and user patches, pending the listening pass.
3. **B-15: legacy "Old" strings silence a bass above about A3** (P-Bass Flatwound). SA-02 pins the legacy path, so the new aging model reproduces the bug. `Combo.everyFactoryPresetPlaysEveryPhrase` fails on it.
4. **There is no Easy or UI rake gesture.** `ScrapeEngine::requestRake` has no UI caller, even on techniques. The rake needs `scrape_armed` plus keyswitch 13/14.
5. **The default pick thickness is 1.07 mm, not 0.73 mm.** `pick_thickness` defaults to normalised 0.5, and `BassFamilyDefaults` treats that as the guitar value.
6. **Pick materials Ultex, Tortex and Stone/horn are missing.**
7. **Squeak triggers only on legato-slide techniques.** Tab/PerformanceScore position shifts and ChordVoicer revoicing never squeak.
8. **The tuning popover ignores Slide Mode.** There is no continuous pitch or glide target.
9. **The CHARACTER tab has no CIRCUIT mirror** (gui-integration 4.4 and 19).
10. **Fret-buzz interactions are missing:** fret-material brightness, fret-wear redistribution, bends raising clearance.
11. **The MIDI-export "environment" event reports stale data.** It sends `CharacterEngine::getTemperature()`/`getHumidity()`, the legacy enum, which is only set by migration, not `env_temperature_c`/`env_humidity_pct`. Receivers see "room / normal" whatever the ENVIRONMENT group says. ModelGapsUi.characterSeedAndEnvironmentGoOutAsTheyChange pins the legacy behaviour.
12. **The legacy `string_age` has no control and is not in GuiReach's `intentionallyHidden()`.** Expect a GuiReach failure.
13. **Slap/pop position ranges are 5-400 mm** (spec 20-200 / 10-150) and not in the `buzz` range family. **Double-thump spacing** ignores `strum_crossing_sps`.
14. **The bass empty-state message is never shown.** `SlapGroup::kInactiveMessage` is defined and tested as a string, but nothing draws it.
15. **Workshop inspector mirrors are missing** for strings (aging rows) and body (mode list/wolf map). The environment SETUP "+x mm (humidity)" line is also missing.
16. **The pick illustration handles do not match the spec.** The body drag sets pluck position and the handle turns the angle, but no handle sets thickness. The colour ignores material.
17. **The circuit is re-solved on the audio thread** (block-rate matrix inverse). `macro_tone` rescales the tone wiper. The custom bleed R/C knobs are always visible.
18. **Missing spec tests:**
    - noise through two bodies
    - no-allocation tests for squeak and slide (pick and buzz are covered only indirectly)
    - a FretBuzz module budget row
    - vibrato does not squeak
    - 50s wiring

### Unspecified gaps noticed

- **Rest stroke may apply twice on a bass.** `BassFingerstyle` damps the next-lower string (LuthierEngine.cpp:1489-1494). realism-b's RightHand path also forces a rest stroke for a bass finger when `rest_stroke` is on (`bassRestStroke` aliases the same ID; LuthierEngineRealismB.cpp:514, 693), and adds its own level/contact scaling and `borrowDamping`. The realism-b path lacks the double-stop exception. Not run, so the audible effect is unverified.
- **The environment has no MIDI-out.** `env_*` changes are not exported (see top gap 11), so a DAW recording of Luthier events cannot reproduce the tuning drift.
- **No global "playing noise" master or clean-DI realism profile.** Aging, environment and coupling added three more realism dimensions, each with its own style box but no single "studio clean" switch.
- **The noise-event strip is not inspectable** (no hover for string or level).
- **No bass amp/DI default** beyond the pre-slot compressor.
- **Slide Mode has no MIDI-learnable momentary.**
- **The wolf map is not audible on demand.** The Tap button taps the body, but clicking a wolf cell does not play that note.

### Small glue candidates

| Item | Param IDs / symbol | Where it should go | Effort |
|---|---|---|---|
| Merge the techniques SLAP and SCRAPE pages, or cherry-pick them as a stopgap | `slap_armed`…`slap_body_part`, `scrape_*` | TECHNIQUES tab (UI/Techniques/TechniquePages.*) | small (merge) |
| Easy / SCRAPE-page "Rake ↓ / Rake ↑" buttons | `ScrapeEngine::requestRake(bool)` | Easy playing strip and the techniques ScrapePage | tiny |
| Show the bass empty-state | `SlapGroup::kInactiveMessage` | Draw it in place of the hidden SLAP group (or on the techniques SLAP page) when not a bass | tiny |
| Mark `string_age` intentionally hidden | `"string_age"` | GuiReachabilityTests.cpp `intentionallyHidden()` with the "string-aging 1: inert, kept for migration" reason | tiny |
| Environment MIDI export from the real params | `env_temperature_c`, `env_humidity_pct` in place of `character.getTemperature()/getHumidity()` | PluginProcessor.cpp:3173-3180 (+ update the ModelGapsUi test) | tiny |
| Pick thickness default | `pick_thickness` default 0.5 → `BassFamilyDefaults::pickThicknessNormalised(0.73)` ≈ 0.316 | Parameters.cpp:494 (presets pin their own value) | tiny |
| Bleed editor visibility | `circuit_bleed_r/_c/_mode` visible only when `circuit_treble_bleed` = Custom | AdvancedPanel.cpp:753-758 | tiny |
| CHARACTER CIRCUIT mirror | the `circuit_*`, `cable_*`, `guitar_volume/tone` builders + `CircuitResponseView` | CharacterPanel new group | small |
| Slap/pop ranges | `slap_position_mm` 20-200, `pop_position_mm` 10-150, add both to `RangeFamily::buzz` | Parameters.cpp:771, 775; PhysicalRange.cpp | tiny (check presets for out-of-range values) |
| Tuning popover in Slide Mode | `SlideEngine::getOverlayFret` / assist target → `TuningPopover` label | GuitarBodyComponent.cpp | small |
| SETUP humidity delta line | `environment` geometry delta (the engine already computes the delta) → "+x mm (humidity)" | SetupGroup.cpp | small |
| Squeak winding → Workshop jump | the winding selector's "Edit in Workshop" | NoiseGroups.cpp:243 | small |
| Rest-stroke de-duplication | skip the realism-b bass forcing when `BassFingerstyle` handles it (or vice versa) | LuthierEngineRealismB.cpp:514 | tiny |

---

## Group F: extended realism

Checked HEAD `961cd55` (claude/luthier-audit). It now contains the merged realism-a/b/c, model-gaps, tune-help, visual, release and review branches. The only unmerged branch is `origin/claude/luthier-techniques` (`1b8bf87`), inspected with `git diff HEAD...origin/claude/luthier-techniques`. "in progress on techniques" marks items that exist only there.

Specs in this group: harmonic-realism, string-interaction, fingerstyle-attack, noise-floor, sustain-and-decay, tuning-stability, strum-dynamics. All seven were covered by the previous report, so there are no new files. Six of them gained "Build notes" or "As built" amendments since `2ede79c`: every spec in the group except strum-dynamics, which is unchanged. performance-budget.md and gui-integration.md 19 also gained rows. Where a spec was amended to match the build, the rows below are judged against the amended text, and the amendment is named in Notes.

Main change: realism-b and realism-c are merged, so about 200 rows move from "no" to "yes". What is still missing is small, apart from one real defect: the `sustain_scale` knob, found in this pass.

### harmonic-realism.md

32 yes, 1 partial, 2 no (plus 3 rows for the new build notes, all yes: 35 yes, 1 partial, 2 no).

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Contact = n-tap node comb in loop (eq 1-3) | 1,2 | yes | StringEngine::addContact / ContactState (StringEngine.h:238-390) | n/a | HR02_HR03, HR04 | |
| `Contact` struct, kMaxContacts=4, add/clear/clearAll | 2 | yes | StringEngine.h:238-243 | n/a | HR12 | |
| Contact path skipped when idle (bit-identical) | 2 | yes | StringEngine::endSample | n/a | HR16 | build note 9: all 36 factory presets bit-identical at 0 |
| g ramps over 1 ms | 2 | yes | ContactState ramp | n/a | HR13 | |
| M,n recomputed on needsLoopUpdate only | 2 | yes | StringEngine | n/a | - | untested |
| Fallback to band isolation, counted by validator | 2 | yes | Validator::getHarmonicFallbackCount | n/a | HR15 | build note 8 |
| setHarmonicRestriction kept as shim | 2 | yes | StringEngine.cpp:500 (adds a Contact) | n/a | - | |
| couplingReceptivity 0.35 while contact active | 2 | yes | StringEngine.cpp:496 | n/a | - | |
| Remove `t60Scale *= 0.55` | 2 | yes | StringEngine.cpp:758-790 (no 0.55 term) | n/a | - | |
| processSample(coupling, directInput) split | 2 | yes | StringEngine.h:282/290, write at StringEngine.cpp:1050 | n/a | HR16 | build note 7: scrape catches go through directInput |
| Natural harmonic pitch = partial n of open string | 0.2,4.1 | yes | MidiInterpreter.cpp:769-775 (touchFret, partialForFret) | CC 73 / velocity | HR01 | the octave-high bug is fixed |
| Harmonic kind renders as a plain pluck (no band isolation) | 3 | yes | LuthierEngineRealismB | n/a | HR02_HR03, HR08 | |
| Artificial harmonic (fret+offset) | 3 | yes | MidiInterpreter.cpp:778-784 | CC 103; offset in HARMONICS | HR09 | |
| Pinch: node nearest pick + thumb offset, not velocity | 3 | yes | TechniqueEngine.cpp:60-63 | CC 72; thumb offset in HARMONICS | HR10 | |
| Tapped harmonic: legato, no voice steal | 3 | yes | LuthierEngineRealismB (lastDecisionWasTappedHarmonic) | CC 104 | HR11 | spec now runs HR-11 with bridge coupling off |
| Analytic node search replaces fret table | 4.1 | yes | Harmonics.h findNode / partialForFret | n/a | HR05, HR06 | efficiency is now `exp(-(d/1.5w)^4)` with a 0.1 threshold, written into spec 3 and build note 1 |
| Sounding-pitch mapping + HarmonicLocator | 4.2 | yes | MidiInterpreter emitSoundingHarmonic (:843) | `harmonic_note_mapping` in HARMONICS | HR14 | |
| `harmonic_touch_pressure` | 5 | yes | Parameters | CHARACTER HARMONICS (HarmonicsGroup.cpp) | HR17 | |
| `harmonic_finger_width` | 5 | yes | Parameters | HARMONICS | HR07 | |
| `harmonic_touch_time` | 5 | yes | Parameters | HARMONICS | - | |
| `harmonic_brief_touch` | 5 | yes | Parameters | HARMONICS | HR10 | |
| `pinch_thumb_offset_mm` | 5 | yes | Parameters | HARMONICS | HR10 | |
| `artificial_harmonic_offset` | 5 | yes | Parameters | HARMONICS | HR09 | |
| `tapped_harmonic_offset` | 5 | yes | Parameters | HARMONICS | HR11 | |
| `harmonic_note_mapping` | 5 | yes | Parameters | HARMONICS | HR14 | |
| Physical rows in RangeRegistry (pick family) | 5 | yes | PhysicalRange.cpp | n/a | HR17 | |
| CC 103 ArtificialHarmonic / CC 104 TappedHarmonic | 6 | yes | MidiInterpreter.cpp:212-213, :1107-1108 | MIDI | HR19_FA17 (drives both CCs), HR09, HR11 | |
| decide() priority incl. artificial and tapped | 6 | yes | TechniqueEngine::decide | n/a | HR09, HR11 | |
| HARMONICS row inside PICK group, with per-fret partial tooltips | 7 | partial | UI/HarmonicsGroup.cpp (tooltips name the partial per offset, :11-14); CharacterPanel.cpp:744 | CHARACTER, a separate group, not inside PICK | RealismBUi.theCharacterTabCarriesTheThreeGroups | placement differs from the spec |
| Fretboard hollow ring at contact, dashed when missed | 7 | yes | UI/FretboardRealismB.cpp paintRealismB (reads getContactDisplay) | fretboard | RealismBUi.theFretboardDrawsTheTouch | new since last audit |
| Section 19 row in gui-integration.md | 7 | no | - | - | - | doc; realism-c's three rows were added, realism-b's were not |
| MIDI export: `touch_fret` + `partial` on NOTE | 7 | no | Export/ has no such field | n/a | - | spec build note 10 defers it to midi-export |
| reset() clears contacts | 8 | yes | resetRealismB / clearAllContacts | n/a | - | |
| Budget (now 1.0 unit at six strings, n=8), no allocation | 8 | yes | - | n/a | HR18 (<=1.0 units, heap hook) | spec 8 amended from 0.05 to 1.0; performance-budget.md has no contact row |
| HR-19 export round trip | 9 | yes | - | - | HR19_FA17_luthierExportRoundTrip | new since last audit |
| Tab reading: an integer fret within 0.35 of a node of partials 2-5 snaps to it (build note 6) | 4.1 | yes | harmonics::tabTouchFret (MidiInterpreter.cpp:773) | n/a | HR05 | new requirement |
| Pinch graze strength 0.5 g (build note 3) | 3 | yes | TechniqueEngine / LuthierEngineRealismB | n/a | HR10 | new requirement |
| Tap is an ideal damper for max(1.5 brief, 0.5 touch time) (build note 4) | 3 | yes | LuthierEngineRealismB | n/a | HR11 | new requirement |

### string-interaction.md

30 yes, 1 partial, 2 no.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Air path rank-1 term, HP 120 Hz, tau 0.29 ms | 1 | yes | CouplingMatrix (air) | n/a | SI01, SI02_SI03 | SI-01 now measured as RMS level (spec amended) |
| a_cat per family (acoustic/archtop/solid/bass) | 1 | yes | CouplingMatrix | n/a | SI01 | |
| Air through receive filter and kEnergyCap | 1 | yes | CouplingMatrix | n/a | - | untested |
| Palm spread w(d), P>0.05, spreadMuted flag | 2 | yes | LuthierEngineRealismB | n/a | SI04_SI05 | |
| Spread never overrides Choked/Silenced/Chuck; restores on lift | 2 | yes | LuthierEngineRealismB | n/a | SI04_SI05 | |
| Adjacent finger damping (Chuck, underside 1.0, tip 0.5) | 3 | yes | LuthierEngineRealismB.cpp:145 | n/a | SI06_SI07 | SI-06 now has the ringing string held by the sustain pedal (spec amended) |
| Fretting style scales it (rock 1.0 / classical 0.1) | 3 | partial | Parameters.cpp:1731 `interaction.frettingStyle = rh_style==Classical ? 0.1 : 1.0` | shown as text in STRING INTERACTION (StringInteractionGroup.cpp:54) | SI06_SI07 | proxy accepted by build note 3; the real source `mute_fretting_style` is in progress on techniques, and nothing wires it through yet |
| Chord's own notes do not mute each other | 3 | yes | LuthierEngineRealismB | n/a | SI06_SI07 | |
| Release stagger (3 ms groups, bias, seeded) | 4 | yes | LuthierEngine stageNoteOffs | n/a | SI08_SI09 | |
| Pickup crosstalk Gaussian aperture, bend lateral offset | 5 | yes | PickupEngine::setStringLateralOffsets / apertureGain | n/a | SI10 | interface in build note 2 |
| Muted-string thump, live strum | 6 | yes | MidiInterpreter + MutedThump.h | n/a | SI11 | |
| Muted-string thump, rhythm engine | 6 | yes | RhythmEngine | n/a | SI11 | |
| deadStrike skips activity, MIDI note and MIDI out | 6 | yes | LuthierEngine | n/a | SI11 | |
| Thump span: outside the struck span, one string interval beyond (build note 1) | 6 | yes | MutedThump.h interpolation | n/a | SI11 | new requirement |
| `coupling_air_amount` | 7 | yes | Parameters | CHARACTER STRING INTERACTION | SI02_SI03 | |
| `palm_mute_spread` | 7 | yes | Parameters | STRING INTERACTION (+ palm strip) | SI04_SI05 | not mirrored in MUTE (below) |
| `adjacent_mute_amount` | 7 | yes | Parameters | STRING INTERACTION | SI06_SI07 | |
| `release_stagger_ms` | 7 | yes | Parameters | STRING INTERACTION | SI08_SI09 | |
| `release_stagger_bias` | 7 | yes | Parameters | STRING INTERACTION | SI08_SI09 | |
| `pickup_aperture_scale` | 7 | yes | Parameters | STRING INTERACTION | SI10 | |
| `muted_thump_level` | 7 | yes | Parameters | STRING INTERACTION | SI11 | |
| Range rows (pick/squeak/circuit) | 7 | yes | PhysicalRange.cpp | n/a | SI12_SI14 | |
| Defaults on (air 1.0, palm 35 mm, neighbour 0.6, stagger 12 ms, thump 0.5): factory renders move slightly (build note 4) | 7 | yes | Parameters defaults | - | SI12_SI14; docs/coverage/REALISM-B.md deltas | this is the only group-F realism spec that ships audible by default |
| STRING INTERACTION group on CHARACTER | 9 | yes | UI/StringInteractionGroup.cpp | CHARACTER | RealismBUi.theCharacterTabCarriesTheThreeGroups | |
| Mute-zone shading shows palm width and weights | 9 | yes | FretboardRealismB.cpp paintRealismB (palm band); StringInteractionGroup palm strip | fretboard + CHARACTER | RealismBUi.theFretboardDrawsTheTouch | new since last audit |
| Palm width mirrored in TECHNIQUES->MUTE | 9 | no | - (techniques' MuteGroup.cpp has palm position/pressure only) | **no** | - | not on techniques either |
| Section 19 rows | 9 | no | - | - | - | build note 5 leaves it to the owner of gui-integration.md |
| Flags cleared by reset, preset load and panic | 9 | yes | resetRealismB | n/a | - | untested |
| Budget (0.2 units at 12 strings), no allocation | 10 | yes | - | n/a | SI13_realtime (<=0.2, heap hook) | section 10 amended to 0.2, but **SI-13's own text in section 11 still says 0.1**, so the spec contradicts itself; no performance-budget.md row |
| SI-12 per-string sum at -80 dBFS with every feature at default | 11 | yes | - | - | Routing.perStringOutputsSumToPreBody (processor defaults, so air/palm/stagger are on) | covered implicitly; no SI-named assertion |
| SI-14 preset round trip | 11 | yes | - | - | SI12_SI14 | |
| No new MIDI; CC 67 drives spread | 8 | yes | MidiInterpreter | MIDI | SI04_SI05 | |

### fingerstyle-attack.md

42 yes, 2 partial, 2 no.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| `Params::releaseSeconds`, lowpass 1/(2 pi tau) | 1 | yes | Excitation.h:72-77 | n/a | FA03 | |
| Full MaterialSpec blend by nail_vs_flesh (no step at 0.5) for Finger/Thumb tools | 1 | yes | LuthierEngineRealismB.cpp:569-621 | CHARACTER PICK / RIGHT HAND | FA01_FA02 | build note 4: a string on Global keeps the old step (LuthierEngine.cpp:1023); FA-01/02 now measure the contact, not the note (build note 2) |
| Defaults reproduce table (2.2/7.0/1.1 kHz) | 1 | yes | Excitation / RightHand | n/a | FA04 | |
| Nail click at pick_click x0.35 x b | 1 | yes | LuthierEngineRealismB | n/a | FA10 | FA-10 level amended (47/39 dB) |
| Thumb position offset, clamp 0.02-0.5 | 1 | yes | LuthierEngineRealismB | n/a | FA14 | |
| Rest stroke terms (level 1.35, length 1.10, bright, bridge drive, sustain) | 2 | yes | Excitation kindGain / pluckLen | n/a | FA06 | spec amended from 1.26/1.15 |
| Rest-stroke neighbour Chuck (finger s+1, thumb s-1) | 2 | yes | LuthierEngineRealismB | n/a | FA07 | FA-07 window amended |
| Auto stroke rule (single note, v>=0.7) | 2 | yes | RightHand | n/a | FA08 | |
| bass `rest_stroke` bool forces Rest | 2 | yes | Parameters.cpp:1714 (hand.bassRestStroke) | bass rest-stroke control | - | untested |
| Tool resolution order CC102 > pattern finger > string > global | 3 | yes | LuthierEngineRealismB.cpp:490 | n/a | FA09, FA15 | |
| Pattern fingers win over Global strings (build note 5) | 3 | yes | RhythmEngine.cpp:424/650 -> LuthierEngineRealismB | n/a | FA09 | new requirement; it re-voices fingerpick presets |
| strikerMaterial wins for strums | 3 | yes | LuthierEngineRealismB | n/a | - | untested |
| NoteOnEvent::finger; scheduleFingerpick passes it | 3 | yes | RhythmEngine.cpp:378-424, :650 | n/a | FA09 | |
| Strings 7-12 follow course / lowest | 3 | yes | RightHand.h:58 toolForString(twelveString, course) | n/a | - | untested |
| i/m alternation (tau, position, +1.5v ms) | 3 | yes | RightHand | n/a | FA12 | FA-12 floor amended to 0.3 % |
| rh_style writes table, one undo step, never on preset load | 4 | yes | RightHandGroup.cpp:188 ScopedUndoAction | RIGHT HAND + Easy Tool | FA16 | |
| Travis thumb PalmMute | 4 | yes | LuthierEngineRealismB | n/a | FA11 | FA-11 bound amended to 0.75x |
| Hybrid snap via SlapEngine pop path | 4 | yes | LuthierEngineRealismB | n/a | FA13 | |
| Slap/Pop tools on any family | 4 | yes | LuthierEngineRealismB | n/a | FA13 | |
| CC 102 RightHandTool (7 bands) | 5 | yes | MidiInterpreter.cpp:211 | MIDI | FA15, HR19_FA17 | |
| CC 105 RestStroke | 5 | yes | MidiInterpreter.cpp:214 | MIDI | FA15, HR19_FA17 | |
| `finger_flesh_release_ms` | 6 | yes | Parameters | RIGHT HAND (+kHz readout) | FA03 | |
| `finger_nail_release_ms` | 6 | yes | Parameters | RIGHT HAND | FA03 | |
| `thumb_position_offset` | 6 | yes | Parameters | RIGHT HAND | FA14 | |
| `rest_stroke_damping` | 6 | yes | Parameters | RIGHT HAND | FA07 | |
| `rh_stroke` | 6 | yes | Parameters | RIGHT HAND | FA08 | |
| `rh_style` | 6 | yes | Parameters | RIGHT HAND + Easy Tool selector | FA16 | |
| `rh_string_tool_1..6` | 6 | yes | Parameters | RIGHT HAND six-cell row | FA10 | |
| `thumb_palm_mute` | 6 | yes | Parameters | RIGHT HAND | FA11 | |
| `hybrid_snap` | 6 | yes | Parameters | RIGHT HAND | FA13 | |
| `finger_alternation_variation` / `rest_stroke` reused | 6 | yes | Parameters.cpp:797-798 (now declared), :1711-1715 | RIGHT HAND alternation knob (RightHandGroup.cpp:161-164) | FA12 | build note 6's "until then 0" no longer applies |
| Range rows (pick family) | 6 | yes | PhysicalRange.cpp | n/a | FA17 | |
| RIGHT HAND group, mirrors use_fingers and nail_vs_flesh | 7 | yes | UI/RightHandGroup | CHARACTER | RealismBUi.theCharacterTabCarriesTheThreeGroups | |
| Easy Playing strip Tool selector (Custom shown as "Mixed") | 7 | yes | EasyPanel.cpp:193 RightHandToolSelector; RightHandGroup.cpp:335 "Mixed" | Easy Playing strip | RealismBUi.theEasyPlayingStripHasTheToolSelector | |
| Illustration: tool glyph at pluck point per string | 7 | partial | FretboardRealismB.cpp paintRealismB (glyph at the board's picking end) | fretboard | RealismBUi.theFretboardDrawsTheTouch | drawn at a fixed right edge, not at `pluck_position` (+thumb offset), and not on the guitar-body illustration |
| Section 19 row | 7 | no | - | - | - | doc |
| MIDI export PICK `tool`, `finger`, `stroke` | 7 | no | Export unchanged | n/a | - | build note 8 defers it to midi-export |
| reset clears rest neighbours and alternation phase | 8 | yes | resetRealismB | n/a | - | untested |
| Budget 0.02 units, no allocation | 8 | yes | - | n/a | FA17 (<=0.02) | no performance-budget.md row |
| FA-05 pick path bit-identical | 9 | yes | - | - | FA05 | |
| FA-17 Luthier-profile export round trip (-60 dBFS null) | 9 | yes | - | - | HR19_FA17_luthierExportRoundTrip | new since last audit |
| Pick-noise interplay (fingertip release noise unchanged) | 1 | yes | PlayingNoise | n/a | existing | |
| Up-strum nails / thumb strikers | 3 | yes | LuthierEngineRealismB | n/a | - | untested |
| Undo for tool cell clicks | 7 | partial | RightHandGroup.cpp:58 `write(...)` | RIGHT HAND | - | no ScopedUndoAction around the cell write; relies on per-parameter undo, not verified |
| Choice lists append-only | 6 | yes | Parameters | n/a | - | |

### noise-floor.md

42 yes, 1 partial, 0 no.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Existing hum kept (ID, range, default, constant) | 1 | yes | PickupEngine::processStrings | Advanced knob | NoiseTests, NoiseFloor.humCalibration | |
| Display name "Single-coil Hum" | 1,5 | partial | Parameters.cpp:547 renamed | **Advanced knob still labelled "Amp Buzz" (AdvancedPanel.cpp:815)** | - | label glue |
| Mains phase: the new sources run their own accumulator from reset() | 2 | yes | NoiseFloor | n/a | NoiseFloor.rendersAreDeterministic | "As built" amendment: not phase-locked to the legacy hum |
| g_pos angle/distance on hum; bit-identical at 0 deg / 1 m | 2.1 | yes | PickupEngine::setHumPositionGain | CHARACTER NOISE FLOOR position pad | positionScalesTheHum | |
| `noise_mains_hz` drives setMainsFrequency | 2.1 | yes | Parameters | NOISE FLOOR 50/60 switch | regionSetsTheHumFrequency | |
| NF-02 hum out of window, recorded | 2.1 | yes | docs/coverage/REALISM-C.md:20,91 | - | humCalibration, calibrationIsLogged | amended: recorded in coverage, not DECISIONS.md; pinned at -57.7 ±2 dB |
| Fluorescent buzz | 2.2 | yes | NoiseFloor | Guitar row | fluorescentSpectrum | |
| Passive (Johnson) hiss from live R | 2.3 | yes | NoiseFloor.cpp:62 | Guitar row | passiveHissIsPhysical | NF-11 derivation amended |
| Cable movement rolls/events, 4 voices, q per CableQuality | 2.4 | yes | NoiseFloor | Guitar row | cableMovementRollsAndScales | |
| Ground loop 2048-pt wavetable at amp input; region change = read rate, rebuilt in place | 2.5 | yes | NoiseFloor groundTable | Rig row | aHumbuckerCancelsHumButNotAGroundLoop | in-place rebuild now specified |
| Radio pickup; no radio with `cableOn` false | 2.6 | yes | NoiseFloor.cpp:421 | Guitar row | ampInputSourcesHitTheirTargets | cableOn clause is new |
| Amp hiss with pinking at amp input | 2.7 | yes | NoiseFloor | Rig row | ampGainRaisesHiss | |
| Tube microphonics, G_m<0.95 (amp gain divided out per block), x0.2 for 4x12 | 2.8 | yes | NoiseFloor, LuthierEngine | Rig row | microphonicsIsBounded | NF-08 now tested around a stand-in amp (amended) |
| `noise_player_angle` | 3 | yes | Parameters | position pad | positionScalesTheHum | |
| `noise_player_distance` | 3 | yes | Parameters | position pad | positionScalesTheHum | |
| `noise_fluorescent` | 3 | yes | Parameters | NOISE FLOOR | fluorescentSpectrum | |
| `noise_passive_hiss` | 3 | yes | Parameters | NOISE FLOOR | passiveHissIsPhysical | |
| `noise_cable_movement` | 3 | yes | Parameters | NOISE FLOOR | cableMovementRollsAndScales | |
| `noise_radio` | 3 | yes | Parameters | NOISE FLOOR | ampInputSourcesHitTheirTargets | |
| `noise_ground_loop` | 3 | yes | Parameters | NOISE FLOOR | aHumbuckerCancels... | |
| `noise_amp_hiss` | 3 | yes | Parameters | NOISE FLOOR | ampGainRaisesHiss | |
| `noise_microphonics` | 3 | yes | Parameters | NOISE FLOOR | microphonicsIsBounded | |
| `noise_floor_to_aux8` | 3 | yes | Parameters | CHARACTER (RealismGroupsC.cpp:265) + ROUTING (RoutingPanel.cpp:284) | aux8IsOptIn, RealismUi.theAux8SwitchIsMirroredOnRouting | |
| `noise_floor_style` + "(modified)" | 3 | yes | RealismStyleActions.cpp:43-65 | NOISE FLOOR dropdown | styleOffIsInert, RealismUi.theStyleBoxesApplyAndReadModified | |
| `noise_amp_buzz` PhysicalRange row | 3 | yes | PhysicalRange.cpp:193 (circuit) | n/a | RangeTests | |
| Options "Default mains region", AUDIO page, seeds Init only | 3 | yes | OptionsPages.cpp:586 (AudioPage); PresetManager.cpp:1277/1319 | Options AUDIO | - | untested; "new instance keeps 60 Hz" is new text |
| NoiseFloor class API (beginBlock, taps, onNoteOn, pushAmpOutput) | 4 | yes | DSP/Noise/NoiseFloor.h | n/a | NoiseFloorTests | |
| getSingleCoilShare factored out | 4 | yes | PickupEngine.h | n/a | - | |
| Acoustic path: hiss and cable only | 4 | yes | NoiseFloor.cpp:62 | n/a | - | untested |
| isIdle skips module | 0.5 | yes | NoiseFloor::isIdle | n/a | idleIsFree (<0.02) | NF-15 amended to 0.02 |
| Seeded, reset reseeds | 0.3 | yes | NoiseFloor | n/a | rendersAreDeterministic | |
| NOISE FLOOR group after aged electronics | 5 | yes | UI/RealismGroupsC NoiseFloorGroup | CHARACTER | RealismUi.everyRealismCParameterHasAControlOnTheCharacterTab | |
| Noise meter (10 Hz, greys after 2 s) | 5 | yes | NoiseMeter (RealismGroupsC) | NOISE FLOOR | RealismUi.theNoiseMeterReadsAndGoesStale | |
| Section 19 row | 5 | yes | gui-integration.md:691 | - | - | new since last audit |
| Older presets load at 0/Off | 6 | yes | Parameters defaults | n/a | defaultsAreBitIdentical | |
| Budget 0.15 units, performance-budget.md row | 7 | yes | performance-budget.md:54 | n/a | idleIsFree | row added |
| No allocation incl. region/style change | 7 | yes | - | n/a | noAllocationOnTheAudioPath | |
| NF-17 sample-rate independence | 8 | yes | - | - | NF-03/06/07 loops | |
| Style table values | 3 | yes | RealismStyles.h | dropdown | styleOffIsInert | |
| DC blocker and NaN guard on recursions | 7 | yes | NoiseFloor | n/a | microphonicsIsBounded | |
| Aux 8 stem includes hum x g_pos | 4.6 | yes | LuthierEngine | n/a | aux8IsOptIn | |
| Passive hiss white noise rescaled to unit variance, 4kTR over 0..sr/2 | 2.3 | yes | NoiseFloor | n/a | passiveHissIsPhysical | new text |
| Shared accumulator not used for legacy hum phase-lock | 2.1 | yes | NoiseFloor | n/a | - | new text, same as the phase row |

### sustain-and-decay.md

36 yes, 1 partial, 1 no.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Legacy single-T60 model kept as slow stage | 1 | yes | StringEngine::updateLoopCoefficients (StringEngine.cpp:824) | - | legacyIsBitIdentical | |
| `sustain_scale` scales the slow stage | 1 | **no** | never bridged: `ParamIDs::sustainScale` is read only by UI (AdvancedPanel.cpp:526, RealismGroupsC.cpp:712/734); strings get setSustainScale(noteSustainScale) only | **Advanced "Sustain" knob is attached but dead** | - | the spec's new "As built" paragraph admits this and defers it to factory-content re-voicing. The decay sketch draws a T60 the audio does not have |
| Control tick every 32 samples | 0.2 | yes | StringEngine.h:408 kShapeTick | n/a | - | |
| Neutral values skip code (bit-identical) | 0.3 | yes | StringEngine | n/a | legacyIsBitIdentical | |
| Brightness overshoot b(t) | 2.1 | yes | StringEngine | n/a | brightnessOvershoot | SUS-06 now measures 2-8 kHz energy (amended) |
| Hammer/pull restarts at 0.5 strength | 2.1 | yes | StringEngine.cpp:338 `exciteStrength = legato ? 0.5*v : v` | n/a | - | untested |
| Longitudinal ping (two-pole, tau 15 ms at f_L, 2f_L -6 dB) | 2.2 | yes | StringEngine resonator | n/a | longitudinalPing | amended from "Q 30" to tau 15 ms |
| coreDiameterMm / tensionNewtons from StringSpec | 2.2,7 | yes | StringEngine.h | n/a | longitudinalPing | |
| Two-stage decay m(t) | 3 | yes | StringEngine | n/a | twoStageKnee, kneeDepth | SUS-02 amended to 1.5x |
| E-Bow and feedback reset the clock | 3 | yes | LuthierEngine.cpp:2533 (feedback), :2579 (E-Bow) restartShapeClock | n/a | - | untested |
| Tension-modulation pitch, clamp +25/+50 c | 4 | yes | StringEngine | n/a | tensionMagnitude, tensionScalesWithLevelSquared, boundedAtTheLimits | SUS-05 amended to [4, 6.5] |
| getCurrentFrequency reports modulated pitch | 4 | yes | StringEngine.cpp:162 | n/a | - | |
| Release damping ramp over T_r | 5.1 | yes | StringEngine.cpp:439 | n/a | releaseRamp | |
| Release sag (fretted only) | 5.2 | yes | StringEngine::release | n/a | releaseSag | SUS-09 amended (compared with d=0, 10-30 ms) |
| Release ring to open string | 5.3 | yes | StringEngine | n/a | releaseRing | SUS-10 amended |
| letRing / E-Bow skip release | 5 | yes | StringEngine | n/a | lettingRingSkipsTheRelease | |
| Range family `strings` | 6 | yes | PhysicalRange.cpp:19, :204-213 | n/a | RangeTests | spec now says `strings` (amended) |
| `sustain_attack_transient` | 6 | yes | Parameters | CHARACTER SUSTAIN SHAPE | brightnessOvershoot | |
| `sustain_attack_time` | 6 | yes | Parameters | SUSTAIN SHAPE | brightnessOvershoot | |
| `sustain_fast_share` | 6 | yes | Parameters | SUSTAIN SHAPE + DECAY row | twoStageKnee | |
| `sustain_fast_ratio` | 6 | yes | Parameters | SUSTAIN SHAPE + DECAY row | twoStageKnee | |
| `sustain_tension_mod` | 6 | yes | Parameters | SUSTAIN SHAPE | tensionMagnitude | |
| `sustain_release_time` | 6 | yes | Parameters | SUSTAIN SHAPE | releaseRamp | |
| `sustain_release_sag` | 6 | yes | Parameters | SUSTAIN SHAPE | releaseSag | |
| `sustain_release_ring` | 6 | yes | Parameters | SUSTAIN SHAPE | releaseRing | |
| `sustain_style` + "(modified)", one undo entry | 6.1 | yes | RealismStyleActions.cpp:81-95 | DECAY row + SUSTAIN SHAPE | styles, RealismUi.theStyleBoxesApplyAndReadModified | |
| `sustain_scale` joins family (advanced 0.05-4) | 6 | yes | PhysicalRange.cpp:205 | Advanced Sustain knob | RangeTests | the range works; the audio does not (row 2) |
| Ship default Legacy | 6.1 | yes | Parameters.cpp:883 default 0 | - | legacyIsBitIdentical | no factory preset uses a style |
| setSustainShape at block rate | 7 | yes | StringEngine.h | n/a | - | |
| release(letRing, fret, sagMm) signature | 7 | partial | StringEngine::release (bool letRing, double fret) (StringEngine.cpp:219) | n/a | releaseSag | sag taken from the shape, not passed; spec 7 still gives the three-argument form |
| DECAY row with decay sketch on STRINGS column | 8 | yes | DecayRow / DecaySketch (RealismGroupsC, AdvancedPanel) | Advanced Col 1 | RealismUi.sustainShapeReadoutAndDecaySketch | |
| Per-string tension pitch readout (30 Hz, greys) | 8 | yes | PitchOffsetReadout | SUSTAIN SHAPE | RealismUi.sustainShapeReadoutAndDecaySketch | |
| Section 19 row | 8 | yes | gui-integration.md:692 | - | - | new since last audit |
| Runtime state reset | 9 | yes | StringEngine::reset | n/a | - | |
| Budget +0.3 units, performance-budget.md row | 10 | yes | performance-budget.md:55 | n/a | costAndSafety | row added; SUS-14 reworded |
| SUS-11 bounded at limits | 11 | yes | - | - | boundedAtTheLimits | |
| SUS-15 sample-rate independence | 11 | yes | - | - | sampleRateIndependence | |
| Test thresholds per spec | 11 | yes | - | - | SustainDecayTests | spec amended to the built thresholds |

### tuning-stability.md

42 yes, 0 partial, 1 no.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Existing drift/LFO/retune kept | 1 | yes | TuningEngine, CharacterEngine::retune | CHARACTER | existing | |
| `stabilityCents` in StringTuning sum | 1 | yes | TuningEngine | n/a | offIsInert | |
| Part fields tuners/nut/capo consumed; capo is an accessory (pressure when chosen, gap by kind, defaults 0.7/5 mm) | 1,5 | yes | PartAcoustics + StabilityModel | Workshop | capoBias | capo text is new |
| Derived capo-bias figure under the offset strip | 1,6 | yes | RealismGroupsC.cpp:651 | TUNING STABILITY | RealismUi.theWorkshopInspectorShowsTheDerivedFigures | new text (moved from Workshop) |
| Settling (sigma, W, material factor) | 2.1 | yes | Model/Playing/StabilityModel.cpp | n/a | settling | |
| Nut binding + seeded ping | 2.2 | yes | StabilityModel | n/a | nutBinding, thePingReleasesTheBind | |
| Tuner backlash (armed on downward approach) | 2.3 | yes | StabilityModel | n/a | backlashNeedsADownwardApproach | |
| Floating equilibrium (+ unscaled bookkeeping) | 2.4.1 | yes | StabilityModel | n/a | floatingEquilibrium | bookkeeping text is new |
| Creep tau 90 s | 2.4.2 | yes | StabilityModel | n/a | creepTimeConstant | |
| Whammy return error | 2.4.3 | yes | StabilityModel (in.whammyCents) | n/a | - | no dedicated test |
| Bend memory | 2.5 | yes | StabilityModel | n/a | bendMemory | |
| Capo bias + capoComp | 2.6 | yes | StabilityModel | n/a | capoBias | |
| Clamp ±50 / ±200 c; caps x4 when advanced | 2, 8 | yes | StabilityModel.cpp:378-379 capScale | n/a | costAndSafety | x4 text is new |
| Retune string n | 3 | yes | requestRetune(mask) (RealismGroupsC.cpp:560, :816) | offset strip click + Easy badge | retuneScope, RealismUi.theOffsetStripRetunes... | |
| Retune all (low to high, feeds equilibrium) | 3 | yes | CharacterPanel.cpp:467 requestRetuneAll | CHARACTER "Retune all" | floatingEquilibrium, RealismUi.theRetuneAllButtonClearsEveryOffset | |
| Glide 250 ms ringing / snap silent | 3 | yes | StabilityModel | n/a | smoothGlides | |
| Auto-retune Off/Idle/Stop/Idle+Stop; Idle only with amount>0 and an offset | 3 | yes | StabilityModel.cpp:517-528 (anyOffset) | dropdown | autoRetuneOnIdle, autoRetuneOnTransportStop | idle guard is new text |
| Atomic command channel | 3 | yes | StabilityModel.h | n/a | - | |
| Retune not undoable | 3 | yes | - | - | - | |
| Retune as MIDI Learn action target (CC>=64 rising edge) | 3 | **no** | nothing in Support/MidiLearn.h or MidiInterpreter calls requestRetune/All | **no** | - | not on techniques either |
| `stability_amount` | 4 | yes | Parameters.cpp:885 | TUNING STABILITY | offIsInert | |
| `stability_settling` | 4 | yes | Parameters | TUNING STABILITY | settling | |
| `stability_nut_binding` | 4 | yes | Parameters | TUNING STABILITY | nutBinding | |
| `stability_backlash` | 4 | yes | Parameters | TUNING STABILITY | backlash... | |
| `stability_saddle_creep` | 4 | yes | Parameters | TUNING STABILITY | floatingEquilibrium, creep... | |
| `stability_bend_memory` | 4 | yes | Parameters | TUNING STABILITY | bendMemory | |
| `stability_capo_bias` | 4 | yes | Parameters | TUNING STABILITY (+capo figure) | capoBias | |
| `stability_auto_retune` | 4 | yes | Parameters | TUNING STABILITY | autoRetuneOn* | |
| Family `strings` | 4 | yes | PhysicalRange.cpp | n/a | RangeTests | amended from `string` |
| onTuningChanged from preset/12-string/detune/fine-tune | 5 | yes | GuitarBodyComponent detune hook etc. | n/a | backlash... | |
| Amount 0: advance returns at once | 5 | yes | StabilityModel | n/a | offIsInert | |
| sigma commits only with transport stopped | 5 | yes | StabilityModel | n/a | resetAndDeterminism | |
| TUNING STABILITY group replaces TUNERS | 6 | yes | CharacterPanel tuningStabilityGroup | CHARACTER | RealismUi.everyRealismCParameter... | |
| Per-string offset strip (colour + dot glyph, 10 Hz, greys) | 6 | yes | OffsetStrip (RealismGroupsC) | TUNING STABILITY | RealismUi.theOffsetStripRetunes... | |
| Easy headstock popover "+3 c" + Retune | 6 | yes | StabilityBadge (GuitarBodyComponent) | Easy | RealismUi.theHeadstockPopoverShowsOffsetsAndRetunes | |
| Workshop inspector figures (tuners, nut) | 6 | yes | WorkshopPanel | Workshop | RealismUi.theWorkshopInspectorShowsTheDerivedFigures | |
| Section 19 row | 6 | yes | gui-integration.md:693 | - | - | new since last audit |
| Session extras `"stability"` with 17-digit text | 7 | yes | PluginProcessor | n/a | serialization | 17-digit text is new |
| Preset load recomputes sigma, clears capoComp | 7 | yes | PluginProcessor | n/a | serialization | |
| Budget 0.02 units, performance-budget row | 8 | yes | performance-budget.md:56 | n/a | costAndSafety | row added |
| Ramps limit per-block step to 0.5 c | 8 | yes | StabilityModel | n/a | smoothGlides | |
| TS-14 sample-rate independence | 9 | yes | - | - | loops in creep/autoRetune tests | no dedicated TS-14 test |
| Deterministic ping draw | 2.2 | yes | StabilityModel | n/a | thePingReleasesTheBind, resetAndDeterminism | |

### strum-dynamics.md

24 yes, 4 partial, 1 no. The spec has not changed and neither has the code (re-verified).

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Crossing velocity in sps | 1 | yes | StrumGesture::effectiveCrossingSps | RHYTHM->STRUM; Advanced | crossingTimingIsExact | |
| Source priority MPE > live spread > pattern > global | 1.1 | yes | RhythmEngine::getCrossingSource, MidiInterpreter | STRUM source label | crossingSourcesResolveInOrder, liveSpreadWins, mpePassesThrough | |
| Acceleration ease profile | 2 | yes | StrumGesture::ease | STRUM | accelerationChangesSpacingNotDuration | |
| Down from low / up from high | 2.1 | yes | StrumGesture::plan | n/a | upIsFasterThanDownByTheRatio | |
| Up faster by up_velocity_ratio | 2.1 | yes | effectiveCrossingSps | STRUM | same | |
| Down steeper/brighter; up softer with more chirp, less click | 2.1 | partial | StrumGesture.cpp:281, kUpStrokeForce 0.85 (StrumGesture.h:227) | n/a | upStrokesAreSofter | force only; no tone or noise difference |
| Tilt ramp ±, mirrored for up | 3 | yes | tiltFactor | STRUM | tiltAndAccentShapeTheForce | |
| Evenness ±40 %, deterministic | 3 | yes | evennessFactor | STRUM (unattached slider) | evennessBoundsTheVariation | |
| Accent +1.5 dB on the first two strings in the stroke direction | 3 | yes | StrumGesture | n/a | tiltAndAccentShapeTheForce | |
| Misses, 3x on leading string, seeded | 3.1 | yes | StrumGesture::plan | STRUM | missesAreWeightedAndDeterministic | |
| RhythmHumanise::missPercent retained | 3.1 | yes | RhythmEngine | RHYTHM | - | |
| Guitar/bass default columns | 4 | yes | StrumSettings defaults | n/a | bassDefaultsApply | |
| Striker set (6) with crossing factors | 5 | yes | getStrikerCrossingFactor | STRUM + PICK mirror | strikersCrossAtTheirOwnSpeed | |
| Striker selects noise generator | 5 | yes | getStrikerMaterial | n/a | strikerSelectsTheNoise | |
| Separate down/up strikers | 5 | yes | strum_striker_down/up | STRUM + PICK | - | |
| Striker applies to live keyboard strums | 5 | partial | MidiInterpreter.cpp:604 forces Striker::pick | n/a | - | by design; the spec is ambiguous |
| Chuck: damp all strings | 6.1 | yes | Damping::Chuck | STRUM | chuckKillsPitch | |
| chuck_amount blend | 6.1 | yes | StrumGesture | STRUM | chuckStepsCarryTheChuck | |
| Chuck from pattern step type | 6.1 | yes | RhythmEngine | RHYTHM pattern editor | chuckStepsCarryTheChuck | |
| Chuck from MIDI note in chuck key range | 6.1 | **no** | - | no | - | not on techniques either |
| Palm mute vs chuck distinct | 6.2 | yes | TechniqueEngine / Chuck | n/a | - | |
| STRUM group on RHYTHM tab | 6.3 | yes | UI/StrumGroup | RHYTHM | theStrumGroupDrivesTheModel | |
| Easy Feel knob maps 60->400 sps, 0.45->0.95 | 6.3 | yes | StrumFeel, EasyPanel | Easy rhythm strip | feelMapsAsSpecified | |
| `strum_crossing_sps` | 7 | yes | Parameters | STRUM, Advanced | parametersReachTheEngine | |
| `strum_acceleration` / `up_velocity_ratio` / `tilt` / `miss_probability` | 7 | yes | Parameters | STRUM | parametersReachTheEngine | |
| `strum_striker_down` / `_up` | 7 | yes | Parameters | STRUM + PICK | same | |
| `chuck_amount` / `chuck_damping` | 7 | yes | Parameters | STRUM | same | |
| `strum_evenness` as a parameter | 7 | partial | RhythmEngine state (Parameters.h:311, StrumGroup.cpp:47) | slider not attached | olderPresetsKeepTheirStrumSpeed | cannot be automated or modulated |
| Old strum_speed superseded but kept | 7 | yes | Parameters | hidden | olderPresetsKeepTheirStrumSpeed | |

### Top gaps (group F)

1. **The Advanced "Sustain" knob (`sustain_scale`) does nothing.** It has an APVTS attachment, a range and a place in the `strings` family, and the DECAY sketch draws the T60 it implies. The parameter bridge never passes it to `StringEngine::setSustainScale`, which receives only `noteSustainScale`. This was missed in the previous audit. The spec's amendment admits it and defers it to factory re-voicing, but a visible dead knob is a user-facing defect.
2. **Every realism feature except string interaction still ships off.** `sustain_style` Legacy, noise 0 and `stability_amount` 0 are the defaults, and no factory preset sets any of them (`Presets/*.cpp`). A buyer hears none of these three specs without opening CHARACTER.
3. **Retune all is not a MIDI Learn action target** (tuning-stability 3). Nothing outside the UI calls `requestRetune` or `requestRetuneAll`.
4. **Chuck by MIDI note (chuck key range) is not implemented** (strum-dynamics 6.1), on any branch.
5. **Up and down strums differ only in force** (kUpStrokeForce 0.85). There is no brightness, chirp or click difference (strum-dynamics 2.1).
6. **The fretting-hand mute style is still a proxy** (`rh_style==Classical`, Parameters.cpp:1731). `mute_fretting_style` is in progress on techniques. When that branch merges, `interaction.frettingStyle` has to read it, or the two will disagree.
7. **Palm width is not mirrored in TECHNIQUES->MUTE** (string-interaction 9). The techniques MuteGroup has only palm position and pressure.
8. **The Advanced knob is still labelled "Amp Buzz"** (AdvancedPanel.cpp:815). The parameter itself was renamed "Single-coil Hum".
9. **`strum_evenness` is not an APVTS parameter.** Its STRUM slider is unattached, so it cannot be automated or modulated.
10. **gui-integration.md 19 has no rows for HARMONICS, RIGHT HAND or STRING INTERACTION.** Realism-c's three rows were added.
11. **performance-budget.md has no rows for harmonic contacts (1.0), string interaction (0.2) or fingerstyle (0.02).** Realism-c's three rows were added.
12. **string-interaction.md contradicts itself.** Section 10 says the budget is 0.2 units, but SI-13 in section 11 still says 0.1.
13. **The tool glyph is drawn at a fixed right edge of the fretboard**, not at `pluck_position` (+`thumb_position_offset` for thumb tools), and not on the guitar-body illustration (fingerstyle-attack 7).
14. **The MIDI export does not carry the notation fields.** `touch_fret`/`partial` (HR) and `tool`/`finger`/`stroke` (FA) are missing. The specs now defer them to midi-export (build notes), and the audio round trip is tested, so this is a notation gap only.
15. **HARMONICS is a separate CHARACTER group, not a row inside PICK** (harmonic-realism 7). This is cosmetic.
16. **`StringEngine::release` takes 2 arguments, but sustain-and-decay 7 still specifies 3** (sag is read from the shape). The spec text is stale.
17. **Tool-cell clicks in RIGHT HAND write without a ScopedUndoAction** (RightHandGroup.cpp:58). Whether they are undoable is unverified.
18. **Several behaviours are built but untested:**
    - hammer/pull overshoot restart at 0.5
    - the E-Bow/feedback clock reset
    - the air path through kEnergyCap
    - strings 7-12 tool by course
    - Options default mains region
    - whammy return error

### Unspecified gaps noticed

- **String-interaction defaults change factory renders**, by up to a couple of dB (spec build note 4, measured in docs/coverage/REALISM-B.md). Pattern fingers now override Global strings (FA build note 5), which re-voices fingerpick presets. Nothing in factory-content.md or the preset QA suite records these as intended re-voicings.
- **No one-click realism tier.** There is no Easy-mode macro and no "Legacy vs Realistic" A/B toggle for the sustain, noise-floor and stability groups. The CHARACTER tab now carries HARMONICS, RIGHT HAND, STRING INTERACTION, TUNING STABILITY, NOISE FLOOR and SUSTAIN SHAPE, plus realism-a's groups, in one scroll with no collapse or sub-tabs.
- **Harmonics still have no on-screen trigger.** The techniques branch's pill row has no harmonic pill either. Natural, pinch, artificial and tapped harmonics need CC 72/73/103/104 or velocity.
- **The decay sketch reads `sustain_scale`**, so it shows a T60 the engine does not produce (see Top gap 1).
- **NF-02's out-of-window hum is recorded in docs/coverage/REALISM-C.md, not DECISIONS.md.** The spec was changed to allow that, because "helpers do not edit DECISIONS.md", but DECISIONS.md is now missing a spec-mandated record.

### Small glue candidates

- **`sustain_scale`:** in Parameters.cpp's bridge, apply `value (ParamIDs::sustainScale)` as a multiplier on the `noteSustainScale` that `LuthierEngine.cpp:1351/2049` and `LuthierEngineRealismB.cpp:687` pass. Then re-voice or clamp the one factory preset that sets 1.5.
- **"Amp Buzz" label:** change `AdvancedPanel.cpp:815 addKnob (ampBuzz, "Amp Buzz", ...)` to "Single-coil Hum".
- **Retune all:** add a MIDI Learn action (rising edge at CC >= 64) that calls `getStabilityModel().requestRetuneAll()`, next to the existing learn targets in Support/MidiLearn.
- **`strum_evenness`:** promote it to an APVTS float (0-1, default 0.75), mirror it into `RhythmEngine::setStrumEvenness`, and attach the existing StrumGroup slider.
- **Fretting style, after techniques merges:** in Parameters.cpp:1731, read `ParamIDs::muteFrettingStyle` (rock 1.0 / classical 0.1) instead of `rh_style`. Update the StringInteractionGroup.cpp:54 text.
- **Palm width:** add a `palm_mute_spread` mirror slider to techniques' `UI/MuteGroup.cpp` once techniques merges.
- **Tool glyph position:** in `FretboardRealismB.cpp paintRealismB`, place the glyph at `pluck_position` (+`thumb_position_offset` for thumb-class tools) instead of `boardArea.getRight() - 18`.
- **Chuck key range:** a fixed keyswitch, or a range parameter, that sets `NoteOnEvent::chuck` using the existing `chuck_damping` path.
- **Docs only:**
  - add HARMONICS, RIGHT HAND and STRING INTERACTION rows to gui-integration.md 19
  - add contact (1.0), interaction (0.2) and fingerstyle (0.02) rows to performance-budget.md
  - fix SI-13's 0.1 to 0.2
  - fix sustain-and-decay 7's `release` signature

---

## Group G: techniques, QA, performance, installer

Audited: working tree at HEAD `961cd55` (`claude/luthier-audit`). The integration branch and these helper branches are now **ancestors of HEAD**, checked with `git merge-base --is-ancestor`: realism-a, realism-b, realism-c, model-gaps, tune-help, visual, release, review. Only `origin/claude/luthier-techniques` (tip `1b8bf87`) is **still unmerged**. HEAD has 177 commits the branch lacks, and the branch has 12 that HEAD lacks. A dry-run `git merge-tree` of the two shows **21 conflicting files**, including StringEngine, LuthierEngine, Parameters.cpp/.h, PresetManager, FactoryPresets, CharacterPanel, AdvancedPanel, EasyPanel, FretboardComponent and Overlays. Legend: "in progress on techniques" means the item exists only on that branch. Test names are `Suite.test`.

Spec changes since the last audit (group G files): only `performance-budget.md` changed. It adds three budget rows (NoiseFloor 0.15, StringEngine sustain shape +0.3, StabilityModel 0.02), and those rows are added to the table below. All 12 group-G spec files exist, and the previous report covered all of them.

Cross-cutting facts on HEAD:
- **27 technique params still have no control anywhere.** These are `scrape_*` ×14 and the slap-technique set ×13: `slap_armed/type/trigger/velocity_zone/trigger_cc/ghost_cc/force/palm_position_mm/string_mask/ghost_mode/rebound_gap/snap_back/body_part`. Two related items were resolved by the merges:
  - model-gaps landed `UI/SlapGroup`, which covers the 12 bass slap/pop/ghost/double-thump params plus 2 fingerstyle ones. It shows **only when a bass is loaded** (CharacterPanel.cpp:353).
  - realism-b landed the Easy "Slap" right-hand style and the per-string S/Po tool cells. These fire the SlapEngine **without** `slap_armed`.
- B-11 is still OPEN: `GuiReach.everyAutomatableParameterHasAVisibleControl` still fails on the scrape_*/slap_* params above, plus `macro_assign_a/b` and `pickup_blend`.
- `Source/UI/HelpContent.cpp:316-548` now ships help topics for a TECHNIQUES tab (technique-slap/scrape/muting/tapping/bends/cascade), but that tab **does not exist on HEAD**. Search and help lead users to UI that is absent.

---

### string-scraping.md

No change on HEAD since the last audit. The engine is complete and tested but still **not playable from the GUI**: no `scrape_*` control and no fire button. The only scrape control on screen is `pick_scrape_amount` (CHARACTER > NoiseGroups). The SCRAPE page, "Scrape now" button and Easy pill are in progress on techniques. The five section-4 presets still exist only as `ScrapeSettings::fromPreset`, which only tests call. They are also missing on techniques, whose `TechniquePresets.cpp` has no scrape-only presets. Counts: yes 19 / partial 4 / no 1. (The previous report said 17/6/1. The difference is a recount of the same rows, not a code change.)

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Per-winding impulses, catches = distance × windings_per_mm | 0.1, 1 | yes | DSP/Noise/ScrapeEngine: `windingsPerMm`, `processBlock` | n/a | Scrape.catchesComeAtWindingsPerMmTimesSpeed | |
| Plain strings near-silent (emergent) | 0.2 | yes | ScrapeEngine (zero windings) | n/a | Scrape.aPlainStringIsNearSilent; ScrapeEngineWiring.thePlainHighEIsThirtyDecibelsUnderTheWoundLowE | |
| Fast vs slow scrape character | 0.3 | yes | ScrapeEngine | n/a | Scrape.theFactoryScrapesAreWhatSectionFourSays | |
| Catch amplitude ∝ pressure; deeper catch adds pitch load | 1 | yes | ScrapeEngine | n/a | Scrape.doublingPressureDoublesEachCatch; ScrapeEngineWiring.harderScrapingLoadsThePitchSlightly | |
| ScrapeGesture struct | 1 | yes | ScrapeEngine.h `ScrapeSettings` | n/a | | |
| Trigger: keyswitch / CC / MPE zone / on-screen button | 0.4, 2 | partial | `scrape_trigger`, `scrape_trigger_cc`=85, KS 12 (TechniqueTriggers.h:43); `ScrapeEngine::requestTrigger` | no (in progress on techniques: "Scrape now" + Easy pill) | Scrape.triggersListenOnlyWhereTheyAreTold; ScrapeEngineWiring.theKeyswitchScrapesAndNeverPlaysANote | "Button Only" mode still has no button |
| Direction B→N / N→B / Hold+Sweep | 2 | yes | `scrape_direction` | no (in progress on techniques) | Scrape.aReversedScrapeIsTheForwardOneMirrored | |
| Sweep source auto/MW/expr/AT/CC | 2 | yes | `scrape_sweep_source`, `scrape_sweep_cc` | no (in progress on techniques) | Scrape.aModwheelSweepCatchesWithinOneBlock | |
| Sweep range 200-900 mm | 2 | yes | `scrape_start_mm`, `scrape_end_mm` | no (in progress on techniques) | | |
| Duration | 4 | yes | `scrape_duration` | no (in progress on techniques) | | |
| Pressure 0.5 | 2 | yes | `scrape_pressure` | no (in progress on techniques) | | |
| Tool pick/nail/thumb | 2 | yes | `scrape_tool` | no (in progress on techniques) | | |
| Angle 20° | 2 | yes | `scrape_angle` | no (in progress on techniques) | | |
| String mask, default wound only | 2 | partial | `scrape_string_mask` (Parameters.cpp:753) default 0 = follow held strings | no (in progress on techniques) | | default still differs from spec (also on techniques) |
| Retrigger 200 ms, silent drop | 2, 6 | yes | `scrape_retrigger` | no (in progress on techniques) | Scrape.aRetriggerInsideTheThresholdIsDropped | |
| Scrape level | C-29 | yes | `pick_scrape_amount` | yes: CHARACTER > NoiseGroups "Scrape" | Scrape.theLevelTrimScalesAndZeroIsFree | |
| trigger/processBlock/reset, zero idle cost | 3 | yes | ScrapeEngine | n/a | Scrape.idleCostsNothing; Scrape.resetRepeatsExactly | |
| Pipeline after TechniqueEngine, before StringEngine | 3 | yes | LuthierEngine.cpp (scrape block before strings) | n/a | | |
| Presets ×5 | 4 | partial | `ScrapeSettings::fromPreset` (only tests call it) | no | Scrape.theFactoryScrapesAreWhatSectionFourSays | not in FactoryPresets on HEAD or on techniques |
| Cascade: mute dulls the scrape | 5 | yes | `ScrapeEngine::setMuteAmount` | n/a | Scrape.aMuteMakesItDullerAndThumpier | fed only by the palm-mute articulation until MuteEngine merges |
| Cascade: slide takes string; bend shifts spacing | 5 | yes | `setStringBlocked`, bend stretch | n/a | | no bend-during-scrape test |
| Cascade: tap damps scrape | 5 | partial | preempt hooks; TapEngine in progress on techniques | n/a | techniques: Cascade.aTapPreemptsAScrapeOnItsString | |
| CPU idle < 0.05%, active < 0.5% | 6 | yes | | n/a | Scrape.idleCostsNothing; Scrape.anActiveScrapeStaysInBudget | |
| GUI: Techniques tab | 2 | no | none on HEAD (help topic `technique-scrape` exists, UI does not) | in progress on techniques (TECHNIQUES > SCRAPE `ScrapePage`) | techniques: TechniquesUi.everySubTabRendersItsControls | B-11 |

### slide-technique-controls.md

No change on HEAD. Only the base `SlideEngine` and `SlideGroup` exist, and every control in this spec is in progress on techniques: `SlideControlSettings`, `SlideEngine::advanceControls/triggerGesture`, `slide_pos_*`/`slide_gesture_*` params, `SlidePage`, pill popover, 4 presets and `SlideTechniqueTests.cpp` (8 tests). The two deviations remain on the branch: the Easy "…" is a pill hold popover, and the Advanced mirror sits at the CHARACTER foot, not in the Col 3 SLIDE group. Counts on HEAD: yes 1 / partial 0 / no 17.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Base slide physics and Slide Mode unchanged | 0.1, 4 | yes | DSP/Slide/SlideEngine; UI/SlideGroup | Advanced SLIDE group | Slide.*; Slide.theBarArrivesWithinTwoCents | |
| Position source MW/PB/MPE Y/expr/CC/drag | 1 | no | in progress on techniques: `slide_pos_source`, `slide_pos_cc`, TechniqueOverlay drag | in progress on techniques | techniques: SlideControls.theModWheelDrivesThePosition | |
| Position mode absolute/relative | 1 | no | in progress on techniques: `slide_pos_mode`, `slide_pos_range` | in progress on techniques | same | |
| Slant control source | 1 | no | in progress on techniques: `slide_slant_source/_cc` | in progress on techniques | SlideControls.everyControlRoundTrips | |
| Pressure control source | 1 | no | in progress on techniques: `slide_pressure_source/_cc` | in progress on techniques | same | |
| Contact string mask | 1 | no | in progress on techniques: `slide_contact` | in progress on techniques | SlideControls.aBassOnlyBarLeavesTheTrebleFree | |
| Speed limit 4800 c/s | 1 | no | in progress on techniques: `slide_speed_limit` | in progress on techniques | SlideControls.theSpeedLimitClampsAJump | |
| Auto-vibrato after 300 ms hold | 1 | no | in progress on techniques: `slide_auto_vibrato*` | in progress on techniques | SlideControls.autoVibratoEngagesAfterAHold | |
| Gesture trigger KS/CC | 1 | no | in progress on techniques: KS 21, `slide_gesture_trigger/_cc` | in progress on techniques | SlideControls.aScriptedGestureArrivesOnTime | |
| SlideGesture struct | 2 | no | in progress on techniques: `SlideGesture` | in progress on techniques | same | tune-builder / MIDI meta trigger not seen |
| setPositionSource / triggerGesture / setSpeedLimit | 3 | no | in progress on techniques | n/a | SlideControls.* | |
| PB as slide position | 0.4 | no | in progress on techniques | in progress on techniques | | |
| GUI Slide sub-tab | 4 | no | in progress on techniques: `SlidePage` | in progress on techniques | TechniquesUi.everySubTabRendersItsControls | |
| Easy slide "…" popover | 4 | no | in progress on techniques: SLIDE pill popover | in progress on techniques | TechniquesUi.holdOpensThePopoverAndEscapeClosesIt | deviation |
| Advanced Col 3 SLIDE expandable section | 4 | no | in progress on techniques: `TechniqueMirrors` (CHARACTER foot) | in progress on techniques | TechniquesUi.theCharacterMirrorsAttachTheSameParameters | wrong location |
| Presets ×4 | 6 | no | in progress on techniques: `TechniquePresets.cpp` | in progress on techniques | TechniquesUi.thePresetChipFilters | |
| Source swap crossfade 10 ms | 7 | no | in progress on techniques | n/a | SlideControls.swappingTheSourceGlides | |
| Preset round-trip | 7 | no | in progress on techniques | n/a | SlideControls.everyControlRoundTrips | |

### string-slap-technique.md

Improved by the merges.
- **Body Tap now also drives the body mode bank.** `LuthierEngine.cpp:1619` calls `bodyCoupling.driveDirect(a.force, bodyPart)` (BodyCouplingBank.cpp:349) alongside SlapEngine's own knock.
- **The Playing-strip Tool selector gained Slap/Pop (realism-b).** Two controls: the Easy `RightHandToolSelector` "Slap" style and the CHARACTER RIGHT HAND per-string S/Po cells. Both reach SlapEngine through `makeToolStrike` (LuthierEngineRealismB.cpp:501-551, LuthierEngine.cpp:1175).
- **Bass slap basics now have controls.** `SlapGroup` covers them, bass only.

Still unchanged:
- None of the 13 generic slap-technique params has a control.
- There is no "Slap now" button.
- The presets are test-only.
- `double_thump_enabled` still defaults to false (Parameters.cpp:776).

Counts: yes 18 / partial 6 / no 1.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Strike + slap-buzz event | 0.2 | yes | SlapEngine → FretBuzz | n/a | Slap.theClackIsTheFretBuzzGenerator; SlapWiring.theClackComesFromTheBuzzGenerator | |
| Any guitar, not bass-only | 0.4 | yes | SlapEngine; RhTool slap/pop on any guitar | Easy "Slap" right-hand style (any guitar) | SlapWiring.thePlainHighEIsAudibleButClacksLess | |
| Slap type Thumb/Pop/Palm/Body Tap | 1 | yes | `slap_type` | partial: thumb/pop via RhTool; palm/body tap only by KS 17/18 | Slap.whatANoteBecomes | `slap_type` itself has no control |
| Trigger velocity zone/KS/CC/MPE/strip button | 1 | partial | `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`; KS 15-18 | no fire button; the RhTool style is a tool, not a trigger (in progress on techniques: "Slap now" + pill) | TechniqueTriggers.*; Slap.theButtonAndTheKeyswitchesQueueTheirStrikes | |
| Contact positions 60/40/100 mm | 1 | yes | `slap_position_mm`, `pop_position_mm`, `slap_palm_position_mm` | partial: thumb/pop in CHARACTER > SLAP (bass only); palm none | Slap.theContactPointIsMeasuredFromTheLastFret | |
| Contact force 0.6 | 1 | yes | `slap_force` | no (in progress on techniques) | | |
| String mask per type | 1 | yes | `slap_string_mask` | no (in progress on techniques) | Slap.theFactorySlapsAreWhatSectionFourSays | |
| Ghost mode + KS/CC | 1 | yes | `slap_ghost_mode`, `slap_ghost_cc`, KS 16; bass `ghost_*` | partial: bass ghost_level/damping/auto/threshold in SLAP group; `slap_ghost_mode` none | SlapWiring.aGhostIsAThumpWithNoPitch | |
| Rebound on by default for thumb, gap 60 ms | 1 | partial | `double_thump_enabled` (**default false**), `slap_rebound_gap` | toggle in CHARACTER > SLAP (bass only); gap none | SlapWiring.theDoubleThumpComesBackAtItsGap | default contradicts spec (unchanged on techniques) |
| Snap-back | 1 | yes | `slap_snap_back` | no (in progress on techniques) | | |
| Body tap via body-coupling mode bank | 1, 2 | yes | LuthierEngine.cpp:1614-1619 `startBodyTap` + `BodyCouplingBank::driveDirect` | no (`slap_body_part` has no control; KS 17 only) | Slap.theBodyPartWeightsTheKnock; BodyCoupling.BC06_BC09_aTapRingsTheStringsNearAMode | **was partial**; no test ties body tap to the bank response |
| SlapEngine API; after TechniqueEngine | 2 | yes | DSP/Slap/SlapEngine | n/a | Slap.idleAndActiveStayInBudget | |
| Body tap bypasses StringEngine | 2 | yes | | n/a | SlapWiring.aBodyTapLeavesTheStringsAlone | |
| Shares engine with bass-techniques | 3 | yes | same params; `UI/SlapGroup` | CHARACTER > SLAP (bass only) | BassTechniques.theSlapGroupIsShownOnlyOnABass; BassTechniques.aPopIsBrighterAndShorterThanASlap | **GUI was branch-only** |
| Presets ×5 | 4 | partial | `SlapSettings::fromPreset` (only tests call it) | no | Slap.theFactorySlapsAreWhatSectionFourSays | only "Rockabilly Slap"-style factory presets exist; section-4 set not loadable |
| Cascade mute-then-slap, bend after slap | 5 | partial | MuteEngine/BendEngine in progress on techniques | n/a | techniques: Muting.aSlappedNoteCarriesItsMute; Bend.aBentSlapPitchesCorrectly | |
| Cascade slide / scrape conflict | 5 | yes | `slap.classify(e, slide.isUnderBar)`, preempt | n/a | SlapWiring.slapAndScrapeTakeTheStringFromEachOther | |
| Cascade tap alternate | 5 | partial | in progress on techniques: CascadeResolver | n/a | techniques: Cascade.everyPairResolvesAsDocumented | |
| GUI Techniques > Slap | 6 | no | none on HEAD | in progress on techniques: `SlapPage` | TechniquesUi.everySubTabRendersItsControls | B-11 |
| Playing strip Tool selector gains Slap/Pop | 6 | yes | UI/RightHandGroup `RightHandToolSelector` "Slap" (style 6), `StringToolCell` S/Po; engine `makeToolStrike` | yes: Easy strip + CHARACTER RIGHT HAND | FingerstyleAttack.FA13_slapAndPopTools | **was partial** |
| Thumb slap within 1 dB of bass reference | 7 | partial | | | SlapWiring.aThumbSlapIsTheSameHoweverItIsFired | compares trigger paths, no stored reference |
| Palm slap < -25 dB pitched | 7 | yes | | | SlapWiring.aPalmSlapIsBroadbandAndPitchless | |
| Body tap/ghost/rebound/plain tests | 7 | yes | | | SlapWiring.* | |
| CPU idle < 0.05%, active < 0.6% | 7 | yes | | | Slap.idleAndActiveStayInBudget | |
| Preset round-trip | 7 | yes | | | SlapPresets.everySlapFieldRoundTrips | |

### muting-rhythm.md

Still only WIP on HEAD: `Source/WIP/Rhythm/Muting.*`, `WIP/UI/MuteGroup.*` and `WIP/Tests/MutingTests.cpp`, none of them compiled. MuteEngine, the per-step `mute_type`, the grid, the MUTE page, the Easy button, the RHYTHM Mute row and the presets are in progress on techniques (19 Muting tests). realism-b landed three adjacent pieces:
- `MutedThump` (Rhythm/MutedThump.h): strings the voicing mutes are struck as pitchless thumps.
- `palm_mute_spread` and `adjacent_mute_amount`.
- `thumb_palm_mute`.

These cover part of the "fretting-hand mute style" requirement. Counts on HEAD: yes 0 / partial 4 / no 16.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| 7 mute types with T60/position/pressure | 1 | partial | HEAD: PalmMute/MutedPick articulation, `StringEngine::Damping::PalmMute`; WIP Muting (not built); in progress on techniques `Muting::dampingFor` | no (in progress on techniques: MUTE page) | techniques: Muting.eachTypeDampsAsDescribed | |
| Muted strike = thump + damped ring | 0.2 | partial | HEAD palm-mute articulation, `muted_thump_level`; in progress on techniques: StringEngine Muted | partial: `muted_thump_level` in realism groups | techniques: Muting.palmMuteHeavyOnLowEDecaysIn40To60Ms | |
| `mute_type` per pattern step; JSON; missing = open | 2 | no | in progress on techniques: `RhythmPattern::getMuteStep/setMuteStep` | in progress on techniques: RHYTHM Mute Row | Muting.theMuteGridsRoundTrip; Muting.existingPatternsPlayIdentically | |
| Live 16-step grid synced to tempo | 2 | no | in progress on techniques: `MuteEngine` | in progress on techniques | Muting.paintingTheLiveGridAppliesWithinABar | |
| Master mute mode | 3 | no | in progress on techniques: `mute_master_mode` | in progress on techniques | Muting.theMasterModeOverridesEverything | |
| Palm position 35 mm / pressure 0.5 | 3 | no | in progress on techniques: `mute_palm_position/_pressure` | in progress on techniques | Muting.eachTypeDampsAsDescribed | |
| Fretting-hand mute style rock/classical | 3 | partial | HEAD: `MutedThump`, `adjacent_mute_amount`, `palm_mute_spread` (realism-b); in progress on techniques: `MuteEngine::deadensOtherStrings` | HEAD: realism-b STRING INTERACTION / RIGHT HAND controls | HEAD: StringInteraction tests; techniques: Muting.rockSpreadDeadensTheStringsAMutedStrumMisses | **was no**; no rock/classical selector on HEAD |
| Chuka source (strum dyn < 0.3) | 3 | partial | HEAD `chuck_amount`/`chuck_damping` | HEAD: RHYTHM > STRUM chuck knobs (B-11: laid out at zero height) | techniques: Muting.aSoftStrumIsAChuka | |
| Random humanise | 3 | no | in progress on techniques | in progress on techniques | Muting.humaniseShiftsAboutHalfTheEligibleSteps | |
| Ghost velocity range 0.4 | 3 | no | in progress on techniques | in progress on techniques | Muting.aGhostNoteHasNoPitchedContent | |
| RhythmEngine passes mute with note-on | 4 | no | in progress on techniques: `RhythmEngine::emitNote` | n/a | Muting.aPatternsMuteRowReachesItsNotes | |
| StringEngine initial damping + release | 4 | no | in progress on techniques: `techniqueStrike` | n/a | Muting.aFretMuteRingsThenStops | |
| Cascade: stacks with everything | 5 | no | in progress on techniques | n/a | Muting.aMuteIsStampedOnAnyTechnique | |
| Presets ×6 | 6 | no | in progress on techniques: TechniquePresets.cpp | in progress on techniques | TechniquesUi.thePresetChipFilters | |
| GUI Muting sub-tab | 7 | no | in progress on techniques: `MutePage`/`MuteGroup` | in progress on techniques | TechniquesUi.everySubTabRendersItsControls | help topic `technique-muting` on HEAD |
| Easy Mute 4-way | 7 | no | in progress on techniques: `EasyMuteButton` | in progress on techniques | Muting.theEasyMuteButtonCyclesFourWays | |
| RHYTHM Mute row | 7 | no | in progress on techniques: RhythmPanel `muteRow` | in progress on techniques | Muting.theMuteControlsDriveTheModel | |
| MIDI export of mute types | 8 | no | none (deferred on techniques too) | n/a | none | |
| Test: slap carries mute_type | 9 | no | in progress on techniques | | Muting.aSlappedNoteCarriesItsMute | |
| Test: grid round-trip | 9 | no | in progress on techniques | | Muting.theMuteGridsRoundTrip | |

### two-hand-tapping.md

No change on HEAD. There is no TapEngine and no `tap_*` param. `TechniqueEngine` still hard-codes `legatoVelocity = 0.63` (TechniqueEngine.h:149, about MIDI 80; spec 40), and `legato_window` defaults to 40 ms (Parameters.cpp:589; spec 150). Everything else is in progress on techniques: TapEngine, `tap_hammer_threshold` (default 40), TapPage, pill, mirror, overlay, 5 presets and 10 Tap tests. MIDI export is deferred there. Counts on HEAD: yes 0 / partial 1 / no 18.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| TapGesture struct | 1 | no | in progress on techniques: `TapEngine.h` | n/a | Tap.fretSnapOffTapsBetweenTheFrets | |
| Tap-on / hold / tap-off | 2 | no | in progress on techniques: `TapEngine`, `playTapEvent` | n/a | Tap.aTapIsAMovableCapoAndLiftsBackToTheFrettedNote | |
| Pull-off flick | 2 | no | in progress on techniques | | Tap.thePullOffFlickIsTheReleaseTransient | |
| Multi-finger taps = capos in series | 2 | no | in progress on techniques: `soundingFret` | | Tap.twoTapsOnAStringAreCaposInSeries | |
| Trigger ch 2 / KS / fretboard | 3 | no | in progress on techniques: `tap_source`, `tap_channel`, KS 19 | in progress on techniques | Tap.theTriggersTakeTheirNotes | |
| Strength curve | 3 | no | in progress on techniques | in progress on techniques | Tap.theStrengthCurveShapesVelocity | |
| Auto pull-off | 3 | no | in progress on techniques | in progress on techniques | | |
| LH hammer-on threshold 40 | 3, 5 | partial | HEAD fixed 0.63 (TechniqueEngine.h:149); in progress on techniques: `tap_hammer_threshold` | HEAD no | techniques: Tap.softNotesCloseTogetherAreHammerOns | |
| Flick/duration/max concurrent/fret snap | 3 | no | in progress on techniques | in progress on techniques | Tap.everyControlRoundTrips | |
| TapEngine API | 4 | no | in progress on techniques | | Tap.* | |
| LH legato promotion via TapEngine, 150 ms | 5 | no | HEAD `legato_window` 40 ms | HEAD: Advanced PERFORMANCE Legato Window | Tap.softNotesCloseTogetherAreHammerOns | |
| GUI Tapping sub-tab | 6 | no | in progress on techniques: `TapPage` | in progress on techniques | TechniquesUi.everySubTabRendersItsControls | |
| Easy Tap pill | 6 | no | in progress on techniques | in progress on techniques | TechniquesUi.thePillsArmOnAClick | |
| CHARACTER Right Hand "Tapping" | 6 | no | in progress on techniques: TechniqueMirrors (CHARACTER foot) | in progress on techniques | | RightHandGroup now on HEAD; the mirror should go inside it |
| Fretboard tap markers | 6 | no | in progress on techniques: TechniqueOverlay | in progress on techniques | TechniquesUi.theOverlaysDrawInsideTheirBudget | |
| Cascade | 7 | no | in progress on techniques | | Tap.theSlideBarHoldsItsStrings | |
| Presets ×5 | 8 | no | in progress on techniques | in progress on techniques | | Eight-Finger at 4 per string |
| MIDI export of taps | 9 | no | none | | none | |
| CPU budget | 10 | no | in progress on techniques | | Tap.cpuStaysInBudget | |

### microtonal-bends.md

No change on HEAD. It still has the global `bend_range` (1-48 st, default 2), MPE per-note PB and a fixed `vibrato_rate/depth/shape`. There is no Scala/.tun loading, quantise, per-string range, pre-bend or curve. The only microtonal item is a "Microtonal Just" factory preset (FactoryPresets.cpp:659), which is a tuning, not bends. BendEngine, MicrotonalScale, `bend_*`, BendPage and 11 Bend tests are in progress on techniques. The branch still adds a second vibrato system beside `vibrato_*`, and still defers PreBendEvent and MIDI export. Counts on HEAD: yes 0 / partial 4 / no 16.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Continuous per-string pitch | 0.1 | partial | MPE per-note PB | Advanced PERFORMANCE | | |
| Source stack | 1 | no | in progress on techniques: `BendEngine::centsFor` | | Bend.theGlobalBendMovesEveryString | |
| Global source PB/expr/CC | 2 | partial | HEAD PB only | in progress on techniques | same | |
| Global range cents | 2 | partial | HEAD `bend_range` st (Parameters.cpp:582) | HEAD Advanced PERFORMANCE | Bend.everyControlRoundTrips | |
| Per-string source | 2 | partial | HEAD MPE PB | in progress on techniques | Bend.aPerStringBendMovesOnlyItsString | |
| Per-string ranges ×6 | 2 | no | in progress on techniques | in progress on techniques | same | |
| Vibrato source/rate/depth/onset | 2 | no | in progress on techniques: `bend_vibrato_*` | in progress on techniques | Bend.vibratoWaitsForItsOnsetThenReachesDepthIn50Ms | overlaps `vibrato_*` |
| Quantise + snap | 2-3 | no | in progress on techniques | in progress on techniques | Bend.quarterToneQuantiseLandsOnTheGrid | |
| .scl/.tun loading | 2-3 | no | in progress on techniques: `MicrotonalScale` | in progress on techniques | Bend.aLoadedScaleIsTheGrid | |
| Pre-bend KS/CC | 2 | no | in progress on techniques: KS 20 | in progress on techniques | Bend.aPreBendStartsFlatAndReleases | |
| Bend / release curves | 2 | no | in progress on techniques: BendCurveEditor | in progress on techniques | Bend.theCurvesShapeTheThrow | |
| Range change at next note-on | 4 | no | in progress on techniques | | Bend.aRangeChangeWaitsForTheNextNote | |
| ModMatrix PreBendEvent | 4 | no | none | | none | |
| GUI sub-tab | 5 | no | in progress on techniques: BendPage | in progress on techniques | TechniquesUi.* | help topic `technique-bends` already on HEAD |
| Easy "…" popover | 5 | no | in progress on techniques | in progress on techniques | | |
| CHARACTER PLAYING "Microtonal" | 5 | no | in progress on techniques: mirror | in progress on techniques | | |
| Fretboard cents badge | 5 | no | in progress on techniques: TechniqueOverlay | in progress on techniques | | |
| Cascade compatible | 6 | no | in progress on techniques | | Bend.aBentSlapPitchesCorrectly | |
| Presets ×5 | 7 | no | in progress on techniques | in progress on techniques | | |
| MIDI export | 8 | no | none | | none | |

### technique-cascade.md

No change on HEAD. Cascade is still ad-hoc and pairwise (scrape/slap/slide `preempt`, `setStringBlocked`, `classify(isUnderBar)`), and there is still no CascadeResolver. The branch has 13 Cascade tests. Counts on HEAD: yes 1 / partial 3 / no 7.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Class table + matrix | 1-2 | partial | HEAD scrape/slap/slide pairs; in progress on techniques: `CascadeResolver::relation` | | Cascade.theMatrixIsTheSpecs | |
| Priority rules | 3 | partial | HEAD slide blocks scrape/slap; in progress on techniques | | Cascade.thePriorityRulesDecide | |
| 10 ms graceful preemption | 3.3 | yes | Scrape/SlapEngine `preempt` | | Scrape.aPreemptedScrapeFadesOutInTenMilliseconds | |
| 7-stage schedule order | 4 | partial | HEAD fixed call order; in progress on techniques: `TechniqueLayer::stages` | | TechniqueLayer.theModulesRunInTheDocumentedOrder | |
| Combined presets ×6 | 5 | no | in progress on techniques | in progress on techniques | Cascade.theCombinedPresetsArmTheirTechniques | |
| Conflict red slash + tooltip | 6 | no | in progress on techniques | in progress on techniques | Cascade.aConflictShowsOnThePillWithinAFrame | |
| Pairwise same/different-string tests | 7, 10 | no | in progress on techniques | | Cascade.everyPairResolvesAsDocumented | HEAD: SlapWiring.slapAndScrapeTakeTheStringFromEachOther only |
| Combined render within 0.5 dB of reference | 7, 10 | no | no stored reference anywhere | | | |
| Fuzz 10k gestures | 7 | no | in progress on techniques | | Cascade.aFuzzOfGesturesLeavesNothingBehind | |
| No undo for cascade decisions; arming does | 8 | no | in progress on techniques: TechniqueUndo | | TechniquesUi.theUndoClassesGroupAsSpecified | HEAD now has UndoHistory; technique classes absent |
| All six ≤ 2.5% CPU; body tap ≤ 1% | 9 | no | untested | | none | |

### engine-technique-layer.md

One row changed: BodyCoupling `driveDirect` now exists and the body tap calls it. Everything else is as before. TapEngine, MuteEngine, CascadeResolver, TechniqueLayer and undo classes are in progress on techniques. Two things are missing everywhere: ModMatrix `PreBendEvent`, and APVTS groups (there is no `AudioProcessorParameterGroup` in Source). There is also no low-CPU-class banner, although CpuRelief now has its own "CPU limit" banner. Counts on HEAD: yes 7 / partial 5 / no 8.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| ScrapeEngine, SlapEngine | 1 | yes | DSP/Noise/ScrapeEngine, DSP/Slap/SlapEngine | | Scrape.*, Slap.* | |
| TapEngine, MuteEngine, CascadeResolver | 1 | no | in progress on techniques: DSP/Techniques/* | | Tap.*, Muting.*, Cascade.* | |
| Pipeline 2b/2c | 2 | partial | HEAD scrape/slap; in progress on techniques: `LuthierEngineTechniques.cpp` | | TechniqueLayer.theModulesRunInTheDocumentedOrder | |
| Zero-cost idle, no allocs, reset | 0.2-0.3 | yes | Scrape/Slap | | Scrape.idleCostsNothing; Slap.idleAndActiveStayInBudget | |
| TechniqueEngine gesture types | 3.1 | partial | HEAD `TechniqueId {scrape, slap}` | | TechniqueTriggers.* | |
| StringEngine N contact points | 3.2 | no | in progress on techniques | | Tap.twoTapsOnAStringAreCaposInSeries | |
| RhythmEngine mute_type | 3.3 | no | in progress on techniques | | Muting.aPatternsMuteRowReachesItsNotes | |
| BodyCoupling `driveDirect(impulse, position)` | 3.4 | yes | BodyCouplingBank.cpp:349; called LuthierEngine.cpp:1619 (body tap) and :1978 | | BodyCoupling.BC06_BC09_aTapRingsTheStringsNearAMode | **was no**; landed with realism merges |
| ModMatrix PreBendEvent + range factor | 3.5 | no | none | | | |
| KS range 12-18, no conflict | 3.6 | yes | TechniqueTriggers.h:41-49 | | TechniqueTriggers.keyswitchesAreTakenAndBecomeEvents | RH tool override uses CC 102, no KS clash |
| MPE Y/Z to slide/bends | 3.6 | no | in progress on techniques | | | |
| Commands / TechniqueFiredResult | 4 | partial | HEAD atomics + `requestTrigger` | | TechniqueLayer.commandsDoNotAllocate (techniques) | |
| 6 arm booleans + ~60 params | 5 | partial | HEAD `scrape_armed`, `slap_armed` + 37 | HEAD: 14 bass ones via SlapGroup only | | |
| APVTS groups | 5 | no | flat layout | | | |
| Pre-delta presets disarmed | 6 | yes | defaults off | | techniques: TechniqueLayer.preDeltaPresetsLoadDisarmedAndIdle | |
| Old bass-slap presets → SlapEngine | 6 | yes | same engine | CHARACTER > SLAP (bass) | BassTechniques.* | |
| Technique state in preset/snapshot | 7 | yes | params | | SlapPresets.everySlapFieldRoundTrips | |
| Undo classes | 8 | no | in progress on techniques: TechniqueUndo | | TechniquesUi.theUndoClassesGroupAsSpecified | |
| Perf idle < 0.1%; low-CPU banner | 9 | partial | scrape/slap idle tests | | Scrape.idleCostsNothing | no CPU-class detection |
| 1000 cmds/s no alloc; 100 pre-delta presets byte-identical | 10 | no | in progress on techniques (weakened) | | TechniqueLayer.* | |

### ambiguity-resolutions.md

Almost fully implemented, as before. Two rows changed:
- The MPE pass-through now has a test (`StrumDynamics.mpePassesThrough`).
- The migration table via content updates is only partial: `ContentPackage` merged (visual), but **nothing in the product calls it**.

The Doubler pitch/HP/LP defaults are still unasserted. B-07 (the legacy `doubler_on` migrates on every load) is still open. Counts: yes 22 / partial 2 / no 0.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Per-string feedback loop | 1.1 | yes | DSP/Feedback/FeedbackLoop | | Feedback.eachStringHearsItsOwnNote | |
| feedback_* params | 1.2 | yes | Parameters.h | Advanced SUSTAIN | | |
| Zero-amount bit-identical | 1.4 | yes | | | Feedback.zeroAmountIsBitIdenticalWhateverTheOtherSettings | |
| Stability; volume attenuation | 1.4 | yes | | | Feedback.staysBoundedForAMinuteAtFullTilt | |
| Feedback LED | 1.3 | yes | `FeedbackLed` | SUSTAIN card | | |
| Legacy feedback_on etc. | 1 | yes | hidden intentionally | n/a | Feedback.oldPresetsThatSwitchedItOnGetAnAmount | |
| Freeze | 2.1 | yes | FreezeOverlay | SUSTAIN Freeze | Sustain.freezeLayerHoldsItsLevelForASixtySecondHold | |
| E-Bow params | 2.2 | yes | EBowDriver | SUSTAIN E-Bow | EBow.* | |
| E-Bow timing | 2.4 | yes | | | EBow.aHeldNoteIsSteadyWithinHalfASecondAtHalfIntensity | |
| Doubler 8 params, ADT defaults | 3 | partial | `DoublerPedal` | POST-FX rack | Doubler.* (5) | pitch/HP/LP defaults unasserted |
| Doubler post-amp pre-cab | 3 | yes | | | Doubler.isAPostAmpRackPedal | |
| Mix 0 null / 100 at 22 ms | 3.2 | yes | | | Doubler.mixZeroIsTheDrySignal; mixFullIsACopyTwentyTwoMillisecondsLate | |
| Old doubler → pedal | 3 | yes | PresetManager::fromVar | hidden | Doubler.presetsWithTheOldDoublerGetThePedal | B-07: not gated on format version |
| Rubric voicer | 4.1-4.6 | yes | RubricVoicer | | RubricVoicer.* | |
| Rubric tests | 4.7 | yes | | | RubricVoicer.everyTemplateInEveryKeyAndStyleIsVoicedOrUnplayable | |
| Morph interpolation rules | 5.1 | yes | PresetMorph | | PresetMorph.theMidpointSwitchesDiscretesAndHalvesTheRest | |
| Morph UI, automatable | 5.2 | yes | Overlays.cpp | browser Morph row | PresetMorph.theBrowserMorphRowFillsTheSelectedSlot | |
| Morph ends / click-free / snapshot cancel | 5.3, 8 | yes | | | PresetMorph.* | |
| crossing_sps | 6 | yes | Rhythm/Patterns | | StrumDynamics.patternCrossingGivesTheDocumentedSpread | |
| MPE / hex bypass strum synthesis | 6 | yes | | | StrumDynamics.mpePassesThrough; liveSpreadWins | **was partial** |
| migration.json | 7 | yes | Resources/Guitars/migration.json | | GuitarMigration.everyPreM49NameResolvesToItsShippedGuitar | |
| Unknown guitar banner | 7 | yes | | banner | GuitarMigration.anUnknownGuitarKeepsThePresetAndSaysSo | |
| Migration table via content updates | 8 | partial | Updates/ContentPackage (install + verify) | no: ContentPackage has no caller outside tests; migration lookup ignores ContentUpdates/ | ContentPackage.* | merged but not wired |
| Feedback/freeze/E-Bow mod destinations | 8 | yes | ModMatrix | right-click Modulate | | |

### qa-polish.md

This group moved most. visual, release and review landed the following:
- CI on every push, on 3 platforms (build.yml, ci.yml), plus nightly (nightly.yml) and release (release.yml) workflows.
- The 100k parameter fuzz, the 10k state fuzz, Boot, Memory, Stress (32 instances, MIDI storm, learn ×100, undo ×1000, bus layout, slide/range toggles) and byte-flip tests for presets and guitars.
- The guitar audio round-trip, the EffectsQa, RealismQa and Latency tests, and Screenshots.
- THIRD_PARTY_LICENCES.txt and panel "?" help buttons.

Still missing:
- the golden-render suite;
- translations (`Resources/i18n/` does not exist);
- the AAX / Pro Tools leg of the matrix;
- VST3↔AU format switching;
- the bug bash, videos and the real EULA/refund policy (the EULA is a placeholder);
- the reachability gate: B-11 still fails.

pluginval runs in CI at strictness 10 **without** the long timeout that B-12 needs. Counts: yes 30 / partial 13 / no 7.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Host × platform × SR × buffer × CPU matrix | 1 | partial | build.yml builds/tests Linux, Windows, macOS universal | n/a | CI unit tests + validators | no host runs; no AAX format (CMakeLists.txt:35) |
| CI on every push, nightly | 2 | yes | .github/workflows/build.yml, ci.yml, nightly.yml | n/a | | **was no** |
| Golden render 36 presets × SR × block | 2 | no | none (no golden WAVs in repo) | n/a | | |
| Param fuzz 100k | 2 | yes | | | Parameters.fuzzAcrossHundredThousandStates | nightly (LUTHIER_PERF) |
| State fuzz 10k | 2 | yes | | | StateModel.tenThousandRandomOperationsLeaveNoStuckState | |
| Boot ≤ 400/200 ms | 2 | partial | | | Boot.coldAndWarmInstantiationStayInBudget | default run allows 2×; spec numbers only under LUTHIER_PERF (warm measured 330-390 ms) |
| Memory 60 min < 900 MB | 2 | yes | Support/MemoryProbe.h | | Memory.sixtyMinuteSessionNoMonotonicGrowth | nightly |
| Preset round trip to the ulp | 2 | yes | | | Presets.everyFactoryPresetRoundTripsToTheUlp | B-16: `noise_player_distance` drifts 1 ulp through the CLAP wrapper |
| Guitar round trip audio null | 2 | yes | | | Workshop.everyFactoryGuitarRoundTripsInAudio | **was partial** |
| Migration vs pre-M49 golden | 2 | partial | resolution only | | GuitarMigration.* | no golden |
| pluginval strictness 10, zero warnings, every format | 2 | partial | scripts/pluginval.sh (VST3, str 10, CI); build.yml str 5 per push, 10 nightly; clap-validator | | | B-12: default 30 s timeout fails Parameter thread safety; script sets no timeout |
| Crash: 32 instances | 3 | partial | | | Stress.thirtyTwoInstancesRenderInTurn | sequential, not concurrent |
| Crash: VST3↔AU switching | 3 | no | | | | |
| Crash: SR / block change | 3 | yes | | | Engine.sampleRateChangesAreSurvived; blockSizeChangesAreSurvived | |
| Crash: loads under MIDI storm | 3 | yes | | | Stress.midiStormDuringLoadsRecallsAndGuitarLoads | |
| Crash: learn ×100, undo ×1000, bus layout, toggles | 3 | yes | | | Stress.midiLearnArmDisarmHundredTimes; undoRedoThousandTimes; busLayoutChangesMidPlay; slideAndAdvancedRangeTogglesMidPlay | |
| Corrupt preset byte-flip | 3 | yes | | | Presets.everyByteOfAFactoryPresetFlippedIsRefusedOrLoads | |
| Corrupt .luthierguitar byte-flip | 3 | yes | | | Workshop.everyByteOfAFactoryGuitarFlippedIsRefusedOrLoads | **was no** |
| Missing files fallback + banner | 3 | yes | | banner | Workshop.aMissingPartFallsBackAndSaysSo | |
| Every control has a tooltip | 4 | yes | Widgets `attachTo` | | GuiReach walk | |
| Tooltip/label in every locale | 4 | no | Accessibility/Localisation (loads `Resources/i18n/<code>.json`) | | Localisation.catalogCoversTheUi | no i18n folder ships; installer offers 5 languages for itself only |
| Accessibility role/value with units | 4 | partial | Accessibility/* | | AccessibilityTests | |
| Right-click reset / entry / paste | 4 | yes | Widgets menu | | GuiReach.operatingEachControlWritesItsParameter | |
| Advanced-range arc, "*", padlock | 4 | yes | RangesUi | | RangesUi.* | |
| Every automatable param reachable | 4, 6 | no | 27 technique + macro_assign_a/b + pickup_blend have no control; the STRUM lower rows are laid out at zero height | | GuiReach.everyAutomatableParameterHasAVisibleControl (fails, B-11) | improved: 14 bass params now in SlapGroup |
| 75-200% scale, every palette | 4 | yes | | | Accessibility.uiScaleStepsAndFontFloor; Screenshots.everyPanelInEveryPalette | **was partial** |
| Reduced motion | 4 | yes | | | Accessibility.reducedMotionRemovesAnimation | |
| Panel "?" opens docs | 4 | yes | UI/PanelHelpButton (EasyPanel, AdvancedPanel) | yes | Onboarding.everyPanelsHelpIconOpensItsOwnTopic | **was partial** |
| Dialogs: Escape closes | 4 | yes | | | Editor.everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt | |
| Empty-state hints | 4 | partial | FirstEncounterHint | | FirstEncounterHint.showsOnceAndOnlyInTheFirstSession | no systematic empty-state check |
| Workshop checks | 4 | yes | | | WorkshopQa.*; WorkshopBench.abRecallRoundTrips; Editor.everyHitRegionOnTheIllustrationDescribesItself | |
| Factory presets click-free, headroom | 5 | yes | | | Combo.everyFactoryPresetPlaysEveryPhrase; presetSwitchUnderARingingNoteDoesNotClick | B-15: P-Bass Flatwound fails 5 renders (string_age Old) |
| DC null -100 dBFS on silence | 5 | yes | | | Engine.silenceInSilenceOutWithNoiseFloorOff | B-04 FIXED (floor is hum by design; no gate, U-2) |
| Mono compatibility | 5 | yes | | | Engine.monoCompatibility | |
| Pedal/amp checks | 5 | yes | | | Effects.zeroMixIsABypassWithinMinus80; toggleIsClickFree; Amp.gainSweepIsMonotonicAt1kHz; neutralToneStackIsFlatWithin1dB; coldStartHasNoTransient | **was partial** |
| Realism checks | 5 | yes | | | Squeak.aThousandRunsAreByteIdentical; Buzz.oneDecibelUnderTheThresholdIsCleanThreeOverBuzzes; Slide.theBarArrivesWithinTwoCents; Circuit.atVolumeFiveKinmanHoldsTheTiltNoneLosesIt | **was partial** |
| Bass slap/pop/ghost checks | 5 | yes | | | SlapWiring.*; BassTechniques.* | |
| Every gui-integration 19 feature wired | 6 | partial | | | GuiReach | TECHNIQUES tab absent; the 3 new rows (noise floor, sustain shape, stability) not re-verified here |
| User docs match build; screenshots; translated | 6 | partial | docs/USER_MANUAL.md, PLAYING_TECHNIQUES.md; Screenshots test | | Screenshots.everyPanelInEveryPalette | HelpContent documents a TECHNIQUES tab HEAD lacks; no translations |
| MIDI export round-trips | 6 | yes | | | MidiExport.luthierRoundTripNullsEveryFactoryPreset | technique classes not exported |
| Drag-out to Downloads | 6 | partial | | | | not verified |
| Performance targets | 7 | no | | | PerfBudget.scenarioTotals (LUTHIER_PERF only) | B-13: idle 8-19% of a core |
| Latency within 1 sample | 7 | yes | | | Latency.anImpulseArrivesWhenReported; ReviewRegression.theOversamplerDelaysWhatItReports | R-112: limiter's 1.5 ms look-ahead not in the reported sum (open) |
| Bug bash | 8 | no | process | | | |
| Installer polish (signed Win, notarized pkg, .deb/.tar.gz) | 9 | partial | packaging/windows/Luthier.iss, scripts/package_macos.sh, CPack | | | pipeline exists; certificates not obtained (RELEASING.md 7) |
| Manual complete + translated; videos | 10 | partial | docs/USER_MANUAL.md | | | |
| THIRD_PARTY_LICENCES | 11 | yes | THIRD_PARTY_LICENCES.txt, Resources/THIRD_PARTY_LICENCES.txt | | Legal.thirdPartyLicencesNameEveryBundledDependency | **was partial** |
| Trademark-free names | 11 | yes | Tools/trademark_scan.py | | Trademarks.* | |
| EULA, refund policy | 11 | partial | packaging/common/EULA.txt (**placeholder**) | installer licence page | | no refund policy |
| Final human check; post-release monitoring | 12-13 | no | process; Diagnostics crash log is opt-in and local, with no upload | | | |

### performance-budget.md

Most of the enforcement merged with visual:
- per-module units, scenario totals, voice and SR scaling;
- boot, memory and latency tests;
- alloc/lock traps (ThreadProbe);
- CpuRelief steps 1-7 with a banner and a string-drop opt-out in Options, plus the oversampler downgrade above 96 kHz;
- the nightly JSON dashboard.

Two problems remain:
- **The machine-relative checks run only under `LUTHIER_PERF=1` (nightly).** The per-push suite does not enforce the budgets.
- **Idle is still 6-12× over budget (B-13 open).** The idle scenario would fail nightly.

The per-module list covers only 8 modules: Coupling, Circuit, Amp, Body, Cab, Room, Master and the pre chain. StringEngine, noise, Slide, Scrape/Slap and the 3 new rows have separate tests at best. R-113 is a blocking lock on the audio thread in the live part swap. There is still no per-pedal eco Quality option. cpu-quality-modes.md (a new spec, other group) covers this and is not implemented: the only quality param is `cable_quality`. Counts: yes 18 / partial 7 / no 2.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Per-module budget ≤ ×1.10 in CI | 0.1, 10 | partial | Tests/PerfBudgetTests.cpp | | PerfBudget.everyModuleWithinBudget | LUTHIER_PERF only; 8 modules, no StringEngine/Noise/Slide/Pickup |
| Regression > 10% flag / > 20% block | 0.3 | partial | nightly.yml dashboard JSON | | | no comparison against a previous run, so nothing flags or blocks |
| Audio callback allocates zero | 0.4 | yes | Support/ThreadProbe.h | | Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks; MidiLearn.learningDoesNotAllocateOrLockOnTheAudioThread | **was no** |
| Audio callback locks nothing | 0.5 | partial | ThreadProbe lock trap | | ThreadProbe.theLockTrapSeesALock; Engine.fiveMinutes… | R-113: `applyPartSwapLive` → `rebuildModalBank` takes a CriticalSection on the audio thread |
| Idle ≤ 1.5 units | 1 | no | | | PerfBudget.scenarioTotals ("idle") | B-13: 8-19% of a core; no silence short-circuit |
| Steady/heavy/realism/slide/bass totals | 1 | yes | | | PerfBudget.scenarioTotals | nightly only |
| NoiseFloor ≤ 0.15, 0 when idle (new row) | 1 | partial | DSP noise floor | | NoiseFloor.idleIsFree | active cost not measured in units |
| Sustain shape +0.3 on StringEngine (new row) | 1 | partial | StringEngine | | SustainDecay.costAndSafety | not in the per-module list |
| StabilityModel 0.02 (new row) | 1 | yes | StabilityModel | | TuningStability.costAndSafety | |
| Per-pedal cap 0.5 + eco Quality | 2 | no | no Quality param on any pedal | | PerfBudget.everyModuleWithinBudget (pre chain as a whole) | |
| Memory baseline ≤ 350 MB, cap 900 | 3 | yes | MemoryProbe | | Memory.baselineInstanceUnder350MB | |
| Session recorder off by default | 3 | yes | Capture/CaptureRing | | Capture.* | |
| Latency ≤ 128 main, ≤ 32 aux | 4 | yes | | | Latency.dspLatencyIsWithinBudget; Routing.perOutputLatencyIsConsistent | |
| Latency reported | 4 | yes | | | Engine.latencyIsReportedAndPlausible; Latency.oversamplerReportsItsGroupDelay | R-112 open (limiter look-ahead) |
| Boot cold/warm; standalone ≤ 1.5 s | 5 | partial | | | Boot.coldAndWarmInstantiationStayInBudget | 2× slack by default; standalone launch untested |
| Guitar load/tune/part swap/delta | 5 | yes | | | Workshop.guitarLoadStaysUnder300ms; hundredRandomPartSwapsStayUnder50ms; Tune.loadStaysUnder100ms; WorkshopSpectrum.hundredShadowRendersStayUnder40ms | |
| Shadow audition ×100 | 10 | yes | | | WorkshopBench.hundredAuditionsStayInBudget | |
| Voice-count scaling | 6 | yes | | | PerfBudget.voiceCountScaling | nightly |
| Coupling O(N) | 6 | yes | CouplingMatrix | | | |
| SR scaling ±10% | 7 | yes | | | PerfBudget.sampleRateScaling | nightly |
| Oversampling downgrade > 96 kHz | 7 | yes | DSP/Common/Oversampler.h | | Engine.oversamplingDowngradesAbove96k | |
| CPU relief ladder 1-7 at > 85% | 8 | yes | Support/CpuRelief; LuthierEngine.cpp:3035-3049 | | CpuRelief.laddersUpAtEightyFivePercentAndBackDown | |
| Relief 7 opt-out + "CPU limit" banner | 8 | yes | UI/CpuReliefUi; OptionsPages.cpp:2183 `cpuDropToggle` | yes: Options (Diagnostics) + banner | CpuRelief.stepSevenIsOptOut; CpuReliefUi.theOptOutIsSavedAndReachesTheLadder; theBannerComesOncePerEpisodeAndGoes | |
| Noise pools halve under load | 8 | yes | `playingNoise.getPool().setDegraded` | | CpuRelief.theEngineHalvesTheNoisePoolsOnlyUnderLoad | |
| Enforcement every merge + dashboard | 9 | partial | ci.yml (every push, no perf); nightly.yml dashboard | | | budgets are not enforced per merge |
| 60-min memory growth | 10 | yes | | | Memory.sixtyMinuteSessionNoMonotonicGrowth | |
| Impulse latency within 1 sample | 10 | yes | | | Latency.anImpulseArrivesWhenReported | **was no** |


### installer.md

This changed the most (release + visual):
- **Windows:** an Inno Setup `.exe` (packaging/windows/Luthier.iss). It has language choice, licence, components, space check, version check, HKLM keys, file associations for all 6 extensions, Start menu, a user-data prompt on uninstall and the Restart Manager.
- **Windows portable zip** (scripts/package_windows.ps1).
- **macOS:** a `.pkg` with a component picker inside a `.dmg`, with codesign, productsign and notarytool when their secrets are set (scripts/package_macos.sh). It includes a universal arm64+x86_64 build (ci_build.sh) and `Uninstall.command` with a purge prompt.
- **Linux:** `.deb`/`.tar.gz`.
- **Release pipeline:** release.yml writes SHA256SUMS (GPG-signed when a key is set) to a draft GitHub Release.
- **Folder tree + `.installed_version`:** `InstallLayout::ensure` runs in the processor constructor.
- **Other merged pieces:** preset migration backups, the migrated-load banner, and the UpdateDownloader behind Options > Updates "Download".

Still missing:
- `.rpm`, the MSI wrapper, a Docs component and a standalone-only bundle;
- macOS Launch Services associations (no CFBundleDocumentTypes);
- real certificates and the real EULA;
- a guitar migration backup;
- wiring for `.luthiercontent` (ContentPackage is uncalled, no drag-drop or Options entry) and delta patches;
- an install/upgrade test harness;
- the rollback banner.

`getInstallLayoutResult()` is unused. Onboarding detects upgrades from its own preference key, not from the marker. Counts: yes 19 / partial 10 / no 5.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Code-signed Win / notarized mac / PGP Linux | 0.2 | partial | package_windows.ps1 signtool; package_macos.sh codesign/productsign/notarytool; release.yml GPG | | | runs only when secrets are present; certificates not obtained |
| Uninstall removes all, keeps user data, purge | 0.3 | yes | Luthier.iss `CurUninstallStepChanged`; macos/Uninstall.command; linux/uninstall.sh `--purge` | | | |
| Deterministic builds | 0.5 | partial | nightly.yml two-build .deb `cmp` (warning only); Inno `TimeStampsInUTC` | | | not enforced; Win/mac not checked |
| Windows Inno .exe, naming | 1 | yes | packaging/windows/Luthier.iss `OutputBaseFilename=Luthier-<v>-Setup-win64` | | | **was no** |
| Win flow: language, licence, components, paths, space, version, rollback | 1.1 | partial | Luthier.iss [Languages] ×5, LicenseFile, [Components], ExtraDiskSpaceRequired, `InitializeSetup` version compare | | | no Docs component; content path fixed (spec: editable); no "What's new" link; no splash |
| Win associations ×6, Start menu, Add/Remove | 1.1 | yes | Luthier.iss [Registry] HKA Luthier.File, [Icons]; FileOpenRouter + StandaloneApp.cpp:123 | | FileOpen.everyAssociationRoutesToItsLoader; FileOpen.theCommandLineNamesTheFile | association is an optional task (Components: standalone) |
| Silent /S /D, exit codes, HKLM keys | 1.2 | partial | Inno `/SILENT` `/VERYSILENT` `/DIR=`; HKLM Software\Luthier InstallPath/Version | | | not the spec's `/S` `/D=`; exit codes undocumented |
| Win uninstaller: manifest, user-data box, DAW check | 1.3 | yes | Inno uninstall log; user-data MsgBox; `CloseApplications=yes` | | | DAW check is the Restart Manager only (RELEASING.md 8) |
| macOS .pkg in signed, notarized .dmg | 2 | yes | scripts/package_macos.sh → `Luthier-<v>-macOS.dmg` | | | **was no**; unsigned without secrets |
| macOS components, paths, Launch Services associations | 2.1 | partial | distribution.xml choices au/vst3/clap/app/content | | | no Docs component; paths match spec (content in /Library/Application Support/Luthier); **no CFBundleDocumentTypes**, so no associations; no "Open Luthier" button |
| Universal binary | 2.2 | yes | scripts/ci_build.sh `CMAKE_OSX_ARCHITECTURES=arm64;x86_64`; `hostArchitectures="arm64,x86_64"` | | | **was no** |
| macOS Uninstall.command | 2.3 | yes | packaging/macos/Uninstall.command (pkgutil forget, purge prompt) | | | |
| Linux .tar.gz + .deb | 3 | yes | CMakeLists.txt:302 CPack TGZ;DEB; scripts/package_linux.sh | | ci.yml package step checks .deb contents | |
| Linux .rpm | 3 | no | | | | RELEASING.md 8 lists it as a known gap |
| Linux layout, .desktop, MIME, icon | 3.1 | yes | packaging/linux/* | | ci.yml `desktop-file-validate` | |
| postinst caches; install.sh; uninstall.sh | 3.2 | yes | packaging/linux/postinst, postrm, install.sh, uninstall.sh | | | |
| Dependencies documented | 3.3 | yes | CPACK_DEBIAN_PACKAGE_DEPENDS; RELEASING.md 7 | | | |
| Standalone-only bundle | 4 | partial | Win portable zip contains the standalone | | | no dedicated bundle on mac/Linux |
| Update banner + release notes | 5.1 | yes | PluginEditor header banner; OptionsPages UpdatesPage | header / Options > Updates | Telemetry.* | |
| "Download" to Downloads, no auto-launch | 5.1 | yes | UpdateDownloader; OptionsPages.cpp:1739 `downloadButton` → `startDownload` | Options > Updates | Updates.theDownloadLandsInDownloadsUnderItsOwnName | **was partial** |
| Delta patches | 5.2 | partial | ContentPackage delta + hash rollback | no (no caller) | ContentPackage.aBadHashRollsBackAndOffersTheFullDownload | installer deltas not built (RELEASING.md 8) |
| First-run tree (18 folders), plugin.json, marker | 6 | yes | Support/InstallLayout `subfolders()`, `ensure` (PluginProcessor.cpp:65) | | InstallLayout.createsTheTreeAndMarker | **was no**; plugin.json creation not checked |
| Marker absent → onboarding; differs → upgrade banner + migrations | 6 | partial | Onboarding `getWelcomeDue` (preference key `kWelcomeVersionKey`) | welcome/upgrade banner | Onboarding.anUpgradeWelcomesOnceAndKeepsTheUsersData; InstallLayout.differentVersionReportsUpgrade | marker result (`getInstallLayoutResult`) is never read; no migration pass is triggered by it |
| Enterprise CLI config, policy pre-placement | 7 | partial | Telemetry policy file honoured | Options "managed by policy" | Telemetry.* | installers do not place the policy file |
| MSI wrapper | 7 | no | | | | |
| Preset without ranges + backup | 8 | yes | PresetManager `backupMigratedOriginal`, `backupFolderFor` | | Presets.backupsGoToThePresetsRootBackupFolder; Ranges.theRangesBlockRoundTripsAndDerivesWhenAbsent | **was partial** |
| Guitar via migration.json + backup | 8 | partial | migration yes | | GuitarMigration.* | no guitar backup |
| Old .luthierloop / .mid | 8 | yes | Looper / MidiExport generic | | | |
| Info banner on first migrated load | 8 | yes | | banner | Editor.aMigratedPresetRaisesOneInfoBanner | **was no** |
| Portable Windows zip | 9 | yes | scripts/package_windows.ps1 `Luthier-<v>-portable-win64.zip` | | | **was no**; app does not detect portable mode (user data still goes to Documents) |
| SHA-256 + PGP manifest | 10 | yes | scripts/release_manifest.sh; release.yml "Checksums", "Sign the checksum manifest" | | | canonical URL not set |
| `.luthiercontent` → ContentUpdates/<name> | 11 | no | Updates/ContentPackage (library only) | no: not in `isInterestedInFileDrag` (PluginEditor.cpp:1255), not in Options | ContentPackage.aSignedPackageInstallsIntoItsFolder; aBadOrMissingSignatureIsRefused; pathTraversalAndCodeAreRefused | a class that nothing calls; loaders do not read ContentUpdates/ |
| Rollback plan (banner) | 12 | no | process; release.yml drafts only | | | |
| Install/uninstall/upgrade/downgrade tests | 13 | no | InstallTests (layout), FileOpenTests (in-app) | | | no installer-level harness |


---

### Top gaps (group G)

1. **The techniques branch is still unmerged and now 177 commits behind, with 21 conflicting files.** These include Parameters.cpp/.h, LuthierEngine, StringEngine, PresetManager, FactoryPresets, CharacterPanel, AdvancedPanel, EasyPanel and Overlays. Until it merges, HEAD has no TECHNIQUES tab: no slide controls, no mute, no tap, no bend and no cascade (about 64 params, 74 tests). The automation-index order of the appended params must be settled in that merge.
2. **27 scrape/slap technique params have no control on HEAD (B-11), and there is no scrape or slap fire button.** `GuiReach.everyAutomatableParameterHasAVisibleControl` fails. Scrape cannot be heard at all from the GUI. Slap can be heard only via the RH-tool Slap style.
3. **Idle CPU 6-12× over budget (B-13).** There is no silence short-circuit. PerfBudget measures it, but only nightly under LUTHIER_PERF, so no per-push gate exists.
4. **Shipping blockers: no real certificates and a placeholder EULA.** The Windows/mac installers are ready but produce unsigned output. There is also no refund policy.
5. **No translations.** `Resources/i18n/` does not exist, and new UI uses literals. This fails ship gate 2 (qa-polish 4, 6).
6. **Help documents features that do not exist.** HelpContent's technique-* topics describe a TECHNIQUES tab and sub-tabs that HEAD lacks. Global search and help lead users to missing UI (qa-polish 6).
7. **No golden-render regression suite** (qa-polish 2). CI checks bounds only.
8. **pluginval strictness 10 in ci.yml has no timeout override,** so B-12 (Parameter thread safety > 30 s) can fail CI on slow runners.
9. **Muting-as-rhythm absent** (WIP only). realism-b's MutedThump and adjacent mute cover only the fretting-hand spread.
10. **Microtonal bends absent**, and the branch adds a second vibrato system beside `vibrato_*`.
11. **Two-hand tapping absent.** The hammer-on velocity threshold is hard-coded at 0.63 (spec MIDI 40), and `legato_window` is 40 ms (spec 150).
12. **`.luthiercontent` content updates are not wired.** ContentPackage is uncalled: there is no drag-drop, no Options entry, and loaders ignore ContentUpdates/.
13. **macOS file associations missing** (no CFBundleDocumentTypes/UTIs). Windows has them.
14. **R-113: the audio thread takes a CriticalSection on a live part swap** (performance-budget 0.5).
15. **Scrape and slap section-4 presets not loadable** (test-only `fromPreset`, on every branch).
16. **No AAX format,** so the Pro Tools leg of the qa-polish host matrix cannot run.
17. **The `.installed_version` marker is written but never read.** Upgrade detection is a separate preference, and no migration pass runs on a version change.
18. **Defaults contradict specs:** `double_thump_enabled` is false (spec: rebound on for thumb), and `scrape_string_mask` defaults to held strings (spec: wound only).
19. **No APVTS parameter groups.** About 700 flat params reach host automation lists.
20. **No per-pedal eco/Quality option** (performance-budget 2; cpu-quality-modes.md unimplemented).

### Unspecified gaps noticed

- **SlapGroup visibility is bass-only.** The slap engine now works on any guitar (string-slap 0.4, Easy "Slap" style), but the only strength and position controls disappear when a guitar is loaded.
- **The RH-tool Slap path bypasses `slap_armed` and the trigger settings.** The same sound has two arming models, and nothing in the UI explains the difference.
- **No keyswitch legend.** KS 12-18 on HEAD (19-21 on techniques) plus the articulation keyswitches are shown nowhere.
- **No per-technique audition phrase.** The AUDITION phrase cannot demonstrate scrape, slap, tap or bend.
- **The portable zip is not a portable mode.** The app still writes to Documents/Luthier and the registry-free promise is only half kept, with no portable marker or settings-beside-exe.
- **The Inno installer's content path is fixed** (spec: editable), and there is no Docs component on any platform, although docs/USER_MANUAL.md exists.
- **The crash log is local and opt-in, with no upload or symbol server.** qa-polish 13 post-release monitoring assumes one.
- **The nightly dashboard JSON is produced but never compared,** so it flags no regressions.
- **The EULA placeholder ships in CI artefacts** (.deb licence page on the installer). A build uploaded by mistake would carry "PLACEHOLDER" text.

### Small glue candidates

| Item | Param IDs / symbol | Panel it should go on | Effort |
|---|---|---|---|
| Scrape controls (interim until techniques merges) | `scrape_armed`, `scrape_trigger`, `scrape_trigger_cc`, `scrape_direction`, `scrape_sweep_source`, `scrape_sweep_cc`, `scrape_start_mm`, `scrape_end_mm`, `scrape_duration`, `scrape_pressure`, `scrape_tool`, `scrape_angle`, `scrape_string_mask`, `scrape_retrigger` | CHARACTER > NoiseGroups beside `pick_scrape_amount` (final: TECHNIQUES > SCRAPE) | low: attach only |
| Scrape / slap fire buttons | `ScrapeEngine::requestTrigger()`, `TechniqueTriggers::request(TechniqueId::slap, …)` | Easy Playing strip beside `RightHandToolSelector` | trivial |
| Slap technique controls | `slap_armed`, `slap_type`, `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`, `slap_ghost_cc`, `slap_force`, `slap_palm_position_mm`, `slap_string_mask`, `slap_ghost_mode`, `slap_rebound_gap`, `slap_snap_back`, `slap_body_part` | extend `UI/SlapGroup` (CharacterPanel.cpp:353) and show it on every guitar, not only bass | low |
| Scrape factory presets ×5 | `ScrapeSettings::fromPreset(ScrapePreset::*)` | FactoryPresets "Techniques" category | low |
| Slap factory presets ×5 | `SlapSettings::fromPreset(SlapPreset::*)` | FactoryPresets "Techniques" category | low |
| Hammer-on threshold param | `TechniqueEngine::setLegatoVelocityThreshold` (new param, default MIDI 40) | Advanced PERFORMANCE next to `legato_window` | low |
| Default fixes | `double_thump_enabled` → true; `scrape_string_mask` default → wound | Parameters.cpp:776, :753 | trivial (changes preset defaults) |
| Content package drag-drop / Options entry | `ContentPackage::install` from `filesDropped` + Options > Updates "Install content…" | PluginEditor.cpp:1255 `isInterestedInFileDrag`; OptionsPages UpdatesPage | low |
| Read the install marker | `processor.getInstallLayoutResult()` → Onboarding upgrade banner + migration pass | PluginEditorOnboarding.cpp | trivial |
| pluginval timeout | `--timeout-ms 900000` in scripts/pluginval.sh RUN | CI | trivial |
| Hide help topics for absent UI | HelpContent technique-* entries (gate until techniques merges) | HelpContent.cpp:496-548 | trivial |
| macOS document types | CFBundleDocumentTypes/UTExportedTypeDeclarations for the 6 extensions (`PLIST_TO_MERGE` in juce_add_plugin) | CMakeLists.txt | low |

---

## Group H: deep integration

Base: `/home/user/luthier` on `claude/luthier-audit` @ 961cd55 (read-only). The tree now includes realism-a/b/c, model-gaps, tune-help, visual, release and review. Only `origin/claude/luthier-techniques` is unmerged. Rows marked "in progress on techniques" exist only there.
Spec texts for the six original files have not changed since the previous audit (last touched 2026-09-23). `licensing.md` and `editions.md` are new (2026-09-24). Both say "Nothing here is implemented yet" and are design proposals, so nearly every row there is "no" by design.

Most important finding, still open: **a saved `.luthierpreset` file still does not contain the modulation matrix, snapshot bank, MIDI Learn mappings, rhythm-engine state, character seed/wear, tone-match IR references or routing.** `PresetManager::toVar` writes only parameters, ranges, guitar, strings and midiMap. Those blocks go only into the host blob (`LuthierAudioProcessor::getStateInformation`). The comment in `applyCurrentSetlistEntry` ("The preset carries its own snapshot bank") is false. So a setlist entry's `snapshotIndex` recalls from whatever bank happens to be loaded. The techniques branch adds a `techniques` block through a `captureTechniquesBlock` hook. That hook is the pattern the other blocks need.

What changed since the previous audit: undo is now essentially complete (UndoHistory, boundaries, 200 ms grouping, history panel with search, Ctrl-Y, Ctrl-Alt-Z, undo-depth footer, family-switch warning, 32 tests). Also new:
- a migration banner and migration backup
- a CPU-limit banner
- 36 factory presets
- 6 example tunes, 12 MIDI clips and 10 example setlists (written at first run)
- pluginval strictness 10 in CI
- a 10 000-op state fuzz and a 32-instance test
- a `.luthiercontent` package class, which nothing calls

Still missing: MIDI out in VST3/AU, preset blocks, the mono-main rejection, parameter groups, crash dumps, corrupt-config handling, log pruning and verbose toggle, MIDI Learn timeout, the section-8 intersections, setlist save, and all of licensing/editions.

### file-formats.md

Summary: 54 requirements. **yes 20, partial 25, no 8, n/a 1.** Since the previous audit: the migration backup is merged (at a different path from the spec). The byte-identical preset round trip and a per-byte fuzz are now tested. The `.luthiercontent` format class is merged but unreachable. Blocks are still missing from presets, and setlist, pattern and loop files still have no magic or schema.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Every file UTF-8 JSON | 0.1 | yes | all writers (`JSON::toString`) | n/a | - | |
| Top-level `schema` int, missing = 1 | 0.2 | partial | Part/PartLibrary/TuneFile/MidiProfiles use `schema`; PresetManager uses `schemaVersion` | n/a | - | A preset with no schemaVersion is refused (`BAD_SCHEMA`) rather than treated as 1; setlist/pattern/loop have no schema |
| Unknown fields preserved on load and written back | 0.3 | partial | PresetManager::fromVar `unknownFields` (top level); TuneFile | n/a | Presets::unknownFieldsSurviveARoundTrip; TuneBuilder::unknownFieldsAreKeptAndWrittenBack | Not setlist/pattern/guitar/host blob |
| Migrations documented per format | 0.4 | partial | PresetManager::needsMigration lists magic, ranges, guitar-name, pickupPosition | n/a | RangeTests, GuitarMigrationTests | Not tabulated in the spec |
| Canonical extension + magic marker | 0.5 | partial | preset/guitar/part/tune/midprofile magic; setlist `format:"luthierset"`, loop `format:"luthierloop"` | n/a | Presets::aFileWithoutTheMagicMarkerIsRefused | Pattern/kit files have none; loaders for setlist/loop don't check |
| Portable relative paths + registered folders | 0.6 | partial | IrSlot relative paths; guitar refs | n/a | ToneMatch::presetPathsAreRelativeUnderTheLibraryRoot | Setlist `presetPath` absolute (TuneExamples writes absolute paths too); no registered-folders list |
| `.luthierpreset` type | 1 | yes | PresetManager kFileExtension/kMagic | Header File menu, browser, OS file-open (FileOpenRouter) | FileOpen::everyAssociationRoutesToItsLoader | |
| `.luthierguitar` type | 1 | yes | PartLibrary kMagic | Workshop Save As; FileOpenRouter | Workshop::everyFactoryGuitarLoadsAndRoundTrips | |
| `.luthierpart` type | 1 | yes | Part.h | Workshop | Workshop::theFactoryLibraryIsThere | |
| `.luthiertune` type | 1 | yes | TuneFile.h | TUNE tab; FileOpenRouter | TuneBuilder tests | |
| `.luthierpattern` type | 1 | partial | Patterns.cpp RhythmPattern::saveTo | RHYTHM tab save | RhythmPatterns::patternsRoundTripThroughJson | No magic/schema |
| `.luthierset` type | 1 | partial | Setlist.cpp | LiveStrip menu (open), LivePanel (edit in session) | LiveSetlist::roundTripsThroughJson; SampleContent::theTenExampleSetlistsInstallOnceOverTheFactoryBank | **Still no "Save setlist" in the UI.** `Setlist::saveTo` is called only by `TuneExamples::installExampleSetlists`. LivePanel edits live only in the host blob |
| `.luthierloop` type | 1 | partial | Looper::save (folder + loop.json, `format`) | PRACTICE drawer; FileOpenRouter | - | A folder, not a single file |
| `.luthiercontent` type | 1 | partial | Updates/ContentPackage::apply (merged from visual) | **no** | ContentPackage::* (4) | Nothing outside tests calls `ContentPackage::apply`. Not in FileOpenRouter |
| `.midprofile` type | 1 | yes | Export/MidiProfiles kProfileMagic | MIDI OUT panel | MidiExportTests | |
| `.mid` Luthier/Generic | 1 | yes | Export/MidiPerformance, MidiProfiles | File → Save last MIDI take / Import MIDI | MidiExportTests, MidiImportTests | |
| wav/aiff/flac audio | 1 | yes | AudioExporter | File → Export audio | - | |
| `.mp3` backing tracks in | 1 | partial | BackingTrack `registerBasicFormats` | PRACTICE | - | JUCE_USE_MP3AUDIOFORMAT still not set |
| Preset `meta` block | 2 | partial | PresetManager::toVar | Save As overlay | - | Flat root; `author` hard-coded ""; no created/modified dates, no version_created |
| Preset `guitar {reference, override}` | 2 | yes | getGuitarBlock / takeGuitarBlock | n/a | WorkshopPresets::aMissingGuitarFileFallsBackToItsType | |
| Preset `parameters` | 2 | yes | toVar (normalised, ULP-stable) | n/a | Presets::everyFactoryPresetRoundTripsToTheUlp | Normalised, not plain values |
| Preset `ranges` block | 2 | yes | RangeState::toVar | Options → Ranges / padlock | Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent | |
| Preset `modulation` | 2 | no | only in getStateInformation | n/a | - | **Lost on preset save/load.** Key in fromVar's `known` list, so an incoming block is silently dropped too |
| Preset `snapshots` | 2 | no | only in getStateInformation | n/a | - | **Lost on preset save**; breaks setlist snapshot recall |
| Preset `midi_mappings` | 2 | partial | `midiMap` (CC → MidiTarget) | n/a | - | MIDI Learn (`midiLearn.toVar`) only in host blob |
| Preset `rhythm_engine` | 2 | no | host blob `rhythm` only | n/a | - | |
| Preset `effects_state` | 2 | partial | slot params inside `parameters` | n/a | PresetPedalTests | Equivalent content |
| Preset `midi_out_profile` | 2 | no | routing only in host blob | n/a | - | |
| Preset `techniques` block | (engine-technique-layer 7) | no | - | - | - | in progress on techniques (`captureTechniquesBlock` / `onTechniquesBlockLoaded`) |
| Migration schema 1 → ranges | 2 | yes | fromVar derive | n/a | RangeTests | |
| Migration `guitar.name` → reference | 2 | yes | migration.json | n/a | GuitarMigration::aPresetNamingAnOldGuitarLoadsItsReplacement | |
| Backup original on migration to `Presets/Backup/<date>/<name>-v<schema>` | 2 | partial | PresetManager::backupMigratedOriginal (merged) | n/a | ModelGapsUi::aMigratedPresetKeepsItsOriginal | Writes to `<preset's folder>/Backup/…`, not the Presets-root `Backup` that `backupFolderFor` uses for overwrite backups |
| `.luthierguitar` schema | 3 | yes | WorkshopGuitar::toVar/fromVar | Workshop | Workshop::everyFactoryGuitarLoadsAndRoundTrips | |
| `.luthierpart` schema | 4 | yes | Part.cpp | Workshop | Workshop::theFactoryLibraryIsThere | |
| `.luthiertune` schema | 5 | yes | TuneFile.cpp | TUNE | TuneBuilder::aHundredRandomTunesRoundTripByteIdentical | |
| `.luthierpattern` schema | 6 | partial | Patterns.cpp | RHYTHM | patternsRoundTripThroughJson | No magic |
| `.luthierset` schema | 7 | partial | Setlist::toVar | LiveStrip | LiveSetlist::roundTripsThroughJson | `format` not `magic`; no `schema`, no meta dates; loadFrom checks neither |
| `.luthierloop` contents | 8 | partial | Looper::save | PRACTICE | - | |
| `.luthiercontent` signed zip manifest | 9 | partial | ContentPackage (RSA-signed manifest.sig, SHA-256 per file, path-traversal refusal) | no | ContentPackage::aBadOrMissingSignatureIsRefused, pathTraversalAndCodeAreRefused | RSA, not the spec's signature scheme; no install flow |
| `.midprofile` schema | 10 | yes | MidiProfiles | MIDI OUT | MidiExportTests | |
| Common meta rules | 12 | partial | parts/guitars/tunes | n/a | - | Presets/setlists lack ISO dates and versions |
| Atomic save: temp, fsync, rename | 13 | partial | PresetManager::writeToFile; TuneFile::save (`replaceFileIn`); Part/PartLibrary `.tmp` + move | n/a | TuneBuilder::saveIsAtomicAndKeepsADatedBackup | Setlist, pattern, kit, loop, prefs use `replaceWithText` |
| Previous version → backup (preset, guitar, tune) | 13 | partial | PresetManager::backupBeforeOverwrite → `Presets/Backup/<date>`; TuneFile `.backup` | n/a | Presets::savingBacksUpTheVersionItReplaces; Presets::backupsGoToThePresetsRootBackupFolder | No guitar backup |
| Backups > 30 days pruned on startup | 13 | partial | PresetManager::pruneOldBackups (ctor) | n/a | - | Tune `.backup` never pruned; the migration-backup folders under each preset folder are not swept |
| Load path: validate, migrate, refuse with banner | 14 | partial | PresetManager::loadPreset; PartLibrary report; TuneFile | banner "preset-load", "migrated" | Presets::everyByteOfAFactoryPresetFlippedIsRefusedOrLoads | Setlist/pattern loaders skip magic and fail silently (`processor.loadSetlist` result ignored in LiveStrip) |
| Missing reference → fallback + named banner | 14 | yes | takeGuitarNotices, IrSlot lastError | banners "missing-part", "ir-missing" | Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce | |
| Corrupt file never overwritten; refusal prompts | 14 | yes | loadPreset | banner | ErrorLog::arefusedPresetLoadIsRecordedAndChangesNothing | |
| Version discipline | 15 | n/a | process | n/a | - | |
| Test: factory files round-trip byte-identical | 16 | partial | - | n/a | Presets::everyFactoryPresetRoundTripsToTheUlp; Workshop spec equality | Presets yes; guitars/parts by value, not bytes |
| Test: 10 000 mutated bytes | 16 | partial | - | n/a | Presets::mutatedPresetsNeverCrashTheLoader; Presets::everyByteOfAFactoryPresetFlippedIsRefusedOrLoads | Presets only |
| Test: migrations across 200 fixtures | 16 | no | - | n/a | GuitarMigration (3 cases) | |
| Test: missing guitar → banner + fallback | 16 | yes | - | n/a | WorkshopPresets::aMissingGuitarFileFallsBackToItsType | |
| Test: kill mid-save 100× | 16 | no | - | n/a | - | |
| Test: 60-day backup pruning | 16 | no | - | n/a | - | |

### factory-content.md

Summary: 34 requirements. **yes 11, partial 16, no 7.** Since the previous audit: the bank is now exactly 36 presets, but the names are this project's own set, not the spec's 36. The first-run preset is "Single-Cut Crunch" (Onboarding::kFirstRunPreset), which stands in for "Modern Overdrive". Six example tunes ship at `Resources/Tunes/Examples`, and twelve MIDI clips at `Resources/Examples/*.mid` (not `Examples/MIDI/`). Ten example setlists are generated at first run. There are still no backing tracks and no pattern/kit files. "Fuzz Face Lead" remains.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| No trademarks; legal review | 0.1 | partial | TrademarkTests; Tools/trademark_scan.py | n/a | Trademarks::* | "Fuzz Face Lead" still in FactoryPresets.cpp:265 and TuneExamples.cpp:530. The spec's own "Fuzz Face Wall" has the same problem. No sign-off recorded |
| Tonal spread (36 covering the map) | 0.2 | partial | FactoryPresets.cpp (36 recipes incl. Init) | Preset browser | Presets::everyFactoryPresetLoadsAndPlays | Count now matches; spread is ours (heavy on utility: Init, Dry Instrument, Physics Showcase, Microtonal Just). Techniques branch adds more (TechniquePresets.cpp) |
| Difficulty ladder | 0.3 | no | - | - | - | |
| ≥2 presets per genre | 0.4 | no | - | - | - | Still missing reggae, latin, indie, punk guitar; bass funk/reggae/punk/jazz |
| Every factory guitar playable at every preset | 0.5 | partial | guitar-block fallback by type | n/a | - | No cross-product test |
| Every factory tune loops, sounds finished | 0.6 | partial | Resources/Tunes/Examples (6) | TUNE → Examples | SampleContent::theSixExampleTunesAreValidAndShipAsBuilt; anExampleTuneOpensFromTheTuneTabAndPlays | Generated (TuneExamples::buildExampleTunes); "finished" is not judged |
| No copyrighted third-party content | 0.7 | yes | IRs, tunes, clips all generated | n/a | Legal::thirdPartyLicencesNameEveryBundledDependency | |
| ≤ 200 MB compressed | 0.8 | yes | Resources 32 MB | n/a | manual du | |
| 36 named presets (Fresh Strings Clean … Jazz Walking Bass) | 1 | partial | FactoryPresets.cpp (36, different names) | Browser | - | Only "Modern Metal Chug" and "Flamenco Rasgueado" match by name. Close equivalents: "T-Style Country Twang", "Jazz Hollowbody", "Blues Slide", "Surf Reverb", "8-String Djent", "12-String Jangle" |
| "Modern Overdrive" default first-run preset | 1 | partial | Onboarding::kFirstRunPreset = "Single-Cut Crunch"; PluginEditorOnboarding::applyFirstRunPreset | first launch | Onboarding tests (line 247, 583) | Deliberate stand-in after the trademark sweep; name differs from spec |
| 15 named factory guitars | 2 | yes | Resources/Guitars (27) | Workshop / guitar combo | Workshop::everyFactoryGuitarLoadsAndRoundTrips | "Selmer-Style" ships as "Gypsy Jazz" |
| Designed finish, distinct parts | 2 | yes | `.luthierguitar` finish blocks | Workshop | - | |
| ~90 parts | 3 | yes | Resources/Parts (148) | Workshop | Workshop::theFactoryLibraryIsThere | |
| Named parts per list | 3 | partial | generate_factory_parts.py | Workshop | - | Not diffed name by name |
| 6 factory tunes (named) | 4 | partial | Resources/Tunes/Examples/01…06 | TUNE tab (TunePanel loadExamples) | SampleContent::theSixExampleTunesAreValidAndShipAsBuilt | Names differ: "Late Night Standard", "Morning Folk Sketch", "Iron Riff", "Bottleneck Blues", "Slap Street" vs Late Night in a Minor Key / Highway Sketch / Chug Test / Delta Slide / Funk Slap Groove. Themes match 1:1 |
| ~28 `.luthierpattern` files incl. bass | 5 | partial | Patterns.cpp (compiled in) | RHYTHM | RhythmPatterns::factoryPatternsAreWellFormed | Not files; bass patterns (Walking, Tumbao …) absent |
| ~28 `.luthierkit` files with pick/setup/noise style | 6 | partial | GenreKit.cpp (compiled in; saveTo exists) | RHYTHM | GenreKits::factoryKitsAreWellFormed; SampleContent::everyGenreKitSuggestsATempoAFeelAndAPalette | No shipped files; no pick/setup/noise styles |
| 10 example setlists | 7 | partial | TuneExamples::buildExampleSetlists / installExampleSetlists (at first run) | LiveStrip setlist menu | SampleContent::theTenExampleSetlistsInstallOnceOverTheFactoryBank | Different names ("Example - Rock Night"…) and entry counts (3-5, not 5-12). Absolute preset paths. Not shipped files. Installed only on the first-run path, so existing users never get them |
| 6 backing tracks | 8 | no | - | - | - | Needs produced audio |
| 12 MIDI clips in `Resources/Examples/MIDI/` | 9 | partial | Resources/Examples/01…12.mid; TuneExamples::getMidiClipDirectory | File → Import MIDI (browse) | SampleContent::theTwelveMidiClipsShipAndPlay | Wrong subfolder (no `MIDI/`) |
| 720 IRs | 10 | yes | Resources/BodyIRs, CabIRs | TONE MATCH | - | |
| Post-release `.luthiercontent` packs | 11 | no | ContentPackage class only | no | - | Deferred |
| Names in locale catalog | 12 | partial | Localisation.cpp | n/a | - | Preset/guitar/tune names not catalogued |
| Preset names are noun phrases | 12 | partial | - | - | - | "Fretless Mwah", "Octave Fuzz Stoner" borderline |
| Guitar names `[Style] [Family]` | 12 | yes | Resources/Guitars | - | - | |
| Part names state physical fact | 12 | yes | Resources/Parts | - | - | |
| Tune names < 32 chars | 12 | yes | Examples + Templates | - | - | |
| Test: every preset in every host | 13 | partial | - | - | Presets::everyFactoryPresetLoadsAndPlays | In-process only |
| Test: guitar spectrum-delta fixtures ±0.2 dB | 13 | no | Workshop/SpectrumDelta | - | - | No fixtures |
| Test: every factory tune plays end to end | 13 | yes | - | - | SampleContent::anExampleTuneOpensFromTheTuneTabAndPlays; theSixExampleTunesAreValidAndShipAsBuilt | Dropout check is in-process |
| Test: every factory setlist resolves | 13 | yes | - | - | SampleContent::theTenExampleSetlistsInstallOnceOverTheFactoryBank | Checks the first entry's file exists |
| Test: backing tracks stream at 48 kHz | 13 | no | - | - | - | |
| Legal sign-off per entry | 13 | no | - | - | - | |
| Content-size gate ≤ 200 MB | 13 | partial | - | - | manual | No automated gate |

### error-recovery.md

Summary: 82 requirements. **yes 18, partial 35, no 28, n/a 1.** Since the previous audit:
- The migration backup and its "migrated" info banner are merged.
- The CPU relief ladder with a "CPU limit" banner (CpuRelief, CpuReliefUi) is merged, with an opt-out in Options → Diagnostics.
- UpdateDownloader is merged and reachable from Options.
- The first-run code is merged.

Unchanged: newer-schema presets still load (logged only). There is still no crash-dump writer and no corrupt-config rename. MIDI Learn has no timeout, there is no MIDI flood throttle, and there is no standalone device polling or demo mode. Banners still show one at a time with two levels. `ErrorLog::pruneOldLogs`/`setVerbose` are never called. `ErrorLog::write` still has only 17 callers, all in PresetManager and PluginProcessor.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Never crash on bad input | 0.1 | partial | loaders | n/a | Presets::everyByteOfAFactoryPresetFlippedIsRefusedOrLoads; Parameters::fuzzAcrossHundredThousandStates | Presets and state fuzzed; setlist/tune/guitar not byte-fuzzed |
| Never silently degrade | 0.2 | partial | NotificationCentre | banner strip | Editor test | Setlist load failure, A/B empty, NaN guard silent |
| Never destroy user work | 0.3 | partial | preset + tune backups; migration backup | n/a | Presets::savingBacksUpTheVersionItReplaces; ModelGapsUi::aMigratedPresetKeepsItsOriginal | No guitar backup; prefs overwritten after a parse failure |
| Prefer partial success | 0.4 | yes | IR/part fallbacks | banners | Workshop::aMissingPartFallsBackAndSaysSo | |
| Every failure logged to errors-yyyymm.log | 0.5 | partial | Support/ErrorLog | n/a | ErrorLog::failuresAreLoggedAsReadableJsonLines | Only preset + processor paths log |
| Severity → banner / banner+halt / modal | 0.6 | partial | Notification::Level {info, warning} | banner | - | No modal/error path |
| File not found → named banner | 1 | yes | loadPreset | banner "preset-load" | Editor test | |
| Permission denied banner | 1 | partial | "could not be read, or is empty" | banner | - | Not distinguished |
| Not UTF-8 banner | 1 | partial | JSON failure path | banner | - | |
| Not JSON banner + re-save hint | 1 | partial | "is not a Luthier preset" | banner | - | No hint |
| Magic missing/wrong banner | 1 | yes | fromVar BAD_MAGIC | banner | Presets::aFileWithoutTheMagicMarkerIsRefused | |
| Newer schema → refuse | 1 | no | fromVar logs NEWER_SCHEMA (info) and loads | no | - | Deliberate deviation (C-20); no banner either |
| Older schema with no migration refused | 1 | partial | schema ≤ 0 refused | banner (generic) | - | |
| Migration: backup + 5 s info banner | 1 | yes | backupMigratedOriginal; PluginEditor "migrated" banner (noteMigration / migrationGeneration) | banner | Editor::aMigratedPresetRaisesOneInfoBanner; ModelGapsUi::aMigratedPresetKeepsItsOriginal | Banner shows once per editor session (`migrationBannerShown`); backup path differs (see file-formats) |
| Migration fails partway → original preserved | 1 | partial | loader never writes the source file | - | - | No explicit test |
| Referenced file missing → info banner | 1 | yes | IrSlot, guitar parts | "ir-missing" (Tone Match action), "missing-part" | Editor test; WorkshopPresets test | |
| Referenced file corrupt → same | 1 | partial | IR decode failure falls back | banner | IrReloadTests | Corrupt IR not specifically tested |
| Cyclic reference refused | 1 | no | - | - | - | |
| Save: folder not writable | 2 | partial | writeToFile SAVE_UNWRITABLE; Save As overlay status "Could not save. Check that Documents/Luthier/Presets/User is writable." | Save As overlay (inline) | - | Header "Save" falls back to Save As silently; no banner |
| Save: disk full | 2 | partial | same | same | - | |
| Save: rename failed → temp deleted, message | 2 | partial | TemporaryFile / TuneFile error text | inline | - | |
| Concurrent save → later wins + warning | 2 | no | - | - | - | |
| Session recorder unique name | 2 | yes | timestamped names | n/a | - | |
| SR change → reprepare + banner | 3 | yes | claimSampleRateChange | "sample-rate" | Engine::sampleRateChangesAreSurvived | |
| Block-size change | 3 | yes | prepareToPlay | n/a | Engine::blockSizeChangesAreSurvived; Stress::busLayoutChangesMidPlay | |
| Bus layout not advertised → false | 3 | partial | isBusesLayoutSupported | n/a | PluginBuses::* | Accepts mono main |
| NaN/denormal guard + log + counter | 3 | partial | LuthierEngine sanitise | no | Engine::fastSlidesProduceNoNansOrDenormals | No log/counter |
| CPU overrun relief ladder + "CPU limit reached" banner | 3 | yes | Support/CpuRelief (7 steps); PluginEditor CPU-limit banner (CpuReliefUi::bannerMessage) | banner; Options → Diagnostics "Drop strings under CPU overload" | CpuReliefTests; CpuReliefUiTests | Merged |
| Sustained overrun → offer to disable heaviest module | 3 | partial | ladder step 7 drops strings | banner | CpuReliefUiTests | Drops strings instead of offering a module |
| Host underrun → log + counter | 3 | no | - | - | - | |
| Convolution failure → bypass + banner | 3 | partial | procedural cab fallback | no banner | Cabinet::procedualFallbackRemovesTheFizz | |
| Malformed MIDI → log, drop | 4 | partial | MidiInterpreter drops | n/a | Stress::midiStormDuringLoadsRecallsAndGuitarLoads | Not logged |
| CC out of range → clamp | 4 | yes | 7-bit | n/a | - | |
| Unknown SysEx ignored | 4 | yes | MidiInterpreter | n/a | - | |
| Corrupt Luthier SysEx → log, drop | 4 | partial | - | n/a | - | Not logged |
| MIDI flood > 5000/s → throttle + banner | 4 | no | - | - | Stress::midiStormDuringLoadsRecallsAndGuitarLoads (no crash only) | |
| MIDI Learn 30 s timeout | 4 | no | MidiLearn (Timer only polls) | no | Stress::midiLearnArmDisarmHundredTimes | |
| Part swap under CPU → queue | 5 | partial | WorkshopBench swap parking | Workshop | WorkshopSwap::aPartSwapDuringANoteIsClickFree | |
| Incompatible part → warning | 5 | partial | advisory (C-21) | Workshop card | Workshop::incompatiblePartsFitWithAWarning | |
| Family change during playback → banner | 5 | partial | undo-side warning only (UndoHistory::kFamilySwitchUndoWarning) | banner on undo | UndoCoverage::undoingAFamilySwitchWarns | No banner at the switch itself |
| Shadow audition timeout 200 ms | 5 | no | - | - | - | |
| Invalid coefficients → refuse swap | 5 | no | - | - | - | |
| Invalid `.luthierguitar` save refused with reason | 5 | partial | "save-guitar" failed banner | banner | - | Generic reason |
| Malformed chord text → red token + status | 6 | partial | TuneHarmony named errors | TUNE text | TuneBuilder::malformedShorthandIsRefusedWithANamedError | No red token highlight |
| Melody generation empty → banner | 6 | no | - | - | - | |
| Sung capture low confidence → banner | 6 | no | - | - | - | |
| Removing last section refused | 6 | no | Tune::removeSection has no last-section guard | - | - | |
| Loop lookahead truncated indicator | 6 | no | - | - | - | |
| Snapshot recall during preset load queued | 7 | partial | message thread serial | n/a | Stress::midiStormDuringLoadsRecallsAndGuitarLoads | |
| Snapshot recall while looper records | 7 | no | - | - | - | |
| Preset load mid-tune → section boundary + banner | 7 | no | - | - | - | |
| Undo with nothing → disabled | 7 | yes | HeaderBar undo state; "Undo history…" disabled when empty | Header | - | |
| A/B empty B → banner | 7 | no | recallSlot silently no-ops | no | - | |
| Update check error → silent + log | 8 | partial | Telemetry | n/a | Telemetry::noNetworkIsSilentRatherThanAnError | Not in ErrorLog |
| Update download interrupted → discarded | 8 | partial | UpdateDownloader (merged) | Options → Updates "Download" | Updates::theDownloadLandsInDownloadsUnderItsOwnName | Interruption path not tested |
| Crash upload failure → banner with path | 8 | no | - | - | - | |
| Licence offline → offline flow | 8 | partial | License::getOfflineChallenge / applyOfflineResponse | **no** (no Options → Licence page) | Telemetry::revalidationCountdownAndOfflineTolerance | Placeholder accepts any 16-char string |
| Revalidation failed in grace → countdown banner | 8 | yes | PluginEditor "licence-grace" | banner | same | |
| Grace expired → modal + demo mute | 8 | no | - | - | - | See licensing.md 8 |
| Host closes instance → save state | 9 | yes | JUCE | n/a | HostState::aSessionSurvivesThePrepareThatFollowsIt | |
| Host crash → restore from host state | 9 | yes | setStateInformation | n/a | Presets::stateRoundTripsExactly | |
| Standalone audio device lost → poll/recover | 9 | no | StandaloneApp.cpp (file-open only) | - | - | |
| Standalone MIDI input lost → same | 9 | no | - | - | - | |
| User config unreadable → rename `.corrupted-<ts>` + banner | 10 | no | UiPreferences::load returns false, keeps defaults | no | - | Next save overwrites the corrupt file |
| User config invalid schema → same | 10 | no | - | - | - | |
| Missing expected folder → create + log | 10 | partial | created lazily; InstallLayout creates tree | n/a | InstallLayout::createsTheTreeAndMarker | Not logged |
| Content-update folder missing → log | 10 | n/a | no content install path | - | - | |
| No writable Documents → prompt | 11 | no | FirstRun (merged) has no location prompt | - | FirstRunTests | |
| OS below minimum → modal + exit | 11 | no | - | - | - | |
| No audio device (Standalone) → prompt | 11 | no | JUCE default | - | - | |
| Crash → minidump | 12 | no | Telemetry only reads `crash-*.dmp` | - | - | No writer |
| Troubleshooting bundle captured | 12 | partial | Diagnostics::writeTroubleshootingReport (manual) | Options → Diagnostics; Debug panel | - | Not at crash |
| Next launch, opted in → upload prompt | 12 | partial | "crash" banner → Privacy page | banner | - | Never fires (no dumps) |
| Next launch, opted out → banner with path | 12 | partial | same banner, no path | banner | - | |
| Log format JSON lines | 13 | yes | ErrorLog::write | n/a | ErrorLog::failuresAreLoggedAsReadableJsonLines | |
| debug/info only when verbose on | 13 | partial | ErrorLog::setVerbose | no | - | Never called; no toggle |
| Monthly rotation | 13 | yes | getLogFile | n/a | - | |
| Old logs pruned 30-day sweep | 13 | no | ErrorLog::pruneOldLogs | n/a | - | Never called |
| ≤ 3 banners visible | 14 | no | NotificationCentre shows 1, queues rest | banner strip | Editor::notificationBannersQueueDismissAndRespectTheirActions | C-22 |
| Priority error > warning > info | 14 | partial | Level {info, warning} | - | - | |
| Auto-dismiss 5 s unless action | 14 | yes | autoDismissMs | - | same | |
| Fixture per failure mode in Tests/Fixtures/Errors | 15 | no | - | - | - | Folder absent |

### state-model.md

Summary: 73 requirements. **yes 31, partial 25, no 17.** Since the previous audit: undo boundaries are merged. There are boundaries for preset load, guitar type, family switch, setlist load and step, and New preset. Snapshot recall is an undo entry. Undo no longer moves the view or the tune. The 10 000-op state fuzz (RobustnessTests) and a 32-instance render test were added. Unchanged: preset files don't carry snapshots, the matrix or mappings. Loads are direct message-thread calls (no command queue). There is no State Inspector, and the section-8 intersections are unbuilt.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Layers nest | 0.1 | partial | guitar block in preset | n/a | StateModel::recallingASnapshotStaysInsideThePreset | Snapshots in host blob only |
| Load atomic per layer | 0.2 | partial | message-thread apply + bridge.applyAllNow | n/a | StateModel::loadingAPresetWhileRenderingProducesNoGarbage | |
| Every "load X while Y" has a policy | 0.3 | partial | see 8.x | - | - | |
| Undo respects state boundaries | 0.4 | yes | UndoHistory boundary; pushUndoBoundary | Header Undo / Ctrl-Z | Undo::aPresetLoadIsABoundary | Merged from visual |
| Structural state via command/result queue | 0.5 | no | direct calls | - | - | |
| uiState not in presets; undo leaves it | 0.6 | yes | UndoState::withSessionLayers keeps "ui","tune","metronome" | n/a | StateModel::loadingAPresetLeavesTheLayersAboveItAlone; Undo::doesNotMoveTheViewOrTheTune | Fixed |
| User-global settings layer | 1 | yes | UiPreferences, Telemetry | Options | - | |
| Per-instance uiState | 1 | partial | UiState in "ui" (now incl. bench slots, piano roll, drawer open) | n/a | Undo::benchSlotsAndSetlistAreInTheStateButBenchSlotsIgnoreUndo | JSON not ValueTree |
| Session state not saved | 1 | yes | processor members | n/a | - | slotBActive flag saved |
| Setlist layer | 1 | yes | SetlistPlayer; host blob `setlist` {file, data, position} | LIVE, LiveStrip | LiveTests | |
| Tune layer | 1 | yes | TuneSession | TUNE | TuneBuilder tests | |
| Snapshot up to 128 | 1 | yes | SnapshotBank | LIVE | LiveSnapshots::programChangeMapsAcrossAllOneTwentyEight | |
| Guitar / part layers | 1 | yes | PartLibrary, Part | Workshop | Workshop tests | |
| Loop sibling | 1 | partial | Looper | PRACTICE | - | |
| Preset load: parse, reference/fallback + banner | 2 | yes | PresetManager + onGuitarBlockLoaded | banner | WorkshopPresets::aMissingGuitarFileFallsBackToItsType | |
| Resolve IRs and patterns | 2 | partial | - | - | - | Not in preset files |
| Post LoadPresetCommand to audio thread | 2 | no | - | - | - | |
| Apply parameters | 2 | yes | fromVar | - | Presets::stateRoundTripsExactly | |
| Swap GuitarSpec atomically | 2 | partial | bridge fade | - | - | |
| Swap mod matrix, snapshot bank, MIDI mappings | 2 | no | not in preset file | - | - | |
| Apply ranges, clamp | 2 | yes | RangeState::applyTo | - | Ranges::narrowingClampsAndReportsTheCount | |
| Crossfade 5-30 ms | 2 | partial | 5 ms guitar fade (LoadFade) | - | - | |
| Push state boundary | 2 | partial | loadPresetAsUserAction; HeaderBar file load; setlist | Header / browser | Undo::aPresetLoadIsABoundary | Host program change (`setCurrentProgram`) and MIDI PC/CC0 loads push nothing |
| Push missing-ref/clamp notices | 2 | partial | missing-part/ir-missing | banner | - | No clamp banner |
| Never touches upper layers | 2 | yes | - | - | StateModel::loadingAPresetLeavesTheLayersAboveItAlone | |
| Snapshot recall: params | 3 | yes | SnapshotBank | LIVE, LiveStrip | LiveSnapshots | |
| Snapshot recall: modulation diff | 3 | partial | - | - | - | |
| Snapshot recall: 30 ms xfade | 3 | yes | Snapshots.cpp | LIVE | LiveSnapshots::recallCrossfadesContinuousAndStepsDiscrete | |
| Discrete params at midpoint | 3 | yes | Snapshots.cpp | - | same | |
| Delay/reverb tails preserved | 3 | partial | - | - | - | |
| Recall never touches file/ranges/guitar/upper | 3 | yes | - | - | StateModel::recallingASnapshotStaysInsideThePreset | |
| Recall pushes undo entry | 3 | yes | recallSnapshot pushes entry | LIVE | Undo::snapshotSaveAndRecallAreEntries | Merged |
| Tune load: lazy per-section presets | 4 | partial | TuneSession | TUNE | - | |
| Tune bundle extraction | 4 | no | - | - | - | |
| Tune load stops playback | 4 | yes | TuneSession::newTune | TUNE | - | |
| Tune load pushes boundary | 4 | partial | TuneSession own stack | TUNE | - | Not on the plugin stack |
| Setlist load: verify refs, flag unresolved | 5 | no | Setlist::loadFrom | LiveStrip | - | Silent failure |
| Setlist in session state | 5 | yes | host blob `setlist` | - | Undo::benchSlotsAndSetlistAreInTheStateButBenchSlotsIgnoreUndo | |
| Load first entry's preset + snapshot | 5 | partial | applyCurrentSetlistEntry | LiveStrip | - | **Bug persists:** snapshot recalled from the stale bank |
| Guitar load: resolve parts + banner | 6 | yes | PartLibrary report | banner | Workshop::aMissingPartFallsBackAndSaysSo | |
| Guitar load: swap, refill, reprepare | 6 | yes | loadGuitarFrom | - | WorkshopPresets tests; HostState::aHostWritingAGuitarTypeWithItsPartsKeepsTheParts | |
| Guitar load: 30 ms crossfade | 6 | partial | 5 ms | - | - | |
| Preset params on top of new guitar | 6 | yes | - | - | WorkshopPresets::aStateLoadKeepsItsOwnRefinements | |
| Guitar load pushes boundary | 6 | yes | guitarType gesture → boundary "Load guitar X"; family switch boundary | guitar combo, Workshop | UndoCoverage::undoingAFamilySwitchWarns | Workshop "Open guitar file" path not verified |
| Part swap: own entry | 7 | yes | WorkshopBench | Workshop | WorkshopBenchTests | |
| 8.1 load supersedes pending recall | 8.1 | partial | serial | - | Stress::midiStormDuringLoadsRecallsAndGuitarLoads | |
| 8.1 tune: wait for section boundary ≤ 4 s | 8.1 | no | - | - | - | |
| 8.1 setlist "user override" flag | 8.1 | no | - | - | - | |
| 8.1 looper state-boundary event | 8.1 | no | - | - | - | |
| 8.1 MIDI Learn arm persists | 8.1 | yes | - | - | StateModel::loadingAPresetLeavesTheLayersAboveItAlone | |
| 8.1 bench unsaved prompt | 8.1 | no | - | - | - | |
| 8.1 Slide / Live / drawer persist | 8.1 | yes | - | - | same | |
| 8.1 A/B clears + banner | 8.1 | no | - | - | - | Test still asserts the A/B slot is kept |
| 8.1 Freeze clears | 8.1 | no | - | - | - | |
| 8.1 E-Bow clears | 8.1 | no | - | - | - | |
| 8.1 feedback damps 100 ms | 8.1 | no | - | - | - | |
| 8.1 held notes decay through new params | 8.1 | partial | - | - | - | |
| 8.2 recall: held notes/tune continue | 8.2 | partial | - | - | LiveSnapshots::thousandRecallsNeverJumpAParameter | |
| 8.2 recall: setlist/looper/Freeze/A/B/feedback | 8.2 | no | - | - | - | |
| 8.3 tune load intersections | 8.3 | partial | TuneSession | - | - | |
| 8.4 guitar load intersections | 8.4 | partial | swap parking; family-switch undo warning | - | - | No prompt/banner at load |
| 8.5 part swap intersections | 8.5 | yes | WorkshopBench | Workshop | WorkshopSwap::aPartSwapDuringANoteIsClickFree | |
| 8.6 ranges toggle clamps | 8.6 | yes | RangeState | Options → Ranges | Ranges::narrowingClampsAndReportsTheCount; Stress::slideAndAdvancedRangeTogglesMidPlay | |
| 8.7 MIDI Learn arm vs flood/recall/load | 8.7 | partial | - | - | Stress::midiLearnArmDisarmHundredTimes | |
| 8.8 undo while held notes/tune/looper | 8.8 | partial | undo keeps the tune | - | Undo::undoMidPlayProducesNoGarbage | |
| 9 save: preset captures snapshot bank | 9 | no | toVar omits snapshots | - | - | |
| 9 save waits for in-flight swap / live edits | 9 | partial | bench parking; TuneSession | - | - | |
| 10 host state persists everything listed | 10 | yes | getStateInformation | n/a | Presets::stateRoundTripsExactly; HostState::* | |
| 10 not persisted: undo, A/B buffers, … | 10 | yes | - | - | - | |
| 11 multi-instance independence | 11 | partial | per-instance objects | - | Stress::thirtyTwoInstancesRenderInTurn (8 by default, 32 in perf runs) | Renders in turn; doesn't check that state stays isolated. ExpressionCalibrationSet and UiPreferences are global |
| 12 "State Inspector" at 4 Hz | 12 | no | DebugPanel/debug window shows data stream, not the layer tree | no | - | |
| 13 intersection tests | 13 | no | 4 StateModel tests | - | StateModelTests.cpp | |
| 13 fuzz 10 000 random ops | 13 | yes | - | - | StateModel::tenThousandRandomOperationsLeaveNoStuckState (1 000 by default, 10 000 in perf runs) | |

### host-integration.md

Summary: 60 requirements. **yes 32, partial 19, no 8, n/a 1.** Since the previous audit: pluginval at strictness 10 runs in `ci.yml` (scripts/pluginval.sh), and clap-validator runs in build.yml. Undo no longer arms the program-change swallow. A family switch keeps host values (961cd55). Unchanged: **`NEEDS_MIDI_OUTPUT FALSE`**, mono main is accepted, and there are no parameter groups, no localised names and no version tag in the blob. Time signature and record state are unread. `docs/HOST_COMPATIBILITY.md` is missing.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| No blocking on audio thread | 0.2 | partial | - | n/a | AudioThreadSafetyTests; pluginval | updateLatency → setLatencyReport per block |
| Announce capabilities correctly | 0.3 | partial | CMake flags | n/a | - | MIDI out not announced |
| Host transport wins | 0.4 | yes | tapTempo.getEffectiveBpm | n/a | RhythmPatterns::silentWhenStoppedUnlessFreeRunning | |
| No IR blobs in state | 0.6 | yes | IrSlot paths | n/a | ToneMatch tests | |
| VST3 all platforms | 1 | yes | CMake | n/a | - | |
| AU on macOS | 1 | yes | CMake `if(APPLE)` | n/a | - | |
| CLAP / AAX | 1 | n/a | clap-juce-extensions (CLAP built) | n/a | clap-validator in build.yml | |
| Standalone | 1 | yes | CMake | n/a | FileOpen tests | |
| Version + build string | 1 | partial | JucePlugin_VersionString = PROJECT_VERSION | footer "v…" | - | No build string |
| Layouts A-D | 2 | yes | buildBusesProperties | ROUTING | PluginBuses::*; Routing::everyLayoutRendersCleanly | |
| Any subset accepted | 2 | yes | isBusesLayoutSupported | n/a | same | |
| Mono main output rejected | 2 | no | accepts mono (PluginProcessor.cpp:309) | n/a | - | C-26 |
| Optional stereo sidechain | 2 | yes | `.withInput("Sidechain")` | ROUTING | - | Mono accepted too |
| Layout change → prepare | 2 | yes | JUCE | n/a | Stress::busLayoutChangesMidPlay | |
| APVTS single source | 3 | yes | Parameters::createLayout | n/a | Parameters::everyParameterHasAUniqueIdAndSaneDefault | |
| Parameter count stable | 3 | yes | - | n/a | - | Techniques adds ~190 lines of params (count grows once) |
| Grouped by ParameterCategory | 3 | no | flat | n/a | - | |
| Display names translated | 3 | no | English | n/a | - | |
| Range/default/text converters | 3 | yes | floatParam/choiceParam | n/a | Parameters::everyParameterTextRoundTrips; HostState::parameterTextRoundTripsStably | |
| Internal changes notify host | 3.1 | yes | setValueNotifyingHost | n/a | HostState::processingDoesNotMoveParameters | |
| Only last write per block | 3.1 | partial | JUCE | n/a | - | |
| Automation base + modulation | 3.2 | yes | ModMatrix | n/a | ModulationTests | |
| Discrete params integer | 3.3 | yes | choice params | n/a | Modulation::discreteDestinationsStepAtBoundaries | |
| State blob version tag | 4 | no | JSON root, no version | n/a | - | |
| APVTS XML + uiState XML | 4 | partial | JSON equivalent | n/a | Presets::stateRoundTripsExactly | |
| Structural blocks in state | 4 | yes | getStateInformation | n/a | HostState::* | |
| MIDI export profile in state | 4 | partial | routing midiOut | n/a | - | |
| Size < 200 KB / < 2 MB | 4 | partial | - | n/a | - | Never measured |
| setStateInformation via swap | 4 | partial | direct apply | n/a | HostState::anUnpreparedInstanceSavesTheSameStateAsAPreparedOne | |
| Older build keeps unknown sections | 4.1 | partial | preset unknownFields only | n/a | - | Host-level unknown keys dropped |
| Newer build migrates + backs up old blob | 4.2 | partial | preset migrations | n/a | HostState::aHostWritingAGuitarTypeWithItsPartsKeepsTheParts | No blob backup |
| getLatencySamples | 5 | yes | updateLatency | ROUTING | Engine::latencyIsReportedAndPlausible; LatencyTests | |
| Latency change → updateHostDisplay | 5 | yes | setLatencySamples | n/a | - | |
| Per-output latency | 5 | partial | LatencyReport (UI) | ROUTING | - | |
| Read tempo/playing/ppq | 6 | yes | getPlayHead | n/a | RhythmScheduler tests | |
| Read time signature, isRecording | 6 | no | - | n/a | - | |
| Missing playhead → internal | 6 | yes | tapTempo | n/a | - | |
| acceptsMidi | 7 | yes | - | n/a | - | |
| producesMidi, usable in hosts | 7 | partial | producesMidi() true; **CMake NEEDS_MIDI_OUTPUT FALSE** | MIDI OUT panel | Routing::midiOutPassThroughIsSampleExact; MidiOutPanelTests (in-process) | VST3/AU have no MIDI out bus |
| Clock/PC/SysEx accepted | 7 | partial | PC → snapshot, CC0 → preset | n/a | - | No MIDI clock |
| MPE | 7 | yes | MidiInterpreter | n/a | - | |
| Sample-accurate MIDI | 7 | yes | MidiOutRouter | n/a | Routing::midiOutPassThroughIsSampleExact | |
| Instances independent | 8 | yes | - | n/a | Stress::thirtyTwoInstancesRenderInTurn | |
| Ableton: swallow first PC after restore | 9.1 | yes | ignoreNextProgramChange (not after undo) | n/a | StateModel::aProgramChangeRightAfterAStateRestoreDoesNotWipeIt | A/B recall (`recallSlot` → setStateInformation) still arms it |
| Ableton MPE auto-detect | 9.1 | partial | - | n/a | - | |
| Logic: state < 500 KB | 9.2 | partial | refs | n/a | - | |
| Logic: PC mapping mode | 9.2 | no | - | no | - | |
| prepareToPlay idempotent + fast | 9.2 | yes | - | n/a | HostState::aSessionSurvivesThePrepareThatFollowsIt | |
| Other hosts' notes | 9.3-9.7 | partial | - | n/a | - | No host matrix |
| Standalone polling, virtual MIDI-out, min size | 9.9 | partial | JUCE standalone; FileOpenRouter | n/a | FileOpen tests | No polling |
| pluginval strictness 10 every merge | 10 | yes | scripts/pluginval.sh in .github/workflows/ci.yml | n/a | CI | build.yml runs strictness 5 on PRs, 10 nightly |
| Plugin undo not in host undo | 11 | yes | UndoHistory | Header | - | |
| Factory presets via programs | 12 | yes | getNumPrograms | host | - | User presets included, so the numbering shifts |
| Threading contract | 13 | partial | message-thread capture | n/a | - | |
| VST3 units | 14.1 | no | - | n/a | - | |
| AU cocoa view | 14.2 | yes | JUCE | n/a | - | |
| State chunk + typed params | 14.3 | yes | - | n/a | - | |
| Tests: SR/block change | 15 | yes | - | n/a | Engine tests | |
| Tests: 32 instances, format switch, per-host, transport | 15 | partial | - | n/a | Stress::thirtyTwoInstancesRenderInTurn | Format switch, per-host round trip and MIDI I/O still missing |
| `docs/HOST_COMPATIBILITY.md` | 16 | no | - | n/a | - | |

### action-and-undo.md

Summary: 50 requirements. **yes 45, partial 4, no 1.** This spec moved from mostly "no" to mostly "yes". Merged:
- `Support/UndoHistory` (class, target, before/after, timestamp, boundary, 200 ms merge, 200 max)
- boundaries for preset, guitar, family, setlist and New preset
- HeaderBar "Undo history…" callout with search (UndoHistoryPanel)
- Ctrl-Y (`redoAlt`) and Ctrl-Alt-Z (`undoAcrossBoundary`) with a banner
- the Options → Diagnostics "Show undo depth" footer
- the family-switch undo warning
- entries for mod-matrix, snapshot, MIDI Learn, pedal move, practice, setlist, IR, routing, rhythm, detune and character edits
- 32 tests (UndoTests 20, UndoCoverageTests 12)

Remaining: the tune is on its own stack, and host/MIDI program changes push no boundary.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| One user change = one entry | 0.1 | yes | parameterGestureChanged → UndoHistory::push | Header Undo | Undo::aGestureIsOneEntry | |
| Boundaries; cross with a separate command | 0.2 | yes | UndoHistory boundary; undoAcrossBoundary | Ctrl-Alt-Z (spec says Shift-Ctrl-Z in 0.2 and Ctrl-Alt-Z in 9) | Undo::aPresetLoadIsABoundary | |
| 200 ms grouping | 0.3 | yes | UndoHistory::kGroupWindowMs | - | Undo::gesturesGroupWithin200ms | |
| Never undoable: live audio/MIDI, recorder, banners | 0.4 | yes | gesture-only | - | Undo::writesWithoutAGestureMakeNoEntries | |
| Per-instance stack | 0.5 | yes | processor member | - | - | |
| Not persisted | 0.6 | yes | - | - | - | |
| Entry fields | 1 | yes | UndoHistory::Entry {class, target, subject, from/to, timestamp, boundary, state} | - | UndoTests | |
| Undo History dropdown | 1/9 | yes | HeaderBar::showUndoHistory (UndoHistoryPanel), File → "Undo history…" | Header File menu | Undo::theHistoryListsNewestFirstAndUndoesToAPoint | In the File menu, not a "View" menu |
| Max 200, oldest dropped | 2 | yes | kMaxEntries | - | Undo::overflowDropsTheOldest (250) | |
| Redo cleared on new action | 2 | yes | - | - | Undo::randomWalkUndoesBackToTheStart | |
| 3.1 "Change X from A to B" | 3.1 | yes | entry.description | history list | UndoTests | |
| 3.1 pause > 200 ms splits a drag | 3.1 | yes | merge window | - | Undo::gesturesGroupWithin200ms | |
| 3.1 no entries from modulation/automation/CC | 3.1 | yes | - | - | Undo::writesWithoutAGestureMakeNoEntries | |
| 3.2 discrete rapid scroll → one entry | 3.2 | yes | same-target merge | - | gesturesGroupWithin200ms | |
| 3.3 toggles "Turn on/off X" | 3.3 | yes | - | - | Undo::aToggleSaysTurnOnOrOff | |
| 3.4 part swap entry | 3.4 | yes | WorkshopBench | Workshop | WorkshopBenchTests | Wording "Fitted X (was Y)" |
| 3.4 family switch = boundary | 3.4 | yes | pushUndoBoundary (kFamilySwitchClass) | Workshop / guitar combo | UndoCoverage::undoingAFamilySwitchWarns | |
| 3.5 illustration drag "Move handle X mm → Y mm" | 3.5 | partial | headstock detune grouped per string | Guitar illustration | UndoCoverage::headstockDetuneIsOneGroupedEntryPerString | No mm wording for bench handles |
| 3.6 mod-matrix entries | 3.6 | yes | ModMatrixPanel | MOD panel | Undo::rightClickModulationIsUndoable; UndoCoverage::modPanelRouteEditsAreEntries | |
| 3.7 snapshot entries | 3.7 | yes | LivePanel | LIVE | Undo::snapshotSaveAndRecallAreEntries; UndoCoverage::snapshotRenameColourAndDeleteAreEntries | |
| 3.8 preset load = named boundary | 3.8 | partial | loadPresetAsUserAction; HeaderBar file open; New preset | Header / browser | Undo::aPresetLoadIsOneNamedEntry | `setCurrentProgram` (host PC) and MIDI PC/CC0 loads push nothing |
| 3.8 save/rename not on stack | 3.8 | yes | - | - | - | |
| 3.9 tune entries + tune-load boundary | 3.9 | partial | TuneSession own stack | TUNE | TuneBuilder tests; Undo::doesNotMoveTheViewOrTheTune | Separate stack; plugin undo no longer clobbers the tune |
| 3.10 setlist load/step/edit | 3.10 | yes | pushUndoBoundary on load/step; LivePanel edits | LIVE / LiveStrip | UndoCoverage::setlistEditsAreEntries | |
| 3.11 ranges toggle | 3.11 | yes | changeRanges | Options → Ranges | Undo::clampToStockIsOneEntry | |
| 3.12 MIDI Learn entries | 3.12 | yes | - | right-click learn | Undo::aMidiLearnIsOneEntry | |
| 3.13 pedal param/bypass | 3.13 | yes | slot attachments | FX racks | - | |
| 3.13 pedal add/remove/move | 3.13 | yes | PedalRack | FX racks | Undo::movingAPedalIsOneEntry | |
| 3.14 practice entries; layer delete restorable | 3.14 | yes | - | PRACTICE | Undo::aClearedLoopLayerComesBack; UndoCoverage::practiceScaleAndLooperLayerAreEntries | |
| 3.15 character entries | 3.15 | yes | - | CHARACTER | UndoCoverage::characterEditsAreGroupedEntries | |
| 3.16 technique control entries | 3.16 | no | - | - | - | in progress on techniques (TechniquesUiTests) |
| 3.17 UI state not undoable | 3.17 | yes | withSessionLayers keeps "ui" | - | Undo::doesNotMoveTheViewOrTheTune | |
| 4 merge rules | 4 | yes | UndoHistory::push | - | gesturesGroupWithin200ms | |
| 5 Ctrl-Z stops at boundary | 5 | yes | stepBack(crossBoundary=false); "undo-boundary" banner | keyboard | Undo::aPresetLoadIsABoundary | |
| 5 separator + "Preset: [name]" subtitle | 5 | yes | buildUndoHistoryMenu / panel | history | UndoCoverage::theHistoryListSearchesAndUndoesToAChosenRow | |
| 6 multi-target atomic | 6 | yes | whole-state entry | - | Undo::resetEverythingIsOneEntry | |
| 7 skip list | 7 | yes | - | - | - | |
| 8 family-switch undo warns | 8 | yes | kFamilySwitchUndoWarning | banner | UndoCoverage::undoingAFamilySwitchWarns | |
| 8 undo after save leaves file | 8 | yes | - | - | - | |
| 9 Ctrl/Cmd-Z | 9 | yes | "undo" | keyboard + Header | Accessibility::shortcutDefaultsMatchTheCanonicalTable | |
| 9 Ctrl-Shift-Z | 9 | yes | "redo" | keyboard | same | |
| 9 Ctrl-Y | 9 | yes | "redoAlt" | keyboard | same | |
| 9 Ctrl-Alt-Z + confirm banner | 9 | yes | "undoAcrossBoundary" | keyboard | - | |
| 9 click-to-point + search | 9 | yes | UndoHistoryPanel | File → Undo history… | UndoCoverage::theHistoryListSearchesAndUndoesToAChosenRow | |
| 11 host automation not undoable | 11 | yes | gesture-only | - | writesWithoutAGestureMakeNoEntries | |
| 12 "Show Undo Depth" footer | 12 | yes | OptionsPages undoDepthToggle; PluginEditor paint (describeDepth) | Options → Diagnostics | UndoCoverage::diagnosticsShowsUndoDepth | |
| 13 per-class fixture tests | 13 | partial | - | - | UndoTests (20) + UndoCoverage (12) | Not one fixture file per class |
| 13 overflow 250 → 50 dropped | 13 | yes | - | - | Undo::overflowDropsTheOldest | |
| 13 199/201 ms test | 13 | yes | - | - | Undo::gesturesGroupWithin200ms | |
| 13 undo mid-play no dropouts | 13 | yes | - | - | Undo::undoMidPlayProducesNoGarbage | |

### licensing.md

Summary: 40 requirements. **yes 4, partial 8, no 28.** The spec itself says nothing is implemented. The only code is the placeholder `License` in `Source/Updates/Telemetry.*`. It does an unsigned HTTP activate, stores a SHA-256 key hash, and accepts any string of 16 or more characters as an offline response. It has a 30 + 14-day grace. **There is no Options → Licence page**, so `License::activate`, `deactivate` and the offline flow are unreachable from the GUI. Only the grace banner and the HELP-tab state line are visible.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Never interrupt audio / gig-safe offline | 0.1 | partial | License never touches audio | n/a | Telemetry::revalidationCountdownAndOfflineTolerance | Trivially true: nothing is enforced |
| Activation by one key paste or sign-in | 0.2 | no | License::activate exists | **no** | - | No UI calls it |
| Copied licence file useless elsewhere (machine binding) | 0.3/4 | no | - | - | - | |
| Keygen impossible (asymmetric signatures) | 3.1 | no | - | - | - | No crypto; any HTTP 2xx activates |
| Ed25519 via Monocypher in ThirdParty | 3.1 (L-2, L-3) | no | ThirdParty has JUCE and clap-juce-extensions only | - | - | |
| Only public keys embedded | 3.1 | no | - | - | - | |
| Signed JSON licence document (v, kid, edition, seats, fp, dates, majorVersions, features, trial) | 3.2 | no | License::toVar stores keyHash + dates | - | - | |
| File `{licence, sig}`; verify bytes before parse | 3.2 | no | - | - | - | |
| Key rotation by `kid` (current + next key embedded) | 3.3 | no | - | - | - | |
| Licence key format (5×5 base-32 + checksum) | 3.4 | no | - | - | - | Any non-empty string accepted |
| Five-component salted fingerprint | 4.1 | no | - | - | - | Placeholder does not even use getUniqueDeviceID |
| Tolerant 3-of-5 match + silent refresh | 4.2 | no | - | - | - | |
| Licence store per-user path | 4.3 | yes | License::getLicenseFile (userApplicationData/Luthier) | n/a | - | |
| Machine-wide store readable | 4.3 | no | - | - | - | |
| Store written atomically | 4.3 | no | License::save uses replaceWithText | - | - | |
| 3 seats per licence | 5 | no | server-side | - | - | |
| Activate request (key, fp, product, version, os) | 5/6.1 | partial | activate posts `{"key": …}` only | no | - | |
| Deactivate: one click, delete file, queue if offline | 5 | partial | License::deactivate deletes the file | no | - | No server call, no queue, no UI |
| Self-service portal | 5 | no | server/web | - | - | |
| Revocation at revalidation | 5/6.3 | no | - | - | - | |
| Server endpoints `/v1/activate`, `/revalidate`, `/deactivate`, `/offline/response`, `/keys` | 6.1 | no | - | - | - | |
| Calls go through `Transport`, logged in privacy log, worker thread | 6.1 | partial | License::setTransport; transport->post | no | - | Path exists; no caller |
| Signed error responses | 6.1 | no | - | - | - | |
| Revalidation every 30 days, background, on load | 6.3 | partial | License::revalidate; kRevalidationDays = 30 | no | Telemetry::revalidationCountdownAndOfflineTolerance | `revalidate` is never called outside tests; `license.load()` in the processor ctor only |
| Offline challenge/response (base-32, QR, file) | 6.4 | partial | getOfflineChallenge / applyOfflineResponse | no | - | Accepts any 16+ char response (the spec calls this out) |
| 30-day Pro trial licence | 6.5 | no | - | - | - | |
| Several independent verification sites | 7.1 | no | - | - | - | |
| Deferred consequences | 7.2 | no | - | - | - | |
| Binary integrity (codesign / Authenticode / manifest hash) | 7.3 | no | - | - | - | scripts/release_manifest.sh exists for releases, but the plugin never reads it |
| Per-release variation, light obfuscation | 7.4-7.5 | no | - | - | - | |
| Never on the audio thread | 7.6 | yes | nothing licensing-related in processBlock | n/a | - | |
| No anti-debug | 7.7 | yes | - | n/a | - | |
| State machine: activated / revalidation due / grace / unlicensed | 8 | partial | License::State {unlicensed, activated, grace, expired}; updateStateFromDates | HELP tab line; header grace banner | Telemetry::revalidationCountdownAndOfflineTolerance | No "revalidation due" state or 7-day note |
| Grace banner each session | 8 | yes | PluginEditor "licence-grace" | banner | same | |
| Demo mode: 1 s of silence every 60 s, 20 ms fades, header note | 8 | no | - | - | - | |
| Demo mode: exports disabled | 8 | no | - | - | - | |
| Licensed ↔ demo change only at load, editor open or transport stop | 8 | no | - | - | - | |
| Clock-rollback guard (monotonic last-seen) | 8 | no | - | - | - | |
| Privacy: no personal data; privacy dashboard shows licence and offers "Deactivate and delete" | 9 | partial | only a key hash stored | Privacy page lacks licence content/button | - | |
| Tests: signature, fingerprint tolerance, state machine, demo fade, ThreadProbe, privacy log | 11 | no | - | - | Telemetry::revalidationCountdownAndOfflineTolerance (placeholder only) | |

### editions.md

Summary: 34 requirements. **yes 3, partial 1, no 30.** This is a design for a post-feature-freeze fork step (section 9), and none of it exists yet. There is no `LUTHIER_EDITION`, no `Source/Edition.h`, no ProFeatureGuard, no ProLockedPanel, no EditionTests and no `packaging/content/free.txt`. The "yes" rows are the prerequisites the spec says must not be broken: Pro keeps today's IDs, and there is one engine with one parameter layout.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Same engine, same sound in both | 0.1 | yes | - | - | - | Holds trivially today |
| Remove headline features only | 0.2/1 | no | - | - | - | No gating |
| Never gate accessibility/safety/privacy | 0.3 | yes | no gating exists | - | - | |
| Free loads Pro files, bypasses, writes Pro data back | 0.4/5.1 | no | - | - | - | Relies on the preset carrying every block. Today modulation, snapshots and routing aren't in presets (see file-formats), so a Pro→Free→Pro round trip via preset would lose them anyway |
| Compile-time gating (Pro code absent from Free) | 0.5/7.2 | no | - | - | - | |
| Side-by-side install (distinct IDs, bundle, content folder) | 0.6/6 | no | CMake single identity | - | - | |
| One upgrade path (Free files open in Pro) | 0.7 | no | - | - | - | |
| Quiet upsell (badge + one panel, no modals/timers) | 0.8/4 | no | - | - | - | |
| Feature table (79 rows) enforced | 2 | no | - | - | - | |
| Free content set (6 guitars, 16 presets, 8 kits, ~130 IRs, 8 clips) | 3 | no | - | - | - | Depends on the spec's 36 named presets, which don't exist (factory-content 1) |
| `packaging/content/free.txt` read by `ci_build.sh stage` | 3/7.6 | no | - | - | - | |
| Locked marking: muted + lock glyph, "More in Luthier Pro" row | 4.1 | no | - | - | - | |
| Locked tooltip + screen-reader text | 4.1 | no | - | - | - | |
| Upsell side panel (non-modal, Esc, Learn more, no price) | 4.2 | no | - | - | - | |
| Locked Pro presets with 20 s pre-rendered demos | 4.3 | no | - | - | - | No `Resources/Demos`, no render_demos.sh |
| Example tunes read-only playback in Free | 4.3 | no | - | - | - | The tunes exist (factory-content 4) |
| Free onboarding "What Luthier Pro adds" page | 4.4 | no | - | - | - | |
| ≤1 upsell notice per preset load; "Don't show again" persisted | 4.5 | no | - | - | - | |
| ProFeatureGuard: detect used Pro features, effective values, banner | 5.1 | no | - | - | - | |
| `savedBy` written by Free, ignored by Pro | 5.1.5 | no | - | - | - | Pro would keep it via unknownFields today |
| Ranges block carried, not applied, in Free | 5.2 | no | - | - | - | |
| Per-file-type Free behaviour table | 5.3 | no | - | - | - | |
| Pro "Import Luthier Free state" | 5.4 | no | - | - | - | |
| Pro keeps today's codes/IDs (Ltha/Lthr, com.luthieraudio.luthier, CLAP id) | 6 (D-4) | yes | CMakeLists.txt juce_add_plugin / clap_juce_extensions_plugin | n/a | - | PRODUCT_NAME is still "Luthier" (renaming to "Luthier Pro" is Q-1) |
| Free identity (Lthf, luthierfree bundle/CLAP ids, AppId, content folder) | 6 | no | - | - | - | |
| `LUTHIER_EDITION` CMake option + cmake/Editions.cmake + Pro-only source regex | 7.1 | no | - | - | - | |
| `Source/Edition.h` (Feature, Limits, isFreeAmp/Pedal) | 7.2 | no | - | - | - | |
| Identical parameter layout; Free marks Pro params non-automatable " (Pro)" | 7.3 | partial | one Parameters::createLayout | n/a | Parameters::everyParameterHasAUniqueIdAndSaneDefault | Single layout holds; the Free variant doesn't exist |
| Panel factories return ProLockedPanel; lists filtered by limits | 7.4 | no | - | - | - | |
| Neutralisation at parameter push, never in processBlock | 7.5 | no | - | - | - | |
| CI edition matrix; scripts take EDITION; six installers | 7.6 | no | build.yml, ci_build.sh, package_* (single edition) | - | - | Scripts assume `Luthier.vst3` |
| Every spec states its edition; PR adds a row | 8 | no | - | - | - | Specs added since (licensing) have no edition header line |
| Fork step files (Edition.h, ProFeatureGuard, ProLockedPanel, EditionTests, docs/EDITIONS.md, LuthierFree.iss) | 9 | no | - | - | - | |
| Tests: both build/validate, symbol grep, Pro presets in Free, null test, side-by-side, locked soak, identical layout | 10 | no | - | - | - | |

### Top gaps (group H)

Ranked by user impact:

1. **Preset files still drop snapshots, the mod matrix, MIDI Learn, rhythm-engine state, character seed/wear, tone-match IRs and routing** (`PresetManager::toVar`). The same cause breaks setlist snapshot recall (`applyCurrentSetlistEntry` recalls from the stale bank, despite its own comment). The editions plan (Pro→Free→Pro via preset) depends on this too. The techniques branch adds a `techniques` block the right way, through a hook.
2. **No MIDI output in VST3/AU** (`CMakeLists.txt:49 NEEDS_MIDI_OUTPUT FALSE`, while `producesMidi()` returns true). The whole MIDI OUT panel, live MIDI out and Luthier SysEx are unusable in a DAW.
3. **Licensing is a placeholder with no GUI.** Any HTTP 2xx activates, and any 16+ char offline response is accepted. There is no Options → Licence page, `revalidate` is never called and there is no demo mode. That is fine before the split, but it blocks a paid release (licensing.md).
4. **No setlist save.** LivePanel edits a setlist that lives only in the host blob. `Setlist::saveTo` runs only for the first-run examples. Load failures are silent, and the file has no magic check or schema.
5. **Factory preset bank is not the spec's 36.** The count matches, but the names and genre coverage don't (no reggae/latin/indie/punk, no bass funk/reggae/punk/jazz). "Fuzz Face Lead" is still shipped and referenced in example setlists. The techniques branch will push the count past 36.
6. **No crash minidump writer**, so the "crash" banner and upload flow never fire.
7. **Section-8 intersections on preset load are missing:** A/B clear + banner, Freeze/E-Bow clear, feedback damp, tune section-boundary wait, setlist override flag, bench unsaved prompt, looper boundary event.
8. **Newer-schema presets load silently (logged only)**, contrary to error-recovery 1.
9. **Mono main output accepted** (`isBusesLayoutSupported`, PluginProcessor.cpp:309).
10. **No VST3 units / parameter groups, and no localised parameter names.** This will get worse when techniques adds its parameters.
11. **Corrupt `plugin.json` is silently discarded and later overwritten** (UiPreferences::load). There is no `.corrupted-<ts>` backup and no banner.
12. **Banner system:** one visible at a time and only two levels (C-22), where the spec wants three visible with an error priority.
13. **Error log:** `pruneOldLogs` and `setVerbose` are never called, and Options → Diagnostics has no verbose toggle. Only preset and processor paths log, and there are no NaN or underrun counters.
14. **MIDI Learn has no 30 s timeout**, and there is no MIDI flood throttle or banner.
15. **Factory content still missing:** 6 backing tracks, pattern/kit files and bass patterns. MIDI clips sit in the wrong folder. Example setlists are generated only on first run (existing users never get them) and use absolute paths.
16. **Host/MIDI program changes push no undo boundary.** A/B recall still arms `ignoreNextProgramChange`. Program numbers shift when a user saves a preset.
17. **`.luthiercontent` is merged but unreachable.** `ContentPackage::apply` has no caller, no file association and no install UI.
18. **Backups are inconsistent:** none for guitars; migration backups land in `<preset folder>/Backup` and escape the pruner; tune `.backup` is never pruned.
19. **Standalone has no device-loss polling.** No State Inspector. Host time signature and record state are not read. `docs/HOST_COMPATIBILITY.md` is missing.
20. **Missing tests:** kill-mid-save, 200-fixture migration, 60-day prune, state-model intersection tests, spectrum-delta fixtures.

### Unspecified gaps noticed

- **Setlist snapshot comment is wrong.** `applyCurrentSetlistEntry` says "The preset carries its own snapshot bank". It does not, and a reader will trust the comment.
- **Example setlists store absolute paths** (`TuneExamples::buildExampleSetlists` → `info->file.getFullPathName()`). Moving Documents or syncing to another machine breaks all ten.
- **Migration backup location diverges from overwrite backups.** `backupMigratedOriginal` uses the preset's own folder, while `backupBeforeOverwrite` uses `Presets/Backup`. So there are two backup trees, and the pruner only sweeps one of them.
- **No preset "unsaved changes" guard** before loading another preset or closing. The Workshop has one for guitars.
- **No factory-content repair.** There is no "restore factory presets/guitars" except the destructive "Reset factory bank" in the Debug panel.
- **The migration banner shows once per editor session** (`migrationBannerShown`). A second migrated preset in the same session is silent apart from its backup.
- **Host program list includes user presets**, so program numbers are unstable.
- **MP3 backing tracks don't decode on Linux** (JUCE_USE_MP3AUDIOFORMAT unset).
- **`License::save` is non-atomic** (`replaceWithText`), which goes against file-formats 13 and licensing 4.3.

### Small glue candidates

- **CMakeLists.txt:49:** `NEEDS_MIDI_OUTPUT TRUE`.
- **PluginProcessor::isBusesLayoutSupported:** drop `|| main == mono` (line 309).
- **PresetManager::toVar/fromVar:** add `captureExtraBlocks` / `onExtraBlocksLoaded` hooks, like `captureGuitarBlock` and the techniques branch's `captureTechniquesBlock`. They would write `modulation`, `snapshots`, `midiLearn`, `rhythmEngine`, `character`, `toneMatch` and `routing` from the serialisers `getStateInformation` already calls. The keys are already in the `known` list. After that, `applyCurrentSetlistEntry`'s snapshot recall becomes correct.
- **LiveStrip::showSetlistMenu:** add "Save setlist…" (a FileChooser calling `Setlist::saveTo`), and show a banner when `processor.loadSetlist` returns false.
- **Setlist::toVar/loadFrom:** write `magic`/`schema`, check them on load, and store preset paths relative to `Presets/`.
- **PresetManager constructor:** call `ErrorLog::pruneOldLogs()` next to `pruneOldBackups()`. Make `backupMigratedOriginal` use `backupFolderFor(original)`.
- **Options → Diagnostics:** add a "Verbose log" toggle that calls `ErrorLog::setVerbose` (next to `undoDepthToggle`).
- **UiPreferences::load:** on a parse failure, rename to `.corrupted-<timestamp>` and post a "Preferences reset" banner.
- **MidiLearn:** start a 30 s deadline on arm, checked in its existing timer, that disarms and posts a banner.
- **setCurrentProgram / MIDI PC path:** call `pushUndoBoundary ("Load preset …")` before `presets.loadPreset`. Make `recallSlot` bypass the `ignoreNextProgramChange` store, as undo already does via `restoringForUndo`.
- **PluginEditor::pollForNotifications:** post an "A/B: slot B is empty" banner, and on preset load clear `slotB` and post "A/B cleared by preset load" (the StateModel test expectation needs flipping).
- **FactoryPresets.cpp:265 + TuneExamples.cpp:530:** rename "Fuzz Face Lead", with a legacy-name mapping.
- **Resources/Examples:** move the clips to `Resources/Examples/MIDI/` (update `TuneExamples::getMidiClipDirectory`).
- **PluginEditorOnboarding:** also call `TuneExamples::installExampleSetlists` on an upgrade launch, not only on first run.
- **OptionsPages:** add a minimal "Licence" page (key field → `License::activate` on a worker, Deactivate, offline challenge/response). That makes the placeholder testable until the licensing.md client replaces it.
- **Parameters::createLayout:** wrap groups in `AudioProcessorParameterGroup` per column-4 tab. The IDs don't change, so saved state is safe. Do it before techniques lands its parameters.

---

## Group I: meta files

Audited on `claude/luthier-audit` @ `961cd55`. That is 166 commits after the previous audit (`51e40a3`), and it includes the merges of realism-a, realism-b, realism-c, model-gaps, tune-help, visual, release and review. Only `origin/claude/luthier-techniques` is unmerged; I read it with `git diff HEAD...origin/claude/luthier-techniques` (80 files, +9588). "in progress on techniques" means the item exists only there.

I only read the code: nothing was built or run. None of the six group-I spec files has changed since `51e40a3` (`git diff --stat` is empty), so there are no new requirements. Rows that were stale before are more stale now. Current counts are taken from the code:
- 528 parameters: `IntegrationTests.cpp:655` expects 450 + 3 + 15 + 29 + 29 + 2.
- 1198 `LUTHIER_TEST` cases.
- 27 factory guitars and 36 factory presets.

Test names use the `Suite::test` form or the test file.

### DECISIONS.md

**Summary: 81 yes, 10 partial, 2 no (93 rows).** Ten rows moved to yes after the merges:
- The WORKSHOP padlock.
- Accent colour.
- Phase-2b engines and range families.
- Block-boundary part swap.
- Rubric bass pattern and `selectNotesForStyle` retired.
- Reset/preset parts (B-05) and render repeatability (B-08).
- The tour.

Still open:
- C-32 (TECHNIQUES tab): in progress on techniques.
- Scrape and slap-trigger UI: in progress on techniques.
- RHYTHM STRUM rows clipped.
- Unconsumed part fields: `coil_turns`, `spring_count`, `plies`, `radius_mm`.
- `.luthierkit` is only half there.
- Doubler migration gate (B-07).
- Rubric unisons listening pass.
- MIDI chuck key range.

The file's own text is stale. It still calls phase 2b "not on disk / blocked" and gives 13 tabs.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Test harness owns its own RangeState; PresetManager takes it by ref | 09-22 | yes | Presets/PresetManager, PhysicalRange:RangeState | n/a | RangeTests | |
| Right-click "Restrict to stock range" only for per-control unlocks | adv-ranges 4 | yes | UI/Widgets.cpp context menu | right-click any ranged control | RangesUiTests | |
| Locked-range notice is a bubble at the control | adv-ranges 6.3 | yes | Widgets.cpp BubbleMessageComponent | at control | RangesUiTests | |
| RANGES master toggle clears per-control unlocks | adv-ranges | yes | UI/OptionsPages RangesPage | Options > RANGES | RangesUiTests | |
| Controls re-attach on a range-generation counter | adv-ranges | yes | PluginEditor seenRangeGeneration / RangeState::getGeneration | n/a | RangesUiTests | |
| Undo stack off-by-one fixed; entries hold before+after | action-undo | yes | PluginProcessor undo stack, UndoHistory | Ctrl+Z / header / File > Undo history | UndoTests, UndoCoverageTests | history list + depth footer added (vwq-undo) |
| No separate "advanced-range clamped" banner | gui-int 15 | yes (by decision) | n/a | n/a | n/a | deliberate |
| Tab-header padlocks on CHARACTER and WORKSHOP | gui-int 21 | yes | AdvancedPanel.cpp:1166-1175 RangeTabButton (CHARACTER: pick/squeak/buzz/slide/strings/environment/body; WORKSHOP: buzz/pick/slide) | col 4 tab headers | RangesUiTests | was partial (WORKSHOP missing) |
| GuitarCircuit MNA solver replaces CableSim | vki 1.1 | yes | DSP/Circuit/GuitarCircuit | Adv col 2 CIRCUIT | CircuitTests | |
| Coil resonance in GuitarCircuit; 50s wiring; active tone corner; bypass neutral | vki 1.4/3.1 | yes | GuitarCircuit | CIRCUIT | CircuitTests | |
| Capacitor params in nF | vki | yes | Parameters.cpp circuit_tone_cap*1e-9 | CIRCUIT | CircuitTests | |
| Pot/cap StandardValueChoice combo beside knob | vki | yes | UI/CircuitPanel | CIRCUIT | CircuitTests | |
| Param count +11 (363 at the time) | vki | yes | Parameters.cpp | n/a | Parameters count test (now 528) | |
| Advanced amp ranges: tone stack 0-1 + shelving | adv-ranges | yes | AmpEngine | amp knobs | CircuitTests / RangeTests | |
| pick_material 12 choices, read every block | pick-noise 2.1 | yes | ParameterBridge, PlayingNoise | CHARACTER PICK | NoiseTests | |
| pick_thickness/angle 0-1 mapped to mm/deg | adv-ranges | yes | Parameters/PlayingNoise | CHARACTER PICK | NoiseTests | |
| Chirp at pluck; squeak replaces glide noise; calibrated at output | pick-noise 4 | yes | DSP/Noise/PlayingNoise, NoiseEngine | CHARACTER | NoiseTests | |
| Engine reset restarts noise sequence (repeatable render) | midi-export 12 | yes | StringEngine::reset (termination brightness + sustain scale now reset) | n/a | Combo::renderDoesNotDependOnWhatWasPlayedBefore | B-08 FIXED (was partial) |
| Winding selector edits string_material until Workshop exists | squeak 9 | partial | UI/NoiseGroups.cpp:245 still attaches ParamIDs::stringMaterial | CHARACTER | NoiseTests | Workshop exists; the selector should edit the strings part |
| Multi-parameter actions are one undo step | action-undo | yes | ScopedUndoAction | n/a | UndoTests | |
| Fret buzz +13 params; fret_action inert | fret-buzz 7 | yes | DSP/Noise/FretBuzz | SETUP group | BuzzTests; GuiReach hides fret_action | |
| Buzz calibration 2.4 mm/unit; relief high nut | fret-buzz 6.1 | yes | FretBuzz | SETUP | BuzzTests | |
| slide_guitar re-pointed; hybrid; vibrato mm; linear bar | slide-guitar 3/7 | yes | DSP/Slide/SlideEngine | CHARACTER SLIDE | SlideTests | |
| Per-note sustain scale | char-wear | yes | LuthierEngine per-note scale | n/a | CharacterTests | REALISM-C SUS-R17: `sustain_scale` still not applied by the bridge (deferred) |
| Phase-2b specs "not on disk, blocked" | 09-23 / C-02 | yes (text stale) | Source/DSP/String/StringAging, Character/EnvironmentModel, DSP/Coupling/BodyCouplingBank, Harmonics.h, LuthierEngineRealismB, DSP/Noise/NoiseFloor, Model/Playing/StabilityModel | CHARACTER groups (StringAging/Environment/BodyCoupling/Harmonics/RightHand/StringInteraction/NoiseFloor/SustainShape/TuningStability) | StringAging/Environment/BodyCoupling/HarmonicRealism/StringInteraction/FingerstyleAttack/NoiseFloor/SustainDecay/TuningStability Tests (135) | merged; DECISIONS text should be updated. B-15 open |
| CharacterPanel sizes itself to content | ui | yes | UI/CharacterPanel | CHARACTER | EditorTests | |
| SLIDE group mode/damping/assist | slide-guitar 4/7 | yes | UI/SlideGroup | CHARACTER SLIDE | SlideTests | |
| Low-action warning offers "Use Slide setup" | slide-guitar 6 | yes | UI/SlideGroup | CHARACTER SLIDE | SlideTests | |
| Factory guitars: 27 files | guitar-workshop 0.6 | yes | Resources/Guitars (27) | Workshop / type menu | WorkshopTests | |
| Part names avoid `:` and `/` | factory-content | yes | Resources/Parts | n/a | WorkshopTests | |
| WorkshopGuitar = GuitarSpec | guitar-workshop 3 | yes | Model/Workshop | n/a | WorkshopTests | |
| Unconsumed part fields | part-acoustics 11 | partial | PartAcoustics.cpp:457-461 now reads tuners ratio/stability/locking and nut friction (REALISM-C) | Workshop inspector (fields shown) | TuningStabilityTests, PartAcousticsTests | still never read: pickup `coil_turns`, pole shape, bridge `spring_count`, pickguard `plies`, fretboard `radius_mm` (grep of all `num/number/text` reads) |
| Termination brightness normalised | part-acoustics | yes | Workshop mapSpec | n/a | PartAcousticsTests | |
| Magnet pull formula | part-acoustics | yes | PartAcoustics lookUpMagnet | n/a | PartAcousticsTests | |
| Guitar-shop theme overrides theme.md | visual-polish 6 | yes | UI/Theme | everywhere | ThemeTests | |
| Model-specific knob caps | visual-polish 3 | yes | UI/Faces | amp/pedal faces | FacesTests | |
| User / follow-the-guitar accent | visual-polish 5 | yes | OptionsPages.cpp:875-926 accentBox ("Follow the guitar" id 100) | Options > APPEARANCE | Accent::everyChoiceMeetsContrastOnEveryPalette, Accent::theWindowTakesTheAccentAndFollowsTheGuitar | was partial |
| Default guitars Player-friendly (1.6/2.0/0.20) | fret-buzz 6.1 | yes | factory guitar files | n/a | BuzzTests | |
| cpuStaysWithinBudget best of three | perf-budget | yes | Tests | n/a | IntegrationTests | |
| State load keeps its own params; only a type pick writes guitar's | state-model | yes | PluginProcessor guitar loader (`guitarLoadKeepsHostWrites && writtenSinceGuitarType`) | n/a | WorkshopPresetTests, BassTechniques::bassDefaultsApplyOnLoad | B-16 family-switch fix at 961cd55 |
| Preset guitar.reference; missing file notice | file-formats | yes | PresetManager, PluginEditor banners | banner | WorkshopPresetTests | |
| Migrated pickup placements become guitar.override | file-formats | yes | PresetManager | n/a | GuitarMigrationTests | |
| Reset loads default type's factory guitar | state-model | yes | resetEverything + presets `partsWin` | Ctrl+Shift+R | Combo::factoryPresetsAndResetUseTheGuitarsOwnParts | B-05 FIXED (was partial) |
| Save As Guitar "Save with parts" | guitar-workshop 6 | yes | PluginEditor saveGuitarAs(bundleParts) | Workshop header / Ctrl+G | WorkshopPresetTests | |
| Save As Part | guitar-workshop | yes | WorkshopPanel | Workshop inspector | WorkshopPanelTests | |
| Workshop swap overwrites SETUP tweaks (known limit) | ui-wiring 6 | partial | known limit | Workshop | none | not changed |
| Guitar change parks audio 5 ms | C-09 | yes | PluginProcessor park | n/a | WorkshopSwap tests | |
| Part swaps that keep string count: off-thread build, block-boundary swap | C-09 / TODO 6e | yes | LuthierEngine::swapPartsAtBlockBoundary; PluginProcessor | Workshop part pick | PartSwap::aSwapKeepingTheStructureIsTakenAtABlockBoundaryWithoutSilence, PartSwap::aStructuralChangeIsNotALiveSwap | merged from model-gaps (was no) |
| Per-string arrays index 0 = high E; partial capo mask | engine 1, amb-res 4.5 | yes | TuningEngine::setCapoStringMask | Workshop capo part (drawn and dragged) | RubricVoicerTests, WorkshopAccessories::theCapoIsDrawnAndDraggedByFrets | GAPS B1 still calls it open (stale) |
| Assistant agents; docs/spec-coverage.md | process | yes | docs/spec-coverage.md, docs/coverage/*.md | n/a | n/a | six coverage docs added |
| C-16 ten tune templates | tune-builder 10 | yes | Resources/Tunes/Templates | TUNE | TuneBuilderTests | plus 6 example tunes |
| C-19 genre kits as `.luthierkit` (magic luthier.kit) | factory-content 6 | partial | Rhythm/GenreKit.cpp GenreKit::loadFrom/saveTo, GenreKitLibrary::save writes Documents/Luthier/Genres/*.json; ContentPackage.cpp:187 allows `.luthierkit` | no (nothing in the UI calls GenreKitLibrary::save) | GenreKitTests | wrong extension (`*.json` scan at GenreKit.cpp:422), no magic, no save UI |
| C-32 14 column-4 tabs, TECHNIQUES before HELP | gui-tech-updates 0.2 | partial | AdvancedPanel.cpp:1143-1157 (13 tabs) | col 4 | Editor::everyWorkspaceTabSelectsAndPaints | in progress on techniques (TechniquesPanel) |
| Illustration lighting/burst/strings/frets | guitar-illus | yes | UI/Guitar/GuitarRenderer | guitar views | GuitarRendererTests, IllustrationRemainderTests | |
| Family templates keep seed/parts; amp defaults per family | guitar-illus 12.2/12.3 | yes | PartLibrary::switchFamily; Workshop/FamilyDefaults.cpp; Model/Guitar/BassDefaults | Workshop Guitar category | WorkshopPanel::theGuitarCategorySwitchesFamily, IllustrationRemainderTests | family-switch undo warning added |
| Guitar pick changes tuning only if needed | amb-res | yes | guitar loader | type menu | WorkshopPresetTests | |
| Pickup heights clamp 0.8-6 mm | guitar-illus 19 | yes | WorkshopBench kMinPickupHeight | bench | WorkshopBenchTests | |
| Bench drag live, commits once | workshop-ui | yes | WorkshopBench | bench | WorkshopBench::aDragIsOneUndoEntryWithItsBeforeAndAfter | |
| IRs reloaded only on file change; cab reset clears tails | perf | yes | BodyEngine/CabinetEngine | n/a | IrReloadTests | |
| Spectrum delta after amp+cab | workshop-ui 6 | yes | Workshop/SpectrumDelta | Workshop spectrum | WorkshopBenchTests | |
| Easy keeps five macros + Humanize + Character | gui-int 3 | yes | UI/EasyPanel | Easy | EasyLayoutTests | |
| Wet/dry before limiter; width M/S | gui-int 3.4 | yes | output_mix, stereo_width | master | EngineTests | |
| Character macro is a parameter | gui-int | yes | macro_character | CHARACTER + Easy | CharacterTests | |
| New params appended at end | host-int | yes | Parameters.cpp | n/a | Parameters count test (528) | |
| Trademarks out of shipped names | factory-content 0.1 | yes | PartLibrary rename maps, migration.json | n/a | TrademarkTests | legal review still a ship gate |
| Reset makes next render repeat exactly | midi-export 12 | yes | Pedal::resetBase, StringEngine::reset | n/a | Combo::renderDoesNotDependOnWhatWasPlayedBefore | B-08 FIXED |
| Poly chord sounds one chord-window after first note | controllers | yes | MidiInterpreter | n/a | Controllers tests | |
| Live noise events as SysEx; 16-entry queue | midi-export | yes | MidiOutRouter | MIDI OUT | MidiExportTests | |
| Export defaults user-global (.midprofile) | midi-export 8 | yes | UI/MidiExportDefaults | Options MIDI, MIDI OUT | MidiOutPanelTests | |
| Aux 8 noise bus after 12 string buses | pick-noise 1.3 | yes | PluginProcessor buses | ROUTING | PluginBusTests | routing-io.md table still lists 7 aux |
| Output buses matched by name | routing-io | yes | PluginProcessor | n/a | PluginBuses tests | |
| Feedback is a physical loop; legacy params inert | amb-res 1 | yes | DSP/Feedback/FeedbackLoop (chambering term now in) | FEEDBACK | FeedbackTests, ModelGaps::chamberingFeedsTheFeedbackCoupling | chambering merged from model-gaps |
| E-Bow self-driven; Silenced damping | amb-res 2 | yes | EBowDriver | SUSTAIN | EBowTests | |
| Doubler = pedal; legacy doubler_on migrates | amb-res 3 | partial | PresetManager.cpp:693-733 | pedal rack | DoublerTests | B-07 OPEN: migration not gated by version |
| migration.json | amb-res 7 | yes | Resources/Guitars/migration.json | n/a | GuitarMigrationTests | |
| Performance capture from string activity incl. technique marks | notation-export 6 | partial | LuthierEngine.cpp:1289 noteOn(... e.technique, harmonicPartial), :1681 bassTechnique, :1699 chordSymbol, :1714 slideBar | NOTATION | Capture::techniquesBendsChordsAndMetersReachTheScore, Capture::bassAndSlideEventsBecomeLuthierEvents | techniques now captured; `PerformanceCapture::mark()` and `bend()` have no engine caller |
| Preset morph | amb-res 5 | yes | Presets/PresetMorph | LIVE | PresetMorphTests | |
| Pedal settings that arrive with a pedal kept | state-model | yes | ParameterBridge | n/a | PresetPedalTests | |
| Engine direct MIDI for TUNE | tune-builder 8 | yes | LuthierEngine::setDirectMidi | TUNE | TuneProcessorTests | |
| Tune keeps its own undo stack (fold later) | action-undo | partial | Tune/TuneSession undoStack; TunePanel.cpp:1394 routes the undo key to it | TUNE | TuneEditingTests | still separate from the processor UndoHistory |
| Click routing monitor/main | practice 0.2 | yes | PluginProcessor clickToMain | Practice > Metronome | PracticeTests | |
| Processor owns routine runner/history/activity | practice 10-12 | yes | Practice/ | PRACTICE | PracticeRoutineTests | |
| Session recorder ring length on PRACTICE tab only | practice 11.2 | yes | PracticePanel | PRACTICE | PracticeSetupPanelTests | |
| Rubric voicer replaces ChordVoicer; bass-pattern setting | amb-res 4 | yes | RubricVoicer::setBassPattern; RhythmEngine.cpp:340; selectNotesForStyle retired (RhythmEngine.h:228) | RHYTHM BASS GRID pattern box (BassGridGroup.cpp:181) | RubricVoicerTests, ModelGapsTests | was partial |
| Rubric unisons listening pass | amb-res 4.2 | no | n/a | n/a | n/a | TODO 2d |
| Help English only; Help > "Take the tour" | accessibility 7 / onboarding 2 | yes | HelpTab.cpp:75-79 tourButton; UI/Onboarding, PluginEditorOnboarding.cpp | col 4 HELP | Onboarding::theTourHasTwelveStopsInTheSpecsOrder, Onboarding::theTourWalksEveryStopAtEverySize | tour merged from tune-help (was no) |
| String scraping engine and keyswitches | string-scraping | partial | DSP/Noise/ScrapeEngine | no (14 scrape_* have no control) | ScrapeTests | UI in progress on techniques |
| Strum dynamics incl. USE KNOB | strum-dynamics | partial | Rhythm/StrumGesture; UI/StrumGroup | RHYTHM STRUM (lower rows clipped) | StrumGestureTests | B-11 layout: AdvancedPanel.cpp:1471 sizes RHYTHM to the viewport height, not RhythmPanel::preferredHeight() |
| strum_speed migrates at 1000/ms | strum-dynamics | yes | PresetManager | n/a | StrumGestureTests | |
| MIDI chuck key range | strum-dynamics | no | none | no | none | |
| Easy Feel scales crossing and evenness | strum-dynamics 6.3 | yes | EasyPanel | Easy Feel | EasyLayoutTests | |
| Phase-2b range families appended | adv-ranges | yes | PhysicalRange.h:42-44 strings/environment/body | Options RANGES; CHARACTER padlock | RangeTests | merged from realism-a (was no) |
| Chuck damps every string | strum-dynamics 6.1 | yes | StringEngine Damping::Chuck | n/a | StrumGestureTests | |

### GAPS.md

**Summary: 32 yes, 2 partial, 3 no (37 rows).** Since the last audit, drag-to-assign, APPEARANCE, DIAGNOSTICS mirror, FILE LOCATIONS, bass UI, `W` and Ctrl+T all landed.

Still open:
- Per-string detune has no parameter.
- Expression calibration is not on the LIVE tab.
- Space still auditions.
- TECHNIQUES tab and scrape/slap-trigger UI: in progress on techniques.
- The strum rows are clipped.

The file text is unchanged, so almost every "absent" row in it is now wrong. Retire or rewrite it.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Eleven realism specs written | B0 | yes | spec/*.md | n/a | n/a | and all nine phase-2b engines now merged |
| GuitarCircuit; CIRCUIT panel | A1 | yes | DSP/Circuit, UI/CircuitPanel | Adv col 2 | CircuitTests | GAPS text stale |
| Workshop parts model + bench + tab | blocked | yes | Model/Workshop, Workshop/, UI/WorkshopPanel | col 4 WORKSHOP | WorkshopTests, WorkshopRemainderTests | nut slot, pick and capo drags now done (WorkshopNut/WorkshopAccessories tests) |
| SlideEngine / Slide Mode | blocked | yes | DSP/Slide | CHARACTER SLIDE, `S` | SlideTests | slide-technique controls in progress on techniques |
| NoiseEngine, noise strip, Aux 8 | blocked | yes | DSP/Noise | CHARACTER, ROUTING | NoiseTests, BuzzTests, NoiseFloorTests | |
| StrumDynamics / RHYTHM STRUM | blocked | partial | Rhythm/StrumGesture, UI/StrumGroup | RHYTHM (lower 6 rows clipped) | StrumGestureTests | B-11 |
| Advanced ranges UI | blocked / A3 | yes | PhysicalRange, RangesUi, RangesPage | Options RANGES; tab padlocks | RangeTests, RangesUiTests | |
| Bass techniques: SLAP group, bass step grid | blocked | yes | UI/SlapGroup (CharacterPanel.cpp:353), UI/BassGridGroup (RhythmPanel.cpp:610) | CHARACTER SLAP (bass), RHYTHM BASS GRID | BassTechniqueTests, ModelGapsUiTests | merged from model-gaps. 13 slap trigger params (`slap_armed`…`slap_body_part`) still have no control: in progress on techniques |
| A1 four columns, 1000-pt minimum | A1 | yes | AdvancedPanel | Advanced | EditorTests | |
| Extra sections on nearest column | A1 | yes | AdvancedPanel | Advanced | EditorTests | |
| A2 canonical col-4 tabs | A2 | yes | AdvancedPanel.cpp:1143-1157 (13) | col 4 | Editor::everyWorkspaceTabSelectsAndPaints | 14th (TECHNIQUES) in progress on techniques |
| TUNE tab | A2 | yes | UI/TunePanel, TunePianoRoll, TuneExportDialog, TuneSetlistStrip, TuneChordEditor | col 4 TUNE | TunePanel/TuneEditing/TuneIntegration/PianoRoll Tests | tune-help merged |
| PRACTICE setup tab | A2 | yes | UI/PracticeSetupPanel | col 4 | PracticeSetupPanelTests | |
| NOTATION tab + capture | A2 | yes | UI/NotationPanel, Capture/ | col 4 | NotationPanelTests, CaptureTests | technique on noteOn captured; bend/mark API unused |
| MIDI OUT tab | A2 | yes | UI/MidiOutPanel | col 4 | MidiOutPanelTests | |
| HELP as a tab | A2 | yes | UI/HelpTab | col 4 | HelpTabTests | |
| Expression calibration on LIVE tab | A2 / gui-int 4.4 | no | only Options EXPRESSION; LivePanel.cpp has no calibration | Options only | none | |
| Last-used tab persists | A2 | yes | UiPreferences | n/a | EditorTests | |
| CONTROLLERS in col 4 | A2 | yes | ControllersPage | col 4 | EditorTests | |
| Options 11 tabs | A3 | yes | Overlays.cpp | Options | EditorTests | |
| APPEARANCE accent tint / data-stream / noise-strip toggles | A3 | yes | OptionsPages.cpp:875-1001 accentBox, dataStreamToggle, noiseStripToggle | Options > APPEARANCE | Accent::*, AppearanceTests | was no |
| UPDATES changelog viewer | A3 | partial | OptionsPages.cpp:1751-1822 read-only releaseNotes from the manifest + changelog link; Updates/UpdateDownloader | Options > UPDATES | InstallTests (downloader) | no changelog endpoint, so it shows only the manifest's release note |
| DIAGNOSTICS Workshop/Slide/ranges flag mirror | A3 | yes | OptionsPages.cpp:2271-2281 mirrorNote + AudioPathView | Options > DIAGNOSTICS | VISUAL-WORKSHOP-QA review | was no |
| FILE LOCATIONS Guitars/ and Parts/ | A3 | yes | OptionsPages.cpp:2385-2397 openGuitarsFolder/openPartsFolder, :2482 listing | Options > FILE LOCATIONS | review only | was partial |
| Easy headstock / whammy popovers | A4 | yes | GuitarBodyComponent | Easy guitar | EasyLayoutTests | |
| Per-string detune automatable/learnable | A4 | no | headstock popover writes TuningEngine (now with an undo entry, vwq-undo); no parameter | popover only | UndoCoverageTests (undo only) | needs 12 params + preset field |
| Right-click > Modulate | A4 | yes | Widgets context menu | right-click | EditorTests | |
| Drag-to-assign modulation | A4 | yes | ModMatrixPanel.cpp:321-337 ModSourceCard drag; Widgets.h:139,296 DragAndDropTarget | MOD source cards onto any knob | DragToModulate::aDroppedSourceRoutesAt25PercentAsOneEntry | was no |
| Section 15 banners | A6 | yes | PluginEditor NotificationCentre | banner strip | EditorTests | comment at PluginEditor.cpp:950 still says five are "unreachable" (stale) |
| Capo: one capo, capo_fret, 3 homes | B1 | yes | TuningEngine, capo_fret | col 1, headstock, fretboard | GenreKits tests | |
| Partial capo mask | B1 | yes | TuningEngine::setCapoStringMask | Workshop capo (drawn on bench) | RubricVoicerTests, WorkshopAccessories::theCapoIsDrawnAndDraggedByFrets | GAPS says open (stale) |
| Shortcut registry single source | A5 | yes | Accessibility.cpp registry | Options ACCESSIBILITY | Accessibility::shortcutDefaultsMatchTheCanonicalTable | |
| Workshop `W` shortcut | A5 | yes | Accessibility.cpp:576 toggleWorkshop | `W` | Accessibility::everyShortcutHasADescriptionInTheCatalog | was no |
| Slide `S`, Save As Guitar Ctrl+G | A5 | yes | Accessibility.cpp | keys | EditorTests | |
| New Tune Ctrl+T | A5 | yes | Accessibility.cpp:633 newTune | Ctrl+T | Accessibility tests | was no |
| Space drives the tune transport | A5 / gui-int 17 | no | Accessibility.cpp:641-644 Space = audition; the comment says the tune transport "does not exist yet" (stale). TunePanel.cpp:1356 handles Space only while TUNE has focus | TUNE focused only | TunePanelTests | downgraded from partial: the tune transport now exists and Space still is not given to it |
| Ctrl+N, Ctrl+Alt+E, Ctrl+[ / ] | A5 | yes | Accessibility.cpp:589-590 | keys | EditorTests | |

### PROGRESS.md

**Summary: 24 yes, 4 partial, 4 no (32 rows).** Since the last audit:
- Linux, CLAP and CI (`.github/workflows/{ci,build,nightly,release}.yml`) exist, and a signing path exists but only runs when its secrets are set.
- FirstRun/onboarding moved out of WIP into the build.
- Drag-to-assign is done.
- B-02 is resolved.

Still open:
- Muting is still in `Source/WIP` (excluded by CMakeLists.txt:73). It is in progress on techniques.
- The host matrix has not been run.
- pluginval hits its default timeout (B-12). CI's `scripts/pluginval.sh` sets no `--timeout-ms`.
- The current-state table and the environment section are stale.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| M1-M30 core milestones | Milestones | yes | Source/* | yes | many suites | |
| M31 routing-io | Milestones | yes | Routing/, buses, producesMidi()=true | ROUTING | RoutingTests, PluginBusTests | CMakeLists.txt:49 NEEDS_MIDI_OUTPUT FALSE still contradicts producesMidi |
| M32 mod matrix | Milestones | yes | Modulation/ModMatrix (offset/depth jlimit ±1 at :369-370) | MOD | ModulationTests, DragToModulate | ±4 offset bug fixed (model-gaps merged) |
| M33 rhythm engine | Milestones | yes | Rhythm/ | RHYTHM | RhythmTests, GenreKitTests, RhythmSchedulerTests | B-02 RESOLVED |
| M34 live | Milestones | yes | Live/ | LIVE | LiveTests | |
| M35 controllers | Milestones | yes | Controllers/ | CONTROLLERS | ControllerTests | |
| M36 practice tools | Milestones | yes | Practice/ | drawer + PRACTICE | PracticeTests, PracticeGapsTests | B-14 still open: PluginProcessor.cpp:2204 panic() stops only audition + engine |
| M37 tone match | Milestones | yes | ToneMatch/ | TONE MATCH | ToneMatchTests | |
| M38 notation export | Milestones | yes | Notation/, Capture/ | NOTATION | NotationTests, CaptureTests | |
| M39 character wear | Milestones | yes | Character/ | CHARACTER | CharacterTests | |
| M40 accessibility incl. localisation | Milestones | partial | Accessibility/Localisation.cpp (English catalog only at :244-530; 15 locales listed) | Options | AccessibilityTests | no translated catalogs |
| M41 updates/telemetry/licence | Milestones | yes | Updates/ (+UpdateDownloader, ContentPackage) | Options | TelemetryTests, InstallTests, ContentPackageTests | |
| Current-state table | Current state | no (stale) | now 528 params, 1198 tests, 27 guitars, 36 presets | n/a | Parameters count test | rewrite |
| "Eleven realism specs not on disk" | Phase 2 | no (stale) | specs + phase-2b engines all merged | n/a | n/a | rewrite |
| Freeze and E-Bow split | Phase 2 | yes | FreezeOverlay, EBowDriver | SUSTAIN | EBowTests | |
| Options overlay matches section 5 | Phase 5 | yes | Overlays.cpp | Options | EditorTests | |
| ErrorLog JSON lines | Phase 5 | yes | Support/ErrorLog | DIAGNOSTICS | error tests | |
| Newer-schema preset logs NEWER_SCHEMA | Phase 5 | yes | PresetManager.cpp:531 | n/a | HostStateTests | |
| Not done: CLAP and Linux builds | Not done | yes | CMakeLists.txt:24-32,107 CLAP; scripts/package_linux.sh; build.yml matrix ubuntu/windows/macos | n/a | clap-validator in build.yml | was partial |
| Not done: signed installers | Not done | partial | release.yml; scripts/package_macos.sh (codesign/notarytool when secrets set), package_windows.ps1; GPG-signed SHA256SUMS | n/a | none | unsigned without secrets ("::warning::... ad-hoc signing") |
| Not done: manual per-host matrix | Not done | no | none | n/a | none | ship gate |
| Not done: drag-out export | Not done | yes | MidiOutPanel drag; practice take drag | MIDI OUT, PRACTICE | MidiExport::dragOutWritesAValidMidiFile, PracticeGaps::theSaveButtonDragsTheSavedTakeOut | |
| pluginval strictness 10 passes | Not done | partial | scripts/pluginval.sh (strictness 10) in ci.yml | n/a | CI | B-12: parameter thread-safety test exceeds the 30 s default; the script sets no timeout, so CI would fail there |
| Editor run-verified by tests | Not done | yes | EditorTests, GuiReachabilityTests, ScreenshotTests | n/a | | |
| A4 "Still open": drag-to-assign; 3 notification routes | A4 | yes | ModSourceCard drag; banners | MOD | DragToModulate, EditorTests | was partial |
| Autonomous run items (09-22/23) | 09-22/23 | yes | as listed | yes | suites | |
| Partial capo "written not built" | 09-22/23 | yes (stale) | TuningEngine | Workshop | RubricVoicerTests | |
| Scrape + StrumGesture "committed UNBUILT" | 09-23/24 | yes (stale) | ScrapeEngine, StrumGesture | scrape: no GUI; strum: rows clipped | ScrapeTests, StrumGestureTests | |
| SlapEngine + TechniqueTriggers | 09-23/24 | yes (stale) | DSP/Slap, TechniqueTriggers; UI/SlapGroup | CHARACTER SLAP (bass) | SlapTests, BassTechniqueTests | trigger params' UI in progress on techniques |
| Muting (muting-rhythm.md) | 09-23/24 | partial (WIP, not built) | Source/WIP/Rhythm/Muting, WIP/UI/MuteGroup | no | WIP/Tests/MutingTests (excluded) | in progress on techniques (moves out of WIP, MuteEngine, Mute Row) |
| FirstRun / FirstEncounterHint | 09-23/24 | yes | UI/FirstRun.cpp, FirstRunOs.cpp, FirstEncounterHint.cpp, Onboarding.cpp | first launch, Help > Take the tour | FirstRunTests, OnboardingTests | merged from tune-help (was partial) |
| Environment section (Windows 10/MSVC) | Environment | no (stale) | build is Linux/clang + CI matrix | n/a | n/a | |

### INDEX.md

**Summary: 13 yes, 6 partial, 2 no (21 rows).** Phase 2b (23a-23i) is now fully on integration: engines, CHARACTER groups and 135 tests.

Phase 5b is mostly still on techniques:
- slide-technique-controls, tapping, microtonal bends, cascade, the engine technique layer, the Techniques GUI and Muting are all on techniques only.
- On integration there is only the scrape engine without UI, and slap with UI for the bass only.

Phase 6 has an automated bug bash (BETA_TEST_REPORT B-01..B-16, 6 open) and no human check.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Phase 1: 11 extension specs | 1-11 | yes | Routing/…Updates/ | yes | suites | |
| Phase 2: 12 realism specs | 12-23 | yes | PhysicalRange, GuitarCircuit, Noise, Slide, Workshop, StrumGesture, SlapEngine, Export | mostly | suites | scrape UI missing; strum rows clipped |
| 23a string-aging | 2b | yes | DSP/String/StringAging; UI StringAgingGroup | CHARACTER | StringAgingTests (16) | B-15 OPEN: `string_age` Old silences a bass above ~A3 ("P-Bass Flatwound" fails Combo); Workshop strings inspector mirror deferred |
| 23b environment | 2b | yes | Character/EnvironmentModel; EnvironmentGroup | CHARACTER | EnvironmentTests (14) | SETUP humidity line deferred |
| 23c body-coupling | 2b | yes | DSP/Coupling/BodyCouplingBank; BodyCouplingGroup/WolfMap | CHARACTER | BodyCouplingTests (13) | off in factory presets pending a listening pass (BC-R13) |
| 23d harmonic-realism | 2b | yes | DSP/String/Harmonics.h; LuthierEngineRealismB; HarmonicsGroup | CHARACTER | HarmonicRealismTests (18) | |
| 23e string-interaction | 2b | yes | CouplingMatrix; StringInteractionGroup | CHARACTER | StringInteractionTests (9) | B-03 FIXED; TECHNIQUES MUTE mirror deferred |
| 23f fingerstyle-attack | 2b | yes | RightHandGroup; SlapGroup rest-stroke/alternation | CHARACTER | FingerstyleAttackTests (16) | |
| 23g noise-floor | 2b | yes | DSP/Noise/NoiseFloor; NoiseFloorGroup | CHARACTER | NoiseFloorTests (18) | |
| 23h sustain-and-decay | 2b | yes | SustainShapeGroup, DecayRow | CHARACTER, col 1 | SustainDecayTests (15) | `sustain_scale` bridge wiring deferred (SUS-R17) |
| 23i tuning-stability | 2b | yes | Model/Playing/StabilityModel; TuningStabilityGroup | CHARACTER | TuningStabilityTests (16) | Retune-all MIDI Learn target deferred |
| Phase 3 tune-builder | 24 | yes | Tune/, TunePanel, TunePianoRoll, TuneExportDialog, TuneSetlistStrip | col 4 TUNE | TuneBuilder/TuneEditing/TuneIntegration/PianoRoll Tests | |
| Phase 4 gap-fills | 25-32 | partial | see groups | | | onboarding done; installers sign only with secrets; idle CPU over budget (B-13) |
| Phase 5 deep-integration | 33-41 | yes | various | | | `.luthierkit` partial (C-19) |
| 42 string-scraping | 5b | partial | ScrapeEngine | no | ScrapeTests | UI in progress on techniques |
| 43 slide-technique-controls | 5b | no | none on integration | no | none | in progress on techniques (SlideEngine +213, SlideTechniqueTests) |
| 45 muting-rhythm | 5b | partial (WIP, not built) | Source/WIP/Rhythm/Muting | no | WIP MutingTests | in progress on techniques |
| 44/46/47 slap-technique, tapping, microtonal-bends | 5b | partial | SlapEngine + SlapGroup (bass) | CHARACTER SLAP (bass only) | SlapTests, BassTechniqueTests | slap triggers, TapEngine, BendEngine, MicrotonalScale in progress on techniques |
| 48/50 technique-cascade, engine-technique-layer | 5b | no | none | n/a | none | CascadeResolver/TechniqueLayer in progress on techniques |
| 49 gui-techniques-updates | 5b | partial | none on integration except the SLAP/BASS GRID groups | no TECHNIQUES tab | none | TechniquesPanel, pill row, overlays in progress on techniques |
| Phase 6 bug bash + final human check | 6 | partial | docs/audit/BETA_TEST_REPORT.md: B-01..B-16, 10 fixed/resolved/by-design, 6 open (B-07, B-11, B-12, B-13, B-14, B-15; B-16 part) | n/a | Combo tests | no human check |

### REVIEW.md

**Summary: 14 yes, 2 partial (16 rows).** Unchanged in status. Notation now captures the techniques played, but the engine sends no bend or mark events. Localisation is still English only. The mod matrix now has drag-to-assign, and string-aging depth is merged.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| 1 Rhythm/pattern engine | Gaps | yes | Rhythm/ | RHYTHM | RhythmTests | |
| 2 Live-performance layer | Gaps | yes | Live/ | LIVE | LiveTests | |
| 3 Modulation matrix | Gaps | yes | Modulation/, ModMatrixPanel | MOD (+ drag-to-assign) | ModulationTests, DragToModulate | |
| 4 Multi-out, sidechain, MIDI out | Gaps | yes | Routing/ | ROUTING | RoutingTests | |
| 5 User IR / tone match | Gaps | yes | ToneMatch/ | TONE MATCH | ToneMatchTests | |
| 6 Notation output | Gaps | partial | Notation/, Capture/ | NOTATION | NotationTests, CaptureTests | noteOn carries the technique; engine never calls `bend()`/`mark()` |
| 7 Controller integration | Gaps | yes | Controllers/ | CONTROLLERS | ControllerTests | |
| 8 Character/aging | Gaps | yes | Character/, StringAging | CHARACTER | CharacterTests, StringAgingTests | B-15 |
| 9 Accessibility / localisation | Gaps | partial | Accessibility/ | Options | AccessibilityTests | English catalog only, no RTL |
| 10 Updates/telemetry/crash | Gaps | yes | Updates/ | Options | TelemetryTests, InstallTests | |
| Chord auto-fingering rubric | Ambiguities | yes | RubricVoicer (+bass pattern) | RHYTHM BASS GRID | RubricVoicerTests | |
| Feedback: physical loop | Ambiguities | yes | FeedbackLoop | FEEDBACK | FeedbackTests | |
| Freeze vs E-Bow | Ambiguities | yes | FreezeOverlay, EBowDriver | SUSTAIN | EBowTests | |
| Doubler defaults | Ambiguities | yes | Doubler pedal | pedal rack | DoublerTests | B-07 |
| Preset morph | Ambiguities | yes | PresetMorph | LIVE | PresetMorphTests | |
| Milestone order | Recommended | yes | PROGRESS M31-M41 | n/a | n/a | |

### JUCE_CLAUDE_GUIDELINES.md

**Summary: 23 yes, 12 partial, 2 no, 1 n/a (38 rows).**

What changed:
- CI now exists and runs the tests plus pluginval on every push and PR.
- The allocation counter is now compiled into the test target (CMakeLists.txt:207).

Rules still broken:
- No warnings-as-errors: MSVC warnings are suppressed, and there are no `-W` flags.
- JUCE is cloned at tag 8.0.10, not pinned as a submodule.
- Factory presets are generated files, not BinaryData.
- Tests use a custom framework, not `juce::UnitTest`.
- The audio thread uses try-locks (37 `ScopedTryLock`/`SpinLock` sites in DSP/Tune/Practice/engine).
- Nothing uses `setBufferedToImage`.
- IR paths outside the library root are stored absolute.
- There is no host matrix.

| Requirement | Section | Implemented? (yes/partial/no) | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Query context7 before JUCE code | 1 | n/a (process) | n/a | n/a | n/a | not verifiable |
| No alloc in audio callback | 2 | partial | pre-sized buffers; fixes 9383a34, 6a0d737 | n/a | AudioThreadSafetyTests; Capture/TunePlayer/PianoRoll/Circuit alloc checks now live (LUTHIER_ALLOCATION_COUNTER=1, CMakeLists.txt:207) | covered by tests now, not proven for every path |
| No locks on audio thread | 2 | partial | ScopedTryLock/SpinLock try in EffectsChain, BodyEngine, CabinetEngine, TunePlayer, BackingTrack (37 sites) | n/a | Combo stress | try-locks, never waits |
| No dynamic_cast/exceptions on audio thread | 2 | yes | EffectsChain casts are message-thread | n/a | none | |
| No GUI calls from audio thread | 2 | yes | timers; MidiLearn dispatchPendingLearn no longer posts from audio (6e14780) | n/a | EditorTests | |
| Pre-allocate in prepareToPlay | 2 | yes | PluginProcessor::prepareToPlay | n/a | | |
| DSP free of GUI headers | 3 | yes | Source/DSP | n/a | build | |
| JUCE pinned as submodule at a tag | 4 | partial | no .gitmodules; setup_linux.sh:12 clones 8.0.10; ci.yml uses it | n/a | n/a | |
| Explicit FORMATS | 4 | yes | CMakeLists.txt:35-45 | n/a | n/a | CLAP optional |
| COPY_PLUGIN_AFTER_BUILD off | 4 | yes | CMakeLists.txt:52 | n/a | n/a | |
| Stable PLUGIN_CODE / MANUFACTURER_CODE | 4 | yes | Lthr / Ltha | n/a | n/a | |
| Static MSVC runtime | 4 | yes | CMakeLists.txt:12 | n/a | n/a | |
| cxx_std, no extensions | 4 | yes | CMakeLists.txt:5-7 | n/a | n/a | |
| Warnings as errors in CI | 4 | no | CMakeLists.txt:10 /wd4244 /wd4267 /wd4305 /wd4996; no -Wall/-Werror anywhere (CMake, workflows, scripts) | n/a | none | CI exists now, so the gap is only the flag |
| Single APVTS | 5 | yes | PluginProcessor apvts | n/a | IntegrationTests | |
| Param IDs in one header | 5 | yes | Parameters.h | n/a | Parameters tests | |
| Cached raw param pointers | 5 | yes | ParameterBridge | n/a | | |
| Attachments, no hand-wired listeners | 5 | partial | most via attachTo; detune popover writes TuningEngine directly | | GuiReach | |
| copyState/replaceState | 5 | partial | custom JSON (PresetManager::toVar) | n/a | HostStateTests | equivalent |
| No GUI from parameterChanged | 5 | yes | AsyncUpdater | n/a | | |
| prepare+reset every module | 6 | yes | reset() everywhere (B-08 fix) | n/a | Combo::renderDoesNotDependOnWhatWasPlayedBefore | |
| SmoothedValue on continuous params | 6 | yes | smoothers | n/a | | not exhaustive |
| ScopedNoDenormals | 6 | yes | PluginProcessor.cpp:1162, LuthierEngine.cpp:2328 | n/a | | |
| Variable block sizes | 6 | yes | processBlock slices at :1170 | n/a | pluginval | |
| isBusesLayoutSupported explicit | 6 | yes | PluginProcessor | n/a | PluginBusTests | |
| Clear unused output channels | 6 | partial | no clear at the top of processBlock; applyDeclick clears only when silent | n/a | PluginBusTests | |
| Oversampling factor a parameter | 6 | yes | oversample param | Options AUDIO + Advanced | | |
| MIDI at sample position | 6 | yes | processSlice | n/a | Controllers tests | |
| Background to audio via atomic swap | 7 | partial | part swaps now built off-thread, swapped at a block boundary (swapPartsAtBlockBoundary); structural changes still park 5 ms | n/a | PartSwap tests | |
| GUI caches paths, 30 Hz, setBufferedToImage | 8 | partial | timers ≤30 Hz; no setBufferedToImage | | | |
| juce::UnitTest suites | 9 | partial | custom LuthierTests (TestFramework.h) | n/a | 1198 tests | |
| pluginval strictness 10 in CI on every PR | 9 | yes | ci.yml (push + pull_request) → scripts/pluginval.sh strictness 10; build.yml strictness 5 on push/PR, 10 nightly | n/a | CI | B-12 timeout likely to fail the job (no --timeout-ms) |
| Host test matrix | 9 | no | none | n/a | none | |
| State versioned | 10 | yes | PresetManager.cpp:359 pluginVersion | n/a | HostStateTests | |
| No absolute paths in state | 10 | partial | IrLibraryPaths::toPresetPath falls back to full path | n/a | ToneMatchTests | |
| Factory presets as BinaryData | 10 | partial | FactoryPresets.cpp generates files with factoryRevision 3 (writeAll); not BinaryData | n/a | PresetQaTests | deviation |
| AAX only with PACE | 11 | yes | not built | n/a | n/a | |
| Definition-of-done checklist enforced | 12 | partial | CI gates tests + pluginval; no warnings gate, no host matrix | n/a | CI | was no |

### Top gaps (group I)

Ranked by user impact:

1. **The techniques layer is still unmerged.** This covers the TECHNIQUES tab, TapEngine, BendEngine/MicrotonalScale, slide-technique controls, CascadeResolver/TechniqueLayer, MuteEngine and the Mute Row. 27 automatable parameters have no control: 14 `scrape_*` and 13 slap triggers. All of this is in progress on techniques.
2. **Idle CPU is about the same as playing CPU (B-13).** There is no silence short-circuit, and idle is 6-12x the budget.
3. **B-15: `string_age` Old silences a bass above about A3.** The factory preset "P-Bass Flatwound" fails `Combo.everyFactoryPresetPlaysEveryPhrase`.
4. **Panic and Reset leave the looper, backing track, tune player, metronome and rhythm engine running (B-14).** `PluginProcessor.cpp:2204`.
5. **The RHYTHM STRUM group's lower six rows are clipped (B-11).** `AdvancedPanel.cpp:1471` sizes RhythmPanel to the viewport height, not `preferredHeight()`.
6. **pluginval's thread-safety test times out at the 30 s default (B-12).** CI's `scripts/pluginval.sh` sets no timeout, so the new CI job would fail there.
7. **Localisation is English only**, despite 15 listed locales.
8. **Space is not the tune transport.** The code comment says the transport "does not exist yet", which is stale.
9. **Per-string detune has no parameter**, so there is no automation and no MIDI Learn.
10. **Several part fields change nothing**: `coil_turns`, `spring_count`, `plies`, `radius_mm` and pole shape. The Workshop shows them anyway.
11. **`pickup_blend` has no control, and `PickupEngine` never reads `blendAmount` in process.** Macro 7/8 also have no knobs.
12. **The meta spec files are stale.** DECISIONS C-02, all of GAPS, PROGRESS "Current state"/"Not done"/Environment, and INDEX phase 2b status are untouched since the merges.
13. **Legacy `doubler_on` migrates on every load (B-07).**
14. **Expression calibration is not on the LIVE tab.**
15. **The genre-kit file format is half there.** Kits are `*.json`, not `.luthierkit`, have no magic, and there is no save UI.
16. **The engine never feeds bend and technique marks to the capture**, so they are missing from the notation.
17. **`NEEDS_MIDI_OUTPUT FALSE` while `producesMidi()` returns true.** MIDI OUT may be invisible to hosts.
18. **No warnings-as-errors and no host test matrix.**
19. **Installers are signed only when the CI secrets are present.** Default builds are ad-hoc or unsigned.
20. **The rubric unisons listening pass and the MIDI chuck key range are still undecided.**

### Unspecified gaps noticed

- **Tuner.** There is still no chromatic tuner or tuning meter. `DSP/Common/PitchTracker` now exists (tune-help), so the pieces are there.
- **Out-of-range notice.** Notes below the instrument are dropped silently (B-09: by design, but no notice and no octave-fold).
- **Preset browsing.** 36 factory presets, with no favourites or tags found in `Overlays.cpp`.
- **Undo history view.** This is now covered: File > Undo history, `UndoHistoryPanel`, a search box and a depth footer.
- **CLAP ulp drift on `noise_player_distance` (B-16, low).** It is harmless, but clap-validator reports it.
- **Stale code comments.** `PluginEditor.cpp:950` says five banners are "unreachable", and `Accessibility.cpp:641` says the tune transport does not exist.

### Small glue candidates

| Item | Param IDs / symbol | Should go on | Notes |
|---|---|---|---|
| STRUM rows clipped | `strum_acceleration`, `strum_up_velocity_ratio`, `strum_tilt`, `strum_miss_probability`, `chuck_amount`, `chuck_damping` | AdvancedPanel.cpp:1476 | add `else if (auto* p = dynamic_cast<RhythmPanel*> (panel)) height = jmax (visible, p->preferredHeight());` |
| Space to tune transport | registry id `audition` (Accessibility.cpp:644) | Accessibility registry | give Space to the tune transport and move audition, as the comment itself says |
| Macro 7/8 knobs | `macro_assign_a`, `macro_assign_b` | MOD tab MACROS row | mod sources only today |
| Pickup blend | `pickup_blend` | Adv col 2 PICKUPS | also needs `blendAmount.getNextValue()` used in `PickupEngine` process |
| Scrape controls | 14 `scrape_*` | TECHNIQUES > SCRAPE | ready on techniques (TechniquePages.cpp) |
| Slap triggers | `slap_armed`, `slap_type`, `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`, `slap_ghost_cc`, `slap_force`, `slap_palm_position_mm`, `slap_string_mask`, `slap_ghost_mode`, `slap_rebound_gap`, `slap_snap_back`, `slap_body_part` | TECHNIQUES > SLAP | ready on techniques |
| Doubler migration gate | `doubler_on` in PresetManager.cpp:697 | preset loader | gate on schemaVersion/pluginVersion |
| pluginval timeout | scripts/pluginval.sh:32 | CI script | add `--timeout-ms 900000` (BETA B-12 says it passes with it) |
| Panic stops players | `LuthierAudioProcessor::panic` | processor | also stop looper, backing track, tune player, metronome, progression looper, rhythm engine |
| Kit save | `GenreKitLibrary::save` | RHYTHM genre row "Save kit…" | switch the extension to `.luthierkit` + magic in `saveTo`/`scanDirectory` |
| Winding selector | NoiseGroups.cpp:245 | CHARACTER squeak group | edit the strings part (Workshop) rather than `string_material` |
| Stale comments | PluginEditor.cpp:950, Accessibility.cpp:641 | n/a | all banners are wired; tune transport exists |
| NEEDS_MIDI_OUTPUT | CMakeLists.txt:49 | CMake | set TRUE to match `producesMidi()` |
| Expression calibration mirror | ExpressionCalibrationSet | LIVE tab summary + "Calibrate…" | avoids a second writer |

---

## Group J: feature specs (feat-* branches)

Audited: `origin/claude/luthier-cloud-session-5lzlix` at e4dee39 (FEAT-STRINGS, FEAT-JAM, FEAT-NORMALIZE and FEAT-CPU merged), read from a `git archive` export. Unmerged helper branches were read with `git show`. Nothing was built or run: a test named here exists in the source, which does not mean it passes.

### animated-strings.md

**Summary:** 88 requirements. **74 yes / 13 partial / 1 no**.
- Done (merged FEAT-STRINGS): SoundingNotes publication, StringMotion model, StringAnimator frame driver, layer 25a ghost drawing on Easy/Advanced illustrations and the fretboard, Options -> APPEARANCE -> VISUAL AIDS rows, shortcut, AS-01..AS-30 tests all present.
- Open: after the FEAT-CPU merge the animator still reads `StringMotionPolicy` (Reduced motion only), not `AnimationPolicy`; CPU quality Medium/Low and the governor's relief never reach `StringAnimator`, and `StringAnimator` has no `AnimationPolicy::Registration` (CQ-22 likely fails).
- Open: no Free edition build (AS-30 is a source scan), tap-marker overlay absent, coordinator spec follow-ups not done.
- Relaxed: AS-05 window [0.75, 0.95], AS-15 thresholds x2 reference-CPU factor, palm-mute mask moved to `smoothstep(0.08, 0.16, 1-u)`.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Off by default; opt-in preference | 0.1, 5 | yes | Source/UI/Guitar/StringAnimator.h:StringAnimationSettings (UiPreferences `animateStrings`) | yes (Options -> APPEARANCE -> VISUAL AIDS) | AnimatedStrings.AS01_defaultOffAndPixelIdentical | |
| Amplitude from engine level follower, stop point from waveguide stop (fret/tap/slide/capo), never from MIDI | 0.2 | yes | Source/LuthierEngine.cpp:LuthierEngine::publishSoundingNotes | n/a | AS10_slideContact, AS11_tapPoint, AS28_everySourceAnimatesWhatSounds | |
| Swept-envelope "motion ghost" (arcsine edges), not instantaneous displacement | 0.3, 2.3 | yes | Source/UI/Guitar/GuitarRenderer.h:paintMotionGhost | yes | AS12_AS13_dirtyRectsMatchAFullRepaint, AS23_materialLook | |
| Zero audio-thread cost of its own; renders bit-identical with toggle on/off | 0.4 | yes | publishSoundingNotes runs every sub-block regardless | n/a | AS17_theAudioThreadIsUnaffected | |
| Dirty-rect repaints only, nothing when silent, pause when hidden, off under Reduced motion | 0.5 | yes | Source/UI/Guitar/StringAnimator.cpp:poll | n/a | AS13, AS14_idleIsFree, AS19, AS20 | |
| Stop point geometry via stringAt / fretX (open = capo, fretted, tapped, slide contactFret with slant) | 2.1 | yes | StringMotion.h:StringMotionGeometry::pointAt; LuthierEngine::updatePerBlockModulation | n/a | AS04_stopPointAndBridgeAreNodes, openStringsStopAtTheCapo, AS10, AS11 | |
| Bridge point: saddle on illustration, virtual scale-length point on fretboard; envelope over full length | 2.1 | yes | StringMotion geometry providers in GuitarBodyComponent / FretboardComponent | n/a | AS04, AS27_bothModes | |
| Nut-to-stop segment static (displaced by bend); behind slide bar static | 2.1 | yes | StringMotion::update | n/a | AS04, AS10 | |
| Envelope A(u,t) Fourier pluck shape, c_k decay 0.12 s, K=4 High / K=1 Low | 2.2 | partial | Source/UI/Guitar/StringMotion.cpp:envelope | n/a | AS05_thePluckShapeRelaxes | Pick placed at u = 1-p (spec formula contradicts AS-05); AS-05 window widened to [0.75, 0.95] |
| Harmonic shape abs(sin(h pi u)) for partial >= 2 | 2.2 | yes | StringEngine::getHarmonicPartial; StringMotion::envelope | n/a | AS06_harmonicNodes | |
| L_n = clamp(level*4) shared normaliser `StringMotion::normaliseLevel` | 2.2 | yes | StringMotion.h:normaliseLevel | n/a | AS02_amplitudeIsProportional | |
| A_max = 0.40 x local spacing, floor 1.5 px | 2.2 | yes | StringMotion::update | n/a | AS02 | |
| Floor 0.5 px: inactive below, one final rest repaint | 2.2 | yes | StringMotion::update | n/a | AS03_theFloorEmitsOneFinalDirtyRect | |
| Drawing table High/Low (fill alpha, edges, mid lines, rest line, 32/12 samples, 60/30 Hz) | 2.3 | yes | GuitarRenderer::paintMotionGhost | yes | AS12_AS13, AS22_frameRateCaps, AS23 | |
| Layer 26 accent glow replaced while animating; played-note dot kept | 2.3 | yes | GuitarRenderer.h:GuitarOverlay::motionActive | yes | AS12_AS13, AS01 | |
| High-contrast palette: Low style in text colour | 2.3 | yes | GuitarSpeakingStyle | yes | AS23_materialLook | |
| Fretboard uses stringLooks colours and gauge-scaled thickness | 2.3, 6.1 | yes | GuitarRenderer::stringLooks; FretboardComponent::paint | yes | AS23 | |
| 12-string: each engine string animates independently | 2.3 | yes | per-index SoundingNotes records | yes | staticSpeakingLengthsMatchPaintString | by construction |
| Bend displacement d = spacing*sqrt(clamp(cents,0,450)/200), treble half toward bass | 2.4 | yes | StringMotion::update | n/a | AS07_bendsPushAcrossTheNeck | |
| bendCents = finger bend only (whammy/slide/drift excluded, negative no push) | 2.4 | yes | publishSoundingNotes `pushCents` | n/a | AS08_theWhammyDoesNotPushButAPitchBendDoes | |
| Damping mask/decay (PalmMute pinned last 8%, LightTouch x0.5, 25 ms decay for Released/Choked/Silenced/Chuck) | 2.5 | partial | StringMotion::dampingMask | n/a | AS09_muting | Mask ramp moved to smoothstep(0.08,0.16,1-u) to satisfy AS-09 |
| Gate: pref && !reducedMotion && showing && relief<2 && !stale | 2.6 | yes | StringAnimator::poll | n/a | AS19, AS20, AS21, AS29 | |
| Frame clock: VBlankAttachment throttled >= 15 ms; 30 Hz timer at Low / no peer; runs only while a string is above floor | 2.6 | yes | StringAnimator::startClock / onVBlank | n/a | AS22_frameRateCaps, AS14 | |
| Stale after 250 ms: ease to rest over 120 ms, then stop | 2.6 | yes | StringAnimator::readSnapshot / staleGain | n/a | AS21_aStaleSnapshotEasesToRest | |
| Relief hook `setReliefLevel` (1 forces Low, >=2 pauses) fed by the CPU relief ladder | 2.6 | partial | StringAnimator::setReliefLevel | n/a | AS29_theReliefHook | Hook exists; nothing calls it outside tests (QualityController relief goes only to AnimationPolicy) |
| Gate reads `AnimationPolicy::getStringsStyle()`; holds an `AnimationPolicy::Registration` (Decorative) | 2.6 (cpu-quality-modes 6) | partial | Source/UI/Guitar/StringMotionPolicy.cpp:getMotion | n/a | cpuQualityMotionPolicy | Still returns Off only for Reduced motion ("Swap for AnimationPolicy::get().getMotion()" comment); StringAnimator has no Registration, so CPU Medium/Low do not change string animation |
| Surfaces: Easy illustration, Advanced illustration, Advanced fretboard animate | 3 | yes | EasyPanel/AdvancedPanel guitarBody, FretboardComponent own a StringAnimator | yes | AS27_bothModes | |
| Workshop bench and thumbnails never animate | 3 | yes | BenchIllustration, GuitarRenderer::render (no animator) | n/a | AS24_benchAndThumbnailsNeverAnimate | |
| Scale trainer / tab reader fretboards animate as FretboardComponent | 3 | yes | FretboardComponent | yes | - | by construction |
| `SoundingNotes` per-string record (level, stopFret, bendCents, pluckPosition, startSample, damping, harmonicPartial, stopKind), seqlock | 4.1 | yes | Source/Support/SoundingNotes.h (Motion record) | n/a | AS16_noAllocations, AS18_thePublishIsRealtimeSafe | Extended the piano roll's double-buffered class rather than a new seqlock struct; two instances (processor publisher + engine) remain |
| Writer at end of processSubBlock before cpuEstimate; never waits | 4.1 | yes | LuthierEngine.cpp:3051 | n/a | AS17, AS18 | |
| Reader retries <=3, keeps previous snapshot | 4.1 | yes | StringAnimator::readSnapshot | n/a | AS16 | 4 attempts (class's own) |
| Data race closed: getStringLevel/getStringFret read published atomics | 4.1 | yes | SoundingNotes::readLevel/readFret | n/a | full suite | |
| Accessor `LuthierEngine::getSoundingNotes()` | 4.1 | yes | LuthierEngine.h | n/a | AS17 | |
| `StringMotion` pure logic, fixed-size frame, dirty rect = union + stroke + 2 px | 4.2 | yes | Source/UI/Guitar/StringMotion.{h,cpp} | n/a | AS02-AS11, AS16 | |
| `StringAnimator` with test hooks (setClockForTesting, stepFrameForTesting, getLastDirtyUnion, isRunning, getFullRepaintCount) | 4.3 | yes | Source/UI/Guitar/StringAnimator.{h,cpp} | n/a | AS12-AS14, AS20-AS22 | |
| Renderer `PaintLayers::omitSpeakingLengths`; cache rebuilt when pref flips | 4.4 | yes | GuitarRenderer.h:omitSpeakingLengths; GuitarBodyComponent::rebuildCache | n/a | staticSpeakingLengthsMatchPaintString, AS19 | |
| `paintSpeakingLengths` layer 25a (nullptr = static lines identical to paintString) | 4.4 | yes | GuitarRenderer::paintSpeakingLengths | n/a | staticSpeakingLengthsMatchPaintString | |
| Skip strings outside clip; overlays painted in same clipped paint | 4.4 | yes | GuitarBodyComponent::paint | n/a | AS12_AS13 | |
| Fretboard: same between strings and sounding-notes blocks; excitement blur replaced | 4.4 | yes | FretboardComponent::paint | yes | AS15, AS27 | |
| GPU-friendly drawing (no effects, no per-frame image alloc) | 4.4 | yes | GuitarRenderer | n/a | - | by inspection |
| VISUAL AIDS section under tooltips/reduced-motion row: toggle, Quality combo, status line | 5 | yes | Source/UI/VisualAidsSection.cpp (in OptionsPages.h AppearancePage) | yes | AS25_theOptionsRows | |
| Controls always visible and enabled; save immediately; all editors pick up on 30 Hz tick | 5 | yes | VisualAidsSection; StringAnimationSettings | yes | AS25, AS26 | |
| No parameters; host state identical; presets never change it | 6 | yes | - | n/a | AS26_persistenceAndScope | |
| ui.json keys `animateStrings`, `animateStringsQuality`; missing = defaults | 6 | yes | StringAnimationSettings | n/a | AS26, AS01 | |
| `GuitarRenderer::stringLooks` extracted from build loop | 6.1 | yes | GuitarRenderer.h:stringLooks | n/a | AS23 | |
| Nothing undoable | 7 | yes | - | n/a | - | by inspection |
| Tab order after Reduced motion; accessible names/values/description | 8 | yes | VisualAidsSection (setExplicitFocusOrder, setDescription) | yes | AS25 | |
| Rebindable "Toggle string animation" shortcut, unbound | 8 | yes | Accessibility.cpp `toggleStringAnimation`; PluginEditor.cpp performs it | yes | AS25 | |
| Reduced motion always wins; pref keeps its value | 8 | yes | StringAnimator::poll | n/a | AS19_reducedMotionWins | |
| Strings in catalog under options.appearance.visualAids.* | 8 | yes | Source/Accessibility/Localisation.cpp | n/a | AS25 | |
| Both editions identical | 9 | partial | no edition check in code | n/a | AS30_notGatedByEdition | No Free build exists; test is a no-gating check only |
| Interactions: slide bar re-anchor, tap stop point and pull-off, chord name overlay, strum stagger | 10 | partial | StringMotion; ChordNameOverlay painted in same clip | yes | AS10, AS11, AS12_AS13, AS27 | Square tap marker "drawn over the string" does not exist on integration (two-hand-tapping overlay is on claude/luthier-techniques) |
| Interactions: rhythm engine, Tune Builder, piano roll, sympathetic ring, E-Bow animate at true level; whammy/drift never push; family switch resets motion | 10 | yes | GuitarBodyComponent::rebuildScene -> animator.resetMotion | n/a | AS28_everySourceAnimatesWhatSounds, AS08 | |
| Message-thread frame budget median < 1 ms, p99 < 2 ms (1920x1080, both views) | 11 | partial | StringAnimator + renderer (quarter-pixel runs for near-horizontal ghosts) | n/a | AS15_frameBudget | Thresholds multiplied by a 2.0 reference-CPU factor; measured ~1.5 / 2.9 ms on CI |
| Dirty union <= 25%, one repaint per string, never full repaint; idle zero; no allocation; frame-rate caps | 11 | yes | StringAnimator | n/a | AS12_AS13, AS14, AS16, AS22 | |
| Failure modes: missing ui.json, no vblank, failed read, stale, NaN clamps, empty scene, 10 frames over budget -> Low + ErrorLog | 12 | yes | StringAnimator (STRING_ANIMATION_OVER_BUDGET) | n/a | AS01, AS07, AS21, overBudgetFramesDropToLow | |
| Coordinator follow-ups (gui-integration 5/19, guitar-illustration 25a, accessibility 5, gui-engine-dataflow, file-formats ui.json keys, editions 2.3) | follow-ups | no | - | n/a | - | Spec-document edits left for the coordinator |
| AS-01 Default off, pixel-identical | 13 | yes | - | - | AnimatedStrings.AS01_defaultOffAndPixelIdentical | |
| AS-02 Proportional amplitude | 13 | yes | - | - | AnimatedStrings.AS02_amplitudeIsProportional | |
| AS-03 Floor | 13 | yes | - | - | AnimatedStrings.AS03_theFloorEmitsOneFinalDirtyRect | |
| AS-04 Stop point and bridge node | 13 | yes | - | - | AnimatedStrings.AS04_stopPointAndBridgeAreNodes | |
| AS-05 Pluck shape relaxes | 13 | partial | - | - | AnimatedStrings.AS05_thePluckShapeRelaxes | Window [0.75, 0.95] not [0.8, 0.95] |
| AS-06 Harmonic nodes | 13 | yes | - | - | AnimatedStrings.AS06_harmonicNodes | |
| AS-07 Bend displacement | 13 | yes | - | - | AnimatedStrings.AS07_bendsPushAcrossTheNeck | |
| AS-08 Whammy does not push | 13 | yes | - | - | AnimatedStrings.AS08_theWhammyDoesNotPushButAPitchBendDoes | |
| AS-09 Muting | 13 | yes | - | - | AnimatedStrings.AS09_muting | |
| AS-10 Slide contact | 13 | yes | - | - | AnimatedStrings.AS10_slideContact | |
| AS-11 Tap point | 13 | partial | - | - | AnimatedStrings.AS11_tapPoint | Stop point checked; square tap marker not in this build |
| AS-12 Dirty-rect correctness | 13 | yes | - | - | AnimatedStrings.AS12_AS13_dirtyRectsMatchAFullRepaint | |
| AS-13 No full repaints | 13 | yes | - | - | AnimatedStrings.AS12_AS13_dirtyRectsMatchAFullRepaint | |
| AS-14 Idle is free | 13 | yes | - | - | AnimatedStrings.AS14_idleIsFree | |
| AS-15 Frame budget | 13 | partial | - | - | AnimatedStrings.AS15_frameBudget | x2 CPU factor |
| AS-16 No allocations | 13 | yes | - | - | AnimatedStrings.AS16_noAllocations | |
| AS-17 Audio thread unaffected | 13 | yes | - | - | AnimatedStrings.AS17_theAudioThreadIsUnaffected | |
| AS-18 Publish is realtime-safe | 13 | partial | - | - | AnimatedStrings.AS18_thePublishIsRealtimeSafe | No lock trap in repo (allocation trap only, per CQ-20 note) |
| AS-19 Reduced motion | 13 | yes | - | - | AnimatedStrings.AS19_reducedMotionWins | |
| AS-20 Hidden editor | 13 | yes | - | - | AnimatedStrings.AS20_aHiddenViewStops | |
| AS-21 Stale snapshot | 13 | yes | - | - | AnimatedStrings.AS21_aStaleSnapshotEasesToRest | |
| AS-22 Frame-rate caps | 13 | yes | - | - | AnimatedStrings.AS22_frameRateCaps | |
| AS-23 Material look | 13 | yes | - | - | AnimatedStrings.AS23_materialLook | |
| AS-24 Bench and thumbnails never animate | 13 | yes | - | - | AnimatedStrings.AS24_benchAndThumbnailsNeverAnimate | |
| AS-25 Options UI | 13 | yes | - | - | AnimatedStrings.AS25_theOptionsRows | |
| AS-26 Persistence and scope | 13 | yes | - | - | AnimatedStrings.AS26_persistenceAndScope | |
| AS-27 Both modes | 13 | yes | - | - | AnimatedStrings.AS27_bothModes | |
| AS-28 Rhythm, piano roll, Tune sources | 13 | yes | - | - | AnimatedStrings.AS28_everySourceAnimatesWhatSounds | |
| AS-29 Relief hook | 13 | partial | - | - | AnimatedStrings.AS29_theReliefHook | Tests the hook in isolation; hook unfed in the product |
| AS-30 Edition | 13 | partial | - | - | AnimatedStrings.AS30_notGatedByEdition | No Free configuration to build and run |


### auto-articulation.md

**Summary:** 90 requirements. **0 yes / 0 partial / 3 no** (plus 87 in progress on claude/luthier-feat-assist).
- Nothing is on the integration branch: the whole feature (AutoArticulator, styles, feed, 4 `aa_*` parameters, Easy AUTO pill, RHYTHM PLAYING group, label overlay, capture/notation marks, `Support/Edition.h`) is in progress on `claude/luthier-feat-assist` (56 files, ~6900 lines), with all AA-01..AA-42 tests written there.
- Open even on the branch: TECHNIQUES CASCADE "Auto" row (techniques tab not merged), Tap-armed / mute-grid context always false, NOTATION/TAB secondary-accent colouring, editions upsell panel (a notice instead), piano-roll ghost dots (PR-05).
- Merge risks: `NoteOnEvent::explicitArticulation` is also added by feat-riffs; parameter append order vs FEAT-JAM (34) and FEAT-MIC (25); `A` shortcut and Visual aids switch placement.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Off / Amount 0 / rules 0 bit-identical; own hash, no MidiInterpreter::rng draws | 0.1 | in progress on claude/luthier-feat-assist | MidiInterpreter::isAssistEffective; AutoArticulator::hash01 | n/a | AutoArticulationEngine.offAmountZeroAndNoRulesAreTheSameAsNothing, AutoArticulation.assistDrawsNothingFromTheInterpretersRandom | plus scripts/assist_off_golden_check.sh |
| Explicit always wins | 0.2, 5 | in progress on claude/luthier-feat-assist | TechniqueEngine::decide(explicitOut); AutoArticulator::decorate | n/a | AutoArticulation.explicitTechniquesWin | |
| Zero added latency; getLatencySamples unchanged | 0.3 | in progress on claude/luthier-feat-assist | chord window untouched | n/a | AutoArticulationEngine.latencyDoesNotDependOnAssist | Slide rule waits up to 25-50 ms for the source note's release (decision D1) |
| Deterministic, block-size independent, sample-accurate | 0.4 | in progress on claude/luthier-feat-assist | NoteRecord stamps | n/a | AutoArticulation.decisionsDoNotDependOnTheBlockSize | |
| Never emits harmonics/Tap/SlideGuitar/MutedPick or arms techniques | 0.5 | in progress on claude/luthier-feat-assist | decorate | n/a | AutoArticulation.neverAHarmonicTapSlideGuitarOrMutedPick | |
| No allocation/locks; SPSC POD feed | 0.6 | in progress on claude/luthier-feat-assist | AutoArticulationFeed.h | n/a | AutoArticulationEngine.noAllocationInProcessBlockInAnyStyle | |
| Decisions visible and captured | 0.7 | in progress on claude/luthier-feat-assist | feed + capture marks | yes (branch) | AA-32, AA-39 tests | |
| Style table (8 styles x 8 columns), Amount scaling, chain caps | 2 | in progress on claude/luthier-feat-assist | Source/Model/Playing/AutoArticulationStyles.cpp:kStyles | yes (branch) | AA-08, AA-11, AA-14, AA-17 tests | Only legato IOI and chug IOI scaled by Amount (D3) |
| Styles on any family (bass style on guitar, guitar style on bass) | 2 | in progress on claude/luthier-feat-assist | startVibratoCandidate, planRollOrTogether | n/a | Combo.performanceAssistAcrossContexts | |
| Factory bass presets enabling assist use Bass style | 2 | in progress on claude/luthier-feat-assist | Source/Presets/FactoryPresets.cpp | n/a | - | |
| Rule bitmask order (position..palm mute), stamped decisions | 3 | in progress on claude/luthier-feat-assist | AutoArticulator | n/a | rule tests | |
| Position: cost terms, ties, hand box, chords via setPreferredPosition, late join | 3.1 | in progress on claude/luthier-feat-assist | AutoArticulator::planSingle, setHandPositionFromVoicing | n/a | AutoArticulation.aScaleStaysInOneBox, positionTiesAreDeterministic, aLateJoinLandsOnAFreeStringAtItsOwnTime | |
| Legato hammer-on/pull-off, hand-over, fretless -> slide | 3.2 | in progress on claude/luthier-feat-assist | resolveLegato, assistNoteOff | n/a | AutoArticulation.legatoPairIsHammerOnAndPullOff, pollyHandOverHasNoReleaseBeforeTheHammerOn | |
| Slide by overlap; 4-7 st position shift | 3.3 | in progress on claude/luthier-feat-assist | planSingle + assistResolvePending | n/a | AutoArticulation.longOverlapIsASlideAndAGapIsAPluck | |
| Delayed vibrato (lead only, 300 ms ramp, +-4% rate, max with CC, 150 ms ramp-out) | 3.4 | in progress on claude/luthier-feat-assist | autoVibratoCents; LuthierEngine::assistPerBlockCents | n/a | AutoArticulation.delayedVibratoOnAHeldBluesNote, aHeldChordGetsNoVibrato | |
| Attack accent/soft on Excitation::Params, velocity unchanged | 3.5 | in progress on claude/luthier-feat-assist | decorate; triggerNote | n/a | AutoArticulation.anAccentBrightensWithoutChangingVelocity | |
| Palm mute (register, chug, repeated pitch, Metal first note) and mute lift | 3.6 | in progress on claude/luthier-feat-assist | decorate; LuthierEngine::assistFireLift (kDampingLift) | n/a | AutoArticulation.rockChugsArePalmMuted, AutoArticulationEngine.anAutoMuteLiftsWhenHeld | |
| Alternate picking (grid / toggle / reset), up-stroke scaling, bass fingers exempt | 3.7 | in progress on claude/luthier-feat-assist | decorate | n/a | AutoArticulation.alternatePickingFollowsTheGridOrToggles | |
| Chord strum direction/speed/up-stroke force, Fingerstyle roll, Bass together, CC76/77 win | 3.7 | in progress on claude/luthier-feat-assist | planStrum, assistPlanStrum | n/a | chordsStrumDownOnTheBeatAndUpOffIt, metalDownstrokesAndFingerstylePinchRoll, rolledChordsKeepTheirTimingAndTheStrumControllerWins | |
| Ornaments: bend-into, slide-in, fall | 3.8 | in progress on claude/luthier-feat-assist | planSingle, onNoteOff, autoPitchCents | n/a | AutoArticulation.bendIntoStartsBelowAndRisesToPitch, aFallDefersTheReleaseAndGlidesDown | |
| New files AutoArticulator / Styles / Feed and API | 4.1 | in progress on claude/luthier-feat-assist | Source/Model/Playing/AutoArticulator.{h,cpp} | n/a | build | |
| NoteOnEvent fields (autoRules, attack scales, palmMuteAmount, upStroke, autoOrnament, arrivalSample) | 4.2 | in progress on claude/luthier-feat-assist | PlayingEvents.h | n/a | rule tests | explicitArticulation also added (duplicate of feat-riffs field) |
| TechniqueEngine setLegatoInferenceEnabled, decide(explicitOut) | 4.2 | in progress on claude/luthier-feat-assist | TechniqueEngine.{h,cpp} | n/a | AA-04, AA-24 tests | |
| MidiInterpreter hooks (Mono, Poly single/late join, strum, decorate, note-off) | 4.2 | in progress on claude/luthier-feat-assist | MidiInterpreterAssist.cpp | n/a | rule tests | |
| LuthierEngine: ExplicitContext, triggerNote fields, feed + capture, dampingLift event, per-block curves | 4.2 | in progress on claude/luthier-feat-assist | Source/LuthierEngineAssist.cpp | n/a | AA-15, AA-28, AA-32 tests | |
| ParameterBridge fast-table read; Free resolved to effective values | 4.2 | in progress on claude/luthier-feat-assist | Parameters::effectiveAssistSettings | n/a | AutoArticulationEngine.freePlaysMetalAsRockAndKeepsMetal | |
| Explicit table: GC/MPE bypass, rhythm driving, controller techniques, slap/scrape/Tap armed, mute grid, CC/AT vibrato, strum CCs, Luthier import pre-articulated | 5 | in progress on claude/luthier-feat-assist | AssistExplicitContext, assistSetContext | n/a | AA-24..AA-27, AA-33 tests | Tap-armed and mute-grid sources filled false (TECHNIQUES layer not merged, D6) |
| Four parameters appended, defaults, automatable | 6 | in progress on claude/luthier-feat-assist | Parameters.cpp FEAT-ASSIST block | yes (branch) | AutoArticulationEngine.theFourParametersAreLastAndInOrder | |
| Morph: enabled/style/rules switch at 50%, amount interpolates | 6 | in progress on claude/luthier-feat-assist | AssistRulesParameter::isDiscrete | n/a | AutoArticulationEngine.presetSnapshotAndHostStateKeepTheFourValues | |
| Easy AUTO pill (56x22, flash, tap, hold/Enter+Down popover with Amount) + style combo | 7.1 | in progress on claude/luthier-feat-assist | Source/UI/PerformanceAssistUi.cpp:AssistPill, AssistStyleBox | yes (branch) | AutoArticulationUi.easyPillStyleAndPopover | Popover is a CallOutBox child (D10) |
| Advanced RHYTHM tab PLAYING group first (mode mirror, switch, style, amount, 9 rule switches, recent list, notice, collapsible) | 7.2 | in progress on claude/luthier-feat-assist | PerformanceAssistGroup; RhythmPanel | yes (branch) | AutoArticulationUi.advancedPlayingGroupComesFirstAndWorks | Spec file name PerformanceAssistGroup.h/.cpp not used (lives in PerformanceAssistUi) |
| TECHNIQUES tab CASCADE read-only "Auto" row | 7.2 | no | - | no | - | TECHNIQUES tab is on claude/luthier-techniques; neither branch adds the row |
| "Show what it did" labels on both fretboards (glyphs, 600 ms fade, reduced motion, strum arrow) | 7.3 | in progress on claude/luthier-feat-assist | AssistLabelOverlay in FretboardComponent / GuitarBodyComponent | yes (branch) | AutoArticulationUi.labelsAppearFadeAndObeyTheOption | |
| Options "Show Performance Assist labels" in Visual aids (UiPreferences, default on) | 7.4 | in progress on claude/luthier-feat-assist | AppearancePage::assistLabelsToggle | yes (branch) | AutoArticulationUi.labelsAppearFadeAndObeyTheOption | Sits at foot of Appearance, not in the (now merged) VISUAL AIDS section |
| `A` shortcut rebindable, help topic, empty list text, locked-style upsell, no errors | 7.5 | in progress on claude/luthier-feat-assist | Accessibility.cpp toggleAssist; HelpContent performance-assist | yes (branch) | AutoArticulationUi.keyboardAndScreenReader | Upsell is a one-line notice (no upsell panel exists) |
| Feature-to-location index row | 7.6 | in progress on claude/luthier-feat-assist | - | n/a | - | |
| State: parameters in preset/snapshot/host; old preset loads off; UiState playingGroupCollapsed | 8 | in progress on claude/luthier-feat-assist | PresetManager::fromVar default fill; UiState | n/a | AutoArticulationEngine.presetSnapshotAndHostStateKeepTheFourValues | |
| Undo wording and classes (toggle/rule 3.3, style 3.2, amount 3.1) | 8 | in progress on claude/luthier-feat-assist | AssistUi::setEnabled/setRule | yes (branch) | AutoArticulationUi.undoEntries | |
| Accessibility: pill announcement, rule labels, list rows, tab order | 8 | in progress on claude/luthier-feat-assist | AssistPill, getNameForRow | yes (branch) | AutoArticulationUi.keyboardAndScreenReader | |
| Capture marks (palm mute, vibrato, accent, bend-into, slideOut/In, pick strokes) | 9 | in progress on claude/luthier-feat-assist | PerformanceCapture::mark | n/a | AutoArticulationEngine.anAssistedPhraseExportsWithItsTechniques | |
| ScoreTechnique pickStrokeUp/Down -> MusicXML up-bow/down-bow, GP pickstroke | 9 | in progress on claude/luthier-feat-assist | PerformanceScore.h, NotationExport.cpp | n/a | AA-32 test | |
| CapturedNote::autoRules; Luthier NOTE `aa=<hex>` | 9 | in progress on claude/luthier-feat-assist | MidiPerformance.cpp | n/a | AA-32, AA-33 tests | |
| NOTATION tab / live TAB draw automatic techniques in secondary accent | 9 | no | - | no | - | Data carried (ScoreNote::autoRules), drawing not done (D9) |
| Performance budget (+0 latency, < 3 us per note-on) | 10 | in progress on claude/luthier-feat-assist | - | n/a | AutoArticulationEngine.assistCostsAlmostNothing | Bound relaxed to +3% render time (D4) |
| Editions: Free styles, nearest-style mapping, aa_rules non-automatable " (Pro)" | 11 | in progress on claude/luthier-feat-assist | Source/Support/Edition.{h,cpp} | n/a | AutoArticulationEngine.freePlaysMetalAsRockAndKeepsMetal | Runtime flag only; no Free build target |
| Interactions: humanize after, strum dynamics untouched, rhythm engine, Tune direct notes, Slide Mode | 12 | in progress on claude/luthier-feat-assist | assistSetContext | n/a | AA-26, AA-42 tests | |
| Piano-roll "Show fingering" ghost dots use planSingle / H (PR-05) | 12 | no | - | no | - | Deferred on the branch |
| Failure modes (unplayable fallback, queue, contradictions, fall cancel) | 13 | in progress on claude/luthier-feat-assist | planSingle | n/a | AA-22, AA-23 tests | |
| AA-01 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.offAmountZeroAndNoRulesAreTheSameAsNothing | |
| AA-02 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.assistDrawsNothingFromTheInterpretersRandom | |
| AA-03 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.latencyDoesNotDependOnAssist | |
| AA-04 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.legatoPairIsHammerOnAndPullOff | |
| AA-05 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.longOverlapIsASlideAndAGapIsAPluck | |
| AA-06 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.aHarderNoteIsRePicked | |
| AA-07 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.pollyHandOverHasNoReleaseBeforeTheHammerOn | |
| AA-08 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.theLegatoChainCapRePicks | |
| AA-09 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.aScaleStaysInOneBox | |
| AA-10 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.positionTiesAreDeterministic | |
| AA-11 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.delayedVibratoOnAHeldBluesNote | |
| AA-12 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.aHeldChordGetsNoVibrato | |
| AA-13 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.anAccentBrightensWithoutChangingVelocity | |
| AA-14 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.rockChugsArePalmMuted | |
| AA-15 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.anAutoMuteLiftsWhenHeld | |
| AA-16 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.alternatePickingFollowsTheGridOrToggles | |
| AA-17 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.chordsStrumDownOnTheBeatAndUpOffIt | |
| AA-18 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.metalDownstrokesAndFingerstylePinchRoll | |
| AA-19 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.rolledChordsKeepTheirTimingAndTheStrumControllerWins | |
| AA-20 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.aLateJoinLandsOnAFreeStringAtItsOwnTime | |
| AA-21 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.bendIntoStartsBelowAndRisesToPitch | |
| AA-22 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.aFallDefersTheReleaseAndGlidesDown | |
| AA-23 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.neverAHarmonicTapSlideGuitarOrMutedPick | |
| AA-24 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.explicitTechniquesWin | |
| AA-25 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.guitarControllerAndMpeAreBypassed | |
| AA-26 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.rhythmDrivenPassIsNotAssisted | |
| AA-27 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.slapZoneAndTapArmedAreNotDecorated | Tap armed simulated; no product source for it |
| AA-28 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulation.decisionsDoNotDependOnTheBlockSize, AutoArticulationEngine.theFeedDoesNotDependOnTheBlockSize | |
| AA-29 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.realtimeAndOfflineRendersAreIdentical | |
| AA-30 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.noAllocationInProcessBlockInAnyStyle | |
| AA-31 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.assistCostsAlmostNothing | Relaxed bound |
| AA-32 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.anAssistedPhraseExportsWithItsTechniques | |
| AA-33 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.aLuthierRoundTripIsNotArticulatedTwice | |
| AA-34 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.theFourParametersAreLastAndInOrder | Must be re-based after FEAT-JAM (+34) on integration |
| AA-35 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.presetSnapshotAndHostStateKeepTheFourValues | |
| AA-36 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationEngine.freePlaysMetalAsRockAndKeepsMetal | Runtime Edition flag, not a Free build |
| AA-37 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationUi.easyPillStyleAndPopover | |
| AA-38 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationUi.advancedPlayingGroupComesFirstAndWorks | |
| AA-39 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationUi.labelsAppearFadeAndObeyTheOption | |
| AA-40 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationUi.keyboardAndScreenReader | |
| AA-41 | 14 | in progress on claude/luthier-feat-assist | - | - | AutoArticulationUi.undoEntries | |
| AA-42 | 14 | in progress on claude/luthier-feat-assist | - | - | Combo.performanceAssistAcrossContexts | |


### cpu-quality-modes.md

**Summary:** 83 requirements. **66 yes / 16 partial / 1 no**.
- Done (merged FEAT-CPU): QualityProfile table, oversampling caps with constant latency and click-free switching, IR truncation variants, modal/room/noise/mod-rate/dispersion reductions, idle-string sleep and ring-out, offline-at-High, Auto controller, `performance.json`, per-instance override, AnimationPolicy with a 38-class registry, governor E1/E2/E3, Options AUDIO QUALITY group, footer QualityBadge, all CQ-01..CQ-32 tests present.
- Open: animated strings do not follow AnimationPolicy (StringAnimator unregistered, reads Reduced motion only), so both CQ-22 tests and CQ-23 with strings on are at risk on the integration branch.
- Open: preset-browser previews at High (FEAT-BROWSER unmerged), no Free build (CQ-29 is a source scan), no lock trap (CQ-20 allocations only).
- Relaxed thresholds: CQ-12/13 ratio gates (IR truncation never applies to factory IRs), CQ-15 amp bound (-60 dBc unattainable, measured against 4x).

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Mode never stored in preset/snapshot/guitar/tune/setlist | 0.1, 3 | yes | Source/Support/PerformanceSettings; UiState::qualityOverride | n/a | CpuQuality.CQ03_theLevelIsNeverInAPreset | |
| Never change pitch (0.1 c), timing, or reported latency | 0.2 | yes | StringEngine cappedCompensation; LatencyPad | n/a | CQ06, CQ07, CQ08, CQ09 | |
| Bounded audible cost; presets within 0.5 dB loudness | 0.3 | yes | QualityProfile | n/a | CpuQuality.CQ12_everyFactoryPresetAtEveryLevel | |
| Final renders High (option default on) | 0.4, 2.6 | yes | QualityController::getEffectiveLevel; LuthierAudioProcessor::setNonRealtime | yes (Options AUDIO toggle) | CQ14, CQ32 | |
| Identity rules (coupling on, body never bypassed, outputs never silenced) | 0.5 | yes | QualityProfile | n/a | CpuQuality.CQ30_perStringOutputsAndSympatheticRing | |
| Real-time safe switching (prepared alternatives, audio thread selects/crossfades) | 0.6 | yes | LuthierEngine::applyQuality | n/a | CpuQuality.CQ20_switchingEveryHundredMsNeverAllocates | |
| QualityProfile single definition, table 2.1 | 2.1 | yes | Source/Support/QualityProfile.h:forLevel | n/a | CQ01_profileTableMatchesTheSpec, CQ01_noLiteralCapsOutsideTheProfile | |
| Amp and drive oversampling caps (effective = min(nominal, cap)) | 2.1, 2.2 | yes | QualityProfile::capFactor; AmpEngine::setOversamplingFactor(eff, nom) | n/a | CQ05_effectiveFactorIsTheCappedNominal | |
| Dispersion stage cap latched at excite() | 2.1, 2.4 | yes | StringEngine::setDispersionRule, latchDispersion | n/a | CQ08, CQ10 | |
| Body IR 1.5 s / 0.75 s, cabinet IR 250/120 ms, -40 dB tail rule, 50 ms fade, same latency, 20 ms switch fade | 2.1, 2.3 | yes | Source/DSP/Common/IrVariants.{h,cpp}; BodyEngine, CabinetEngine | n/a | CQ16_truncationObeysTheTailRuleAndKeepsLatency | Factory IRs shorter than every cap, so no effect on factory content |
| Body modal bank 48/32/20, 8 lowest kept, 20 ms ramp | 2.1 | yes | BodyEngine::setQualityLevel | n/a | CQ16_modalCapKeepsTheLowestModesAndNullsTheBody | |
| Room ER taps 16/16/8 energy-compensated | 2.1 | yes | RoomEngine::setTapCount | n/a | CQ12 | Same-delay taps merged first |
| Noise pools halved at Low | 2.1 | yes | NoiseEngine::setDegraded via applyQuality | n/a | CpuQuality.noisePoolsHalveAtLowAndNeverUnderLoadAtHigh | |
| Mod-matrix interval x2 at Low unless LFO > 20 Hz | 2.1 | yes | ModMatrix::setControlIntervalMultiplier | n/a | CQ12, CQ20 | |
| Idle-string sleep (Medium/Low) keeping coupling | 2.1, 2.4 | yes | StringEngine::goToSleep / wake | n/a | CQ30_perStringOutputsAndSympatheticRing | |
| Ring-out truncation -80/-60 dB, max 8 at Low, exemptions | 2.1, 2.4 | yes | LuthierEngine::qualityPerBlock; StringEngine::fadeToSleep | n/a | CQ30_drivenStringsAreNeverTruncatedOrSlept | |
| UI motion Full/Limited/Off and live readouts <= 10 Hz at Low | 2.1, 6 | partial | Source/UI/AnimationPolicy.{h,cpp} | n/a | CQ21, CQ23 | Animated strings not driven by the policy (see Implemented? of the StringAnimator row below) |
| Latency never follows the mode; Oversampler::latencyFor; LatencyPad | 2.2 | yes | Oversampler::latencyFor; LatencyPad | n/a | CQ06_latencyNeverFollowsTheLevel, CQ07 | |
| Click-free factor change (twin state, 32-sample history, 10 ms crossfade) | 2.2 | yes | AmpEngine twin; PedalsDrive; Oversampler::InputHistory | n/a | CQ11_switchingLevelsDoesNotClick | |
| Switching via one atomic, applyQuality; hard switch in prepare/reset/silence | 2.5 | yes | LuthierAudioProcessor::applyQualityForBlock; LuthierEngine::applyQuality | n/a | CQ11, CQ14, CQ20 | |
| Offline: Auto always High; setNonRealtime override; AudioExporter sets non-realtime | 2.6 | yes | LuthierAudioProcessor::setNonRealtime; AudioExporter, TuneExport | n/a | CQ14, CQ32 | |
| Preset-browser previews, audition phrases and SpectrumDelta render at High | 2.6 | partial | SpectrumDelta builds its own engine (High) | n/a | - | Audition phrases play at the live level (D18); preset previews not on integration (claude/luthier-feat-browser) |
| Auto: step down/up rules, dwell, 30 s, hold after 3 in 10 min, start level | 2.7 | yes | Source/Support/QualityController.cpp:tick | yes (Auto pill) | CQ17_autoStepsDownUpAndHoldsByTheRules | |
| Auto notices: banner <= 1 per 60 s, switchable, polite announcements | 2.7 | yes | QualityEditorLink::update | yes | CQ18, CQ27 | |
| `performance.json` fields/defaults, temp-and-rename, broadcast, re-read on prepare/open | 3 | yes | Source/Support/PerformanceSettings.{h,cpp} | n/a | CQ02, CQ28 | |
| Per-instance override in uiState, never in presets | 3 | yes | UiState::qualityOverride | yes (override combo) | CQ04_perInstanceOverrideRoundTripsAndFollowsTheRules | |
| No parameter added; oversample parameter unchanged | 3 | yes | - | yes (existing controls) | CQ03 | |
| CpuLoadMonitor (own share, 200 ms/2 s/20 s, p95) | 4 | yes | Source/Support/CpuLoadMonitor.h | n/a | CQ19_loadMonitorMeansAndP95 | |
| Insertion points (applyQuality fan-out, applyStructural passes nominal, getAnimationMs delegates) | 4 | yes | LuthierEngine::applyQuality; AccessibilitySettings::animationMsHook | n/a | CQ21 | |
| Options -> AUDIO QUALITY: pills, override combo, Now running + load bar, two toggles, oversampling note, disclosure | 5 | yes | Source/UI/QualityOptions.{h,cpp} in AudioPage | yes | CpuQualityUi.CQ26_audioPageBadgeAndAppearanceNote | |
| Oversampling note as tooltip of Advanced col 3 Master oversampling | 5 | yes | AdvancedPanel::setOversamplingNote | yes | CQ26_masterOversamplingTooltipCarriesTheCap | |
| AppearancePage note at Low | 5 | yes | AppearancePage::lowMotionNote | yes | CQ26 | |
| DIAGNOSTICS emergency_string_drop toggle; audio-path view and debug window lines | 5 | partial | DiagnosticsPage::emergencyDropToggle; AudioPathView; QualityDiagnostics::describe | yes | CQ26, theDiagnosticsOptOutIsSavedAndReachesE3 | IR lengths and drive factors only in the debug window, not "What's on the audio path" (D12) |
| Footer QualityBadge (labels, zones + glyph, click/Enter/Space -> AUDIO, tooltip, 4 Hz, stale "-") | 5 | yes | Source/UI/QualityBadge.{h,cpp} | yes (Easy and Advanced footer) | CQ26, CQ27 | |
| "Cycle CPU quality" rebindable, unbound | 5 | yes | Accessibility.cpp cycleCpuQuality; PluginEditor performs | yes | CQ27_groupKeysNamesOrderAnnouncementsAndShortcut | |
| Empty/error states ("Not playing yet", held Auto text) | 5 | yes | QualityOptions::Status, QualityStrings | yes | CQ26 | |
| AnimationPolicy API (getMotion, mayAnimate, frameRateHz, transitionMs, setReliefLevel, listeners, Registration, notePaint) | 6 | yes | Source/UI/AnimationPolicy.h | n/a | CQ21_policyTruthTable | |
| Registry: every animated component registered or allow-listed; no timer at Off | 6 | partial | AnimationPolicy::Registration in 38 classes; getPollOnlyAllowList | n/a | CQ22_everyTimerDrivenUiClassIsRegisteredOrAllowListed, CQ22_builtEditorsRegisterEveryTableClassThatExists | `StringAnimator` (Source/UI/Guitar/StringAnimator.cpp, startTimerHz + VBlankAttachment) arrived with FEAT-STRINGS and has neither a Registration nor an allow-list entry; both CQ-22 tests are expected to fail on the integration branch |
| Behaviour at Off per table (static glow, stepped readouts, no ballistics, instant transitions) | 6 | partial | GuitarBodyComponent::staticRefresh, FretboardComponent static mode, LevelMeter, VuMeter, etc. | n/a | CQ23_lowMeansNoAnimationRepaints, CQ24_transitionsAreInstantAtLow | Illustrations go static, but their StringAnimator's own vblank/timer is not stopped by CPU Low (only by Reduced motion) |
| Animated strings forced Low style at Medium/relief 1, Off at Low | 6 | partial | AnimationPolicy::getStringsStyle exists | n/a | CQ21 | Nothing in Source/UI/Guitar calls getStringsStyle |
| Governor E1/E2 (relief 1/2, data stream suspended, shadow audition frozen) | 7 | yes | QualityController::tick; LuthierAudioProcessor::auditionGuitar | n/a | CQ19_governorEntersAndLeavesAtItsThresholds, governorReliefSuspendsTheStreamAndFreezesTheAudition | |
| Governor E3 on audio thread: least-recent string 10 ms fade, banner, opt-out, not offline | 7 | yes | LuthierAudioProcessor::stampBlockLoad; LuthierEngine::dropLeastRecentString | yes (opt-out in DIAGNOSTICS) | CQ19_emergencyDropFadesOneStringWithItsBanner | |
| StringAnimator::setReliefLevel fed only through AnimationPolicy | 7 | no | - | n/a | - | No caller in the product |
| performance-budget 8 CpuRelief ladder superseded and removed | 7 | yes | Support/CpuRelief removed | n/a | ported tests | |
| CPU targets and ratio gates (Medium <= 0.85x, Low <= 0.70x) | 8 | partial | the reductions | n/a | CQ12, CQ13_scenarioBudgets | Gates relaxed (Low <= 0.80x summed, per-scenario 0.88x); absolute budgets only with LUTHIER_MID_CLASS_RUNNER |
| Not undoable | 9 | yes | - | n/a | - | |
| Accessibility: radio group "CPU quality", arrows, descriptions, badge last in footer tab order, announcements <= 1/5 s, catalogue strings | 9 | yes | QualityOptions::Group; QualityBadge::createAccessibilityHandler; Source/Accessibility/QualityStrings | yes | CQ27 | |
| Edition: both, identical | 9 | partial | no edition code | n/a | CQ29_noEditionGatesTheQualityModes | Source scan only; no Free build |
| Interactions (presets/snapshots/Reset, timing, Workshop SpectrumDelta, tone match full IRs, routing, multi-instance) | 10 | yes | as above | n/a | CQ03, CQ09, CQ30, CQ28 | |
| Failure modes (json corrupt, IR build fail, host flips non-realtime, oscillation, blocked message thread) | 11 | yes | PerformanceSettings, IrVariants, QualityController | n/a | CQ02, CQ17, CQ19 | |
| Coordinator follow-ups in other specs | follow-ups | yes | spec edits marked (FEAT-CPU) | n/a | - | done per FEAT-CPU doc |
| CQ-01 | 12 | yes | - | - | CpuQuality.CQ01_profileTableMatchesTheSpec, CQ01_noLiteralCapsOutsideTheProfile | |
| CQ-02 | 12 | yes | - | - | CpuQuality.CQ02_settingsFileRoundTripsAndFallsBackToDefaults | |
| CQ-03 | 12 | yes | - | - | CpuQuality.CQ03_theLevelIsNeverInAPreset | |
| CQ-04 | 12 | yes | - | - | CpuQuality.CQ04_perInstanceOverrideRoundTripsAndFollowsTheRules | |
| CQ-05 | 12 | yes | - | - | CpuQuality.CQ05_effectiveFactorIsTheCappedNominal | |
| CQ-06 | 12 | yes | - | - | CpuQuality.CQ06_latencyNeverFollowsTheLevel | |
| CQ-07 | 12 | yes | - | - | CpuQuality.CQ07_anImpulseArrivesWhereItDidAtHigh | |
| CQ-08 | 12 | yes | - | - | CpuQuality.CQ08_fundamentalAndTenthPartialHoldAtEveryLevel | |
| CQ-09 | 12 | yes | - | - | CpuQuality.CQ09_onsetsAreSampleIdenticalAcrossLevels | |
| CQ-10 | 12 | yes | - | - | CpuQuality.CQ10_aRingingNoteKeepsItsStagesUntilReExcited | |
| CQ-11 | 12 | yes | - | - | CpuQuality.CQ11_switchingLevelsDoesNotClick | |
| CQ-12 | 12 | partial | - | - | CpuQuality.CQ12_everyFactoryPresetAtEveryLevel, CQ12_aMidRenderSwitchPassesTheClickCriterion | Relaxed ratio gates |
| CQ-13 | 12 | partial | - | - | CpuQuality.CQ13_scenarioBudgets | Relaxed ratios; measured table placeholders (MEDIUM_X, SCENARIO_TABLE) not filled in the coverage doc |
| CQ-14 | 12 | yes | - | - | CpuQuality.CQ14_offlineRendersAreHighAndDeterministic | |
| CQ-15 | 12 | partial | - | - | CpuQuality.CQ15_aliasingStaysWithinItsBounds | Amp bound relative to 4x, not -60 dBc |
| CQ-16 | 12 | yes | - | - | CQ16_truncationObeysTheTailRuleAndKeepsLatency, CQ16_modalCapKeepsTheLowestModesAndNullsTheBody | |
| CQ-17 | 12 | yes | - | - | CpuQuality.CQ17_autoStepsDownUpAndHoldsByTheRules | |
| CQ-18 | 12 | yes | - | - | CpuQuality.CQ18_oneBannerPerDownStepAtMostOncePerMinute | |
| CQ-19 | 12 | yes | - | - | CQ19_governorEntersAndLeavesAtItsThresholds, CQ19_emergencyDropFadesOneStringWithItsBanner, CQ19_loadMonitorMeansAndP95 | |
| CQ-20 | 12 | partial | - | - | CpuQuality.CQ20_switchingEveryHundredMsNeverAllocates | Allocation trap only; no lock trap exists |
| CQ-21 | 12 | yes | - | - | CpuQualityUi.CQ21_policyTruthTable | |
| CQ-22 | 12 | partial | - | - | CpuQualityUi.CQ22_everyTimerDrivenUiClassIsRegisteredOrAllowListed, CQ22_builtEditorsRegisterEveryTableClassThatExists | Tests exist; StringAnimator gap should make them fail after the FEAT-STRINGS merge (not run: build forbidden) |
| CQ-23 | 12 | partial | - | - | CpuQualityUi.CQ23_lowMeansNoAnimationRepaints | Written before animated strings merged; the animator's own timer at Low is not stopped |
| CQ-24 | 12 | yes | - | - | CpuQualityUi.CQ24_transitionsAreInstantAtLow | |
| CQ-25 | 12 | yes | - | - | CpuQualityUi.CQ25_reducedMotionAndLowStayIndependent | |
| CQ-26 | 12 | yes | - | - | CpuQualityUi.CQ26_audioPageBadgeAndAppearanceNote, CQ26_masterOversamplingTooltipCarriesTheCap | |
| CQ-27 | 12 | yes | - | - | CpuQualityUi.CQ27_groupKeysNamesOrderAnnouncementsAndShortcut | |
| CQ-28 | 12 | yes | - | - | CpuQuality.CQ28_aGlobalChangeReachesEveryInstanceButOverrides | |
| CQ-29 | 12 | partial | - | - | CpuQuality.CQ29_noEditionGatesTheQualityModes | No Free configuration |
| CQ-30 | 12 | yes | - | - | CQ30_drivenStringsAreNeverTruncatedOrSlept, CQ30_perStringOutputsAndSympatheticRing, CQ30_rhythmTuneSnapshotsAndMidiOutAreLevelIndependent | |
| CQ-31 | 12 | yes | - | - | CpuQuality.CQ31_irVariantsAddAtMost12MB | Measured with mallinfo2 heap |
| CQ-32 | 12 | partial | - | - | CpuQuality.CQ32_offlineRendersMatchWhateverTheLiveLevel | No golden WAV set exists; checks offline equality instead |


### global-search.md

**Summary:** 81 requirements. **0 yes / 0 partial / 7 no** (plus 74 in progress on claude/luthier-feat-search).
- Nothing is on the integration branch: the whole palette (Source/UI/Search, 47 files, ~11 700 lines: index, matcher, 12+ providers, ActionRegistry / performAction refactor, navigator + highlighter, inline values, Options group, header magnifier, Ctrl+K) is in progress on `claude/luthier-feat-search` with GS-01..GS-45 tests written there.
- Open on the branch: first-run empty-state hint (onboarding), popover-only control routes untested under xvfb, curated synonyms only for main controls, upsell panel (no editions), GS-02 tolerates the ~57 parameters that GuiReach already reports as having no control.
- Sibling-provider contract (section 8) not met by anything on integration: no jam commands/places registered, no riff provider, no "Reset mic placement", no "Toggle auto-articulation" ActionDef (all depend on unmerged branches).

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| UI only: no parameters, no audio code, nothing in presets | 0.1, 9 | in progress on claude/luthier-feat-search | Source/UI/Search | n/a | SearchEditor.GS30_openCloseFocusAndState, GS40_noAudioThreadEffect | |
| One action path: keyPressed -> performAction; buttons call it | 0.2, 4.3 | in progress on claude/luthier-feat-search | LuthierAudioProcessorEditor::performAction | n/a | GS04, GS45 | |
| Index from existing registries (APVTS, tabs, Options pages, shortcuts, HelpContent, PresetManager, PartLibrary) | 0.3 | in progress on claude/luthier-feat-search | SearchProviders.cpp | n/a | GS01..GS06 | |
| Navigation wins ties; value only with explicit value + Enter | 0.4 | in progress on claude/luthier-feat-search | InlineValue::read | n/a | Search.GS23_navigationReadings | |
| Keyboard first, announced | 0.5 | in progress on claude/luthier-feat-search | CommandPalette::handleKey | yes (branch) | GS36, keyboardAndLiveRows | |
| Indexed kinds: param, opt, place, cmd, key, preset, guitar, part, pedal, snapshot, help, setting, provider | 2 | in progress on claude/luthier-feat-search | ParameterProvider ... SettingProvider, GenreKitProvider, TuneProvider | n/a | GS01, GS03..GS06, GS44 | Pedal slot type choices use the `pedal:` kind instead of `opt:` (D13) |
| Keywords: English title x0.8, `search.syn.*` synonyms, units spelled out, breadcrumb | 2 | in progress on claude/luthier-feat-search | SearchMatcher::scoreItem; SearchCatalog.cpp | n/a | Search.GS15_germanLocaleAndDiacritics, GS11 | |
| Curated English synonyms for every parameter and place | 2 | in progress on claude/luthier-feat-search | SearchCatalog.cpp | n/a | GS11 | Partial on branch: main controls and places only; other params keyed by id/unit words (D6) |
| Pedal slot titles from fitted pedal; empty slots hidden by default | 2 | in progress on claude/luthier-feat-search | ParameterText::titleFor, isInertSlotParameter | n/a | GS01, GS02 | |
| Shared `ParameterVisibility.h` hidden list (GuiReach + index) | 2 | in progress on claude/luthier-feat-search | Source/UI/Search/ParameterVisibility.h | n/a | GS01 | |
| Classes in Source/UI/Search (SearchItem, Matcher, Index, Providers, ActionRegistry, ParameterLocations, LiveControls, Navigator, InlineValue, CommandPalette) | 3.1 | in progress on claude/luthier-feat-search | Source/UI/Search/* | n/a | build | |
| SearchIndex owned per editor, message thread only | 3.1 | in progress on claude/luthier-feat-search | LuthierAudioProcessorEditor::getSearch | n/a | - | |
| LearnTarget ctor/dtor registers with LiveControls | 3.2 | in progress on claude/luthier-feat-search | LiveControls.cpp | n/a | GS02 | |
| Place tags (`SearchAnchors::tag`) on tabs, columns, Easy strips, Options pages, drawer, overlays, groups | 3.2 | in progress on claude/luthier-feat-search | SearchAnchors; AdvancedPanel columns; navigator tags the rest | n/a | GS03 | |
| Popover-only controls via ParameterLocations rows | 3.2 | in progress on claude/luthier-feat-search | ParameterLocations.cpp | n/a | - | Untested: needs a desktop peer (D9) |
| Non-parameter settings `tagSetting` | 3.2 | in progress on claude/luthier-feat-search | SearchAnchors::tagSetting | n/a | entryPointsAndOptions | |
| UiLocation steps with named open functions | 3.3 | in progress on claude/luthier-feat-search | SearchNavigator::runStep | n/a | GS03 | |
| Availability values computed on demand | 3.4 | in progress on claude/luthier-feat-search | providers' availabilityOf | n/a | GS02, GS24, GS32 | |
| Normalisation shared with HelpContent::findTopic; tokens AND; score table; bonuses; cut 150; cap 50; ties; scopes/chips | 4.1 | in progress on claude/luthier-feat-search | SearchMatcher; HelpContent::normalise | yes (branch) | Search.GS10..GS16 | Whole-query synonym scores 600 (D2) |
| Navigation steps 1-9 (target choice, auto mode switch + notice, unavailable mode, context gates, dismiss overlay, scroll, focus, highlight, announce) | 4.2 | in progress on claude/luthier-feat-search | SearchNavigator | yes (branch) | GS02, GS31, GS32, GS37, GS43 | |
| ActionRegistry; commands without keys (Workshop, Options, browser, Help, Export, Import MIDI, Retune all, New tune, snapshots, arm techniques, Live Mode, drawer, clear recent) | 4.3 | in progress on claude/luthier-feat-search | ActionRegistry; registerActions; performExtendedAction | yes (branch) | GS04, GS45 | Per-technique arm commands only for slap/scrape (TECHNIQUES pills unmerged) |
| Inline value grammar (number+unit, relative, %, keywords, option text), two readings | 4.4 | in progress on claude/luthier-feat-search | InlineValue | yes (branch) | Search.GS20_inlineValues, GS23 | Unitless knobs use the 0-10 dial scale (D1) |
| Preview "Set to X (now Y)" + 44x8 slider; Enter/Shift+Enter/Ctrl+Enter/Alt+arrows/drag | 4.4 | in progress on claude/luthier-feat-search | CommandPalette::paintRow, nudge | yes (branch) | GS20, GS21, GS36 | |
| Apply via gestures; one 3.1 undo entry; nudges merge in 200 ms | 4.4 | in progress on claude/luthier-feat-search | setAsGesture | n/a | SearchEditor.GS21_inlineSetUndoAndGestures | |
| Clamp text; stock-locked edge; modulation base value; refusals | 4.4 | in progress on claude/luthier-feat-search | InlineValue::resolve; SearchNavigator::applyValue | n/a | Search.GS22_clamping, GS24 | |
| Recent items/queries in UiPreferences, pruned | 4.5 | in progress on claude/luthier-feat-search | RecentStore | n/a | GS13, GS34 | |
| Secondary actions (parameter rows = buildParameterContextMenu; other kinds) | 5 | in progress on claude/luthier-feat-search | CommandPalette::buildSecondaryMenu | yes (branch) | SearchEditor.GS38_mouse | |
| Header magnifier (three-dot menu below 1280 px) | 6.1 | in progress on claude/luthier-feat-search | HeaderBar::searchButton (MagnifierButton) | yes (branch) | GS30 | Below 1280 it is a File menu item |
| Ctrl/Cmd+K rebindable; again closes | 6.1 | in progress on claude/luthier-feat-search | Accessibility.cpp `search` | yes (branch) | GS30 | |
| HELP tab Search field opens with `?` scope | 6.1 | in progress on claude/luthier-feat-search | HelpSearchField in HelpTab | yes (branch) | entryPointsAndOptions | |
| First-run empty-state hint (InlineNotice) | 6.1 | no | - | no | - | Deferred on branch (D10) |
| Layer above OverlayHost, 40% scrim, cancels MIDI Learn, size/rows/empty/no-results/errors | 6.2 | in progress on claude/luthier-feat-search | CommandPalette | yes (branch) | GS34, GS35, GS38, keyboardAndLiveRows | |
| Keyboard and mouse table; field consumes keys; 200-char cap | 6.3 | in progress on claude/luthier-feat-search | CommandPalette::handleKey | yes (branch) | GS36, GS38 | |
| Options -> ACCESSIBILITY Search group (auto-switch, remember recent, clear) | 7 | in progress on claude/luthier-feat-search | SearchOptionsGroup in AccessibilityPage | yes (branch) | entryPointsAndOptions, GS33, GS34 | |
| Provider contract `SearchProvider`, `buildSearchProviders` | 8 | in progress on claude/luthier-feat-search | SearchProvider.h | n/a | Search.GS44_providerContract | |
| Jam mode registers commands (start/stop, key, style) and places | 8 | no | - | no | - | JAM is merged on integration but search is not; jam shortcuts would become commands automatically; JAM tab place row absent |
| Riff library `riff:` provider (Audition, Insert into tune) | 8 | no | - | no | - | Neither feat-riffs nor feat-search provides it |
| Preset browser replaces PresetProvider activation ("open browser at entry") | 8 | no | - | no | - | feat-browser exposes PresetBrowserPanel::selectPresetNamed; not wired |
| Mic placement place + "Reset mic placement" command | 8 | no | - | no | - | |
| Auto-articulation ActionDef "Toggle auto-articulation" | 8 | no | - | no | - | `toggleAssist` shortcut on feat-assist would become a command once both merge |
| Animated strings `set:` setting on its toggle | 8 | no | - | no | - | SettingProvider may discover it after merge; not verified |
| Undo classes: navigation not undoable; inline 3.1; commands keep theirs | 9 | in progress on claude/luthier-feat-search | ActionDef::undo | n/a | GS21, GS45 | |
| Edition split: locked rows, -100 rank, Enter opens upsell, Free ActionRegistry stand-ins | 10 | in progress on claude/luthier-feat-search | SearchNavigator::proLockPredicate | n/a | SearchEditor.GS39_proLocked | No upsell panel; states "Available in Luthier Pro" (D8) |
| Performance budget (query p95 <= 4 ms, build <= 30 ms, open <= 16 ms, navigate <= 100 ms, 4 MB) | 11 | in progress on claude/luthier-feat-search | precomputed masks | n/a | Search.GS41_performance | |
| Interactions (techniques, rhythm kits, tunes, MIDI export, snapshots, host automation, Workshop, Live Mode, preset load while open, multi-instance) | 12 | in progress on claude/luthier-feat-search | GenreKitProvider, TuneProvider | n/a | GS42, GS43, keyboardAndLiveRows | |
| Failure modes (navigate miss -> footer + ErrorLog, no longer exists, duplicates, resize) | 13 | in progress on claude/luthier-feat-search | SEARCH_NAVIGATE_MISS | n/a | GS06, GS44, GS02 | |
| Accessibility (dialog role, row titles, announcements at open and 400 ms, catalog keys, RTL) | 14 | in progress on claude/luthier-feat-search | CommandPalette::createAccessibilityHandler (dialogWindow) | yes (branch) | SearchEditor.GS36_accessibility | |
| GS-01 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS01_everyParameterIndexed | |
| GS-02 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS02_everyParameterNavigates | Reports rather than fails on params with no control (D11) |
| GS-03 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS03_everyPlaceIndexedAndReal | JAM tab (integration) not in its catalogue; will fail after merge until a row is added |
| GS-04 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS04_shortcutsAreCommands | |
| GS-05 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS05_helpTopicsAndAliases | Checked in `?` scope (D12) |
| GS-06 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS06_contentIndexedAndFresh | |
| GS-10 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS10_topResults | |
| GS-11 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS11_inTopThree | |
| GS-12 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS12_deterministicOrderAndTieBreaks | |
| GS-13 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS13_recentUseAndDecay | |
| GS-14 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS14_scopes | |
| GS-15 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS15_germanLocaleAndDiacritics | |
| GS-16 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS16_fuzz | |
| GS-20 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS20_inlineValues | |
| GS-21 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS21_inlineSetUndoAndGestures | |
| GS-22 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS22_clamping | |
| GS-23 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS23_navigationReadings | |
| GS-24 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS24_inlineSetRefusals | |
| GS-30 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS30_openCloseFocusAndState | Focus checked via requested focus (xvfb, D9) |
| GS-31 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS31_modeSwitching | |
| GS-32 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS32_advancedUnavailable | |
| GS-33 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS33_autoSwitchOffConfirms | |
| GS-34 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS34_emptyState | |
| GS-35 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS35_didYouMean | |
| GS-36 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS36_accessibility | |
| GS-37 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS37_reducedMotionHighlight | |
| GS-38 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS38_mouse | |
| GS-39 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS39_proLocked | Predicate-driven, no Free build |
| GS-40 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS40_noAudioThreadEffect | |
| GS-41 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS41_performance | |
| GS-42 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS42_presetLoadWhileOpen | |
| GS-43 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS43_workshopOverlay | |
| GS-44 | 15 | in progress on claude/luthier-feat-search | - | - | Search.GS44_providerContract | |
| GS-45 | 15 | in progress on claude/luthier-feat-search | - | - | SearchEditor.GS45_commandsMatchTheirButtons | |


### jam-mode.md

**Summary:** 108 requirements. **97 yes / 10 partial / 1 no**.
- Done (merged FEAT-JAM): JamEngine/conductor/chord follower/predictor, modal drum kit and waveguide bass, 10 factory styles, 34 `jam_*` parameters all with visible controls, JAM tab between TUNE and LIVE, Easy JAM group, Live pill, J / Shift+J / Alt+J, Aux 9/10 outputs, MIDI out, capture, drag-out, tune-export band, JM-01..JM-50 tests present (plus JM51 edition table).
- Open: PROG looper as a chord source, Export MIDI via the midi-export 4.1 dialog (a save chooser instead), predicted chords italic (dimmed instead), edition split only behind a never-defined `LUTHIER_FREE_EDITION`.
- Changed by the FEAT-CPU merge: the "CPU relief step between 4 and 5" (thin cymbals) now keys on CPU quality Low, not on the governor.
- Not registered with global search (feat-search unmerged; its static place list lacks JAM).

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| No samples: modal/stochastic drums, waveguide bass, stick count-in | 0.1 | yes | Source/DSP/Jam/ModalResonatorBank, DrumPieces, JamDrumKit, JamBassVoice | n/a | JamDsp.JM31_aJamReadsNoFiles, Jam.JM16_countInSticks | |
| Band belongs to the processor; never the guitar path; reads tune chords only | 0.2 | yes | LuthierAudioProcessor::jam; Source/Jam/JamProcessor.cpp | n/a | JamPlugin.JM38_loopsAreGuitarOnlyTakesHaveTheBandKillMutesIt | |
| Bass changes on musical boundary; drums never wait | 0.3 | yes | JamChordFollower | n/a | Jam.JM06..JM08 | |
| Real-time safe (atomic swaps, no locks/alloc/strings) | 0.4 | yes | JamEngine::setChordMap/collectGarbage, JamStatusChannel, JamCapture | n/a | JamDsp.JM33_noAllocationsAndNoLocks | No lock trap in repo; allocations checked |
| Deterministic; RtRandom per (seed, bar, lane) | 0.5 | yes | JamEngine::laneSeed | n/a | Jam.JM04_rendersAreDeterministicPerSeed, JM05_blockSizesNull | |
| No latency of its own; events at musical time + L | 0.6 | yes | JamEngine | n/a | Jam.JM02_rockKicksLandOnTheHostGrid | |
| Free when off; conductor only while armed | 0.7 | yes | processor skips jam when disabled | n/a | JamPlugin.JM34_offCostsNothingAndTheJamScenario | |
| Five states Off/Armed/Counting/Playing/Ending | 2 | yes | JamEngine | yes (pill) | JamPanel.JM48_easyGroupAndLivePill | |
| Start modes Auto/Host/First Note/Count-In/Tap In; START semantics; join host at bar | 2.1 | yes | JamEngine::handleCommands, startBand, tapAtSample | yes (`jam_start_mode`) | Jam.JM14, JM15, JM16 | |
| Stop: host stop, STOP ending, second STOP cut, stop on silence, ending, panic | 2.2 | yes | JamEngine::requestEnding, cut; LuthierAudioProcessor::panic | yes | Jam.JM17, JM18, JamPlugin.JM18_panicSetsJamPlayOff | |
| Tempo priority, meters (4/4, 3/4, 6/8, generic), double/half time, own clock drives rhythm engine | 2.3 | yes | JamConductor.h | n/a | Jam.JM03, JM22, JamPlugin.JM46 | |
| Chord sources Auto/Live/Tune; >= 2 pitch classes; Unknown keeps; slash chords; token resolution | 3.1 | yes | JamChordFollower; JamBassLine::resolve | yes (`jam_chord_source`) | Jam.JM09, JM10, JM13 | |
| PROG looper as an anticipated source (Auto) | 3.1, 11 | no | - | no | - | PROG looper has no playback engine emitting chords (Decision 5) |
| Follow quantum Tight/Natural/Relaxed/Bar and grace window | 3.2 | yes | JamChordFollower | yes (`jam_follow`) | Jam.JM06, JM07, JM08 | |
| Tune anticipation: JamChordMap (<= 1024, atomic swap), approach notes, section fill + jam hint | 3.3 | yes | JamChordMap::fromTimeline; TuneSession::onTimelineBuilt | n/a | Jam.JM11_tuneChangesLandExactlyWithApproaches | |
| Prediction from repeated cycles, correction, status italic | 3.3 | partial | JamPredictor | yes | Jam.JM12_predictionFromTheThirdCycle | Predicted chords dimmed + "(predicted)", not italic (no italic face) |
| Ten factory styles, A/B, intensities, fills, ending, double/half time, genre kit only when linked | 4.1 | yes | JamStyleLibrary::buildFactoryStyles | yes | Jam.JM01_everyFactoryStyleParsesAndIsComplete | |
| Intensity 1-5 and dynamics follow with hysteresis ("3 (+1)") | 4.2 | yes | JamEngine; JamUiText::statusLine | yes | Jam.JM21_dynamicsFollow | |
| Fill period, Fill Now, humanise, swing | 4.3 | yes | JamEngine scheduler | yes | Jam.JM20_fillNow | |
| Changes while playing land on beat/bar | 4.4 | yes | JamEngine latches | n/a | Jam.JM19_changesLandOnBeatsAndBars | |
| Drum kit synth pieces and physics, voice pool, kits, tuning/damping/room/perspective/width | 5 | yes | DrumPieces, JamDrumKit, KitRoom | yes (KIT row) | JamDsp.JM24..JM28 | |
| Bass voice: ping-pong StringEngines, string choice, excitations, JamBassTone | 6.1 | yes | JamBassVoice | yes (BASS row) | JamDsp.JM29, JM30 | |
| Bass lines tokens (R 3 5 7 8 A W m - .), register, voice choices | 6.2 | yes | JamBassLine | yes | Jam.JM10, JM11 | |
| Mixer (volume, balance, pans, mutes) | 7 | yes | JamEngine mixer | yes (MIXER row) | Jam.JM07_defaultLevelSitsWithTheGuitar | Stem trims -13/+10 dB calibration (Decision 18) |
| Bassist rests when a bass is loaded | 7 | yes | isBassFamily check | yes (message) | JamPlugin.JM43 | |
| `jam_output` Main/Separate (Aux 9/10), fallback on Layout A/C, ROUTING strips | 7 | yes | RoutingMatrix::writeJamBuses; TapBuffers kJamDrumsAux/kJamBassAux | yes | JamPlugin.JM37_separateOutputs | |
| Mix point after looper, before sessionRecorder, after master; kill ramp | 7 | yes | mixJam; KillSwitch::applyBlockRamp | n/a | JamPlugin.JM38 | |
| JAM tab after TUNE with the sketch's groups, lanes, drag/export, 480 px stacking | 8.1 | yes | Source/UI/JamPanel.cpp, JamLaneView | yes | JamPanel.JM47_theTabSitsBetweenTuneAndLiveAndLaysOutAt480To1600 | RIFFS (feat-riffs) also claims the TUNE-LIVE slot |
| Variation (A)(B) radio pair | 8.1 | partial | JamPanel | yes | JM47 | A dropdown, not a radio pair (Decision 17) |
| "Metronome goes quiet while the band plays" switch in START/STOP | 8.1, 11 | yes | JamPanel (`jamMetronomeQuiet`) | yes | JamPlugin.JM44 | |
| Easy rhythm strip JAM group (pill, style, 5-dot intensity, Band volume) | 8.2 | yes | JamStripGroup, JamPill | yes | JamPanel.JM48_easyGroupAndLivePill | |
| Live Strip JAM pill (tap start/stop, long-press FILL), only while enabled | 8.2 | yes | LiveStrip::refreshJamPill | yes | JM48 | |
| Shortcuts J / Shift+J / Alt+J, rebindable, in cheat sheet | 8.2 | yes | Accessibility.cpp jamStartStop/jamFill/jamArm | yes | JamPanel.JM49_shortcutsAreRebindableListedAndIgnoredWhileTyping | |
| JamStatus double buffer, 30 Hz drain, 250 ms stale | 8.3 | yes | JamStatusChannel; JamPanel::refresh | yes | JamPanel.JM50 | |
| Empty states and errors (8.4 list) | 8.4 | yes | JamUiText::messages | yes | JamPanel.JM50_messagesLabelsAndTheLaneDescription | |
| Live MIDI out ch 10 GM map / ch 11 bass, editable, sample-accurate | 9 | yes | MidiOutConfig::jamParts; MidiOutPanel "JAM BAND" | yes | JamPlugin.JM39_midiOutMatchesTheAudio | |
| JamCapture ring 8192 events | 9 | yes | Source/Jam/JamCapture.h | n/a | JamDsp.JM33, JamPlugin.JM40 | |
| Drag-out 4/8/16/32/all bars, Type 1, two tracks, Generic default, Luthier text metas | 9 | yes | JamMidiExport; JamPanel::DragOut | yes | JamPlugin.JM40_dragOutIsAType1FileWithTwoTracks | |
| Export MIDI... opens midi-export 4.1 dialog with "Jam capture" range | 9 | partial | JamPanel Export MIDI | yes | JM40 | Save chooser writing the same file, not the 4.1 dialog (Decision 14) |
| Tune export "Include Jam band" (audio and MIDI tracks) | 9 | yes | TuneExport::renderAudio, appendJamTracks; TuneExportDialog | yes | JamPlugin.JM09_tuneExportIncludesTheBand | |
| 34 parameters appended in order; RangeFamily::jam for tuning/damping | 10 | yes | Parameters.h/.cpp FEAT-JAM block; PhysicalRange | yes (every ID attached in JamPanel/EasyPanel/LiveStrip) | JamPlugin.JM36_jamParametersAreTheLast34InTableOrder, JM28_kitTuningClampsToStockUntilUnlocked | |
| Transient `jam_play`/`jam_fill_now` excluded from presets/snapshots/morph/randomise, off after host restore; fill self-resets | 10 | partial | PresetManager, PresetMorph, Snapshots, serviceJam | yes | JamPlugin.JM35_presetsSnapshotsAndHostState | Fill resets on the processor timer, not one block after (Decision 2) |
| Rhythm engine interplay; link_rhythm_kit | 11 | yes | serviceJam | n/a | Jam.JM13, JamPlugin.JM46 | |
| Looper bar quantise, guitar only, renderPlaybackMidi feeds follower | 11 | yes | Looper::setRecordStartDelay, renderPlaybackMidi | n/a | JamPlugin.JM38 | |
| Metronome/tune click quiet under drums | 11 | yes | processor click gating | yes | JamPlugin.JM44 | |
| Tune: percussion replaced (strip note), tune bass through JamBassVoice, bass instrument rests, jam hints | 11 | yes | TuneLayersStrip::setPercussionReplaced; onJamTimeline | yes | JamPlugin.JM42, JM43 | |
| Backing track hint | 11 | yes | JamUiText | yes | JM50 | |
| Snapshots/setlist/preset load never stop the band | 11 | yes | PresetManager::keepOnLoad | n/a | JamPlugin.JM45 | |
| Host sync, cycle jumps resync | 11 | yes | JamEngine host clock | n/a | Jam.JM23_cycleJumpResyncs | |
| Preset `jam` block (style_ref, link_rhythm_kit, seed) incl. snapshots | 12 | yes | getJamBlock / setJamBlock | n/a | JamPlugin.JM35 | |
| `.luthierjam` format, Resources/Jam overrides, user folder, atomic save | 12 | yes | JamStyle::toVar/fromVar; JamStyleLibrary | yes (User + file picker) | Jam.JM01, JamPlugin.JM41 | |
| Undo classes; `jam-style-file`; transport not undoable | 12 | yes | loadJamStyleFile | n/a | JamPlugin.JM12_transportIsNotUndoable | |
| Accessibility: labels, Tab order, pill announcements, rate-limited chord announcements, lane description, reduced-motion playhead, glyphs | 12 | yes | JamPanel, JamLaneView, JamUiText | yes | JamPanel.JM47, JM50 | |
| Failure modes (style fallback + banner, chord-map truncation, no play head, re-prepare, NaN guard) | 13 | yes | JamStyleLibrary::loadUserStyle; ModalResonatorBank NaN reset | n/a | JamPlugin.JM41, JamDsp.JM32 | |
| CPU relief step: cymbals 48 -> 24, hat 32 -> 16 | 13 | partial | PluginProcessor.cpp:1707 jam.setReducedCymbals (quality Low) | n/a | - | Relief ladder removed by FEAT-CPU; now keyed on CPU quality Low, governor never thins cymbals; no test found |
| Performance budget (Jam <= 1.7 units, scenario <= 10) | 14 | partial | whole band | n/a | JamDsp.JM34_budgets, JamPlugin.JM34 | Units measured against a StringEngine yardstick (Decision 1) |
| Edition split (Free styles/kits/voices, Pro gates) | 15 | partial | Source/Jam/JamEdition.h; readJam | n/a | JamPanel.JM51_editionTable | Behind `LUTHIER_FREE_EDITION`, never defined anywhere |
| New classes and insertion points | 16 | yes | Source/Jam, Source/DSP/Jam | n/a | build | |
| JM-01 | 17 | yes | - | - | Jam.JM01_everyFactoryStyleParsesAndIsComplete | |
| JM-02 | 17 | yes | - | - | Jam.JM02_rockKicksLandOnTheHostGrid | |
| JM-03 | 17 | yes | - | - | Jam.JM03_tempoAutomationDoesNotDrift | |
| JM-04 | 17 | yes | - | - | Jam.JM04_rendersAreDeterministicPerSeed | |
| JM-05 | 17 | yes | - | - | Jam.JM05_blockSizesNull | |
| JM-06 | 17 | yes | - | - | Jam.JM06_naturalFollowChangesOnTheNextBeat | |
| JM-07 | 17 | yes | - | - | Jam.JM07_graceWindowChangesAtDetection | |
| JM-08 | 17 | yes | - | - | Jam.JM08_tightRelaxedAndBarQuantise | |
| JM-09 | 17 | yes | - | - | Jam.JM09_singleNotesAndUnknownsNeverChangeTheChord | |
| JM-10 | 17 | yes | - | - | Jam.JM10_slashChordRootTokensPlayTheBassNote | |
| JM-11 | 17 | yes | - | - | Jam.JM11_tuneChangesLandExactlyWithApproaches | |
| JM-12 | 17 | yes | - | - | Jam.JM12_predictionFromTheThirdCycle | |
| JM-13 | 17 | yes | - | - | Jam.JM13_rhythmEngineChordIsJamsChord | |
| JM-14 | 17 | yes | - | - | Jam.JM14_firstNoteStartsOnItsSample | |
| JM-15 | 17 | yes | - | - | Jam.JM15_tapInSetsTempoAndPhase | |
| JM-16 | 17 | yes | - | - | Jam.JM16_countInSticks | |
| JM-17 | 17 | yes | - | - | Jam.JM17_stopWhenIStopPlaying | |
| JM-18 | 17 | yes | - | - | Jam.JM18_hostStopEndsOrCutsAndPanicChokes, JamPlugin.JM18_panicSetsJamPlayOff | |
| JM-19 | 17 | yes | - | - | Jam.JM19_changesLandOnBeatsAndBars | |
| JM-20 | 17 | yes | - | - | Jam.JM20_fillNow | |
| JM-21 | 17 | yes | - | - | Jam.JM21_dynamicsFollow | |
| JM-22 | 17 | yes | - | - | Jam.JM22_unsupportedMeterPlaysTheGenericBar | |
| JM-23 | 17 | yes | - | - | Jam.JM23_cycleJumpResyncs | |
| JM-24 | 17 | yes | - | - | JamDsp.JM24_kickModesAndPitchDrop | |
| JM-25 | 17 | yes | - | - | JamDsp.JM25_snareWires | |
| JM-26 | 17 | yes | - | - | JamDsp.JM26_closingTheHatChokesIt | |
| JM-27 | 17 | yes | - | - | JamDsp.JM27_rideRestrikeIsContinuous | |
| JM-28 | 17 | yes | - | - | JamDsp.JM28_kitTuningIsATensionChange, JamPlugin.JM28_kitTuningClampsToStockUntilUnlocked | |
| JM-29 | 17 | yes | - | - | JamDsp.JM29_bassPitchAndClicklessChanges | |
| JM-30 | 17 | yes | - | - | JamDsp.JM30_bassStaysInPositionAndAlternates | |
| JM-31 | 17 | yes | - | - | JamDsp.JM31_aJamReadsNoFiles | |
| JM-32 | 17 | partial | - | - | JamDsp.JM32_noNanAndNoDcAtEveryRate | 60 s in CI, 10 min only with LUTHIER_JAM_LONG=1 |
| JM-33 | 17 | partial | - | - | JamDsp.JM33_noAllocationsAndNoLocks | Shortened by default; no lock trap |
| JM-34 | 17 | partial | - | - | JamDsp.JM34_budgets, JamPlugin.JM34_offCostsNothingAndTheJamScenario | Yardstick-scaled units |
| JM-35 | 17 | yes | - | - | JamPlugin.JM35_presetsSnapshotsAndHostState | |
| JM-36 | 17 | yes | - | - | JamPlugin.JM36_jamParametersAreTheLast34InTableOrder | |
| JM-37 | 17 | yes | - | - | JamPlugin.JM37_separateOutputs | |
| JM-38 | 17 | yes | - | - | JamPlugin.JM38_loopsAreGuitarOnlyTakesHaveTheBandKillMutesIt | |
| JM-39 | 17 | yes | - | - | JamPlugin.JM39_midiOutMatchesTheAudio | |
| JM-40 | 17 | yes | - | - | JamPlugin.JM40_dragOutIsAType1FileWithTwoTracks | |
| JM-41 | 17 | yes | - | - | JamPlugin.JM41_aMalformedStyleFallsBack | |
| JM-42 | 17 | yes | - | - | JamPlugin.JM42_jamDrumsReplaceTheTunesPercussion | |
| JM-43 | 17 | yes | - | - | JamPlugin.JM43_theTunesBassPlaysThroughTheJamBass | |
| JM-44 | 17 | yes | - | - | JamPlugin.JM44_theMetronomeGoesQuietUnderTheDrums | |
| JM-45 | 17 | yes | - | - | JamPlugin.JM45_recallsAndPresetLoadsKeepTheBand | |
| JM-46 | 17 | yes | - | - | JamPlugin.JM46_strumsLandOnTheBandsGrid | Skips the band's first two blocks (Decision 11) |
| JM-47 | 17 | yes | - | - | JamPanel.JM47_theTabSitsBetweenTuneAndLiveAndLaysOutAt480To1600 | |
| JM-48 | 17 | yes | - | - | JamPanel.JM48_easyGroupAndLivePill | |
| JM-49 | 17 | yes | - | - | JamPanel.JM49_shortcutsAreRebindableListedAndIgnoredWhileTyping | |
| JM-50 | 17 | yes | - | - | JamPanel.JM50_messagesLabelsAndTheLaneDescription | |


### mic-placement.md

**Summary:** 91 requirements. **0 yes / 0 partial / 2 no** (plus 89 in progress on claude/luthier-feat-mic).
- Nothing is on the integration branch: continuous placement (MicPlacementModel/Stage, TptSvf, SlewedDelayLine, AcousticMicModel, landmarks, migration + mirror, legacy-automation mapping, room bleed, 25 parameters, MicPlacementView in the CAB section, expanded editor, Easy pad) is in progress on `claude/luthier-feat-mic` with MP-01..MP-37 tests written there.
- Open on the branch: plot does not include the anchor IR's magnitude (shows the delta only), acoustic factory presets not moved to `ac_mic_mix` ~0.5, several thresholds relaxed (MP-03 3.0/5.5/6.5 dB, MP-24 ratio gate, acoustic sigma quartered, room bleed made anchor-relative).
- Merge risk: `CabinetEngine` now carries FEAT-CPU IR truncation variants per mic path on integration; FEAT-MIC rewrites the same processBlock order and the IR reload keys; parameter append order after FEAT-JAM.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Continuous placement on one anchor IR; placement never swaps an IR | 0.1 | in progress on claude/luthier-feat-mic | Source/DSP/Amp/MicPlacement.{h,cpp}:MicPlacementStage; CabinetEngine::anchorConfig | n/a | MicPlacement.placementNeverReloadsAnIr | |
| Identity at the anchor | 0.2 | in progress on claude/luthier-feat-mic | stage skips identity filters; bridge quantises values | n/a | MicPlacement.anchorIsIdentity | |
| Named physical mechanisms with realistic magnitudes | 0.3 | in progress on claude/luthier-feat-mic | MicPlacementModel::evaluate | n/a | MP-07..MP-10, MP-16..MP-19 tests | |
| Click-free (30 ms smoothing, 32-sample control rate, TPT SVFs, slewed delays, crossfaded switches) | 0.4 | in progress on claude/luthier-feat-mic | ExpSmoother; TptSvf::rampTo | n/a | MicPlacement.sweepsAreClickFree | |
| One model, three consumers (engine, plot, tests) | 0.5 | in progress on claude/luthier-feat-mic | PlacementResponse | yes (branch plot) | MicPlacement.plotEqualsEngine | |
| No samples | 0.6 | in progress on claude/luthier-feat-mic | existing make_irs.py anchors | n/a | MP-13 test | |
| Coordinates in cone-landmark units, speaker tables, cabinet geometry, out-of-range speaker = 1 | 2.1 | in progress on claude/luthier-feat-mic | Source/DSP/Amp/CabinetVoices.h:kCabGeometry, resolveSpeaker | yes (branch) | MicPlacement.speakersVaryALittleAndDeterministically | |
| Snap rings Cap/Cap Edge/Cone/Edge | 2.1 | in progress on claude/luthier-feat-mic | MicPlacementView | yes (branch) | MicPlacementUi.releaseNearARingSnapsExactly | |
| Distance/angle/rear ranges (stock/advanced) | 2.1 | in progress on claude/luthier-feat-mic | PhysicalRange RangeFamily::mic | yes (branch) | MP-01 test | |
| kSpeakers/kMics moved to CabinetVoices.h; kMicPolar, kCabGeometry | 2.2 | in progress on claude/luthier-feat-mic | CabinetVoices.h | n/a | MP-08 test | |
| Terms: beaming, presence A_r, HF corner T_r, angle/polar, proximity, dust cap, surround, shelves, rear, level, level match, floor bounce, per-speaker variation | 2.2 | in progress on claude/luthier-feat-mic | MicPlacementModel::evaluate | n/a | MP-07..MP-10, MP-16, MP-17, MP-19 tests | Level match also trims floor energy and 1 kHz tone (D4) |
| Two mics time of arrival (Physical/Aligned), mic_phase_align kept, Lagrange slewed line | 2.3 | in progress on claude/luthier-feat-mic | SlewedDelayLine | yes (branch ToF switch) | MicPlacement.twoMicTimeOfArrival, delayGlidesUnderTheSlewLimit | |
| Acoustic landmarks (`computeAcousticLandmarks`) | 3 | in progress on claude/luthier-feat-mic | Source/Model/Guitar/AcousticLandmarks.h | n/a | MicPlacement.acousticLandmarks | |
| Radiator weights (Gaussian sigma) and composite filter table | 3 | in progress on claude/luthier-feat-mic | Source/DSP/Body/AcousticMicModel.{h,cpp} | n/a | MicPlacement.acousticPositionsHaveTheirVoices | sigma quartered vs spec (D5); no soundhole zeroes all air terms (D6) |
| Mic voice shared terms; calibration K within +-1 dB of internal mic | 3 | in progress on claude/luthier-feat-mic | AcousticMicModel | n/a | MP-21 test | |
| `ac_mic_mix` after circuit, 20 ms fade, skipped (bit-identical) at 0; mic 2 equal-power blend | 3 | in progress on claude/luthier-feat-mic | LuthierEngine acoustic branch | yes (branch knob) | MicPlacement.acousticMicsOffCostNothingAndChangeNothing | |
| AcousticDI bypasses cabinet placement stage | 3 | in progress on claude/luthier-feat-mic | CabinetEngine | n/a | - | AcousticDI keeps legacy structural IR choice (D1) |
| Legacy params kept at indices; not read directly | 4 | in progress on claude/luthier-feat-mic | Parameters.cpp | yes (Quick combos) | MicPlacement.layoutAppendsTheTwentyFiveInTableOrder | |
| Legacy -> continuous mapping | 4 | in progress on claude/luthier-feat-mic | Source/Presets/MicPlacementMigration.cpp:mapLegacy | n/a | MicPlacement.migrationMapsEveryLegacyPair | |
| Mirror into serialised output only | 4 | in progress on claude/luthier-feat-mic | MicPlacementMigration::mirror | n/a | MicPlacement.mirrorWritesTheNearestChoiceIntoFilesOnly | |
| Migration hooks: preset load, setStateInformation, snapshots, morph endpoints; key presence test; no schema bump | 4 | in progress on claude/luthier-feat-mic | PresetManager, Snapshots.cpp, PresetMorph::setSlot | n/a | MicPlacement.migrationTriggers | |
| Legacy host writes map outside +-250 ms of continuous writes | 4 | in progress on claude/luthier-feat-mic | MicLegacyAutomation | n/a | MicPlacement.migrationTriggers | |
| Legacy params no longer structural; IR reload keys on cabinet/speaker/mic type | 4 | in progress on claude/luthier-feat-mic | ParameterBridge::applyStructural | n/a | MP-13 test | |
| MicPlacementStage filter chain (7 SVFs), signed gain, floor tap, ToF, 12 ms buffers | 5 | in progress on claude/luthier-feat-mic | MicPlacement.cpp | n/a | MP-11, MP-14 tests | |
| `TptSvf` added to DspCommon.h | 5 | in progress on claude/luthier-feat-mic | DspCommon.h:TptSvf | n/a | MP-11 test | |
| Processing order convolution -> stage -> ToF/phase -> taps -> blend; fallback uses anchor | 5 | in progress on claude/luthier-feat-mic | CabinetEngine::processBlock, prepareFallback | n/a | MP-02, MP-14 tests | Conflicts with integration's FEAT-CPU per-path IR variants in the same function |
| New API setMicPlacement / setTimeOfFlightMode / setLevelMatch / setRoomMaterialForFloor | 5 | in progress on claude/luthier-feat-mic | CabinetEngine.{h,cpp} | n/a | MP-14, MP-17 tests | |
| Bridge pushes placement to cabinet, acoustic model, room (mod matrix applies) | 5 | in progress on claude/luthier-feat-mic | ParameterBridge::applyToEngine FEAT-MIC block | n/a | Combo.micPlacementUnderAnLfoWhileStrumming | |
| Room close-mic bleed by critical distance; Aux 5 excludes bleed | 5 | in progress on claude/luthier-feat-mic | RoomEngine::setCloseMicDistance, bleedFor | n/a | MicPlacement.closeMicsBleedTheRoomWhenBackedOff | Bleed made anchor-relative (D2) |
| Routing: Aux 3/4 carry acoustic mics on acoustics | 5 | in progress on claude/luthier-feat-mic | LuthierEngine taps | n/a | MP-21 test | |
| Advanced CAB section: MicPlacementView replaces Position/Distance combos (face, rings, handles, thumbnail, Quick combos, mini-plot, ToF, LvL) | 6.1 | in progress on claude/luthier-feat-mic | Source/UI/MicPlacementView.cpp; AdvancedPanel | yes (branch) | MicPlacementUi.advancedCabSectionHasTheViewAndOneEntryDrags | |
| Acoustic "MICROPHONES" variant with body outline, Pickup<->Mic knob, Mic 2 toggle | 6.1 | in progress on claude/luthier-feat-mic | MicPlacementView | yes (branch) | MicPlacementUi.easyPad, familyAndCabinetSwitchesLeaveNoStaleHandles | |
| Expanded editor over Columns 3-4 (cabinet render, side view, cards, response plot, Reset, Grille) | 6.2 | in progress on claude/luthier-feat-mic | Source/UI/MicPlacementEditor.cpp; AdvancedPanel::openMicEditor | yes (branch) | MicPlacementUi.expandedEditorTakesOverColumnsThreeAndFour | |
| Generic mic silhouettes per MicType, no trade dress | 6.2 | in progress on claude/luthier-feat-mic | MicPlacementEditor | yes (branch) | MicPlacementUi.everyStringIsInTheCatalogAndGeneric | |
| Handle interactions (snap 6 px + 60 ms ease, Alt, wheel distance, side-view drags, double-click speaker, right-click menu) | 6.2 | in progress on claude/luthier-feat-mic | MicHandle | yes (branch) | MP-26, MP-29 tests | |
| Response plot includes anchor IR magnitude; notch readout | 6.2 | no | - | no | - | Plot shows the delta only, titled "Change from Cap Edge, 2.5 cm" (D15) |
| Status chips (null, automation, baked IR) | 6.2 | in progress on claude/luthier-feat-mic | MicPlacementView AutomationWatch | yes (branch) | MicPlacementUi.chipsSayWhoIsDrivingAndWhenAMicIsInItsNull | |
| Easy Cabinet card pad "Mic: bright <-> warm", mic 2 ghost, acoustic knob, double-click overlay | 6.3 | in progress on claude/luthier-feat-mic | MicPad, MicPlacementOverlay; EasyPanel | yes (branch) | MicPlacementUi.easyPad | |
| Empty states and errors | 6.4 | in progress on claude/luthier-feat-mic | mic.status.* | yes (branch) | MicPlacementUi.aUserIrHidesTheHandleAndSaysSo | |
| Options Appearance "Snap mics to landmarks", "Show mic response plot"; UiState micGrilleVisible/micFocusedHandle | 6.5 | in progress on claude/luthier-feat-mic | OptionsPages (VISUAL AIDS rows); UiPreferences mic.* | yes (branch) | MP-29, MP-34 tests | |
| 25 parameters appended with ids/ranges/defaults | 7 | in progress on claude/luthier-feat-mic | Parameters.h/.cpp FEAT-MIC block | yes (branch, GuiReach acoustic context) | MicPlacement.layoutAppendsTheTwentyFiveInTableOrder | Must be re-based after FEAT-JAM (+34) |
| RangeFamily::mic; preset ranges.families "mic" | 7 | in progress on claude/luthier-feat-mic | PhysicalRange; RangesUi | yes (branch) | MP-01 test | |
| All automatable; continuous ones mod destinations; automatable in Free | 7 | in progress on claude/luthier-feat-mic | APVTS + bridge value() | n/a | Combo.micPlacementUnderAnLfoWhileStrumming | |
| Round trip presets/host/snapshots/morph | 8 | in progress on claude/luthier-feat-mic | parameter maps + migration | n/a | MicPlacement.allTwentyFiveRoundTripThroughEveryContainer | |
| Undo: drag one 3.5 multi-target entry, grouped nudges, Quick combo 3.2, rear/speaker 3.3, Reset one entry | 8 | in progress on claude/luthier-feat-mic | MicEdit | yes (branch) | MP-26, MP-27 tests | Description "Move Mic N from X, Y cm" (end position unknown, D14) |
| Accessibility: handle groups with 4 sliders, value text, key map, Tab order, reduced motion, Easy pad arrows | 8 | in progress on claude/luthier-feat-mic | MicHandle::createAccessibilityHandler | yes (branch) | MicPlacementUi.handlesAreAccessibleGroupsOfFourSliders, keyboardMovesTheFocusedHandle | |
| Tone Match: user IR bypasses that mic's stage, handle hidden | 9 | in progress on claude/luthier-feat-mic | CabinetEngine::setPlacementBypassed | yes (branch) | MicPlacement.aUserIrBypassesItsMicsStage | |
| Snapshots/morph move the mic smoothly | 9 | in progress on claude/luthier-feat-mic | continuous params | n/a | Combo.micPlacementMorphsSmoothly | |
| Randomize keeps u <= 1 and <= 30 cm with stock-range rule | 9 | in progress on claude/luthier-feat-mic | MicPlacementMigration::keepPlausible | n/a | MicPlacement.randomiseKeepsMicsPlausible | |
| No audio-path interaction with techniques/feedback/MIDI export | 9 | in progress on claude/luthier-feat-mic | - | n/a | existing Feedback suites | |
| Acoustic factory presets set ac_mic_mix ~0.5 | 9 | no | - | n/a | - | Deferred on branch (would break MP-20, D13) |
| Failure modes (no anchor IR, NaN, ribbon null cap, switch mid-drag, legacy lane vs drag, rate change) | 10 | in progress on claude/luthier-feat-mic | prepareFallback, sanitise, MicLegacyAutomation | n/a | MP-02, MP-12, MP-17, MP-23, MP-32 tests | |
| Performance budget (cabinet 0.5 units, stage 0.05/mic, acoustic 0.12) | 11 | in progress on claude/luthier-feat-mic | chunked processing | n/a | MicPlacement.cpuWithinBudget | Ratio gate only; measured cabinet ~1.06 units on CI (D10) |
| Both editions, no gating | 12 | in progress on claude/luthier-feat-mic | - | n/a | - | No Free flag to test |
| MP-01 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.layoutAppendsTheTwentyFiveInTableOrder | |
| MP-02 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.anchorIsIdentity | |
| MP-03 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.legacyFidelityAgainstTheShippedIrs | Relaxed to 3.0/5.5/6.5 dB (D3) |
| MP-04 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.migrationMapsEveryLegacyPair | |
| MP-05 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.migrationTriggers | |
| MP-06 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.mirrorWritesTheNearestChoiceIntoFilesOnly | |
| MP-07 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.radiusDarkensMonotonically | |
| MP-08 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.angleFollowsThePolarPattern | |
| MP-09 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.proximityFallsWithDistance | Floor rho 0 in test (D8) |
| MP-10 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.farMicsHearTheWholeCone | |
| MP-11 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.sweepsAreClickFree | |
| MP-12 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.automationIsRealTimeSafe | No lock probe (D17) |
| MP-13 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.placementNeverReloadsAnIr | |
| MP-14 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.twoMicTimeOfArrival | |
| MP-15 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.delayGlidesUnderTheSlewLimit | |
| MP-16 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.rearToggleIsSmoothAndPolarityFollowsTheMode | |
| MP-17 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.levelMatchHoldsTheLevel | |
| MP-18 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.closeMicsBleedTheRoomWhenBackedOff | |
| MP-19 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.speakersVaryALittleAndDeterministically | |
| MP-20 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.acousticMicsOffCostNothingAndChangeNothing | Model-off check plus offline md5 comparison (D11) |
| MP-21 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.acousticPositionsHaveTheirVoices | |
| MP-22 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.acousticLandmarks | |
| MP-23 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.deterministicAndRateIndependent | 0.3 dB window (D9) |
| MP-24 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.cpuWithinBudget | Ratio regression gate |
| MP-25 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.plotEqualsEngine | |
| MP-26 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacementUi.advancedCabSectionHasTheViewAndOneEntryDrags | |
| MP-27 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacementUi.keyboardMovesTheFocusedHandle | |
| MP-28 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacementUi.handlesAreAccessibleGroupsOfFourSliders | |
| MP-29 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacementUi.releaseNearARingSnapsExactly | |
| MP-30 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacementUi.easyPad | |
| MP-31 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacementUi.expandedEditorTakesOverColumnsThreeAndFour | |
| MP-32 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacementUi.familyAndCabinetSwitchesLeaveNoStaleHandles | |
| MP-33 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacement.aUserIrBypassesItsMicsStage, MicPlacementUi.aUserIrHidesTheHandleAndSaysSo | |
| MP-34 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacementUi.reflowsAndRespectsReducedMotion | |
| MP-35 | 13 | in progress on claude/luthier-feat-mic | - | - | Combo.micPlacementMorphsSmoothly, MicPlacement.allTwentyFiveRoundTripThroughEveryContainer | Free-build half not testable |
| MP-36 | 13 | in progress on claude/luthier-feat-mic | - | - | Combo.micPlacementUnderAnLfoWhileStrumming | |
| MP-37 | 13 | in progress on claude/luthier-feat-mic | - | - | MicPlacementUi.everyStringIsInTheCatalogAndGeneric | |


### output-normalization.md

**Summary:** 88 requirements. **75 yes / 13 partial / 0 no**.
- Done (merged FEAT-NORMALIZE): LoudnessNormalizer in MasterBus behind a block-level branch, 300 ms timeline-aligned glide, true-peak detector and -1 dBTP limiter mode, loudness roles, ConfigChangeTracker, worker calibrator with LRU/factory/disk caches, BS.1770 meter, offline wait, session state, Options AUDIO group, header and Easy badge, banner, captions, ON-01..ON-36 tests present.
- Open: tune-load prefetch (setlist only), preview-player offset not consumed (PreviewPlayer lives on unmerged feat-browser, which also does not record `truePeakDbtp` or use the shared meter), no Free edition (ON-32 checks the hash only).
- Deviations: roles default to Config instead of failing on unlisted ids; a fresh render instance per calibration; high live rates calibrate at 96/192 kHz; the unmerged feat-normalize commit only regenerates the ON-02 golden hashes.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Off by default; off path bit-identical (stage skipped) | 0.1 | yes | Source/DSP/Master/MasterBus.cpp (block-level branch) | yes (Options AUDIO) | Normalization.ON01_OffByDefault, NormalizationGolden.ON02_OffPathMatchesGoldenHashes | Golden hashes refresh pending on claude/luthier-feat-normalize (72489a9) |
| Calibrated static gain from the configuration, never auto-gain | 0.2 | yes | NormalizationCalibrator | n/a | ON05, ON06 | |
| Honest about cost (banner + permanent caption) | 0.3 | yes | NormalizationOptions caption; NormalizationUi::postEnabledBanner | yes | NormalizationGui.ON34_OptionsAudioPage | |
| True-peak safety holds <= -1.0 dBTP while on | 0.4, 4.1 | yes | Source/DSP/Master/TruePeakDetector.h; MasterBus::processBlockNormalized | n/a | ON09_TruePeakSafety, ON10 | Attack changed to sliding-min + box average on the normalization path (Decision 6) |
| Real-time rules; result as one atomic word | 0.5 | yes | LoudnessNormalizer::publishResult | n/a | ON17_RealtimeSafety | |
| Offline renders deterministic | 0.6, 4.6 | yes | OutputNormalization::processBlockStart | n/a | ON16_OfflineDeterminism | |
| Main output only | 0.7, 10 | yes | master bus structure | n/a | ON22_AuxAndPerStringUntouched | |
| Switch and target (-14/-16/-18/-20/-23, default -18); gain clamp -12..+24, 0.01 dB; ahead of master_gain | 2.1 | yes | Source/Support/OutputNormalization; LoudnessNormalizer::gainForMeasurement | yes | GainRuleClampsAndRounds, ON07 | |
| 300 ms linear-dB glide per 32-sample timeline segment | 2.2 | yes | LoudnessNormalizer | n/a | GlideIsTimelineAlignedAndBlockIndependent, ON11 | |
| Cached preset/guitar load rides the load crossfade | 2.2 | yes | LoudnessNormalizer::applyLoadGain; PresetManager::onPresetLoaded | n/a | ON28_PresetLoadWithCachedGain | Offline loads snap (Decision 8) |
| Recalibration triggers: discrete events immediate; Config params 250 ms timeline debounce; nothing else | 2.3 | yes | Source/Support/ConfigChangeTracker.{h,cpp} | n/a | ON13_Debounce, ON06, ON14 | Tracker is its own class, not inside ParameterBridge (Decision 1) |
| Hold while measuring; discard superseded results | 2.3 | yes | NormalizationCalibrator serial check | n/a | ON12 | |
| Every parameter has a LoudnessRole; unlisted id is a test failure | 3.1 | partial | Source/DSP/Master/LoudnessRoles.{h,cpp} | n/a | Normalization.ON26_RolesAndHash | Unlisted ids default to Config (coordinator instruction); ON-26 does not fail on them |
| Snapshots/morph are performance (PerformanceWriteScope) | 3.2 | yes | SnapshotBank::applyBlend, PresetMorph::apply | n/a | ON14_PerformanceWritesDoNotRecalibrate | |
| Preset morph gain lerp between endpoint calibrations | 3.2 | yes | OutputNormalization::updateMorph | n/a | ON15_PresetMorph | |
| Configuration hash (SHA-256, canonical JSON, quantised, GuitarSpec, IRs, edition, revision) | 3.3 | yes | NormalizationCalibrator::hashSoundState | n/a | ON26, ON27 | Derived per-string detune left out (Decision 9) |
| MasterBus normalizer stage, isActive, applyLoadGain | 4.1 | yes | Source/DSP/Master/LoudnessNormalizer.{h,cpp} | n/a | ON11, ON33 | |
| Limiter forced on (even if limiter_on off), ceiling glide -0.3 -> -1.0 dBTP, latency unchanged | 4.1 | yes | MasterBus | yes (Options text) | ON09 | |
| Worker `NormalizationCalibrator` (low priority, polls serial, captureSoundState without message thread) | 4.2 | yes | Source/Support/NormalizationCalibrator.{h,cpp} | n/a | ON12, ON16, ON25 | |
| Reference render: offline instance borrowed from PreviewRenderService calibration lane, or own reused instance | 4.3 | partial | NormalizationCalibrator::renderAndMeasure | n/a | ON25_NoRecursion | Fresh instance per calibration (Decision 5); PreviewRenderService not on integration |
| Render setup (Performance params at defaults, 48 kHz/256, 100 BPM, non-realtime, 0.5 s settle) | 4.3 | partial | renderAndMeasure | n/a | ON03, ON30 | 88.2/96 and 176.4/192 kHz live rates calibrate at 96/192 kHz with their own hash (engine ~3 dB louder at 96 kHz, Decision 4) |
| NormalizationPhrase (guitar I-IV-V-I + eighths + ring; bass line) via setDirectMidi | 4.3 | yes | Source/Support/NormalizationPhrase.{h,cpp} | n/a | ON03 | |
| Bs1770Meter shared with PreviewRenderer | 4.3 | partial | Source/DSP/Master/Bs1770Meter.{h,cpp} | n/a | Bs1770MeterReadsReferenceTones | feat-browser's PreviewRenderer uses its own ToneFeatures meter |
| Render cost <= 1.5 s; cancel on newer serial; abandon at 10 s | 4.3 | yes | renderAndMeasure | n/a | ON33_Performance, ON29 | |
| Caches: LRU 512, factory table (BinaryData, `--calibrate-factory`), disk cache | 4.4 | yes | NormalizationCalibrator caches; Resources/NormalizationFactory.json; Tools/RenderCli.cpp | n/a | ON27_Cache, ON03_FactoryTableDoesNotDrift | Disk cache under ~/Documents/Luthier/Cache (Decision 12) |
| Prefetch on preset load; tune/setlist queue background calibrations | 4.4 | partial | OutputNormalization::prefetchPresets; LuthierAudioProcessor::loadSetlist | n/a | ON31_Combinations | Tune load path not wired (TuneSession does not expose section presets) |
| Analytic estimate on failure (factory entry by type/amp/drive), flagged, never cached | 4.5 | yes | NormalizationCalibrator::estimateFor | yes (readout "about") | ON29_FailurePath | |
| Offline sanctioned wait (1 ms polls, 10 s cap); prepareToPlay verifies restored calibration; AudioExporter setNonRealtime | 4.6 | yes | OutputNormalization::processBlockStart; AudioExporter | n/a | ON16, ON29 | |
| Options -> AUDIO OUTPUT NORMALIZATION group (switch, target, readout, caption, limiter note) | 5.1 | yes | Source/UI/NormalizationOptions.{h,cpp} in AudioPage | yes | NormalizationGui.ON34_OptionsAudioPage | |
| Header badge left of output meter + Easy meter column; click opens AUDIO with switch focused | 5.1 | yes | Source/UI/NormalizationBadge.{h,cpp}; HeaderBar; EasyPanel; PluginEditor::openNormalizationOptions | yes | NormalizationGui.ON35_Badge | |
| Readout states (off, measuring, applied, clamped, estimate, unmeasurable, morphing); 10 Hz; stale muted | 5.2 | yes | OutputNormalization::readoutText / badgeText | yes | ON07, ON08, ON15, ON29, ON34 | Measuring text uses "..." ASCII |
| Banner `normalization.on` with [Options] [Don't show again]; not from session restore | 5.3 | yes | NormalizationUi::postEnabledBanner; Notification::secondaryAction | yes | ON34, ON36 | |
| ROUTING caption, Workshop bench note, Diagnostics audio path + State Inspector, debug overlay lines | 5.4 | yes | NormalizationCaption; RoutingPanel; WorkshopPanel; DiagnosticsPage; DebugPanel | yes | NormalizationGui.CaptionsAndDiagnostics | |
| Locale keys options.audio.normalization.*, banner.*, badge.* | 5.4 | yes | Source/Accessibility/Localisation.cpp | n/a | ON34, ON36 | |
| Session state root key; snap on restore; verify in background | 6 | yes | OutputNormalization::toVar / restoreFromSession | n/a | ON18_SessionRoundTrip | |
| UiPreferences defaults for new instances; legacy sessions load off | 6 | yes | NormalizationOptions defaults registrar | n/a | ON19_LegacySessionsAndThePreference | |
| Not preset data | 6 | yes | outside presets.toVar() | n/a | ON20_NotPresetData | |
| Undo/redo/A-B keep it (RestoreScope::soundOnly) | 6, 8 | yes | LuthierAudioProcessor::restoreState | n/a | ON21_UndoAndABDoNotTouchIt | |
| updateHostDisplay non-parameter change on toggle/target | 6 | yes | OutputNormalization | n/a | - | no dedicated test found |
| No parameters added | 7 | yes | - | n/a | ON26 | |
| Not undoable | 8 | yes | - | n/a | ON21 | |
| Accessibility: switch name + description = warning, target label/disabled, readout announcements <= 1/2 s, focusable badge, unbound "Toggle output normalization" | 9 | yes | NormalizationOptionsGroup; NormalizationBadge; `toggleNormalization` | yes | NormalizationGui.ON36_Accessibility | |
| Aux 1-6/8 and per-string untouched; Aux 7 follows main | 10 | yes | engine structure | n/a | ON22 | |
| Meter shows normalized level | 10 | yes | MasterBus meter | yes | ON23_MeterShowsNormalizedLevel | Meter reads 3 dB under BS.1770 on centred stereo (Decision 10) |
| Jam band mixed after master, not normalized, not in reference render | 10 | yes | mixJam after master (FEAT-JAM) | n/a | JamPlugin.JM38 | |
| Preview player adds target+18 dB limited to -1 dBTP; previews render with normalization off | 10 | partial | OutputNormalization::getPreviewGainOffsetDb | n/a | ON24_Previews | API only; no PreviewPlayer on integration and feat-browser does not call it or store truePeakDbtp |
| Workshop / spectrum delta / Tone Match run with normalization off | 10 | yes | their own engines | n/a | - | |
| Randomise/Reset leave it; Diagnostics reset turns preference off | 10 | yes | DiagnosticsPage | yes | - | |
| Telemetry usage boolean `normalization_enabled` | 10 | yes | Telemetry::record | n/a | - | |
| Editions: both, identical; Free factory table; Pro preset in Free measured on neutralised sound | 11 | partial | edition string in every hash | n/a | Normalization.ON32_EditionIsPartOfTheHash | No Edition.h, no Free CI configuration |
| Failure modes (render fail/timeout, instance creation failure banner, unmeasurable, clamp, hash mismatch, corrupt disk cache, rate change) | 12 | yes | ErrorLog CALIBRATION_FAILED; estimate | yes | ON07, ON08, ON27, ON29 | |
| Performance budget (MasterBus active <= 0.22 units, worker <= 1.5 s) | 13 | yes | SIMD FIR with exact gating | n/a | ON33 (active 0.17 units) | |
| ON-01 | 15 | yes | - | - | Normalization.ON01_OffByDefault | |
| ON-02 | 15 | yes | - | - | NormalizationGolden.ON02_OffPathMatchesGoldenHashes, Normalization.ON02_OffPathIdenticalToBypassedStage | Full 36x26 grid only with LUTHIER_SLOW_TESTS=1 |
| ON-03 | 15 | yes | - | - | Normalization.ON03_ON04_FactoryCombinationsLandOnTarget, ON03_FactoryTableDoesNotDrift | Seeded sample by default |
| ON-04 | 15 | yes | - | - | Normalization.ON03_ON04_FactoryCombinationsLandOnTarget | |
| ON-05 | 15 | yes | - | - | Normalization.ON05_DynamicsPreserved | |
| ON-06 | 15 | yes | - | - | Normalization.ON06_GainIsInputIndependent | |
| ON-07 | 15 | yes | - | - | Normalization.ON07_Clamping | |
| ON-08 | 15 | yes | - | - | Normalization.ON08_Unmeasurable | |
| ON-09 | 15 | yes | - | - | Normalization.ON09_TruePeakSafety | |
| ON-10 | 15 | yes | - | - | Normalization.ON10_LimiterTransparency | |
| ON-11 | 15 | yes | - | - | Normalization.ON11_ToggleIsAPureGlide | |
| ON-12 | 15 | yes | - | - | Normalization.ON12_CalibrationUpdateGlides | |
| ON-13 | 15 | yes | - | - | Normalization.ON13_Debounce | |
| ON-14 | 15 | yes | - | - | Normalization.ON14_PerformanceWritesDoNotRecalibrate | Wah/volume-pedal slot positions are not parameters (Decision 3) |
| ON-15 | 15 | yes | - | - | Normalization.ON15_PresetMorph | |
| ON-16 | 15 | yes | - | - | Normalization.ON16_OfflineDeterminism | |
| ON-17 | 15 | partial | - | - | Normalization.ON17_RealtimeSafety | No mutex trap exists in the repo |
| ON-18 | 15 | yes | - | - | Normalization.ON18_SessionRoundTrip | |
| ON-19 | 15 | yes | - | - | Normalization.ON19_LegacySessionsAndThePreference | |
| ON-20 | 15 | yes | - | - | Normalization.ON20_NotPresetData | |
| ON-21 | 15 | yes | - | - | Normalization.ON21_UndoAndABDoNotTouchIt | |
| ON-22 | 15 | yes | - | - | Normalization.ON22_AuxAndPerStringUntouched | |
| ON-23 | 15 | yes | - | - | Normalization.ON23_MeterShowsNormalizedLevel | On the meter's own scale |
| ON-24 | 15 | partial | - | - | Normalization.ON24_Previews | Tests the offset API; no PreviewPlayer on integration |
| ON-25 | 15 | yes | - | - | Normalization.ON25_NoRecursion | |
| ON-26 | 15 | partial | - | - | Normalization.ON26_RolesAndHash | Does not fail on unlisted ids |
| ON-27 | 15 | yes | - | - | Normalization.ON27_Cache | |
| ON-28 | 15 | yes | - | - | Normalization.ON28_PresetLoadWithCachedGain | |
| ON-29 | 15 | yes | - | - | Normalization.ON29_FailurePath | |
| ON-30 | 15 | partial | - | - | Normalization.ON30_SampleRates | 96 kHz hashes separately (rate families) |
| ON-31 | 15 | partial | - | - | Normalization.ON31_Combinations | Tune playback across sections not covered by prefetch |
| ON-32 | 15 | partial | - | - | Normalization.ON32_EditionIsPartOfTheHash | No Free CI configuration |
| ON-33 | 15 | yes | - | - | Normalization.ON33_Performance | |
| ON-34 | 15 | yes | - | - | NormalizationGui.ON34_OptionsAudioPage | |
| ON-35 | 15 | yes | - | - | NormalizationGui.ON35_Badge | |
| ON-36 | 15 | yes | - | - | NormalizationGui.ON36_Accessibility | |


### preset-browser-previews.md

**Summary:** 89 requirements. **0 yes / 0 partial / 5 no** (plus 84 in progress on claude/luthier-feat-browser).
- Nothing is on the integration branch (it still has the old `PresetBrowserPanel` in Overlays): previews (PreviewPhrase/Renderer/RenderService/Cache/ClipPool/Player/ToneFeatures), shipped factory clips + manifest, PresetIndex/Search/ToneDescriptors/similarity, PresetLibraryPrefs, the rebuilt browser in Source/UI/PresetBrowser and the Options groups are in progress on `claude/luthier-feat-browser` with PB-01..PB-34, PB-36, PB-37 tests written there.
- Open on the branch: editions (PB-35, locked Pro presets and upsell), content-pack previews, CPU-relief pause hook unfed, descriptor localisation keys, and the output-normalization coordination (shared Bs1770Meter/TruePeakDetector not used, no `truePeakDbtp` in the sidecar, PreviewPlayer never applies `getPreviewGainOffsetDb`, calibrator does not borrow the service).
- Deviations: fresh offline instance per job, CLI renders through the processor (no PreviewRenderHost), browser keys in their own rebindable group, PB-27/PB-15 timing gates x2.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Rendered by Luthier's engine, bit-identical | 0.1 | in progress on claude/luthier-feat-browser | Source/Presets/Preview/PreviewRenderer | n/a | PresetPreview.PB01_renderingIsBitIdentical | Fixed Ogg serial per hash |
| Live instance never touched; no undo entries | 0.2 | in progress on claude/luthier-feat-browser | LuthierAudioProcessor::ScopedOfflineRenderConstruction | n/a | PresetPreview.PB06_previewsLeaveTheLiveInstanceAlone | |
| Real-time safe mix | 0.3 | in progress on claude/luthier-feat-browser | PreviewPlayer::processBlock | n/a | PresetPreview.PB07_switchingAllocatesNothing | |
| Never interrupts playing; transport rule | 0.4 | in progress on claude/luthier-feat-browser | PresetLibrary::whyBlocked | n/a | PB09, PB10, PB11 | |
| No new parameters | 0.5, 9 | in progress on claude/luthier-feat-browser | - | n/a | Integration count unchanged | |
| Both editions | 0.6, 12 | no | - | n/a | - | No edition module; lock flag and upsell hook only |
| Hybrid: factory clips shipped, others rendered + cached | 2 | in progress on claude/luthier-feat-browser | PreviewRenderService::process | n/a | PresetPreview.shippedClipsPlayWithoutRendering, PB15, PB17 | |
| `luthier-render --render-previews`, render_previews.sh, shipped via luthier_copy_resources | 2 | in progress on claude/luthier-feat-browser | Tools/RenderCli.cpp; scripts/render_previews.sh; CMakeLists.txt | n/a | PB02 | |
| Content-pack `Previews/<uid>.ogg` | 2 | no | - | n/a | - | Content packs not built |
| Save queues high-priority render; browser open queues unrendered (visible first); hover/select to front | 2 | in progress on claude/luthier-feat-browser | PresetManager::onPresetSaved; PresetLibrary::queueUnrendered | yes (branch) | PB15, PB28, PB30 | |
| Staleness by sound hash | 2, 5.1 | in progress on claude/luthier-feat-browser | PreviewRenderer::computeSoundHash | n/a | PresetPreview.PB14_soundHash | |
| PreviewPhrase enum (16 phrases), <= 3.2 s notes, <= 4.0 s clip, 250 ms fade | 3.1 | in progress on claude/luthier-feat-browser | Source/Presets/Preview/PreviewPhrase | n/a | PB03, PB04 | |
| Phrase selection order (field, words, features) | 3.1 | in progress on claude/luthier-feat-browser | PreviewPhrase::choose | n/a | PresetPreview.PB04_phrasesAreChosenCorrectly | Bass presets choose only bass phrases |
| Pitch follows the guitar; lead/twang octave fit | 3.1 | in progress on claude/luthier-feat-browser | PreviewPhrase::build | n/a | PresetPreview.PB05_theLowestNoteFollowsTheTuning | |
| Rhythm-engine presets preview as two held chords | 3.1 | in progress on claude/luthier-feat-browser | hitsFor(rhythmEngine), OfflinePlayHead | n/a | - | No factory preset stores a rhythm engine; untested |
| Renderer job steps (reset, own loadPreset, 48k/256, 0.5 s settle, 256 blocks) | 3.2 | in progress on claude/luthier-feat-browser | PreviewRenderer::renderLoaded | n/a | PB01, PB02 | |
| Loudness -18 LUFS, -3 dBTP, no limiter, gain in sidecar | 3.2 | in progress on claude/luthier-feat-browser | ToneFeatures::integratedLoudness / truePeakDb | n/a | PB03 | |
| Use shared Bs1770Meter/TruePeakDetector; record `truePeakDbtp`; render with normalization off; PreviewPlayer adds getPreviewGainOffsetDb | 3.2, output-normalization 10 | no | - | n/a | - | Own meter in ToneFeatures; sidecar lacks truePeakDbtp; no call to OutputNormalization |
| Ogg Vorbis q0.5 <= 64 KB + 64-point peaks | 3.2 | in progress on claude/luthier-feat-browser | PreviewRenderer::encodeOgg, computePeaks | n/a | PB03 | JUCE_USE_OGGVORBIS in CMake |
| Cancel per block; 10 s timeout; Free instance renders Free values | 3.2 | in progress on claude/luthier-feat-browser | renderLoaded | n/a | PB18, PB19 | Free half not applicable |
| PreviewRenderHost shared with CLI | 3.2 | in progress on claude/luthier-feat-browser | CLI renders through LuthierAudioProcessor | n/a | PB02 | Different structure than specified, same effect |
| PreviewRenderService: one low-priority thread, priorities, callAsync + WeakReference, 500 ms shutdown | 3.3 | in progress on claude/luthier-feat-browser | Source/Presets/Preview/PreviewRenderService | n/a | PB15, PB17, PB19 | |
| Background jobs pause while transport runs or CPU relief active | 3.3 | in progress on claude/luthier-feat-browser | setPaused(transport, cpuRelief) | n/a | - | CPU relief input not fed (FEAT-CPU governor relief exists on integration now) |
| PreviewPlayer: host-rate immutable clip, pending/inUse handoff, pool, two voices, fades, 20 ms volume ramp, stop/panic/prepare | 4.1 | in progress on claude/luthier-feat-browser | PreviewPlayer; PreviewClipPool | n/a | PB07, PB08, PB12, PB13 | |
| Insertion in processSlice after practice/click, before routing.distribute; mono -3 dB; kill switch | 4.2 | in progress on claude/luthier-feat-browser | LuthierAudioProcessor::processSlice | n/a | PB09, PB10, PB11 | |
| Gate table (idle, live notes, host/tune transport, whileTransport, non-realtime, kill) and 200 ms not-processing hint | 4.3 | in progress on claude/luthier-feat-browser | PreviewPlayer::publishGate; PresetLibrary::whyBlocked | yes (branch footer) | PresetPreview.PB10_transportRules, PB36 | |
| Triggers: hover 300 ms, glyph click / Click only, Space, selection 150 ms; stops; no loop; neighbours predecoded | 4.4 | in progress on claude/luthier-feat-browser | PresetBrowserPanel::timerCallback, togglePreview | yes (branch) | PresetBrowserUi.PB28_easyModeKeyboard, PB30_hover | |
| Disk cache per OS path, sidecar, temp-rename, locks, 128 MB LRU, "Luthier Free" folder | 5.2 | in progress on claude/luthier-feat-browser | PreviewCache | n/a | PresetPreview.PB16_pruningAndLocks | |
| Options -> FILE LOCATIONS "Preview cache: Open / Clear" | 5.2 | in progress on claude/luthier-feat-browser | PresetCacheGroup | yes (branch) | PresetBrowserUi.PB37_reachability | |
| Shipped manifest; edited factory ignored; version mismatch plays stale + re-render | 5.3 | in progress on claude/luthier-feat-browser | PreviewRenderService::tryShipped | n/a | PB02, shippedClipsPlayWithoutRendering | |
| Preset `uid` and `previewPhrase` fields, known keys, no descriptors in files | 5.4 | in progress on claude/luthier-feat-browser | PresetManager FEAT-BROWSER blocks | n/a | PresetPreview.PB26_uidAndPreviewPhraseRoundTrip | |
| PresetLibraryPrefs `preset-library.json` (favourite, rating, lastLoaded, loadCount, recent 30) | 5.5 | in progress on claude/luthier-feat-browser | Source/Presets/PresetLibraryPrefs | yes (branch heart/stars) | PB23, PB25 | Counts only browser loads |
| PresetIndex per processor, worker-built, incremental; PresetInfo new fields | 5.6 | in progress on claude/luthier-feat-browser | Source/Presets/Search/PresetIndex | n/a | PB22, PB23 | |
| Parameter features read without loading | 6.1 | in progress on claude/luthier-feat-browser | Source/Presets/Search/PresetFeatures | n/a | PB20, PB23 | Mute/Tap/Bend arm params absent in build |
| ToneFeatures spectral features | 6.2 | in progress on claude/luthier-feat-browser | Source/Presets/Preview/ToneFeatures | n/a | PB20, PB13 | Magnitude-weighted centroid |
| Descriptor vocabulary and rules, calibration file, genres | 6.3 | in progress on claude/luthier-feat-browser | Source/Presets/Search/ToneDescriptors; Resources/Presets/descriptor-calibration.json | n/a | PresetSearch.PB20_factoryDescriptors | djent also accepts an active gate |
| Query tokens, phrases, prefix, Damerau-1 >= 5 chars, weights, sort | 6.4 | in progress on claude/luthier-feat-browser | Source/Presets/Search/PresetSearch | yes (branch) | PB21, PB22 | |
| Similarity 24-dim weighted, 8 nearest, exclusions, "still being analysed" | 6.5 | in progress on claude/luthier-feat-browser | PresetSearch::similar | yes (branch) | PresetSearch.PB24_soundsLike | |
| Panel moved to Source/UI/PresetBrowser, editor wiring unchanged | 7 | in progress on claude/luthier-feat-browser | PresetBrowserPanel | yes (branch) | PB28 | |
| Easy layout (search, speaker/volume, chips, rows with play + waveform, detail, More like this + breadcrumb, Morph row) | 7.1 | in progress on claude/luthier-feat-browser | PresetBrowserPanel::layoutContent; PresetDetailPane | yes (branch) | PresetBrowserUi.PB29_layouts | |
| Advanced layout (sidebar filters, sort options, columns, SOUNDS LIKE pane); filters persist | 7.2 | in progress on claude/luthier-feat-browser | PresetFilterSidebar; rememberView | yes (branch) | PB29, PB23 | |
| Rows/detail: glyph states, waveform, author vs auto chips, heart, stars, lock, "~" approximate | 7.3 | in progress on claude/luthier-feat-browser | PresetBrowserWidgets; PresetRow | yes (branch) | PB32, PB33 | Lock/upsell not exercisable (no Free build) |
| Keyboard table, overrides global Space/digits, rebindable "Preset browser" group | 7.4 | in progress on claude/luthier-feat-browser | handleBrowserKey; PresetBrowserKeys | yes (branch) | PB28, PresetBrowserUi.PB31_keysDoNotLeak | Own key group (global table refuses clashes) |
| Empty states and errors | 7.5 | in progress on claude/luthier-feat-browser | getEmptyText / getFooterText | yes (branch) | PresetBrowserUi.PB34_emptyStates, PB18 | |
| Options -> APPEARANCE PRESET BROWSER group (5 keys) and footer mirror | 8 | in progress on claude/luthier-feat-browser | PresetBrowserAppearanceGroup | yes (branch) | PresetBrowserUi.PB33_previewSettings | |
| Undo: only the load boundary | 9 | in progress on claude/luthier-feat-browser | loadEntry | n/a | PB06, PB28 | |
| Accessibility (row names, glyph/heart/stars/chips, announcements, tab order, reduced motion, localisation) | 10 | in progress on claude/luthier-feat-browser | describeRow, announce, StarRating | yes (branch) | PresetBrowserUi.PB32_accessibility | Descriptor words not in the English catalogue |
| Performance budget table | 11 | in progress on claude/luthier-feat-browser | - | n/a | PresetSearch.PB27_performance | Gates x2 on CI |
| Editions: Free previews Pro presets from shipped clips, upsell on load, Free-valued local renders | 12 | no | - | no | - | No Free build |
| Interactions (morph slots, snapshots, automation, rhythm/tune, techniques chip, exclusions, workshop, audition, onboarding line) | 13 | in progress on claude/luthier-feat-browser | load path; Onboarding.cpp tour text | yes (branch) | PB36, PB31, PB14 | |
| New/changed files incl. CMake JUCE_USE_OGGVORBIS | 14 | in progress on claude/luthier-feat-browser | CMakeLists.txt | n/a | build | |
| Failure modes table | 15 | in progress on claude/luthier-feat-browser | renderer/service/cache | yes (branch) | PB13, PB18, PB19 | |
| PB-01 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB01_renderingIsBitIdentical | |
| PB-02 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB02_shippedClipsMatchFreshRenders | Bands 50 Hz-16 kHz, floored 50 dB |
| PB-03 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB03_everyClipMeetsTheLoudnessAndLengthRules | |
| PB-04 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB04_phrasesAreChosenCorrectly | |
| PB-05 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB05_theLowestNoteFollowsTheTuning | |
| PB-06 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB06_previewsLeaveTheLiveInstanceAlone | |
| PB-07 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB07_switchingAllocatesNothing | No mutex trap in repo |
| PB-08 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB08_fades | |
| PB-09 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB09_perStringOutputNullsAgainstNoPreview | |
| PB-10 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB10_transportRules | |
| PB-11 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB11_recordingsAndBusesExcludeThePreview | |
| PB-12 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB12_levelAndVolumeSmoothing | |
| PB-13 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB13_sampleRateChange | |
| PB-14 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB14_soundHash | |
| PB-15 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB15_aSaveRendersItsPreview | 3 s gate x2 |
| PB-16 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB16_pruningAndLocks | |
| PB-17 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB17_twoProcessorsRenderOnce | |
| PB-18 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB18_failures | |
| PB-19 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB19_destroyMidRender | |
| PB-20 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetSearch.PB20_factoryDescriptors | |
| PB-21 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetSearch.PB21_queries | |
| PB-22 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetSearch.PB22_fieldsAndWeights | |
| PB-23 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetSearch.PB23_filters | |
| PB-24 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetSearch.PB24_soundsLike | |
| PB-25 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB25_libraryPrefsSurvive | |
| PB-26 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB26_uidAndPreviewPhraseRoundTrip | |
| PB-27 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetSearch.PB27_performance | Gates x2 |
| PB-28 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetBrowserUi.PB28_easyModeKeyboard | |
| PB-29 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetBrowserUi.PB29_layouts | |
| PB-30 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetBrowserUi.PB30_hover | |
| PB-31 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetBrowserUi.PB31_keysDoNotLeak | |
| PB-32 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetBrowserUi.PB32_accessibility | |
| PB-33 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetBrowserUi.PB33_previewSettings | |
| PB-34 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetBrowserUi.PB34_emptyStates | |
| PB-35 | 16 | no | - | - | - | Free build does not exist |
| PB-36 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetPreview.PB36_combinations | Uses processor pair, not CombinationTests harness |
| PB-37 | 16 | in progress on claude/luthier-feat-browser | - | - | PresetBrowserUi.PB37_reachability | |


### riff-library.md

**Summary:** 93 requirements. **0 yes / 0 partial / 5 no** (plus 88 in progress on claude/luthier-feat-riffs).
- Nothing is on the integration branch: 300 factory riffs (12 `.riffdef` files + deterministic generator + deny list), `.luthierriff` format, RiffLibrary/Analysis/Compiler/Transposer/Player, drag-out, Add to Tune, Send to Looper, Learn It, Save as riff, RIFFS tab, Easy drawer, `R` shortcut and a scrolling workspace tab strip are in progress on `claude/luthier-feat-riffs` with RL-01..RL-31 tests (except RL-29) written there.
- Open on the branch: editions (Free set, locks, `edition::Feature` entries, RL-29), in-editor drop onto the TUNE melody strip, RL-06 -60 dBFS render null, runtime `riff.<id>.name` locale lookup, jam "Key: Jam" / RiffLibrary::query use by jam mode, global-search `riff:` provider.
- Merge risks: RIFFS and JAM both claim the slot between TUNE and LIVE; `NoteOnEvent::explicitArticulation` also added by feat-assist; the `riffs` and `search` shortcut entries collide at the same line of Accessibility.cpp.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Original content only; generic names; trademark scan and deny list | 0.1, 12 | in progress on claude/luthier-feat-riffs | Tools/generate_factory_riffs.py check_names; Tools/riffs/deny.txt; Tools/trademark_scan.py | n/a | Riffs.catalogNamesAreOriginalAndClean | |
| Techniques explicit; auto-articulation leaves riff notes alone | 0.2, 5.4 | in progress on claude/luthier-feat-riffs | NoteOnEvent::explicitArticulation | n/a | Riffs.riffNotesAreExplicitAndKeepTheirStrings | feat-assist honours the same field |
| `.luthierriff` is the source of truth; .mid derived by MidiProfiles | 0.3 | in progress on claude/luthier-feat-riffs | RiffDestinations::writeDragFile | n/a | Riffs.dragFileIsAValidMidiFileInBothProfiles | |
| Content as reviewable text compiled by a deterministic generator | 0.4 | in progress on claude/luthier-feat-riffs | Tools/riffs/*.riffdef | n/a | Riffs.generatorOutputIsCommittedAndDeterministic | |
| Audition is a MIDI-level player, no alloc/lock | 0.5 | in progress on claude/luthier-feat-riffs | Source/Riffs/RiffPlayer | n/a | Riffs.swapsFromAnotherThreadNeverAllocateOnTheAudioThread | |
| No new automatable parameters | 0.6 | in progress on claude/luthier-feat-riffs | - | n/a | parameter count unchanged | |
| 300 factory items per genre/type table and coverage rules | 2.1 | in progress on claude/luthier-feat-riffs | generator check_coverage | n/a | Riffs.catalogMeetsTheCoverageTable | |
| Free set: 60 items, 5 per genre, one bass each, difficulty <= 3 | 2.1 | in progress on claude/luthier-feat-riffs | `free yes` flags; Resources/Riffs/free.txt | n/a | catalogMeetsTheCoverageTable | Data only; nothing gates on it |
| Authoring notation grammar (durations, notes, chords, suffixes, strums, bass prefixes, bar sums) | 2.2 | in progress on claude/luthier-feat-riffs | generate_factory_riffs.py compile_item; Tools/riffs/README.md | n/a | generator run | Extra tokens added (tah, wb, slide in/out variants) |
| Generator outputs files, catalog.json, free.txt; deterministic; CI diff check | 2.3 | in progress on claude/luthier-feat-riffs | write_tree, canonical | n/a | RL-01 test | |
| Technique legality checks | 2.3 | in progress on claude/luthier-feat-riffs | compile_item legality | n/a | generator run | |
| `luthier-render --export-riffs <dir> [--profile]` | 2.3 | in progress on claude/luthier-feat-riffs | Tools/RenderCli.cpp exportRiffs | n/a | manual run | |
| `.luthierriff` schema, limits, conventions; techniques recomputed on load | 3 | in progress on claude/luthier-feat-riffs | Source/Riffs/Riff.{h,cpp} | n/a | Riffs.factoryFilesLoadAndRoundTripByteIdentical, mutatedFilesLoadOrRefuseCleanly | |
| User riffs folder, atomic saves, `user.<type>.<uuid>` ids | 3 | in progress on claude/luthier-feat-riffs | RiffLibrary::saveUserRiff | yes (branch) | Riffs.saveAsRiffAnalysesTheCapture | |
| library.json favourites / recents 50 / play counts | 3 | in progress on claude/luthier-feat-riffs | RiffLibrary::loadGlobal/saveGlobal | yes (branch) | RiffPanel.stateSurvivesReopenAndHostRestoreButNotPresets | |
| file-formats.md row for `.luthierriff` | 3 | in progress on claude/luthier-feat-riffs | spec edit on branch | n/a | - | |
| Riff = metadata + PerformanceScore | 4 | in progress on claude/luthier-feat-riffs | Riff | n/a | RL-06 test | |
| RiffLibrary index (catalog + user scan on ThreadPool at editor open, LRU 32, bitset masks, AND text search) | 4 | in progress on claude/luthier-feat-riffs | Source/Riffs/RiffLibrary | yes (branch) | Riffs.searchAndFilterMatchBruteForceQuickly | |
| RiffAnalysis (Krumhansl key, tempo, techniques, difficulty formula) | 4 | in progress on claude/luthier-feat-riffs | Source/Riffs/RiffAnalysis | n/a | RL-22 test | |
| RiffCompiler pure compile to POD events, curves, strum stagger, BASS_TECH, technique mapping | 5.1 | in progress on claude/luthier-feat-riffs | Source/Riffs/RiffCompiler | n/a | Riffs.compileIsPureAndThreadSafe, techniquesMapOntoTheEngine | Harmonics mapped per REALISM-B contract (Artificial/Tap), not all NaturalHarmonic |
| NoteOnEvent `palmMuteDepth` and `explicitArticulation` | 5.1 | in progress on claude/luthier-feat-riffs | PlayingEvents.h; LuthierEngine::triggerNote | n/a | RL-05, RL-16 tests | |
| Transposition, scale map, placement candidates, chain moves, drops, family octave, whammy as bends, notices | 5.2 | in progress on claude/luthier-feat-riffs | Source/Riffs/RiffTransposer | yes (branch preview status) | Riffs.transposeTakesTheSmallestShiftThatFits, scaleMapMovesDegreesAndKeepsPassingTones, refrettingKeepsPitchAcrossTuningsAndFamilies | |
| "Fits this instrument" filter default on | 5.2 | in progress on claude/luthier-feat-riffs | RiffQuery | yes (branch) | RL-23 test | |
| RiffPlayer in processSubBlock after direct events; SpinLock handover, retired slot, collectGarbage | 5.3 | in progress on claude/luthier-feat-riffs | LuthierEngine::playRiffEvents | n/a | RL-11, RL-14 tests | |
| Clock Auto/Own, next bar/beat start, bar loops, host stop/jump, tempo factor/bpm | 5.3 | in progress on claude/luthier-feat-riffs | RiffPlayer::renderSubBlock | yes (branch) | Riffs.hostClockStartsOnTheNextBarAndLoopsWithoutDrift, ownClockRunsAtTheFactorAndStrumsStayInSeconds | |
| Bends per 64 samples; <= 96 queue slots; bends dropped first; overflow counter | 5.3 | in progress on claude/luthier-feat-riffs | RiffPlayer::emitBends; LuthierEngine::riffBendCents | n/a | Riffs.queuePressureDropsBendPointsNotNotes | |
| Stop hygiene (stop, panic, new riff, preset load, guitar change) | 5.3 | in progress on claude/luthier-feat-riffs | RiffPlayer::releaseAll | n/a | Riffs.stopPanicNewRiffAndResetLeaveNothingSounding | |
| Notes through triggerNote (capture, fretboard, meters); not on live MIDI out; after MIDI Learn | 5.3 | in progress on claude/luthier-feat-riffs | StringActivityEvent::preview; MidiOutRouter::emit | n/a | Riffs.captureSeesRiffNotesExactly | |
| Audition level trim -24..0 dB scales velocity | 5.3 | in progress on claude/luthier-feat-riffs | RiffPlaySettings::levelDb | yes (branch) | - | code review only |
| Rhythm engine coexistence; cascade last event wins; playing mode ignored | 5.4 | in progress on claude/luthier-feat-riffs | playRiffEvents | n/a | Riffs.riffsCoexistWithTheRhythmEngine | |
| Drag to DAW (compile at shown key/tempo, fromScore + STRUM + BASS_TECH, Riffs/Drag file, Drag-as toggle, Alt = Generic, 30-day prune) | 6.1 | in progress on claude/luthier-feat-riffs | RiffDestinations; RiffBrowser::makeDragFile | yes (branch) | Riffs.dragFileIsAValidMidiFileInBothProfiles | |
| fromScore writes CC67/72/73 for pm/pinch/natural | 6.1 | in progress on claude/luthier-feat-riffs | Source/Export/MidiPerformance.cpp addScoreNote | n/a | Riffs.fromScoreWritesPalmMuteAndHarmonicControllers | |
| Save .mid... (Ctrl+E) | 6.1 | in progress on claude/luthier-feat-riffs | DragTile::onClick; RiffBrowser::keyPressed | yes (branch) | RL-26 test | |
| Add to Tune button (section melody/bass manual, transposed, clipped, locks kept, extra riff_str/fret/tech, one undo) | 6.2 | in progress on claude/luthier-feat-riffs | RiffDestinations::addToTune | yes (branch) | RiffPanel.addToTuneKeepsLocksAndIsOneUndo | |
| Drag a row onto the TUNE melody strip (DragAndDropTarget) | 6.2 | no | - | no | - | Not built: RIFFS and TUNE share column 4 |
| TuneMidi prefers MelodyNote::extra for playback | 6.2 | no | - | n/a | - | Coordination point; not found on branch |
| Send to Looper, whole bars at current tempo | 6.3 | in progress on claude/luthier-feat-riffs | RiffDestinations::sendToLooper | yes (branch) | RiffPanel.sendToLooperMakesAWholeBarLayer | Renders via fresh engine, not MidiImportTargets |
| Learn It: drawer TAB, openScore, loop, 70%, speed trainer +5% | 6.4 | in progress on claude/luthier-feat-riffs | TabReaderTab::openScore; RiffBrowser::learnIt | yes (branch) | RiffPanel.learnItOpensTheTabReaderAtSeventyPercent | Every pass counted clean |
| Piano roll lights through SoundingNotes | 6.5 | in progress on claude/luthier-feat-riffs | string activity | n/a | - | code review |
| RIFFS Col 4 tab between TUNE and LIVE, remembered by name | 7.1 | in progress on claude/luthier-feat-riffs | AdvancedPanel::buildWorkspace | yes (branch) | RiffPanel.riffsTabAndDrawerAreReachable | Integration already put JAM in that slot |
| Easy Riff drawer (320 px, ease, Riffs button in rhythm strip, Escape, focus return) | 7.1, 7.3 | in progress on claude/luthier-feat-riffs | EasyPanel::setRiffDrawerOpen | yes (branch) | RL-25 test | |
| `R` shortcut rebindable | 7.1 | in progress on claude/luthier-feat-riffs | Accessibility.cpp `riffs` | yes (branch) | RL-25 test | |
| Options FILE LOCATIONS "Riffs folder"; Options General "Audition on select" | 7.1, 7.2 | in progress on claude/luthier-feat-riffs | FileLocationsPage RIFFS row | yes (branch) | - | No General page; both on FILE LOCATIONS |
| RIFFS layout (search, fits filter, + Save, genre chips, filters, virtual list, preview, status, stacks < 640) | 7.2 | in progress on claude/luthier-feat-riffs | RiffBrowser::resized | yes (branch) | RL-25 test | |
| List columns, sorts, tooltips | 7.2 | in progress on claude/luthier-feat-riffs | RiffBrowser::paintListBoxItem | yes (branch) | RL-26 test | |
| RiffTabView on compiled placement, playhead 30 Hz, stale 250 ms | 7.2 | in progress on claude/luthier-feat-riffs | Source/UI/RiffTabView | yes (branch) | RL-26 test | |
| Controls (select, Space/Enter/Play, Up/Down while playing) and states (playing, waiting, loading) | 7.2 | in progress on claude/luthier-feat-riffs | RiffBrowser::keyPressed, updateStatus | yes (branch) | RL-26 test | States by code review |
| "Key: Tune" option when a tune is loaded | 14 | in progress on claude/luthier-feat-riffs | RiffBrowser keyBox item 14 | yes (branch) | - | |
| Save as riff (marked region / last 2 bars, 1/16 quantise, fields, user riff Edit info/Duplicate/Reveal/Delete) + NOTATION "Save as riff" | 7.4 | in progress on claude/luthier-feat-riffs | RiffBrowser::showSaveDialog; RiffDestinations::riffFromCapture; NotationPanel | yes (branch) | Riffs.saveAsRiffAnalysesTheCapture | |
| Import .mid as riff (Generic via RubricVoicer) | 7.4 | in progress on claude/luthier-feat-riffs | RiffBrowser::importMidiAsRiff | yes (branch) | - | Generic files placed by channel/tuning (toScore), not RubricVoicer |
| Empty states and errors | 7.5 | in progress on claude/luthier-feat-riffs | RiffBrowser::updateStatus | yes (branch) | RiffPanel.emptyAndErrorStatesShowTheirHints | |
| Per-instance uiState; never in presets; audition never restored | 8 | in progress on claude/luthier-feat-riffs | RiffUiState; UiState::riffs | n/a | RiffPanel.stateSurvivesReopenAndHostRestoreButNotPresets | |
| Audition excluded from offline renders | 8 | in progress on claude/luthier-feat-riffs | player only plays when asked | n/a | - | code review |
| Undo: only Add to Tune (and looper's own layer undo) | 9 | in progress on claude/luthier-feat-riffs | TuneSession::edit | n/a | RL-19 test | |
| Accessibility (table role, row names, letter jump, chips, dual sliders, tab text, announcements, focus) | 10 | in progress on claude/luthier-feat-riffs | RiffBrowser::getRowName, getFocusOrder | yes (branch) | RiffPanel.keyboardOrderAndAccessibleNames | |
| Catalog strings; riff.<id>.name lookup; strings.en.json | 10 | in progress on claude/luthier-feat-riffs | Resources/Riffs/strings.en.json | n/a | - | Runtime lookup of riff.<id>.name deferred (English fallback) |
| Editions: Free set playable, locks + upsell, Drag/Add to Tune/Learn It/user riffs Pro, Looper within Free limit; edition::Feature entries; Free installer subset | 11 | no | - | no | - | No edition module |
| Legal: origin + author initials; similarity guard 80% 6-grams | 12 | in progress on claude/luthier-feat-riffs | check_similarity | n/a | RL-24 test | |
| Performance budget (player 0.02 units, compile 2 ms, index 150 ms, search 8 ms) | 13 | in progress on claude/luthier-feat-riffs | - | n/a | Riffs.playerCompileAndIndexStayInBudget | Player bound relaxed to 0.5% of a core |
| Jam mode may use RiffLibrary::query and RiffPlayer with "Key: Jam" | 14 | no | - | no | - | Not on either branch |
| Workshop guitar swap recompiles at next beat; snapshots keep playing; preset load ends notes | 14 | in progress on claude/luthier-feat-riffs | RiffPlayer::notifyEngineReset | n/a | RL-14, RL-31 tests | |
| Failure modes table | 15 | in progress on claude/luthier-feat-riffs | RiffLibrary, RiffTransposer | yes (branch) | RL-27 test | |
| RL-01 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.generatorOutputIsCommittedAndDeterministic | |
| RL-02 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.catalogMeetsTheCoverageTable | |
| RL-03 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.factoryFilesLoadAndRoundTripByteIdentical, Riffs.mutatedFilesLoadOrRefuseCleanly | |
| RL-04 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.compileIsPureAndThreadSafe | |
| RL-05 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.techniquesMapOntoTheEngine | |
| RL-06 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.everyFactoryRiffSurvivesTheLuthierProfile | -60 dBFS render null deferred |
| RL-07 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.dragFileIsAValidMidiFileInBothProfiles | |
| RL-08 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.transposeTakesTheSmallestShiftThatFits | |
| RL-09 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.scaleMapMovesDegreesAndKeepsPassingTones | |
| RL-10 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.refrettingKeepsPitchAcrossTuningsAndFamilies | |
| RL-11 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.swapsFromAnotherThreadNeverAllocateOnTheAudioThread | 200 swaps, not 10 minutes |
| RL-12 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.hostClockStartsOnTheNextBarAndLoopsWithoutDrift | |
| RL-13 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.ownClockRunsAtTheFactorAndStrumsStayInSeconds | |
| RL-14 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.stopPanicNewRiffAndResetLeaveNothingSounding | |
| RL-15 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.queuePressureDropsBendPointsNotNotes | |
| RL-16 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.riffNotesAreExplicitAndKeepTheirStrings | |
| RL-17 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.riffsCoexistWithTheRhythmEngine | |
| RL-18 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.captureSeesRiffNotesExactly | |
| RL-19 | 16 | in progress on claude/luthier-feat-riffs | - | - | RiffPanel.addToTuneKeepsLocksAndIsOneUndo | |
| RL-20 | 16 | in progress on claude/luthier-feat-riffs | - | - | RiffPanel.sendToLooperMakesAWholeBarLayer | Free 60 s limit untestable |
| RL-21 | 16 | in progress on claude/luthier-feat-riffs | - | - | RiffPanel.learnItOpensTheTabReaderAtSeventyPercent | |
| RL-22 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.saveAsRiffAnalysesTheCapture | |
| RL-23 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.searchAndFilterMatchBruteForceQuickly | |
| RL-24 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.catalogNamesAreOriginalAndClean | |
| RL-25 | 16 | in progress on claude/luthier-feat-riffs | - | - | RiffPanel.riffsTabAndDrawerAreReachable | |
| RL-26 | 16 | in progress on claude/luthier-feat-riffs | - | - | RiffPanel.keyboardOrderAndAccessibleNames | |
| RL-27 | 16 | in progress on claude/luthier-feat-riffs | - | - | RiffPanel.emptyAndErrorStatesShowTheirHints | |
| RL-28 | 16 | in progress on claude/luthier-feat-riffs | - | - | RiffPanel.stateSurvivesReopenAndHostRestoreButNotPresets | |
| RL-29 | 16 | no | - | - | - | No edition module |
| RL-30 | 16 | in progress on claude/luthier-feat-riffs | - | - | Riffs.playerCompileAndIndexStayInBudget | Player bound relaxed |
| RL-31 | 16 | in progress on claude/luthier-feat-riffs | - | - | RiffPanel.riffAuditionSurvivesTheCombination | |


### Top gaps (group J)

1. **Five of the nine specs exist only on unmerged branches**: auto-articulation (`claude/luthier-feat-assist`), global-search (`-feat-search`), mic-placement (`-feat-mic`), preset-browser-previews (`-feat-browser`) and riff-library (`-feat-riffs`). Each is essentially complete there, but none is reachable on the integration branch. Expected merge conflicts: RIFFS and JAM both claim the slot between TUNE and LIVE; `NoteOnEvent::explicitArticulation` is added by both assist and riffs; the `riffs`/`search` shortcut entries land on the same line; the parameter-order tests of assist (+4) and mic (+25) must be re-based after jam (+34); and `CabinetEngine::processBlock` is changed by both FEAT-CPU IR variants (merged) and the mic placement stage.
2. **Animated strings ignore AnimationPolicy after the FEAT-CPU merge.** `StringMotionPolicy::getMotion` still returns Off only for Reduced motion. CPU quality Medium/Low and governor relief never reach `StringAnimator`, and `setReliefLevel` has no caller. `StringAnimator` (startTimerHz + VBlankAttachment) has no `AnimationPolicy::Registration` and no allow-list entry, so both CQ-22 tests are expected to fail on integration, and CQ-23's "Low means no animation repaints" is at risk with strings on.
3. **There is no editions infrastructure.** No `Edition.h` on integration, and `LUTHIER_FREE_EDITION` is never defined. As a result AS-30, CQ-29, AA-36, JM edition table, ON-32, PB-35, RL-29 and GS-39 are all source scans, runtime flags or deferred, and no upsell panel exists for search, browser, riffs or assist.
4. **Preview-to-normalization coordination is missing.** feat-browser's renderer has its own loudness and true-peak meter instead of the shared `Bs1770Meter`/`TruePeakDetector`. Its sidecar does not store `truePeakDbtp`. `PreviewPlayer` never applies `OutputNormalization::getPreviewGainOffsetDb`, and the calibrator does not borrow the preview service's instance.
5. **Global search's provider contract (section 8) is unmet for sibling features.** Nothing registers JAM commands and places: the static tab list in `SearchProviders.cpp` lacks JAM and RIFFS, so GS-03 will fail after merge. There is also no `riff:` provider, no preset-browser "open at entry" activation, no "Reset mic placement" command and no auto-articulation ActionDef.
6. **Auto-articulation depends on the unmerged techniques branch.** The TECHNIQUES CASCADE "Auto" row is missing. The Tap-armed and mute-grid context sources are hard-wired false. NOTATION/TAB secondary-accent drawing of automatic techniques is not done.
7. **Jam: several spec items are partial.** PROG looper as a chord source is not implemented. Export MIDI is a plain save chooser, not the midi-export 4.1 dialog. CPU-relief cymbal thinning now follows CPU quality Low rather than the governor, with no test.
8. **Mic placement has three gaps on its branch.** The response plot shows the delta only, without the anchor IR's magnitude. Acoustic factory presets are not moved to `ac_mic_mix` ~0.5. The MP-03 fidelity bounds are relaxed to 3.0/5.5/6.5 dB.
9. **Riff library has four gaps on its branch.** There is no drop target on the TUNE melody strip. `TuneMidi` never reads `MelodyNote::extra` (riff_str/fret/tech), so tune playback can differ from audition. The RL-06 audio null is deferred, and there is no runtime `riff.<id>.name` localisation.
10. **Many tests pass only with relaxed or shortened criteria:**
    - AS-05 window, AS-15 x2 CPU factor
    - CQ-12/13/15 gates
    - MP-03/24
    - PB-15/27 x2
    - RL-11 (200 swaps) and RL-30
    - AA-31 +3%
    - JM-32/33/34 shortened or yardstick-based

    The repo also has no lock trap, so every "no locks on the audio thread" test checks allocations only.

### Unspecified gaps noticed

- The working tree `claude/luthier-audit` is behind the integration branch and does not contain the FEAT-STRINGS/CPU/JAM/NORMALIZE merges (for example, `Source/UI/AnimationPolicy.cpp` is absent). This audit reads the integration branch through `git archive`.
- Two `SoundingNotes` instances coexist: the processor's `SoundingNotesPublisher`, which is per host block, and the engine's own, which is per sub-block. FEAT-STRINGS left unifying them as a follow-up.
- The engine renders about 3 dB louder at 96 kHz than at 48 kHz (FEAT-NORMALIZE decision 4). This rate dependence is unexplained in the engine.
- The amp model aliases at -37 to -39 dBc at gain 10 even at 4x oversampling (FEAT-CPU decision 1).
- `DataStreamDisplay` is declared and registered with AnimationPolicy, but no panel instantiates it.
- About 57 parameters from other workstreams have no visible control or a zero-size control, according to FEAT-SEARCH D11 and GuiReach. `GuiReach.everyAutomatableParameterHasAVisibleControl` and several `Combo.*` tests are reported as failing before any of these features.
- The CQ-13 measured table in docs/coverage/FEAT-CPU.md still contains the placeholders `MEDIUM_X`, `LOW_X` and `SCENARIO_TABLE`.
- Legal: an artist name ("Freddie Green") appears in existing rhythm content, flagged by FEAT-RIFFS.

### Small glue candidates

| Item | IDs / API | Where | Notes |
|---|---|---|---|
| Drive string animation from the CPU policy | `StringMotionPolicy::getMotion` -> `AnimationPolicy::get().getMotion()` / `getStringsStyle()` | Source/UI/Guitar/StringMotionPolicy.cpp | One function body, as its own comment says |
| Register the animator for CQ-22 | `AnimationPolicy::Registration` (Decorative, "StringAnimator"), or start its timer through the registration | Source/UI/Guitar/StringAnimator.h/.cpp | Registration takes a Component: pass the owner, or add an allow-list entry with the reason "drives owner repaint" |
| Feed the animator's relief hook | `AnimationPolicy` listener -> `StringAnimator::setReliefLevel` | GuitarBodyComponent / FretboardComponent `motionPolicyChanged` | cpu-quality-modes 7 says "fed only through AnimationPolicy" |
| Preview level under normalization | `OutputNormalization::getPreviewGainOffsetDb (truePeakDbtp)` | feat-browser PreviewPlayer start; sidecar `truePeakDbtp` in PreviewRenderer::sidecarFor | Needs feat-browser merged |
| Share the loudness meter | `Bs1770Meter`, `TruePeakDetector` in place of `ToneFeatures::integratedLoudness/truePeakDb` | feat-browser Source/Presets/Preview/PreviewRenderer.cpp | Keeps the two -18 LUFS measurements identical |
| Tune prefetch for normalization | `OutputNormalization::prefetchPresets` | TuneSession load path | Only the setlist path calls it today |
| Search place rows | `tab:JAM`, `tab:RIFFS` in the static tab list | feat-search Source/UI/Search/SearchProviders.cpp:335 | Otherwise GS-03 fails after merge |
| Search commands for jam and assist | shortcut ids `jamStartStop`, `jamFill`, `jamArm`, `toggleAssist` | ActionRegistry (automatic from `AccessibilitySettings`) | Arrive for free once performAction is merged; check titles and synonyms |
| Jam Export MIDI via the standard dialog | midi-export 4.1 dialog with a "Jam capture" range | Source/UI/JamPanel.cpp Export MIDI | Today it opens a save chooser |
| Assist technique context | `AssistExplicitContext::tapArmed`, `muteGridActive` | feat-assist LuthierEngineAssist.cpp `assistSetContext` | Fill from TECHNIQUES once claude/luthier-techniques merges |
| Riff-to-tune fidelity | read `MelodyNote::extra` `riff_str` / `riff_fret` / `riff_tech` | TuneMidi | So tune playback matches audition |
| One explicit-articulation field | `NoteOnEvent::explicitArticulation` | Source/Model/Playing/PlayingEvents.h | Added by both feat-assist and feat-riffs; keep one at merge |
| Column 4 tab order | `{ "JAM" }`, `{ "RIFFS" }` both "between TUNE and LIVE" | Source/UI/AdvancedPanel.cpp tabs table | Decide TUNE, JAM, RIFFS, LIVE (feat-riffs' scrolling tab strip absorbs the width) |
