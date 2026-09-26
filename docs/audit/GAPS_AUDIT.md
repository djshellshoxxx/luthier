# Spec gap audit

Auditor branch `claude/luthier-audit`. Code audited: the integration branch
`claude/luthier-cloud-session-5lzlix` at `6bda872` (review merged; none of the
seven helper branches landed yet). Helper branches were inspected with
`git diff origin/claude/luthier-cloud-session-5lzlix...origin/claude/luthier-<name>`,
and items implemented only there are marked "in progress on <branch>".

Method: every Markdown file under `spec/` and `spec/proposals/` was read in full
and split into actionable requirements (controls, behaviours, UI locations,
file fields, defaults, shortcuts, and every item in its Tests section). Each was
checked against `Source/`, `Resources/`, `Tools/`, `CMakeLists.txt` and
`scripts/`. A class existing is not "yes" unless something calls it; a
parameter is not GUI-reachable unless a visible control is attached to it.
`Source/WIP/` is not compiled, so code there counts as "partial (WIP)". GUI
reachability was then checked mechanically by `GuiReach.*` (see
BETA_TEST_REPORT.md B-11), and that result overrides a desk check where they
disagree.

This file is refreshed after each helper branch lands. Per-requirement tables
are in "Detail by spec file" below.

## Summary by spec file

"Partial" includes "in progress on <branch>". The realism and technique
specs (groups E-G) score near zero here because their work sits on
realism-a/b/c, techniques and model-gaps, which have not landed.

| Spec file | Group | Yes | Partial / in progress | No | Rows | % done |
|---|---|---|---|---|---|---|
| spec.md | A | 123 | 47 | 16 | 186 | 66% |
| engine.md | A | 75 | 20 | 4 | 99 | 76% |
| include.md | A | 20 | 5 | 1 | 26 | 77% |
| theme.md | A | 16 | 10 | 8 | 34 | 47% |
| issues.md | A | 3 | 6 | 3 | 12 | 25% |
| README.md | A | 15 | 4 | 3 | 22 | 68% |
| TODO.md | A | 2 | 11 | 17 | 30 | 7% |
| CLAUDE_CODE_BRIEF.md | A | 4 | 10 | 8 | 22 | 18% |
| gui-integration.md | B | 66 | 34 | 33 | 133 | 50% |
| ui-wiring.md | B | 20 | 21 | 13 | 54 | 37% |
| gui-engine-dataflow.md | B | 12 | 15 | 8 | 35 | 34% |
| gui-techniques-updates.md | B | 2 | 4 | 25 | 31 | 6% |
| workshop-ui.md | B | 36 | 12 | 6 | 54 | 67% |
| proposals/visual-polish.md | B | 21 | 2 | 4 | 27 | 78% |
| onboarding.md | B | 10 | 13 | 15 | 38 | 26% |
| accessibility.md | B | 16 | 18 | 13 | 47 | 34% |
| guitar-illustration.md | C | 46 | 43 | 12 | 101 | 46% |
| guitar-workshop.md | C | 35 | 9 | 1 | 45 | 78% |
| part-acoustics.md | C | 38 | 11 | 10 | 59 | 64% |
| advanced-ranges.md | C | 36 | 8 | 3 | 47 | 77% |
| tune-builder.md | C | 49 | 34 | 5 | 88 | 56% |
| routing-io.md | D | 22 | 3 | 1 | 26 | 85% |
| modulation-matrix.md | D | 27 | 10 | 3 | 40 | 68% |
| rhythm-engine.md | D | 25 | 3 | 0 | 28 | 89% |
| live-performance.md | D | 24 | 10 | 4 | 38 | 63% |
| controllers.md | D | 8 | 9 | 4 | 21 | 38% |
| practice-tools.md | D | 39 | 14 | 6 | 59 | 66% |
| tone-match.md | D | 18 | 5 | 6 | 29 | 62% |
| notation-export.md | D | 17 | 10 | 2 | 29 | 59% |
| character-wear.md | D | 18 | 12 | 0 | 30 | 60% |
| updates-telemetry.md | D | 13 | 5 | 3 | 21 | 62% |
| midi-export.md | D | 19 | 7 | 1 | 27 | 70% |
| input-routing.md | D | 8 | 9 | 8 | 25 | 32% |
| volume-knob-interaction.md | E | 21 | 3 | 1 | 25 | 84% |
| pick-noise.md | E | 19 | 5 | 3 | 27 | 70% |
| string-squeak.md | E | 20 | 2 | 3 | 25 | 80% |
| fret-buzz.md | E | 19 | 1 | 5 | 25 | 76% |
| slide-guitar.md | E | 20 | 5 | 2 | 27 | 74% |
| bass-techniques.md | E | 10 | 5 | 13 | 28 | 36% |
| string-aging.md | E | 0 | 2 | 21 | 23 | 0% |
| environment.md | E | 0 | 2 | 19 | 21 | 0% |
| body-coupling.md | E | 0 | 0 | 18 | 18 | 0% |
| harmonic-realism.md | F | 0 | 1 | 34 | 35 | 0% |
| string-interaction.md | F | 0 | 0 | 30 | 30 | 0% |
| fingerstyle-attack.md | F | 1 | 1 | 43 | 45 | 2% |
| noise-floor.md | F | 1 | 2 | 38 | 41 | 2% |
| sustain-and-decay.md | F | 1 | 1 | 35 | 37 | 3% |
| tuning-stability.md | F | 1 | 2 | 38 | 41 | 2% |
| strum-dynamics.md | F | 25 | 3 | 0 | 28 | 89% |
| string-scraping.md | G | 19 | 4 | 1 | 24 | 79% |
| slide-technique-controls.md | G | 1 | 0 | 17 | 18 | 6% |
| string-slap-technique.md | G | 16 | 8 | 1 | 25 | 64% |
| muting-rhythm.md | G | 0 | 3 | 17 | 20 | 0% |
| two-hand-tapping.md | G | 0 | 1 | 18 | 19 | 0% |
| microtonal-bends.md | G | 0 | 4 | 16 | 20 | 0% |
| technique-cascade.md | G | 1 | 3 | 7 | 11 | 9% |
| engine-technique-layer.md | G | 6 | 5 | 9 | 20 | 30% |
| ambiguity-resolutions.md | G | 21 | 3 | 0 | 24 | 88% |
| qa-polish.md | G | 12 | 26 | 12 | 50 | 24% |
| performance-budget.md | G | 3 | 6 | 15 | 24 | 12% |
| installer.md | G | 2 | 11 | 21 | 34 | 6% |
| file-formats.md | H | 20 | 22 | 10 | 52 | 38% |
| factory-content.md | H | 9 | 10 | 15 | 34 | 26% |
| error-recovery.md | H | 16 | 31 | 34 | 81 | 20% |
| state-model.md | H | 28 | 23 | 22 | 73 | 38% |
| host-integration.md | H | 29 | 21 | 9 | 59 | 49% |
| action-and-undo.md | H | 17 | 11 | 21 | 49 | 35% |
| DECISIONS.md | I | 72 | 14 | 8 | 94 | 77% |
| GAPS.md | I | 25 | 5 | 7 | 37 | 68% |
| PROGRESS.md | I | 21 | 6 | 5 | 32 | 66% |
| INDEX.md | I | 4 | 9 | 8 | 21 | 19% |
| REVIEW.md | I | 14 | 2 | 0 | 16 | 88% |
| JUCE_CLAUDE_GUIDELINES.md | I | 22 | 10 | 5 | 37 | 59% |
| **Total** | | **1379** | **707** | **827** | **2913** | **47%** |
| piano-roll-chord-display.md | (new, 2026-09-24) | 0 | 0 | 23 | 23 | 0% |

`spec/piano-roll-chord-display.md` arrived on the integration branch after
the audit agents ran. Nothing in it is built on any branch: no piano-roll
strip (sections 1-3: keyboard, 4 s roll, Advanced strip under the fretboard,
Easy strip under the illustration, ROLL/KEYS, Latch, Show fingering,
`SoundingNotes` snapshot at 30 Hz, playing from the keys through the
on-screen-keyboard MIDI path), no chord name on the illustration (section 4:
pitch-class naming, E5 power chords, slash chords, 60 ms fade in, 1.2 s hold,
0.8 s fade out, 30 ms merge), no Options toggles (section 5), none of tests
PR-01..07, CD-01..05, OP-01..02. `ChordDetector` exists and can supply the
names.

## Helper branch status

| Branch | Commits ahead of integration | Landed? | Covers (from this audit) |
|---|---|---|---|
| realism-a | 4 | no | string-aging, environment, body-coupling (temperature/humidity become audible) |
| realism-b | 1 | no | harmonic-realism, string-interaction, fingerstyle-attack |
| realism-c | 3 | no | noise-floor, sustain-and-decay, tuning-stability (29 params, no UI yet) |
| techniques | 9 | no | TECHNIQUES tab: scrape/slap controls, muting, tapping, microtonal bends, cascade |
| model-gaps | 6 | no | bass-techniques defaults and controls, part fields |
| tune-help | 11 | no | TUNE editor and export, onboarding FirstRun / hints / restore first-run, HELP |
| visual | 19 | no | workspace panels no longer 80 px slivers, illustration items, W shortcut, thumbnails, drag and drop |
| release | 4 | no | (installer / packaging) |

Merge risks the agents spotted: realism-a and realism-c both create
`Source/UI/RealismGroups.*`; model-gaps and realism-b both declare
`rest_stroke` and `finger_alternation_variation`.

## Top gaps across the product

Ranked by what a buyer meets first. B-numbers refer to BETA_TEST_REPORT.md.

1. **Runaway strings after the rhythm engine** (B-02): output grows to -14 dBFS with nothing played; panic does not stop it.
2. **Nothing stops everything** (issues.md 9, B-14): Panic and Reset leave looper, backing track, tune player, progression looper, metronome and rhythm running.
3. **Technique controls missing**: 39 scrape/slap parameters have engines and no controls; TECHNIQUES tab, Easy pills and fretboard overlays absent (gui-techniques-updates 6% done). In progress on techniques / model-gaps.
4. **Realism specs not landed**: string-aging, environment, body-coupling, harmonic-realism (natural harmonics an octave high on HEAD), string-interaction, fingerstyle-attack, noise-floor, sustain-and-decay, tuning-stability all ~0% on HEAD; on realism-a/b/c.
5. **Released notes ring on** (B-03), sustain-and-decay SUS-08.
6. **Workspace tabs render as an 80 px sliver** (MOD, RHYTHM, LIVE, ROUTING, TONE MATCH); fix on visual, but RHYTHM still clips its STRUM group (B-11).
7. **Dead controls**: backing-track pitch/tempo, expression-pedal calibration, controller latency wizard, controller-profile MPE/bend overwritten every block, `pickup_blend`, six Character controls (pot taper, cap drift, jack, body age, temperature, humidity), Custom amp model, UI scale.
8. **Presets lose state**: a saved `.luthierpreset` drops the modulation matrix, snapshots, MIDI Learn, rhythm, character and tone-match state (file-formats / state-model); only the session state keeps them.
9. **Error recovery thin** (error-recovery 20%): most specified banners, fallbacks and repair paths missing.
10. **Guitar illustration and Workshop**: body swap never redraws the outline (14 of 34 outlines unreachable); no finish/colour editor; four Workshop slots (top, fretboard, tailpiece, pickguard) have no drawer category, so 21 shipped parts cannot be fitted; slide part never reaches the engine (fix on visual).
11. **Onboarding absent** (26%): no welcome, tour, hints; on tune-help.
12. **Localisation English-only**: about 288 literal UI strings vs 12 `tr()` calls; no `Resources/i18n`.
13. **Release engineering**: no installers (installer 6%), no AU on Linux-built tree (AU added for macOS by review), no AAX, no CI running pluginval, placeholder support links, missing `THIRD_PARTY_LICENCES.txt`, no crash reporter.
14. **Performance budget unenforced** (12%): idle CPU ~ playing CPU (B-13), no per-module budget tests, no allocation hook in test mode.
15. **Undo not as specified** (action-and-undo 35%): no 200 ms grouping, depth 200 not 64, routing/mod edits bypass undo, undo restores the whole state including the tune.
16. **Input routing** (32%): consumer order, footswitch/CC for live actions, file-drop handler, MIDI import.
17. **Header incomplete**: snapshot strip, meters, tap tempo, dice/reset/gear.
18. **Accessibility gaps**: meter/overlay announcements never called, fretboard has no per-fret accessible children, snapshot buttons unnamed.
19. **Piano roll and chord display** (new spec): not started.
20. **Host integration**: `NEEDS_MIDI_OUTPUT FALSE` while `producesMidi()` is true and a MIDI OUT tab exists; program list shifts when user presets are saved; program changes swallowed after undo/A-B/setlist steps.

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
- Text search across name/author/tags; tag filter chips; favourite star and a Favourites view; stable factory program numbers for hosts (user presets appended after, not interleaved); unsaved-changes prompt before loading another preset; "restore factory content" button.
- Tests: search latency < 50 ms with 2000 presets; favourites persist; saving a user preset does not renumber factory programs.

### U-4 Undo history list (`spec/undo-history.md`)
- A menu/panel listing named steps (the descriptions already recorded), click to jump; routing, modulation, snapshot and Workshop edits all on the stack; tune edits on their own stack.

### U-5 On-screen keyboard and technique audition (`spec/audition.md`)
- Playable keyboard (overlaps piano-roll spec section 3; merge there), per-technique demo phrases (scrape, slap, tap, bend, harmonics, palm mute), keyswitch legend showing what KS 12-21 and articulation keys do, a harmonic trigger reachable without CC controllers.

### U-6 Left-handed mode (`spec/left-handed.md`)
- Mirror the illustration, fretboard and Workshop bench; strum direction semantics unchanged; saved per user.

### U-7 Out-of-range note policy (`spec/note-range.md`)
- Notes below the lowest open string (after capo) or above the last fret: options Drop (with a one-time notice), Octave-fold (default), Nearest string; notice in the status line. Today they are silently dropped (B-09).

### U-8 Quality / CPU mode and meter (`spec/quality-modes.md`)
- Eco / Realtime / Render quality switch in Easy mode (oversampling, body IR length, noise pool size), a CPU and voice meter in the header, an overload indicator tied to performance-budget section 8's relief mechanisms, and an idle short-circuit (silent strings, silent input -> skip body/amp/cab/room after their tails; B-13).

### U-9 Deterministic render (`spec/determinism.md`)
- Every random source (humanise, noise pools, character wear, realism detune) seeded from state, so the same session renders bit-identically in any instance and after reset; offline render mode flag. Test: B-08's harness at zero difference.

### U-10 Parts and guitar library management (`spec/library-management.md`)
- Rename, duplicate, delete, import/export `.luthierguitar` bundles and parts, reveal folder, rescan; a preset bundle (preset + guitar + IRs) for sharing.

### U-11 DI / re-amp input (`spec/reamp.md`)
- Process a real guitar through the rig (amp, cab, room, pedals) from the sidechain input, with the modelled guitar muted; include.md hints at an effects version.

### U-12 MIDI monitor (`spec/midi-monitor.md`)
- A view showing each incoming event and which consumer took it (input-routing's order), for "why isn't MIDI Learn seeing my CC".

### U-13 Drag-out clips from TUNE (`spec/tune-dragout.md`)
- Drag a section or the whole tune as MIDI or audio straight into the DAW.

## Small glue candidates

Engine features that lack only an attachment, a call, or a preset field. The
ones in files under active helper development are listed for the owner, not
done here.

| Item | IDs / API | Where | Owner / status |
|---|---|---|---|
| Macro 7/8 knobs | `macro_assign_a`, `macro_assign_b` | MOD tab header; Easy strip | open (ModMatrixPanel is being changed on visual) |
| Pickup blend | `pickup_blend`; `PickupEngine` must read `blendAmount` | Col 2 PICKUPS | open (PickupEngine changed on realism-b/c) |
| Scrape and slap groups | 14 `scrape_*`, 25 slap/pop/ghost | TECHNIQUES tab | in progress on techniques / model-gaps |
| RHYTHM panel height | `RhythmPanel::preferredHeight()` into `AdvancedPanel::resized` | AdvancedPanel | open (AdvancedPanel changed on visual) |
| Stop everything | looper, backing track, tune player, metronome, progression, rhythm, freeze, effect tails into `LuthierAudioProcessor::panic()` | PluginProcessor | open (~15 lines) |
| Backing track pitch/tempo | `BackingTrackPlayer::setPitchShiftSemitones`, `setTempoRatio` read by the renderer | Practice | open |
| Expression calibration | call `ExpressionCalibration::observe()` / `map()` from the MIDI path | PluginProcessor | open |
| Controller profile MPE/bend | stop `ParameterBridge::applyToEngine` overwriting them each block (Parameters.cpp) | Parameters | open |
| Character outputs | `applyPotTaper`, `getCapacitorDrift`, `getJackGain`, balance getters into the engine | LuthierEngine | open (realism-a covers temperature/humidity) |
| Air resonance / chambering / gloss | `airResonanceHz/Q`, `bodyGainDb`, `finishDampingDb` into the engine | LuthierEngine | open |
| Workshop drawer categories | top, fretboard, tailpiece, pickguard slots | WorkshopPanel | open |
| MIDI output flag | `NEEDS_MIDI_OUTPUT TRUE` in CMakeLists | CMake | open (host-integration decision) |
| MP3 decoding | `JUCE_USE_MP3AUDIOFORMAT=1` or drop .mp3 from the chooser | CMake / Practice | open |
| Done on this branch | preset-morph position in session state (B-10), engine lock (B-01) | | fixed |

Each group's own "Small glue candidates" table below has the full list with
exact IDs (group A 17, C 20, D 22, E 11 items, others).

# Detail by spec file

Groups: A core (spec, engine, include, theme, issues, README, TODO, brief);
B GUI; C illustration, Workshop, parts, ranges, tune builder; D phase-1
extensions; E phase-2 realism; F extended realism; G techniques, QA,
performance, installer; H deep integration; I meta files.


---

## Group A



Audited: `origin/claude/luthier-cloud-session-5lzlix` at f63a7f7 (the local checkout of `claude/luthier-audit` is the same commit). Helper branches checked: `realism-c` (ba777f9), `tune-help` (5911597), `visual` (9f67749). `realism-a`, `realism-b`, `techniques` and `model-gaps` did not exist yet.

Abbreviations: AP = `Source/UI/AdvancedPanel.cpp`, EP = `Source/UI/EasyPanel.cpp`, HB = `Source/UI/HeaderBar.cpp`, PB = `ParameterBridge::applyToEngine` in `Source/Parameters.cpp`, LE = `Source/LuthierEngine.cpp`, PP = `Source/PluginProcessor.cpp`, Col1/2/3 = Advanced columns, "tab X" = Advanced column-4 workspace tab. Test names are `Suite.test` from `Source/Tests/*` (743 tests in total).

Parameter reachability was checked mechanically: every `ParamIDs::` constant was grepped against `Source/UI/*` and `PluginEditor.cpp`. **Parameters with no attached control anywhere:** `macro_assign_a`, `macro_assign_b`, `pickup_blend`, all 14 `scrape_*`, all 25 slap/pop/ghost/`double_thump_*` params, and legacy `fret_action`, `strum_speed`, `feedback_on/threshold/speed`, `doubler_on/amount`. Retired and superseded on purpose: `pickupN_position` and `pickupN_height`, which moved to the Workshop bench.

---

### spec.md

**Summary:** 186 requirements. **123 yes / 47 partial / 16 no.** The engine layer (string, body, pickup, whammy, amp, cab, room, MIDI modes) is broadly real and tested. The largest gaps are UI and deliverables. Per-string editing (material, gauge, age, sustain, fretless, custom frets) does not exist. Easy mode has no clickable fretboard, and its macros are small knobs, not large. The scrape and slap techniques have no controls, custom amp has no controls, and `pickup_blend` is dead. There is no audio drag-out and no AU target. There is no CI or pluginval run, and there are no installers. Of the 12 "ship without hotfix" gates, only the automated ones have evidence.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Waveguide per string, up to 12 strings | 1 String | yes | DSP/String/StringEngine; LE numStrings | n/a | StringEngine.pluckProducesCorrectPitch; GuitarLibrary.twelveStringCoursesAreOctavePaired | |
| Scale length 500-750 mm | 1 | partial | GuitarSpec.scaleLengthMm, Workshop neck part | Workshop tab (part swap / inspector) | WorkshopBench.* | No direct scale knob |
| Tension from pitch, mass, length | 1 | yes | StringMaterials; LE getStringTensionNewtons | Col1 string rows (display) | StringPhysics.standardSetsLandInTheUsualTensionRange | |
| Linear density from gauge and material | 1 | yes | StringMaterials | Col1 String Set | StringPhysics.thickerStringsAreHeavierAndTighter | |
| Young's modulus / stiffness | 1 | yes | StringMaterials (core diameter) | no (derived) | StringPhysics.woundStringsAreLessStiff… | |
| Frequency-dependent damping | 1 | yes | StringEngine loop filter | Col1 Sustain knob `sustain_scale` | StringEngine.higherNotesDecayFaster | |
| Continuous fretting position 0-24 | 1 | yes | TuningEngine fretPosition (double) | n/a | Tuning.fretPositionIsContinuous | |
| Pluck position | 1 | yes | `pluck_position` → Excitation | Col2 Playing Hand | — | |
| Pluck strength = velocity | 1 | yes | Excitation velocity | n/a | StringEngine.harderPluckIsBrighterNotJustLouder | |
| Pluck material pick/thumb/nail/thumbpick/brush | 1 | yes | Excitation::Material (12) | Col2 "Pick / Finger" | — | |
| Pick 2-4 ms bright asymmetric excitation | 1 | yes | Excitation::specFor | n/a | — | |
| Finger 5-8 ms, thumb low-passed, nail vs pad | 1 | yes | Excitation; `nail_vs_flesh` | Col2 | — | |
| Inharmonicity B per string, wound > plain | 1 | yes | StringEngine dispersion allpass | no | StringEngine.dispersionStretchesPartialsSharp | |
| Sympathetic NxN coupling matrix | 1 | yes | DSP/Coupling/CouplingMatrix | Col1 Sympathetic `coupling_amount` | Coupling.* (4) | Per-sample, deliberate deviation |
| Two-stage decay (fast HF 200 ms, then slow) | 1 | partial | StringEngine single loop filter + T60 | no | — | Explicit fast stage in progress on realism-c (`sustain_fast_share/ratio`) |
| Sustain per string (Advanced) | 1 | partial | `sustain_scale` is global only | Col1 (global) | Sustain.* | No per-string sustain |
| Body: convolution with IR | 2 Body | yes | DSP/Body/BodyEngine convolution | Col1 Body Mode | Engine.everyGuitarTypeLoadsAndSounds | |
| Body IR library for every type | 2 | yes | Resources/BodyIRs (216 wav, 18 shapes) | n/a | — | Synthesised, not measured |
| Modal bank 20-40 modes | 2 | yes | BodyEngine resonators (kMaxModes 48) | Body Mode = Modal | Body.modalBankReproducesTheAirResonance | |
| Modes shift with dimensions | 2 | yes | BodyModels; `body_width/depth` | Col1 Body | Body.dimensionsMoveTheModes | |
| Wood type (8 woods) with modal profile | 2 | yes | Wood enum (16) | Col1 top/back wood | — | |
| Body size small…parlor | 2 | partial | BodyShape via Workshop body part | Workshop tab | — | No size selector; width/depth knobs |
| Body depth | 2 | yes | `body_depth` | Col1 Body | — | |
| Top thickness | 2 | yes | `body_top_thickness` | Col1 Body | — | |
| Bracing incl. solid/semi/hollow | 2 | yes | `body_bracing` (8) | Col1 Body | — | |
| Sound-hole size | 2 | yes | `body_soundhole` | Col1 Body | — | |
| Air resonance frequency | 2 | partial | `body_air_gain` (gain only) | Col1 Body | Body.modalBank… | Frequency not user-set |
| Age simulation | 2 | yes | `body_age` | Col1 Body | — | |
| Pickup types (SC, HB, P90, piezo, soundhole, mic) | 3 Pickup | yes | PickupEngine; `pickupN_type` | Col2 Pickups | Pickup.* | |
| Blended piezo + mic balance | 3 | yes | `piezo_mic_blend` | Col2 Pickups | — | |
| Position along string | 3 | yes | Workshop placement (params retired) | Workshop bench drag | Pickup.positionCombNullsTheExpectedHarmonic; WorkshopBench.* | |
| Coil count / spacing | 3 | partial | HB = 2 coils in PickupEngine; spacing via part fields | Workshop inspector (double-click field) | — | |
| Coil R / L / C | 3 | partial | Part fields → GuitarCircuit | Workshop inspector | Pickup.resonantFrequencyMatchesTheLcrValues | Only as raw part fields |
| Magnet type EQ | 3 | yes | `pickupN_magnet` | Col2 Pickups | — | |
| Pole spacing | 3 | no | — | no | — | |
| Height above strings | 3 | yes | Workshop placement height | Workshop bench scroll | — | |
| Pickup selector positions | 3 | yes | `pickup_selector` (7) | Col2 and guitar illustration switch | — | No "none"/per-pickup on-off |
| Continuous blend knob | 3 | no | `pickup_blend` set in PB, but PickupEngine never reads `blendAmount` | no | — | Dead parameter |
| Coil-tap | 3 | yes | `coil_tap` | Col2 Pickups | — | |
| Fretted mode snaps to fret | 4 Fret | yes | TuningEngine / MidiInterpreter | n/a | Engine.chromaticScalePlaysAtTheRightPitch | |
| 24 frets default, 12-27 per type | 4 | yes | GuitarSpec.maxFrets | n/a | GuitarLibrary.everyEntryIsInternallyConsistent | |
| Temperaments (just, meantone, well, custom) | 4 | partial | Temperament (7) | Col1 Temperament; headstock popover | Tuning.temperamentsDifferButStayInRange | Custom ratios are preset-only, no editor |
| Fret noise | 4 | yes | `noise_fret` | Col2 String Noise | NoiseTests | |
| Fret buzz physical | 4 | yes | DSP/Noise/FretBuzz; setup geometry | tab CHARACTER › SETUP; Col1 "Contact" | Buzz.* | |
| Fret action height | 4 | yes | `setup_action_treble/bass` (fret_action retired) | CHARACTER SETUP; Workshop | Buzz.* | |
| Fretless mode continuous | 4 | yes | `fretless` | Col1 Neck | Engine.fretlessModeIsGenuinelyContinuous | Global, not per string |
| Fretless: no buzz, softer attack, less sustain | 4 | partial | Technique fretless glide | n/a | Technique.fretlessTurnsLegatoIntoGlide | Buzz/attack differences not tested |
| Bend (cents, multi-string) | 4 Tech | yes | pitch bend / MPE per string | n/a | StringEngine.bendIsSmoothAndReachesTarget | |
| Pre-bend | 4 | partial | Works only as a bend sent before note-on | no | — | No pre-bend technique |
| Vibrato rate/depth | 4 | yes | `vibrato_rate/depth` | Col3 Performance | — | |
| Vibrato shapes incl. classical / blues | 4 | partial | `vibrato_shape`: Sine, Tri, Square, Saw, Random, Finger | Col3 | — | Classical and blues missing |
| Slide legato vs picked | 4 | yes | TechniqueEngine Slide | n/a | Technique.fastNotesBecomeASlide; Technique.slideDurationScalesWithDistance | |
| Hammer-on / pull-off | 4 | yes | TechniqueEngine | n/a | Technique.legatoBecomesHammerOnAndPullOff | |
| Palm mute, depth | 4 | yes | CC67 → Damping::PalmMute | no UI (CC only) | StringEngine.palmMuteShortensAndDarkens | |
| Natural harmonic | 4 | yes | Excitation Harmonic; CC73 / velocity | CC only | Technique.harmonicNodesAreDetected | |
| Pinch harmonic | 4 | yes | CC72 trigger | CC only | — | |
| Artificial harmonic | 4 | partial | Technique::ArtificialHarmonic | no | — | No trigger control |
| Tapping | 4 | yes | CC74 / Tap technique | CC only | TechniqueTriggers.* | |
| Slide guitar (bottleneck) toggle | 4 | yes | SlideEngine; `slide_guitar` | Header "Slide", key S, CHARACTER SLIDE | Slide.* | |
| Whammy vintage / locking / transposing, dive | 4 | yes | WhammyEngine; `bridge_type` | Col1 Bridge; illustration bridge popover | Whammy.transTremPreservesChordIntervals | |
| String / pick scrape | 4 | partial | ScrapeEngine wired in PB | **no**: 14 `scrape_*` params unattached, `scrape_armed` defaults off | Scrape.* (15) | TECHNIQUES tab not built |
| Muted picking | 4 | yes | Technique MutedPick; CC71 | CC only | — | |
| Pick or fingers, per string or per note | 5 Right hand | partial | `use_fingers` global | Col2 | — | Not per string or per note |
| Pick material (6) | 5 | yes | Excitation materials | Col2 | — | |
| Pick thickness | 5 | yes | `pick_thickness` | Col2 and CHARACTER PICK | PickNoise.* | |
| Pick angle | 5 | yes | `pick_angle` | Col2 | — | |
| Pick position in mm | 5 | partial | `pluck_position` normalised | Col2 | — | Not in mm |
| Fingers thumb…little, finger-to-string assignment | 5 | partial | Rhythm/Patterns Finger (pattern only) | RHYTHM tab patterns | RhythmPatterns.* | No user finger assignment |
| Nail vs flesh | 5 | yes | `nail_vs_flesh` | Col2 | — | |
| Strum direction and delay 2-15 ms | 5 | yes | `strum_direction`, `strum_crossing_sps` | Col3 Performance; RHYTHM STRUM | StrumDynamics.* | |
| Strum speed | 5 | yes | `strum_crossing_sps` | Col3 and STRUM group | StrumDynamics.* | |
| Finger slide squeak (speed, wound) | 6 Noise | yes | NoiseEngine squeak | CHARACTER STRING NOISE | Squeak.* | |
| Pick attack transient | 6 | yes | pick click/chirp | CHARACTER PICK; Col2 | PickNoise.* | |
| Fret noise | 6 | yes | `noise_fret` | Col2 | — | |
| Release noise | 6 | yes | `noise_release` | Col2 | — | |
| Body knock | 6 | yes | `noise_body_knock` | Col2 | — | |
| Pickup handling noise | 6 | no | — | no | — | |
| Amp buzz 60 Hz for single-coils | 6 | yes | `noise_amp_buzz` | Col2 | — | 50/60 Hz choice on realism-c (`noise_mains_hz`) |
| Global and per-type mechanical volume | 6 | partial | per-type knobs; macro_character | Col2; CHARACTER | NoiseUi.* | No single global noise level |
| 11 electric + 8 acoustic + 5 bass + custom types | Guitar types | yes | GuitarLibrary (24 + Custom); 28 guitar files | Header guitar selector | Engine.everyGuitarTypeLoadsAndSounds | Trademark-renamed |
| Custom guitar from scratch; save as user preset | Guitar types | yes | Workshop parts; Save As Guitar (Ctrl+G) | tab WORKSHOP | Workshop*, WorkshopPresets.* | |
| 13 string materials | Strings | partial | StringMaterial (12) | Col1 String Set | StringPhysics.* | "Roundwound" not a separate entry |
| Gauges XL…Heavy + custom per string | Strings | partial | StringGauge (11 + Custom); `customGaugeInches` preset field | Col1 (set only) | — | Custom per-string gauge has no editor |
| String age fresh / broken-in / old | Strings | yes | `string_age` | Col1 | StringPhysics.ageDullsAndShortens | |
| Material/gauge per string selectable | Strings | no | global params only | no | — | Spec says per string |
| Standard + 11 alternate tunings | Tuning | yes | TuningPreset (17 + Custom) | Header tuning; headstock popover | Tuning.everyPresetProducesSaneFrequencies | |
| Custom per-string tuning (any note) | Tuning | partial | `openFrequencyHz` / `useCustomTuning` preset fields | no note picker; detune ±100 only | — | |
| Per-string detune ±100 c | Tuning | partial | TuningEngine.detuneCents | Headstock popover sliders | Editor.theHeadstockPopoverEditsPerStringTuning | Not a parameter; written from message thread |
| Realism detune 0-20 c, refreshed on load / request | Tuning | partial | `realism_detune`; PB randomise with fixed seed 0x9E3779B9 | Col1 Tuning Realism | — | Same pattern every time; no re-roll button |
| Intonation error scales with fret | Tuning | yes | `intonation_error` | Col1 | Tuning.intonationErrorGoesSharpUpTheNeck | Global slope only |
| Fine tuner per string | Tuning | partial | TuningEngine.fineTuneCents | no | — | Only character wear sets it |
| Mono mode with legato | Playing modes | yes | MidiInterpreter Mono | EP Mode; RHYTHM | Technique.* | |
| Poly / chord voicing, detection, strum | Playing modes | yes | ChordVoicer, RubricVoicer | EP Mode | ChordVoicer.*; RubricVoicer.*; Engine.aChordVoicesAcrossStrings | |
| Guitar controller: channel = string, MPE | Playing modes | yes | MidiInterpreter; ControllerProfile | EP Mode; tab CONTROLLERS | Controllers.* | |
| Auto technique: bend, AT vibrato, mod wheel, sustain, sostenuto | Auto detect | yes | MidiInterpreter ccMap, sostenuto | n/a | Technique.controllersTakePriorityOverInference | |
| Mod wheel → vibrato OR whammy (user-mapped) | Auto detect | partial | `setCcTarget` exists; only restored from preset `midiMap` | no remap UI | — | MIDI Learn is separate |
| High velocity → pinch (user-mappable) | Auto detect | partial | velocity trigger exists for natural harmonic only, and is never enabled | no | — | |
| CC for palm mute, pick position, slide | Auto detect | yes | default ccMap 67 / 70 / 65 / 75 | n/a | — | |
| Signal chain order (cable → pre → amp → post → cab → room) | Effects | yes | LE process | n/a | — | Cable replaced by GuitarCircuit |
| Pre pedals: comp, wah, env, octaver, pitch, OD, dist, fuzz | Effects | yes | PedalsDrive (10 pre types) | Col2 Pedalboard; Easy compact rack | Effects.everyPedalTypeRunsCleanly | |
| Tuner-mute pedal | Effects | no | — | no | — | No tuner anywhere |
| 14 amp models incl. Bassman, Twin, Deluxe, Champ, Ampeg | Amp | yes | AmpModel (13 + Custom) | Col3 Amplifier; Easy amp card | Amp.gainProducesHarmonicDistortion | |
| Gain / master / bright / mid boost / presence / standby | Amp | yes | AmpFacePanel | Col3; Easy card | Amp.standbyIsSilentAndWarmsUp | |
| Cabinet sizes incl. open / closed back | Cab | yes | CabinetType (10) | Col3; Easy | — | |
| Speaker types; speaker age | Cab | yes | SpeakerType (8); `cab_speaker_age` | Col3 | — | |
| Mic types, position, distance, dual-mic blend | Mic | yes | MicType (7); `mic_*`; `dual_mic` | Col3; Easy mic1/mic2/blend | — | |
| Room size (7), material (4), mic-to-room blend | Room | yes | RoomEngine | Col3; Easy | Room.biggerRoomsRingLonger | |
| Stereo via dual cab / stereo fx / dual amp | Stereo | partial | `mic_width`; stereo pedals | Col3 | Engine.monoCompatibility | No dual-amp routing |
| MIDI capture 60 s; Save last take | Additional | yes | Support/MidiCapture (60 s) | File menu "Save last MIDI take" | MidiCapture.capturesAndWritesAFile | |
| Chord library, searchable | Additional | yes | ChordAndTabPanel | Chord overlay button | — | |
| Chord library: edit fingerings | Additional | no | — | no | — | |
| Scale / mode overlay on fretboard | Additional | partial | FretboardComponent scale overlay | Advanced fretboard strip, right-click only | — | Not in Easy |
| Real-time tab display, exportable | Additional | yes | ChordAndTabPanel; NotationPanel | Chord overlay; tab NOTATION | Notation.*; NotationTab.* | |
| Practice: metronome, progression looper, backing track | Additional | yes | Source/Practice/* | Practice drawer (D); tab PRACTICE | PracticeMetronome.*; PracticeLooper.* | |
| Freeze / E-Bow | Additional | yes | FreezeOverlay, EBowDriver | Col3 Sustain | Sustain.*; EBow.* | |
| Doubler | Additional | yes | Doubler post-rack pedal | Post rack | Doubler.* | Old `doubler_on` is legacy |
| Feedback simulation, threshold and speed | Additional | yes | FeedbackLoop (physical) | Col3 Sustain feedback row | Feedback.* | |
| Humanize: timing, velocity, detune, attack, noise probability | Humanize | yes | MidiInterpreter::Humanisation | Col3 Humanise | — | |
| Humanize: vibrato timing / depth variation | Humanize | no | — | no | — | |
| Rounded window with cutaway | GUI window | yes | PluginEditor paint (drawn cutaway) | n/a | Editor.itLaysOutAndPaints… | |
| 1200x720, resizable, aspect locked | GUI window | yes | PluginEditor constrainer | n/a | Editor.theProcessorHandsOverAnEditorAtItsDocumentedSize | |
| Header items 1-9 (logo…mode switch) | Header | yes | HeaderBar | header | Editor.* | Logo is text |
| Easy band 1: guitar image with pickup switch and knobs overlaid | Easy | yes | GuitarBodyComponent / GuitarRenderer | Easy | GuitarIllustration.* | Quality under rework (TODO G) |
| Easy band 1: 24-fret clickable fretboard, live notes | Easy | no | FretboardComponent is only in Advanced | no (Easy) | — | |
| Six LARGE macro knobs with dice and lock | Easy band 2 | partial | EP macros are `Size::Small`; dice and lock shown | Easy playing strip | EasyLayout.* | Size wrong |
| Style dropdown, mode selector, MIDI-in blink, Audition | Easy band 3 | yes | EP styleBox, playingModeSelector, auditionButton; HB midiDot | Easy | — | |
| Export / drag-out handle | Easy band 3 | partial | Export button only | Easy | — | No audio drag-out anywhere |
| Advanced col 1: per-string rows (tuning, material, gauge, age, tension, mute, edit) | Advanced | partial | StringRow shows note, tension, mute | Col1 Strings | — | Material/gauge/age global; no Edit; mute writes engine from UI thread and is not a param |
| Col 2: selected-string detail editing | Advanced | partial | `stringInfoLabel` read-only | Col1 Selected String | — | Nothing editable per string |
| Per-string fretless toggle | Advanced | no | global `fretless` | no | — | |
| Custom fret positions / scalloping per string | Advanced | no | — | no | — | |
| Per-fret tuning offset editor | Advanced | no | — | no | — | |
| Col 3 body/pickup/hand/noise sections | Advanced | yes | AP buildColumn1/2 | Col1/Col2 | — | Layout follows gui-integration, not spec.md |
| Col 4 amp/cab/mic/room/fx rack/humanise | Advanced | yes | AP buildColumn3 | Col3 | — | |
| Drag-and-drop pedal reorder, compact UI per pedal | Advanced | yes | PedalRack::reorder; PedalFace | Col2/Col3 racks | Effects.chainReordersWithoutGlitching | |
| Knob left-drag, double-click reset, context menu (value, reset, copy/paste, learn, macro, lock, randomise) | Interaction | yes | Widgets.cpp showParameterContextMenu | all LuthierKnobs | MidiLearn.*; Editor.rightClickOffersModulationAndBuildsTheRoute | "Assign to macro" is the Modulate submenu |
| Fretboard click plays; right-click mute / capo / mark | Interaction | partial | FretboardComponent::mouseDown | Advanced strip | — | No "mark" |
| MIDI Learn via right-click | Interaction | yes | Support/MidiLearn | all knobs; header Learn | MidiLearn.* | |
| Overlays: Escape, click-outside, close button, one at a time | Interaction | yes | OverlayPanel | overlays | Editor.everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt | Test covers Escape only |
| `.luthierpreset` JSON with all state, MIDI map, tags | Presets | yes | PresetManager | File menu; browser | Presets.stateRoundTripsExactly | |
| Factory presets by category in bundle | Presets | partial | FactoryPresets::writeAll (36: Electric/Acoustic/Classical/Bass/Utility) | Browser | Presets.everyFactoryPresetLoadsAndPlays | No Custom/User category; written at runtime |
| User folders Win / mac; extra folders via Preferences | Presets | yes | PresetManager::getUserPresetFolder; FileLocationsPage add | Options › File locations | — | No remove-folder button found |
| Quick export phrase to WAV | Audio export | yes | AudioExporter | Easy Export; File menu | — | |
| Export As: format, depth, rate, length, normalise, filename | Audio export | yes | Overlays ExportPanel | Export overlay | — | |
| MIDI capture export `.mid` | Audio export | yes | File menu item 7 | header | MidiCapture.* | |
| Identity 1: tension range, warn on impossible | Identity | partial | Validator::checkTension clamps | StringRow colours tension | Validator.* | No warning banner |
| Identity 2: fret range reject / transpose | Identity | yes | Validator::checkFretRange | n/a | Validator.correctsRatherThanCrashing | |
| Identity 3: body always, "no body" experimental | Identity | yes | `body_mode` "No Body (Experimental)" | Col1 | — | |
| Identity 4: all pickups off → silence and warning | Identity | partial | Validator::checkPickupOutput | no warning UI | Pickup.silenceWhenEverythingIsOff | |
| Identity 5: velocity changes brightness | Identity | yes | Excitation | n/a | StringEngine.harderPluckIsBrighter… | |
| Identity 6: higher notes decay faster | Identity | yes | StringEngine pitchScale | n/a | StringEngine.higherNotesDecayFaster | |
| Identity 7: coupling cannot be disabled | Identity | yes | coupling floor | Col1 | Coupling.* | |
| Identity 8: slide noise present by default | Identity | yes | `noise_slide` default > 0 | Col2 | — | |
| Identity 9: playable chord voicings | Identity | yes | ChordVoicer | n/a | ChordVoicer.impossibleChordDegradesGracefully | |
| Validator 5 stages, nudge or reject, log | Validator | partial | Validator.h | Debug panel | Validator.* (3) | Check 5 tests level only, not spectral shape |
| RT-safe audio, denormals off, NaN guards | Threading | partial | ScopedNoDenormals PP:945, LE:1510; sanitise() | n/a | Engine.fastSlidesProduceNoNansOrDenormals | UI thread calls engine directly (panic, StringRow mute, detune) |
| Partitioned FFT body convolution | Threading | yes | BodyEngine (partition 128) | n/a | Engine.latencyIsReportedAndPlausible | |
| Worker threads for IR / preset / coupling recalculation | Threading | partial | Cabinet/Body async IR with fallback | n/a | — | Coupling recalculation not off-thread |
| SR / block agnostic, prepare recomputes | Threading | yes | prepare() everywhere | n/a | Engine.sampleRateChangesAreSurvived; Engine.blockSizeChangesAreSurvived | |
| 20 ms parameter smoothing | Threading | yes | kParamSmoothSeconds = 0.020 | n/a | — | |
| CC for all continuous params | MIDI | yes | MidiLearn | right-click | MidiLearn.* | |
| MPE pitch bend, pressure, timbre | MIDI | yes | MidiInterpreter MPE | Col3 MPE toggle | Controllers.* | |
| Per-string bend range (controller mode) | MIDI | partial | setStringBendRange via ControllerProfile only | tab CONTROLLERS (profile) | Controllers.* | |
| Program change recalls presets | MIDI | yes | PP:1547 | n/a | — | |
| Prefs: default preset on load | Prefs | no | — | no | — | |
| Prefs: preset folders add / remove | Prefs | partial | FileLocationsPage add | Options | — | Remove not found |
| Prefs: MIDI mapping global defaults | Prefs | partial | MidiPage "clear all" only; MIDI-export defaults | Options › MIDI | — | |
| Prefs: export defaults | Prefs | partial | MIDI export defaults only | MIDI OUT tab | — | |
| Prefs: oversampling / FFT block / max polyphony | Prefs | partial | `oversampling` only | Options › Audio; Col3 Master | — | |
| Prefs: realism defaults | Prefs | no | — | no | — | |
| Prefs: appearance | Prefs | yes | AppearancePage | Options | Theme.* | |
| Windows VST3 + macOS VST3/AU | Deliverables | partial | CMake `FORMATS VST3 Standalone` | n/a | — | **No AU target**, although README lists `Luthier_AU` |
| Source layout DSP/Model/UI/Presets | Deliverables | partial | Source/* | n/a | — | UI/Easy, UI/Advanced, UI/Fretboard are flat files |
| 100+ body IRs, 50+ speaker IRs | Deliverables | yes | 216 body + 504 cab wav | n/a | — | Synthesised |
| Docs (GUITAR_PHYSICS … TROUBLESHOOTING) | Deliverables | yes | docs/*.md | n/a | — | TROUBLESHOOTING stale (TODO 13); no root README.md |
| Unit tests per DSP module | Tests | yes | Source/Tests | n/a | 743 tests | |
| String tension tests | Tests | yes | StringPhysics.* | n/a | 6 tests | |
| Chord voicing tests | Tests | yes | ChordVoicer.*, RubricVoicer.* | n/a | 21 tests | |
| Pluginval level 10 in CI | Tests | no | no CI config (.github absent) | n/a | — | README claims it passes; unverifiable |
| UI overlay dismissal tests | Tests | partial | EditorTests | n/a | Editor.everyOverlayShortcut… | Escape only |
| Fuzz 10,000 states | Tests | yes | — | n/a | Parameters.fuzzAcrossTenThousandStates | |
| Latency reporting test | Tests | yes | PP setLatencySamples | n/a | Engine.latencyIsReportedAndPlausible | |
| Signed and notarised installers | Deliverables | no | nothing in scripts/ | n/a | — | |
| CLI batch renderer | Deliverables | yes | Tools/RenderCli.cpp | n/a | — | |
| Ship gates 1-9 (stability, coupling, latency, IR clicks, chords, fretless, zipper, pluginval, overlay tests) | Ship | partial | tests above | n/a | Engine.*, Coupling.cannotRunAway | Pluginval and IR click tests missing |
| Ship gates 10-12 (7-host soak, GK/MPE hardware, blind A/B) | Ship | no | — | n/a | — | Manual; no record |

---

### engine.md

**Summary:** 99 requirements. **75 yes / 20 partial / 4 no.** The DSP ground rules and module structure are followed and well tested. Gaps: custom amp controls, per-string whammy, tuner/mute, the aftertouch-to-bend choice, and interpolation choice have no UI (README claims "selectable"). Standby warm-up is 8 s, not 30 s. The CPU test asserts only < 0.85x real time, and mono/round-trip checks do not cover every factory preset.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Double-precision DSP | 0.1 | yes | DSP/* (double state) | n/a | — | |
| No allocation or I/O in processBlock | 0.2 | partial | prepare-allocated buffers | n/a | — | Allocation counter compiled out (TODO 2k) |
| DC blocker + NaN guard on every recursive stage | 0.3 | partial | StringEngine dcBlocker + sanitise; others sanitise only | n/a | Common.dcBlockerRemovesOffset; Common.sanitiseClampsNonFinite | Body modal, coupling and FDN have NaN guards; per-stage DC blockers not evident |
| 20 ms linear smoothing, 30 ms filter exp, 5 ms discrete crossfade | 0.4 | yes | DspCommon kParamSmoothSeconds; kSwitchCrossfadeSeconds | n/a | — | |
| No hardcoded SR | 0.5 | partial | defaults `sr = 44100.0` in headers, overwritten in prepare | n/a | Engine.sampleRateChangesAreSurvived | Cosmetic |
| Times in s / Hz | 0.6 | yes | params in ms / s | n/a | — | |
| ScopedNoDenormals | 0.7 | yes | PP:945, LE:1510 | n/a | — | |
| reset() on every module; on transport start / preset / panic | 0.8 | yes | *.reset | n/a | Presets.audioIsIdenticalAfterARoundTrip | |
| Modules testable in isolation | 0.9 | yes | Tests per module | n/a | many | |
| Oversampling 4x default, 2x / 8x | 0.10 | yes | `oversampling` (1x/2x/4x/8x, default 4x) | Options › Audio; Col3 Master | Common.oversamplingSuppressesAliasing | |
| Pipeline order as diagram | 1 | yes | LE process | n/a | — | CableSim → GuitarCircuit |
| Only back-flow is coupling | 1 | partial | also FeedbackLoop (amp → strings) | n/a | Feedback.* | Deliberate (ambiguity-resolutions 1) |
| Typed events (NoteOn, Bend, Pressure, CC, Sustain, Whammy) | 2 | yes | PlayingEvents.h | n/a | — | |
| Mode A mono, B poly, C controller / MPE | 2 | yes | MidiInterpreter | EP Mode | Controllers.*, Technique.* | |
| String assignment algorithm | 2 | yes | ChordVoicer / RubricVoicer | n/a | ChordVoicer.singleNotesStayNearTheHand | |
| Default CC map (1, 2, 4, 11, 64, 65, 66, 67, 70-79) | 2 | yes | MidiInterpreter::resetCcMapToDefaults | n/a | — | |
| CC map user-remappable | 2 | partial | setCcTarget; preset `midiMap` | no editor | — | |
| Aftertouch → vibrato or bend (user choice) | 2 | partial | setAftertouchTarget | no | — | Never set from UI or params |
| TuningEngine f = f_open·2^(…) with detune and intonation | 3 | yes | TuningEngine | n/a | Tuning.* | |
| Temperaments incl. Werckmeister III, Kirnberger III, custom (12 doubles in preset) | 3 | partial | Temperament enum; `customTemperament` | Col1 (choice only) | Tuning.temperamentsDiffer… | No custom-ratio editor |
| Alternate tunings | 3 | yes | TuningPreset | header | Tuning.everyPresetProducesSaneFrequencies | |
| Custom tuning array in preset | 3 | partial | `openFrequencyHz` | no | — | |
| Manual detune ±100 | 3 | partial | detuneCents | headstock popover | Editor.theHeadstockPopover… | Not automatable |
| Realism detune ±20 persisted | 3 | yes | `realismDetuneCents` saved | Col1 | — | Fixed seed |
| Drift every 30 s up to ±5 c, off by default | 3 | yes | TuningEngine drift | Col1 Drift toggle | — | |
| Intonation slope 0.3 c/fret, per string | 3 | partial | setIntonationSlope from global param | Col1 | Tuning.intonationError… | Not per string |
| Technique set (pluck … strum) | 4 | yes | TechniqueEngine / Technique enum | n/a | Technique.* | |
| Detection logic order | 4 | yes | TechniqueEngine.cpp | n/a | Technique.controllersTakePriorityOverInference | |
| HammerOn / PullOff re-excite 10-30 % | 4 | yes | Excitation Kind | n/a | StringEngine.legatoDoesNotRetriggerTheAttack | |
| Slide ramp + noise | 4 | yes | Slide | n/a | Technique.slideDurationScalesWithDistance | |
| Palm mute 5 kHz → 800 Hz | 4 | yes | StringEngine Damping::PalmMute | CC67 | StringEngine.palmMute… | |
| Harmonics, pinch, tap mappings | 4 | yes | Excitation Harmonic / PinchHarmonic / Tap | CC | Technique.harmonicNodesAreDetected | |
| SlideGuitar softer attack + bottleneck vibrato | 4 | yes | SlideEngine | header Slide | Slide.* | |
| Strum offsets, up-strums lighter | 4 | yes | StrumGesture; `strum_up_velocity_ratio` | STRUM group | StrumDynamics.* | |
| EKS delay line + loop filter + loss + DC + NaN | 5.1 | yes | StringEngine | n/a | StringEngine.* | |
| Fractional delay: allpass recommended, 3 options | 5.2 | partial | FractionalDelayLine (Allpass1 / Lagrange3 / Lagrange5) | no | Engine tests (interp modes) | Lagrange5 default (documented); README says "selectable", but nothing selects it |
| Buffer size for 40 Hz at 96 kHz | 5.2 | yes | FractionalDelayLine | n/a | StringEngine.survivesExtremeParameters | |
| Delay length smoothed ~2 ms | 5.2 | yes | smoothedDelay | n/a | StringEngine.bendIsSmooth… | |
| Loop filter, damping by material | 5.3 | yes | loopFilter cutoff | n/a | — | Cutoff-based (documented deviation) |
| Palm 0.6 / muted pick 0.75 | 5.3 | yes | Damping modes | n/a | StringEngine.palmMute… | |
| Loss allpass inharmonicity, B per string | 5.4 | yes | dispersion stages | n/a | StringEngine.dispersionStretches… | |
| Triangle excitation, comb at 2x pluck, material filters | 5.5 | yes | Excitation | n/a | StringEngine.harderPluck… | |
| Random attack 0.5-2 ms | 5.5 | partial | `hum_attack` variation | Col3 Humanise | — | |
| Coupling with bandpass at receiver f0, symmetric, cap | 5.6 | yes | CouplingMatrix | Col1 | Coupling.* | |
| Muted string still receives coupling | 5.6 | yes | coupling independent of damping | n/a | — | |
| reset() clears delay / filters | 5.7 | yes | StringEngine::reset | n/a | — | |
| Voice steal 5 ms ramp | 5.8 | yes | stealTotal = 0.005·sr | n/a | — | |
| Body convolution, partition 128/256, latency reported | 6.1 | yes | BodyEngine kBodyPartitionSize = 128 | n/a | Engine.latencyIsReported… | |
| IR path BodyIRs/<type>/<size>_<wood>_<age>.wav, 100+ | 6.1 | yes | Resources/BodyIRs (216) | n/a | — | |
| Modal 30-50 modes scaled by dimensions, air mode | 6.2 | yes | BodyEngine (48) | Col1 | Body.* | |
| Modal params per body in JSON | 6.2 | partial | compiled tables + part JSON | n/a | PartAcoustics.* | |
| Pickup types and params | 7 | yes | PickupEngine / parts | Col2 / Workshop | Pickup.* | |
| Position comb | 7.1 | yes | per-string comb | Workshop | Pickup.positionComb… | Per string (deviation) |
| LCR tank resonance and Q | 7.2 | yes | GuitarCircuit (moved) | Col2 Circuit | Circuit.*, Pickup.resonant… | |
| Magnet EQ curves | 7.3 | yes | PickupEngine | Col2 | — | |
| Humbucker as 2 coils + comb; coil tap | 7.4 | yes | PickupEngine | Col2 Coil tap | — | |
| Piezo HP 40, LP 15k, 3 kHz peak | 7.5 | yes | PickupEngine piezo | Col2 | — | |
| Internal mic tilt | 7.6 | yes | PickupEngine | Col2 | — | |
| 3-way / 5-way / independent volumes; 5 ms crossfade | 7.7 | yes | `pickup_selector`, `pickupN_volume`, slotGain crossfade | Col2; illustration | — | |
| Whammy modes (vintage down-only option, Floyd, TransTrem, Bigsby) | 8 | yes | WhammyEngine; bridge_type (5) | Col1 Bridge | Whammy.* | |
| Floyd spring resonance burst | 8.2 | yes | `whammy_springs` | Col1 | — | |
| TransTrem ratio + transpose detents | 8.3 | yes | `transpose_lock` | Col1 | Whammy.transTrem… | |
| 5 ms whammy smoothing | 8 | yes | WhammyEngine | n/a | — | |
| Whammy on CC2 / MPE Y | 8 | yes | ccMap[2] | n/a | — | |
| Per-string whammy toggle | 8 | no | WhammyEngine::setPerStringEnabled has no caller | no | — | Glue |
| CableSim one-pole by length + 4 kHz bump | 9 | yes | GuitarCircuit cable model (`cable_length`, `cable_quality`) | Col2 Circuit | Circuit.* | Superseded by volume-knob-interaction |
| Pre-effects: compressor opt / FET | 10 | yes | PedalsDrive kCompCharacter | racks | Effects.* | |
| Wah, env filter, octaver, pitch shifter | 10 | yes | PedalsDrive | racks | Effects.everyPedalTypeRunsCleanly | Pitch shifter uses two crossfaded heads, not PSOLA |
| OD / Dist / Fuzz / Boost / Volume | 10 | yes | PedalsDrive | racks | — | |
| 8 slots, drag reorder, 10 ms bypass fade | 10 | yes | EffectsChain kNumSlots 8; Pedal bypassFade 10 ms | racks | Effects.bypassIsTransparent | |
| 4x oversampling on drive stages | 10 | yes | PedalsDrive Oversampler | n/a | Common.oversampling… | |
| Amp model list incl. Custom | 11.1 | partial | AmpModel | Col3 | — | Custom has no controls (setCustomStages/PowerTube/ToneStack never called) |
| Preamp stages, asymmetric waveshaper, coupling HP | 11.2 | yes | AmpEngine | n/a | Amp.gainProduces… | |
| Passive tone stack (Yeh) + presence in NFB | 11.3 | yes | ToneStack | amp face | — | |
| Phase inverter with asymmetry | 11.4 | yes | AmpEngine | n/a | — | |
| Push-pull power amp + sag | 11.5 | yes | AmpEngine sagAmount | n/a | — | |
| Output transformer | 11.6 | yes | transformerHf / Lf | n/a | — | |
| Standby with 30 s warm-up | 11.7 | partial | warmupGain 8 s | amp face | Amp.standbyIsSilentAndWarmsUp | 8 s, not 30 s |
| Post fx: chorus, phaser (stages), flanger, tremolo, rotary, delay (types / sync / ping-pong), reverbs, spring, EQ | 12 | yes | PedalsMod | post rack | Effects.* | |
| Cab IR 200+, partitioned | 13.1 | yes | CabinetEngine; 504 IRs | Col3 | Cabinet.procedualFallback… | |
| Mic selector swaps IR | 13.2 | yes | CabinetEngine | Col3 | — | |
| Dual mic blend + opposite pan | 13.3 | yes | `mic_blend`, `mic_width` | Col3 | Engine.monoCompatibility | |
| Async IR load with fallback | 13.4 | yes | CabinetEngine fallback | n/a | Editor.aFailedPresetLoadAndAMissingIr… | |
| Room ER taps + FDN, size / damping / mix | 14 | yes | RoomEngine kFdnSize 8 | Col3 | Room.* | |
| Master gain -60…+12, limiter -0.3 dBFS, DC 5 Hz | 15 | yes | MasterBus | Col3 Master; Easy output | Master.limiterHoldsTheCeiling | |
| Metering peak / RMS / LUFS | 15 | yes | MasterBus | LevelMeter (Easy) | Master.meteringTracksTheSignal | LUFS not displayed |
| Per-preset state incl. whammy, chains, MIDI maps | 16 | yes | PresetManager | n/a | Presets.* | |
| Host state incl. UI state (mode, view, A/B, history) | 16 | yes | PP uiState | n/a | StateModel.* | |
| Threading: serial strings, workers for IR / preset | 17 | yes | — | n/a | — | |
| Communication only via FIFO / atomics | 17 | partial | — | n/a | — | UI calls engine setters directly (panic, mute, detune) |
| Latency = body + cab + oversampling + lookahead | 18 | yes | PP:922 | n/a | Engine.latencyIsReportedAndPlausible | |
| Unit tests: String, Tuning (0.1 c), Technique, Pickup, Whammy, Body IR + modal, Amp per stage, Coupling | 19 | partial | Tests | n/a | see Tests | Tuning "sane" not 0.1 c for all tunings; no explicit body IR-load test; amp tested at one stage |
| Integration: chromatic, bend, fast slide | 19 | yes | IntegrationTests | n/a | Engine.chromaticScale…, Engine.fastSlides… | |
| 30 s at 96 kHz all fx < 15 % CPU | 19 | partial | Engine.cpuStaysWithinBudget | n/a | asserts < 0.85x real time, 3 s | Much weaker than the target |
| Round-trip every factory preset within 0.01 dB | 19 | partial | Presets.audioIsIdenticalAfterARoundTrip | n/a | yes | Scope of presets not verified as "every" |
| Pluginval L10 CI | 19 | no | no CI | n/a | — | |
| Host matrix / controller testing | 19 | no | — | n/a | — | Manual, not recorded |
| Mono-compatibility of every factory preset | 20.19 | partial | Engine.monoCompatibility (one synthetic rig) | n/a | yes | Not per preset |
| Perf: < 8 % CPU at 96k/128; 16 instances; preset load < 500 ms; MIDI < 2 ms | 22 | no | not measured | n/a | — | performance-budget pass not done (TODO 15) |

---

### include.md

**Summary:** 26 requirements. **20 yes / 5 partial / 1 no.** Help, debug, crash log, troubleshooting file, hard reset, randomise, tooltips toggle, WAV export dialog and easter egg exist. Gaps: support links are placeholders, `THIRD_PARTY_LICENCES.txt` is missing, there is no MIDI import to play or sequence, the standalone audio/MIDI device options are only a note, and there is no CLAP or Linux-installer path.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| In-plugin help: every feature, workflow, GUI description | Help | yes | UI/HelpContent, HelpTab | tab HELP; F1; header ? | HelpTab.theContentCoversWhatIncludeMdAsksFor | |
| Version, licence | Help | yes | HelpContent "about" | HELP | — | |
| GitHub / homepage / support email links | Help | partial | HelpContent.h:92-94 `luthieraudio.example` | HELP buttons | — | Placeholders; release blocker (TODO 13) |
| Install / uninstall troubleshooting; where presets go | Help | yes | HelpContent | HELP | — | |
| Debug button in help | Help | yes | help.onOpenDebug | HELP → Debug overlay | — | |
| Unique app icon | Icon | yes | Resources/icon.png, CMake ICON_BIG | n/a | — | |
| Preset bank with descriptive names | Presets | yes | FactoryPresets (36) | browser | Presets.everyFactoryPresetLoadsAndPlays | |
| Reset button to defaults | Reset | partial | EP resetButton; File › Reset all; Ctrl+Shift+R | Easy; header | Presets.resetRestoresDefaults | Does not stop looper, backing track, tune, rhythm (issues.md) |
| File dropdown: save / save as / open / options | Menus | yes | HB showFileMenu | header File | — | |
| Options: tooltips on/off | Menus | yes | AppearancePage tooltipsToggle | Options | — | |
| Options: MIDI and audio card settings | Menus | partial | AudioPage note "Where are the device settings?" | Options | — | Standalone device selector not embedded |
| "Open location in explorer" | Menus | yes | File › Open user preset folder | header | — | |
| Export audio to WAV with quality options | Export | yes | ExportPanel | Easy / File | — | |
| Success popup with location, length, name, quality | Export | yes | Overlays.cpp:926 | — | — | |
| Import / export MIDI | MIDI | partial | export: capture, MIDI OUT, notation; import: Export › "MIDI file…" (render only), TUNE record | several | MidiExport.*, MidiOutPanel.* | No import to play or edit (issues.md; TODO 10 "import UI") |
| Right-click: MIDI map, reset, set value | Right click | yes | Widgets showParameterContextMenu | all knobs | MidiLearn.* | |
| Drag-and-drop when importing samples | DnD | yes | IrSlotEditor filesDropped | tab TONE MATCH | — | |
| Hover tooltips | Tooltips | yes | TooltipWindow 400 ms | everywhere | — | Not in the locale catalog |
| Randomise: new each press, resets first | Randomise | yes | PresetManager::randomise | File menu; Easy; Ctrl+R | Presets.randomiseRespectsLocks | |
| Tripple-test / logic-error pass / bug bash | Build notes | no | — | n/a | — | qa-polish bug bash not done (TODO 17) |
| VST3 + standalone; future CLAP / Linux | Build notes | partial | CMake VST3 Standalone | n/a | — | Linux build script exists; no CLAP |
| Debug window: raw data live | Extra | yes | DebugPanel stateView / streamView | Debug overlay (Ctrl+D) | — | |
| "Create log on crash" off at every load | Extra | yes | Diagnostics crashLogEnabled false | Debug | Telemetry.* | |
| Hard reset + clear caches | Extra | yes | PP::hardResetAndClearCaches | Debug | — | |
| Export troubleshooting file (settings, audio / MIDI, licence, version, DAW) | Extra | yes | Diagnostics | Debug | — | |
| Easter egg pixel → hidden tab with close + secret tooltips | Extra | yes | SecretPanel; PluginEditor notch tip | notch pixel | Effects.secretEffectIsStable… | |

---

### theme.md

**Summary:** 34 requirements. **16 yes / 10 partial / 8 no.** Metrics (8 px grid, radii, knob sizes, arcs, 28 px buttons, 400 ms tooltips, meter peak hold, notch, version footer, output LED) match. The shipped "guitar-shop" re-skin (walnut, brass, Tolex, Lato and Bebas) breaks the theme's "neutrals identical", "no faux wood/metal" and Inter/JetBrains font rules. It came from `spec/proposals/visual-polish.md` and a user request (TODO V). The 80 ms value animation, vertical-resize cursor and scrolling data stream are not wired (`DataStreamDisplay` is never constructed).

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Core palette neutrals (#0E1116 etc.) | Colour | no | Theme.h Palette (walnut / ivory / brass) | — | Theme.theDefaultIsTheGuitarShop | Only one accent may be re-tinted; neutrals changed too |
| One accent re-tint allowed | Colour | yes | accent 0xffd4a24c | — | — | |
| Shadow 0.55, 8 px, y+2 | Colour | partial | — | — | — | Not verified |
| Inter / JetBrains Mono fonts | Type | no | Fonts::ui = Lato, display = Bebas | — | Theme.theBundledFontsLoad | |
| Labels 11 px uppercase +0.08 em; values 13-14 px | Type | partial | Fonts::drawTrackedText | — | — | |
| Knob 48 / 36 / 64 px, flat radial gradient | Knobs | partial | Metrics knob sizes | — | Theme.controlsRender… | "Black bell knobs", skeuomorphic |
| 2 px indicator, 270° external 3 px arc, 4 px gap | Knobs | yes | Metrics arc*, LookAndFeel | — | — | |
| Centre dot active / default | Knobs | partial | — | — | — | Not verified |
| Double-click reset; right-click value entry | Knobs | yes | Widgets | — | — | |
| Value above on hover / drag, else label | Knobs | partial | LuthierKnob | — | — | |
| Slider track 4 px, 16x24 thumb, ticks | Sliders | partial | LookAndFeel | — | — | "Brass fader caps" |
| Buttons 4 px radius, 28 px, on / off states, 100 ms flash | Buttons | partial | Metrics::buttonHeight 28 | — | — | Flash not found |
| Meter gradient, 1.5 s peak hold falling at 20 dB/s, numeric readout | Meters | yes | Widgets LevelMeter | Easy | — | |
| 8 px grid | Layout | yes | Metrics::grid | — | EasyLayout.* | |
| 1 px separators, no boxes-in-boxes | Layout | partial | Column section rules | — | — | Walnut panels with screws |
| Section headers with 2x12 accent bar | Layout | yes | drawSectionHeader | — | — | |
| Corner radii 6 / 4 / 2 | Layout | yes | Metrics | — | — | |
| 16 px min edge padding | Layout | yes | windowPadding | — | — | |
| No skeuomorphism / faux wood / metal | Layout | no | guitar-shop theme | — | — | Direct violation (proposal-driven) |
| 80 ms ease-out value animation | Feel | no | Metrics::animationMs defined, unused | — | — | |
| Hover brighten 8 %; vertical-resize cursor | Feel | no | no setMouseCursor in Widgets | — | — | |
| Shift coarse / Ctrl ultra-fine drag | Feel | yes | KnobSlider::mouseDrag | — | — | Shift is coarse, Ctrl fine |
| Tooltip pill after 400 ms | Feel | yes | tooltipDelayMs 400 | — | — | |
| Header strip 32 px (spec.md says 48) | Header | yes | headerHeight 48 (spec.md wins) | — | — | |
| Header: name left, gear, preset selector, A/B right | Header | partial | HB (File menu instead of gear) | header | — | |
| Signature diagonal notch top-left | Signature | yes | drawSignatureNotch | — | — | |
| Version in 9 px mono footer | Signature | yes | PluginEditor:359 | — | — | |
| Output LED grey → white, red > 0 dB | Animation | yes | OutputLed in HeaderBar | header | — | |
| Scrolling matrix data stream in empty space | Animation | no | DataStreamDisplay defined, never instantiated | no | — | README claims it ships |
| Stream stops when idle, faded edges, green | Animation | no | (dead code) | — | — | |
| Colourful, obvious scrollbars (issues.md) | — | no | drawScrollbar edgeBright / panelSunken | — | — | Still low contrast |
| Palette actually applied (was not) | — | yes | Palette::remap | Options › Appearance | Theme.aPaletteChangeReachesBuiltComponents | |
| Contrast AA on palettes | — | yes | — | — | Accessibility.palettesMeetContrast…; Theme.everyTextPairMeetsContrast… | |
| Tab strip readable at 1280 | — | partial | AP resized | tab strip | — | Wrapping fix in progress on `visual` |

---

### issues.md (user bug list)

**Summary:** 12 issues. **3 fixed / 6 partial / 3 open.** Pedal settings and fingers are fixed in code. The pedal fix is 1c331d5 plus the PresetPedals tests, and the fingers fix is `pick-noise.md 2` wiring in PB. Chords are fixed via a Poly default and the chord-timing fix. The squished MOD/RHYTHM/LIVE panels (80-pt sliver) are fixed only on `visual`. Still open: scrollbars and wheel conflict, a real "stop everything", and a piano roll that mirrors the strings.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Effects must audibly work | 1 | partial | EffectsChain; adoptPedalTypesFromParameters (1c331d5) | racks | PresetPedals.*, Effects.* | Fixed preset path; no listening check recorded |
| Pickup changes should be audible | 2 | partial | PickupEngine / GuitarCircuit | Col2 | Pickup.*, Circuit.* | `pickup_blend` dead; no A/B audibility test |
| Pre-amp pedals / pedalboard functional | 3 | partial | as 1 | Col2 rack; Easy compact rack | PresetPedals.* | |
| Fingers vs pick makes a difference | 4 | yes | PB setPickMaterialAndFingers | Col2 | — | Previously never read |
| Chords (two notes at once) | 5 | yes | Poly default; a357142 chord timing | EP Mode | Engine.aChordVoicesAcrossStrings | |
| Guitar picture ugly | 6 | partial | GuitarRenderer (2845 lines) | Easy / Advanced | GuitarIllustration.* | TODO G still in progress |
| Scrollbars colourful + arrows + "scroll" tooltip; wheel scrolls not knob | 7 | no | Theme drawScrollbar muted; no wheel guard in Widgets | — | — | Open |
| MOD / RHYTHM dropdowns squished; LIVE tiny boxes; more tooltips | 8 | partial | AP resized sets 80 px height | tabs | — | Fixed on `visual` (panels fill viewport; tab wrap) |
| "Reset and stop" that stops everything | 9 | no | PP::panic stops audition + engine; resetEverything doesn't stop looper / backing / progression / tune / rhythm / metronome | header Panic | Engine.panicSilencesEverything (engine only) | Loops keep running |
| MIDI import working | 10 | partial | Export overlay "MIDI file…" (render); TUNE Record from MIDI in | Export overlay; TUNE | — | No File › Import MIDI (TODO 10) |
| Basic internal sequencer | 10 | yes | Tune Builder (TUNE tab) | tab TUNE | TuneBuilder.*, TunePlayer.* | |
| Small piano roll mirroring plucked strings and vice versa | 11 | no | TunePianoRoll is a melody editor only | TUNE | — | No live mirror / click-to-play |

---

### README.md (spec/README)

**Summary:** 22 claims. **15 true / 4 partial / 3 false.** Counts are accurate: 25 instruments, 17 tunings, 7 temperaments, 12 materials, 11 gauges, 21 pedals, 13 amps, 10/8/7 cab/speaker/mic, 720 IRs, 36 presets. False: `Source/DSP/Cable/`, `THIRD_PARTY_LICENCES.txt`, and "the scrolling data stream is unchanged". The `Luthier_AU` target is marked partial because the other targets exist, but there is no AU.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Targets VST3, Standalone, **AU**, Tests, Render | Build | partial | CMakeLists FORMATS VST3 Standalone | n/a | — | **AU missing** |
| Install copies Resources beside artefacts | Build | yes | CMake | n/a | — | |
| 25 instruments, 17 tunings, 7 temperaments | Box | yes | enums | header | — | |
| 12 materials, 11 gauges | Box | yes | StringMaterials | Col1 | — | |
| 21 pedals, two 8-slot racks | Box | yes | PedalType | racks | — | |
| 13 amps, 10 cabs, 8 speakers, 7 mics | Box | yes | enums | Col3 | — | |
| 720 IRs (216 + 504) | Box | yes | Resources | n/a | — | |
| 36 factory presets | Box | yes | FactoryPresets | browser | — | |
| Architecture diagram modules | Arch | yes | Source/* | n/a | — | |
| Source layout incl. `DSP/Cable/` | Arch | no | Cable removed (GuitarCircuit) | n/a | — | Stale doc |
| DSP rules 1-10 | Rules | partial | see engine.md | n/a | — | Rule 2 unverified (allocation counter off) |
| Render CLI flags | CLI | yes | Tools/RenderCli.cpp | n/a | — | |
| Tests: fuzz, preset audio round trip, mono check | Tests | yes | — | n/a | Parameters.fuzz…; Presets.audioIsIdentical…; Engine.monoCompatibility | |
| Pluginval 1.0.3 L10 passes | Tests | partial | no CI or log in repo | n/a | — | Unverifiable |
| IR scripts deterministic | IRs | yes | scripts/make_irs.py | n/a | — | |
| Runtime folders + Options buttons | Runtime | yes | FileLocationsPage | Options | — | |
| docs/* list | Docs | yes | docs/ | n/a | — | TROUBLESHOOTING / USER_MANUAL stale (TODO 13) |
| Lagrange default, "all three selectable" | Deviations | partial | FractionalDelayLine | no | — | Not user-selectable |
| Cutaway drawn, not clipped | Deviations | yes | PluginEditor | — | — | |
| Theme structure unchanged incl. "scrolling data stream" | Deviations | no | DataStreamDisplay unused | no | — | False claim |
| `THIRD_PARTY_LICENCES.txt` | Licence | no | file absent | — | — | Help also refers to it |
| Help › About licence | Licence | yes | HelpContent | HELP | — | |

---

### TODO.md

**Summary:** 30 rows. **2 yes / 11 partial / 17 no.** The Done section's items (1-6, 2b, 2g) all hold up in code, so they have no rows here; the rows are the open items. 2 are stale or contradictory. "13c BLOCKED: phase 2b specs not on disk" contradicts "13c UNBLOCKED"; the nine files do exist in spec/. "C. docs/spec-coverage.md IN PROGRESS" has actually landed (3088 lines).

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Scrape / strum / slap integration green | In progress [x] | yes | ScrapeEngine, SlapEngine in PB | **no UI** | Scrape.*, Slap.*, SlapWiring.* | Params unattached |
| Remaining WIP: Muting + MuteGroup | In progress | no | Source/WIP/Rhythm/Muting, WIP/UI/MuteGroup (not built) | no | WIP MutingTests (not built) | partial (WIP, not built) |
| WIP: FirstRun + FirstEncounterHint | In progress | partial | WIP → built on `tune-help` | Options › Diagnostics (branch) | FirstRunTests (branch) | In progress on tune-help |
| 13c phase-2b realism | In progress | partial | — | — | — | NoiseFloor / sustain / stability params in progress on realism-c; no UI there either |
| G realistic illustration: headstocks, family switch UI, zoom/pan, thumbnails, per-string material, capo drawing, reduced motion, 60 ms test | In progress | partial | GuitarRenderer | Easy / Advanced | GuitarIllustration.* | Listed remainders open |
| C spec-coverage.md | In progress | yes | docs/spec-coverage.md | — | — | Stale "in progress" |
| 6e off-thread part swap | In progress | no | — | — | WorkshopSwap.* (queueing only) | |
| 5b slide-technique-controls (sources, Techniques › Slide sub-tab) | In progress | no | — | no TECHNIQUES tab | — | |
| V visual appeal: header face, accent choices, VU / room light, screenshots | Remaining | partial | Theme | — | FacesIntegration.* | Screenshot harness on `visual` |
| 2k chambering feedback term | Remaining | no | FeedbackLoop k_couple | — | — | |
| 2k capture never gets chord / technique → NOTATION history empty | Remaining | no | PerformanceCapture hooks unused | NOTATION | — | User-visible |
| 2k allocation counter compiled out | Remaining | no | LUTHIER_ALLOCATION_COUNTER undefined | — | — | Rule 0.2 unverified |
| 2k FeedbackLed 20 Hz vs 30 | Remaining | no | — | — | — | |
| 2k migrate-on-load backup | Remaining | no | PresetManager (save-only backup) | — | Presets.savingBacksUp… | |
| 2k notation export on worker | Remaining | no | — | — | — | |
| 2h Easy amp card cramped | Remaining | no | EP rig strip | Easy | — | |
| 2d rubric bass pattern; crossing velocity; fb / freeze / E-Bow as mod dest; Aux 1 toggle | Remaining | partial | — | — | — | |
| 7 Workshop remaining (per-string strings, nut drag, pick / slide / capo overlays, padlock, Easy wrench test, SR announce) | Remaining | partial | WorkshopPanel | WORKSHOP | WorkshopPanel.* | Known IR-onset issue |
| 8 StrumGesture then BassTechniques SLAP group + bass step grid | Remaining | partial | StrumGesture built; SLAP UI absent | STRUM yes, SLAP no | StrumGesture.* | |
| 9 capture techniques, marked region, fretboard tab dots, Mono chord extraction | Remaining | no | — | — | — | |
| 10 MIDI import UI, marked ranges, CHARACTER / env events, practice drag | Remaining | no | — | — | — | |
| 11 SessionRecorder honours audio / MIDI / autosave; looper / trainer hooks | Remaining | no | PracticeRoutineSetup TODO(lead hook) | — | — | |
| 12 Tune: popovers, drag, section drag / Vary, note editing, bass / layer editors, export dialog, hum, Ctrl+T, 15-07…15-10 | Remaining | partial | TunePanel | TUNE | TunePanel.* | Many items open |
| 13 support links placeholders; per-panel ? icons; stale docs | Remaining | no | HelpContent.h:92 | — | — | **Release blocker** |
| 13b phase 5b technique specs (scrape … engine-technique-layer, TECHNIQUES tab) | Remaining | partial | engines exist | no TECHNIQUES tab | Scrape.*, Slap.* | |
| 14 audit ui-wiring / onboarding / perf / qa / installer | Remaining | no | — | — | — | |
| 14b action-and-undo audit | Remaining | no | — | — | — | |
| 14c Restore first-run also clears ranges flag | Remaining | partial | restoreFirstRun on `tune-help` calls RangesUi resync | Options (branch) | — | In progress on tune-help |
| 15-17 polish / perf / onboarding / installer / bug bash | Remaining | no | — | — | — | |
| 18 plugin targets build clean | Remaining | partial | VST3 / Standalone only | — | — | No AU |

---

### CLAUDE_CODE_BRIEF.md

**Summary:** 22 rules and steps. **4 yes / 10 partial / 8 no.** The column-4 tab order is exact. The brief's "done" definition is not met: no READY TO SHIP marker, qa-polish gates not run, no installer or performance pass. Code violates four hard rules:

- Every parameter must be in the APVTS: per-string detune, fine tune, custom tuning, custom gauge, custom temperament and string mute are engine state.
- Threading contract: the UI thread calls `engine.panic()`, `getString().setDamping()` (StringRow mute) and `TuningEngine::setDetuneCents`.
- Every user-visible string must be in the locale catalog: AdvancedPanel, EasyPanel, HeaderBar, Overlays and TunePanel contain zero `tr(` calls and hundreds of literals.
- No features outside the spec: the guitar-shop theme was built from a proposal file.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Surface every engine feature in the GUI | Problem 1 | partial | — | — | — | Scrape, slap, custom amp, macros 7/8, pickup blend, CC map unreachable |
| Realism specs landed | Problem 2 | partial | — | — | — | Phase 2b on realism-c; 5b partial |
| Step 2: audit against gui-integration 19 | Order | partial | docs/spec-coverage.md | — | — | |
| Step 3: panels via ui-wiring attachment, theme, a11y, tooltip, docs, PhysicalRange | Order | partial | LuthierKnob::attachTo; RangesUi | — | Ranges.*, RangesUi.* | Headstock detune bypasses attachments |
| Step 4: ambiguity-resolutions applied | Order | partial | freeze / E-Bow, feedback, doubler, morph | — | Sustain.*, Doubler.*, PresetMorph.* | 2d items open |
| Step 5: realism modules wired | Order | yes | NoiseEngine, GuitarCircuit, SlideEngine, Workshop swap | — | many | |
| Step 6: Tune Builder MIDI-only; MIDI export round-trip ≤ -60 dBFS | Order | yes | TunePlayer → rhythm / notes; MidiExport | TUNE; MIDI OUT | MidiExport.* | |
| Step 7: polish pass (qa-polish 4, 5) | Order | no | — | — | — | |
| Step 8: performance pass | Order | no | — | — | — | |
| Step 9: onboarding pass / tour | Order | partial | FirstRun on `tune-help` | — | FirstRunTests (branch) | |
| Step 10: installer pass | Order | no | — | — | — | No installer scripts |
| Step 11-12: bug bash, human check | Order | no | — | — | — | |
| Do not skip or drop spec features | Rules | partial | — | — | — | See Top gaps |
| No features outside spec (proposal first) | Rules | partial | visual-polish.md in spec/proposals implemented | — | — | User request per TODO V |
| Column-4 tab order WORKSHOP … HELP | Rules | yes | AP buildWorkspace | tab strip | Editor.everyWorkspaceTabSelectsAndPaints | TECHNIQUES tab (5b) not added |
| Threading contract | Rules | no | PP::panic, StringRow::mouseDown, TuningPopover | — | — | Violations |
| Every parameter in APVTS | Rules | no | TuningEngine per-string state | — | — | Violation (documented in GAPS) |
| Structural state via command queue | Rules | partial | Workshop swap, IR installer | — | WorkshopSwap.* | |
| Every string in locale catalog | Rules | no | Localisation exists; panels hard-code | — | Localisation.* | Violation |
| Every feature has tests from its spec's Tests section | Rules | partial | 743 tests | — | — | Many spec tests absent (e.g. tune 15-07…15-10) |
| PROGRESS.md updated per milestone | Rules | yes | spec/PROGRESS.md | — | — | |
| Done: READY TO SHIP marker and sign-off | Done | no | none | — | — | |

---

### Top gaps (group A)

Ranked by what a user would hit first.

1. **Nothing stops everything** (issues.md 9). Panic and Reset leave the looper, backing track, progression looper, tune player, rhythm engine and metronome running; `PP::panic` covers only the audition and the engine. A user can get stuck with a loop that won't stop.
2. **Scroll UX is broken** (issues.md 7). Scrollbars are low-contrast and have no arrows or hints, and the mouse wheel over a knob changes the knob instead of scrolling the column.
3. **Workspace tabs render as an 80-px sliver** (issues.md 8). MOD, RHYTHM, LIVE, ROUTING and TONE MATCH are affected on the integration branch; the fix exists only on `visual`.
4. **Techniques with no controls:** 14 `scrape_*` and 25 slap/pop/ghost params are wired to the engine but have no control, and scrape/slap default to off. There is no TECHNIQUES tab.
5. **No per-string editing** in Advanced columns 1-2 (material, gauge, age, sustain, fretless, detune as params, custom tuning notes, custom frets, per-fret offsets). This is spec.md's core Advanced promise.
6. **Easy mode has no playable fretboard.** Band 1's clickable 24-fret board exists only in Advanced, and the macros are small knobs, not large.
7. **No AU target**, no CI or pluginval, no signed installers. README claims AU and a clean pluginval run.
8. **Support links are placeholders** (`luthieraudio.example`) and `THIRD_PARTY_LICENCES.txt` is missing. These are release and legal blockers.
9. **MIDI import to play or edit is missing** (File › Import MIDI, drop a .mid). There is also no live piano roll mirroring the strings.
10. **`pickup_blend` is dead** (PickupEngine never reads `blendAmount`). Together with issues 1-3, "pickup changes don't do much" is only partly fixed.
11. **Custom amp model has no controls:** `setCustomStages`, `setCustomPowerTube` and `setCustomToneStackStyle` have no callers.
12. **Threading violations:** UI-thread calls into the engine (panic, StringRow mute, headstock detune). These are a risk of races and crashes in hosts.
13. **Locale-catalog rule broken** across all major panels.
14. **Theme deviations:** the guitar-shop skin contradicts theme.md (neutrals, fonts, no skeuomorphism). The data stream and 80 ms animations are missing, and README claims otherwise.
15. **Performance and quality gates not run:** the CPU test asserts only < 0.85x real time, mono and round-trip checks don't cover every factory preset, the host matrix is untested, and the allocation counter is compiled out.
16. **The NOTATION tab's chord and technique history is empty in use** (the capture's chordSymbol and technique hooks are never called).
17. **MIDI remapping missing:** no UI for the CC target map, aftertouch target, harmonic-velocity trigger or per-string bend range outside controller profiles.
18. **Realism-detune "re-roll" is a fixed seed**, so every guitar gets the same "random" detune pattern, and there is no refresh button.
19. **Missing vibrato variants and humanise:** classical and blues vibrato shapes are absent, and so is humanise vibrato variation.
20. **Amp standby warm-up is 8 s** (spec says 30 s), and there is no tuner or tuner-mute.

### Unspecified gaps noticed

- **No built-in tuner display** (only a tuner-mute is specified, and even that is missing). Every $200 guitar plugin ships a chromatic tuner.
- **No CPU / voice meter or overload indicator** in the UI.
- **Plugin is `NEEDS_MIDI_OUTPUT FALSE`,** while the MIDI OUT tab offers live MIDI out. In a plugin host the live out has nowhere to go, since VST3 MIDI out is not declared.
- **No per-preset "favourite" or star in the browser**, and there are no preset-browser thumbnails yet (TODO G 15).
- **No undo history list** (only Undo and Redo buttons).
- **No DI / re-amp input path** for processing a real guitar through the rig (effect-version use case). include.md hints at an effects version.
- **No resizable UI scale presets in the header** (only Options › Appearance scale).
- **No online manual or quick-start card on first open** until FirstRun (on `tune-help`) lands.

### Small glue candidates

Each item below has an engine feature, API or param that exists and lacks only a UI attachment, preset field or one call.

| Item | Exact IDs / API | Where it should go | Effort |
|---|---|---|---|
| User macros 7/8 knobs | `macro_assign_a`, `macro_assign_b` (already mod sources) | tab MOD header row, and Easy playing strip beside Character | 2 LuthierKnob::attachTo |
| Pickup blend | `pickup_blend`; also read `blendAmount` in PickupEngine::process (currently unused) | Col2 PICKUPS, after Selector; illustration blend knob | 1 knob + ~5 lines DSP |
| Scrape group | `scrape_armed`, `scrape_trigger`, `scrape_direction`, `scrape_tool`, `scrape_pressure`, `scrape_duration`, `scrape_start_mm`, `scrape_end_mm`, `scrape_angle`, `scrape_string_mask` (StringMaskSelector exists), `scrape_trigger_cc`, `scrape_sweep_source`, `scrape_sweep_cc`, `scrape_retrigger` | TECHNIQUES tab (gui-techniques-updates); interim: CHARACTER tab new SCRAPE group, or Col2 Playing Hand | attachments only |
| Slap group | `slap_armed`, `slap_type`, `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`, `slap_ghost_cc`, `slap_strength`, `slap_position_mm`, `slap_thumb_hardness`, `slap_fret_contact`, `pop_strength`, `pop_position_mm`, `double_thump_enabled`, `double_thump_up_ratio`, `ghost_level`, `ghost_damping`, `ghost_auto`, `ghost_velocity_threshold`, `slap_force`, `slap_palm_position_mm`, `slap_string_mask`, `slap_ghost_mode`, `slap_rebound_gap`, `slap_snap_back`, `slap_body_part` | TECHNIQUES › SLAP (or RHYTHM tab SLAP group per bass-techniques) | attachments only |
| Stop-everything panic | `Looper`, `BackingTrackPlayer`, `ProgressionLooper`, `TunePlayer`, `Metronome`, rhythm engine, `freeze_enable`, effect tails `EffectsChain::reset` | `LuthierAudioProcessor::panic()`, posted to the audio thread; header Panic + Easy Reset | ~15 lines |
| Wheel scroll vs knob | `juce::Slider::setScrollWheelEnabled(false)` on knobs inside Advanced column viewports; accent thumb in `LuthierLookAndFeel::drawScrollbar` | Widgets.cpp LuthierKnob; Theme.cpp | small |
| Easy fretboard | existing `FretboardComponent` (Advanced strip) | Easy band 1 under the illustration (EP resized) | layout only |
| Per-string whammy | `WhammyEngine::setPerStringEnabled` | new bool param `whammy_per_string` in Col1 Bridge | 1 param + toggle |
| Custom amp controls | `AmpEngine::setCustomStages/setCustomPowerTube/setCustomToneStackStyle` | 3 new params (`amp_custom_stages`, `amp_custom_power_tube`, `amp_custom_tone_stack`) shown when `amp_model` = Custom, Col3 Amplifier | 3 params + choices |
| CC map / aftertouch target editor | `MidiInterpreter::setCcTarget`, `setAftertouchTarget` (preset field `midiMap` already saved) | Options › MIDI page table | UI only |
| Harmonic / pinch velocity trigger | `TechniqueEngine::setHarmonicVelocityThreshold`, `setHarmonicVelocityTriggerEnabled`; `setLegatoVelocityThreshold`, `setHammerOnEnabled`, `setSlideEnabled` | new params in Col3 Performance | params + knobs |
| Realism-detune re-roll | `TuningEngine::randomiseRealismDetune(max, seed)` with a fresh seed; `realismDetuneCents` preset field exists | button beside `realism_detune` in Col1 Tuning Realism | one button |
| Interpolation choice | `FractionalDelayLine::setInterpolation` | Options › Audio choice (README says selectable) | 1 param |
| Custom tuning / gauge / temperament editors | preset fields `openFrequencyHz` + `useCustomTuning`, `customGaugeInches`, `customTemperament`; `TuningEngine::setOpenFrequency`, `setCustomTemperament` | headstock TuningPopover (note picker per string); Col1 String Set when gauge = Custom; Temperament = Custom editor | UI only (engine-state path) |
| Data stream | `DataStreamDisplay` (Widgets.cpp:1237), never constructed | Easy rhythm strip spare area or Advanced column-4 empty state | construct + setSource |
| Aux/legacy params | `doubler_on`, `doubler_amount`, `feedback_on`, `feedback_threshold`, `feedback_speed`, `strum_speed`, `fret_action` | hide from host automation lists or mark "(legacy)" in the name | naming |
| realism-c params (on branch) | `noise_*` (11), `sustain_*` (9), `stability_*` (8) have no UI on `realism-c` either | CHARACTER tab NOISE FLOOR / SUSTAIN / STABILITY groups | attachments once merged |

---

## Group B



Audited at `f63a7f7` (integration branch `claude/luthier-cloud-session-5lzlix`, contained in HEAD). Helper branches checked: `realism-c`, `tune-help`, `visual` (no realism-a/b, techniques, model-gaps branches exist yet). `Source/WIP/*` is not compiled. Paths are relative to `Source/` unless noted.

### gui-integration.md

The skeleton of the spec is in place: a header, the Easy layout, the four-column Advanced layout with all 13 Col-4 tabs in spec order, the Workshop bench, the Options overlay with all 11 tabs, a Live strip, a Practice drawer, notification banners and a registry of rebindable shortcuts. Most of the gaps are in the header, the cross-cutting rules and the tests. Missing from the header: the snapshot strip, the meters, tap and the dice/reset/gear icons. Missing cross-cutting rules: panel headers with collapse, `?` icons, NEW dots, empty-state hints, drag-to-modulate, per-source mod arcs, three of the right-click menu items, panel right-click menus and undo grouping. The window size contract is also wrong: the minimum is 940x560, the spec says 1280x800. About 60 automatable parameters have no UI at all (scrape, slap/bass, pickup_blend, macro 7/8). Only one of the ten section 22 tests exists in full. Counts: **yes 66 / partial 34 / no 33**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Every automatable param has exactly one canonical UI control | 0.1 | partial | Parameters.cpp vs UI/* | no for ~60 params | none | scrape_* (15), slap_*/pop/ghost/double_thump (25), pickup_blend, macro_assign_a/b have no control; realism-c adds 29 more with none |
| Every feature within 3 interactions | 0.2 | partial | — | — | none | Options reached via File menu (2 clicks) OK; scrape/slap unreachable |
| Easy never hides audible behaviour | 0.3 | partial | EasyPanel | — | EasyLayout tests | Slap/scrape armed via MIDI have no Easy summary |
| Nothing discoverable only by right-click | 0.4 | partial | HeaderBar Learn; ModMatrix | — | MidiLearn tests | Modulate is right-click or MOD tab; snapshot rename/colour only by right-click |
| Panels fixed (no dock/drag) | 0.5 | yes | AdvancedPanel | — | — | |
| Panel header: uppercase + accent bar, status line, collapse chevron, per-preset collapse memory | 0.6 | partial | AdvancedPanel::Column::addSection, Theme drawSectionHeader | — | none | No collapse chevron, no status line, no collapse state |
| Panels present/absent, never greyed | 0.7 | partial | EasyPanel whammy, SlideGroup | — | Editor bridgePopover test | Workshop Slide category is greyed (workshop-ui 9 asks for that); SLAP group n/a since there isn't one |
| Min window 1280x800, cap 2560x1600 | 0.8 | no | PluginEditor.h minimumWidth=940/560, cap 2400x1440, fixed aspect | — | Editor itLaysOutAndPaints... | Different numbers; aspect ratio is locked |
| Advanced-range arc in warning colour + `*` suffix | 0.9 / 21 | yes | Widgets LuthierKnob, RangesUi | all knobs | RangesUi controlsFollowASwappedRangeAndMarkTheValue | |
| Padlock at top of tabs holding advanced values | 0.9 / 21 | partial | AdvancedPanel RangeTabButton | CHARACTER tab | NoiseUi theCharacterTabCarriesAPadlock | Missing on WORKSHOP tab |
| Header 32 px; footer 16 px | 1 | yes | Metrics::headerHeight/footerHeight | — | — | |
| Brand + signature notch Easter egg | 2 | yes | PluginEditor getSecretPixelBounds, SecretPanel | header | — | |
| Preset prev/name/next/save/A-B | 2 | yes | HeaderBar presetPrev/Name/Next, compareA/B, File>Save | header | — | Save is in File menu, not a button |
| Easy/Advanced pill, wrench, slide glyph | 2 | yes | HeaderBar modeButton/workshopButton/slideButton | header | Editor modes test | |
| Snapshot strip (8 buttons + prev/next + bank) in header | 2 / 8 | no | SnapshotStrip exists only in LiveStrip | Live strip only | Editor theLiveTab... | Not in header at all |
| Input meter, output meter, output LED | 2 | partial | HeaderBar OutputLed | header LED only | none | No input meter; no header output meter (Easy tone strip has a LevelMeter) |
| Utility: Panic, MIDI Learn, tap tempo, kill (Live) | 2 | partial | HeaderBar panic/midiLearn | header | MidiLearn arming tests | Tap and kill only on Live strip |
| Overflow: gear, help, dice, reset | 2 | partial | HeaderBar helpButton; File menu Options/Randomise/Reset | File menu | — | Help is the only icon; the other three are File-menu items |
| Header collapse below 1280 (numeric snapshot, 3-dot menu) | 2 | no | — | — | none | |
| Every header control has tooltip + documented shortcut | 2 | partial | HeaderBar setTooltip | — | — | Guitar/tuning selectors have no shortcut |
| A/B compare transient, not serialized | 2 | yes | PluginProcessor slotB, storeToSlot | header A/B | — | |
| Range-lock padlock next to preset name opens Options>Ranges | 2 | yes | HeaderBar rangePadlock, onOpenRanges | header | RangesUi theHeaderPadlockShows... | |
| Easy layout: illustration + rig + playing/tone/rhythm strips | 3 | yes | EasyPanel | Easy | EasyLayout theWindowMatchesSection3 | |
| Headstock click: tuning popover (per-string, capo, temperament) | 3.1 | yes | GuitarBodyComponent TuningPopover | Easy illustration | Editor theHeadstockPopoverEditsPerStringTuning | Per-string detune is engine state, not a parameter (no automation) |
| Pickup click selects active pickup | 3.1 | yes | GuitarBodyComponent onPickupSelected | Easy | Editor everyHitRegion... | |
| Bridge click: whammy popover only if fitted | 3.1 | yes | WhammyPopover::isWhammyFitted | Easy | Editor theBridgePopoverAppearsOnly... | |
| Fretboard shows played notes live | 3.1 | yes | GuitarOverlay stringLevel/stringFret | Easy | — | |
| Rig strip 280 px: circuit card (vol, tone, visualiser) | 3.2 | yes | EasyPanel guitarVolumeKnob/circuitView | Easy rig | — | |
| Rig: pre/post racks 8 slots, click opens pedal popover | 3.2 | yes | CompactRack | Easy rig | FacesIntegration everyRackSlot... | |
| Rig: amp (model + 6 knobs) | 3.2 | yes | EasyPanel ampModel, AmpFacePanel | Easy rig | FacesIntegration theEasyAmpCard... | |
| Rig: cab model, mic1, mic2, blend | 3.2 | yes | EasyPanel cabModel/mic1/mic2/micBlend | Easy rig | — | |
| Rig: room size, wet/dry | 3.2 | yes | EasyPanel roomSize/roomMix | Easy rig | — | |
| Playing strip: mode, humanize, character macro, whammy if fitted | 3.3 | yes | EasyPanel playingModeSelector etc. | Easy | EasyLayout characterMacro test | Mode is Mono/Poly/Controller, not Mono/Poly/Chord |
| Tone strip: input, output, wet/dry, width | 3.4 | yes | EasyPanel inputKnob..widthKnob | Easy | EasyLayout theToneStripIsHeard | |
| Rhythm strip: kit + dice, feel, enable, chord + next-strum readout | 3.5 | yes | EasyPanel rhythmGenreBox/rhythmDice/... | Easy | — | |
| Advanced 4 columns, each scrollable | 4 | yes | AdvancedPanel columns/viewports | Adv | Editor everyWorkspaceTab... | |
| Col1 GUITAR: instrument library, tuning, capo, temperament, scale readout, "Open in Workshop" | 4.1 | partial | AdvancedPanel "Temperament" section; header guitar/tuning selectors | Adv col1 + header | — | No library list and no "Open in Workshop" button in col 1; section is labelled Temperament |
| Col1 BODY: model, dims, woods, bracing, air readout | 4.1 | yes | AdvancedPanel bodyMode..backWood | Adv col1 | — | Link to CHARACTER tab missing |
| Col1 STRINGS: material/gauge per string, age, tension readout | 4.1 | partial | String Set section, stringInfoLabel | Adv col1 | — | Material/gauge are set-wide; per-string is only in Workshop |
| Col1 WHAMMY: type, range, spring, dive/up stop; absent if none | 4.1 | partial | AdvancedPanel "Bridge" section | Adv col1 | — | Always shown, not hidden on hardtail |
| Col2 PICKUPS: selector, per-pickup gain, per-pickup phase, "Edit in Workshop" | 4.2 | partial | AdvancedPanel pickupSelector/pickupVolume | Adv col2 | — | No per-pickup phase param; no Edit-in-Workshop button; pickup_blend unattached |
| Col2 CIRCUIT: vol, tone, pot, cap, bleed R/C, active, cable, visualiser | 4.2 | yes | AdvancedPanel Circuit section, CircuitResponseView | Adv col2 | Circuit tests | |
| Col2 PRE-FX rack: drag reorder, click controls, right-click bypass/delete | 4.2 | yes | PedalRack | Adv col2 | FacesIntegration | |
| Col3 AMP: model, tone, sag, bright, bias, master | 4.3 | partial | AmpFacePanel | Adv col3 | FacesIntegration | No sag/bias parameters |
| Col3 POST-FX, CAB (mic1/2/blend/phase), ROOM | 4.3 | yes | AdvancedPanel | Adv col3 | — | |
| Col3 SUSTAIN: freeze row, e-bow row, feedback LED | 4.3 | yes | AdvancedPanel Sustain section, FeedbackLed | Adv col3 | Sustain tests | |
| Col4 tab strip, fixed order of 13 | 4.4 | yes | AdvancedPanel tabs list | Adv col4 | Editor everyWorkspaceTabSelectsAndPaints | Wraps onto 2 rows on `visual` |
| WORKSHOP tab takes over cols 3+4 | 4.4 / 6 | yes | AdvancedPanel::resized isWorkshopShowing | Adv col4 | WorkshopPanel itPaintsAndTheWorkshopTabTakesOver | |
| MOD tab: LFO/ENV/STEP/FOLLOW/MACROS/RAND cards + route table | 4.4 | partial | ModMatrixPanel | Adv MOD | Modulation tests | MACROS card has no macro knobs; macros 7/8 have no control anywhere |
| RHYTHM tab: voicer, strum + fingerpick editors, feel, STRUM group | 4.4 | yes | RhythmPanel, StrumGroup | Adv RHYTHM | — | |
| RHYTHM: bass step grid for bass family | 4.4 | no | — | — | none | |
| TUNE tab | 4.4 | yes | TunePanel | Adv TUNE | TunePanel tests | Tune-builder detail is audited by another group |
| LIVE tab: snapshots, setlist, morph, expression cal | 4.4 | yes | LivePanel | Adv LIVE | Editor theLiveTabEdits... | Expression cal is on Options EXPRESSION |
| ROUTING tab incl. Aux 8 noise bus | 4.4 | yes | RoutingPanel AuxStrip, kNoiseAux | Adv ROUTING | Routing tests | |
| TONE MATCH tab | 4.4 | yes | ToneMatchPanel | Adv TONE MATCH | ToneMatch tests | |
| CHARACTER: dead spots, wear, drift, electronics, body age, environment | 4.4 | yes | CharacterPanel | Adv CHARACTER | Character tests | |
| CHARACTER > STRING NOISE group + noise-event strip | 4.4 | yes | NoiseGroups | Adv CHARACTER | NoiseUi tests | |
| CHARACTER > PICK group | 4.4 | yes | NoiseGroups pick* | Adv CHARACTER | — | |
| CHARACTER > SETUP group + buzz heatmap | 4.4 | yes | SetupGroup, BuzzHeatmap | Adv CHARACTER | — | |
| CHARACTER > SLIDE group (Slide Mode only) | 4.4 | yes | SlideGroup | Adv CHARACTER | WorkshopPanel aSlideNeedsSlideMode | |
| CHARACTER > CIRCUIT mirror | 4.4 | no | — | — | none | |
| PRACTICE / NOTATION / MIDI OUT / CONTROLLERS / HELP tabs | 4.4 | yes | PracticeSetupPanel, NotationPanel, MidiOutPanel, ControllersPage, HelpTab | Adv col4 | respective tests | |
| CONTROLLERS: custom CC-map editor, multi-controller merge display | 4.4 | no | ControllerMerge (unused outside tests) | — | Controller tests (engine only) | |
| Last-used tab persists globally | 4.4 | yes | UiPreferences | — | Editor theWorkspaceTabWrapsAndIsRemembered | |
| Column widths 260/220/480; stack <1280; Adv unavailable <1000 | 4.5 / 13 | yes | AdvancedPanel::resized, isAdvancedModeAvailable | — | Editor advancedModeIsRefusedBelow... | |
| Options overlay, Ctrl+, , 11 tabs, Escape closes | 5 | yes | OptionsPanel (Overlays.cpp:449) | File menu / Ctrl+, | Editor everyOptionsPageSelectsAndPaints | No header gear icon |
| APPEARANCE: accent tint, data-stream toggle, noise-strip toggle | 5 | no | AppearancePage pendingLabel "not built yet" | — | none | Palette, scale, reduced motion and tooltips are present |
| DIAGNOSTICS: Workshop/Slide/ranges booleans mirror | 5 | no | DiagnosticsPage mirrorNote (stale: says "not built") | — | none | The features exist now, so the note is wrong |
| FILE LOCATIONS incl. Guitars/ and Parts/ | 5 | partial | FileLocationsPage | Options | — | No Guitars/ or Parts/ buttons |
| Workshop bench layout (header, illustration, inspector, drawer, setup, spectrum) | 6 | yes | WorkshopPanel | Adv WORKSHOP / Easy overlay | WorkshopPanel tests | |
| Workshop direct manipulation (pickup, height, saddles) | 6 | partial | BenchIllustration Drag::pickup/saddle, wheel | bench | WorkshopBench tests | Nut drag, fret brush, pick, slide and capo drags are missing (see workshop-ui) |
| Bench A/B 8 slots | 6 | yes | WorkshopPanel slotButtons | bench | WorkshopBench abRecallRoundTrips | |
| Alt-hover audition on shadow spec | 6 | yes | WorkshopBench::beginAudition | bench | auditionNeverCommits | |
| Easy wrench opens Workshop overlay; Esc closes | 6 | yes | WorkshopOverlay | Easy | Editor overlay test | |
| Slide Mode header toggle + S | 7 | yes | HeaderBar::toggleSlideMode | header | — | |
| Slide: fretboard bar at position + slant | 7 | yes | FretboardComponent slide bar | Easy/fretboard | — | |
| Slide: Workshop illustration draws same bar | 7 | partial | GuitarOverlay slideFret | — | — | No slant on the illustration |
| Slide: tuning popover shows glide target | 7 | no | — | — | none | |
| Snapshot click load; Shift-click write; right-click rename/colour/clear | 8 | partial | SnapshotStrip::mouseDown | Live strip | — | No Shift-click write (click on empty captures; right-click captures) |
| Snapshot button colour tag, 12-char label, accent outline | 8 | yes | SnapshotStrip paint | Live strip | — | |
| Live strip contents (snapshots, setlist, tap, morph, kill, monitor) | 9 | yes | LiveStrip | Live Mode | Live tests | |
| Live Mode: 44 px targets, lock Advanced toggle | 9 | yes | LiveStrip, HeaderBar updateModeButtonEnablement | — | — | |
| Live Mode suppresses non-critical tooltips | 9 | no | — | — | none | |
| Practice drawer 32–360 px, collapsed info, 8 tabs | 10 | yes | PracticePanel | drawer | PracticeDrawer tests | Open state persistence is on `tune-help` |
| Mod arcs: 4 px out, 2 px, per-source colour, segmented | 11.1 | partial | LuthierKnob paint (Widgets.cpp:562) | all knobs | none | Single secondary-colour arc; not per source, not segmented |
| Drag source card onto control creates 25% route | 11.2 | no | — | — | none | |
| Right-click "Modulate ->" submenu | 11.2 | yes | Widgets showParameterMenu kModulateMenuBase | all knobs | Editor rightClickOffersModulation... | |
| Footer: version, status line, CPU %, voice count | 12 | partial | PluginEditor::paint | footer | — | Has version, CPU and latency; no status line or voice count |
| Scrolling data stream in empty main area | 12 | no | DataStreamDisplay exists but is never instantiated | — | none | Dead class |
| Reflow: Easy <900 rig becomes tab | 13 | no | — | — | none | Window min is 940 anyway |
| Scale >125% auto-picks smaller scale + notice | 13 | no | — | — | none | UI scale is not applied at all (see accessibility) |
| Empty-state hints (7 listed texts) | 14 | partial | LivePanel setlistEmptyLabel, PracticePanel "No track loaded." | — | none | None of the spec wording; no mod-routes, snapshot, non-slide-guitar, bass or stock-range hints |
| Notifications: banner under header, 32 px, auto-dismiss 5 s unless actionable | 15 | yes | NotificationCentre | — | Editor notificationBannersQueue... | |
| Triggers: preset error / missing IR / missing part / missing guitar / SR change / update / policy / crash / licence | 15 | yes | PluginEditor post*/pollForNotifications | — | Editor theWindowRaisesSectionFifteensTriggers | |
| Trigger: advanced range clamped on preset save | 15 | partial | RangesUi changeRanges returns clamped count | Options RANGES list | RangesUi theRangesPageListsLocksAndClamps | No banner |
| Control right-click: value, reset, copy/paste, MIDI Learn, Modulate | 16 | yes | Widgets.cpp showParameterMenu | all knobs | Editor rightClick test | |
| Control right-click: Assign to macro (8) | 16 | no | — | — | none | |
| Control right-click: unlock/restrict range | 16 | yes | kUnlockRangeMenuId / kRestrictRangeMenuId | all knobs | RangesUi rightClickUnlocksAndRestricts | |
| Control right-click: Automation ID; Show in Options>Shortcuts | 16 | no | — | — | none | Adds non-spec Lock/Randomise items |
| Panel right-click: collapse, reset panel, screenshot, docs | 16 | no | — | — | none | |
| Shortcuts all rebindable (registry) | 17 | yes | AccessibilitySettings buildDefaultShortcuts | Options ACCESSIBILITY | Accessibility shortcutDefaultsMatchTheCanonicalTable | |
| Shortcuts P, \, [ ], 1-9, Shift+1-9, Tab, S, L, D, Ctrl+, O S Shift+S G N R Shift+R / L Z Shift+Z, F1, Ctrl+?, Ctrl+] [, PgUp/Dn, Ctrl+Alt+E, Ctrl+Shift+E | 17 | yes | PluginEditor::keyPressed | — | Editor overlay shortcut tests | Kill toggles rather than holds |
| W toggles Workshop | 17 | partial | — | — | — | In progress on `visual` (toggleWorkshop) |
| Ctrl+T new tune | 17 | no | TunePanel tooltip says "(Ctrl+T)" but nothing binds it | — | none | The tooltip advertises a key that does nothing |
| Space play/pause tune | 17 | partial | TunePanel::keyPressed (when focused); global Space = audition | TUNE | — | |
| Ctrl+E context-aware export | 17 | partial | export -> ExportPanel | — | — | Always the audio export overlay |
| Undo stack of 64, groups within 200 ms | 18 | partial | PluginProcessor kMaxUndoSteps=200, per-gesture | header Undo/Redo | RangesUi Undo stepsOneAction... | Depth 200; grouping is by gesture, not time |
| State boundaries (snapshot/preset/guitar/setlist) need Shift to undo across | 18 | no | — | — | none | |
| Feature-to-location index rows | 19 | partial | see rows above | — | none | Missing: Import MIDI (File menu, drag-drop), bass techniques UI, guided build, drag-out on session recorder, File-menu New Tune/template picker, "Header preset export menu" |
| Import MIDI (File menu / drag onto window) | 19 | no | — | — | none | |
| Bass slap/pop/ghost/LH slap/double thump UI | 19 | no | Parameters slap_* (engine-wired) | no | Slap tests (engine) | |
| Guided build (templates) rail in Workshop | 19 | no | — | — | none | |
| Right-click illustration part -> parts drawer | 19 | no | — | — | none | |
| `?` icon on panels opening pinned Help | 20 | no | — | — | none | Help pins by F1/focus only |
| NEW dot for one week | 20 | no | — | — | none | |
| Diagnostics "What's on the audio path" diagram | 20 | no | — | — | none | |
| First-unlock explainer, once | 20 | yes | RangesUi kExplainerText | — | RangesUi tests | Re-trigger via restore first-run is on `tune-help` |
| Slide bar overlay 6 px, material colour, 80%, 80 ms ease | 21 | yes | FretboardComponent slide bar | fretboard | — | |
| Pick overlay on illustration with handles | 21 | no | GuitarOverlay has no pick | — | none | |
| Buzz heatmap warning/accent + dot glyph | 21 | yes | BuzzHeatmap | CHARACTER SETUP | — | |
| Circuit visualiser live | 21 | yes | CircuitResponseView | col2, Easy | Circuit theAudioPathMatchesTheResponse | |
| Noise event strip 24 px; static count under reduced motion | 21 | partial | NoiseEventStrip | CHARACTER | NoiseUi theEventStripShows... | No reduced-motion count |
| Pressure-state readout mono text | 21 | yes | SlideGroup pressureState | CHARACTER SLIDE | — | |
| Test: every §19 entry resolves to a component | 22 | no | — | — | none | |
| Test: automation ID -> one focused control | 22 | no | — | — | none | |
| Test: tab-order walk | 22 | no | — | — | none | |
| Test: reflow at 6 widths x 6 scales | 22 | partial | EditorTests itLaysOutAndPaintsAcrossItsResizeRange | — | — | 3 sizes, no scales |
| Test: empty-state hints | 22 | no | — | — | none | |
| Test: 13 right-click items per param | 22 | partial | Editor rightClick; RangesUi rightClick | — | — | |
| Test: 1000-op undo random walk | 22 | no | — | — | none | |
| Test: 10 000 random workshop clicks | 22 | partial | GuitarIllustration hitTestingFindsThePartOnTop; WorkshopPanel everyFittedPartIsReachable | — | — | |
| Test: Slide toggle 100x during playback | 22 | no | — | — | none | |
| Test: advanced-range arc marking | 22 | yes | RangesUi controlsFollowASwappedRangeAndMarkTheValue | — | — | |

### ui-wiring.md

The plumbing works, but it is not built the way this spec describes. There is one APVTS, and every control attaches through SliderAttachment, ComboBoxAttachment or ButtonAttachment inside `LuthierKnob`, `LuthierChoice` and `LuthierToggle`. `PhysicalRange` and live range swapping are real and tested. These parts of the spec are not implemented: the display FIFO for meters (meters poll atomics), the command/result queue (structural changes go through direct calls and spin locks), a `uiState` ValueTree (it is a struct), the panel base API, the locale-change broadcast, MIDI Learn "save as global", and most of the section 23 tests. Counts: **yes 20 / partial 21 / no 13**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Single APVTS, no side-channel audible state | 0.1 | partial | PluginProcessor apvts | n/a | — | Per-string detune, rhythm density/hand position, and character maps are engine state outside the APVTS |
| Attach via APVTS attachments only | 0.2 | partial | Widgets.cpp:412,744,815 | n/a | — | EasyPanel rhythmFeelSlider and RhythmPanel density/hand sliders write the engine directly |
| uiState ValueTree saved with preset | 0.3 | partial | PluginProcessor::UiState struct | n/a | StateModel tests | A struct serialised to JSON in the plugin state; Slide Mode is a parameter |
| Listeners removed in destructors | 0.4 | yes | e.g. HeaderBar dtor removeChangeListener | n/a | none | No leak test |
| Structural state via command queue + atomic swap | 0.6 / 4.3 | no | ToneMatch atomic IR pointer only; 16 SpinLock/CriticalSection uses | n/a | none | No LoadPreset/SwapPart/ShadowAudition/AddModRoute commands |
| Params declared once (IDs + layout) | 1 | yes | Parameters.h / Parameters.cpp | n/a | — | Files are Parameters.*, not ParameterIDs.h / ParameterLayout.cpp |
| Display name translated via i18n | 1 | no | Parameters.cpp literal names | n/a | — | |
| PhysicalRange stock/advanced pair | 1 | yes | PhysicalRange.cpp, RangeState | n/a | RangeTests, RangesUi tests | realism-c adds a Strings family |
| Unit enum + category tag per param | 1 | no | — | n/a | — | Unit is a suffix string; no category tag |
| AttachedKnob: rotary, arc, mod arc, warning arc + `*`, size variants, right-click menu | 2 | yes | LuthierKnob | everywhere | RangesUi, Editor rightClick | Mod arc is single-colour |
| AttachedSwitch momentary vs latching | 2 | partial | LuthierToggle | — | — | Latching only |
| AttachedCombo rebuilds on metadata change | 2 | no | LuthierChoice | — | — | |
| PartSlotWidget (uiState + command queue) | 2 | partial | WorkshopPanel cards + BenchIllustration regions | Workshop | WorkshopPanel tests | No shared widget class; no command queue |
| Panel base API (getPanelId, collapse, VT listener) | 3 | no | — | — | — | |
| Panels never own DSP state | 3 | yes | — | — | — | |
| APVTS atomics read on message thread | 4.1 | yes | getRawParameterValue throughout | — | — | |
| Display FIFO per subsystem with the listed drain rates | 4.2 | partial | NoiseEngine::drainEvents ring; workshopFifo | — | NoiseUi eventStrip test | Meters, chord and fretboard poll engine atomics; timer rates differ (e.g. noise strip 30 Hz, not 15) |
| Preset load via swap at block boundary, ranges applied | 5 | partial | PresetManager::loadPreset + bridge.applyAllNow | — | Combo presetSwitchUnderARingingNoteDoesNotClick | Message-thread apply; not a posted command |
| Snapshot never changes ranges | 5 | yes | Snapshot recall path | — | — | |
| LoadGuitar swap + part-acoustics refill | 6.1 | yes | processor loadGuitar / mapSpec | Workshop | WorkshopPreset tests, ThreadProbe mapSpecCalls | |
| Part swap with 5 ms crossfade | 6.2 | partial | WorkshopBench fit | Workshop | none for crossfade | No click test |
| Shadow audition, 30 ms crossfade back | 6.3 | yes | WorkshopBench::beginAudition -> processor.auditionGuitar | Workshop | auditionNeverCommits | 30 ms crossfade not verified |
| Spectrum delta on worker thread, 40 ms budget | 6.4 | yes | Workshop/SpectrumDelta (juce::Thread) | Workshop | WorkshopSpectrum theWorkerCoalescesAndStaysInBudget | Dedicated thread, not a pool |
| Ranges per family; clamp; ClampNotification banner | 7 | partial | RangesUi::changeRanges | Options RANGES | RangesUi tests | No banner |
| Per-control unlock stored in preset | 7 | yes | lockedParameters / per-control state | right-click | RangesUi rightClickUnlocks... | |
| MIDI Learn arm from header/Ctrl+L, next control | 8 | yes | MidiLearnArmLayer, MidiLearn::claimArmedLearn | header | MidiLearn arming tests | Next *clicked* control, not right-clicked |
| MIDI Learn "Save as global" | 8 | no | — | — | none | |
| Meters: peak hold UI-side | 9 | partial | LevelMeter, OutputLed | — | — | |
| Buzz heatmap per-string/per-fret headroom | 9 | yes | BuzzHeatmap | CHARACTER | — | |
| Noise event strip; static count under reduced motion at 5 Hz | 10 | partial | NoiseEventStrip | CHARACTER | NoiseUi | No reduced-motion behaviour |
| LogStream data stream (200 lines, fade, stop 500 ms) | 11 | no | DataStreamDisplay never instantiated | — | none | |
| Mod arcs from snapshot at 30 Hz | 12 | partial | LuthierKnob reads ModMatrix::getOffsetFor at paint | — | none | |
| Drag source onto control -> AddModRoute 0.25, ghost drag | 12 | no | — | — | none | |
| GuitarIllustration from GuitarSpec, one class, interactionMode | 13 | partial | GuitarRenderer shared by GuitarBodyComponent and BenchIllustration | Easy + Workshop | GuitarIllustration tests | Two components share one renderer; no pick overlay |
| NoiseEngine pool, oldest eviction, Aux 8 tap, event on trigger | 14 | yes | DSP/Noise/NoiseEngine | — | Squeak/PickNoise tests | Audited by another group |
| GuitarCircuit control-rate coefficient updates | 15 | yes | DSP/Circuit | — | Circuit sweepingEveryControlDoesNotAllocate | |
| SlideEngine pressure states | 16 | yes | DSP/Slide/SlideEngine | CHARACTER SLIDE | Slide tests | |
| Full state serialisation (APVTS, uiState, mod, snapshots, MIDI maps, ranges, GuitarSpec ref/blob, MIDI export profile) | 17 | yes | PluginProcessor get/setStateInformation | — | StateModel, WorkshopPreset tests | |
| Missing parts fall back with a banner | 17 | yes | processor.takeGuitarNotices -> "missing-part" | — | GuitarMigration tests | |
| Undo: every param change undoable, compound structural | 18 | yes | pushUndoState / gesture entries | header | RangesUi Undo, WorkshopBench | Snapshot-based undo (whole state per entry) |
| Workshop undo strings in real units | 18 | yes | WorkshopBench descriptions | — | WorkshopBench aDragIsOneUndoEntry... | |
| Threading contract; ThreadPool of 2 | 19 | partial | — | — | ThreadProbe in WorkshopPresetTests | No shared pool |
| LocaleCatalog; no literals; LocaleChanged -> refreshStrings | 20 | no | Localisation `tr` used ~12 times in UI vs ~288 literal setText/drawText | — | Localisation catalogCoversTheUi (partial) | No live refresh broadcast |
| AccessibilityHandler on every attachable; custom handlers for fretboard/grid/heatmap/illustration | 21 | partial | AccessibleSetup::configure* (53 uses), CircuitResponseView handler, BuzzHeatmap/illustration titles | — | GuitarIllustration accessibleDescriptionsNameTheParts | Fretboard has no per-fret children |
| Test: attachment leak x100 | 23 | no | — | — | none | |
| Test: 60 s cross-thread invariant | 23 | partial | Support/ThreadProbe | — | WorkshopPreset tests | Narrow |
| Test: preset load determinism | 23 | partial | Combo renderIsDeterministicAfterReset | — | — | |
| Test: 1000 random undo/redo | 23 | no | — | — | none | |
| Test: MIDI Learn 128 CCs | 23 | partial | MidiLearn mapsAndUnmapsCleanly | — | — | |
| Test: display FIFO overflow | 23 | no | — | — | none | |
| Test: shadow audition 100 parts byte-identical | 23 | partial | WorkshopBench auditionNeverCommits | — | — | |
| Test: PhysicalRange toggle 100x no alloc | 23 | partial | RangeTests / Circuit everyCorner... | — | — | |
| Test: part swap crossfade click threshold | 23 | no | — | — | none | |
| Test: spectrum delta vs offline 0.2 dB | 23 | partial | WorkshopSpectrum aNullChangeIsFlat / aRealChange... | — | — | |

### gui-engine-dataflow.md

Most live elements exist and read their data by polling engine atomics or accessors from UI timers. No element implements the per-element drain rate, stale threshold and stale state this spec defines, apart from the buzz heatmap, the noise strip, the feedback LED and the valve glow. Several elements are missing outright: the header input/output meters, the pick overlay, the pickup pulse, the voice count, the tap LED in the header, the preset-browser thumbnails, the MIDI Learn pulse and the data-flow debug overlay. There is no `Tests/Ui/Dataflow/`. Counts: **yes 12 / partial 15 / no 8**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Each element reads one source; no busy-wait; UI never triggers audio work | 0 | yes | — | — | — | By inspection |
| Stale rule defined per element | 0.3 | partial | BuzzHeatmap::isStale, NoiseEventStrip::isStale, AmpFace driveStale | — | FacesIntegration theValvesGlow...GreyWhenStale | Missing on the other elements |
| Input meter (header) 30 Hz, stale 200 ms | 2 | no | — | — | none | |
| Output meter (header) | 2 | partial | LED only; Easy LevelMeter | Easy tone strip | none | |
| Aux bus meters (ROUTING) 30 Hz | 2 | yes | RoutingPanel AuxStrip drawMeter | ROUTING | Routing tests | No stale rule |
| Per-string activity on fretboard (60 Hz), compact strip (30 Hz) | 2 | partial | GuitarOverlay stringLevel/stringFret; FretboardComponent | Easy | — | No separate compact strip |
| Output LED colour map, 60 Hz, red hold 400 ms, stale | 3 | partial | OutputLed (Widgets.cpp:1046) | header | none | 30 Hz; red only while over; no 400 ms hold; dark colour differs from #5A5F66 |
| Chord readout 10 Hz, dim after 3 s | 4 | partial | EasyPanel chordLabel / rhythmReadout | Easy | — | No dimmed stale state |
| Next-strum arrow 60 Hz, hide after 500 ms | 5 | partial | EasyPanel rhythmReadout | Easy | — | Text readout |
| Fretboard played-notes layer (amplitude dot, 60 ms decay) | 6.1 | yes | GuitarRenderer paintOverlay | Easy/Workshop | — | |
| Scale highlight layer (scale trainer) | 6.2 | yes | PracticePanel SCALE / FretboardComponent | drawer | PracticeTrainers | |
| Buzz heatmap layer 30 Hz, 200 ms fade when stale | 6.3 | yes | BuzzHeatmap | CHARACTER SETUP | — | Not drawn on the Workshop bench (workshop-ui 2) |
| Slide bar overlay, stale = frozen at 40% less alpha | 6.4 | partial | FretboardComponent slide bar | fretboard | — | No stale alpha |
| Pick overlay (position, angle) | 6.5 | no | — | — | none | |
| Pickup pulse layer (poles brighten 40 ms) | 6.6 | no | GuitarOverlay highlightedPickup is hover only | — | none | |
| Mod arcs from destination contribution; stale desaturation | 7 | partial | LuthierKnob paint | knobs | none | Single colour; not segmented; no stale state |
| Circuit visualiser updates within 100 ms | 8 | yes | CircuitResponseView | col2/Easy | Circuit tests | |
| Spectrum delta pane: grey committed, accent pending, summary; stale after 30 s with hint | 9 | partial | WorkshopPanel paintSpectrum | Workshop | WorkshopSpectrum tests | Draws the delta, not two traces; no 30 s stale text |
| Noise event strip 15 Hz, 1 s per event, reduced-motion count | 10 | partial | NoiseEventStrip (30 Hz) | CHARACTER | NoiseUi theEventStripShows... | No reduced-motion count |
| Footer voice count + CPU at 4 Hz, "-" when stale | 11 | partial | PluginEditor::paint (CPU + latency) | footer | none | No voice count |
| Scrolling data stream (LogStream) | 12 | no | DataStreamDisplay unused | — | none | |
| Snapshot strip on change | 13 | yes | SnapshotStrip::refresh | Live strip | — | Not in header |
| Setlist triptych | 14 | yes | SetlistTriptych | Live strip | Live tests | |
| Tap-tempo LED in header 60 Hz | 15 | partial | LiveStrip tap pad blinks | Live strip | LiveTapTempo tests | Not in header |
| Kill pill from `kill_switch_active` param | 16 | partial | LiveStrip killButton, KillSwitch object | Live strip | Live tests | Engine state, not a parameter |
| Illustration static cache + live layers | 17/25 | yes | GuitarRenderer build/paint/paintOverlay, keyFor | Easy/Workshop | GuitarIllustration fullRenderIsFastEnough | |
| Preset browser thumbnail preview (worker, 200-item cache) | 18 | no | GuitarRenderer Options::thumbnail exists but the browser does not use it | — | none | |
| A/B compare highlight | 19 | yes | HeaderBar compareA/B | header | — | |
| MIDI Learn button + target pulse at 1 Hz | 20 | no | midiLearnButton toggle state only | header | none | |
| Practice loop LED (red rec / green play) + 4 Hz pulse | 21 | partial | PracticePanel Looper state strip | drawer | PracticeDrawer | Pulse unverified |
| Feedback LED 30 Hz, brightness = magnitude | 22 | yes | FeedbackLed | col3 SUSTAIN | Feedback tests | |
| Session recorder buffer bar in SESSION tab | 23 | partial | PracticePanel session capacity text | drawer | PracticeSetupPanel sessionSetup... | Text, not a bar |
| Tune transport playhead + beat marker 30 Hz | 24 | yes | TuneSectionStrip::setPlayhead | TUNE | TunePanel tests | |
| Tests in Tests/Ui/Dataflow/ (latency, stale, reduced motion per element) | 26 | no | — | — | none | |
| Diagnostics "Show data-flow overlay" | 27 | no | — | — | none | |

### gui-techniques-updates.md

Nothing in this spec is built. There is no TECHNIQUES tab, no Easy technique pills, no technique fretboard overlays, no Mute Row, no preset filter and no tour stop. The engines for SCRAPE and SLAP exist and read their parameters (Parameters.cpp:948, 1103), but none of their 40 parameters has a control. MUTE exists only under `Source/WIP` (MuteGroup, MuteGridEditor, Muting), which is not built. TAP and BEND (microtonal) have neither parameters nor engines. Counts: **yes 2 (trivial "no change" rows) / partial 4 / no 25**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Additive only (no locations removed) | 0.1 | yes | — | — | — | Nothing was changed |
| TECHNIQUES tab in Col 4 between CONTROLLERS and HELP | 0.2 / 1 | no | AdvancedPanel tabs (13) | no | none | |
| Easy Techniques pill row | 0.3 / 2 | no | — | no | none | WIP EasyMuteButton not built |
| Vertical sub-tab rail layout | 0.4 / 1 / 10 | no | — | no | none | |
| SCRAPE sub-tab (string-scraping.md 2) | 1 | partial | Params scrape_* + ScrapeEngine wired | no | Scrape tests (engine) | Glue: 15 params, no UI |
| SLIDE sub-tab (slide-technique-controls.md 1) | 1 | partial | SlideGroup in CHARACTER | CHARACTER SLIDE | WorkshopPanel aSlideNeedsSlideMode | Wrong location; source/range/scripting controls missing |
| SLAP sub-tab (string-slap-technique.md 1) | 1 | partial | Params slap_* + SlapEngine wired | no | Slap tests (engine) | Glue: 25 params, no UI |
| MUTE sub-tab (16-step grid + muting-rhythm 3) | 1 | partial (WIP, not built) | WIP/UI/MuteGroup, WIP/Rhythm/Muting | no | WIP/Tests/MutingTests (not built) | |
| TAP sub-tab (two-hand-tapping 3) | 1 | no | — | no | none | No params or engine |
| BEND sub-tab (microtonal-bends 2) | 1 | no | — | no | none | Only bend_range |
| CASCADE overview grid + conflicts | 1 / 10 | no | — | no | none | |
| Arm pill per sub-tab | 1 | no | scrape_armed / slap_armed params exist | no | none | Glue: two toggles |
| Pill tap toggles arm | 2 | no | — | no | none | |
| Pill hold -> popover (3–5 controls), Esc closes | 2 | no | — | no | none | |
| Pill right-click -> Advanced sub-tab | 2 | no | — | no | none | |
| Pill armed fill + firing indicator dot | 2 | no | — | no | none | |
| Header unchanged | 3 | yes | HeaderBar | — | — | |
| Scrape trail overlay | 4 | no | — | — | none | |
| Tap markers overlay | 4 | no | — | — | none | |
| Mute-zone shading | 4 | no | — | — | none | |
| Bend arc + cents badge | 4 | no | — | — | none | |
| Slap impact flash | 4 | no | — | — | none | |
| Overlays each toggleable, within the 2 ms budget | 4 | no | — | — | none | |
| CHARACTER Right Hand > Tapping section | 5 | no | — | — | none | No "Right Hand" group exists either |
| CHARACTER PLAYING > Microtonal section | 5 | no | — | — | none | |
| RHYTHM pattern editor Mute Row (mute_type per step) | 6 | no | RhythmPanel | — | none | |
| Preset browser "Uses Techniques" filter chip | 7 | no | PresetBrowserPanel | — | none | |
| Onboarding tour stop at Techniques | 8 | no | — | — | none | There is no tour |
| Pill screen-reader labels; Tab/Space/Enter; non-colour armed state | 9 | no | — | — | none | |
| Reduced motion makes overlays instant | 9 | no | — | — | none | |
| Tests (sub-tabs at 1280x800, pills, long-press, cascade, overlay budget, tour) | 12 | no | — | — | none | |

### workshop-ui.md

The bench is the most complete surface in this group. It has hit-tested parts and hover that does not select. Alt-hover plays a real shadow-spec audition, and pickup dragging snaps (1 mm, Shift 0.1 mm, Alt free) with collisions stopped and the reason shown. Height scrolls, with tilt via Shift/Alt, and saddles drag. Each commit is one undo entry in real units. The spectrum delta runs on a worker, is fixed at ±12 dB with auto-zoom, and shows comb notches. There are eight A/B slots, a Swap/Revert/Save-as-user-part inspector, and keyboard nudges. Missing: nut-slot, fret-wear, pick, slide and capo manipulation on the illustration; the pick/slide/capo/heatmap overlays; the narrow-width fallbacks (collapsing inspector, dropdown drawer); per-string override fields; and a screen-reader summary of the spectrum. Counts: **yes 36 / partial 12 / no 6**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Bench is not modal (instrument keeps playing) | 0.6 | yes | WorkshopPanel | Workshop | — | |
| Three feedbacks (visual, numeric, audible) per interaction | 0.3 | partial | WorkshopBench, SpectrumDelta | Workshop | WorkshopBench aMovedPickupIsSeenReadAndHeard | Pickup only |
| Layout: header, illustration, inspector, drawer, setup strip, spectrum | 1 | yes | WorkshopPanel::resized | Adv WORKSHOP / Easy overlay | WorkshopPanel itPaints... | |
| Header: name (modified), Save As Guitar, A/B | 1 | yes | WorkshopPanel guitarName/saveAsButton/slotButtons | Workshop | — | |
| Below 900: inspector collapses to a drawer | 1 | no | resized uses a narrower (190 px) inspector | — | none | |
| Below 700: parts drawer becomes a dropdown | 1 | no | — | — | none | |
| Easy overlay works down to window minimum | 1 | yes | WorkshopOverlay (preferred 1180x720) | Easy | — | |
| Procedural render from GuitarSpec | 2 | yes | GuitarRenderer | Workshop | GuitarIllustration tests | |
| Ruler (mm from saddle) under pickup rail | 2 | yes | BenchIllustration paint | Workshop | WorkshopPanel aPickupDrag...RulerValueFollows | |
| Hit-region outlines on hover/selection | 2 / 3.1 | yes | GuitarRenderer overlay hovered/selected | Workshop | WorkshopPanel hoverDoesNotSelect | |
| Live overlays: pick, slide at slant, capo, buzz heatmap when setup focused | 2 | no | GuitarOverlay has only slideFret | — | none | |
| Repaint budget 8 ms full / 2 ms overlay; separate layer | 2 | partial | cached scene + paintOverlay | — | GuitarIllustration fullRenderIsFastEnough | Overlay budget not tested |
| Hover tooltip: name + one summary value | 3.1 | yes | BenchIllustration setDescription/tooltip | Workshop | Editor everyHitRegion... | |
| Hover doesn't select or flicker the inspector | 3.1 | yes | BenchIllustration hovered vs selected | Workshop | WorkshopPanel hoverDoesNotSelect | |
| Click selects (sticky, full outline) | 3.1 | yes | BenchIllustration::select | Workshop | — | |
| Alt-hover card audition on shadow; inspector greyed; delta shown; 30 ms back | 3.2 | yes | WorkshopPanel::hoverCard, WorkshopBench audition | drawer | WorkshopPanel auditionFromTheDrawerNeverCommits | 30 ms crossfade unverified |
| Per-string select shows set + override fields; overridden string drawn in its colour | 3.3 | partial | BenchIllustration selectedString | Workshop | GuitarIllustration stringColoursFollowSection10 | Inspector override fields for one string not built |
| Hit regions generated from drawing geometry | 4 | yes | GuitarScene hits | — | GuitarIllustration hitTestingFindsThePartOnTop | |
| Pickup drag along axis, 1 mm / Shift 0.1 / Alt free, collision stop | 4 | yes | BenchIllustration Drag::pickup, WorkshopBench::movePickup | Workshop | WorkshopBench snapIs..., aPickupStopsBefore... | |
| Pickup height via screws or scroll, 0.1 mm, 0.5–6 mm | 4 | partial | mouseWheelMove -> setPickupHeights | Workshop | WorkshopBench heightsAndSetupEdits... | Scroll only; no screw-handle drag |
| Pickup tilt by one screw | 4 | partial | Shift/Alt on wheel changes treble/bass only | Workshop | — | Modifier-wheel, not a handle |
| Bridge saddles drag ±6 mm | 4 | yes | Drag::saddle | Workshop | — | |
| Nut slots drag down per string 0.05 mm | 4 | partial | nutDepths knobs in setup strip | Workshop setup strip | — | Knobs, not a drag on the illustration |
| Frets click select, brush wear | 4 | partial | select only; wear via CHARACTER FretWearMap | CHARACTER | — | No brush on the bench |
| Pick drag along string, rotate at corner | 4 | no | Drag enum has no pick | — | none | |
| Slide drag + rotate slant | 4 | no | — | — | none | |
| Capo drag along neck 0–12 | 4 | no | — | — | none | Capo is a Col 1 choice |
| Live value follows pointer in inspector | 4 | yes | onPickupDragged -> inspector | Workshop | WorkshopPanel aPickupDrag... | |
| Comb notches live while dragging a pickup | 4 / 6 | yes | SpectrumDelta::combNotches | Workshop | WorkshopSpectrum combNotchesSit... | |
| Inspector: name, origin, fields with units, compatibility | 5 | yes | WorkshopPanel::refreshInspector | Workshop | WorkshopPanel theInspectorShows... | |
| Editing a factory part makes a user copy + "Save as user part" | 5 | yes | editInspectorField, savePartButton | Workshop | WorkshopPanel editingAFieldMakesAUserCopy | |
| Swap control (drawer filtered) | 5 | yes | swapButton | Workshop | — | |
| Revert control | 5 | yes | revertButton | Workshop | — | |
| Fields get tooltips/accessibility like params | 5 | partial | — | — | — | Rows are painted, not components |
| Plain clamps from part-acoustics, no stock/advanced marking | 5 | yes | Part/PartAcoustics | — | — | |
| Fixture render committed vs candidate, dB delta | 6 | yes | Workshop/SpectrumDelta | Workshop | WorkshopSpectrum aRealChangeShows... | |
| Flat 0 dB shown plainly | 6 | yes | SpectrumDelta summary | — | WorkshopSpectrum aNullChangeIsFlat | |
| Y axis fixed ±12 dB, auto-zoom toggle | 6 | yes | paintSpectrum range; autoZoomToggle | Workshop | — | |
| Worker thread, 40 ms budget, coalesced (latest wins) | 6 | yes | SpectrumDelta (juce::Thread) | — | WorkshopSpectrum theWorkerCoalescesAndStaysInBudget | |
| A/B 8 slots: click store/recall, undoable, uiState, Shift-click clear | 7 | yes | WorkshopPanel slotButtons, UiState::benchSlots | Workshop | WorkshopBench abRecallRoundTrips | |
| One undo entry per commit, real-unit text; drag = one entry; no grouping of swaps; audition never pushes | 8 | yes | WorkshopBench | — | WorkshopBench aDragIsOneUndoEntry..., fittingAPartSaysWhatItReplaced | |
| Empty user-parts text | 9 | yes | WorkshopPanel.cpp:1394 | drawer | — | |
| Slide category greyed with "Turn on Slide Mode (S)" | 9 | yes | WorkshopPanel.cpp:1338 | drawer | WorkshopPanel aSlideNeedsSlideMode | |
| Single-option part type still shown | 9 | yes | drawer categories | drawer | — | |
| Incompatible part hovered shows warning, audition still works | 9 | yes | part.suits check in paintDrawer | drawer | — | |
| Hit regions focusable, in builder order | 10 | yes | BenchIllustration::builderOrder, Tab walks | Workshop | WorkshopPanel keyboardNudgesMatchADrag | Regions are not separate accessibility children |
| Arrow nudge by snap; Shift fine | 10 | yes | BenchIllustration::keyPressed | Workshop | WorkshopPanel keyboardNudgesMatchADrag | Pickup and height only |
| Spectrum announced as a sentence | 10 | partial | getSpectrumSummary | — | — | Not exposed to the accessibility handler |
| Everything reachable by drag is reachable by keyboard | 10 | partial | — | — | — | Saddles have no key path |
| Test: every part hit-testable | 11 | yes | — | — | WorkshopPanel everyFittedPartIsReachableAndNothingElseIs | |
| Test: hover does not select | 11 | yes | — | — | WorkshopPanel hoverDoesNotSelect | |
| Test: audition does not commit (+30 ms return) | 11 | partial | — | — | WorkshopBench auditionNeverCommits | No audio-return check |
| Test: drag one entry / constrained / snap / three feedbacks / null delta flat / 40 ms / A-B / keyboard parity | 11 | yes | — | — | WorkshopBench + WorkshopSpectrum + WorkshopPanel | Three-feedbacks test covers the pickup only |
| Test: nothing on audio thread during bench interaction | 11 | partial | ThreadProbe mapSpecCalls | — | WorkshopPreset tests | |

### proposals/visual-polish.md

This proposal is largely built on the integration branch. It has the guitar-shop palette with exact values, bundled Bebas Neue and Lato fonts, engraved plates, corner screws, a bell knob, a brass fader, and mini toggles. The guitar has key-light shading, lacquer sheen, metal fills and drop shadows. Amp and pedal faces have Tolex, a pilot light, valve glow with a stale state, and knob caps. The headstock brand mark is inlaid, High contrast is flat, and the tests pass. Not built: the VU meter, the room light, and user accent / follow-the-guitar. The `visual` branch adds screenshot tests and tab-label fitting. Counts: **yes 21 / partial 2 / no 4**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Section 6 theme replaces theme.md | 0.1 | yes | UI/Theme.h Palette | all | Theme theDefaultIsTheGuitarShop | |
| 4.5:1 contrast on all 3 palettes; High contrast untextured | 0.2 | yes | Palette::textured | all | Theme everyTextPairMeetsContrast..., highContrastIsFlat | |
| Cached textures, no per-frame work | 0.3 | yes | Faces cached images | — | Faces facesRenderIdenticallyTwice | |
| No animation beyond live elements | 0.4 | yes | — | — | — | |
| Guitar key light + diffuse + edge highlight | 1 | yes | GuitarRenderer.cpp:811 | Easy/Workshop | GuitarIllustration contactSheet | |
| Lacquer sheen by gloss | 1 | yes | GuitarRenderer.cpp:826 | — | — | |
| Metal hardware reflection + hot highlight | 1 | yes | GuitarRenderer metalFill | — | — | |
| Drop shadows for pickups/bridge/guard | 1 | yes | GuitarRenderer shadow() | — | — | |
| Test: High contrast render has no gradient/sheen | 1 | yes | — | — | GuitarIllustration highContrastHasNoLighting | |
| Amp head face (Tolex, grille, plate, generic logo, pilot follows Standby) | 2 | yes | UI/Faces/AmpFace, AmpFacePanel | Adv AMP, Easy rig | Faces thePilotFollowsStandby..., noFaceTextNamesABrand | |
| Pedal faces (enclosure, footswitch, LED follows bypass, knob layout) | 2 | yes | UI/Faces/PedalFace, PedalRack | racks | Faces thePilotFollowsStandbyAndTheLedFollowsBypass | |
| Same knobs and params (layout unaffected) | 2 | yes | — | — | FacesIntegration theAdvancedAmpSectionHasItsControlsOnTheFace | |
| Model-specific knob caps on faces only, arc kept | 3 | yes | UI/Faces/KnobCaps | faces | Faces everyKnobCapRendersAndKeepsTheArc | |
| Tube glow follows drive; grey when stale | 4 | yes | AmpFace drive/driveStale | Adv AMP | FacesIntegration theValvesGlowWithTheDriveAndGreyWhenStale | |
| Optional VU needle meter | 4 | no | — | — | none | |
| Room light (ROOM card warms with size/wet) | 4 | no | — | — | none | |
| Default/High contrast/Light palettes complete | 5 | yes | Accessibility PaletteId | Options APPEARANCE | Theme / Accessibility palette tests | Also colour-blind palettes |
| User accent choice (brass + 5), each ≥4.5:1 | 5 | no | AppearancePage says "accent tint not built" | — | none | |
| Follow-the-guitar accent | 5 | no | — | — | none | |
| Palette values (#1E1511, #2A1E17, #EFE3CC, #B9A58A, #D4A24C, #6FA58A) | 6.1 | yes | Theme.h | — | Theme theDefaultIsTheGuitarShop | |
| Light palette maple/cream | 6.1 | yes | PaletteId::light | — | — | |
| Condensed display heading font + warm sans + tabular numbers | 6.2 | yes | Resources/Fonts Bebas Neue, Lato; Theme.cpp:135 | — | Theme theBundledFontsLoad | |
| Engraved-plate section headers | 6.2 | yes | Palette plate/plateText | — | — | |
| Bell knob with arc; mini toggles; brass fader; framed panels with screws | 6.3 | yes | Theme drawRotarySlider, drawLinearSlider, drawCornerScrews | — | Theme controlsRenderInEveryPaletteAndRepeatExactly | |
| Brass inlaid headstock brand mark; LED kept | 6.4 | yes | Theme.cpp:528 | header | — | |
| Focus rings restyled and visible | 6.5 | partial | — | — | none | Not verified |
| Tests: renders twice identical, HC flat, contrast, pilot/LED, palettes, perf budget | 7 | partial | Faces/FacesIntegration/Theme tests; ScreenshotTests on `visual` | — | — | No perf-budget test with every face visible; the accent test is moot |

### onboarding.md

The onboarding experience is almost entirely missing. There is no welcome banner and no tour, and none of the post-tour discoverability hints exist. The fresh-install preset "Modern Overdrive" does not exist. The range first-unlock explainer is built. The first-run OS defaults, the TUNE and Workshop first-encounter hints, "Restore first-run experience" and drawer-state restore are on the `tune-help` branch, not integration. Sample content: presets, guitars, parts and templates are nearly there. Example tunes, MIDI clips, backing tracks and setlists are not. Counts: **yes 10 / partial 13 / no 15** (items only on tune-help count as partial).

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Great sound <30 s, no modal on first run | 0.1–0.2 | partial | defaults | — | none | No curated default preset |
| First run does no network | 0.5 | yes | Telemetry/updates opt-in | Options | Telemetry tests | |
| Fresh preset `Factory/Rock/Modern Overdrive` | 1 | no | FactoryPresets (no such preset) | — | none | |
| Fresh guitar `Les Paul Standard` | 1 | no | Resources/Guitars/Electric "Vintage Single-Cut" etc. | — | none | No preset with that name; trademark-free names used |
| Easy mode, Live off, drawer collapsed, Workshop closed, Slide off, ranges stock | 1 | yes | UiState defaults, param defaults | — | — | |
| Realism defaults: squeak 25%, player-friendly setup style | 1 | partial | squeak_amount default 0.25 | — | — | Setup style default unverified |
| Welcome banner (Yes / Maybe later x3 / Don't ask again) each version | 2 | no | — | — | none | |
| 12-step tour with popovers, Next/Back/Skip, Esc | 3 | no | — | — | none | |
| Help -> Take the tour | 2 | no | HelpTab | — | none | |
| Post-tour discoverability: `?` pulse, tab dots, dice tooltip, wrench/TUNE/slide pulses for 7 days | 4 | no | — | — | none | |
| First-run defaults: sidechain, recorder, updates, telemetry, crash, beta all off | 5 | yes | Telemetry / recorder defaults | — | — | |
| Reduced motion, high contrast, DPI >150% -> 125%, locale follow OS on first launch | 5 | partial | WIP/UI/FirstRun (not built on integration) | — | WIP FirstRunTests | In progress on `tune-help` (moved out of WIP) |
| Range mode stock, Slide off, Live off | 5 | yes | defaults | — | — | |
| 36 factory presets across 5 categories | 6 | yes | FactoryPresets (~44: Electric 22, Acoustic 7, Bass 8, Classical 2, Utility 5) | browser | Presets tests | |
| 12 factory guitars `.luthierguitar` | 6 | yes | Resources/Guitars (26) | Workshop/header | GuitarMigration tests | |
| 60+ factory parts | 6 | yes | Resources/Parts (148 files) | Workshop drawer | — | |
| 12 tune templates | 6 | partial | Resources/Tunes/Templates (10) | TUNE | TunePanel tests | 2 short |
| 6 example `.luthiertune` tunes | 6 | no | — | — | none | |
| 12 example MIDI clips in Resources/Examples | 6 | no | — | — | none | |
| 6 royalty-free backing tracks | 6 | no | — | — | none | |
| 10 example setlists | 6 | no | — | — | none | |
| Tour as a reusable walkthrough | 6 | no | — | — | none | |
| Advanced-range first-encounter popover, once, exact text | 7 | yes | RangesUi kExplainerText | knobs | RangesUi tests | |
| Re-trigger via Diagnostics "Restore first-run" | 7 / 12 | partial | — | — | — | On `tune-help` |
| TUNE first-session inline hint | 8 | partial | WIP/UI/FirstEncounterHint | — | WIP FirstRunTests | On `tune-help` |
| Workshop first-session inline hint | 9 | partial | WIP/UI/FirstEncounterHint | — | WIP FirstRunTests | On `tune-help` |
| Paths A/B/C documented in manual/videos | 10 | no | — | — | — | Manual not audited here |
| Restore last preset if closed clean | 11 | partial | host state restore | — | StateModel tests | No standalone "last preset" memory |
| Restore window size, mode, tab | 11 | yes | UiState editorWidth/Height/advancedMode; UiPreferences tab | — | Editor theWorkspaceTabWrapsAndIsRemembered | |
| Restore practice drawer state | 11 | partial | — | — | — | On `tune-help` (UiState::practiceDrawerOpen) |
| Restore Slide Mode, last tune | 11 | yes | slide_guitar param; TuneSession restore | — | TuneProcessor tests | |
| Skip welcome unless armed; update banner if available | 11 | partial | "update" notification | — | Editor startup notifications | No welcome banner |
| Reset to first-run (modal confirm, keeps libraries) | 12 | partial | — | — | — | On `tune-help` (DiagnosticsPage restoreFirstRunButton) |
| Version upgrade: "What's new" banner | 13 | no | — | — | none | Changelog is on the UPDATES page |
| NEW dot on new features | 13 | no | — | — | none | |
| Changelog one click away | 13 | partial | UpdatesPage "WHAT IS NEW" | Options UPDATES | — | Two clicks |
| Forward-compatible formats; migrate + dated Backup folder | 13 | partial | Resources/Guitars/migration.json, GuitarMigration | — | GuitarMigration tests | Backup folder not verified |
| Tests: fresh state, tour 12 steps, skip, OS detection, upgrade, restore, first-encounter once | 14 | no | — | — | none | Only WIP FirstRunTests (on `tune-help`) |

### accessibility.md

The data model is thorough: six palettes, the scale steps, reduced motion, verbosity, font override, 15 ship locales with placeholders and fallback, and a rebindable, searchable shortcut table. The rest is thin. **The UI scale is stored but never applied to the window**, so the scale selector does nothing. Only one component (the guitar illustration) reads reduced motion. Almost all UI strings are hard-coded English: no non-English catalog ships, and `tr()` appears about 12 times in UI code against roughly 288 literals. The overlay announcement and meter-value helpers exist but are never called. The fretboard and snapshot strip expose no accessible children. Counts: **yes 16 / partial 18 / no 13**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Every control has an accessible label, role and value | 0.1 / 1 | partial | AccessibleSetup::configure* (53 uses), LuthierKnob titles | — | none | Painted widgets (SnapshotStrip, maps, inspector rows) have none |
| Nothing conveyed by colour alone | 0.2 | partial | SnapshotStrip thicker border; heatmap dot glyph | — | Accessibility colourblindPalettesSeparate... | Mod arc and output LED are colour-only |
| Shortcuts rebindable + printable | 0.3 | yes | AccessibilitySettings rebind/getPrintableShortcuts | Options ACCESSIBILITY, HELP cheat sheet | Accessibility shortcutsRebindAndRefuseClashes | |
| All UI strings in a catalog | 0.4 / 6 | partial | Localisation getBuiltInEnglish | — | Localisation catalogCoversTheUi | Most UI strings are literals |
| Font sizes scale 75–200% without breaking layout | 0.5 / 4 | no | AccessibilitySettings::setUiScale is never consumed | Options APPEARANCE (no effect) | Accessibility uiScaleStepsAndFontFloor (model only) | **Selector has no effect** |
| AccessibleValueInterface on knobs/sliders/buttons/lists | 1 | yes | JUCE default handlers + configureSlider | — | — | |
| Meters report peak dBFS | 1 | no | configureMeter exists, never called | — | none | |
| Fretboard per-fret child elements | 1 | no | FretboardComponent (no handler) | — | none | |
| Snapshot buttons expose names | 1 | no | SnapshotStrip is painted | — | none | |
| Overlays announce on open, focus first element | 1 | partial | OverlayHost::show grabs focus | — | none | announceOverlayOpened is never called |
| Escape closes overlay and returns focus to launcher | 1 | partial | OverlayHost::dismiss -> parent focus | — | Editor everyOverlayShortcut... | Focus goes to the editor, not the launcher |
| NVDA/VoiceOver/Orca regression | 1 / 10 | no | — | — | none | |
| Tab/Shift-Tab order | 2 | partial | JUCE default focus traversal; bench builderOrder | — | none | No order defined or tested for the main window |
| Arrows adjust (Shift fine, Ctrl coarse) | 2 | partial | JUCE slider keys | — | none | No Shift/Ctrl step sizes |
| Enter opens dropdowns / confirms | 2 | yes | JUCE default | — | — | |
| Esc cancels / dismisses | 2 | yes | PluginEditor::keyPressed | — | Editor overlay test | |
| F1 context help for focused control | 2 | yes | getHelpContext (focused section) | — | HelpTab f1AndTheHeaderOpenHelp... | |
| All actions by shortcut | 2 | partial | registry | — | — | No W on integration (on `visual`), no Ctrl+T |
| Show-all-shortcuts overlay with search | 2 | yes | AccessibilityPage searchBox; showShortcuts | Options | HelpTab theSearchFiltersTheSheet | |
| Palettes: Default, Deut, Prot, Trit, High contrast, Light | 3 | yes | PaletteId | Options APPEARANCE | Accessibility palettesMeetContrast..., colourblind... | |
| Palettes ship as Resources/Themes/*.json | 3 | partial | JSON round trip exists; built-in | — | Accessibility palettesRoundTripThroughJson | No Resources/Themes |
| Meters use shape (narrow under -18 dB, over-0 bracket) | 3 | no | LevelMeter | — | none | |
| Scale steps 75/100/125/150/175/200 | 4 | yes | AccessibilitySettings::kScales | Options APPEARANCE | uiScaleStepsAndFontFloor | Stored only |
| Min 10 px font at 100% | 4 | yes | font floor | — | uiScaleStepsAndFontFloor | |
| Reflow when scale increases (knobs shrink, wrap) | 4 | no | — | — | none | |
| Window minimum grows with scale; recover with smaller scale + warn once | 4 | no | — | — | none | |
| Reduced-motion toggle | 5 | yes | AppearancePage reducedMotionToggle | Options APPEARANCE | Accessibility reducedMotionRemovesAnimation (model) | |
| Reduced motion disables data stream, 80 ms ease, header LED pulse | 5 | partial | only GuitarBodyComponent reads it | — | — | Data stream does not exist; noise strip ignores it |
| Catalog `Resources/i18n/<locale>.json`, flat JSON | 6 | partial | Localisation::loadCatalog | — | Localisation tests | No Resources/i18n directory; English only |
| 15 ship locales offered | 6 | yes | Localisation::getShipLocales | Options LOCALIZATION | Localisation everyShipLocaleIsOffered | Offered, but only English has strings |
| Locale switch without restart; UI redraws | 6 | partial | LocalizationPage setLocale | Options LOCALIZATION | — | No refreshStrings broadcast |
| Missing key -> en; missing en logs warning | 6 | yes | Localisation fallback, loggedMissingKeys | — | Localisation missingKeysFallBack... | |
| Named placeholders; no concatenation | 6 | partial | tr (key, {{name}}) | — | Localisation placeholdersAreNamed... | Heavy concatenation in UI (e.g. PluginEditor advancedUnavailableMessage) |
| RTL-safe layout + bidi fixture test | 6 | no | — | — | none | |
| Help shows manual in current locale | 7 | partial | HelpContent (English) | HELP | HelpTab tests | |
| Shortcut panel is live view of bindings | 7 | yes | HelpTab cheat sheet | HELP | HelpTab aRebindShowsUpInTheCheatSheet | |
| Honour system fonts; theme is preference | 8 | partial | fontOverride | Options ACCESSIBILITY | — | |
| Numeric readouts in tabular mono regardless of locale | 8 | yes | Fonts::mono | — | — | |
| CJK/Arabic fallback font stack | 8 | partial | LocaleInfo needsCjk flag | — | none | No glyph test |
| Options>Accessibility: verbosity, rebind table (search, reset), scale, palette, reduced motion, font override | 9 | partial | AccessibilityPage (verbosity, font, table, search, reset) | Options | Editor everyOptionsPage... | Scale/palette/motion are on APPEARANCE (matches gui-integration 5) |
| Options>Localization: locale, fallback, custom path | 9 | yes | LocalizationPage | Options | — | |
| Test: NVDA smoke | 10 | no | — | — | none | |
| Test: keyboard Tab walk | 10 | no | — | — | none | |
| Test: contrast ≥4.5 on 3 palettes; switch does not clip | 10 | yes | — | — | Theme / Accessibility contrast tests | Clip check partial |
| Test: every locale renders every panel at 100/150% | 10 | no | — | — | none | |
| Test: reduced motion no animation frames | 10 | partial | Accessibility reducedMotionRemovesAnimation | — | — | Model-level only |
| Test: CJK font fallback no tofu | 10 | no | — | — | none | |

### Top gaps (group B)

Ranked by user impact.

1. **About 60 engine-wired parameters have no UI**: `scrape_*` (15), slap/bass (`slap_*`, `pop_*`, `ghost_*`, `double_thump_*`; 25) and `pickup_blend`. `realism-c` adds 29 more (noise floor, sustain shaping, tuning stability), also with no UI. These features are only reachable by MIDI and automation, which breaks gui-integration 0.1 and 19.
2. **The TECHNIQUES tab, the Easy technique pills and the fretboard technique overlays do not exist** (gui-techniques-updates, all of it). MUTE is WIP-only; TAP and BEND have no engine.
3. **The UI scale does nothing.** It is stored and offered in Options but never applied, and there is no reflow or scale-aware minimum (accessibility 0.5/4, gui-integration 13).
4. **No onboarding.** No welcome banner, no 12-step tour, no discoverability pulses or NEW dots, and no "Modern Overdrive" fresh preset. The first-run defaults, first-encounter hints and restore-first-run exist only on `tune-help`.
5. **The header is missing the snapshot strip, input/output meters, tap tempo and the dice/reset/gear icons** (gui-integration 2/8). Snapshots are only visible in Live Mode.
6. **Localisation is effectively English-only.** No `Resources/i18n` catalogs ship, and about 96% of UI strings are literals (roughly 288 literal setText/drawText calls against 12 `tr()` calls).
7. **Modulation UX is incomplete.** There is no drag-to-modulate, the mod arc is one colour rather than per source, the MOD tab has no macro knobs, and macros 7/8 (`macro_assign_a/b`) have no control anywhere.
8. **Workshop manipulation gaps.** There is no pick, slide or capo drag; no nut-slot drag or fret-wear brush; no pick/slide/capo/heatmap overlays on the bench; no narrow-width inspector/drawer fallback; and no per-string override fields.
9. **Undo is not what the spec describes.** There is no 200 ms grouping, no Shift-guarded state boundaries, a depth of 200 instead of 64, and none of the 1000-op random-walk tests.
10. **The right-click menu is missing three items** (Assign to macro, Automation ID, Show in Shortcuts), and panels have no right-click menu, `?` icon or collapse chevron.
11. **The window size contract is wrong.** The minimum is 940x560 and the cap 2400x1440 with a fixed aspect ratio; the spec says 1280x800 minimum and 2560x1600 cap.
12. **Screen-reader gaps.** The meter dBFS helper and the overlay announcement are never called, the fretboard has no per-fret children, snapshot buttons are painted with no accessible names, and focus does not return to the launcher.
13. **Empty-state hints (section 14) are mostly absent**, and none uses the spec's wording.
14. **Data-flow staleness is missing on most live elements.** The data stream, voice count, pick overlay, pickup pulse, MIDI Learn pulse and preset-browser thumbnails are not built.
15. **Missing tabs and pages.** The bass step grid, CHARACTER → CIRCUIT mirror, and the CONTROLLERS CC-map editor and multi-controller merge display are not built. Import MIDI (File menu and drag-drop) is also missing.
16. **Shortcuts.** W (Workshop) is only on `visual`. Ctrl+T is advertised in a TunePanel tooltip but not bound. Ctrl+E is not context-aware.
17. **Stale messages in Options.** DIAGNOSTICS still says the Workshop, Slide and advanced-ranges flags are "not built", and FILE LOCATIONS has no Guitars/ or Parts/ buttons although the Workshop writes both folders.
18. **Sample content gaps.** There are no example tunes, MIDI clips, backing tracks or setlists, and 10 of the 12 tune templates.

### Unspecified gaps noticed

- **No preset tagging, favourites, rating or search** in the preset browser, beyond "Uses Techniques", which is not built either. A $200 plugin is expected to have favourites and search.
- **No resizable layout presets** (S/M/L window sizes or a "fit to screen" button). The aspect ratio is also locked, which is unusual for a four-column editor.
- **No in-app undo history list** (a menu of named steps), even though every entry carries a real-unit description.
- **No visible CPU or quality mode switch** ("eco / realtime / render") in Easy Mode. Oversampling is buried in the Advanced MASTER section.
- **No MIDI keyboard or on-screen keyboard** to audition without hardware. There is only the Audition button with fixed phrases.
- **No tuner display.** Guitar players expect one, and none of these specs covers it.
- **No first-class user guitar/part library browser** outside the Workshop drawer (rename, delete, import/export `.luthierguitar` bundles).

### Small glue candidates

Cheap fixes: the engine parameter or feature exists and needs only a control, attachment, button or preset field.

| Param IDs / feature | Put it on | Notes |
|---|---|---|
| `pickup_blend` | Adv Col 2 PICKUPS (next to Selector), Easy pickup popover | Read by the engine (`pickups.setBlend`), no control |
| `macro_attack`, `macro_body`, `macro_drive`, `macro_tone`, `macro_space`, `macro_humanize`, `macro_assign_a` (Macro 7), `macro_assign_b` (Macro 8) | Col 4 MOD tab MACROS card (8 LuthierKnobs) | Macros 7/8 have no control anywhere; 1–6 are only in the Easy playing strip |
| `scrape_armed`, `scrape_trigger`, `scrape_direction`, `scrape_sweep_source`, `scrape_trigger_cc`, `scrape_sweep_cc`, `scrape_start_mm`, `scrape_end_mm`, `scrape_duration`, `scrape_pressure`, `scrape_tool`, `scrape_angle`, `scrape_string_mask` (StringMaskSelector), `scrape_retrigger` | New TECHNIQUES > SCRAPE sub-tab. Interim: a SCRAPE group in CHARACTER beside PICK | Wired in Parameters.cpp:948 |
| `slap_armed`, `slap_type`, `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`, `slap_ghost_cc`, `slap_force`, `slap_palm_position_mm`, `slap_string_mask`, `slap_ghost_mode`, `slap_rebound_gap`, `slap_snap_back`, `slap_body_part`, `slap_strength`, `slap_position_mm`, `slap_thumb_hardness`, `slap_fret_contact`, `pop_strength`, `pop_position_mm`, `double_thump_enabled`, `double_thump_up_ratio`, `ghost_level`, `ghost_damping`, `ghost_auto`, `ghost_velocity_threshold` | TECHNIQUES > SLAP sub-tab and Col 4 RHYTHM (bass mode). Interim: a SLAP group in CHARACTER shown only for the bass family | Wired in Parameters.cpp:1103 |
| `scrape_armed` / `slap_armed` Easy toggles | Easy playing strip (first two technique pills) | Two LuthierToggles |
| Range padlock on the WORKSHOP tab | AdvancedPanel tab builder: use `RangesUi::RangeTabButton` for "WORKSHOP" as for CHARACTER | Comment at AdvancedPanel.cpp:1057 already anticipates it |
| CHARACTER → CIRCUIT mirror | CharacterPanel: add a second `CircuitResponseView` plus `guitar_volume`/`guitar_tone`/circuit knobs | The params and the view already exist |
| "Open in Workshop" / "Edit in Workshop" buttons | Adv Col 1 GUITAR section and Col 2 PICKUPS | Call `setWorkspaceTabNamed ("WORKSHOP")` |
| Ctrl+T New tune | Add a `newTune` binding to the registry; route it to `TunePanel::newFromTemplate` (template picker) | The tooltip already promises it |
| W toggle Workshop | Merge `origin/claude/luthier-visual` (`toggleWorkshop`) | Done on branch |
| Options DIAGNOSTICS feature-flag mirror | Replace `mirrorNote` with three read-only labels: Workshop edited, `slide_guitar`, ranges unlocked | The flags exist |
| Options FILE LOCATIONS Guitars/ and Parts/ buttons | FileLocationsPage | The Workshop already writes these folders |
| Snapshot strip in the header | Reuse `SnapshotStrip` in HeaderBar | The component exists |
| Tap button in the header | HeaderBar, calling `processor.tapTempoNow()` | The shortcut T already works |
| `DataStreamDisplay` | Instantiate in empty main-area space; gate on reduced motion and a new APPEARANCE toggle | The class exists and is unused |
| Meter accessibility | Call `AccessibleSetup::configureMeter` on OutputLed / LevelMeter / AuxStrip | The helper exists and is unused |
| Overlay announcement | Call `AccessibleSetup::announceOverlayOpened` in `OverlayHost::show` | The helper exists and is unused |
| UI scale | Apply `AccessibilitySettings::getUiScale()` via `setScaleFactor` / `Desktop::setGlobalScaleFactor` in the editor's change listener | Stored value is ignored today |
| Noise floor / sustain shaping / tuning stability params (`noise_*` x12, `sustain_*` x9, `stability_*` x8) | CHARACTER (NOISE FLOOR group), Col 3 SUSTAIN (shaping row), Col 1 Tuning Realism | In progress on `realism-c`, which has no UI yet |

---

## Group C



Checked against the integration branch at `91f946f` (`origin/claude/luthier-cloud-session-5lzlix` merged into `claude/luthier-audit`). Helper branches checked: `visual`, `tune-help`, `model-gaps`, `realism-a/b/c`, `techniques`. The notation "in progress on X" means the item exists only on `origin/claude/luthier-X`.

Places things live. **WS tab** = Advanced mode, column 4, WORKSHOP tab (`AdvancedPanel.cpp` workspace tab list). **WS overlay** = Easy mode's wrench, which opens `WorkshopOverlay`. **Illus** = `GuitarBodyComponent`, the illustration shown in both the Easy and Advanced panels. **TUNE tab** = column 4 TUNE tab. **Opt→Ranges** = `OptionsPages.cpp:RangesPage`.

---

### guitar-illustration.md

**Summary:** 101 requirements. **yes 46 / partial 43 / no 12.** About 20 of the partial items are complete on `visual`. The renderer (`GuitarRenderer`, `BodyOutlines.h` with 34 body styles, `HeadstockOutlines.h` with 23 layouts) is substantial and well tested. The biggest gap is ground rule 5, "the illustration is authoritative to the ear". None of the 20 factory body parts carries `illustration.body_style`, and no neck carries `illustration.headstock`. So the outline comes from the guitar file's `meta.body_style`, and **swapping a body in the Workshop never changes the drawn outline**. That also leaves 14 of the 34 outlines unreachable: flying_v, reverse_firebird, bass_p, bass_musicman, bass_thunderbird, headless_bass, acoustic_bass, om, auditorium_000, grand_auditorium_cutaway, cutaway_classical, flamenco, gypsy_jazz_grande_bouche, resonator_square_neck and multiscale. The GUI has **no finish or hardware-colour editor at all**, so section 11 can only be exercised by editing files. Drag-and-drop from the drawer (13.2) is replaced by click-to-fit. There is no buzz-heatmap or pickup-pulse overlay, and the `extended` family is missing.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Procedurally drawn from spec; same renderer for Easy, Workshop and thumbnails | 0.1 | yes | UI/Guitar/GuitarRenderer.cpp:build; GuitarBodyComponent; WorkshopPanel | Illus; WS tab/overlay | GuitarIllustration.* | Thumbnails only on visual |
| Flat visual language (single gradient, no textures) | 0.2 | partial | GuitarRenderer:buildLighting | Illus | GuitarIllustration.highContrastHasNoLighting | Deliberate deviation: visual-polish lighting (DECISIONS "Guitar illustration materials") |
| Every part is a first-class visual (swap → redraw) | 0.3 | partial | resolveStyle, buildHeadstock, inlay choice | WS | theKeyChangesWithEveryVisibleChange | Body swap does not change the outline; headstock and inlay follow body style, not the neck/fretboard part |
| Family switch fully rebuilds | 0.4 | yes | PartLibrary::switchFamily; Processor::switchGuitarFamily | WS drawer "Guitar" | aFamilySwitchGivesTheTargetFamilysGuitar | |
| Illustration authoritative to the ear (drawn = loaded) | 0.5 | partial | resolveStyle vs mapSpec | — | — | Body outline and body part can disagree; pickups/bridge agree |
| Three feedbacks (visual, inspector, audible) | 0.6 | yes | WorkshopPanel inspector + SpectrumDelta + audition | WS | WorkshopBench.aMovedPickupIsSeenReadAndHeard | |
| Full repaint <8 ms, overlay <2 ms | 0.7 | partial | — | — | fullRenderIsFastEnough (120 ms bound) | Budget not asserted at spec level |
| mm coords, origin at saddle, X to headstock, Y treble | 1 | yes | GuitarScene; SceneBuilder | — | — | |
| Fit zoom always shows whole guitar | 1 | yes | GuitarRenderer::fitTransform | Illus/WS | everyFactoryGuitarRendersWithoutClipping | |
| Ctrl-scroll zoom to 4x with pan | 1 | yes | BenchIllustration::mouseWheelMove | WS | BenchZoom test on visual | Pan with middle or right button |
| Static scene cached by spec hash | 2.1 | yes | GuitarRenderer::keyFor; GuitarBodyComponent::rebuildScene | — | theKeyChangesWithEveryVisibleChange | |
| Live overlays from display FIFO each frame | 2.2 | partial | GuitarBodyComponent::timerCallback | Illus | — | Polls engine getStringLevel/getSlideEngine, not a FIFO |
| Hover/selection outlines | 2.2 | yes | GuitarRenderer::paintOverlay | Illus/WS | WorkshopPanel.hoverDoesNotSelect | |
| Drag ghost for part cards | 2.2 / 5.32 | no | — | no | — | No drag-and-drop from drawer |
| Preset-browser thumbnails 128x256 on worker, hash cache | 2.3 / 15 | partial | Options.thumbnail in renderer | no | — | Worker + cache: in progress on visual (GuitarThumbnails.cpp) |
| Families electric/acoustic/classical/bass/resonator | 3 | yes | getGuitarFamilies; getFamilyTemplate | WS "Guitar" | aFamilySwitch… | |
| `extended` family (7/8, fanned) | 3 | no | — | no | — | Not in getGuitarFamilies or getFamilyTemplate; 7/8-strings are "electric" |
| Incompatible parts → family defaults + banner | 3 / 12.1 | yes | PartLibrary::switchFamily | WS | aFamilySwitch… | |
| Electric bodies (12 styles) | 4.1 | partial | BodyOutlines.h | WS (by guitar only) | everyFactoryGuitarHasItsParts | flying_v, reverse_firebird, multiscale drawn by no guitar or part |
| Acoustic bodies (8 styles, 12-string, Selmer petit/grand) | 4.2 | partial | BodyOutlines.h | by guitar only | — | om, 000, GA-cutaway, grande-bouche unreachable |
| Classical / flamenca / cutaway classical | 4.3 | partial | classical, flamenco, cutaway_classical | by guitar only | — | Flamenca Blanca guitar uses the "classical" outline |
| Bass bodies (8 styles) | 4.4 | partial | bass_p, bass_offset, … | by guitar only | — | P-Style Bass guitar draws bass_offset; musicman, thunderbird, acoustic, headless unreachable; no multi-scale bass outline |
| Resonator steel/wood/square-neck | 4.5 | partial | resonator, resonator_square_neck | by guitar only | — | Square-neck unreachable |
| Body part carries outline + attachment points | 4 / 18 | partial | Part::illustration read in resolveStyle | — | — | Code honours illustration.body_style; 0 of 20 body parts set it |
| Z-order layers 1-25 (shadow…truss cover) | 5 | yes | SceneBuilder::build* | Illus | everyFactoryGuitarHasItsParts | |
| Grain per wood (flame, quilt, ash, mahogany, alder) | 5.6 | yes | buildGrain | Illus | — | |
| Pickguard tortoise stipple / pearloid swirl | 5.7 | partial | buildPickguard | Illus | — | Tortoise yes; pearloid swirl missing |
| Bracing shadow through soundhole | 5.9 | yes | SceneBuilder (bracing) | Illus | — | |
| Played-notes overlay | 5.26 | yes | paintOverlay stringLevel | Illus | NoteDots 60 ms on visual | |
| Buzz heatmap overlay | 5.27 | no | — | no | — | Not on any branch |
| Slide bar overlay | 5.28 | yes | paintOverlay slideFret | Illus | — | Slant/material on visual |
| Pick overlay | 5.29 | partial | — | — | — | In progress on visual (pickPath, WorkshopAccessories) |
| Pickup pulse layer | 5.30 | no | — | no | — | |
| Bolt-on / set / through neck drawing | 6 | yes | buildNeck (joint) | Illus | — | |
| Fretboard radius shading (vintage/modern/compound) | 6 | partial | buildNeck radius | Illus | — | Compound gradient missing |
| Fretboard woods + binding | 6 | yes | woodColour; buildBinding | Illus | — | |
| Inlays dot/block/trapezoid/sharktooth/vine | 6 | partial | buildNeck inlay | Illus | — | Chosen by body style, not the fretboard part; vine missing |
| Headstocks 3+3, inline6, reverse, 4-in-line, 2+2, slotted, 6+6 | 6 | partial | HeadstockOutlines.h (23) | Illus | Headstocks test on visual | Reverse only on visual; neck parts carry no headstock id |
| Truss cover + "L" mark at 4% | 6 | yes | buildHeadstock | Illus | — | |
| TOM+stopbar, vintage trem, 2-point, Floyd, wrap, Bigsby | 7 | yes | buildBridge, tremPlate, buildTailpiece | Illus | — | |
| Hardtail ferrules on back | 7 | no | — | — | — | Back is not drawn |
| Pin / pinless / floating+trapeze / moustache / tie-block | 7 | partial | pinBridge, floatingBridge, tieBlock | Illus | — | Pinless missing |
| Resonator biscuit / spider | 7 | yes | buildBridge resonator | Illus | — | |
| Bass vintage / high-mass / with mutes | 7 | partial | bassBridge(highMass) | Illus | — | Mutes missing |
| Pickup draw variants (SC, HB, P90, mini, bar, split-P, J, MM) | 8 | yes | SceneBuilder::pickup | Illus | — | |
| Piezo shown as preamp/jack; soundhole magnetic straddling | 8 | partial | pickup() | Illus | — | Soundhole pickup drawn; piezo indicator unclear |
| `pole_spacing_mm` drives pole spacing | 8 | no | — | — | — | Field never read |
| Cover colour and mounting rings | 8 | yes | pickup() cover | Illus | — | |
| Pickguard colours (9 incl. transparent, gold anodised) | 9 | partial | pickguardColour (by name) | WS: none | — | Pickguard slot has no drawer category, so it cannot be changed |
| Per-body default pickguard template | 9 | yes | BodyStyle.pickguard | Illus | — | |
| String colours per material table | 10 | yes | GuitarRenderer::stringColour | Illus | stringColoursFollowSection10 | |
| String counts 6/7/8/12-pairs/4/5/6 bass | 10 | yes | buildStrings | Illus | everyFactoryGuitarHasItsParts | |
| Per-string material override rendered | 10 | partial | — | — | — | In progress on visual (StringOverride, WorkshopStrings tests) |
| Finish fields type/colour_a/b/burst_shape/gloss/aging | 11 | yes | GuitarFinish; file round-trip | **no** | — | No finish editor in any UI; only guitar files set it |
| Named solid palette (18 colours) | 11.1 | no | — | no | — | No palette or picker |
| Bursts (2-tone, 3-tone, cherry, tobacco, …) | 11.2 | partial | build finish "burst" | no | — | Generic radial + 3-tone band; named bursts only via hex |
| Transparent 60% over grain | 11.3 | yes | finish "transparent" | no | — | |
| Natural | 11.4 | yes | else branch | no | — | |
| Metallic (one highlight stroke) | 11.5 | partial | finish "metallic" | no | — | Gradient rather than a stroke |
| Sparkle dots at 3% | 11.6 | partial | finish "sparkle" | no | — | 30-35% alpha, not 3% |
| Aging: edge wear, fade, yellowing, seeded dings, checking | 11.7 | yes | buildAging | no | agingIsSeededAndStable | Belt-buckle back wear missing |
| Hardware colour table + applies to metal parts | 11.8 | yes | GuitarRenderer::hardwareColour | **no** | hardwareColourIsSilent | No UI; no per-part override |
| Family selector = first drawer category | 12.1 | yes | kCategories "Guitar" | WS drawer | theGuitarCategorySwitchesFamily | |
| One-time confirmation per session | 12.1 | yes | WorkshopPanel::switchFamily | WS | — | |
| 250 ms crossfade | 12.1 | partial | — | — | — | In progress on visual (IllustrationMotion.h) |
| Banner listing replaced parts | 12.1 | yes | switchFamily `replaced` | WS limit line | aFamilySwitch… (banner) | Also posted as a "missing-part" warning via takeGuitarNotices |
| Family change via template; keep compatible parts | 12.2 | yes | PartLibrary::switchFamily | WS | aFamilySwitch… | Not a literal SwapPartCommand |
| Per-family template files `*_default_template.luthierguitar` | 12.2 | partial | getFamilyTemplate maps to factory guitars | — | — | No template files; extended missing |
| 12.3 changes (body, strings, scale, bridge, pickups, nut, frets, tuners, circuit) | 12.3 | yes | template parts + writeGuitarParameters | WS | — | |
| Amp defaults follow family | 12.3 | partial | applySpec amp.setModel | — | — | Family-aware amp: in progress on visual (FamilyDefaults) |
| Family → bass mode in rhythm engine | 12.3 | yes | retargetStrumDefaults; TunePlayer bass | — | TunePlayer.theBassGoesToTheEngineOnlyForABass | |
| Slide-friendly hint on a high nut | 12.3 | no | — | — | — | |
| MIDI-out profile default per family | 12.3 | no | — | — | — | |
| Preserve name, FX, amp, mod, snapshots, seed | 12.4 | yes | switchGuitarFamily | — | WorkshopFamily.aFamilySwitchKeepsWhatSection12_4Keeps | |
| Hit regions, topmost wins | 13.1 | yes | GuitarRenderer::hitTest | WS | hitTestingFindsThePartOnTop (10k clicks) | |
| Alt+click = audition | 13.1 | partial | hoverCard(altDown) | WS drawer | auditionFromTheDrawerNeverCommits | Alt on the illustration means "bass side only" |
| Shift+click targets the region below | 13.1 | no | — | — | — | Shift means fine or treble-only |
| Ctrl+click string → override drag target | 13.1 | partial | — | — | — | Card onto selected string: in progress on visual |
| Drag cards onto the illustration (pickup/bridge/body/string/pick/slide/capo) | 13.2 | no | clickCard fits into targetSlot | WS (click) | clickingACardFitsItAsOneUndoEntry | Click-to-fit replaces drag-and-drop |
| Capo card onto fret / headstock removes | 13.2 | partial | — | — | — | Capo drag by frets on visual |
| Drag pickup along axis, ruler snap | 13.3 | yes | BenchIllustration::mouseDrag | WS | aPickupDragIsOneEntry…; snapIsOneMillimetre… | |
| Scroll/drag screw handles → height per side | 13.3 | yes | mouseWheelMove (Shift treble, Alt bass) | WS | heightsAndSetupEditsAreOneEntryEach | |
| Drag saddle → intonation | 13.3 | yes | Drag::saddle | WS | — | |
| Drag nut slot → slot depth | 13.3 | partial | setup-strip knobs | WS setup strip | — | Slot drag on visual (WorkshopNut test) |
| Drag capo / pick / slide on illustration | 13.3 | partial | — | — | — | In progress on visual (WorkshopAccessories tests) |
| Snap modifiers fine / coarse Shift / ultra-fine Ctrl | 13.3 | partial | mouseDrag: Shift fine, Alt free | WS | snapIsOneMillimetreFineWithShiftFreeWithAlt | Modifier mapping differs from spec |
| Frequency-band warm/cool tint while auditioning (500 ms) | 14 | partial | — | — | — | In progress on visual (8b77b82) |
| Thumbnail reduced detail | 15 | yes | Options.thumbnail | — | — | Nothing calls it on base |
| Accessible child per part with documented strings | 16 | partial | Hit.description; Bench setDescription | WS | accessibleDescriptionsNameTheParts | One component whose description changes; no per-part children |
| Tab walks parts; Enter opens inspector; announce on click | 16 | partial | BenchIllustration::keyPressed | WS | keyboardNudgesMatchADrag | Tab yes; arrows nudge instead of navigating |
| Reduced motion: no crossfade, static states | 16 | partial | overlay.reducedMotion (glow) | Illus | — | Full treatment on visual |
| Perf budgets (40 ms / 2 ms / 120 ms / 100 ms / cache 200) | 17 | partial | — | — | fullRenderIsFastEnough | Only the static render is timed |
| New shapes as data, no code | 18 | partial | illustration.body_style / headstock | — | — | Bridge and pickup shapes chosen by name heuristics |
| T: render at 128…3072 px, no clipping | 19 | yes | — | — | everyFactoryGuitarRendersWithoutClipping | |
| T: hash changes on swap / move >0.5 mm | 19 | yes | — | — | theKeyChangesWithEveryVisibleChange | |
| T: every family → every family | 19 | yes | — | — | aFamilySwitchGivesTheTargetFamilysGuitar | 5 families |
| T: 10 000 random clicks | 19 | yes | — | — | hitTestingFindsThePartOnTop | |
| T: drag bounds (route; height ≥0.8 mm) | 19 | partial | WorkshopBench::movePickup | — | aPickupStopsBeforeItOverlaps… | 0.8 mm stop on visual |
| T: burst vs reference SVG | 19 | no | — | — | — | |
| T: live dot within 60 ms | 19 | partial | — | — | — | NoteDots on visual |
| T: reduced motion / thumbnail cache | 19 | partial | — | — | — | On visual |

---

### guitar-workshop.md

**Summary:** 45 requirements. **yes 35 / partial 9 / no 1.** The data model (`Part`, `PartLibrary`, `WorkshopGuitar`), resolution and fallback, compatibility warnings, string-count clamping, Save As Guitar/Part, presets carrying the override, and parameter retirement are all done and tested. The UI gaps: **4 of 15 slots (top, fretboard, tailpiece, pickguard) have no drawer category**, so their parts ship but cannot be fitted from the GUI. The string-count excess is not shown in the inspector. There is no rescan when the parts folder changes. The "part swap changes audio" test compares derived values rather than rendered spectra.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| A guitar is its parts | 0.1 | yes | WorkshopGuitar; mapSpec | WS | — | |
| Parts are files; factory read-only, user in Documents/Luthier/Parts | 0.2 | yes | PartLibrary folders | — | theFactoryLibraryIsThere | |
| Compatibility is advisory | 0.4 | yes | getCompatibilityWarnings; drawer "Unusual here" | WS | incompatiblePartsFitWithAWarning | |
| Spec owned by the audio thread, atomic swap | 0.5 | yes | applyWorkshopGuitar (ScopedStructuralChange) | — | aPartSwapDuringANoteIsClickFree | Block-boundary swap on model-gaps |
| 25 enum guitars ship as .luthierguitar | 0.6 | yes | Resources/Guitars (27 files) + migration.json | Guitar type combo | GuitarMigration.* | |
| Slot body | 1 | yes | GuitarSlot::body | WS "Body" | — | |
| Slot top | 1 | partial | GuitarSlot::top | **no** | — | 6 Tops parts; no drawer category |
| Slot neck | 1 | yes | | WS "Neck" | — | |
| Slot fretboard | 1 | partial | | **no** | — | 6 parts; no category |
| Slot frets / nut / bridge / tuners | 1 | yes | | WS categories | — | |
| Slot tailpiece | 1 | partial | | **no** | — | 3 parts; no category |
| Slots pickups neck / middle / bridge | 1 | yes | | WS "Pickups" | — | |
| Slots wiring / strings | 1 | yes | | WS "Wiring"/"Preamp"/"Strings" | — | |
| Slot pickguard | 1 | partial | | **no** | — | 6 parts incl. None; no category |
| Non-part fields hardware_color / finish / setup / seed | 1 | partial | WorkshopGuitar | Setup strip only | — | No finish or hardware UI |
| Zero-pickup guitar legal (acoustic) | 1 | yes | mapSpec | — | — | |
| Part types + field sets (16 types) | 2 | yes | PartType; Part::fields | WS inspector | — | |
| Accessories slide / pick / capo | 2 | yes | PartType slide/pick/capo | WS "Pick"/"Slide"/"Capo" | aSlideNeedsSlideMode; WorkshopCapo.* | |
| GuitarSpec in-memory model | 3 | yes | WorkshopGuitar (named differently) | — | — | |
| Existing GuitarSpec widened; table as fallback | 3.1 | yes | mapSpec baseTypeFor | — | aMissingGuitarFileFallsBackToItsType | |
| DerivedAcoustics cached; 5 ms crossfade | 3.2 | partial | mapSpec once per swap | — | aSwapMapsOnceNotPerBlock | Several derived fields are never read by the engine (see part-acoustics) |
| Factory / user layout Parts/<Cat>, Guitars/<Family> | 4 | yes | PartLibrary::get*Folder | — | — | |
| Scanned at startup **and on folder change** | 4 | partial | PartLibrary::refresh (ctor, savePartAs) | — | — | No watcher |
| User part wins over factory | 4 | yes | PartLibrary::find | — | aUserPartBeatsTheFactoryOne | |
| Missing ref → category default | 4.1 | yes | PartLibrary::resolve/getDefault | — | aMissingPartFallsBackAndSaysSo | |
| Missing-part notification with jump-to-Workshop | 4.1 | partial | PluginEditor notices "missing-part" | banner | — | No jump action; stale comment at PluginEditor.cpp:805 says it is unreachable |
| Missing part in error log | 4.1 | yes | ErrorLog PART_MISSING | — | aMissingPartFallsBackAndSaysSo | |
| Compatibility warning text | 5 | yes | PartLibrary.cpp:125; inspector line | WS | incompatiblePartsFitWithAWarning | |
| String count = min(neck, bridge) | 5.1 | yes | getStringCount | — | aStringCountMismatchClamps | |
| Excess reported **in the inspector** | 5.1 | no | ErrorLog only | no | — | |
| Tuning/per-string state resize | 5.1 | yes | writeGuitarParameters tuning | — | choosingATypeGivesItsStringCount | |
| Save As Guitar, Ctrl+G | 6 | yes | Processor::saveGuitarAs; registry 'g' | WS button, Ctrl+G | saveAsGuitarWritesAFile… | Accessibility.cpp:484 comment stale |
| By reference; Bundle parts option | 6 | yes | saveGuitarAs(bundleParts) | dialog button | — | |
| Preset guitar.reference updated | 6 | yes | guitarReference = "User/…" | — | saveAsGuitar… | |
| Save As Part from inspector | 7 | yes | Processor::savePartAs | WS "Save as user part" | saveAsPartMakesAUserPartAndFitsIt | |
| Editing a factory part → unsaved user copy | 7 | yes | WorkshopBench::editField | WS inspector | editingAFieldMakesAUserCopy | |
| Preset reference + override; override wins | 8 | yes | Processor guitarOverride | — | anEditedGuitarTravelsWholeInTheState | |
| Workshop adds no parameters | 9 | yes | — | — | — | |
| pickupPosition/Height retired, migrated | 9 | yes | PresetManager.cpp:577 | — | oldPickupPlacementParametersBecomeTheGuitars | 6 IDs retired, not 9 |
| T: factory round trip | 10 | yes | — | — | everyFactoryGuitarLoadsAndRoundTrips | |
| T: part swap changes the spectrum; swap back restores to −80 dB | 10 | partial | — | — | everyMappedFieldMovesSomething; SpectrumDelta | Derived values only, no render |
| T: swap click-free | 10 | yes | — | — | WorkshopSwap.aPartSwapDuringANoteIsClickFree | |
| T: missing / user-wins / incompatible / string clamp | 10 | yes | — | — | Workshop.* | |
| T: no FS on audio thread; mapped once | 10 | yes | ThreadProbe | — | noFileIsTouched…; aSwapMapsOnce… | |
| T: override self-contained | 10 | yes | — | — | anEmbeddedGuitarNeedsNoPartFiles | |

---

### part-acoustics.md

**Summary:** 59 requirements. **yes 38 / partial 11 / no 10.** `mapSpec` exists, is deterministic and runs once per swap. Wood, chambering, joint coupling, bridge mass, frets, nut, magnets, covers, position, wiring, strings and setup are mapped. Several mapped values never reach the audio path: **`airResonanceHz/Q`, `bodyGainDb` and `finishDampingDb` are computed and never read by the engine** (so the "chambering air mode" test checks a struct field, not audio). `windingPitchPerMm` is recomputed separately by the engine. `feedbackGain` is wired only on model-gaps. These fields are unmapped: tuners (all), `nut.friction`, `fretboard.radius_mm`, neck `wood`/`profile`, `strings.core`, `tension_kg`, `coil_turns`, `wiring.switching` and `bridge.piezo`; DECISIONS.md:158 records most of these as deferred. Wood E and tanδ do not set body mode Q; only density moves the modes.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| One function mapSpec → DerivedAcoustics, once per swap | 0.1 | yes | Model/Workshop/PartAcoustics.cpp:mapSpec | n/a | aSwapMapsOnceNotPerBlock | |
| Physical units in, DSP units out | 0.2 | yes | mapSpec | n/a | — | |
| Monotonic and continuous | 0.3 | yes | — | n/a | theMappingIsMonotonic | 5 sweeps |
| Every constant named and sourced | 0.5 | partial | comments | n/a | — | Several fitted constants (0.09 pull, 1.4 coupling) are loosely sourced |
| Wood table (17 woods: ρ, E, tanδ) | 1 | yes | lookUpWood | inspector | — | |
| Body density/E sets mode frequency | 1.1 | partial | body.resonanceTrim = √(ρ_table/ρ) | — | monotonic (density) | E not used |
| Damping tanδ sets mode Q | 1.1 | no | — | — | — | Only the fretboard uses tanδ |
| Neck/fretboard density → coupling and dead spots | 1.1 | no | — | — | — | |
| body.wood / density override | 2 | yes | mapSpec body | WS inspector | everyMappedFieldMovesSomething | |
| body.thickness_mm | 2 | yes | scaleDepth | inspector | moves-something | |
| body.area_cm2 | 2 | yes | scaleWidth; air volume | inspector | moves-something | |
| body.chambering → modes/shape | 2.1 | yes | shapeFor | inspector | — | Via BodyShape |
| Chambering air resonance (range, Q, 1/√V) | 2.1 | partial | d.airResonanceHz/Q | — | chamberingPutsTheAirModeInItsRange | Computed, never used by the engine |
| Chambering mode gain dB | 2.1 | partial | d.bodyGainDb | — | — | Not consumed |
| Chambering sustain | 2.1 | yes | sustainScale × chamber.sustain | — | — | |
| Chambering feedback coupling | 2.1 | partial | d.feedbackGain | — | — | Wired on model-gaps (FeedbackLoop::setBodyCoupling) |
| body.bracing → mode splitting | 2 | yes | engineBracing | — | — | |
| scale_length → tension T=(2Lf)²μ | 3 | yes | stringTensionNewtons | inspector | scaleLengthSetsTension | |
| neck.profile (mass only) | 3 | no | — | — | — | |
| neck.joint coupling 0.55/0.80/0.95 | 3 | yes | jointCoupling | inspector | couplingsMultiply | |
| fretboard.wood → termination brightness | 3 | yes | fretBrightness × tanδ | — | — | |
| fretboard.radius_mm → buzz clearance | 3 | no | — | — | — | DECISIONS:158 defers it |
| frets.material brightness (NS .70, SS .90, EVO .80, brass .60) | 4 | yes | fretMaterialBrightness | inspector | aReferenceGuitarSoundsLikeTheEngineDefault | |
| frets.height → buzz | 4 | yes | setup.fretHeight | setup | moves-something | |
| frets.width → duller | 4 | yes | fretBrightness | — | moves-something | |
| frets.count → range | 4 | yes | spec.maxFrets | — | — | |
| nut.material, open strings only | 4 | yes | nutBrightnessFactor | — | — | |
| nut.slot_depths → open clearance | 4 | yes | setup.nutDepth | setup strip | — | |
| nut.friction → bend stability | 4 | no | — | — | — | |
| bridge.mass_g → termination | 5 | yes | terminationMassG → sustainScale | inspector | monotonic | |
| bridge.coupling | 5 | yes | couplingFraction → spec.couplingAmount | inspector | couplingsMultiply | |
| bridge.type defaults table | 5 | partial | part files | — | — | Values live in part files, not a type table |
| has_tremolo / tremolo_type → WhammyEngine | 5 | yes | d.spec.bridge | — | — | |
| bridge.piezo adds a piezo source | 5 | no | — | — | — | Only a pickup with family "piezo" counts |
| tailpiece.mass_g | 5 | yes | terminationMassG | — | — | |
| tailpiece.break_angle → brighter | 5 | yes | fretBrightness × angle | — | — | Tailpiece not swappable in UI |
| Pickup L, R, C → resonance | 6 | yes | PickupSpec | inspector | moves-something | |
| magnet → pull, damping, flat pull | 6 / 6.2 | yes | lookUpMagnet; magnetSustain/Detune | inspector | magnetPullShortensSustainAndPullsFlat | |
| coil_turns → output + L | 6 | no | output_dbfs_reference used instead | — | — | |
| pole_piece_material | 6 | yes | poleBrightness | — | — | |
| cover: nickel −0.8 dB @ 4 kHz | 6 | yes | coverLossDbAt4k | — | aCoverCostsTopEnd | |
| position_mm → comb | 6.1 | yes | spec.position | WS drag | pickupPositionSetsTheComb | |
| height → level and damping | 6 | yes | heightMm → magnet proximity | WS wheel | — | Level trim from height not clear |
| wiring pots/cap/taper/bleed/active → circuit | 7 | yes | d.wiring → writeGuitarParameters | Circuit panel | — | |
| wiring.switching topology | 7 | no | — | — | — | |
| strings gauges → μ, tension | 8 | yes | computeSpec | — | — | |
| winding round/flat/half/coated | 8 | yes | materialFor | — | — | |
| winding_material | 8 | yes | materialFor | — | — | |
| core round/hex | 8 | no | — | — | — | |
| winding_pitch_per_mm → squeak/chirp | 8 | partial | d.windingPitchPerMm | — | — | Not passed to engine; PlayingNoise recomputes |
| tension_kg override | 8 | no | — | — | — | |
| Inharmonicity B ∝ d⁴E/(TL²) | 8 | yes | StringMaterials::computeSpec | — | — | |
| pickguard.mass_g → top damping | 9 | partial | terminationMassG only | — | moves-something | Not a top-damping term |
| hardware_color silent | 9 | yes | — | — | hardwareColourIsSilent | |
| finish.gloss → acoustic −0.5 dB, Q −8% | 9 | partial | d.finishDampingDb | — | — | Never consumed |
| finish.aging → body break-in | 9 | yes | body.age | — | — | |
| Composition: masses add, couplings multiply, losses add | 10 | partial | — | — | couplingsMultiply | Loss-domain Q sum not implemented |
| T: every numeric field moves the **spectrum** | 11 | partial | — | — | everyMappedFieldMovesSomething | Derived struct only; unmapped fields skipped |
| T: scale length / magnet / comb / cover / chambering / hw colour / determinism / once | 11 | yes | — | — | PartAcoustics.* | Chambering test is on a struct field |

---

### advanced-ranges.md

**Summary:** 49 requirements. **yes 36 / partial 8 / no 5.** `PhysicalRange`, `RangeRegistry`, `RangeState`, the preset `ranges` block with legacy derivation, the Options→RANGES page, right-click unlock/restrict, marking (arc + `*`), the stop at the stock edge with an inline notice, the first-unlock explainer and randomise-respects-stock are implemented and tested. Gaps:
- The **modulation family is not implemented** (`ModSources::setRateHz` hard-clamps 0.01–40 regardless of mode).
- **Telemetry `advanced_ranges_used` and its Diagnostics mirror are missing.**
- Snapshots store normalised values, so a recall after locking re-maps rather than clamps. There is no test for it.
- There is no explicit undo-restores-clamp test.
- Squeak probability/moisture/pressure, pick bevel/wear and slide pressure have no registry rows.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Stock is the default everywhere | 0.1 | yes | RangeState default | — | theRangesBlockRoundTrips… | |
| Advanced is marked, never hidden | 0.2 | yes | RangesUi::tagSlider/markReadout | all knobs | markingFollowsTheValueNotTheMode | |
| Widening changes no audio; narrowing clamps, undoable, announced | 0.3 | yes | RangeState::applyTo; changeRanges | Opt→Ranges | wideningPreserves…; narrowingClamps… | |
| Range mode in preset; display prefs per user | 0.4 | yes | PresetManager; UiPreferences | — | — | |
| Only physical params get a PhysicalRange | 0.5 | yes | RangeRegistry sparse | — | — | |
| No parameter count change | 0.6 | yes | — | — | — | |
| PhysicalRange struct + invariants | 1 | yes | PhysicalRange::isValid | — | everyPhysicalRangeIsValid | |
| Stock = shipped declared range | 1.0 | yes | noteDeclaration / findDeclarationMismatches | — | stockMatchesTheDeclaredRange | |
| Normalised against the live range | 1.1 | yes | makeRange | — | normalisationFollowsTheLiveRange | |
| Mode change is structural via the command queue | 1.1 | partial | Processor::changeRanges (message thread) | — | — | Direct call, not queued |
| Switch to advanced preserves plain values | 1.2 | yes | applyTo | — | wideningPreservesEveryPlainValue | |
| Switch to stock clamps + banner with count | 1.3 | yes | RangesUi::apply | Opt→Ranges | narrowingClampsAndReportsTheCount | |
| Mod routes sweep the live range | 1.4 | yes | ParameterBridge (normalised) | — | — | By construction |
| Seven family keys | 2 | yes | RangeFamily | — | — | |
| Per-control unlock via right-click | 2 / 4 | yes | Widgets showParameterContextMenu | right-click any physical knob | rightClickUnlocksAndRestrictsOneControl | |
| Modulation family = setter clamps | 2.1 / 3.4 | **no** | ModSources::setRateHz fixed jlimit(0.01, 40) | no | — | Envelope, sequencer and follower clamps also absent |
| amp rows (gain, EQ, presence, master) | 3.1 | yes | PhysicalRange.cpp:99-104 | Amp face | — | |
| circuit rows (pots, cap, cable, input Z) | 3.2 | yes | PhysicalRange.cpp:111-119 | Circuit panel | — | treble_bleed is a choice, not ranged |
| squeak family rows | 3.3 | partial | squeakAmount, squeakMinTravel | — | — | probability/moisture/pressure missing |
| pick family rows | 3.3 | partial | thickness, angle, tip, click, chirp, scrape | — | — | bevel, wear missing |
| buzz family rows | 3.3 | yes | action, relief, nut depths, fret height | WS setup | — | "buzz threshold" absent |
| slide family rows | 3.3 | partial | slant, noise, clank | — | — | mass (part) and pressure missing |
| strings family (new) | — | partial | — | — | — | In progress on realism-c/realism-a (PhysicalRange.cpp diffs) |
| Sparse registry, empty family reads stock | 3.5 | yes | RangeRegistry | — | — | |
| `ranges` block schema (families + per_control_unlocks) | 4 | yes | RangeState::toVar/fromVar | — | theRangesBlockRoundTrips… | |
| Redundant unlock dropped on save | 4 | yes | setUnlockedIndividually | — | — | |
| Restrict offered only when in the list | 4 | yes | buildParameterContextMenu | right-click | rightClickUnlocksAndRestricts… | |
| Legacy derivation per family from plain values | 4.1 | yes | deriveFromCurrentValues | — | theRangesBlockRoundTripsAndDerivesWhenAbsent | |
| Malformed block = absent | 4.1 | yes | fromVar | — | same | |
| Snapshots don't carry mode; recall clamps | 5 | partial | Snapshots.cpp stores getValue() (normalised) | — | — | Re-maps rather than clamps; untested |
| A/B slots carry own ranges | 5 | yes | slotA/slotB full state | header A/B | — | Assumed via getStateInformation |
| Randomise respects stock (pref, default on) | 5 | yes | presets.randomise(respectsStock) | Opt→Ranges | randomiseStaysInStockUnlessToldOtherwise | |
| Reset to default never changes mode | 5 | yes | — | — | — | |
| MIDI Learn over the live range | 5 | yes | normalised | — | — | By construction |
| Warning-colour arc past stock | 6.1 | yes | LookAndFeel + kStockMin/MaxProperty | knobs | controlsFollowASwappedRangeAndMarkTheValue | Arc test on visual |
| `*` suffix on marked readouts | 6.1 | yes | RangesUi::markReadout | knobs | markingFollows… | |
| Tab padlock (accent unlocked / muted locked) | 6.1 | partial | RangeTabButton (CHARACTER only) + header PadlockButton | CHARACTER tab, header | theHeaderPadlockShowsOnly… | WORKSHOP tab padlock on visual |
| Opt→Ranges master toggle with clamp preview | 6.2 | yes | RangesPage::masterToggled | Opt→Ranges | theRangesPageListsLocksAndClamps | |
| "Always show warning colour" pref | 6.2 | yes | kWarningColourKey | Opt→Ranges | — | |
| Out-of-stock list with per-row clamp + empty text | 6.2 | yes | RangesPage::clampOne | Opt→Ranges | theRangesPageLists… | |
| Locked: inline notice text; drag stops at stockMax | 6.3 | yes | showLockedRangeNoticeIfAtEdge; Widgets:693 | knobs | — | |
| First-unlock explainer, once | 6.4 | yes | showExplainerIfFirstTime | popover | — | No test |
| Undo `ranges-toggle`, grouped 200 ms, restores clamps | 7 | partial | pushUndoState full snapshot | Ctrl+Z | Undo.stepsOneActionAtATime (generic) | No ranges-specific undo or grouping test |
| Telemetry `advanced_ranges_used` | 8 | **no** | — | — | — | Telemetry has no usage booleans |
| Opt→Diagnostics mirrors it | 8 | no | — | — | — | Diagnostics flag mirror on visual (51eb9e2) may cover it; unverified |
| Mode read on change, nothing per block | 9 | yes | applyTo | — | — | |
| T: invariants / widening / narrowing / normalisation / legacy / marking / randomise | 10 | yes | — | — | Ranges.*, RangesUi.* | |
| T: undo restores a clamp | 10 | no | — | — | — | |
| T: snapshots do not carry mode | 10 | no | — | — | — | |

---

### tune-builder.md

**Summary:** 88 requirements. **yes 49 / partial 34 / no 5.** 15 of the partial items are complete on `tune-help`. On base, the model, parser, generators, harmony tools, bass, layers, templates, file format, player and MIDI export are strong and well tested. The **base TUNE tab is thin**. Export is MIDI-only. There is no chord popover and no drag or right-click on pills. The piano roll only draws, deletes and locks. There is no setlist editing, bass or layer UI, "Vary", or sing. Most of that is **in progress on `tune-help`**: the export dialog (audio+stems, MIDI profiles, notation, project), chord popover, pill editing, the full piano roll, the bass/layers strip, the setlist strip, Vary and hum capture. Built in the engine but **unreachable on every branch**: Suggest next chord, Reharmonize, Style transfer, "Follow mode" on modal shift, and melody range/density. MP3 export, variations (A/B arrangements), mod-matrix targets for tune parameters, live-snapshot section switching and looper capture are absent everywhere.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Control-only module | 0.1 | yes | TunePlayer → engine MIDI | — | theMelodySoundsWhileTheRhythmEngineStrums | |
| Save/reload .luthiertune, small, forward-compatible | 0.2 | yes | TuneFile | TUNE SAVE/LOAD | aHundredRandomTunesRoundTrip…; unknownFieldsAreKept… | |
| Every edit undoable; locked notes survive regenerate | 0.3 | yes | TuneSession::edit; regenerateMelody | TUNE | regenerateLeavesLockedNotesByteIdentical | |
| Works inside the plugin, no DAW | 0.4 | yes | Processor owns TuneSession/TunePlayer | TUNE | — | |
| Export audio/MIDI/notation from one score | 0.5 | partial | buildTuneMidiFile; buildTuneScore | EXPORT = MIDI only | — | Full dialog on tune-help |
| Meta: title, artist, tempo, time sig, key, mode, swing, feel | 1 | yes | TuneMeta | title/tempo/key/mode | — | Swing, feel_pct, time sig, artist have no header control |
| Sections with length, chords, rhythm, kit, melody, bass, layers | 1 | yes | TuneSection | TUNE | — | |
| Setlist of section refs with repeats | 1 | yes | TuneArrangement setlist | partial | editsKeepNamesUniqueAndTheSetlistInStep | Setlist strip on tune-help |
| Variations (A/B of the arrangement) | 1 | partial | TuneVariation struct | no | — | Model only |
| ChordCell fields (root … emphasis) | 1.1 | yes | ChordCell | popover on tune-help | — | |
| Hold the underspecified last cell | 1.1 | yes | resolveChordSpans | — | chordSpansRepeat…Hold… | |
| MelodyTrack string_hint, articulation default | 1.2 | partial | MelodyTrack.stringHint | no | — | No control |
| MelodyNote velocity/articulation/technique/locked | 1.2 | yes | MelodyNote | right-click menu on tune-help | — | Base: lock only |
| Absolute and relative pitch (root+N, chord_tone_N) | 1.2 | yes | MelodyPitch; resolveMelodyPitch | — | relativePitchesFollowTheChord… | |
| Genre kit dropdown with suggested tempo/feel/palette | 2.1 | partial | kitBox; GenreKit | TUNE rhythm row | — | Kit tempo/feel/palette on tune-help; spec names (Folk Strum, Country Boom-Chick, Rock Ballad) differ from shipped kits |
| Progression text field (bars, sections, `*2`) | 2.2 | yes | parseProgression; applyProgressionText | TUNE progression | shorthandParses…; typedShorthandReplaces… | |
| Melody Auto | 2.3 | yes | generateMelody | AUTO | autoMelodyIsByteIdentical… | |
| Melody Draw | 2.3 | yes | TunePianoRoll::addNote | DRAW | thePianoRollDrawsSnappedLockedNotes… | |
| Melody Record from MIDI in, quantise on release | 2.3 / 4.3 | yes | TuneSession::finishRecording | RECORD | recordingQuantisesATake… | |
| Improvise + Freeze | 2.3 / 4.4 | yes | generateImprovisedPass; freezeImprovisedPass | IMPROVISE/FREEZE | improviseVariesEachPassAndFreeze… | |
| Hit play; loops; edits take effect on the bar | 2.5 | yes | TunePlayer setTimeline | TUNE transport | anEditWhilePlayingWaitsForTheBarLine… | |
| Ctrl+S saves the tune, Ctrl+E exports (panel focus) | 2.6 | yes | TunePanel::keyPressed | shortcut | — | |
| Export dialog is one screen | 2.6 | partial | chooseAndExport → MIDI file chooser | TUNE EXPORT | — | TuneExportDialog on tune-help |
| TUNE tab between RHYTHM and LIVE | 3 | yes | AdvancedPanel.cpp:1042 | tab | — | |
| Header: name, Save, Export, Tempo, Key | 3.1 | yes | buildHeader | TUNE | theHeaderEditsTitleTempoKeyAndSaves… | |
| Section strip | 3.1 / 3.3 | yes | TuneSectionStrip | TUNE | theSectionStripsMenu… | |
| Rhythm strip: kit, feel, strum, on | 3.1 / 3.5 | yes | buildRhythm | TUNE | theRhythmStripSets… | |
| Piano-roll strip, scrollable, shrinks below 1280 | 3.1 | partial | TunePianoRoll | TUNE | — | No horizontal scroll on base |
| Transport: <<, play/pause, >>, loop, metronome, count-in | 3.1 / 3.6 | yes | buildTransport | TUNE | theTransportAndSpaceDriveThePlayer | |
| Pills coloured by diatonic function; neutral non-diatonic | 3.2 | yes | TuneChordPills::colourForDegree | TUNE | diatonicPaletteAndFunctionsFollowTheKey | |
| Click cell → popover (7 fields) | 3.2 | partial | — | — | — | TuneChordEditor on tune-help |
| Drag right edge = duration; drag to reorder | 3.2 | partial | — | — | — | tune-help (dragsResizeAndReorderChordPills) |
| Typing the field updates pills live | 3.2 | yes | progressionTextChanged | TUNE | theProgressionFieldWrites… | |
| Right-click: insert, duplicate, delete, copy/paste, suggest substitution | 3.2 | partial | suggestSubstitutions (model) | — | — | Menu on tune-help |
| Sections: click, rename, duplicate, delete, repeat, role tag | 3.3 | yes | TuneSectionStrip::buildMenu | right-click tab | theSectionStripsMenuRenames… | |
| Drag sections to reorder | 3.3 | partial | Tune model reorder | — | aThousandSectionReorders… | UI drag on tune-help |
| "Vary" creates a sibling | 3.3 | partial | — | — | — | TuneVary on tune-help |
| Setlist edited by dragging tabs into a timeline | 3.3 | partial | — | — | — | TuneSetlistStrip on tune-help |
| Roll: bar/beat lines, scale shading | 3.4 | yes | TunePianoRoll::paint | TUNE | rendersWithTheLookAndFeel | |
| Snap to key; C toggles chromatic | 3.4 | yes | setChromatic | key C | — | |
| Right-click note: velocity, articulation, technique, unlock, delete | 3.4 | partial | toggleLockAt; deleteNoteAt | lock/delete only | thePianoRoll… | Full menu on tune-help |
| Drag-box multi-select, shift-click | 3.4 | partial | — | — | — | tune-help (thePianoRollSelectsNudgesCopies…) |
| Cut/copy/paste; arrow nudge, Shift larger | 3.4 | partial | — | — | — | tune-help |
| Generators act on the current section, keep locked notes | 3.4 | yes | generateMelody(section) | — | regenerateLeavesLocked… | |
| Rhythm strip edits the section; Link rhythm to X | 3.5 | yes | rhythmLinkedTo; linkBase menu | right-click | theSectionStripsMenu…Links… | |
| Host transport wins when playing | 3.6 | yes | TunePlayer followingHost | — | theHostWinsWhenItPlays… | |
| Space play/pause; Shift+Space from section start | 3.6 | yes | TunePanel::keyPressed | TUNE focus | theTransportAndSpaceDrive… | |
| Auto: chord-tone start, step/leap mix, cadence rests, kit profile, range, density, seed | 4.1 | yes | generateAutoMelody | AUTO | autoMelodyStaysInRangeRests… | |
| melody_range / melody_density "wider on request" | 4.1 | partial | MelodyTrack.rangeLow/High, density | no | — | No control |
| "Regenerate" increments the seed | 4.1 | yes | regenerateMelody | AUTO again | — | |
| Quantise grids 1/4, 1/8, 1/8T, 1/16, 1/16T | 4.3 | yes | QuantiseGrid | Quantise combo | — | |
| "Follow chord changes" toggle | 4.3 | partial | MelodyTrack.followChords | no | recordQuantiseKeepsVelocityAndRefits… | Read by record but no toggle |
| Style transfer (bluegrass fiddle, jazz sax, …) | 4.5 | partial | applyMelodyStyle; TuneSection.style | **no** (any branch) | styleTransferChangesPhrasing… | Glue candidate |
| Diatonic palette | 5 | partial | makeDiatonicChord | — | diatonicPalette… | Kit palette button on tune-help |
| Suggest next chord | 5 | partial | suggestNextChords | **no** (any branch) | suggestNextChordOffersThree… | Glue candidate |
| Reharmonize one-shot with undo | 5 | partial | reharmonizeSection | **no** (any branch) | reharmonizeSubstitutesAndKeeps… | Glue candidate |
| Transpose; melodies follow | 5 | yes | transposeTune | Key combo | transposeMovesChords… | Only as a key change; no ±N control |
| Modal shift + "Follow mode" | 5 | partial | shiftMode(followMode) | Mode combo (follow=false hard-coded) | modalShiftMoves… | Follow toggle missing |
| Bass modes off/root/root-fifth/walking/genre/manual | 6 | yes | BassMode; generateBassLine | **no** on base | bassLinesFollowTheChords… | Bass row on tune-help |
| Bass through the bass engine, else separate MIDI | 6 | partial | TunePlayer setBassToEngine; MIDI out | — | theBassGoesToTheEngineOnlyForABass; midiOutCarriesTheTune… | No companion-instance routing |
| Layers pad/arpeggio/countermelody/percussion | 7 | yes | TuneLayer; TuneMidi | **no** on base | countermelodyStaysUnder… | Layers strip on tune-help |
| Layer on/off, volume, pan | 7 | partial | TuneLayer.volume/pan | no on base | — | tune-help |
| Playback through the realism engine | 8 | yes | engine direct MIDI | — | theMelodySounds… | |
| Per-section state boundary resets mod envelopes / rhythm phase | 8 | yes | stateBoundary → crossedStateBoundary | section menu | — | |
| Loop plays the setlist end to end | 8 | yes | TunePlayer loop | Loop | loopingWrapsOnTheSample… | |
| Audio export WAV/FLAC/**MP3**, 16/24/32f, SR, stems aux1-8, tail 0-5 s, Renders dir | 9.1 | partial | — | — | — | tune-help: WAV/AIFF/FLAC + stems + tail; MP3 missing everywhere |
| MIDI export: Luthier/Generic profile, track split, realism vs plain | 9.2 | partial | writeTuneMidiFile (split per part) | EXPORT | theMidiFileHasAMetaTrack… | Profiles and realism option on tune-help |
| Notation MusicXML/GP/ASCII, headings, chord symbols | 9.3 | partial | buildTuneScore | no on base | thePerformanceScoreKeepsSections… | Export UI on tune-help |
| Project export with bundled preset + guitar | 9.4 | partial | TuneFile save | SAVE | — | Bundle on tune-help |
| 10 templates | 10 | yes | Resources/Tunes/Templates; TuneTemplates | NEW menu | theTenTemplatesLoadInOrder… | |
| .luthiertune JSON schema per example | 11 | yes | TuneFile | — | templateFilesAreInCanonicalForm… | |
| Standalone: last tune loads on launch | 12 | partial | tune in plugin state | — | theSessionRoundTripsThroughPluginState | Relies on the JUCE standalone state; no explicit last-tune file |
| Standalone: MIDI in / audio in armed | 12 | no | — | — | — | |
| Sung/hummed capture, 0.6 confidence, Sing button | 13 | partial | — | — | — | tune-help (TuneHumCapture, PitchTracker) |
| Mod routes over tune timeline (feel, tempo drift) | 14 | no | — | — | — | |
| Live snapshots capture section state / footswitch sections | 14 | no | — | — | — | |
| Looper captures a Tune render | 14 | no | — | — | — | |
| Ctrl+T new tune (registry) | gui | partial | TunePanel Ctrl+T (focus) | — | — | Registry entry on tune-help |
| T: 100 shorthand strings; named errors | 15 | yes | — | — | shorthandParses…; malformedShorthandIsRefused… | |
| T: auto determinism ×1000 | 15 | yes | — | — | autoMelodyIsByteIdenticalForASeed… | |
| T: locked notes | 15 | yes | — | — | regenerateLeavesLocked… | |
| T: 1000 section reorders | 15 | yes | — | — | aThousandSectionReorders… | |
| T: 32-bar loop 60 s no drift | 15 | yes | — | — | aLoopedTuneRunsSixtySecondsWithoutDrift | |
| T: 100 random files round trip | 15 | yes | — | — | aHundredRandomTunesRoundTripByteIdentical | |
| T: offline render nulls live −80 dBFS | 15 | partial | — | — | — | theOfflineRenderNulls… on tune-help |
| T: Luthier-profile MIDI re-import same audio | 15 | partial | — | — | — | tune-help |
| T: sung capture 95% | 15 | partial | — | — | — | tune-help HumCapture |
| T: standalone reload | 15 | no | — | — | — | |

---

### Top gaps (group C)

Ranked by user impact.

1. **A body swap does not redraw the body** (guitar-illustration 0.3/0.5). No body part carries `illustration.body_style`, so the outline is locked to the guitar file. 14 of 34 drawn outlines (Flying-V, Firebird, Thunderbird, MM bass, headless, OM/000, cutaways, square-neck, multiscale) are unreachable. A visible "the picture lies" bug, and it contradicts ground rule 5.
2. **No finish or hardware-colour editor anywhere** (illustration 11, workshop 1). Colour, burst, sparkle, relic/aging and hardware colour exist in the renderer and the file format, but a user cannot change a guitar's colour.
3. **Four Workshop slots have no drawer category** (top, fretboard, tailpiece, pickguard). 21 shipped parts cannot be fitted, so pickguard colours, fretboard wood/inlays and tailpiece break angle cannot be changed from the UI.
4. **TUNE export is MIDI-only on base** (tune-builder 9). Audio, stems, notation and project bundle wait on `tune-help`. MP3 is missing everywhere.
5. **The TUNE editing surface is thin on base** (3.2–3.4): no chord popover, pill drag/right-click, note velocity/articulation/technique, multi-select, copy/paste, nudge, setlist editing, Vary, or bass/layer UI. All are on `tune-help`, so merging it is the fix.
6. **Harmony tools and style transfer are built but unreachable on every branch** (tune-builder 4.5, 5): Suggest next chord, Reharmonize, Style transfer, the Follow-mode toggle, melody range/density and "Follow chord changes".
7. **Chambering air resonance, mode gain and gloss damping are computed and discarded** (part-acoustics 2.1, 9). `airResonanceHz/Q`, `bodyGainDb` and `finishDampingDb` never reach `LuthierEngine`. Semi-hollow and hollow lose their defining air mode, and the chambering test passes on a struct field.
8. **The modulation range family is not implemented** (advanced-ranges 2.1/3.4). LFO rate is hard-clamped to 0.01–40 Hz in every mode, and envelope, sequencer and follower times are unclamped by family.
9. **Drawer to illustration drag-and-drop is absent** (13.2, drag ghost). Click-to-fit works. Pick, slide, capo and nut-slot drags, per-string overrides and the 250 ms crossfade are on `visual`, which is the fix.
10. **The `extended` family (7/8-string, fanned) is missing** from families, templates and the drawer.
11. **Several part fields change nothing** (part-acoustics): tuners (all), `nut.friction`, `fretboard.radius_mm`, neck wood/profile, `strings.core`, `coil_turns`, `wiring.switching`, `bridge.piezo` and `tension_kg`. Wood tanδ does not set body Q. Ground rule 3 of guitar-workshop says these should either act or not exist.
12. **Snapshots re-map instead of clamping after a lock** (advanced-ranges 5), and are untested.
13. **Telemetry `advanced_ranges_used` and its Diagnostics mirror are missing** (advanced-ranges 8).
14. **Live overlays are incomplete**: no buzz heatmap (layer 27) and no pickup pulse (layer 30). Overlays poll the engine rather than a display FIFO. The pick overlay is on `visual`.
15. **Accessibility of the illustration**: one component with a changing description instead of per-part accessible children. Arrows nudge rather than navigate, and there is no Enter→inspector.
16. **Missing-part banner has no jump-to-Workshop action**, and the string-count excess is not shown in the inspector (guitar-workshop 4.1, 5.1).
17. **The preset-browser thumbnail worker and cache are only on `visual`** (2.3/15).
18. **Tune ↔ rest-of-plugin integration is missing**: mod-matrix targets over the tune timeline, live snapshots per section, looper capture and standalone MIDI/audio arming (tune-builder 12, 14).

### Unspecified gaps noticed

- **Left-handed guitars.** No mirror option for the illustration, fretboard or Workshop. Buyers of a $200 guitar plugin commonly expect it.
- **Parts library management.** There is no rename, delete, duplicate, import or "reveal parts folder" from the Workshop drawer. Only Save As exists, and the folder is only rescanned on save.
- **Drag MIDI or audio from TUNE into the DAW.** Export goes through a file chooser only; drag-out clips are standard in this class of product.
- **Drums or a click track in TUNE.** The spec forbids drum sounds. A "hear a full arrangement" feature with no drums will surprise buyers, so this should at least be a documented decision. A MIDI drum-lane export for the DAW would fit.
- **Time-signature or tempo changes within a tune.** The model has one time signature and tempo per tune.
- **Chord diagrams or voicing preview in the TUNE progression.** None is specified.
- **Undo for the colour and finish of a guitar.** Moot until an editor exists.
- **Stale meta comments.** `PluginEditor.cpp:805` says "missing part" is unreachable, but it is posted at :965. `Accessibility.cpp:484` says Ctrl+G is absent, but it is registered at :539. `spec/GAPS.md:197` says "TUNE not built" and :301 says "No RANGES", both now false.

### Small glue candidates

The engine or model feature exists; it only needs a control, a registry row or a data field.

| Item | Exists at | Glue needed | Panel |
|---|---|---|---|
| Top / Fretboard / Tailpiece / Pickguard drawer categories | PartType::top/fretboard/tailpiece/pickguard; 21 parts in Resources/Parts | Add 4 rows to `kCategories` in WorkshopPanel.cpp:556 | WS drawer |
| Body style per body part | GuitarRenderer resolveStyle reads `illustration.body_style` | Add `"illustration": {"body_style": …}` to the 20 body parts. Add body parts for the 14 unused outlines (data only) | WS "Body" |
| Headstock per neck part | buildHeadstock reads `illustration.headstock` | Add to the 15 neck parts | WS "Neck" |
| Finish editor | WorkshopGuitar.finish (type, colourA/B, burstShape, gloss, aging), hardwareColour | Inspector rows when Body is selected, or a "Finish" drawer category writing through WorkshopBench (needs a bench setter) | WS |
| Suggest next chord | TuneHarmony:suggestNextChords | Button with 3 chips appending via session.edit | TUNE progression row |
| Reharmonize | TuneHarmony:reharmonizeSection | Button with undo via session.edit | TUNE progression row |
| Follow mode on modal shift | shiftMode(tune, mode, followMode) | Toggle next to modeBox; pass it instead of `false` (TunePanel.cpp:948) | TUNE header |
| Style transfer | TuneSection.style, applyMelodyStyle | ComboBox of MelodyStyle values | TUNE melody row |
| Melody range and density | MelodyTrack.rangeLow/High, density | Two small controls | TUNE melody row |
| Follow chord changes (record) | MelodyTrack.followChords | Toggle | TUNE melody row |
| Swing / feel_pct / time signature / artist | TuneMeta.swingPercent, feelPercent, timeSig, artist | Header controls | TUNE header |
| Extended family | 7-String Modern.luthierguitar exists | `getFamilyTemplate("extended")` → "Electric/7-String Modern.luthierguitar"; add "extended" to getGuitarFamilies | WS "Guitar" |
| Air resonance / body gain / gloss damping into the engine | DerivedAcoustics airResonanceHz/Q, bodyGainDb, finishDampingDb | Read in LuthierEngine::applyWorkshopGuitar (BodyEngine air mode + gain) | n/a (engine) |
| Winding pitch into noise | DerivedAcoustics.windingPitchPerMm | Pass to PlayingNoise/ScrapeEngine StringInfo in applyWorkshopGuitar | n/a |
| Registry rows for squeak/pick/slide params | ParamIDs squeak_probability, squeak_finger_moisture, squeak_finger_pressure, pick_bevel, pick_wear, slide_pressure | Add rows in PhysicalRange.cpp table (stock = declared range) | knobs gain marking/unlock |
| Modulation range family | RangeState family `modulation` | Clamp ModSources::setRateHz (and envelope, sequencer, follower setters) to 0.01–20 Hz stock or 0.001–200 advanced from RangeState | MOD tab + Opt→Ranges |
| Telemetry flag | RangeState::isAnythingAdvanced() | Add `advanced_ranges_used` to the usage payload; mirror on Options→Diagnostics | Opt→Diagnostics |
| String-count excess in inspector | mapSpec d.stringExcess / LoadReport.stringExcess | One inspector line | WS inspector |
| Missing-part banner action | Notification "missing-part" | Add an action that opens the WORKSHOP tab or overlay | banner |
| WORKSHOP tab padlock | RangesUi::RangeTabButton | Construct WORKSHOP with families {buzz} (done on visual) | column-4 tabs |

---

## Group D



Scope: routing-io, modulation-matrix, rhythm-engine, live-performance, controllers, practice-tools, tone-match, notation-export, character-wear, updates-telemetry, midi-export, input-routing.
Audited against HEAD 2ede79c (integration 6bda872 plus audit-only test commits; Source/ is otherwise identical). Re-verified 15:42 UTC: the only new remote commit, realism-c 8a8aafd, changes spec/docs and TuningStability and touches no group-D item. Helper branches checked: realism-a, realism-b, realism-c, techniques, model-gaps, tune-help, visual. Items that exist only on a helper branch are marked "in progress on <branch>".
"Engine-only" means the class or setter exists but nothing in the UI or the processor calls it.

### routing-io.md

The routing work is largely done. All four bus layouts are advertised and tested, Aux 1-7 plus a new Aux 8 noise bus are distributed, and 12 per-string buses work. The ROUTING tab exists in Column 4. Sidechain-to-amp, MIDI-out sources, macro-to-CC mapping, the latency readout and the preset state all work. Gaps: there is no "sidechain compressor" pedal (the effect is absent from PedalType). The layout "selector" is a read-only label. The latency test checks only the ordering between outputs, and does not measure an impulse through the chain. **Counts: yes 22 / partial 3 / no 1.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Layouts A-D advertised via BusesProperties | 0.1, 1 | yes | PluginProcessor::buildBusesProperties / isBusesLayoutSupported | n/a | Routing::everyLayoutRendersCleanly, PluginBuses::* | also Aux 8 noise bus appended last |
| Runs correctly at every layout | 0.2 | yes | processSlice + RoutingMatrix::distribute | n/a | Routing::everyLayoutRendersCleanly | |
| Aux 1-7 tap assignments (DI, pre-cab, mic1, mic2, room, wet, monitor) | 2 | yes | TapBuffers, AuxBus, RoutingMatrix::distribute/writeMonitorBus | n/a | Routing::diTapNullsAgainstReappliedAmp | Aux1 pre/post-circuit toggle `aux1_pre_circuit` in progress on model-gaps |
| Per-aux gain trim | 2 | yes | RoutingMatrix::setAuxGainDb (smoothed) | ROUTING tab AuxStrip | Routing::stateRoundTrips | |
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
| Aux strips: mute, solo, gain, meter | 8 | yes | AuxStrip | ROUTING | Routing::muteAndSoloResolveTogether | 8 strips (incl. Aux 8 noise) |
| MIDI-out enable + source checkboxes + CC table | 8 | yes | RoutingPanel midiOut* toggles | ROUTING (also mirrored on MIDI OUT) | MidiOutPanel::liveSwitches... | |
| Latency readout per active output | 8 | yes | RoutingPanel latencyLabel | ROUTING | - | |
| Preset: aux mute/solo/gain, MIDI-out, sc-to-amp stored; layout not stored | 9 | yes | RoutingMatrix::toVar/fromVar | n/a | Routing::stateRoundTrips, loadingClearsPreviousState | |
| Test: latency measured with an impulse, within 1 sample | 10 | partial | - | n/a | perOutputLatencyIsConsistent | checks ordering only; no measurement |

### modulation-matrix.md

The engine side is complete: 8 LFOs, 4 envelopes, 2 step sequencers, 2 followers, note/CC/14-bit/random sources, compiled lock-free routes, the 8-per-destination limit, and discrete destinations. All five listed tests exist. The UI is thinner than the engine. Missing from the UI: the per-route **offset** column, LFO phase and custom breakpoints, envelope stage curves and loop mode, and the step sequencer's per-step grid. There are no per-source colour tags. Snapshot morph position is not a modulation destination. The macros are fixed-function with fixed names. Dragging a source card onto a control is in progress on the visual branch. **Counts: yes 23 / partial 12 / no 3.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Control rate = block/32, min 128 | 0.1 | yes | ModMatrix::prepare | n/a | - | |
| Linear interpolation between ticks | 0.2 | yes | ModMatrix offsets ramp | n/a | Modulation::routeModulatesItsDestination | |
| Additive over base, clamped | 0.3 | yes | ModMatrix::apply | n/a | routeModulatesItsDestination | model-gaps removes a +-4 clamp bug (in progress on model-gaps) |
| Up to 8 sources per destination | 0.4 | yes | kMaxRoutesPerDestination | Modulate menu shows limit | destinationAcceptsEightSourcesAndNoMore | |
| Per-route depth and offset +-100% | 0.4, 3 | partial | ModRoute.depth/offset | depth typed in table; **no offset column** | presetRoundTripIsExact | offset reachable only through preset/state |
| Sources reset on preset load / snapshot / transport start | 0.5 | partial | ModMatrix::reset, resetEnvelopes; LFO Retrigger::onTransportStart | n/a | - | no explicit reset on snapshot recall |
| Free-running LFO option | 0.5 | yes | ModLfo::Retrigger::freeRun | MOD card retrigger box | - | |
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
| Enabled toggle per route | 3 | yes | ModRoute.enabled | table "On" | - | |
| MOD tab in Column 4 | 5 | yes | ModMatrixPanel | Col 4 MOD | GuiReachability | stacked, not 1/3-2/3 (documented) |
| Routing table columns incl offset + delete | 5 | partial | ModRouteTable | MOD | - | no Offset column |
| Add-route button | 5 | yes | ModMatrixPanel addButton | MOD | - | |
| Drag source card onto control creates route | 5 | partial | - | no | - | in progress on visual (ModSourceCard::mouseDrag + AttachedKnob DragAndDropTarget) |
| Right-click "Modulate" submenu | 5 | yes | Widgets.cpp kModulateMenuBase | any knob | Editor::rightClickOffersModulationAndBuildsTheRoute | |
| Depth arc around modulated controls | 5, 7 | yes | Widgets paint "modulation arc" | all AttachedKnobs | - | single colour (Palette::secondary) |
| Per-source user colour tags, routes/arcs inherit | 5 | no | - | no | - | |
| Snapshot "includes modulation" vs preset-level flag | 6 | no | captureSnapshot always stores modMatrix | no | - | always included |
| Import validates destination IDs, warns | 6 | yes | ModMatrix::fromVar unknown list | - | unknownDestinationsAreReportedNotFatal | no user-visible banner verified |
| Tests: source accuracy, 1000-route stress, round trip, discrete, determinism | 8 | yes | - | n/a | Modulation::* (17 tests) | |

### rhythm-engine.md

The rhythm engine is complete in function. It has the chord detector (84 templates), the voicer with 9 styles, strum and fingerpick scheduling, humanisation, bypass, free-run, all 29 GenreKits and the 7 fingerpick patterns. The RHYTHM tab and the Easy strip exist, and all six listed tests are present. Deviations: the factory patterns and kits are built in code, not stored as `Resources/Rhythm` / `Resources/Genres` JSON. The `rhythm_engine.enabled/state` values live in a preset blob, not as host parameters, so the rhythm on/off cannot be automated. **Counts: yes 25 / partial 3 / no 0.**

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
| 29 GenreKits (listed) | 7 | yes | GenreKitLibrary (makeKit x29 + 4 bass kits on model-gaps) | genre box | factoryKitsAreWellFormed | |
| Kits as JSON under Resources/Genres/ | 7 | partial | in code; user dir scanned | - | kitsRoundTripThroughJson | |
| Kit rig as soft reference | 7 | yes | rigHintLabel | RHYTHM | applyingAKitDoesNotLoadItsRig | |
| UI: genre + dice, voicing, capo +/- | 8 | yes | RhythmPanel | Col 4 RHYTHM | GenreKits::capoRemovesFrets... | |
| Strum editor 16/32 grid, right-click dynamic/mask/delete | 8 | yes | StrumGrid | RHYTHM | - | |
| Fingerpick editor 5 rows | 8 | yes | FingerpickGrid | RHYTHM | - | |
| Feel: swing, timing, velocity, miss, ghost | 8 | yes | RhythmPanel sliders | RHYTHM | - | |
| Pattern browser tag filter/load/save/export | 8 | yes | patternList, tagFilterBox | RHYTHM | - | |
| Live indicators: chord, fretboard dots, next-strum light | 8 | yes | RhythmIndicators | RHYTHM | - | |
| Easy strip: kit, feel knob, on/off, Mono hint | 8 | yes | EasyPanel::buildRhythmStrip | Easy | EasyLayout tests | |
| Preset params `rhythm_engine.enabled` / `.state` | 9 | partial | "rhythm" blob in state | - | - | not host-automatable parameters |

### live-performance.md

The engines are complete and every listed test exists: snapshot bank, crossfade, morph with curves and exclusions, setlist with preload, tap tempo, kill switch and monitor mix. The keyboard map is done ([ ] in Live Mode, 1-9 and Shift+digit, PageUp/PageDown, \, P, T). The main gaps:
- **Expression calibration does not work.** The heel/toe wizard has UI, but `ExpressionCalibrationSet::observe()` is never fed incoming CCs, and `map()` is never applied to incoming CCs.
- Live actions (next/previous snapshot, snapshot by CC, kill, tap, setlist navigation) cannot be assigned to a CC. MIDI Learn is parameter-only.
- Snapshot morph cannot be driven by a pedal CC, LFO or mod wheel, because it is not a parameter.
- Morph exclusions and Bezier points have no UI.
- Monitor pan and EQ have no UI.
- There is no per-preset or global MIDI-learn scope.

**Counts: yes 25 / partial 9 / no 3.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Every live control reachable without a mouse (key, CC, PC) | 0.1 | partial | keyPressed; handleLiveMidi | keys yes; CC no | - | only PC/CC0 hard-wired; no CC-assign for live actions |
| Glitch-free snapshot switching | 0.2, 1 | yes | SnapshotBank crossfade | n/a | LiveSnapshots::thousandRecallsNeverJumpAParameter | |
| Kill within one block, 3 ms fades | 0.4, 6 | yes | KillSwitch | LiveStrip KILL pill, key \ | LiveKillSwitch::* (3) | key toggles (not momentary), documented |
| No modal dialogs on live surface | 0.5 | yes | LiveStrip | - | - | rename AlertWindow is on the LIVE setup tab only |
| 128 snapshots: params, mod, bypasses, rhythm, 32-char label, 16 colours | 1 | yes | Snapshots.h kMaxSnapshots/kMaxLabelLength/kNumColourTags; captureSnapshot | LIVE tab grid | captureAndRecallRoundTrip, bankRoundTripsThroughJson | |
| Crossfade `snapshot_xfade_ms` 0-500, default 30 | 1 | yes | SnapshotBank::setCrossfadeMs | LIVE crossfade slider | recallCrossfadesContinuousAndStepsDiscrete | |
| Bypass at midpoint; tails preserved; coupling on worker | 1 | partial | SnapshotBank | n/a | - | discrete switching tested; tail double-buffer / coupling worker not verified |
| Snapshots stored in preset `snapshots` | 1, 11 | yes | processor state "snapshots" | n/a | bankRoundTripsThroughJson | |
| PC = snapshot index; CC0 selects preset | 2 | yes | handleLiveMidi | n/a | programChangeMapsAcrossAllOneTwentyEight | |
| CC assign: next/previous/by-value snapshot | 2 | no | - | no | - | MidiLearn maps CC -> parameter only |
| `[` `]` step, 1-9, Shift+digit | 2 | yes | PluginEditor::keyPressed | keys | - | [ ] step snapshots only in Live Mode |
| On-screen snapshot strip (8 + prev/next) | 2, 10 | yes | SnapshotStrip | LiveStrip | Editor::theLiveTab... | |
| Foot controller via MIDI Learn | 2 | partial | MidiLearnManager | right-click Learn | - | params only |
| Morph A/B, knob, enable | 3 | yes | SnapshotBank morph | LiveStrip MORPH/A/B/knob | morphFollowsItsCurve | |
| Morph curve linear/S/exp/Bezier | 3 | partial | MorphCurve; setBezierControlPoints | LIVE morphCurveBox | morphFollowsItsCurve | Bezier points have no UI |
| Discrete params switch at 0.5; per-param exclusion | 3 | partial | SnapshotBank::setParameterExcludedFromMorph | no exclusion UI | morphHonoursExclusionsAndDiscreteSwitching | exclusions engine-only |
| Morph driven by pedal CC / LFO / mod wheel / sidechain env | 3 | no | - | no | - | snapshot morph is not a parameter or mod destination |
| Setlist `.luthierset` in ~/Documents/Luthier/Setlists | 4 | yes | Setlist | LIVE setlist list, triptych | roundTripsThroughJson | |
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

**Counts: yes 8 / partial 9 / no 4.**

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
| Profile MPE flag / member bend range take effect | 1, 4 | no | ControllerProfileLibrary::apply | - | applyingAProfile... (interpreter alone) | **overwritten every block by ParameterBridge::applyToEngine (Parameters.cpp:1226-1227)** |
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

**Counts: yes 33 / partial 16 / no 6.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Isolated panel, zero CPU when closed | 0.1 | yes | processSlice `practicePanelOpen` gate | drawer | - | |
| Click to monitor by default, optional main | 0.2 | yes | setClickToMain | METRO toggle | - | |
| Looper re-renders stored MIDI through new tone | 0.3 | partial | Looper::captureMidi | - | - | MIDI captured; the re-render is never done |
| Backing tracks stream from disk | 0.4 | yes | BackingTrackPlayer (BufferingAudioSource) | TRACK | - | |
| Metronome time sigs + custom | 1 | yes | Metronome::setTimeSignature | METRO | settingsRoundTrip | |
| Tempo 20-300, follows tap + host tempo | 1 | partial | Metronome::setTempo | METRO slider | - | never follows tap or host tempo |
| Accent map, 3 levels (+silent) | 1 | yes | setBeatAccent | METRO beat buttons | accentPatternIsObeyed | |
| Subdivisions with own gain | 1 | yes | setSubdivision/setSubdivisionLevelDb | METRO | - | |
| 6 click sounds | 1 | yes | ClickSound | METRO sound box | everyClickSoundIsAudibleAndFinite | |
| 16 click WAVs in Resources/Practice/Clicks | 1, 10 | no | synthesized | - | - | folder does not exist |
| Silent bars every N | 1 | yes | setSilentBarPeriod | METRO | silentBarsMuteTheClick... | |
| Progressive tempo A->B over N bars | 1 | yes | startProgressiveTempo | METRO | progressiveTempoRampsAndStops | |
| 4-dot visual | 1 | yes | beat indicator paint | METRO | - | |
| Loop length 1-240 s, bar-quantized | 2 | yes | Looper | LOOP | loopLengthQuantisesToBars | |
| 8 layers, MIDI + audio | 2 | yes | Looper kMaxLayers | LOOP | recordsClosesAndOverdubs | |
| Undo/redo per layer | 2 | yes | LoopLayer undo (depth 1) | LOOP | layerUndoAndRedo | |
| Reverse / half-speed per layer | 2 | yes | LoopLayer | LOOP | reverseAndHalfSpeedDoNotAlterTheRecording | |
| Overdub / replace / play-once modes | 2 | yes | LayerMode | LOOP mode box | - | |
| Per-layer volume, pan | 2 | yes | LoopLayer | LOOP | mutedLayersAreSilent | |
| Per-layer low-cut / high-cut | 2 | partial | LoopLayer::setLowCutHz/setHighCutHz | no controls | - | engine-only |
| Export mixdown / stems | 2 | yes | exportMixdown/exportStems | LOOP | - | |
| .luthierloop save/load | 2 | yes | Looper::save/load | LOOP | - | |
| Temp-folder audio snapshots moved on save | 2 | partial | - | - | - | not verified |
| Formats WAV/AIFF/FLAC/MP3 (dr_mp3) | 3 | partial | registerBasicFormats | TRACK Load | - | MP3 not enabled (no JUCE_USE_MP3AUDIOFORMAT, no dr_mp3). The TRACK file chooser (PracticePanel.cpp:668) still offers *.mp3, so choosing one fails to load |
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
| Save last take WAV + MIDI | 8 | yes | saveLastTake | SESSION Save | - | recorder MIDI capture in progress on model-gaps |
| Temp dir, 24 h cleanup, off by default, enabled in Options | 8 | yes | cleanUpOldTempFiles; OptionsPages recorderToggle | Options | disabledByDefault | |
| Drawer 32-360 px resizable, collapsed strip readouts | 9 | yes | PracticePanel | drawer | PracticeDrawer::* | |
| Tabs METRO/LOOP/TRACK/SCALE/EAR/TAB/PROG/SESSION | 9 | yes | PracticePanel tabs | drawer | PracticeRoutine::toolsAreInTheDrawersTabOrder | |
| Drawer master volume, tap, panic | 9 | yes | practiceLevel, tapButton, panicButton | drawer | - | |
| 11.2 Progress (90 days, per tool, accuracy, tempo, streak, CSV, clear) | 11 | yes | PracticeStats; PracticeSetupPanel | Col 4 PRACTICE | progressReportsDaysStreaksAccuracyAndTempo, clearHistory... | |
| 11.2 Routines (3 factory, drive drawer, saved JSON) | 11 | yes | PracticeRoutineLibrary/Runner | PRACTICE | theThreeFactoryRoutines..., aRoutineDrivesTheDrawer... | |
| 11.2 Defaults per tool | 11 | yes | PracticeDefaults | PRACTICE | defaultsSurviveAReopen... | looper default length / trainer range in progress on model-gaps |
| 11.2 Library (loops, sessions, backing folder, recent tabs) | 11 | yes | PracticeLibrary | PRACTICE | libraryListsLoadsAndDeletes... | |
| 11.2 Session setup (ring length, audio/MIDI, auto-save, size warning) | 11 | partial | SessionRecorderSetup | PRACTICE | sessionSetupStatesTheRingSize... | audio/MIDI switches + auto-save in progress on model-gaps |
| 11.3 No transport on tab / 11.4 empty states | 11 | yes | - | - | theTabExposesNoTransport, emptyStatesReadAsTheSpecWritesThem | |
| Tests: metronome accuracy; session no-alloc | 12 | yes | - | - | interClickInterval..., ringBufferNeverGrows | |
| Tests: looper -80 dB null; 60-min stream memory; 50 GP files | 12 | no | - | - | - | missing |

### tone-match.md

The IR slots (body plus 2 cab), windowed-sinc resampling, cab-match deconvolution with three test signals, EQ-match fitting, the capture utility, the library folders, sidecar metadata, relative preset paths and the missing-IR banner are all implemented. The TONE MATCH tab exists. Gaps:
- **The EQ-match result is loaded into cab IR slot 2**, replacing mic 2, instead of being inserted as a filter at pre-amp / post-amp / post-master.
- The match analysis runs on the message thread (spec: worker thread).
- The capture sources are main and sidechain only.
- IR start/end trim has no UI.
- There is no recent-IRs list and no library search.
- Three of the five listed tests are missing.

**Counts: yes 16 / partial 7 / no 6.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| IR loaded on message thread, lock-free swap | 0.1 | yes | IrSlot::load | - | anEmptySlotLeavesTheAudioAlone | |
| Windowed-sinc resample, >=512 taps | 0.2 | yes | IrSlot::resample | - | - | no resampling-accuracy test |
| Truncate at max_ir_seconds (4 s) | 0.3 | yes | IrSlot::setMaxSeconds | - | - | |
| No extra latency for user IRs | 0.4 | yes | - | - | - | not tested |
| Match analysis on worker thread | 0.5 | no | MatchWizard step handler (message thread) | - | - | EqMatch::fit / deconvolve run inline |
| Body IR slot + 2 cab slots | 1 | yes | processor getBodyIrSlot/getCabIrSlot | TONE MATCH slot cards | irSlotSettingsRoundTrip | |
| Slot: file, channel/auto-sum, gain +-24, predelay, reverse, mix | 1 | yes | IrSlotEditor | TONE MATCH | irSlotSettingsRoundTrip | |
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
| Capture 100 ms-60 s, WAV 32f to Captures/, autotrim | 4 | yes | Capture | capture pane | captureAutoTrimsSilence | |
| IR library folder convention | 5 | yes | IrLibraryPaths | - | - | |
| Sidecar .json, filename fallback | 5 | yes | IrMetadata | - | metadataRoundTripsAndFallsBackToTheFilename | |
| TONE MATCH tab: slots, wizards, capture, library with tag filter + search | 6 | partial | ToneMatchPanel | Col 4 TONE MATCH | - | tag filter yes; search no |
| Preset IR paths relative/absolute; missing-IR banner | 7 | yes | IrLibraryPaths; PluginEditor "ir-missing" | header banner | presetPathsAreRelativeUnderTheLibraryRoot | |
| Test: 100 IRs resampled within 0.5 dB | 8 | no | - | - | - | |
| Test: EQ fits shelf/bell/notch within 1 dB | 8 | yes | - | - | eqMatchFitsKnownCurves | |
| Test: 60 s capture nulls vs offline render | 8 | no | - | - | - | |
| Test: sample-rate change during IR playback | 8 | no | - | - | - | |

### notation-export.md

`PerformanceScore`, `PerformanceCapture` (8192-slot lock-free ring, drop-oldest, off/rolling/armed states, quarter-note or seconds timing) and the MusicXML / GP8 / ASCII / MIDI writers are implemented. The NOTATION tab has capture state, live tab, export and preview; the TAB drawer tab and File-menu export also exist. Much of the rest is in progress on model-gaps:
- chord symbols from the detector
- technique, BASS_TECH and slide events fed into the capture
- Mono-mode offline chord extraction
- current-bar fretboard dots
- marked-region range
- export on a worker thread

Still missing: a Guitar Pro importer (so no GP round trip), the 100-progression chord test, and a MuseScore fixture. **Counts: yes 17 / partial 10 / no 2.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Export offline on worker thread | 0.1 | partial | NotationExporter (message thread) | - | - | NotationTakeExport::writeAsync in progress on model-gaps |
| Guitar-aware string/fret | 0.2 | yes | PerformanceScore, ScoreNote | - | Notation::stringAndFretAreNotDerivedFromPitch | |
| Technique metadata preserved | 0.3 | partial | ScoreNote techniques; capture flags | - | Capture::techniquesBendsChordsAndMetersReachTheScore | engine-side technique hook in progress on model-gaps |
| Round-trip per format | 0.4 | partial | NotationImporter | - | musicXmlRoundTrips, asciiTabRoundTrips | GP cannot be re-imported |
| PerformanceScore data model | 1 | yes | Notation/PerformanceScore | - | captureBuildsMeasures | |
| Capture last N minutes (default 10) | 1 | yes | PerformanceCapture rolling | NOTATION rollingMinutes | Capture::rollingKeepsTheLastMinutes | |
| MusicXML 4.0 technical elements | 2.1 | yes | NotationExporter | format box | musicXmlIsWellFormedAndGuitarAware | |
| MusicXML chord symbols (Poly) | 2.1, 4 | partial | - | - | - | in progress on model-gaps (captureBlockState chord track) |
| Grace notes / multi-voice | 2.1 | partial | - | - | - | not verified |
| Whammy as text directions | 2.1 | yes | formatsDeclareTheirLosses | - | formatsDeclareTheirLosses | |
| Guitar Pro 8 .gp bundle, techniques, chord diagrams | 2.2 | yes | NotationExporter GPIF zip | format box + option | guitarProBundleIsAValidZip | fidelity checked only for validity |
| ASCII tab: 6 lines, ruler, symbols, width 80, headings | 2.3 | yes | renderAsciiTab | lineWidth slider | asciiTabColumnsAlign, asciiTabUsesTheSpecifiedSymbols | |
| MIDI per-string tracks, RPN hints, bend, CC68 | 2.4 | yes | NotationExporter MIDI | format box | midiExportIsPerString | |
| Live TAB view in practice panel, last N beats | 3 | yes | TabReaderTab / NotationPanel tabView | drawer TAB, Col 4 NOTATION | Notation::liveTabWindowRendersASlice, Capture::theLiveTabShowsWhatWasPlayed | |
| Current bar as fretboard tab dots | 3 | partial | - | - | - | in progress on model-gaps (FretboardComponent::refreshTabDots) |
| Controls: show/hide, scroll speed, bars 1-8, density | 3 | yes | NotationPanel speedBox/barsBox/densityBox | NOTATION | NotationTab::stateButtonsLiveTabAndPreview | |
| Poly chord symbols at change points | 4 | partial | - | - | - | in progress on model-gaps |
| Mono offline chord extraction | 4 | partial | - | - | - | in progress on model-gaps (extractChordsWhenMissing) |
| File menu Export -> Notation | 5 | yes | HeaderBar File menu | header | - | |
| Dialog: format, range, options, destination, preview | 5 | yes | NotationPanel | NOTATION | NotationTab::exportsEveryFormat | "marked region" range in progress on model-gaps |
| "Export to Notation" beside Save last take | 5 | yes | HeaderBar (notation-export 5 comment) | header | - | |
| Capture records voiced notes, not MIDI | 6.1 | yes | PerformanceCapture | - | Capture::recordsVoicedNotesNotMidi | |
| Chord / tempo / BASS_TECH / slide tracks | 6.1 | partial | tempo/TS via CaptureClock | - | - | chord, BASS_TECH and slide in progress on model-gaps |
| 8192 lock-free ring, 10 Hz drain, drop-oldest counter | 6.2 | yes | CaptureRing | overflow count shown | ringOverflowDropsTheOldestAndCountsExactly, capturingTenThousandNotesDoesNotAllocate | allocation counter compiled in on model-gaps |
| States off/rolling (default)/armed | 6.3 | yes | PerformanceCapture | NOTATION state buttons | offWritesNothing..., armedStartsCleanFromTheNextNote | |
| Timing: quarter notes with transport, seconds free | 6.4 | yes | CaptureClock | quantise box | transportTimingIsInQuarterNotes, freePlayIsInSeconds... | |
| Test: MusicXML via MuseScore fixture | 7 | partial | - | - | musicXmlRoundTrips (own importer) | |
| Test: Guitar Pro round trip via fixture parser | 7 | no | - | - | - | zip validity only |
| Test: chord extraction on 100 progressions >95 % | 7 | no | - | - | - | |

### character-wear.md

The CharacterEngine is complete and deterministic, and the CHARACTER tab has every listed control. But **most of its outputs are never consumed by the DSP.** Only these reach the audio: dead spots, fret-wear sustain and detune, tuner drift, nut damping and nut material.
Computed but unused:
- fret-wear buzz multiplier
- pot taper, cap drift, jack drop
- piezo saddle balance, per-string pickup balance, saddle height
- body-age Q, air and HF multipliers
- temperature and humidity multipliers

So the pot-linearity, cap-drift, dodgy-jack, body-age, temperature and humidity controls have **no audible effect**. Temperature and humidity are being replaced by EnvironmentModel on realism-a. **Counts: yes 17 / partial 13 / no 0.**

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
| Environmental envelope over minutes | 4 | partial | sessionSeconds | readout | - | EnvironmentModel in progress on realism-a |
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
| Temperature -> tension/tuning | 9 | partial | getTemperatureOffsetCents | temperature box | temperatureProducesTheExpectedOffset | unused on integration; in progress on realism-a |
| Humidity -> body Q / compliance | 9 | partial | getHumidityQMultiplier | humidity box | humidityMovesTheBodyTheRightWay | unused; in progress on realism-a |
| Session time drift + Retune | 9 | yes | retune | Retune | retuneResetsTheDrift | |
| CHARACTER tab contents incl All fresh / All old | 10 | yes | CharacterPanel | Col 4 CHARACTER | allFreshAndAllOldPresets | |
| Stacks with humanize | 11 | yes | - | - | - | |
| Test: temperature step measured in the tuning engine output | 12 | partial | - | - | temperatureProducesTheExpectedOffset | checks the character value, not the tuning output |
| Test: zero-character bitwise identical | 12 | yes | - | - | zeroCharacterIsExactlyNeutral | |

### updates-telemetry.md

Everything is off by default. The update check (manifest, semver, 24 h throttle, worker thread, banner, beta toggle), the local outbound log, the policy file with its "Managed by policy" banner, and the privacy dashboard with its paranoia button are done and tested. Gaps:
- **Usage and diagnostics telemetry never record anything.** `Telemetry::record` and `sendPending` have no callers outside tests.
- **No crash handler writes minidumps.** Existing dumps are detected, but nothing ever creates one.
- The License class exists but has no activation, offline-challenge or deactivate UI.
- There are no delta updates.

**Counts: yes 13 / partial 5 / no 3.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| All opt-in, default off | 0.1 | yes | Telemetry | Options UPDATES/PRIVACY | everythingIsOffByDefault | |
| No PII | 0.2 | yes | - | - | - | |
| Every outbound call logged locally | 0.3 | yes | Transport logging | - | everyOutboundCallIsLogged | |
| Update check on worker thread | 0.4 | yes | PluginEditor thread launch | - | - | |
| Never auto-install | 0.5 | yes | - | - | - | |
| Opt-in toggle, on-load throttled 24 h, Check now | 1 | yes | UpdatesPage; PluginEditor | Options -> UPDATES | updateCheckIsThrottled | |
| Manifest JSON, semver compare | 1 | yes | checkForUpdate | - | updateCheckReadsTheManifest, versionComparison | |
| Non-modal banner with What's new / Download | 1 | yes | NotificationCentre | header | - | |
| Beta channel toggle | 1 | yes | betaToggle | UPDATES | - | |
| Delta updates (bsdiff/rsync) | 2 | no | - | - | - | acknowledged in TelemetryTests header |
| Usage telemetry (panels, presets, CPU, daily send) | 3A | partial | Telemetry::record/sendPending | toggle | recordsAreLoggedLocallyAndSentWhenAllowed | **no call sites; nothing is ever recorded** |
| Diagnostics telemetry (errors, host info) | 3B | partial | same | toggle | same | no call sites |
| Log at Diagnostics/telemetry-yyyymm.log | 3 | yes | Telemetry | PRIVACY | recordsAreLogged... | |
| Crash minidump written on crash | 4 | no | - | - | - | no crash handler installed |
| Next-launch prompt with diff viewer; 3-attempt upload | 4 | partial | hasPendingCrashReport; banner "Review" | PRIVACY | crashReportDescribesItself | no dumps are ever produced |
| License activate / revalidate 30 d / 14 d grace | 5 | partial | License | no activation UI | licenceActivationAndGrace, revalidationCountdown... | grace banner + Help readout only |
| Offline challenge/response, one-click deactivate | 5 | no | License::getOfflineChallenge, deactivate | no UI | - | engine-only |
| Privacy dashboard: explanations, toggles, view log, clear, endpoints, paranoia | 6 | yes | PrivacyPage | Options -> PRIVACY | turnEverythingOffDeletesAndDisables | |
| Enterprise policy file + "Managed by policy" | 7 | yes | Policy::getPolicyFile | banner | policyOverridesTheUser | |
| Tests: no-network, defaults, policy | 8 | yes | - | - | noNetworkIsSilentRatherThanAnError etc. | |
| Tests: crash dump privacy grep; delta SHA | 8 | partial | - | - | crashReportDescribesItself | delta test absent |

### midi-export.md

The codec is thorough: 18 event classes, text and SysEx redundancy, Generic and Luthier profiles, PPQ 96-3840, track splits, `.midprofile`, strip-identifiers, and a corrupt-file sweep. Every listed test exists, including the null tests against all factory presets. Gaps:
- Capture-based export writes NOTE and technique data, plus BASS_TECH/SLIDE_BAR (on model-gaps). It never writes STRUM, PICK, SQUEAK, BUZZ, CLANK, VIBRATO, WHAMMY or RASGUEADO: those only go out as live SysEx.
- The import UI (File -> Import, drop a `.mid`, choose a target) and the session-recorder drag-out are in progress on model-gaps.
- The tune export dialog is in progress on tune-help.
- The default MIDI options live on the MIDI OUT tab, not in Options -> MIDI.
- Installers do not register `.midprofile`.

**Counts: yes 19 / partial 7 / no 1.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Luthier profile lossless for every event class | intro, 2.1 | partial | LuthierEvents codec (18 classes) | - | everyEventClassRoundTripsWithEveryField | the capture never produces STRUM/PICK/SQUEAK/BUZZ/CLANK/VIBRATO/WHAMMY/RASGUEADO |
| Generic profile with LUTHIER: text metas | intro, 3 | yes | MidiProfiles | profile toggle | genericProfileIsPlainMidi | |
| Export on worker thread | 0.1 | partial | - | - | - | worker writer in progress on model-gaps (notation); MIDI OUT export on message thread |
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
| Export from tune-builder dialog / File -> Export -> MIDI | 4.1 | partial | - | - | - | TuneExportDialog in progress on tune-help |
| Fields: profile, range, split, realism, PPQ, preview | 4.1 | yes | MidiOutPanel | MIDI OUT | previewDescribesTheOpeningBar | marked-region/current-section ranges in progress on model-gaps |
| Drag-out from session recorder Save (Alt = Generic) | 4.2 | partial | MidiOutPanel drag helper | - | dragOutWritesAValidMidiFile | SESSION SaveButton drag in progress on model-gaps |
| Import: File -> Import -> MIDI, drop .mid | 5 | no | - | - | - | in progress on model-gaps (editor FileDragAndDropTarget, header menu) |
| Auto-detect profile by header | 5 | yes | MidiProfiles | - | headerStrippedFileLoadsAsGenericWithoutWarning | |
| Import targets: session / tune / looper | 5 | partial | - | - | - | in progress on model-gaps (MidiImportTargets) |
| Live MIDI-out 7 sources incl Character/noise SysEx, workshop | 6 | yes | MidiOutRouter; sysExOut | ROUTING + MIDI OUT | liveEventsAndWorkshopChangesGoOutAsLuthierSysEx | CHARACTER seed/environment live events in progress on model-gaps |
| .midprofile save/load | 7 | yes | MidiExportDefaults | SAVE/LOAD PROFILE | midprofileSavesAndLoads | |
| Installer registers .midprofile | 7 | partial | - | - | - | not found in Tools/scripts |
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
- There is **no root file-drop handler**: only the IR slot accepts drops, and `.mid` handling is in progress on model-gaps.
- There is no Diagnostics fixture injection and no `Tests/InputRouting/` suite.

Keyboard handling works: shortcuts go through the rebind registry with clash detection, and JUCE gives text fields focus.

**Counts: yes 8 / partial 9 / no 8.**

| Requirement | Section | Implemented? | Where | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| MIDI consumer order (export -> learn -> profile -> interpreter -> technique -> rhythm -> tune -> practice -> strings) | 1 | partial | PluginProcessor::processSlice | n/a | - | tune capture before learn; profile applied inside the interpreter; practice not fed |
| MIDI-out pass-through first | 1 | yes | midiOutRouter.captureInput | n/a | Routing::midiOutPassThroughIsSampleExact | |
| MidiLearn consumes the first non-note event | 1.1 | partial | MidiLearnManager::processMidi (const buffer) | right-click / header arm | - | the learned CC is not consumed |
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
| Drag consumers (parts, pedals, snapshots, presets->setlist, mod sources) | 2.2 | partial | - | - | - | mod-source drag in progress on visual |
| Root file-drop by extension (10 types) + unknown-type banner + batch | 4 | no | only IrSlotEditor accepts drops | - | - | .mid drop in progress on model-gaps |
| Text field consumes keys; Escape passes | 3, 3.1 | yes | JUCE focus + PluginEditor::keyPressed | - | - | |
| Global shortcut table, rebinding in Options | 3 | yes | AccessibilitySettings | Options -> ACCESSIBILITY | Accessibility tests | |
| Rebind conflict detection | 3.2 | yes | OptionsPages clash hint | Options | - | inline hint rather than a modal |
| IME honoured | 3.3 | partial | JUCE TextEditor | - | - | untested |
| Host transport -> rhythm, tune, tap, metronome, recorder | 5 | partial | processSlice playhead | - | RhythmPatterns::silentWhenStopped... | metronome is not transport-synced |
| Sidechain consumers (followers, compressor, sc-to-amp, EQ/cab match) | 6 | partial | - | - | - | no sidechain compressor |
| Standalone audio input: practice trainer input | 7 | no | - | - | - | |
| Options -> Diagnostics "Inject fixture MIDI/audio" | 8 | no | - | - | - | |
| Tests/InputRouting/ suite + 60 s integration | 9 | no | - | - | - | missing |

### Top gaps (group D)

1. **Backing-track pitch shift and tempo shift are dead controls.** `BackingTrackPlayer::setPitchShiftSemitones` and `setTempoRatio` only store the values, and the renderer never reads them. The TRACK sliders do nothing, and practice routines that set a tempo ratio have no effect.
2. **Expression-pedal calibration is non-functional.** The wizard is never fed CCs (`observe()` has no caller), and calibrated values are never applied to incoming CCs (`map()` has no caller).
3. **Controller profile MPE and bend settings are overwritten every block.** `ParameterBridge::applyToEngine` (Parameters.cpp:1226-1227) resets `setMpeEnabled` and `setPitchBendRange` from the `mpe_enabled`/`bend_range` params. MPE profiles (Seaboard, Linn, Osmose) lose MPE mode and their 48-semitone range.
4. **Controller latency compensation is never applied,** and the latency wizard never receives notes (`addMeasurement` has no caller). The profile selection is not persisted.
5. **Character "aged electronics", body age, temperature and humidity have no audible effect.** Their outputs are never consumed by the engine (`applyPotTaper`, `getCapacitorDrift`, `getJackGain`, `getPickupBalanceDb`, `getSaddleBalanceDb`, `getBodyQMultiplier`, `getAirResonanceMultiplier`, `getTemperatureOffsetCents`, `getHumidity*`). Environment is in progress on realism-a; the rest is not.
6. **The scale trainer's quiz, interval and chord-tone modes cannot be answered by playing:** `ScaleTrainer::answer` is never fed MIDI.
7. **The EQ-match result overwrites cab IR slot 2** instead of being an insert filter at a chosen position, and cab/EQ analysis runs on the message thread, so the UI stalls.
8. **Live actions cannot be put on a footswitch or CC:** next/previous snapshot, snapshot-by-CC, tap, kill and setlist navigation. MIDI Learn is parameter-only, and there is no per-preset or global learn scope.
9. **Snapshot morph cannot be driven by an expression pedal, LFO or mod wheel,** because it is not a parameter or modulation destination. Morph exclusions and Bezier points have no UI.
10. **There is no root file-drop handler** for presets, guitars, tunes, loops, setlists, profiles, audio or MIDI. `.mid` import and drop are in progress on model-gaps.
11. **The tab reader is thin:** no GP5/GP/PTB import, static text only, no cursor or playback, no fretboard highlight, no play-along scoring.
12. **Usage and diagnostics telemetry and crash reporting are shells:** nothing records events, and there is no crash handler to write dumps. The License class has no activation UI.
13. **The metronome ignores host and tap tempo** and is not transport-synced.
14. **MP3 backing tracks are not supported** (the spec requires MP3 via dr_mp3). The file chooser still offers `*.mp3`, so choosing one fails.
15. **The MOD tab lacks several editors that exist in the engine:** route offset, LFO phase and custom shape, envelope curves and loop mode, and the step-sequencer step grid. There are also no per-source colours. Drag-to-route is in progress on visual.
16. **Luthier-profile MIDI export from a capture omits most realism classes** (STRUM, PICK, SQUEAK, BUZZ, CLANK, VIBRATO, WHAMMY), which weakens the "lossless" claim for captured takes.
17. **MIDI input path gaps:** MIDI Learn does not consume the learned CC, incoming Luthier SysEx is not dispatched, and MIDI clock/transport messages are ignored.
18. **No sidechain-compressor pedal** exists, although routing-io 4 and input-routing 6 list it as a sidechain consumer.
19. **The Capture utility cannot record DI or per-string sources,** and three tone-match tests are missing.
20. **The looper does not re-render through a new tone.** The MIDI is captured, but no re-render path exists.

### Unspecified gaps noticed

- A **built-in tuner** (strobe/needle) for tuning against the backing track or for checking Character drift. No spec in group D covers it.
- **An undo history for routing and modulation edits.** The route table and aux strips bypass the undo stack (`pushUndoState` is used for presets only).
- **Host automation of routing, monitor and practice levels.** These are deliberately not parameters (see the RoutingPanel.h comment), but buyers expect to automate the monitor or backing level.
- **A MIDI-monitor / activity view** showing which consumer took an event, to debug "why isn't MIDI Learn seeing my CC". input-routing.md motivates this but does not specify a view.
- **A per-controller setup check.** CONTROLLERS shows the routing text, but there is no live "play each string" check that confirms the channel-to-string mapping for a hex pickup.
- **Stale documentation:** spec/GAPS.md still says PRACTICE, NOTATION and MIDI OUT are unbuilt ("no spec", "needs live capture", "unimplemented spec"), but all three tabs exist and are tested.

### Small glue candidates

These engine features exist and only need a control, a call site, or a preset field.

| Feature / id | Engine hook | Where the control should go | Effort |
|---|---|---|---|
| Mod route offset | `ModMatrix` route `offset` (a setter or `setRoute`) | MOD tab ModRouteTable: add an "Offset" column beside Depth | small |
| LFO phase offset | `ModLfo::setPhaseOffsetDegrees` | MOD tab ModSourceCard (LFO): add a slider | trivial |
| Envelope stage curve, loop mode | `ModEnvelope::setStageCurve`, `setLoopMode` | MOD tab ModSourceCard (Envelope): combo boxes | small |
| Step-sequencer steps | `ModStepSequencer::setStep` | MOD tab card: mini grid (value, gate, slide, probability) | medium |
| Morph exclusions | `SnapshotBank::setParameterExcludedFromMorph` | LIVE tab: exclusion list; or knob right-click "Exclude from morph" | small |
| Morph Bezier points | `SnapshotBank::setBezierControlPoints` | LIVE tab beside morphCurveBox (shown when Bezier is selected) | small |
| Monitor pan + 3-band EQ | `MonitorMix::setPan`, `setEqLowDb`/`setEqMidDb`/`setEqHighDb` | LIVE tab (monitor section); level stays on LiveStrip | trivial |
| Snapshot morph as parameter | add a `snapshot_morph_position` APVTS param mirrored to `SnapshotBank::setMorphPosition` | makes it learnable and a mod destination; LiveStrip knob attaches to it | small |
| Expression calibration feed and apply | call `getExpression().observe(cc,val)` and `map()` in the CC path before MidiLearn/feedModulationSources | Options -> EXPRESSION (UI exists) | small |
| Controller latency wizard feed | call `wizard.addMeasurement(ms)` on note-on relative to the metronome click | CONTROLLERS page (UI exists) | small |
| Controller profile vs params | on profile apply, write `mpe_enabled` and `bend_range` params (or skip them in `applyToEngine`); persist the profile id in state | CONTROLLERS page | small |
| Backing-track pan / low-cut / high-cut | `BackingTrackPlayer::setPan`, `setLowCutHz`, `setHighCutHz` | Practice drawer TRACK tab | trivial |
| Backing-track playlist | `BackingTrackPlayer::setPlaylist` + next/previous | Practice drawer TRACK tab (Add to playlist) | small |
| Looper per-layer low/high cut | `LoopLayer::setLowCutHz`, `setHighCutHz` | Practice drawer LOOP tab, per-layer row | trivial |
| IR start/end trim | `IrSlot::setStartTrim`, `setEndTrim` | TONE MATCH IrSlotEditor | trivial |
| EQ-match band | `EqMatch::Options.lowHz/highHz` | TONE MATCH EQ wizard: two sliders | trivial |
| Capture source DI / per-string | extend `Capture::Source`; taps already exist in TapBuffers (Aux 1, per-string) | TONE MATCH capture pane: source combo | small |
| Scale quiz from played notes | forward note-ons to `ScaleTrainer::answer` when the SCALE tab is in quiz mode | Practice drawer SCALE | small |
| Metronome follows tap/host | `metronome.setTempo(blockTempo)` when a "Follow" toggle is on | Practice drawer METRO | trivial |
| Character pickup/saddle balance, cap drift, pot taper, jack gain, body age | apply `getPickupBalanceDb`/`getSaddleBalanceDb` in PickupEngine, `applyPotTaper`/`getCapacitorDrift` in the circuit, `getJackGain` on output, `get*Multiplier` in BodyEngine | CHARACTER tab (controls exist) | small each |
| LinnStrument rows-as-strings / Osmose curve | apply `rowsAsStrings` / `applyPitchCurve` in `ControllerProfileLibrary::apply` / MidiInterpreter | CONTROLLERS page (toggle exists) | small |
| Rhythm on/off as a parameter | add `rhythm_enabled` APVTS bool mirrored to `RhythmEngine::setEnabled` | Easy rhythm strip + RHYTHM enable toggle (ButtonAttachment) | trivial |

---

## Group E



Audited: the `claude/luthier-audit` checkout (HEAD `2ede79c`, 10 commits ahead of `origin/claude/luthier-cloud-session-5lzlix` `6bda872`; nothing behind). Helper branches were compared with `git diff origin/claude/luthier-cloud-session-5lzlix...origin/claude/luthier-<name>`: realism-a, realism-b, realism-c, techniques, model-gaps, visual, tune-help. The audit was static. Nothing was built or run, so "Test covering it" means the test exists, not that it passes.

Legend: **yes** = implemented and wired on HEAD. **partial** = wired, but it departs from the spec or is incomplete. **no** = absent on HEAD. "in progress on X" = present only on helper branch X.

---

### volume-knob-interaction.md

**Summary:** `GuitarCircuit` replaces `CableSim`. It is solved as one nodal network, sits in the chain, and has all 15 parameters, the Advanced column 2 CIRCUIT panel, a live visualiser and the Easy compact card. Nearly every spec test exists in `CircuitTests.cpp`, and the feedback coupling test is in `FeedbackTests.cpp`. The gaps:
- The CHARACTER tab has no full-size CIRCUIT mirror.
- The custom bleed R/C editor is always visible instead of appearing only for Custom.
- The network is re-solved on the audio thread. There is no allocation, but ground rule 5 asks for the message thread.
- `macro_tone` silently rescales the tone wiper.

Counts: **yes 20 / partial 4 / no 1**.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Solve pickup+pots+cap+bleed+cable+amp in as one network | 0.1, 1.1 | yes | DSP/Circuit/GuitarCircuit.cpp: `buildPassiveNetlist`, `rebuild` | n/a | Circuit.theAudioPathMatchesTheResponse | Nodal/trapezoidal solve, not a biquad pair; exact for fixed knobs |
| CableSim removed, not deprecated | intro | yes | only a comment in GuitarCircuit.h remains | n/a | – | |
| Coefficients computed on the message thread; audio thread runs a fixed filter | 0.5 | partial | LuthierEngine.cpp:processBlock → `circuit.setComponents()` (matrix inverse at block rate, on the audio thread) | n/a | Circuit.sweepingEveryControlDoesNotAllocate | No allocation, but not the thread the spec names |
| Ls/Rs/Cp from the pickup part | 1, 4 | yes | LuthierEngine.cpp:`getLiveCircuitComponents` ← `pickups.getSelectedCoil()` | n/a | Circuit.theEngineRunsThroughTheCircuit | Acoustic → no coil |
| Volume = wiper (level + load + cable decoupling) | 1.2 | yes | `taperFraction`, netlist | Adv col 2 CIRCUIT; Easy rig card; guitar illustration | Circuit.turningDownDarkensAsWellAsQuietens | |
| Treble bleed None / Kinman / Fender / Custom | 1.3 | yes | `TrebleBleed`; Parameters.cpp:`trebleBleedNames` | Adv col 2 | Circuit.aKinmanBleedKeepsTheTop | Shown as "Modern RC" / "Vintage Cap" (trademark rule) |
| Custom bleed R, C, series/parallel | 1.3 | yes | `circuit_bleed_r/_c/_mode` | Adv col 2 | Circuit.everyCornerOfTheAdvancedRangeIsStable | |
| Active mode: buffer, plain attenuator, active 1-pole tone | 1.4 | yes | `buildActivePickupNetlist`, `activeToneCornerHz` | Adv col 2 "Active" toggle | Circuit.activeModeRemovesTheLoading | |
| Cable quality pF/m table (52/98/160/220) | 2 | yes | `cableCapacitancePerMetre` | Adv col 2 | Circuit.cableCapacitanceMovesTheResonance | |
| `cable_on` off = zero-length cable | 2 | yes | `cableCapacitance` | Adv col 2 | Circuit.bypassIsNeutral | |
| 50s wiring topology | 3.1 | yes | `PotTaper::fiftiesWiring` | Adv col 2 taper dropdown | – | No dedicated test |
| 15 params, IDs and defaults | 3 | yes | Parameters.cpp:606-620 | – | Parameters count | Caps are declared in nF, not F (documented) |
| Advanced ranges (circuit family) | 3 | yes | PhysicalRange.cpp circuit rows | Ranges padlock | Circuit.everyCornerOfTheAdvancedRangeIsStable | |
| Feedback level taken after the circuit (±0.5 dB) | 4 | yes | FeedbackLoop.cpp | n/a | Feedback.theVolumeKnobLowersTheLoopByTheCircuitsAttenuation | |
| Chain position: String→Pickup→Circuit→PreFX | 4 | yes | LuthierEngine.cpp:1877 | n/a | Circuit.theEngineRunsThroughTheCircuit | |
| Large volume/tone knobs in Adv col 2 | 5 | yes | UI/AdvancedPanel.cpp:`addLargeKnob` | Adv col 2 CIRCUIT | GuiReach | |
| Pot / cap dropdowns with a custom value | 5 | yes | UI/CircuitPanel.h:`StandardValueChoice` | Adv col 2 | GuiReach | |
| Custom R/C editor appears only for Custom | 5 | partial | AdvancedPanel.cpp:677-678 | Adv col 2 (always shown) | – | No visibility toggle on `circuit_treble_bleed` |
| Live H(s) visualiser with marked peak | 5 | yes | UI/CircuitPanel.cpp:`CircuitResponseView` | Adv col 2, Easy | – | No UI test |
| CHARACTER tab CIRCUIT group (full-size mirror) | 5; gui-int 4.4 | no | CharacterPanel.cpp has no circuit | no | – | gui-integration 19 also lists it |
| Easy compact card (vol, tone, mini visualiser) | 5 | yes | EasyPanel.cpp:276-281 | Easy rig strip | – | |
| Test: tone sweeps monotonic, never >unity | 6 | yes | – | – | Circuit.theToneControlSweepsDownAndNeverBoosts | |
| Test: pot value moves the resonance | 6 | yes | – | – | Circuit.potValueMovesTheResonance | |
| Test: stability at 44.1-192 kHz, no NaN, ≤+24 dBFS | 6 | yes | – | – | Circuit.everyCornerOfTheAdvancedRangeIsStable | |
| `guitar_tone` semantics are the tone wiper only | 3 | partial | Parameters.cpp:1339 `tone * (0.45 + macroTone*1.1)` | – | – | The Easy tone macro silently rescales the physical wiper, which breaks "physical deltas only" |

---

### pick-noise.md

**Summary:** The shared `NoiseEngine` pool matches the spec: six classes, fixed pools, oldest-steal, deterministic hashing, and correct injection points. Aux 8 is wired. Click, chirp and fingertip noise follow the spec formulas. The PICK group on CHARACTER holds every control. The gaps:
- Three materials are missing (Ultex, Tortex, Stone/horn).
- The default thickness is 1.07 mm, not 0.73.
- There is no pick illustration with drag handles.
- The rake can only be fired by keyswitch while the SCRAPE technique is armed. There is no Easy gesture, and SCRAPE has no UI on HEAD.
- Pool degradation (step 4) is never called.
- There are no "noise rides the instrument" or allocation tests.

Counts: **yes 17 / partial 6 / no 4**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| NoiseEngine pools 16/16/16/8/16/8, pre-allocated | 1 | yes | DSP/Noise/NoiseEngine.h:`kPoolSizes` | n/a | NoisePool.aFullPoolStealsTheOldest | |
| Steal the oldest | 1 | yes | `NoiseEngine::trigger` | n/a | NoisePool.aFullPoolStealsTheOldest | |
| Degradation step 4 halves pools | 1 | partial | `NoiseEngine::setDegraded` | n/a | NoisePool (degraded=8) | Nothing calls `setDegraded` outside tests |
| Per-material textures synthesised once, random offset | 1 | partial | `synthesiseTexture` (5 generic textures) | n/a | NoisePool.aSeedRepeatsExactly | Shared by texture class, not one table per material |
| Generator = excitation → resonator bank → envelope | 1.1 | yes | `NoiseGenerator` | n/a | – | |
| Injection points (click→excitation; others pre-body) | 1.2 | yes | `processSample(excitationNoise, surfaceNoise)` | n/a | – | |
| Aux 8 carries the noise sum | 1.3 | yes | LuthierEngine.cpp:1758; RoutingMatrix | ROUTING tab | PluginBuses.aux8CarriesThePlayingNoiseAndObeysItsStrip | |
| `pick_material` choice + 8 materials | 2.1 | partial | PlayingNoise.h:`getPickMaterial`; `pickMaterialNames` | CHARACTER → PICK | PickNoise.clickPitchTracksMaterialAndThickness | Ultex, Tortex and Stone/horn missing (the code comment admits it) |
| `pick_thickness` 0.38-3 mm, default 0.73 | 2, 7 | partial | Parameters.cpp:486 (0-1 norm, default 0.5) | CHARACTER → PICK | – | Default decodes to 1.07 mm via `pickThicknessMm`. It should be about 0.316 |
| `pick_tip_radius`, `pick_bevel`, `pick_wear` | 2 | yes | Parameters.cpp:500-505 | CHARACTER → PICK | – | |
| `pick_angle` 0-60°, default 20 | 2 | yes | normalised 0.35 → 21° | CHARACTER → PICK | PickNoise.angleTradesClickForChirp | |
| Click level = amount×vel^0.7×stiffness×cos^1.5, −30 dB nominal | 3 | yes | `PlayingNoise::makeClick` | – | PickNoise.clickScalesWithVelocityToThePower0_7; aClickSitsAboutThirtyDecibelsUnderTheNote | |
| Click fundamental formula; decay 3-15 ms; wear sub-resonance; brighter near bridge | 3 | yes | `makeClick` | – | PickNoise.clickPitchTracksMaterialAndThickness | |
| Chirp: wound only, sin(angle)×rough×depth, glide | 4 | yes | `makeChirp` | – | PickNoise.plainStringsNeverChirp | |
| Scrape/rake: pick_scrape MIDI class or Easy rake gesture | 5 | partial | ScrapeEngine keyswitches 13/14 → `triggerPickScrape` | no | – | Fires only while SCRAPE is armed in keyswitch mode, and no scrape_* control on HEAD. No Easy gesture |
| Rake sweeps across strings; 8-voice pool | 5 | yes | `PlayingNoise::startScrape` | – | – | |
| `use_fingers` silences pick noise | 6 | yes | `makeClick`/`makeChirp` guards | Adv RIGHT HAND | PickNoise.fingersNeitherClickNorChirp | |
| Nail/flesh + fingertip release noise (−12 dB) | 6 | yes | `makeFingertipNoise` | Adv (nail_vs_flesh) | PickNoise.fingersNeitherClickNorChirp | |
| Params: `pick_click/chirp/scrape_amount` with advanced range 0-4 | 7 | yes | Parameters.cpp; PhysicalRange pick family | CHARACTER → PICK | Ranges | |
| PICK group: striker dropdowns, material…scrape | 8 | yes | UI/NoiseGroups.cpp | CHARACTER → PICK | NoiseUi.* | Striker mirrors STRUM |
| Pick illustration at true size, angle-rotated, 8 px drag handles | 8 | no | – | no | – | |
| Workshop pick part drives the pick params | 8 / workshop | yes | Workshop/WorkshopBench.cpp:190-194 | Workshop | – | |
| Test: noise through two bodies differs; amount 0 bit-identical | 9 | no | – | – | – | |
| Test: zero is free over 10 000 notes | 9 | yes | – | – | NoisePool.zeroIsFree | |
| Test: 20 clicks into a 16-pool | 9 | yes | – | – | NoisePool.aFullPoolStealsTheOldest | |
| Test: no audio-thread allocation for noise | 9 | no | – | – | – | Allocation counter exists (CircuitTests) but no noise test |
| Test: deterministic seed | 9 | yes | – | – | NoisePool.aSeedRepeatsExactly | |

---

### string-squeak.md

**Summary:** The squeak model follows the spec: winding pitch × speed, glide, plain strings silent, min travel, deterministic probability, moisture/pressure, style presets with "(modified)", and the noise-event strip. The gaps:
- Only the legacy `TechniqueEngine` slide triggers a squeak. PerformanceScore position changes and ChordVoicer revoicing do not.
- Fret wear does not raise squeak.
- The winding selector edits `string_material` directly instead of mirroring or jumping to the Workshop part.
- The bass pressure default (0.35) exists only on model-gaps.
- Allocation and vibrato tests are missing.

Counts: **yes 17 / partial 5 / no 2**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Wound strings only | 0.1 | yes | `PlayingNoise::makeSqueak` | – | Squeak.flatwoundIsNearlySilentAndPlainIsSilent | |
| No "squeak now" control | 0.2 | yes | – | – | – | |
| f = speed × winding pitch, with glide | 1, 3 | yes | `makeSqueak` | – | Squeak.pitchTracksSpeedAndWinding | The glide check is inside that test |
| Trigger: legato slide | 2 | yes | LuthierEngine.cpp:950 `onShift` | – | Squeak.aLegatoSlideInTheEngineSqueaksAndABendDoesNot | |
| Trigger: PerformanceScore position change / ChordVoicer revoice / MIDI slide event | 2 | partial | only `Technique::Slide` with `slideFromFret` | – | – | A voicer revoice of a held note never squeaks |
| Not a trigger: new pluck, bend, vibrato | 2 | yes | `onShift` gate | – | Squeak.aLegatoSlide…ABendDoesNot | No vibrato test |
| `squeak_min_travel` 1.5 frets | 2.1 | yes | `makeSqueak` | CHARACTER → STRING NOISE | Squeak.theMinimumTravelIsRespected | |
| Level formula (pressure^1.3, moisture roughness, speed/300) | 3 | yes | `makeSqueak` | – | Squeak.aShiftSitsTwentyToThirtyDecibelsUnderTheNote | |
| Per-winding brightness/texture table (8 windings) | 4 | yes | `windingBrightness`, `windingTexture` | – | Squeak.flatwound… | Textures are shared classes |
| Injection pre-body + Aux 8 | 5 | yes | NoiseEngine surface path | – | PluginBuses.aux8… | |
| `squeak_probability` deterministic per seed and note | 6 | yes | `onShift(shiftIndex)` | CHARACTER | Squeak.theProbabilityRollIsDeterministic | |
| `squeak_finger_moisture` lowers probability and brightness | 6 | yes | `makeSqueak` | CHARACTER | – | |
| `squeak_finger_pressure` | 7 | yes | – | CHARACTER | – | |
| Bass default pressure 0.35 | 7 | no | – | – | – | In progress on model-gaps (`BassFamilyDefaults`) |
| Style presets (5) + first-run "Natural (modified)" at 0.25 | 8 | yes | NoiseGroups.cpp:238-267; Parameters default 0.25 | CHARACTER → STRING NOISE | NoiseUi.squeakStylesApplyAndReadModified | |
| 6 params, advanced ranges | 9 | yes | Parameters.cpp:510-515; PhysicalRange squeak | CHARACTER | Ranges | |
| STRING NOISE group contents | 9 | yes | UI/NoiseGroups.cpp | CHARACTER → STRING NOISE | NoiseUi.* | |
| Noise-event strip 24 px, 8 s, 30 Hz, greys at 2 s | 9.1 | yes | `NoiseEventStrip` | CHARACTER | NoiseUi.theEventStripShowsWhatTheEngineTriggered | |
| Winding selector mirrors the Workshop string part | 9.1 | partial | NoiseGroups.cpp:181 attaches `string_material` | CHARACTER | – | The Workshop exists now, so this should edit or jump to the part |
| Fret-buzz coexistence (no ducking) | 10 | yes | separate pools | – | – | |
| Slide Mode suppresses squeak on contacted strings | 10 | yes | `!slide.isUnderBar(s)` | – | Squeak.zeroIsFreeAndSlideModeSuppressesIt; Slide.squeakStopsUnderTheBar… | |
| Fret wear raises squeak | 10 | no | – | – | – | |
| Age multiplies roughness up to 1.4 | 10 | yes | PlayingNoise.cpp:24 (3-step) | Adv STRINGS age | – | Continuous version in progress on realism-a |
| MIDI export SQUEAK class; Generic drops it | 11 | yes | Export/LuthierMidiEvents.cpp | MIDI OUT | MidiExport.everyEventClassRoundTripsWithEveryField | |
| Test: no audio-thread allocation | 13 | no | – | – | – | |

---

### fret-buzz.md

**Summary:** The setup geometry, parabolic relief, block-rate sensing, burst-at-f0 generator, sitar mode, six setup styles, SETUP group and 30 Hz heatmap are all in and tested. The Workshop setup strip mirrors action, relief and nut depth. The gaps:
- Fret material does not set buzz brightness.
- Fret wear does not move buzz.
- A bend does not change clearance.
- The bass default style (Factory low) exists only on model-gaps.
- Budget and allocation tests are missing.

Counts: **yes 18 / partial 2 / no 5**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Buzz sensed from amplitude vs clearance, never scheduled | 0.1, 3.1 | yes | DSP/Noise/FretBuzz.cpp:`sense`, `process` | – | Buzz.lowActionBuzzesAndHighActionDoesNot | |
| Geometry params (action T/B, relief, nut×6, fret height) | 1, 7 | yes | Parameters.cpp:519-527 | CHARACTER → SETUP; Workshop setup strip | – | |
| Clearance: nut→12th lerp + relief parabola peaking at 7 | 1 | yes | `SetupGeometry::clearanceMm` | – | Buzz.reliefMovesWhereItBuzzes | |
| Only frets past the finger can buzz | 1 | yes | `clearanceMm` | – | Buzz.onlyFretsAheadOfTheFingerBuzz | |
| Modal amplitude at the fret, block rate | 2 | yes | `displacementMm` | – | – | |
| Threshold trim ±0.15 mm, not a mute | 3.2 | yes | – | CHARACTER | Buzz.theThresholdIsATrimNotAMute | |
| Generator bursts at f0 while excess > 0 | 4 | yes | `NoiseEvent::burstHz` | – | Buzz.buzzStopsAsTheNoteDecays | |
| Spectrum 3-6 kHz rising with fret | 4 | yes | FretBuzz.cpp | – | – | |
| Fret material sets brightness | 4 | no | – | – | – | Not read from the part |
| Level min(1, excess/0.3) × fret height | 4 | yes | `levelFor` | – | Buzz.fretHeightChangesLevelNotPosition | |
| Envelope 0.5 ms, decay tracks excess | 4 | yes | – | – | Buzz.buzzStopsAsTheNoteDecays | |
| Injection pre-body + Aux 8 | 4 | yes | NoiseEngine | – | – | |
| Sitar mode: continuous grazing, threshold bypassed | 5 | yes | FretBuzz.cpp:188-230 | CHARACTER toggle | Buzz.sitarModeIsContinuous | |
| Setup style dropdown (6 styles), Player-friendly default | 6.1 | yes | `getSetupStyle`, SetupGroup.cpp:196 | CHARACTER → SETUP | BuzzUi.setupStylesApplyAsOneStepAndReadModified | |
| Buzz heatmap: warning, accent and dot glyph; 30 Hz; greys at 2 s | 6.2 | yes | UI/SetupGroup.cpp:`BuzzHeatmap` | CHARACTER → SETUP | BuzzUi.heatmapCellsReadInMonochromeTerms | |
| Params +16 with advanced ranges (buzz family) | 7 | yes | PhysicalRange buzz rows | Ranges padlock | Ranges | |
| Fret wear moves buzz | 8 | no | – | – | – | Wear affects sustain only |
| Slide style suggested on Slide Mode | 8 | yes | SlideGroup.cpp:28 (one-click button) | CHARACTER → SLIDE | SlideUi.* | |
| Bass default: Factory low | 8 | no | – | – | – | In progress on model-gaps |
| Bends reduce buzz at the fret, raise it further up | 8 | no | – | – | – | |
| Old in-loop clipper uses the same setup | – | yes | LuthierEngine.cpp:1072 `fretActionMm` | – | – | |
| Test: heatmap matches audio | 9 | yes | – | – | Buzz.theHeatmapAgreesWithTheGenerator | |
| Test: Player-friendly only when attacked hard | 6.1 | yes | – | – | Buzz.playerFriendlyBuzzesOnlyWhenAttackedHard | |
| Test: block-rate sensing within the 0.2-unit budget | 9 | no | – | – | – | |
| Test: no audio-thread allocation | 9 | partial | – | – | – | Only covered indirectly by the general engine tests |

---

### slide-guitar.md

**Summary:** `SlideEngine` has the four modes, continuous pitch, damping behind the bar, slant, mm-based vibrato, intonation assist, friction, clank with rattle, the low-action message, the fretboard bar overlay, the header toggle and the rebindable `S` shortcut. The SLIDE group (shown only in Slide Mode) has every control. Spec tests are present. The gaps:
- **The bar is hard-coded.** A slide part fitted in the Workshop is stored by `WorkshopBench` but never reaches `SlideEngine::setBar`, so material, mass, length and diameter changes are inaudible. In progress on visual as `setSlideBar`.
- The tuning popover does not show continuous pitch.
- `slide_enabled` is shipped as `slide_guitar`.
- No allocation test.

Counts: **yes 20 / partial 5 / no 2**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Modes bottleneck / lap_steel / dobro / hybrid | 1 | yes | DSP/Slide/SlideEngine.h:`SlideMode` | CHARACTER → SLIDE | Slide.* | Dobro shown as "Resonator" |
| Hybrid routes un-barred notes through the fretted path | 1 | yes | `noteOn`, `isUnderBar` | – | Slide.squeakStopsUnderTheBarButNotBesideIt | |
| Bar = Workshop part (material/mass/length/diameter) | 2 | partial | `SlideBar` default only; WorkshopBench.cpp:198 stores `slidePart` | Workshop parts drawer (no audible effect) | – | `setBar` has no non-test caller. In progress on visual (`LuthierEngine::setSlideBar`) |
| Material table (7) | 2.1 | yes | `getSlideMaterial` | read-only mirror | Slide.theBarClanksWhenItLands | |
| Pitch continuous: f = f_open·L/(L−x) | 3 | yes | `contactFret` | – | Slide.pitchIsContinuous | |
| Damping behind (1.0 lap/dobro, 0.55 others) | 3 | yes | `sustainScale`, `defaultDampingBehind` | CHARACTER | Slide.theSegmentBehindIsDamped | |
| Contact loss by material + mass | 3 | yes | `sustainScale` | – | Slide.aHeavierBarSustainsLonger | Mass is only reachable in tests |
| Pressure: too light rattles, too heavy chokes | 3 | partial | `makeClank` rattle; pressure term in `sustainScale` | CHARACTER | SlideUi.pressureSaysWhatItMeans | No choke on the frets at high pressure |
| Slant: per-string contact offset | 3.1 | yes | `contactFret` | CHARACTER | Slide.slantGivesEachStringItsOwnInterval | |
| Slide vibrato moves x, depth in mm | 3.2 | yes | `vibratoCents`; LuthierEngine.cpp:1245 | Adv vibrato | – | |
| Intonation assist, 120 ms time constant | 4 | yes | `assist` | CHARACTER (labelled as an aid) | Slide.theAssistPullsToPitch | |
| Friction noise ∝ amount × friction × speed | 5.1 | yes | LuthierEngine.cpp:935 `setNoiseAmount` | CHARACTER | – | |
| Clank pool 8; landing + rattle below 0.3 pressure | 5.2 | yes | `makeClank` | CHARACTER | Slide.theBarClanksWhenItLands | |
| Low-action (<2.2 mm) message; setup not changed | 6 | yes | `kLowActionMessage`; SlideGroup | CHARACTER → SLIDE | SlideUi.theSlideGroupAppears… (warning check) | Test does not assert that the setup is unchanged |
| Buzz still active under a slide | 6, 8 | yes | FretBuzz unchanged | – | – | |
| 8 params | 7 | partial | Parameters.cpp:532-538; `slide_guitar` = enable | CHARACTER, header | – | Spec's `slide_enabled` ID is `slide_guitar` |
| Advanced ranges (slide family) | 7 | yes | PhysicalRange slide rows | padlock | Ranges | |
| SLIDE group only in Slide Mode | 7 | yes | CharacterPanel.cpp:325 `addChildComponent` | CHARACTER → SLIDE | SlideUi.theSlideGroupAppearsWithSlideModeAndTheTabFitsIt | |
| Header toggle + `S` shortcut | 7; gui 7, 17 | yes | HeaderBar.cpp:`toggleSlideMode`; Accessibility.cpp:499 | Header | – | |
| Workshop Slide category enabled only in Slide Mode | 7 | yes | WorkshopPanel.cpp:956 | Workshop | WorkshopPanel.aSlideNeedsSlideMode | |
| Fretboard bar overlay (slant, material colour, 80% opacity, 80 ms ease) | 7 | yes | FretboardComponent.cpp:168-195; GuitarBodyComponent.cpp:143 | Fretboard | – | |
| Tuning popover shows continuous pitch / glide target | 7 | no | GuitarBodyComponent.cpp:`TuningPopover` (no slide branch) | no | – | |
| Squeak suppressed on contacted strings only | 8 | yes | – | – | Slide.squeakStopsUnderTheBarButNotBesideIt | |
| MIDI export SLIDE_BAR (Luthier), pitch bend (Generic) | 8 | partial | Export classes | MIDI OUT | Capture.bassAndSlideEventsBecomeLuthierEvents | Live capture of bar movement in progress on model-gaps |
| Test: mode switch mid-note is clean | 9 | yes | – | – | Slide.switchingModeMidNoteIsClean | |
| Test: no audio-thread allocation | 9 | no | – | – | – | |
| Slide technique controls (sources, gestures) | beyond spec | partial | – | – | – | In progress on techniques (`SlideControlSettings`, SLIDE sub-tab) |

---

### bass-techniques.md

**Summary:** On HEAD the engine side mostly exists. `SlapEngine` has slap/pop/double-thump/ghost/auto-ghost with 12 parameters bridged, and the slap clack comes from the fret-buzz generator. Strum defaults already retarget for bass. The problems:
- **None of the 12 slap/pop/ghost parameters has a control on HEAD**: no SLAP group, no bass step grid, no bass kits.
- `finger_alternation_variation` and `rest_stroke` do not exist on HEAD.
- The bass family defaults for squeak, setup, pick, pluck position and compressor are missing.
- The bass palm-mute profile is missing.
- Double-thump timing uses `slap_rebound_gap`, not `1/(2·strum_crossing_sps)`.

Nearly all of it is in progress on model-gaps: `SlapGroup`, `BassGridGroup`, `BassStepGrid`, `BassFingerstyle`, `BassFamilyDefaults`, plus 2 parameters and a full `BassTechniqueTests` suite. Note that realism-b also declares `rest_stroke` and `finger_alternation_variation`, under a different header constant name (`bassRestStroke`), so those two branches will conflict when merged. GuiReach will probably flag the unattached slap parameters on HEAD (not verified).

Counts: **yes 9 / partial 6 / no 13**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Family detection from the spec, not string count | 1 | yes | SlapEngine `isBassFamily` | – | TunePlayer.theBassGoesToTheEngineOnlyForABass | |
| Inert on a guitar + fixed empty-state text | 0.1 | partial | Auto-ghost is gated on bass; slap is a guitar technique too (string-slap) | no | – | Empty-state text in progress on model-gaps |
| Slap collision: fast rise, into the frets, via the buzz generator | 2.1 | yes | SlapEngine.cpp:`shapeExcitation`; LuthierEngine.cpp:924 | – | Slap.theClackIsTheFretBuzzGenerator; SlapWiring.theClackComesFromTheBuzzGenerator | |
| `slap_strength/position_mm/thumb_hardness/fret_contact` | 2.2 | partial | Parameters.cpp:762-765; bridged at 1276 | **no** | SlapPresets.everySlapFieldRoundTrips | Position range 5-400 vs spec 20-200; not in the buzz range family. Controls in progress on model-gaps `SlapGroup` |
| Pop: release snap-back, brighter/shorter | 3 | yes | SlapEngine pop | no | (model-gaps BassTechniques.aPopIsBrighter…) | |
| `pop_strength`, `pop_position_mm` | 3 | partial | Parameters.cpp:766-767 | no | – | Range 5-400 vs spec 10-150 |
| Double thump: up stroke ×0.65, brighter | 4 | yes | SlapEngine.cpp:469 | no | Slap.theUpStrokeComesAtItsGapAndItsRatio | |
| Double-thump spacing = 1/(2·strum_crossing_sps) | 4 | partial | uses `reboundGapMs` (`slap_rebound_gap`) | no | SlapWiring.theDoubleThumpComesBackAtItsGap | Not tied to strum gesture timing |
| Ghost: damping before strike, shared with chuck | 5 | yes | `SlapEngine::applyGhostDamping` | no | SlapWiring.aGhostIsAThumpWithNoPitch | |
| `ghost_level/damping/auto/velocity_threshold`; auto on for bass | 5 | yes | SlapEngine.cpp:291 | **no** | – | No UI on HEAD |
| `BASS_TECH` MIDI triggers a ghost | 5 | yes | Export classes | – | MidiExport.everyEventClassRoundTripsWithEveryField | |
| Finger alternation (`finger_alternation_variation`) | 6 | no | – | no | – | In progress on model-gaps (`BassFingerstyle`) and realism-b (RightHand) |
| Rest stroke (`rest_stroke`) damps next-lower string | 6 | no | – | no | – | In progress on model-gaps (`StringEngine::touch`) |
| Bass pluck position nearer the bridge | 6 | no | – | – | – | In progress on model-gaps (0.12) |
| Pick bass 1.14 mm, more click | 7 | no | – | – | – | In progress on model-gaps |
| Bass palm-mute profile | 7 | no | – | – | – | In progress on model-gaps (`Damping::PalmMuteBass`) |
| Bass strum 100 sps / miss 0.01 | 8 | yes | PluginProcessor.cpp:`retargetStrumDefaults` | RHYTHM STRUM | StrumDynamics.bassDefaultsApply | |
| Bass squeak pressure 0.35 | 8 | no | – | – | – | model-gaps |
| Bass setup Factory low | 8 | no | – | – | – | model-gaps |
| Bass 864 mm scale default | 8 | partial | GuitarRenderer / part defaults | – | – | Rendering only; engine uses the guitar spec |
| Bass compressor on, 2:1 | 8 | no | – | – | – | model-gaps |
| SLAP group on CHARACTER (bass only) | 9 | no | – | no | – | In progress on model-gaps (UI/SlapGroup.cpp); techniques also adds a SLAP sub-tab |
| Bass step grid on RHYTHM (thumb/pop/ghost/finger/dead) | 9 | no | – | no | – | In progress on model-gaps (`BassStepGrid`, `BassGridGroup`) |
| Genre kits gain bass kits | 9 | no | – | – | – | model-gaps |
| MIDI export BASS_TECH Luthier/Generic | 10 | yes | Export/LuthierMidiEvents.cpp | MIDI OUT | Capture.bassAndSlideEventsBecomeLuthierEvents | |
| Live capture of BASS_TECH from the engine | 10 | no | – | – | – | model-gaps `captureBassTechnique` |
| Test: slap not a loud pluck; headroom; alternation; rest stroke; defaults on load | 12 | no | – | – | – | All in model-gaps `BassTechniqueTests.cpp` |
| Test: slap uses FretBuzz; double thump; ghosts pitchless | 12 | yes | – | – | SlapTests.cpp | |

---

### string-aging.md

**Summary:** Nothing of this spec is on HEAD beyond the legacy three-step `string_age` choice: `kAgeEffects` applied to all strings, with the squeak roughness table. None of the five parameters, `StringAging`, per-string state, restring, accrual, the `strings` range family or the STRING AGING group exists.

realism-a implements it all (`DSP/String/StringAging.*`, `UI/RealismGroups.*` `StringAgingGroup`, `StringAgingTests.cpp` SA-01..16) except two items: the Workshop strings-inspector mirror is deferred, and false beating is deferred by the spec itself. realism-a also amends the spec text. Merge risk: realism-c adds a different `Source/UI/RealismGroups.cpp/.h` (noise floor, sustain, tuning stability), so the two branches collide on file names.

Counts on HEAD: **yes 0 / partial 2 / no 21** (all 21 in progress on realism-a).

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Legacy `string_age` Fresh/BrokenIn/Old | 1 | partial | StringMaterials.cpp:`kAgeEffects`; Adv col 1 "Age" choice | Adv col 1 STRINGS | – | The spec wants this inert after load, kept for migration |
| Age roughness into squeak/chirp | 1, 3.5 | partial | PlayingNoise.cpp:24 (1.0/1.15/1.4) | – | – | No centroid term |
| `StringAging` class (POD, fixed arrays) | 5 | no | – | – | – | In progress on realism-a |
| Effective hours per string (wc/wk weights, jitter from seed) | 3.1 | no | – | – | – | realism-a (`CharacterEngine::hashedValue` exposed) |
| Coating rate factor (none/thin/thick) | 3.1-3.2 | no | – | – | – | realism-a |
| Physical curves B/S/D/P/C | 3.2 | no | – | – | – | realism-a |
| Legacy blend by `string_age_detail` | 3.3 | no | – | – | – | realism-a |
| Open-string detune + intonation slope added | 3.4 | no | – | – | – | realism-a (`TuningEngine::setAgingIntonation`) |
| Squeak level/centroid reconciliation | 3.5 | no | – | – | – | realism-a |
| Params `string_age_hours`, `string_corrosivity`, `string_age_detail`, `string_coating`, `string_age_accrual` | 4 | no | – | no | – | realism-a |
| `strings` range family | 4 | no | – | – | – | realism-a |
| `StringEngine::setAgingFactors` | 5 | no | – | – | – | realism-a |
| Block-rate advance in processBlock | 5 | no | – | – | – | realism-a |
| Restring one / restring all (one undo) | 6 | no | – | no | – | realism-a |
| Accrual of played time | 6 | no | – | no | – | realism-a |
| Adv col 1 age slider bound to hours | 7 | no | – | no | – | realism-a |
| CHARACTER → STRING AGING group, per-string rows, 4 Hz | 7 | no | – | no | – | realism-a `StringAgingGroup` |
| Workshop strings inspector mirror | 7 | no | – | no | – | Deferred even on realism-a |
| Padlock covers `strings` | 7 | no | – | – | – | realism-a |
| `character.aging` block + legacy migration | 8 | no | – | – | – | realism-a |
| 200 ms hours smoothing, budget 0.02 | 9 | no | – | – | – | realism-a |
| Tests SA-01..SA-16 | 10 | no | – | – | – | realism-a StringAgingTests.cpp |
| False beating | 3.6 | no | – | – | – | Deferred by the spec (proposals/) |

---

### environment.md

**Summary:** HEAD still has the placeholder the spec replaces:
- `CharacterEngine` has a three-step temperature at 0.125 cents/K.
- Its humidity multipliers are never read.
- The CHARACTER tab has two combo boxes for these.

There is no `EnvironmentModel`, none of the five parameters, and no lag, profiles, clock, body scaling, setup deltas or corrosion hook. realism-a implements it all (`Character/EnvironmentModel.*`, `EnvironmentGroup` + sparkline, `BodyEngine::setRuntimeScaling`, ENV-01..15). It defers the SETUP group's "+x mm (humidity)" secondary line.

Counts on HEAD: **yes 0 / partial 2 / no 19**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Temperature offset (placeholder) | intro | partial | CharacterEngine.cpp:555 `getTemperatureOffsetCents` | CHARACTER combo box | Character.temperatureProducesTheExpectedOffset | 8-30× too small; scaled by character amount (violates 0.4) |
| Humidity Q/compliance multipliers | intro | partial | CharacterEngine.cpp:586, 603 | CHARACTER combo box | CharacterTests | **Dead code: nothing in the engine reads them**. The UI control does nothing audible |
| `EnvironmentModel` class | 4 | no | – | – | – | realism-a |
| Thermal detuning from each string's strain | 2.1 | no | – | – | – | realism-a |
| Lags: wire / neck / body / wood | 2.2 | no | – | – | – | realism-a |
| EMC isotherm; relief/top/action deltas → FretBuzz | 2.3-2.4, 4 | no | – | – | – | realism-a |
| Fretting-stretch intonation | 2.5 | no | – | – | – | realism-a |
| Body plate/air/Q multipliers → BodyEngine + bank | 2.6 | no | – | – | – | realism-a (`setRuntimeScaling`, `BodyMode::isAir`) |
| k_RH corrosion hook | 2.7 | no | – | – | – | realism-a |
| Session profiles (6) | 3.1 | no | – | no | – | realism-a |
| Tuned-at reference + Retune (one undo) | 3.2 | no | – | no | – | realism-a |
| Closed form, seekable host clock / free-running | 3.3-3.4 | no | – | no | – | realism-a |
| Runs with character disabled | 0.4, 4 | no | – | – | – | realism-a |
| Params `env_temperature_c`, `env_tuned_at_c`, `env_humidity_pct`, `env_profile`, `env_clock` | 5 | no | – | no | – | realism-a |
| `environment` range family | 5 | no | – | – | – | realism-a |
| Serialization + legacy temperature/humidity conversion | 6 | no | – | – | – | realism-a |
| CHARACTER → ENVIRONMENT group, readouts 10 Hz, 60 s sparkline | 7 | no | – | no | – | realism-a |
| Convolution-mode note | 7 | no | – | – | – | realism-a |
| SETUP secondary "+x mm (humidity)" line | 7 | no | – | – | – | Deferred on realism-a |
| NaN guards, clamps, budget 0.02 | 8 | no | – | – | – | realism-a |
| Tests ENV-01..ENV-15 | 9 | no | – | – | – | realism-a EnvironmentTests.cpp |

---

### body-coupling.md

**Summary:** On HEAD the body is feed-forward only, and `CouplingMatrix` is the only string back-coupling. There is no `BodyCouplingBank`, `waveImpedance`, `getBridgeWave`, tap injection, five parameters, `body` range family, Coupling knob, BODY COUPLING group or wolf map. realism-a implements the spec, with amended numbers (budget 0.1 units; the bridge wave is the loop return). It defers two items:
- The Workshop body-inspector mirror.
- Factory re-voicing. Factory presets load with coupling 0 until a listening pass, so out of the box the feature is off in every factory sound.

Counts on HEAD: **yes 0 / partial 0 / no 18**.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Bridge admittance bank from BodyModels modes | 2.1-2.2 | no | – | – | – | realism-a DSP/Coupling/BodyCouplingBank.* |
| Loaded, passive discrete form (Q′), cap 0.35 | 2.3 | no | – | – | – | realism-a |
| Tap tones: tap force into the bank | 2.4 | no | – | – | – | realism-a (`driveDirect`, via slap body tap + Tap button) |
| `StringEngine::Physical::waveImpedance`, `getBridgeWave` | 3 | no | – | – | – | realism-a |
| Per-sample insertion after `coupling.process` | 3 | no | LuthierEngine.cpp:1753 (matrix only) | – | – | realism-a |
| Redesign on body rebuild / string refresh / part swap | 3 | no | – | – | – | realism-a |
| Shared scaling with BodyEngine (env × freq × Q) | 3 | no | – | – | – | realism-a |
| Params `body_coupling_amount`, `body_mode_mass_scale`, `body_mode_q_scale`, `body_mode_freq_scale`, `body_coupling_modes` | 4 | no | – | no | – | realism-a |
| `body` range family | 4 | no | – | – | – | realism-a |
| Adv col 1 BODY → Coupling knob | 5 | no | – | no | – | realism-a |
| CHARACTER → BODY COUPLING group: scales, mode list | 5 | no | – | no | – | realism-a `BodyCouplingGroup`, `BodyModeList` |
| Wolf map (strings × frets 0-19, warning + dot) | 5 | no | – | no | – | realism-a `WolfMap` |
| Tap button | 5 | no | – | no | – | realism-a |
| Workshop body-inspector mirror | 5 | no | – | no | – | Deferred on realism-a |
| Legacy load writes 0 | 6 | no | – | – | – | realism-a |
| Factory presets re-voiced with coupling on | 6 | no | – | – | – | Deferred on realism-a (listening pass) |
| Smoothing (20 ms amount, 0.2 %/block slew) | 7 | no | – | – | – | realism-a |
| Tests BC-01..BC-13 | 8 | no | – | – | – | realism-a BodyCouplingTests.cpp |

---

### Top gaps (group E)

Ranked by user impact.

1. **Bass techniques have no UI on HEAD.** The 12 `slap_*`/`pop_*`/`ghost_*`/`double_thump_*` parameters and the `slap_armed`/`slap_type`/`slap_trigger`… family have no control, so five bass guitars ship without a reachable SLAP group or bass step grid. This is ready on model-gaps (`SlapGroup`, `BassGridGroup`). GuiReach probably fails on HEAD.
2. **Environment controls do nothing, or too little.** The CHARACTER humidity box drives multipliers nothing reads. Temperature is 8-30× too weak and scales with the character amount. The fix is on realism-a.
3. **String aging is a 3-step global enum.** There is no per-string aging, coating, restring or hours. The fix is on realism-a.
4. **There are no wolf notes or body coupling.** They are on realism-a, but even there factory presets load with coupling 0 until a listening pass.
5. **The Workshop slide part is inaudible.** `SlideEngine::setBar` is never called, so bar material, mass, length and diameter are fixed at 65 g glass. The fix is on visual (`setSlideBar`).
6. **The bass family defaults are missing:** squeak pressure 0.35, Factory-low setup, 1.14 mm pick, bridge-ward pluck, 2:1 compressor, palm-mute profile, rest stroke, finger alternation. They are on model-gaps.
7. **Pick rake is unreachable.** It needs SCRAPE armed with a keyswitch trigger, and HEAD has no `scrape_*` control and no Easy rake gesture. The SCRAPE UI is on techniques.
8. **Default pick thickness is 1.07 mm, not 0.73 mm.** `pick_thickness` is normalised to 0.5. Every patch clicks lower and louder than the spec intends.
9. **Pick materials Ultex, Tortex and Stone/horn are missing.** These are the popular modern picks.
10. **Squeak triggers only on legato-slide techniques.** Tab playback position changes and ChordVoicer revoicing never squeak, so strummed and rhythm material is squeak-free.
11. **The CHARACTER tab has no CIRCUIT mirror.** gui-integration 4.4 and 19 require it.
12. **The tuning popover ignores Slide Mode.** It should show continuous pitch or the glide target.
13. **Fret-buzz interactions are missing:** fret material brightness, fret-wear redistribution, and bends raising the string.
14. **Noise pool degradation (performance-budget 9, step 4) is never triggered.** `setDegraded` has no caller.
15. **The custom treble-bleed R/C knobs are always visible,** even with None or a preset selected. That is confusing, because they do nothing then.
16. **`macro_tone` rescales `guitar_tone`.** The physical wiper position no longer matches the knob, which breaks "physical deltas only".
17. **Missing spec tests:**
    - noise through two bodies
    - no-allocation tests for noise, buzz and slide
    - buzz budget
    - vibrato causes no squeak
    - the slide low-action warning leaves the setup untouched
18. **Branches will conflict at merge.**
    - realism-a and realism-c both create `Source/UI/RealismGroups.cpp/.h`.
    - model-gaps and realism-b both declare `rest_stroke` and `finger_alternation_variation`, under different header constant names.
    - Several branches touch the parameter count in `IntegrationTests`.

### Unspecified gaps noticed

- **No global "playing noise" master or trim across classes.** Squeak, click, chirp, buzz, clank, fret and release noise each have their own amount. The Easy character macro covers squeak only (per gui-integration 19). A buyer expects one "finger/pick noise" knob on the Easy page.
- **No per-preset "clean DI / studio" realism profile** that disables every noise class and drift in one click. Style dropdowns exist per group but not globally.
- **No audible preview or audition button** on the STRING NOISE, PICK and SETUP groups. You must play MIDI to hear a change. The Tap button (realism-a) is the only audition control in this group.
- **No bass-specific amp or cab defaults** beyond the compressor, such as a DI + bass-cab blend. Bass players expect a bass amp or DI option.
- **The noise-event strip is not inspectable:** no hover to see which string or level. That would help "what is that noise?" further.
- **No MIDI-learnable "Slide Mode" momentary.** The keyboard shortcut is a latch, and the CC 75 toggle is documented only in Help.

### Small glue candidates

Items where the engine parameter or feature already exists on HEAD and only a control, a call or a field is missing.

| Item | Param IDs / symbol | Where it should go | Effort |
|---|---|---|---|
| SLAP group | `slap_strength`, `slap_position_mm`, `slap_thumb_hardness`, `slap_fret_contact`, `pop_strength`, `pop_position_mm`, `double_thump_enabled`, `double_thump_up_ratio`, `ghost_level`, `ghost_damping`, `ghost_auto`, `ghost_velocity_threshold` | CHARACTER → SLAP (bass only). Cherry-pick `UI/SlapGroup.*` from model-gaps minus its 2 new params | small |
| Slap arming/trigger | `slap_armed`, `slap_type`, `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`, `slap_ghost_cc`, `slap_force`, `slap_palm_position_mm`, `slap_string_mask`, `slap_ghost_mode`, `slap_rebound_gap`, `slap_snap_back`, `slap_body_part` | TECHNIQUES → SLAP sub-tab (techniques branch), or the CHARACTER SLAP group as a stopgap | small–medium |
| Scrape / rake controls | `scrape_armed`, `scrape_trigger`, `scrape_direction`, `scrape_tool`, `scrape_pressure`, `scrape_duration`, … | TECHNIQUES → SCRAPE sub-tab (techniques), plus an Easy "rake" button calling `LuthierEngine::triggerPickScrape` | small |
| Fitted slide part → engine | `WorkshopBench::slidePart` → `LuthierEngine::setSlideBar` → `SlideEngine::setBar` | PluginProcessor, on fit (already done on visual) | tiny |
| CHARACTER CIRCUIT mirror | `guitar_volume`, `guitar_tone`, `circuit_*`, `cable_*`, `amp_input_impedance` + `CircuitResponseView` | CHARACTER tab, new CIRCUIT group, reusing the AdvancedPanel column-2 builders | small |
| Bleed editor visibility | `circuit_bleed_r`, `circuit_bleed_c`, `circuit_bleed_mode` shown only when `circuit_treble_bleed` = Custom | Adv col 2 CIRCUIT | tiny |
| Pick thickness default | `pick_thickness` default 0.5 → 0.316 (0.73 mm via `pickThicknessMm`); PhysicalRange default too | Parameters.cpp:486, PhysicalRange.cpp | tiny (changes the sound of existing patches: presets pin their value) |
| Squeak winding mirror → Workshop | `string_material` selector in STRING NOISE → "Edit in Workshop" jump / part edit | CHARACTER → STRING NOISE | small |
| Noise-pool degradation | `NoiseEngine::setDegraded` ← performance-budget degradation step 4 | LuthierEngine CPU governor | tiny |
| Tuning popover in Slide Mode | `SlideEngine::getOverlayFret` / `assist` output → `TuningPopover` label | Guitar illustration headstock popover | small |
| Remove the dead humidity control (until realism-a lands) | CharacterPanel `humidityBox` → `CharacterEngine::setHumidity` (unused) | CHARACTER → environment: hide, or label "no effect" | tiny |

---

## Group F



Checked HEAD `2ede79c` (claude/luthier-audit), which contains `origin/claude/luthier-cloud-session-5lzlix`. I also checked the helper branches with `git diff origin/claude/luthier-cloud-session-5lzlix...origin/claude/luthier-<name>` and inspected their trees read-only (`git archive` into the scratchpad).

- **realism-b** (`2773bf1`) has harmonic-realism, string-interaction and fingerstyle-attack.
- **realism-c** (`15cf205`) has noise-floor, sustain-and-decay and tuning-stability.
- **Only strum-dynamics is on the integration branch.**
- Key: "rb" means in progress on `origin/claude/luthier-realism-b`, and "rc" means in progress on `origin/claude/luthier-realism-c`.
- The Implemented? column describes the integration branch. Branch status is in Notes.

### harmonic-realism.md

On the integration branch almost nothing in this spec exists (0 yes, 1 partial, 34 no). The old behaviour is still there:
- `StringEngine.cpp:282 t60Scale *= 0.55`
- pinch partial `TechniqueEngine.cpp:75 2 + velocity*3`
- natural harmonics sound an octave high, because the touch fret is voiced as the stopped fret

The in-progress branch realism-b covers 29 of the 35 rows. What it still lacks:
- the fretboard contact ring (the engine publishes `getContactDisplay`, but nothing in the UI reads it)
- MIDI-export `touch_fret` / `partial`
- HR-19
- section-19 doc rows
- HR-18's budget: the test allows 1.0 units, 20x the spec's 0.05, and the spec was not updated
- node efficiency: the branch changed the formula (super-Gaussian, off-node threshold 0.1) and recorded it only in a code comment, not in DECISIONS.md

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Contact = n-tap node comb in loop (eq 1-3) | 1,2 | no | rb: StringEngine::addContact/ContactState | n/a | rb HR02-04 | |
| `Contact` struct, kMaxContacts=4, add/clear/clearAll | 2 | no | rb: StringEngine.h:133-138 | n/a | rb HR12 | |
| Contact path skipped when idle (bit-identical) | 2 | no | rb | n/a | rb HR16 | |
| g ramps over 1 ms | 2 | no | rb | n/a | rb HR13 | |
| M,n recomputed on needsLoopUpdate only | 2 | no | rb | n/a | - | |
| Fallback to band isolation, counted by validator | 2 | no | rb: LuthierEngineRealismB.cpp, Validator::reportHarmonicFallback | n/a | rb HR15 | |
| setHarmonicRestriction kept as shim | 2 | partial | HEAD has old restriction; rb shim StringEngine.cpp:201 | n/a | - | |
| couplingReceptivity 0.35 while contact active | 2 | no | rb StringEngine.cpp:197 | n/a | - | |
| Remove `t60Scale *= 0.55` | 2 | no | HEAD StringEngine.cpp:282 still has it; rb removed | n/a | - | |
| processSample(coupling, directInput) split | 2 | no | rb StringEngine.h:177 | n/a | rb HR16 | scrape/slap/tap route through directInput in rb |
| Natural harmonic pitch = partial n of open string | 0.2,4.1 | no | rb MidiInterpreter.cpp:740 (touchFret) | n/a | rb HR01 | **HEAD sounds an octave high** |
| Harmonic kind renders as plain pluck (no band isolation) | 3 | no | rb | n/a | rb HR03, HR08 | |
| Artificial harmonic (fret+offset) | 3 | no | rb MidiInterpreter.cpp:745-751 | CC 103 only | rb HR09 | |
| Pinch: node nearest pick + thumb offset, not velocity | 3 | no | rb TechniqueEngine.cpp:60; HEAD :75 velocity | n/a | rb HR10 | |
| Tapped harmonic: legato, no voice steal | 3 | no | rb LuthierEngineRealismB.cpp:381 | CC 104 only | rb HR11 | |
| Analytic node search replaces fret table | 4.1 | no | rb Harmonics.h:findNode | n/a | rb HR05, HR06 | efficiency formula changed in code (super-Gaussian, 0.1 threshold) |
| Sounding-pitch mapping + HarmonicLocator | 4.2 | no | rb MidiInterpreter::emitSoundingHarmonic, Harmonics.h | CHARACTER->HARMONICS (rb) | rb HR14 | |
| `harmonic_touch_pressure` | 5 | no | rb Parameters | rb CHARACTER HarmonicsGroup | rb HR17 | |
| `harmonic_finger_width` | 5 | no | rb | rb HarmonicsGroup | rb HR07 | |
| `harmonic_touch_time` | 5 | no | rb | rb HarmonicsGroup | - | |
| `harmonic_brief_touch` | 5 | no | rb | rb HarmonicsGroup | rb HR10 | |
| `pinch_thumb_offset_mm` | 5 | no | rb | rb HarmonicsGroup | rb HR10 | |
| `artificial_harmonic_offset` | 5 | no | rb | rb HarmonicsGroup | rb HR09 | |
| `tapped_harmonic_offset` | 5 | no | rb | rb HarmonicsGroup | rb HR11 | |
| `harmonic_note_mapping` | 5 | no | rb | rb HarmonicsGroup | rb HR14 | |
| Physical rows in RangeRegistry (pick family) | 5 | no | rb PhysicalRange.cpp REALISM-B block | n/a | rb HR17 | |
| CC 103 ArtificialHarmonic / CC 104 TappedHarmonic | 6 | no | rb MidiInterpreter.cpp:208-209 | MIDI | - | no dedicated CC test |
| decide() priority incl. artificial and tapped | 6 | no | rb TechniqueEngine::decide | n/a | - | |
| HARMONICS row inside PICK group, with per-fret partial tooltips | 7 | no | rb UI/HarmonicsGroup.cpp | rb: separate group placed after PICK on CHARACTER | - | not literally inside PICK; tooltips list the offset partials |
| Fretboard hollow ring at contact, dashed when missed | 7 | no | rb engine getContactDisplay only | **no** (no UI reads it) | - | |
| Section 19 row in gui-integration.md | 7 | no | - | - | - | doc |
| MIDI export: `touch_fret` + `partial` on NOTE | 7 | no | Export/LuthierMidiEvents.cpp unchanged in rb | n/a | - | |
| reset() clears contacts | 8 | no | rb resetRealismB | n/a | - | |
| Budget 0.05 units, no allocation | 8 | no | rb | n/a | rb HR18 (allows ≤1.0 units) | spec not amended |
| HR-19 export round trip | 9 | no | - | - | **missing** | |

### string-interaction.md

On the integration branch the only piece that exists is the bridge `CouplingMatrix`, which predates this spec (0 yes, 0 partial, 30 no). realism-b implements all six interactions, the 7 parameters and the STRING INTERACTION group; 25 of the 30 rows are in progress there. Missing:
- palm-width shading on the fretboard
- palm width mirrored in TECHNIQUES->MUTE
- a real fretting-style source: it is derived from `rh_style==Classical`, because `mute_fretting_style` exists only on the techniques branch and in WIP
- SI-12, the per-string sum at -80 dBFS, which is not actually asserted
- SI-13's budget: the test allows 0.2 units against the spec's 0.1

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Air path rank-1 term, HP 120 Hz, tau 0.29 ms | 1 | no | rb CouplingMatrix (air) | n/a | rb SI01, SI03 | |
| a_cat per family (acoustic/archtop/solid/bass) | 1 | no | rb | n/a | rb SI01 | |
| Air through receive filter and kEnergyCap | 1 | no | rb | n/a | - | |
| Palm spread w(d), P>0.05, spreadMuted flag | 2 | no | rb LuthierEngineRealismB | n/a | rb SI04 | |
| Spread never overrides Choked/Silenced/Chuck; restores on lift | 2 | no | rb | n/a | rb SI05 | |
| Adjacent finger damping (Chuck, underside 1.0, tip 0.5) | 3 | no | rb LuthierEngineRealismB.cpp:145 | n/a | rb SI06 | |
| Fretting style scales it (rock 1.0 / classical 0.1) | 3 | no | rb: proxy via rh_style Classical | no (mute_fretting_style is on techniques/WIP only) | rb SI07 | merge conflict with the techniques branch |
| Chord's own notes do not mute each other | 3 | no | rb | n/a | rb SI07 | |
| Release stagger (3 ms groups, bias, seeded) | 4 | no | rb LuthierEngine::stageNoteOffs | n/a | rb SI08, SI09 | |
| Pickup crosstalk Gaussian aperture, bend lateral offset | 5 | no | rb PickupEngine::setStringLateralOffsets/apertureGain | n/a | rb SI10 | |
| Muted-string thump, live strum | 6 | no | rb MidiInterpreter.cpp:643-673, MutedThump.h | n/a | rb SI11 | |
| Muted-string thump, rhythm engine | 6 | no | rb RhythmEngine.cpp:614-634 | n/a | rb SI11 | |
| deadStrike skips activity, MIDI note and MIDI out | 6 | no | rb LuthierEngine.cpp:758 | n/a | rb SI11 | |
| `coupling_air_amount` | 7 | no | rb | rb CHARACTER->STRING INTERACTION | rb SI02 | |
| `palm_mute_spread` | 7 | no | rb | rb STRING INTERACTION; not in MUTE | rb SI04 | |
| `adjacent_mute_amount` | 7 | no | rb | rb STRING INTERACTION | rb SI06 | |
| `release_stagger_ms` | 7 | no | rb | rb STRING INTERACTION | rb SI08 | |
| `release_stagger_bias` | 7 | no | rb | rb STRING INTERACTION | rb SI09 | |
| `pickup_aperture_scale` | 7 | no | rb | rb STRING INTERACTION | rb SI10 | |
| `muted_thump_level` | 7 | no | rb | rb STRING INTERACTION | rb SI11 | |
| Range rows (pick/squeak/circuit) | 7 | no | rb PhysicalRange.cpp | n/a | rb SI12_SI14 | |
| STRING INTERACTION group on CHARACTER | 9 | no | rb UI/StringInteractionGroup | rb CHARACTER | - | fretting style shown as text only |
| Mute-zone shading shows palm width and weights | 9 | no | - | **no** | - | |
| Palm width mirrored in TECHNIQUES->MUTE | 9 | no | - | **no** | - | |
| Section 19 rows | 9 | no | - | - | - | doc |
| Flags cleared by reset, preset load and panic | 9 | no | rb resetRealismB | n/a | - | |
| Budget 0.1 units, no allocation | 10 | no | rb | n/a | rb SI13 (≤0.2 units) | test is looser than the spec |
| SI-12 per-string sum at -80 dBFS | 11 | no | - | - | **not asserted** in rb | |
| SI-14 preset round trip | 11 | no | rb | - | rb SI12_SI14 | |
| No new MIDI; CC 67 drives spread | 8 | no | rb | n/a | rb SI04 | |

### fingerstyle-attack.md

The integration branch has only the old binary material switch (1 partial, 44 no), and the step at 0.5 is still there (`LuthierEngine.cpp:583 nailVsFlesh > 0.5`). realism-b covers 38 of 45 rows: profiles, per-string tools, fingers carried from patterns, stroke, styles, CC 102/105, the RIGHT HAND group and the Easy Tool selector. Missing:
- tool glyph at the pluck point in the illustration
- MIDI-export PICK `tool` / `finger` / `stroke` fields
- FA-17's export round trip, which is not tested
- section-19 doc row

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| `Params::releaseSeconds`, lowpass 1/(2 pi tau) | 1 | no | rb Excitation.h | n/a | rb FA03 | |
| Full MaterialSpec blend by nail_vs_flesh (no step at 0.5) | 1 | partial | HEAD blends cutoff only (LuthierEngine.cpp:583 step); rb full blend | CHARACTER PICK (HEAD) | rb FA01_FA02 | |
| Defaults reproduce table (2.2/7.0/1.1 kHz) | 1 | no | rb | n/a | rb FA04 | |
| Nail click at pick_click x0.35 x b | 1 | no | rb | n/a | rb FA10 | |
| Thumb position offset, clamp 0.02-0.5 | 1 | no | rb | n/a | rb FA14 | |
| Rest stroke terms (level, length, bright, bridge drive, sustain) | 2 | no | rb | n/a | rb FA06 | |
| Rest-stroke neighbour Chuck (finger s+1, thumb s-1) | 2 | no | rb | n/a | rb FA07 | |
| Auto stroke rule (single note, v≥0.7) | 2 | no | rb | n/a | rb FA08 | |
| bass rest_stroke bool forces Rest | 2 | no | rb (param reused) | ? | - | |
| Tool resolution order CC102 > pattern finger > string > global | 3 | no | rb LuthierEngineRealismB | n/a | rb FA09, FA15 | |
| strikerMaterial wins for strums | 3 | no | rb LuthierEngineRealismB.cpp:487 | n/a | - | |
| NoteOnEvent::finger; scheduleFingerpick passes it | 3 | no | rb RhythmEngine.cpp:450,676 | n/a | rb FA09 | HEAD drops the finger |
| Strings 7-12 follow course / lowest | 3 | no | rb? | n/a | - | not verified |
| i/m alternation (tau, position, +1.5v ms) | 3 | no | rb | n/a | rb FA12 | |
| rh_style writes table, one undo step, never on preset load | 4 | no | rb RightHandGroup.cpp:188 ScopedUndoAction | rb RIGHT HAND | rb FA16 | |
| Travis thumb PalmMute | 4 | no | rb | n/a | rb FA11 | |
| Hybrid snap via SlapEngine pop path | 4 | no | rb | n/a | rb FA13 | |
| Slap/Pop tools on any family | 4 | no | rb | n/a | rb FA13 | |
| CC 102 RightHandTool (7 bands) | 5 | no | rb MidiInterpreter.cpp:207 | MIDI | rb FA15 | |
| CC 105 RestStroke | 5 | no | rb MidiInterpreter.cpp:210 | MIDI | rb FA15 | |
| `finger_flesh_release_ms` | 6 | no | rb | rb RIGHT HAND (+kHz readout) | rb FA03 | |
| `finger_nail_release_ms` | 6 | no | rb | rb RIGHT HAND (+kHz readout) | rb FA03 | |
| `thumb_position_offset` | 6 | no | rb | rb RIGHT HAND | rb FA14 | |
| `rest_stroke_damping` | 6 | no | rb | rb RIGHT HAND | rb FA07 | |
| `rh_stroke` | 6 | no | rb | rb RIGHT HAND | rb FA08 | |
| `rh_style` | 6 | no | rb | rb RIGHT HAND + Easy Tool selector | rb FA16 | |
| `rh_string_tool_1..6` | 6 | no | rb | rb RIGHT HAND six-cell row (click/right-click) | rb FA10 | |
| `thumb_palm_mute` | 6 | no | rb | rb RIGHT HAND | rb FA11 | |
| `hybrid_snap` | 6 | no | rb | rb RIGHT HAND | rb FA13 | |
| `finger_alternation_variation` / `rest_stroke` reused | 6 | no | rb Parameters | rb RIGHT HAND (conditional) | rb FA12 | |
| Range rows (pick family) | 6 | no | rb PhysicalRange.cpp | n/a | rb FA17 | |
| RIGHT HAND group, mirrors use_fingers and nail_vs_flesh | 7 | no | rb UI/RightHandGroup | rb CHARACTER | - | |
| Easy Playing strip Tool selector (Custom shown as "Mixed") | 7 | no | rb EasyPanel RightHandToolSelector | rb Easy Playing strip | - | |
| Illustration: tool glyph at pluck point per string | 7 | no | - | **no** | - | |
| Section 19 row | 7 | no | - | - | - | doc |
| MIDI export PICK `tool`, `finger`, `stroke` | 7 | no | Export unchanged | n/a | - | |
| reset clears rest neighbours and alternation phase | 8 | no | rb | n/a | - | |
| Budget 0.02 units, no allocation | 8 | no | rb | n/a | rb FA17 | |
| FA-05 pick path bit-identical | 9 | no | rb | - | rb FA05 | |
| FA-17 Luthier-profile export round trip (-60 dBFS null) | 9 | no | - | - | **missing** | |
| Pick-noise interplay (fingertip release noise unchanged) | 1 | yes* | HEAD PlayingNoise | n/a | existing | *pre-existing |
| Up-strum nails / thumb strikers | 3 | no | rb | n/a | - | |
| Undo for tool cell clicks | 7 | no | rb | rb | - | not verified |
| Choice lists append-only | 6 | no | rb | n/a | - | |
| Easy "Mixed" label | 7 | no | rb | rb | - | not verified |

### noise-floor.md

The integration branch has only the pre-existing single-coil hum (`noise_amp_buzz`, still labelled "Amp Buzz"): 1 yes, 1 partial, 38 no. realism-c implements the `NoiseFloor` module, the 12 parameters, all injection points, styles, the CHARACTER NOISE FLOOR group (position pad and meter), the ROUTING Aux 8 mirror, the Options default mains region and NF-01..NF-17; 36 of 40 rows are in progress there. Deviations:
- **NF-02:** the hum measures -58 dB at 1.0, outside the -40 ±6 window. The spec requires a DECISIONS.md entry for that; realism-c records it only in the commit message.
- **Advanced panel:** the knob is still labelled "Amp Buzz" (AdvancedPanel.cpp:740).
- **Ground-loop table:** it is rebuilt in place, not swapped through an atomic pointer on the message thread.
- **NF-15:** the test threshold is 0.02 units against the spec's 0.005.
- **performance-budget.md:** the NoiseFloor row was not added.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Existing hum kept (ID, range, default, constant) | 1 | yes | PickupEngine::processStrings | Advanced "Amp Buzz" knob | NoiseTests | |
| Display name "Single-coil Hum" | 1,5 | partial | rc Parameters.cpp:529 renamed | rc Advanced knob still "Amp Buzz" | - | label glue |
| Shared mains phase accumulator | 2 | no | rc NoiseFloor | n/a | rc NF-09 | |
| g_pos angle/distance on hum; bit-identical at 0°/1 m | 2.1 | no | rc PickupEngine::setHumPositionGain | rc position pad | rc NF-06 | |
| `noise_mains_hz` drives setMainsFrequency | 2.1 | no | rc Parameters.cpp:1319 | rc NOISE FLOOR 50/60 switch | rc NF-03 | HEAD hard-codes 60 Hz |
| Fluorescent buzz | 2.2 | no | rc NoiseFloor | rc Guitar row | rc NF-12 | |
| Passive (Johnson) hiss from live R | 2.3 | no | rc | rc Guitar row | rc NF-11 | |
| Cable movement rolls/events, 4 voices, q per CableQuality | 2.4 | no | rc | rc Guitar row | rc NF-10 | |
| Ground loop 2048-pt wavetable at amp input | 2.5 | no | rc NoiseFloor groundTable | rc Rig row | rc NF-04 | built in place, no atomic swap |
| Radio pickup | 2.6 | no | rc | rc Guitar row | rc ampInputSourcesHitTheirTargets | |
| Amp hiss with pinking at amp input | 2.7 | no | rc | rc Rig row | rc NF-07 | |
| Tube microphonics, G_m<0.95, x0.2 for 4x12 | 2.8 | no | rc NoiseFloor.cpp:433, LuthierEngine.cpp:1850 | rc Rig row | rc NF-08 | |
| `noise_player_angle` | 3 | no | rc | rc position pad (wheel) | rc NF-06 | |
| `noise_player_distance` | 3 | no | rc | rc position pad (drag) | rc NF-06 | |
| `noise_fluorescent` | 3 | no | rc | rc | rc NF-12 | |
| `noise_passive_hiss` | 3 | no | rc | rc | rc NF-11 | |
| `noise_cable_movement` | 3 | no | rc | rc | rc NF-10 | |
| `noise_radio` | 3 | no | rc | rc | rc | |
| `noise_ground_loop` | 3 | no | rc | rc | rc NF-04 | |
| `noise_amp_hiss` | 3 | no | rc | rc | rc NF-07 | |
| `noise_microphonics` | 3 | no | rc | rc | rc NF-08 | |
| `noise_floor_to_aux8` | 3 | no | rc | rc CHARACTER + ROUTING Aux 8 mirror | rc NF-13 | |
| `noise_floor_style` + "(modified)" | 3 | no | rc RealismStyleActions | rc NOISE FLOOR dropdown | rc NF-14 | one undo entry |
| `noise_amp_buzz` PhysicalRange row | 3 | no | rc | n/a | - | |
| Options "Default mains region" preference (seeds Init only) | 3 | no | rc OptionsPages | rc Options | - | |
| NoiseFloor class API (beginBlock, taps, onNoteOn, pushAmpOutput) | 4 | no | rc DSP/Noise/NoiseFloor.h | n/a | rc | |
| getSingleCoilShare factored out | 4 | no | rc PickupEngine.h:138 | n/a | - | |
| Acoustic path: hiss and cable only | 4 | no | rc | n/a | - | not verified |
| isIdle skips module | 0.5 | no | rc | n/a | rc NF-15 (<0.02, spec <0.005) | |
| Seeded, reset reseeds | 0.3 | no | rc | n/a | rc NF-09 | |
| NOISE FLOOR group after aged electronics | 5 | no | rc UI/RealismGroups NoiseFloorGroup | rc CHARACTER | rc RealismUiTests | |
| Noise meter (10 Hz, greys after 2 s) | 5 | no | rc NoiseMeter | rc | - | |
| Section 19 row | 5 | no | - | - | - | doc |
| Older presets load at 0/Off | 6 | no | rc | n/a | rc NF-01 | |
| Budget 0.15 units, row in performance-budget.md | 7 | partial | rc | n/a | - | spec row not added |
| No allocation incl. region/style change | 7 | no | rc | n/a | rc NF-16 | |
| NF-02 out-of-window hum recorded in DECISIONS.md | 2.1 | no | commit msg only (-58 dB) | - | rc NF-02 | rule violated |
| NF-17 sample-rate independence | 8 | no | rc | - | rc NF-03/06/07 loops at 44.1/96 k | |
| Style table values | 3 | no | rc RealismStyles.h | rc | rc NF-14 | |
| DC blocker and NaN guard on recursions | 7 | no | rc | n/a | rc NF-08 | |
| Aux 8 stem includes hum x g_pos | 4.6 | no | rc | n/a | rc NF-13 | |

### sustain-and-decay.md

The integration branch has only the legacy single T60 and `sustain_scale` (1 yes, 0 partial, 36 no). realism-c implements the whole StringEngine shape tick, the 9 parameters, the styles, the DECAY row with its sketch, SUSTAIN SHAPE with the per-string pitch readout, and SUS-01..SUS-15; 35 of 37 rows are in progress there. The branch commit also says "three thresholds corrected", but spec/ was not updated to match, and the performance-budget row is missing.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Legacy single-T60 model kept as slow stage | 1 | yes | StringEngine::updateLoopCoefficients | Sustain knob | existing | |
| Control tick every 32 samples | 0.2 | no | rc StringEngine kShapeTick | n/a | - | |
| Neutral values skip code (bit-identical) | 0.3 | no | rc | n/a | rc SUS-01 | |
| Brightness overshoot b(t) | 2.1 | no | rc | n/a | rc SUS-06 | |
| Hammer/pull restarts at 0.5 strength | 2.1 | no | rc | n/a | - | not verified |
| Longitudinal ping (Q30 at f_L, 2f_L -6 dB) | 2.2 | no | rc StringEngine Resonator | n/a | rc SUS-07 | |
| coreDiameterMm / tensionNewtons from StringSpec | 2.2,7 | no | rc StringEngine.h:48-49 | n/a | - | |
| Two-stage decay m(t) | 3 | no | rc | n/a | rc SUS-02, SUS-03 | |
| E-Bow and feedback reset the clock | 3 | no | rc | n/a | - | not verified |
| Tension-modulation pitch, clamp +25/+50 c | 4 | no | rc | n/a | rc SUS-04, SUS-05, SUS-11 | |
| getCurrentFrequency reports modulated pitch | 4 | no | rc | n/a | - | |
| Release damping ramp over T_r | 5.1 | no | rc StringEngine::release | n/a | rc SUS-08 | |
| Release sag (fretted only) | 5.2 | no | rc | n/a | rc SUS-09 | |
| Release ring to open string | 5.3 | no | rc | n/a | rc SUS-10 | |
| letRing / E-Bow skip release | 5 | no | rc | n/a | rc SUS-13 | |
| Range family `string` (named `strings`) | 6 | no | rc PhysicalRange.h (strings); DECISIONS "Phase 2b range families" | n/a | - | key is `strings`, not `string`; the decision is recorded |
| `sustain_attack_transient` | 6 | no | rc | rc CHARACTER SUSTAIN SHAPE | rc SUS-06 | |
| `sustain_attack_time` | 6 | no | rc | rc SUSTAIN SHAPE | rc SUS-06 | |
| `sustain_fast_share` | 6 | no | rc | rc SUSTAIN SHAPE + DECAY row | rc SUS-02 | |
| `sustain_fast_ratio` | 6 | no | rc | rc SUSTAIN SHAPE + DECAY row | rc SUS-02 | |
| `sustain_tension_mod` | 6 | no | rc | rc SUSTAIN SHAPE | rc SUS-04 | |
| `sustain_release_time` | 6 | no | rc | rc SUSTAIN SHAPE | rc SUS-08 | |
| `sustain_release_sag` | 6 | no | rc | rc SUSTAIN SHAPE | rc SUS-09 | |
| `sustain_release_ring` | 6 | no | rc | rc SUSTAIN SHAPE | rc SUS-10 | |
| `sustain_style` + "(modified)", one undo entry | 6.1 | no | rc RealismStyleActions | rc DECAY row + SUSTAIN SHAPE | rc SUS-12 | |
| `sustain_scale` joins family (advanced 0.05-4) | 6 | no | rc PhysicalRange.cpp:169 | Advanced Sustain knob | - | |
| Ship default Legacy | 6.1 | no | rc | - | rc SUS-01 | all presets stay legacy |
| setSustainShape at block rate | 7 | no | rc StringEngine.h:133 | n/a | - | |
| release(letRing, fret, sag) signature | 7 | no | rc release(letRing, fret) | n/a | - | sag taken from shape |
| DECAY row with decay sketch on STRINGS column | 8 | no | rc DecayRow / DecaySketch | rc Advanced Col 1 | rc RealismUiTests | |
| Per-string tension pitch readout (30 Hz, greys) | 8 | no | rc PitchOffsetReadout | rc SUSTAIN SHAPE | - | |
| Section 19 row | 8 | no | - | - | - | doc |
| Runtime state reset | 9 | no | rc | n/a | - | |
| Budget +0.3 units, no allocation | 10 | partial | rc | n/a | rc SUS-14 (≤0.3) | performance-budget.md not updated |
| SUS-11 bounded at limits | 11 | no | rc | - | rc | |
| SUS-15 sample-rate independence | 11 | no | rc | - | rc | |
| Test thresholds per spec | 11 | no | rc | - | rc (3 changed) | spec text not amended |

### tuning-stability.md

The integration branch has only `tuning_drift`, the character tuner-drift LFO and Retune (2 yes, 1 partial, 38 no). realism-c has `StabilityModel` with all six mechanisms, the Retune string/all/auto commands, session state, the 8 parameters, the TUNING STABILITY group with its offset strip, Easy headstock badges, the Workshop inspector figures and TS-01..TS-16; 37 of 41 rows are in progress there. Missing:
- Retune as a MIDI Learn action target
- performance-budget row
- section-19 doc row

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Existing drift/LFO/retune kept | 1 | yes | TuningEngine, CharacterEngine::retune | CHARACTER TUNERS | existing | |
| `stabilityCents` in StringTuning sum | 1 | no | rc TuningEngine | n/a | rc TS-01 | |
| Part fields tuners/nut/capo consumed (TuningHardware) | 1,5 | partial | HEAD: unconsumed (DECISIONS); rc PartAcoustics + setCapoHardware | n/a | - | |
| Settling (sigma, W, material factor) | 2.1 | no | rc StabilityModel | n/a | rc TS-08 | |
| Nut binding + seeded ping | 2.2 | no | rc | n/a | rc TS-02, TS-03 | |
| Tuner backlash (armed on downward approach) | 2.3 | no | rc | n/a | rc TS-04 | |
| Floating equilibrium | 2.4.1 | no | rc | n/a | rc TS-05 | |
| Creep tau 90 s | 2.4.2 | no | rc | n/a | rc TS-06 | |
| Whammy return error | 2.4.3 | no | rc | n/a | - | no dedicated test |
| Bend memory | 2.5 | no | rc | n/a | rc TS-07 | |
| Capo bias + capoComp | 2.6 | no | rc | n/a | rc TS-09 | |
| Clamp ±50 / ±200 c | 2 | no | rc | n/a | rc TS-16 | |
| Retune string n (clears all, clearDrift, retuneString) | 3 | no | rc requestRetune(mask) | rc offset strip click + Easy badge | rc TS-10 | |
| Retune all (low to high, feeds equilibrium) | 3 | no | rc requestRetuneAll | CHARACTER "Retune all" | rc TS-05, TS-10 | |
| Glide 250 ms ringing / snap silent | 3 | no | rc | n/a | rc TS-13 | |
| Auto-retune Off/Idle/Stop/Idle+Stop | 3 | no | rc AutoRetune | rc dropdown | rc TS-11 | |
| Atomic command channel | 3 | no | rc StabilityModel.h:134 | n/a | - | |
| Retune not undoable | 3 | no | rc | - | - | |
| Retune as MIDI Learn action target (CC≥64 rising edge) | 3 | no | - | **no** | - | |
| `stability_amount` | 4 | no | rc | rc TUNING STABILITY | rc TS-01 | |
| `stability_settling` | 4 | no | rc | rc | rc TS-08 | |
| `stability_nut_binding` | 4 | no | rc | rc | rc TS-02 | |
| `stability_backlash` | 4 | no | rc | rc | rc TS-04 | |
| `stability_saddle_creep` | 4 | no | rc | rc | rc TS-05/06 | |
| `stability_bend_memory` | 4 | no | rc | rc | rc TS-07 | |
| `stability_capo_bias` | 4 | no | rc | rc (+capo figure) | rc TS-09 | |
| `stability_auto_retune` | 4 | no | rc | rc | rc TS-11 | |
| onTuningChanged from preset/12-string/detune/fine-tune | 5 | no | rc (GuitarBodyComponent detune hook) | n/a | rc TS-04 | |
| Amount 0: advance returns at once | 5 | no | rc | n/a | rc TS-01 | |
| sigma commits only with transport stopped | 5 | no | rc | n/a | rc TS-12 | |
| TUNING STABILITY group replaces TUNERS (looseness, Retune all) | 6 | no | rc CharacterPanel | rc CHARACTER | rc RealismUiTests | |
| Per-string offset strip (colour + dot glyph, 10 Hz, greys) | 6 | no | rc OffsetStrip | rc | - | |
| Easy headstock popover "+3 c" + Retune | 6 | no | rc StabilityBadge in GuitarBodyComponent | rc Easy | - | |
| Workshop inspector figures (tuners, nut, capo) | 6 | no | rc WorkshopPanel describeTuningFigures | rc Workshop | - | |
| Section 19 row | 6 | no | - | - | - | doc |
| Session extras `"stability": {sigma, capo_comp}` | 7 | no | rc PluginProcessor.cpp:2066 | n/a | rc TS-15 | |
| Preset load recomputes sigma, clears capoComp | 7 | no | rc | n/a | rc TS-15 | |
| Budget 0.02 units, performance-budget row | 8 | partial | rc | n/a | rc TS-16 | spec row not added |
| Ramps limit per-block step to 0.5 c | 8 | no | rc | n/a | rc TS-13 | |
| TS-14 sample-rate independence | 9 | no | rc | - | rc TS-06/11 loops | |
| Deterministic ping draw | 2.2 | no | rc | n/a | rc TS-03 | |

### strum-dynamics.md

This is the only spec in group F that is on the integration branch, and it is largely done (24 yes, 4 partial, 1 no). `Rhythm/StrumGesture` does the planning, `StrumGroup` sits on RHYTHM, the strikers are mirrored in CHARACTER->PICK, and the Easy Feel knob works; `StrumGestureTests` has 25 tests. Gaps:
- **Chuck from a MIDI note in a chuck key range:** no.
- **Up vs down stroke:** only the force differs (kUpStrokeForce 0.85). There is no brighter/steeper down-stroke and no extra chirp or less click on the up-stroke.
- **`strum_evenness`:** rhythm-engine state with an unattached slider, so it is not automatable and not a mod target.
- **Live keyboard chords:** they ignore the striker (MidiInterpreter.cpp:565).

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Crossing velocity in sps | 1 | yes | StrumGesture::effectiveCrossingSps | RHYTHM->STRUM; Advanced knob | crossingTimingIsExact | |
| Source priority MPE > live spread > pattern > global | 1.1 | yes | RhythmEngine::getCrossingSource, MidiInterpreter | STRUM source label | crossingSourcesResolveInOrder, liveSpreadWins, mpePassesThrough | |
| Acceleration ease profile | 2 | yes | StrumGesture::ease | STRUM | accelerationChangesSpacingNotDuration | uses inverted smoothstep, recorded in DECISIONS |
| Down from low / up from high | 2.1 | yes | StrumGesture::plan | n/a | upIsFasterThanDownByTheRatio | |
| Up faster by up_velocity_ratio | 2.1 | yes | effectiveCrossingSps | STRUM | same | |
| Down steeper/brighter; up softer with more chirp, less click | 2.1 | partial | kUpStrokeForce 0.85 only | n/a | upStrokesAreSofter | no tone/noise difference |
| Tilt ramp ±, mirrored for up | 3 | yes | tiltFactor | STRUM | tiltAndAccentShapeTheForce | |
| Evenness ±40 %, deterministic | 3 | yes | evennessFactor | STRUM (unattached slider) | evennessBoundsTheVariation | not an APVTS param |
| Accent +1.5 dB on first two strings in direction | 3 | yes | StrumGesture accent | n/a | tiltAndAccentShapeTheForce | |
| Misses, 3x on leading string, seeded | 3.1 | yes | StrumGesture::plan | STRUM | missesAreWeightedAndDeterministic | live chords: misses off by design |
| RhythmHumanise::missPercent retained | 3.1 | yes | RhythmEngine | RHYTHM | - | |
| Guitar/bass default columns | 4 | yes | StrumSettings::guitarDefaults/bassDefaults/retargetDefaults | n/a | bassDefaultsApply | |
| Striker set (6) with crossing factors | 5 | yes | getStrikerCrossingFactor | STRUM + CHARACTER PICK mirror | strikersCrossAtTheirOwnSpeed | |
| Striker selects noise generator | 5 | yes | getStrikerMaterial | n/a | strikerSelectsTheNoise | |
| Separate down/up strikers | 5 | yes | strum_striker_down/up | STRUM + PICK mirror | - | |
| Striker applies to live keyboard strums | 5 | partial | MidiInterpreter.cpp:565 uses player's pick | n/a | - | by design comment; spec is ambiguous |
| Chuck: damp all strings (chuck_damping) | 6.1 | yes | StringEngine Damping::Chuck, triggerNote | STRUM | chuckKillsPitch | |
| chuck_amount blend | 6.1 | yes | StrumGesture chuck | STRUM | chuckStepsCarryTheChuck | |
| Chuck from pattern step type | 6.1 | yes | RhythmEngine | RHYTHM pattern editor | chuckStepsCarryTheChuck | |
| Chuck from MIDI note in chuck key range | 6.1 | **no** | - | no | - | |
| Palm mute vs chuck distinct | 6.2 | yes | TechniqueEngine / Chuck | n/a | - | |
| STRUM group on RHYTHM tab with all 9 controls | 6.3 | yes | UI/StrumGroup, RhythmPanel.cpp:603 | RHYTHM | theStrumGroupDrivesTheModel | |
| Easy Feel knob maps 60→400 sps, 0.45→0.95 | 6.3 | yes | StrumFeel, EasyPanel rhythmFeelSlider | Easy rhythm strip | feelMapsAsSpecified, theEasyFeelKnobScalesTheStrum | knob also scales humanise |
| `strum_crossing_sps` | 7 | yes | Parameters.cpp:751 | STRUM, Advanced | parametersReachTheEngine | |
| `strum_acceleration` / `up_velocity_ratio` / `tilt` / `miss_probability` | 7 | yes | Parameters.cpp:752-755 | STRUM | parametersReachTheEngine | |
| `strum_striker_down` / `_up` | 7 | yes | Parameters.cpp:756-757 | STRUM + PICK | same | |
| `chuck_amount` / `chuck_damping` | 7 | yes | Parameters.cpp:758-759 | STRUM | same | |
| `strum_evenness` as parameter | 7 | partial | RhythmEngine state (saved in rhythm state) | STRUM slider, not attached | olderPresetsKeepTheirStrumSpeed | not automatable/modulatable |
| Old strum_speed superseded but kept | 7 | yes | Parameters.cpp:575 | hidden | olderPresetsKeepTheirStrumSpeed | |

### Top gaps (group F)

1. **Harmonics are wrong on the integration branch.** Natural harmonics sound an octave high (they are voiced at the touch fret), they are band-passed sines, the harmonic's T60 is guessed (`t60Scale*=0.55`), and velocity picks the pinch partial. The fix exists only on realism-b.
2. **None of the six realism specs in this group are merged.** realism-b and realism-c sit off the integration branch, and merging them will conflict:
   - realism-a and realism-c both add `Source/UI/RealismGroups.{h,cpp}` and `Tests/RealismUiTests.cpp` (add/add).
   - realism-b and realism-c both edit StringEngine, LuthierEngine, Parameters and PhysicalRange `kNumEntries`.
   - realism-b's fretting style clashes with the techniques branch's `mute_fretting_style`.
3. **Every realism feature ships off.** The defaults are Legacy style, noise 0 and stability 0, and no factory preset uses the new behaviour. A buyer hears none of it without digging into CHARACTER.
4. **Fingerstyle is still one tool for all strings on the integration branch.** Pattern `p i m a` fingers are dropped, and the nail/flesh blend has a timbre step at 0.5 (realism-b fixes both).
5. **No string interaction on the integration branch.** There is no palm spread across strings, no neighbour damping, chord releases are gated on one sample, and there is no muted-string thump (realism-b).
6. **Missing illustration feedback on realism-b.** No contact ring for harmonics, no palm-width shading, no tool glyph. The engine data (`getContactDisplay`) exists, but no UI reads it.
7. **MIDI export is not extended.** There is no `touch_fret`/`partial` (HR) and no `tool`/`finger`/`stroke` (FA). HR-19 and the FA-17 export round trip are not tested.
8. **CPU budget tests are looser than the specs, and the specs were not amended:**
   - HR-18 allows 1.0 units against the spec's 0.05.
   - SI-13 allows 0.2 against 0.1.
   - NF-15 allows 0.02 against 0.005.
   - performance-budget.md has no NoiseFloor, StabilityModel or StringEngine-shape rows.
9. **Retune all is not a MIDI Learn action target** (tuning-stability 3), on any branch.
10. **Chuck by MIDI note (chuck key range) is not implemented** (strum-dynamics 6.1).
11. **Up and down strums differ only in force.** There is no brightness or chirp/click difference (strum-dynamics 2.1).
12. **The NF-02 hum calibration is outside its window** (-58 dB against -40 ±6), and the DECISIONS.md entry the spec requires is missing. Likewise, the three changed SUS thresholds and the node-efficiency formula change are not recorded in the spec or DECISIONS.
13. **SI-12's per-string sum at -80 dBFS is not asserted** on realism-b.
14. **Palm width is not mirrored in TECHNIQUES->MUTE, and the fretting-hand mute style is not a real parameter.** realism-b proxies it through `rh_style==Classical`.
15. **`strum_evenness` is not an APVTS parameter.** It cannot be automated or modulated.
16. **`noise_amp_buzz` is still labelled "Amp Buzz"** in the Advanced panel on realism-c. The spec wants the "Single-coil Hum" label.
17. **The section-19 rows (gui-integration.md) are missing** for all six new groups.

### Unspecified gaps noticed

- **No one-click "realism" preset tier or Easy-mode macro.** With every realism behaviour neutral by default, a buyer who does not know the CHARACTER tab will never hear sustain shape, noise floor, tuning instability or string interaction. Nothing in group F specifies a factory-content pass, only that one "may" come later.
- **No A/B or "Legacy vs Realistic" comparison toggle** for the new CHARACTER groups. The CHARACTER tab is also becoming very long: realism-a, b and c each append 2-3 groups to one scroll with no sub-tabs or collapse.
- **Harmonics have no on-screen trigger.** Natural, pinch, artificial and tapped harmonics are reachable only by CC 72/73/103/104 or velocity. Mouse or keyboard users with no CC controller have no way to play them, and there are no keyswitch defaults.
- **No noise gate is paired with the new amp hiss.** The spec says players "reach for a gate" at high gain; check whether EffectsChain offers one on the Easy tone strip.

### Small glue candidates

- **`strum_evenness`:** promote it to an APVTS float (0-1, default 0.75) mirrored into `RhythmEngine::setStrumEvenness`, and attach it to the existing `StrumGroup` evenness slider on RHYTHM->STRUM (integration branch). Kits keep writing it through the parameter.
- **`noise_amp_buzz`:** relabel `AdvancedPanel.cpp:740 addKnob (ampBuzz, "Amp Buzz", …)` to "Single-coil Hum" (realism-c).
- **`palm_mute_spread`:** add a mirror slider to the TECHNIQUES->MUTE group (`UI/MuteGroup.cpp`, techniques branch) once realism-b merges.
- **`mute_fretting_style` (techniques):** have realism-b's `interaction.frettingStyle` (Parameters.cpp:1370) read it instead of `rh_style==Classical`, and mirror it in CHARACTER->STRING INTERACTION.
- **`LuthierEngine::getContactDisplay(s)` (realism-b):** have `FretboardComponent` draw the hollow ring, dashed when `missed`, with alpha from `life`. The data is already published per string.
- **Right-hand tool glyphs:** the per-string tool is resolved from `rh_string_tool_1..6`, so the illustration can draw the glyph at `pluck_position` (+`thumb_position_offset` for thumb-class tools) on the Guitar illustration.
- **Retune all:** register `StabilityModel::requestRetuneAll()` as a MIDI Learn action (fire on a rising edge at CC ≥ 64), next to the existing learn targets.
- **Chuck key range:** a MIDI note in a chuck range could set `NoteOnEvent::chuck = 1` using the existing `chuck_damping`. This needs a range parameter or a fixed key; the engine path already exists.
- **performance-budget.md rows:** add NoiseFloor (0.15), StabilityModel (0.02), the StringEngine shape (+0.3), contacts and the string-interaction air path. Amend HR-18/SI-13/NF-15 to the budgets actually measured.
- **Section 19 rows:** add HARMONICS, RIGHT HAND, STRING INTERACTION, NOISE FLOOR, SUSTAIN SHAPE and TUNING STABILITY rows to gui-integration.md. Docs only.

---

## Group G



Audited: working tree at HEAD `51e40a3` (integration `claude/luthier-cloud-session-5lzlix` plus the audit commits). Helper branches were inspected with `git show`. The `techniques` branch carries most of this group's work (TECHNIQUES tab, Tap/Mute/Bend engines, CascadeResolver, slide controls). It is **not merged**, and it is **behind** the integration branch (the integration tip is not an ancestor of it). The `visual` branch carries the QA, perf, installer and CI work. Legend: "branch:X" means the item is in progress on `origin/claude/luthier-X`. "WIP" means `Source/WIP` (not compiled). Test names are `Suite.test`.

Cross-cutting fact: `docs/audit/BETA_TEST_REPORT.md` B-11 confirms that on HEAD, **39 technique parameters (`scrape_*` ×14, `slap_*`/`pop_*`/`ghost_*`/`double_thump_*` ×25) have no control anywhere**. `GuiReach.everyAutomatableParameterHasAVisibleControl` fails for them.

---

### string-scraping.md

On HEAD the scrape **engine is complete and well tested**: a per-winding catch model, all 14 `scrape_*` params, keyswitch/CC/MPE triggers and cascade hooks. It is **not playable from the GUI**. No control is attached to any `scrape_*` param, and there is no on-screen trigger button. The only exception is `pick_scrape_amount`, which sits in CHARACTER > noise. The five section-4 presets exist only as `ScrapeSettings::fromPreset`, which only the tests call. No user can load them, on HEAD or on any branch. The techniques branch adds the SCRAPE sub-tab, a "Scrape now" button and an Easy pill. Counts: yes 17 / partial 6 / no 1.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Per-winding impulses, catches = distance × windings_per_mm | 0.1, 1 | yes | DSP/Noise/ScrapeEngine: `windingsPerMm`, `processBlock` | n/a | Scrape.catchesComeAtWindingsPerMmTimesSpeed | no convolution shortcut |
| Plain strings near-silent (emergent) | 0.2 | yes | ScrapeEngine (zero windings) | n/a | Scrape.aPlainStringIsNearSilent; ScrapeEngineWiring.thePlainHighEIsThirtyDecibelsUnderTheWoundLowE | |
| Fast vs slow scrape character | 0.3 | yes | ScrapeEngine | n/a | Scrape.theFactoryScrapesAreWhatSectionFourSays (zipper > 5× ratchet rate) | |
| Catch amplitude ∝ pressure; deeper catch adds pitch load | 1 | yes | ScrapeEngine (pressure cents) | n/a | Scrape.doublingPressureDoublesEachCatch; ScrapeEngineWiring.harderScrapingLoadsThePitchSlightly | |
| ScrapeGesture struct (string, start, end, duration, pressure, tool, angle) | 1 | yes | ScrapeEngine.h `ScrapeSettings`/gesture | n/a | | |
| Trigger: keyswitch / CC / MPE zone / on-screen button | 0.4, 2 | partial | `scrape_trigger` (KS 12, `scrape_trigger_cc`=85, MPE zone, Button Only); `ScrapeEngine::requestTrigger` | no (HEAD); branch:techniques SCRAPE "Scrape now" + Easy pill | Scrape.triggersListenOnlyWhereTheyAreTold; ScrapeEngineWiring.theKeyswitchScrapesAndNeverPlaysANote | "Button Only" is useless on HEAD because no button exists |
| Direction: B→N, N→B, Hold+Sweep | 2 | yes | `scrape_direction` | no (branch:techniques) | Scrape.aReversedScrapeIsTheForwardOneMirrored | |
| Sweep source: auto/modwheel/expression/aftertouch/custom CC | 2 | yes | `scrape_sweep_source`, `scrape_sweep_cc` | no (branch:techniques) | Scrape.aModwheelSweepCatchesWithinOneBlock | |
| Sweep range start/end, default 200-900 mm | 2 | yes | `scrape_start_mm`, `scrape_end_mm` | no (branch:techniques) | | |
| Duration (preset field) | 4 | yes | `scrape_duration` (600 ms) | no (branch:techniques) | | |
| Pressure, default 0.5 | 2 | yes | `scrape_pressure` | no (branch:techniques) | | |
| Tool pick/nail/thumb, default pick | 2 | yes | `scrape_tool` | no (branch:techniques) | | |
| Angle, default 20° | 2 | yes | `scrape_angle` | no (branch:techniques) | | |
| String mask, default wound strings only | 2 | partial | `scrape_string_mask` default 0 = "follow held strings" | no (branch:techniques StringMaskSelector) | | default differs from spec |
| Retrigger threshold, default 200 ms, silent drop | 2, 6 | yes | `scrape_retrigger` | no (branch:techniques) | Scrape.aRetriggerInsideTheThresholdIsDropped | |
| Scrape level | (C-29) | yes | `pick_scrape_amount` → `scrape.level` | CHARACTER > NoiseGroups "Scrape" knob | Scrape.theLevelTrimScalesAndZeroIsFree | |
| ScrapeEngine trigger/processBlock/reset, zero idle cost | 3 | yes | ScrapeEngine | n/a | Scrape.idleCostsNothing; Scrape.resetRepeatsExactly | |
| Pipeline: after TechniqueEngine, before StringEngine, sample-offset impulses | 3 | yes | LuthierEngine.cpp ~1566-1627 | n/a | | |
| Presets: Classic Rock, Metal Zipper, Slow Ratchet, Nail, Modwheel-Sweep | 4 | partial | `ScrapeSettings::fromPreset` (called only by tests) | no | Scrape.theFactoryScrapesAreWhatSectionFourSays | not in the factory list or browser on any branch |
| Cascade: mute dulls the scrape | 5 | yes | `ScrapeEngine::setMuteAmount` | n/a | Scrape.aMuteMakesItDullerAndThumpier | |
| Cascade: slide takes the string; bend shifts winding spacing | 5 | yes | `setStringBlocked` (slide), bend stretch | n/a | | no dedicated bend-during-scrape test |
| Cascade: a tap on the string damps the scrape | 5 | partial | preempt hooks exist; TapEngine only on branch:techniques | n/a | branch: Cascade.aTapPreemptsAScrapeOnItsString | |
| CPU idle < 0.05%, active < 0.5% | 6 | yes | | n/a | Scrape.idleCostsNothing; Scrape.anActiveScrapeStaysInBudget | |
| GUI: Techniques tab | 2 | no | none on HEAD | branch:techniques TECHNIQUES > SCRAPE | branch: TechniquesUi.everySubTabRendersItsControls | B-11 |

### slide-technique-controls.md

On HEAD **nothing from this spec exists**. Only the base `SlideEngine` from slide-guitar.md is present: Slide Mode, bar material, slant, pressure and the SlideGroup. Every control in this spec is **in progress on the techniques branch**: `SlideControlSettings`, `SlideEngine::advanceControls/triggerGesture`, 20-odd `slide_pos_*`/`slide_gesture_*` params, the SLIDE sub-tab, a pill popover, 4 presets and `SlideControls.*` tests. The branch deviates on two points. The Easy "…" is the SLIDE pill's hold popover, not a glyph. The Advanced mirror sits at the foot of CHARACTER, not in the Col 3 SLIDE group. Counts on HEAD: yes 1 / partial 0 / no 17. On the branch almost all are yes.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Base slide physics and Slide Mode unchanged | 0.1, 4 | yes | DSP/Slide/SlideEngine; UI/SlideGroup | Advanced SLIDE group | Slide.*, SlideUi.* | |
| Position source: modwheel/PB/MPE Y/expr/CC/fretboard drag | 1 | no | branch:techniques `slide_pos_source`, `slide_pos_cc`, `TechniqueOverlay::handleMouseDrag` | branch: TECHNIQUES > SLIDE | branch: SlideControls.theModWheelDrivesThePosition | |
| Position mode absolute/relative | 1 | no | branch `slide_pos_mode`, `slide_pos_range` | branch | same | |
| Slant control, source selectable | 1 | no | branch `slide_slant_source`/`_cc` | branch | SlideControls.everyControlRoundTrips | |
| Pressure control, source selectable | 1 | no | branch `slide_pressure_source`/`_cc` | branch | same | |
| Contact string mask (all / bass 3 / treble 3) | 1 | no | branch `slide_contact`, `SlideEngine::contactsString` | branch | SlideControls.aBassOnlyBarLeavesTheTrebleFree | |
| Speed limit 4800 c/s, advanced higher | 1 | no | branch `slide_speed_limit` + PhysicalRange row | branch | SlideControls.theSpeedLimitClampsAJump | |
| Auto-vibrato after 300 ms hold (depth, rate), off by default | 1 | no | branch `slide_auto_vibrato`/`_depth`/`_rate` | branch | SlideControls.autoVibratoEngagesAfterAHold | |
| Gesture trigger keyswitch/CC | 1 | no | branch KS 21, `slide_gesture_trigger`/`_cc` | branch | SlideControls.aScriptedGestureArrivesOnTime | |
| SlideGesture struct (from, to, duration, curve, slant, pressure) | 2 | no | branch `SlideGesture`, `slide_gesture_*` | branch | same | triggering from tune-builder / MIDI meta not seen |
| SlideEngine::setPositionSource / triggerGesture / setSpeedLimit | 3 | no | branch SlideEngine | n/a | SlideControls.* | |
| PB as slide position (integration with pitch bend) | 0.4 | no | branch (PB source) | branch | | |
| GUI Slide sub-tab | 4 | no | branch `SlidePage` | branch | TechniquesUi.everySubTabRendersItsControls | |
| Easy Playing strip slide glyph "…" popover | 4 | no | branch: SLIDE pill hold popover | branch (Easy pill row) | TechniquesUi.holdOpensThePopoverAndEscapeClosesIt | deviation, recorded in branch decisions |
| Advanced Col 3 SLIDE group expandable section | 4 | no | branch `TechniqueMirrors` at foot of CHARACTER | branch (CHARACTER) | TechniquesUi.theCharacterMirrorsAttachTheSameParameters | wrong location vs spec |
| Presets: Standard (MW), Pitch-Bend, Lap Steel Full, Auto-Vibrato Hold | 6 | no | branch `TechniquePresets.cpp` | branch browser "Techniques" | TechniquesUi.thePresetChipFilters | |
| Source swap crossfade 10 ms | 7 | no | branch `advanceControls` smoothing | n/a | SlideControls.swappingTheSourceGlides | |
| Preset round-trip of every added control | 7 | no | branch | n/a | SlideControls.everyControlRoundTrips | |

### string-slap-technique.md

On HEAD the **SlapEngine is complete**: 4 types, 5 trigger sources, ghost, rebound, snap-back and body tap. The slap-buzz comes from the FretBuzz generator. About 20 tests cover it. **None of its 25 params has a control on HEAD** (B-11). The `model-gaps` branch adds a bass-only SlapGroup (strength, position, pop, ghost, double thump). The `techniques` branch adds the full SLAP sub-tab and a "Slap now" button. Body Tap uses three fixed resonators per part inside SlapEngine instead of body-coupling's mode bank. `double_thump_enabled` defaults to off, but the spec says the rebound is on by default. The five presets are engine-only (`SlapSettings::fromPreset`), just like the scrape presets. Counts: yes 17 / partial 7 / no 1.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Slap is a strike plus a slap-buzz event | 0.2 | yes | SlapEngine → FretBuzz generator | n/a | Slap.theClackIsTheFretBuzzGenerator; SlapWiring.theClackComesFromTheBuzzGenerator | |
| Works on any guitar, not bass-only | 0.4 | yes | SlapEngine | n/a | SlapWiring.thePlainHighEIsAudibleButClacksLess | |
| Slap type: Thumb/Pop/Palm/Body Tap | 1 | yes | `slap_type` | no (branch:techniques SLAP) | Slap.whatANoteBecomes | |
| Trigger: velocity zone/KS/CC/MPE zone/strip button | 1 | partial | `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`; KS 15-18 | no; branch:techniques "Slap now" + Easy pill | TechniqueTriggers.*; Slap.theButtonAndTheKeyswitchesQueueTheirStrikes | no on-screen button on HEAD |
| Contact position defaults 60 / 40 / 100 mm | 1 | yes | `slap_position_mm`=60, `pop_position_mm`=40, `slap_palm_position_mm`=100 | no (model-gaps SlapGroup for thumb/pop; techniques all) | Slap.theContactPointIsMeasuredFromTheLastFret | |
| Contact force 0-1, default 0.6 | 1 | yes | `slap_force` | no (branch:techniques) | | |
| String mask: bass low E/A, palm all | 1 | yes | `slap_string_mask` (0 = per-type default) | no (branch:techniques) | Slap.theFactorySlapsAreWhatSectionFourSays (effectiveMask) | |
| Ghost mode + modifier KS / dedicated CC | 1 | yes | `slap_ghost_mode`, `slap_ghost_cc`=87, KS 16 | no (branch:techniques) | SlapWiring.aGhostIsAThumpWithNoPitch | |
| Rebound on/off, gap default 60 ms, on by default for thumb | 1 | partial | `double_thump_enabled` (**default false**), `slap_rebound_gap` | no (model-gaps/techniques) | SlapWiring.theDoubleThumpComesBackAtItsGap | default contradicts spec |
| Snap-back (bass only) | 1 | yes | `slap_snap_back` | no (branch:techniques) | | |
| Body tap resonance top/side/back via body-coupling mode bank | 1, 2 | partial | `slap_body_part`; SlapEngine.cpp `knockFor` (3 fixed resonators) | no (branch:techniques) | Slap.theBodyPartWeightsTheKnock | does not drive BodyCoupling modes (no `driveDirect`) |
| SlapEngine trigger/processBlock/reset; after TechniqueEngine | 2 | yes | DSP/Slap/SlapEngine; LuthierEngine ~1570 | n/a | Slap.idleAndActiveStayInBudget | |
| Body tap bypasses StringEngine | 2 | yes | SlapEngine | n/a | SlapWiring.aBodyTapLeavesTheStringsAlone | |
| Shares engine with bass-techniques; old bass presets keep working | 3 | yes | `slap_strength` etc. read by same engine | model-gaps SlapGroup (bass, unmerged) | branch: TechniqueLayer.bassSlapPresetsStillReachTheSlapEngine | |
| Presets: Bass Std, Bass Aggressive, Funk Palm, Acoustic Body Tap, Perc. Fingerstyle | 4 | partial | `SlapSettings::fromPreset` (called only by tests) | no | Slap.theFactorySlapsAreWhatSectionFourSays | not loadable anywhere |
| Cascade: mute-then-slap, bend after slap | 5 | partial | MuteEngine/BendEngine on branch only | n/a | branch: Muting.aSlappedNoteCarriesItsMute, Bend.aBentSlapPitchesCorrectly | |
| Cascade: slide conflict, scrape conflict | 5 | yes | `slap.classify(e, slide.isUnderBar)`, preempt | n/a | SlapWiring.slapAndScrapeTakeTheStringFromEachOther | |
| Cascade: tap alternate | 5 | partial | branch:techniques CascadeResolver | n/a | branch: Cascade.everyPairResolvesAsDocumented | |
| GUI: Techniques > Slap sub-tab | 6 | no | none on HEAD | branch:techniques `SlapPage` | TechniquesUi.everySubTabRendersItsControls | B-11 |
| Playing strip Tool selector gains Slap / Pop | 6 | partial | branch:realism-b `RhTool {…slap, pop}` StringToolCell | branch:realism-b RightHandGroup | branch: FingerstyleAttackTests | wiring from RhTool::slap to SlapEngine not verified |
| Test: thumb slap within 1 dB of the bass-techniques reference | 7 | partial | | | SlapWiring.aThumbSlapIsTheSameHoweverItIsFired | compares trigger paths, not a reference |
| Test: palm slap < -25 dB pitched | 7 | yes | | | SlapWiring.aPalmSlapIsBroadbandAndPitchless | |
| Tests: body tap, ghost, rebound ±3 ms, plain string | 7 | yes | | | SlapWiring.* | |
| CPU idle < 0.05%, active < 0.6% | 7 | yes | | | Slap.idleAndActiveStayInBudget | |
| Preset round-trips every field | 7 | yes | | | SlapPresets.everySlapFieldRoundTrips | |

### muting-rhythm.md

On HEAD this spec **does not exist beyond WIP**. `Source/WIP/Rhythm/Muting.*`, `WIP/UI/MuteGroup.*` and `WIP/Tests/MutingTests.cpp` are not compiled. The only muting on HEAD is older: the PalmMute/MutedPick keyswitch articulation (MidiInterpreter) and the strum-dynamics chuck (`chuck_amount`/`chuck_damping`). The techniques branch moves Muting out of WIP and adds `MuteEngine`, `StringEngine` Muted damping, `mute_type` per pattern step, the live 16-step grid, the MUTE sub-tab, an Easy 4-way button, a RHYTHM Mute Row and 6 presets. MIDI export of mute types is deferred on the branch too. Counts on HEAD: yes 0 / partial 3 / no 17.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| 7 mute types with T60 targets and position/pressure defaults | 1 | partial (WIP, not built) | WIP/Rhythm/Muting; branch `MuteType`, `Muting::dampingFor` | branch MUTE page | branch: Muting.eachTypeDampsAsDescribed | HEAD only has the PalmMute/MutedPick articulation |
| Muted strike produces thump + damped ring | 0.2 | partial | HEAD palm-mute articulation; branch `StringEngine` Damping::Muted | | branch: Muting.palmMuteHeavyOnLowEDecaysIn40To60Ms | |
| `mute_type` per pattern step; JSON extension; missing = open | 2 | no | branch `RhythmPattern::getMuteStep/setMuteStep`, toVar/fromVar | branch RHYTHM Mute Row | Muting.theMuteGridsRoundTrip; Muting.existingPatternsPlayIdentically | |
| Live 16-step Mute Grid synced to host tempo | 2 | no | branch `MuteEngine` live grid | branch MUTE page (not a Col 4 overlay) | Muting.paintingTheLiveGridAppliesWithinABar | |
| Master mute mode overrides | 3 | no | branch `mute_master_mode` | branch MUTE + Easy button | Muting.theMasterModeOverridesEverything | |
| Palm position 35 mm / pressure 0.5 | 3 | no | branch `mute_palm_position`/`_pressure` | branch | Muting.eachTypeDampsAsDescribed | |
| Fretting-hand mute style rock/classical | 3 | no | branch `MuteEngine::deadensOtherStrings` | branch | Muting.rockSpreadDeadensTheStringsAMutedStrumMisses | realism-b has palm_mute_spread / adjacent_mute_amount |
| Chuka source (strum dyn < 0.3) | 3 | partial | HEAD `chuck_amount` (strum-dynamics); branch `Muting::resolve` | HEAD: strum group chuck knobs | Muting.aSoftStrumIsAChuka | |
| Random humanise 0-1 | 3 | no | branch `Muting::humanise` | branch | Muting.humaniseShiftsAboutHalfTheEligibleSteps | |
| Ghost velocity range 0.4 | 3 | no | branch | branch | Muting.aGhostNoteHasNoPitchedContent | |
| RhythmEngine passes mute with each note-on | 4 | no | branch `RhythmEngine::emitNote` pendingMute | n/a | Muting.aPatternsMuteRowReachesItsNotes | |
| StringEngine applies initial damping + post-strike release | 4 | no | branch `techniqueStrike` | n/a | Muting.aFretMuteRingsThenStops | |
| Cascade: stacks with everything | 5 | no | branch CascadeResolver | n/a | Muting.aMuteIsStampedOnAnyTechnique | |
| Presets ×6 (Metal Chug 16ths…Classical Staccato) | 6 | no | branch TechniquePresets.cpp | branch | TechniquesUi.thePresetChipFilters | |
| GUI Techniques > Muting sub-tab | 7 | no | branch `MutePage`/`MuteGroup` | branch | TechniquesUi.everySubTabRendersItsControls | |
| Easy Mute button 4-way | 7 | no | branch `EasyMuteButton` | branch Easy pill row | Muting.theEasyMuteButtonCyclesFourWays | |
| RHYTHM tab Mute row | 7 | no | branch RhythmPanel `muteRow` | branch | Muting.theMuteControlsDriveTheModel | |
| MIDI export (Luthier SysEx NOTE mute_type; generic text meta) | 8 | no | none (deferred on branch too) | n/a | none | |
| Test: cascade with slap carries mute_type | 9 | no | branch | | Muting.aSlappedNoteCarriesItsMute | |
| Test: preset save/restore round-trips grid | 9 | no | branch | | Muting.theMuteGridsRoundTrip | |

### two-hand-tapping.md

On HEAD **no tapping exists**. There is no TapEngine, no `tap_*` params and no tap trigger. The only related piece is the legato hammer-on/pull-off promotion in `TechniqueEngine`: a `legato_window` knob (default 40 ms, not 150) and a velocity threshold hard-coded at 0.63 (MIDI 80, not 40). It is not user-set. The techniques branch implements the spec fully: TapEngine, 9 params, the TAP page, pill and CHARACTER mirror, fretboard square markers, 5 presets and `Tap.*` tests. The branch has two gaps: MIDI export of taps is deferred, and Eight-Finger ships at 4 taps per string. Counts on HEAD: yes 0 / partial 1 / no 18.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| TapGesture struct | 1 | no | branch `TapEngine.h` | n/a | branch: Tap.fretSnapOffTapsBetweenTheFrets | fret is continuous on branch |
| Tap-on / hold (movable capo) / tap-off | 2 | no | branch `TapEngine`, `LuthierEngine::playTapEvent` | n/a | Tap.aTapIsAMovableCapoAndLiftsBackToTheFrettedNote | |
| Pull-off lateral flick | 2 | no | branch `TapEngine::tapOff` | | Tap.thePullOffFlickIsTheReleaseTransient | |
| Multi-finger taps = capos in series | 2 | no | branch `soundingFret` | | Tap.twoTapsOnAStringAreCaposInSeries | |
| Trigger: MIDI ch 2 / KS / fretboard tap layer | 3 | no | branch `tap_source`, `tap_channel`, KS 19, TechniqueOverlay | branch TAP page + fretboard | Tap.theTriggersTakeTheirNotes; TechniquesUi.theFretboardTapsWhenTheTapLayerIsOn | |
| Strength curve | 3 | no | branch `tap_strength_curve` (bipolar exponent) | branch | Tap.theStrengthCurveShapesVelocity | |
| Auto pull-off default on | 3 | no | branch `tap_auto_pull_off` | branch | | |
| LH hammer-on threshold default 40 | 3, 5 | partial | HEAD `TechniqueEngine::setLegatoVelocityThreshold` fixed 0.63; branch `tap_hammer_threshold` | HEAD: no; branch | Tap.softNotesCloseTogetherAreHammerOns | HEAD window is 40 ms, spec says 150 ms |
| Lateral flick 0.5, duration 200 ms, max concurrent 2 (adv 8), fret snap on | 3 | no | branch `tap_flick`, `tap_duration`, `tap_max_concurrent`, `tap_fret_snap` | branch | Tap.everyControlRoundTrips | |
| TapEngine trigger/release/processBlock/reset | 4 | no | branch | | Tap.* | |
| Left-hand legato promotion routed through TapEngine (150 ms) | 5 | no | branch hammer window | | Tap.softNotesCloseTogetherAreHammerOns | |
| GUI Tapping sub-tab | 6 | no | branch `TapPage` | branch | TechniquesUi.everySubTabRendersItsControls | |
| Easy Tap pill | 6 | no | branch TechniquePillRow | branch | TechniquesUi.thePillsArmOnAClick | |
| CHARACTER Right Hand "Tapping" section | 6 | no | branch TechniqueMirrors (CHARACTER foot) | branch | TechniquesUi.theCharacterMirrorsAttachTheSameParameters | realism-b adds RightHandGroup; mirror not placed in it |
| Fretboard square tap markers, 100 ms fade | 6 | no | branch TechniqueOverlay | branch | TechniquesUi.theOverlaysDrawInsideTheirBudget | |
| Cascade (slide conflict, slap alternate, scrape conflict) | 7 | no | branch CascadeResolver | | Tap.theSlideBarHoldsItsStrings | |
| Presets ×5 | 8 | no | branch TechniquePresets | branch | TechniquesUi.thePresetChipFilters | Eight-Finger at 4 |
| MIDI export of taps (SysEx / generic ch 2) | 9 | no | none (deferred) | | none | |
| CPU idle < 0.05%, busy < 0.8% | 10 | no | branch | | Tap.cpuStaysInBudget | |

### microtonal-bends.md

On HEAD bends are the **old global model**: `bend_range` in semitones (1-48, default 2), MPE per-note pitch bend (`mpe_enabled`), and `vibrato_rate`/`vibrato_depth`/`vibrato_shape`, all in Advanced PERFORMANCE. There is no per-string ranges, quantise, scale loading, pre-bend or bend curves. The techniques branch implements `BendEngine`, `MicrotonalScale` (Scala/.tun), about 20 `bend_*` params, the BEND page, popover, mirror and fretboard arc/badge. It defers the ModMatrix `PreBendEvent` source and MIDI export. It also creates a **second vibrato system** alongside HEAD's `vibrato_*`. Counts on HEAD: yes 0 / partial 4 / no 16.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Continuous per-string pitch | 0.1 | partial | HEAD MPE per-note bend | Advanced PERFORMANCE (MPE, Bend Range) | | |
| Source stack: global + per-string + vibrato + pre-bend + slide | 1 | no | branch `BendEngine::centsFor` | | Bend.theGlobalBendMovesEveryString | |
| Global bend source PB/expression/CC | 2 | partial | HEAD PB only; branch `bend_global_source`/`_cc` | branch BEND | same | aftertouch / fretboard drag not offered as bend sources |
| Global range cents, default 200, advanced to 2400 | 2 | partial | HEAD `bend_range` (st, 1-48); branch `bend_global_range` | HEAD Advanced PERFORMANCE; branch | Bend.everyControlRoundTrips | |
| Per-string source (MPE Y / CC per string / none) | 2 | partial | HEAD MPE PB; branch `bend_string_source`/`_cc` | branch | Bend.aPerStringBendMovesOnlyItsString | |
| Per-string ranges ×6, default 200 | 2 | no | branch `bend_string_range_1..6` | branch | same | |
| Vibrato source LFO/AT/MPE Z; rate 6; depth 20; onset 200 ms | 2 | no | branch `bend_vibrato_*` | branch | Bend.vibratoWaitsForItsOnsetThenReachesDepthIn50Ms | HEAD `vibrato_rate` 5.2/22 c has no source or onset |
| Quantise none/quarter/semi/24-EDO/22/31/53/custom + snap | 2-3 | no | branch `bend_quantise`, `bend_snap` | branch | Bend.quarterToneQuantiseLandsOnTheGrid | branch inverts snap meaning (majority rule) |
| Custom .scl/.tun loading | 2-3 | no | branch `MicrotonalScale`, BendPage::loadScale | branch | Bend.aLoadedScaleIsTheGrid | |
| Pre-bend KS/CC, amount -200 c | 2 | no | branch KS 20, `bend_pre_bend_*` | branch | Bend.aPreBendStartsFlatAndReleases | |
| Bend curve / release curve (linear/exp/drawn) | 2 | no | branch `bend_curve`, `bend_release_curve`, BendCurveEditor | branch | Bend.theCurvesShapeTheThrow | |
| Range change at next note-on | 4 | no | branch | | Bend.aRangeChangeWaitsForTheNextNote | |
| ModMatrix `PreBendEvent` source class | 4 | no | none (deferred on branch) | | none | |
| GUI Microtonal Bends sub-tab | 5 | no | branch BendPage | branch | TechniquesUi.* | |
| Easy bend/vibrato "…" popover | 5 | no | branch BEND pill popover | branch | | |
| CHARACTER PLAYING "Microtonal" section | 5 | no | branch TechniqueMirrors (CHARACTER foot) | branch | | |
| Fretboard cents badge + dot pushed along the fret gap | 5 | no | branch TechniqueOverlay bendArc | branch | | |
| Cascade (compatible with all) | 6 | no | branch | | Bend.aBentSlapPitchesCorrectly | |
| Presets ×5 | 7 | no | branch | branch | TechniquesUi.thePresetChipFilters | |
| MIDI export per-source / MPE split | 8 | no | none (deferred) | | none | |

### technique-cascade.md

On HEAD there is **ad-hoc pairwise cascade** between scrape, slap and slide only: `preempt`, `setStringBlocked` and `slap.classify(isUnderBar)`, with a 10 ms scrape fade. There is no CascadeResolver, no matrix, no conflict UI and no combined presets. The techniques branch implements `CascadeResolver`, the priority rules, the CASCADE page, red-slash pills, 6 combined presets and a 10k-gesture fuzz. The "reference within 0.5 dB" test is weakened on the branch to a comparison of two renders. Counts on HEAD: yes 1 / partial 3 / no 8.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Class table + same-string matrix | 1-2 | partial | HEAD scrape/slap/slide pairs; branch `CascadeResolver::relation` | | branch: Cascade.theMatrixIsTheSpecs | |
| Priority rules (user > auto, most recent, slide holds, mute/bend always) | 3 | partial | HEAD slide blocks scrape/slap; branch `CascadeResolver::request` | | Cascade.thePriorityRulesDecide | |
| Graceful preemption 10 ms crossfade | 3.3 | yes | ScrapeEngine/SlapEngine `preempt` | | Scrape.aPreemptedScrapeFadesOutInTenMilliseconds | spectral-discontinuity criterion not measured |
| Cascade schedule order (7 stages) | 4 | partial | HEAD fixed call order in LuthierEngine; branch `TechniqueLayer::stages` | | TechniqueLayer.theModulesRunInTheDocumentedOrder | |
| Combined presets ×6 | 5 | no | branch TechniquePresets | branch browser | Cascade.theCombinedPresetsArmTheirTechniques | |
| Conflict indicator red slash + tooltip text | 6 | no | branch TechniquePill, `conflictMessage` | branch pills | Cascade.aConflictShowsOnThePillWithinAFrame | |
| Pairwise same/different-string tests | 7, 10 | no | branch | | Cascade.everyPairResolvesAsDocumented; techniquesOnDifferentStringsAreIndependent | HEAD: SlapWiring.slapAndScrapeTakeTheStringFromEachOther only |
| Combined-preset render within 0.5 dB of saved reference | 7, 10 | no | branch compares two renders | | Cascade.theCombinedPresetsArmTheirTechniques | no stored reference |
| Fuzz 10 000 gestures | 7 | no | branch | | Cascade.aFuzzOfGesturesLeavesNothingBehind | |
| Cascade decisions push no undo; arming does | 8 | no | branch TechniqueUndo | | TechniquesUi.theUndoClassesGroupAsSpecified | |
| All six active ≤ 2.5% CPU; body tap ≤ 1% | 9 | no | untested | | none | |

### engine-technique-layer.md

HEAD has 2 of the 5 modules (ScrapeEngine, SlapEngine), `TechniqueTriggers` with KS 12-18, and 39 params. TapEngine, MuteEngine, CascadeResolver, `TechniqueLayer`, undo classes and migration resets are on the techniques branch. Four items are missing everywhere: BodyCoupling `driveDirect`, ModMatrix `PreBendEvent` and the per-string range factor, APVTS parameter groups `parameters/techniques/<t>/*`, and the low-CPU banner. Counts on HEAD: yes 5 / partial 4 / no 11.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| ScrapeEngine, SlapEngine modules | 1 | yes | DSP/Noise/ScrapeEngine, DSP/Slap/SlapEngine | | Scrape.*, Slap.* | |
| TapEngine, MuteEngine, CascadeResolver | 1 | no | branch DSP/Techniques/* | | Tap.*, Muting.*, Cascade.* | |
| Pipeline 2b/2c insertion | 2 | partial | HEAD scrape/slap only; branch `LuthierEngineTechniques.cpp` | | TechniqueLayer.theModulesRunInTheDocumentedOrder | |
| Zero-cost idle (flag check), no allocs, reset() | 0.2-0.3 | yes | Scrape/Slap | | Scrape.idleCostsNothing | |
| TechniqueEngine gesture types scrape/slap/tap/body-tap; emit() | 3.1 | partial | HEAD `TechniqueId {scrape, slap}`; branch adds tap/pre-bend/slide | | TechniqueTriggers.* | |
| StringEngine N concurrent contact points | 3.2 | no | branch `TapEngine::soundingFret` | | Tap.twoTapsOnAStringAreCaposInSeries | |
| RhythmEngine mute_type | 3.3 | no | branch | | Muting.aPatternsMuteRowReachesItsNotes | |
| BodyCoupling `driveDirect(impulse, position)` for body tap | 3.4 | no | none; SlapEngine internal knock resonators | | Slap.theBodyPartWeightsTheKnock | not done on any branch |
| ModMatrix PreBendEvent + microtonal range factor | 3.5 | no | none (deferred on branch) | | | |
| MidiInterpreter new KS range (no conflict) | 3.6 | yes | TechniqueTriggers.h `TechniqueKeyswitch` 12-18 (branch 19-21) | | TechniqueTriggers.keyswitchesAreTakenAndBecomeEvents | |
| MPE Y/Z routing to slide / bends | 3.6 | no | branch | | | |
| Commands Arm/SetParam/TriggerGesture/LoadMuteGrid; TechniqueFiredResult | 4 | partial | HEAD atomics + `requestTrigger`, fired counts; branch lock-free queue | | TechniqueLayer.commandsDoNotAllocate | no formal command types |
| 6 arm booleans + ~60 params | 5 | partial | HEAD `scrape_armed`, `slap_armed` +37; branch +64 | HEAD: none visible | Integration param count | |
| APVTS groups `parameters/techniques/<t>/*` | 5 | no | flat layout (no AudioProcessorParameterGroup anywhere) | | | hosts show a flat list of about 700 params |
| Pre-delta presets load disarmed/default | 6 | yes | params default off | | branch: TechniqueLayer.preDeltaPresetsLoadDisarmedAndIdle | |
| Old bass-slap presets map to SlapEngine | 6 | yes | same engine and params | | branch: TechniqueLayer.bassSlapPresetsStillReachTheSlapEngine | |
| Technique state in PRESET; snapshot captures arm state | 7 | yes | params are preset/snapshot state | | SlapPresets.everySlapFieldRoundTrips | grid/scale block on branch |
| Undo classes technique-arm / technique-param (200 ms) / mute-grid-paint | 8 | no | branch `TechniqueUndo` | | TechniquesUi.theUndoClassesGroupAsSpecified | |
| Perf: idle < 0.1%, all six ~2.5%; low-CPU class banner | 9 | partial | scrape/slap idle tests | | Scrape.idleCostsNothing | banner missing everywhere (no CPU-class detection) |
| Tests: 1000 cmds/s no alloc; 100 pre-delta presets byte-identical | 10 | no | branch (weakened: whole suite plus one idle test) | | TechniqueLayer.* | |

### ambiguity-resolutions.md

**Almost fully implemented on HEAD.** The feedback loop, Freeze, E-Bow, Doubler pedal (post-amp, pre-cab: verified, the post rack runs before `cabinet.processBlock`), rubric voicer, preset morph, crossing_sps and guitar migration all have params, UI and tests. Minor gaps: the Doubler "enable" is the slot bypass and the pedal is not in the rack by default ("always available" means available to insert). The Doubler pitch/HP/LP defaults are not asserted. The migration-table update via content packages depends on the installer work (branch:visual). Counts: yes 22 / partial 3 / no 0.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Physical per-string feedback loop, one-block delay, post-circuit | 1.1 | yes | DSP/Feedback/FeedbackLoop | | Feedback.eachStringHearsItsOwnNote | |
| Params feedback_amount/distance/angle/focus/octave_bias | 1.2 | yes | Parameters.h:257-261 | Advanced Col 3 SUSTAIN feedback row | | |
| Zero-amount bypass bit-identical | 1.4 | yes | | | Feedback.zeroAmountIsBitIdenticalWhateverTheOtherSettings | |
| Stability 60 s; volume-knob attenuation | 1.4 | yes | | | Feedback.staysBoundedForAMinuteAtFullTilt; theVolumeKnobLowersTheLoopByTheCircuitsAttenuation | |
| Feedback LED on resonance | 1.3 | yes | Widgets `FeedbackLed` | SUSTAIN card | | |
| Legacy feedback_on/threshold/speed | 1 | yes | kept inert, intentionally hidden | n/a | Feedback.oldPresetsThatSwitchedItOnGetAnAmount | |
| Freeze captured loop, 7 params | 2.1 | yes | DSP/Master/FreezeOverlay; `freeze_*` | SUSTAIN Freeze row | Sustain.freezeLayerHoldsItsLevelForASixtySecondHold; aSecondFreezeReplacesTheFirst | loop, not "granular" |
| E-Bow params enable/mask/intensity/harmonic | 2.2 | yes | DSP/Feedback/EBowDriver; `ebow_*` | SUSTAIN E-Bow row | EBow.* (4) | |
| E-Bow steady within 500 ms; decay within 200 ms | 2.4 | yes | | | EBow.aHeldNoteIsSteadyWithinHalfASecondAtHalfIntensity; switchingItOffSilencesTheStringWithin200ms | |
| Doubler 8 params with ADT defaults | 3 | partial | `DoublerPedal` (7 knobs + slot bypass) | POST-FX rack pedal | Doubler.* | pitch/HP/LP defaults unasserted |
| Doubler post-amp pre-cab | 3 | yes | postEffects before `cabinet.processBlock` | | Doubler.isAPostAmpRackPedal | |
| Doubler mix-0 null, mix-100 at 22 ms | 3.2 | yes | | | Doubler.mixZeroIsTheDrySignal; mixFullIsACopyTwentyTwoMillisecondsLate | |
| Old doubler params migrate to pedal | 3 | yes | PresetManager::fromVar | hidden (intentional) | Doubler.presetsWithTheOldDoublerGetThePedal | |
| Rubric constraints, weights, style bias, transition, capo, determinism | 4.1-4.6 | yes | Model/Playing/RubricVoicer | | RubricVoicer.* (15) | |
| Rubric tests (84 templates, I-IV-V-I ≤ 3 frets, BEAD bass) | 4.7 | yes | | | RubricVoicer.everyTemplateInEveryKeyAndStyleIsVoicedOrUnplayable etc. | |
| Morph: continuous interpolate, discrete switch at 0.5, structural at 0.5 | 5.1 | yes | Presets/PresetMorph | | PresetMorph.theMidpointSwitchesDiscretesAndHalvesTheRest | |
| Morph toggle, A/B slots, slider; `preset_morph_position` automatable | 5.2 | yes | Overlays.cpp:1123 | Preset browser Morph row | PresetMorph.theBrowserMorphRowFillsTheSelectedSlot | |
| Morph ends identical; 4 s sweep click-free; snapshot cancels | 5.3, 8 | yes | | | PresetMorph.* | |
| crossing_sps from pattern/step; global default 200/100 | 6 | yes | Rhythm/Patterns `crossingSps` | | StrumDynamics.rhythmEngineSuppliesVelocityFromThePattern; patternCrossingGivesTheDocumentedSpread | |
| MPE / hex routing bypasses strum synthesis | 6 | partial | | | StrumDynamics.liveSpreadWins | no explicit MPE pass-through test |
| migration.json maps pre-M49 names | 7 | yes | Resources/Guitars/migration.json | | GuitarMigration.everyPreM49NameResolvesToItsShippedGuitar | |
| Unknown guitar: factory default + banner, params preserved | 7 | yes | | banner | GuitarMigration.anUnknownGuitarKeepsThePresetAndSaysSo | |
| Migration table ships via content updates | 8 | partial | branch:visual ContentPackage | | branch: ContentPackageTests | |
| Feedback/freeze/E-Bow as mod destinations | 8 | yes | ModMatrix (params) | right-click Modulate | | |

### qa-polish.md

This is a ship-gate checklist. Much of it is process (hosts, platforms, bug bash, videos, legal review, post-release). None of that can be verified in code, and none of it is done. Code-side state on HEAD: 1200+ unit/combination tests, a UI walker (GuiReach), trademark tests, a 10k param fuzz, SR/BS change and mono-compat tests. pluginval strictness 10 was run by hand (passes only with a long timeout, B-12). Missing on HEAD: CI, golden-render suite, 100k fuzz, state fuzz, boot/memory tests, byte-flip corruption, 32-instance stress, and installers. All of these except golden render, host-format switching and translations are **in progress on branch:visual**. Localisation has the catalog API, but no `Resources/i18n/*.json` translations ship and most UI strings are literals. Counts: yes 14 / partial 17 / no 20.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Host × platform × SR × buffer × CPU-class matrix | 1 | no | process; Linux container only | n/a | | no AAX target, so Pro Tools cannot be covered |
| CI on every push, nightly full matrix | 2 | no | none on HEAD; branch:visual `.github/workflows/ci.yml`, `nightly.yml` | n/a | | |
| Golden render: 36 presets × SR × block vs golden WAV | 2 | no | none (Combo renders check bounds only) | n/a | | |
| Parameter fuzz 100 000 | 2 | partial | HEAD 10k | | Parameters.fuzzAcrossTenThousandStates; branch:visual Parameters.fuzzAcrossHundredThousandStates | |
| State fuzz 10 000 ops | 2 | partial | branch:visual | | StateModel.tenThousandRandomOperationsLeaveNoStuckState | |
| Boot ≤ 400/200 ms | 2 | partial | branch:visual | | Boot.coldAndWarmInstantiationStayInBudget | |
| Memory 60-min session < 900 MB, no growth | 2 | partial | branch:visual `MemoryProbe` | | Memory.sixtyMinuteSessionNoMonotonicGrowth | |
| Preset round trip to the ulp | 2 | partial | HEAD Presets.stateRoundTripsExactly; branch:visual per factory preset | | Presets.everyFactoryPresetRoundTripsToTheUlp | |
| Guitar round trip audio null | 2 | partial | | | Workshop.everyFactoryGuitarLoadsAndRoundTrips | audio null not asserted |
| Migration round trip vs pre-M49 golden (-60 dB) | 2 | partial | resolution only | | GuitarMigration.* | no golden |
| pluginval strictness 10, zero warnings, every format | 2 | partial | manual run (BETA report); branch:visual `scripts/pluginval.sh` | | | B-12: default timeout fails |
| Crash: multi-instance 32 | 3 | partial | branch:visual | | Stress.thirtyTwoInstancesRenderInTurn | sequential, not concurrent |
| Crash: format switching VST3↔AU | 3 | no | | | | macOS only |
| Crash: SR / block-size change mid-play | 3 | yes | | | Engine.sampleRateChangesAreSurvived; blockSizeChangesAreSurvived | |
| Crash: preset/snapshot/guitar load under MIDI storm | 3 | partial | branch:visual | | Stress.midiStormDuringLoadsRecallsAndGuitarLoads | HEAD: Combo.structuralChangesWhileAudioRuns |
| Crash: MIDI Learn ×100, undo ×1000, bus layout, Slide/advanced toggles | 3 | partial | branch:visual | | Stress.* | |
| Crash: corrupt preset byte-flip | 3 | partial | branch:visual | | Presets.everyByteOfAFactoryPresetFlippedIsRefusedOrLoads | |
| Crash: corrupt .luthierguitar byte-flip | 3 | no | | | | |
| Missing files fallback + banner | 3 | yes | | banner | Workshop.aMissingPartFallsBackAndSaysSo; WorkshopPresets.aMissingGuitarFileFallsBackToItsType; Editor.aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce | |
| Every control has a tooltip | 4 | yes | Widgets `attachTo` (falls back to param name) | | GuiReach walk | |
| Tooltip/label in every locale | 4 | no | Accessibility/Localisation API; no `Resources/i18n` | | Localisation.catalogCoversTheUi | literals throughout (e.g. technique pages) |
| Accessibility role/value with units | 4 | partial | Accessibility/* | | AccessibilityTests | |
| Right-click reset / value entry / paste | 4 | yes | Widgets.cpp:40-44 menu | | GuiReach.operatingEachControlWritesItsParameter | double-click reset only on a few sliders |
| Advanced-range warning arc, "*" suffix, padlock | 4 | yes | RangesUi, HeaderBar padlock | | RangesUi.* | |
| Every automatable param reachable | 4, 6 | no | 45 params with no control (B-11) | | GuiReach.everyAutomatableParameterHasAVisibleControl (fails) | |
| Renders at 75-200% scale and in every palette | 4 | partial | | | Accessibility.uiScaleStepsAndFontFloor; Theme.controlsRenderInEveryPaletteAndRepeatExactly; branch:visual Screenshots.everyPanelInEveryPalette | |
| Reduced motion | 4 | yes | | | Accessibility.reducedMotionRemovesAnimation | |
| Panel "?" opens docs | 4 | partial | branch:tune-help `PanelHelpButton` | | | |
| Dialogs: Escape closes | 4 | yes | | | Editor.everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt | |
| Empty-state hints | 4 | partial | per-panel ad hoc | | | no systematic check |
| Workshop checks (hit-test, bench ≤ 30 ms, Save As Guitar) | 4 | partial | | | Editor.everyHitRegionOnTheIllustrationDescribesItself; WorkshopBench.abRecallRoundTrips; branch:visual WorkshopQaTests | |
| Factory preset render without clicks / > 0 dBFS+3 | 5 | yes | | | Combo.everyFactoryPresetPlaysEveryPhrase; presetSwitchUnderARingingNoteDoesNotClick | |
| DC null -100 dBFS on silent input | 5 | partial | | | Engine.silenceInSilenceOut | B-04: idle floor -22 to -30 dBFS at high gain |
| Mono compatibility | 5 | yes | | | Engine.monoCompatibility | |
| Pedal toggle click-free, zero-mix -80 dB, amp gain monotonic, tone flat, cold start | 5 | partial | branch:visual | | Effects.*; Amp.* (EffectsQaTests) | HEAD: Effects.bypassIsTransparent |
| Realism checks (squeak determinism, buzz threshold, slide ±2 c, Kinman) | 5 | partial | branch:visual RealismQaTests | | Squeak.aThousandRunsAreByteIdentical etc. | |
| Bass slap/pop/ghost/double-thump checks | 5 | yes | | | SlapWiring.* | |
| Every gui-integration 19 feature present and wired | 6 | partial | | | GuiReach | techniques not merged |
| Every user doc matches build; screenshots current; translated | 6 | partial | docs/USER_MANUAL.md etc. | | | no translations, no screenshot pipeline on HEAD |
| MIDI export: Luthier round-trip of every class; generic | 6 | yes | | | MidiExport.luthierRoundTripNullsEveryFactoryPreset; genericRoundTripNullsWithinThirtyDb | technique classes not exported |
| Drag-out export to Downloads | 6 | partial | | | | not verified here |
| Performance targets (idle ≤ 1.5, steady ≤ 8, heavy ≤ 22) | 7 | no | | | Combo.cpuPerFactoryPreset (limit 50%) | B-13: idle 8-19% |
| Latency accurate within 1 sample | 7 | partial | | | Engine.latencyIsReportedAndPlausible; branch:visual Latency.anImpulseArrivesWhenReported | |
| Bug bash (8 guitarists) | 8 | no | process | | | |
| Installer polish (signed Win, notarized pkg, .deb/.tar.gz) | 9 | no | branch:visual Linux only | | | see installer.md |
| User manual complete + translated; videos | 10 | partial | docs/USER_MANUAL.md | | | no translations or videos |
| THIRD_PARTY_LICENCES.txt | 11 | partial | branch:visual | | Legal.thirdPartyLicencesNameEveryBundledDependency | missing on HEAD |
| Trademark-free names | 11 | yes | Tools/trademark_scan.py | | Trademarks.* | |
| EULA, refund policy | 11 | no | | | | |
| Final human check; post-release monitoring / rollback | 12-13 | no | process | | | |

### performance-budget.md

On HEAD enforcement is thin. `Engine.cpuStaysWithinBudget` and `Combo.cpuPerFactoryPreset` exist, but the latter asserts only < 50% of a core. There are no per-module measurements, no allocation/lock traps on the audio thread (ThreadProbe counts only file access), no memory, boot or scaling tests, no CPU relief ladder and no SR downgrade above 96 kHz. **B-13: idle costs 8-19% of a core, against a 1.5-unit budget.** branch:visual adds most of this: PerfBudgetTests, AudioThreadSafetyTests (alloc/lock trap), CpuRelief (steps 1-7, opt-out), Oversampler downgrade, MemoryProbe, LatencyTests and a nightly dashboard. No pedal has an eco Quality option. Counts on HEAD: yes 3 / partial 7 / no 14.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Per-module budget measured in CI, ≤ budget × 1.10 | 0.1, 10 | no | branch:visual | | PerfBudget.everyModuleWithinBudget | |
| Regression > 10% flag / > 20% block | 0.3 | no | branch:visual nightly dashboard JSON | | | |
| Audio callback allocates zero (heap hook) | 0.4 | no | branch:visual ThreadProbe additions | | ThreadProbe.theLockTrapSeesALock; Engine.fiveMinutesOfPlaybackNeitherAllocatesNorLocks | HEAD ThreadProbe: file access only |
| Audio callback locks nothing | 0.5 | no | branch:visual | | same | B-01 fixed a race |
| Idle total ≤ 1.5 units | 1 | no | | | Combo.cpuPerFactoryPreset | B-13: 8-19% |
| Steady ≤ 8, heavy ≤ 22, realism ≤ 12, slide ≤ 18, bass ≤ 15 | 1 | partial | | | Combo.cpuPerFactoryPreset (heaviest 18.6%); branch:visual PerfBudget.scenarioTotals | not measured on the reference CPU |
| Per-pedal cap 0.5 + eco Quality option | 2 | no | no Quality param on any pedal | | | |
| Memory: baseline ≤ 350 MB, cap 900 MB | 3 | partial | branch:visual MemoryProbe | | Memory.baselineInstanceUnder350MB | |
| Session recorder ring off by default | 3 | yes | Capture/CaptureRing | | Capture.* | |
| Latency ≤ 128 samples main; aux ≤ 32 | 4 | partial | | | Routing.perOutputLatencyIsConsistent; branch:visual Latency.dspLatencyIsWithinBudget | |
| Latency reported per routing-io 7 | 4 | yes | LuthierEngine latency sum | | Engine.latencyIsReportedAndPlausible | |
| Boot cold ≤ 400 / warm ≤ 200 ms; standalone ≤ 1.5 s | 5 | partial | branch:visual | | Boot.coldAndWarmInstantiationStayInBudget | standalone launch untested |
| Guitar load ≤ 300, tune ≤ 100, part swap ≤ 50, delta ≤ 40 ms | 5 | partial | branch:visual | | Workshop.guitarLoadStaysUnder300ms; hundredRandomPartSwapsStayUnder50ms; Tune.loadStaysUnder100ms; WorkshopSpectrum.hundredShadowRendersStayUnder40ms | |
| Shadow audition ×100 budget | 10 | partial | branch:visual | | WorkshopBench.hundredAuditionsStayInBudget | |
| Voice-count scaling curve | 6 | no | branch:visual | | PerfBudget.voiceCountScaling | |
| Sympathetic coupling O(N) regardless of voices | 6 | yes | CouplingMatrix | | | |
| Sample-rate scaling curve ±10% | 7 | no | branch:visual | | PerfBudget.sampleRateScaling | |
| Oversampling downgrade above 96 kHz | 7 | no | branch:visual Oversampler.h | | Engine.oversamplingDowngradesAbove96k | |
| CPU relief ladder steps 1-7 at > 85% | 8 | no | branch:visual Support/CpuRelief | | CpuRelief.laddersUpAtEightyFivePercentAndBackDown | |
| Relief 7 opt-out in Options > Diagnostics, "CPU limit" banner | 8 | no | branch:visual | branch: Options > Diagnostics | CpuRelief.stepSevenIsOptOut | |
| Noise pools halve under load | 8 | no | branch:visual | | CpuRelief.theEngineHalvesTheNoisePoolsOnlyUnderLoad | |
| Enforcement on every merge + dashboard | 9 | no | branch:visual nightly | | | no CI on HEAD |
| 60-min memory growth test | 10 | no | branch:visual | | Memory.sixtyMinuteSessionNoMonotonicGrowth | |
| Latency accuracy within 1 sample (impulse) | 10 | no | branch:visual | | Latency.anImpulseArrivesWhenReported | |

### installer.md

On HEAD **no installer or packaging exists at all**: no `packaging/`, no CPack, no signing, no first-run tree or marker (FirstRun is WIP), no content packages and no update download. Folders under `~/Documents/Luthier` are created lazily per feature. branch:visual adds Linux `.deb`/`.tar.gz` via CPack, install/uninstall.sh with a manifest, `.desktop`, MIME xml, postinst/postrm, `scripts/release_manifest.sh` (SHA-256 + optional GPG), reproducible-build check in nightly, `InstallLayout` (folder tree + `.installed_version`), `FileOpenRouter` (file associations in-app), `ContentPackage` (signed `.luthiercontent` + delta) and `UpdateDownloader`. **Windows and macOS installers, signing, notarization, universal binary, portable zip, rpm, MSI and the enterprise silent install are missing everywhere.** Counts on HEAD: yes 1 / partial 6 / no 27.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Code-signed (Win) / notarized (mac) / PGP (Linux) | 0.2 | no | branch:visual GPG manifest only | | | |
| Uninstall removes all, preserves user data, purge option | 0.3 | no | branch:visual Linux uninstall.sh (manifest) | | | |
| Deterministic byte-identical builds | 0.5 | no | branch:visual nightly "Reproducible packages" | | | |
| Windows NSIS/Inno .exe, naming | 1 | no | none (scripts/build.ps1 builds only) | | | |
| Win flow: language, licence, components, paths, space/version check, rollback | 1.1 | no | | | | |
| Win file associations (6 extensions), Start menu, Add/Remove | 1.1 | no | branch:visual FileOpenRouter (in-app routing only) | | FileOpen.everyAssociationRoutesToItsLoader | no OS registration on Win/mac |
| Silent install /S /D, exit codes, HKLM keys | 1.2 | no | | | | |
| Win uninstaller (manifest, user-data checkbox, DAW-running check) | 1.3 | no | | | | |
| macOS .pkg in signed, notarized .dmg | 2 | no | | | | |
| macOS components, paths, Launch Services associations | 2.1 | no | CMake builds AU on mac only | | | |
| Universal binary | 2.2 | no | no CMAKE_OSX_ARCHITECTURES seen | | | |
| macOS Uninstall.command | 2.3 | no | | | | |
| Linux .tar.gz + .deb | 3 | partial | branch:visual CPack TGZ;DEB | | | |
| Linux .rpm (best-effort) | 3 | no | | | | release_manifest accepts *.rpm but nothing makes one |
| Linux layout, .desktop, MIME xml, icon | 3.1 | partial | branch:visual packaging/linux/* | | | |
| postinst updates icon/desktop/MIME caches; install.sh user/system; uninstall.sh | 3.2 | partial | branch:visual | | | |
| Dependencies documented | 3.3 | partial | branch:visual CPACK_DEBIAN_PACKAGE_DEPENDS | | | |
| Standalone-only bundle | 4 | no | | | | |
| Update banner "Version X available" + release notes | 5.1 | yes | PluginEditor.cpp:879; OptionsPages.cpp:1624 | header banner / Options > Updates | Telemetry.* | |
| "Download" to Downloads, no auto-launch | 5.1 | partial | branch:visual UpdateDownloader | | Updates.theDownloadLandsInDownloadsUnderItsOwnName | |
| Delta patches (SHA-256, rollback) | 5.2 | partial | branch:visual ContentPackage "kind":"delta" | | ContentPackageTests | |
| First-run tree (18 subfolders), plugin.json, `.installed_version` | 6 | no | WIP/UI/FirstRun (not built); branch:visual InstallLayout; branch:tune-help un-WIPs FirstRun | | InstallLayout.createsTheTreeAndMarker | |
| Marker absent → onboarding; differs → upgrade banner + migrations | 6 | no | branch:visual + branch:tune-help Onboarding | | InstallLayout.differentVersionReportsUpgrade | |
| Enterprise: CLI config, policy file pre-placement | 7 | partial | Telemetry policy file honoured | Options banner "managed by policy" | Telemetry.* | no installer-side placement |
| MSI wrapper | 7 | no | | | | |
| Migration: preset without ranges block + backup to Presets/Backup/<date> | 8 | partial | ranges derived on load (Ranges.theRangesBlockRoundTripsAndDerivesWhenAbsent); backup only on branch:visual | | Presets.backupsGoToThePresetsRootBackupFolder | |
| Migration: guitar via migration.json + backup | 8 | partial | migration yes; backup branch:visual | | GuitarMigration.* | |
| Old .luthierloop / .mid without Luthier chunk | 8 | yes | Looper / MidiExport generic load | | | |
| Subtle info banner on first migrated load | 8 | no | branch:visual | | Editor.aMigratedPresetRaisesOneInfoBanner | |
| Portable Windows zip | 9 | no | | | | |
| SHA-256 checksums + PGP manifest | 10 | partial | branch:visual scripts/release_manifest.sh | | | |
| `.luthiercontent` signed package → ContentUpdates/<name> | 11 | partial | branch:visual ContentPackage | Options > Updates (branch) | ContentPackageTests | drag-onto-plugin not verified |
| Rollback plan (manifest revert, banner) | 12 | no | process | | | |
| Tests: install/uninstall/upgrade/downgrade/signature/portable/associations/200 migrations | 13 | no | branch:visual covers layout + in-app associations only | | InstallTests, FileOpenTests | |

---

### Top gaps (group G)

1. **TECHNIQUES tab missing on the integration branch.** On HEAD, 39 `scrape_*`/`slap_*`/`pop_*`/`ghost_*`/`double_thump_*` params have no control (B-11), and no scrape or slap can be fired from the GUI. Slide controls, Mute, Tap, Bend and Cascade (about 64 more params) exist only on `claude/luthier-techniques`. That branch is **behind** the integration branch and needs a merge and conflict pass. Parameters.cpp is also appended by realism-a/b/c, visual and model-gaps, so the automation-index order must be settled once.
2. **Idle CPU 6-12× over budget** (B-13: idle 8-19% of a core against 1.5 units). A buyer sees this in their DAW meter immediately. Nothing on HEAD enforces the budget (Combo limit is 50%).
3. **No Windows or macOS installer, no signing, no notarization, no universal binary.** Nothing ships on the two platforms most buyers use. Linux packaging exists only on branch:visual.
4. **No CI on the integration branch** and no golden-render regression suite (branch:visual has ci.yml/nightly.yml but no golden WAVs).
5. **No translations.** The localisation catalog API exists, but no `Resources/i18n/*.json` ships and new UI (technique pages) uses literal strings. This is ship gate 2.
6. **Muting-as-rhythm absent** (WIP only on HEAD). There is no mute grid, no mute_type per step, no Easy mute button, and no palm-mute T60 model beyond the keyswitch articulation.
7. **Microtonal bends absent.** HEAD has only global semitone `bend_range` and a fixed vibrato. There is no per-string range, quantise, .scl/.tun, pre-bend or curves. The branch adds a second vibrato system that overlaps `vibrato_*`.
8. **Two-hand tapping absent.** HEAD's hammer-on velocity threshold is hard-coded (0.63, about MIDI 80, spec 40) and not user-settable. The legato window defaults to 40 ms (spec 150).
9. **Audio-thread allocation/lock traps, memory, boot and scaling tests** exist only on branch:visual. performance-budget 0.4/0.5 are unenforced on HEAD.
10. **CPU relief ladder and SR>96k oversampling downgrade missing on HEAD** (branch:visual). No "CPU limit" banner, and no low-CPU-class banner anywhere.
11. **First-run folder tree, `.installed_version` marker, onboarding and upgrade banner** are split across WIP, branch:visual and branch:tune-help. None of them is on HEAD.
12. **Scrape and slap factory presets not loadable.** The 10 presets from string-scraping 4 and string-slap 4 exist only as `*Settings::fromPreset`, which only the tests call, on every branch.
13. **MIDI export of technique events** (mute types, taps, bend contributions) is deferred on every branch. Slap and scrape are not exported either.
14. **Body Tap does not drive the body-coupling mode bank** (no `BodyCoupling::driveDirect`). It uses three fixed resonators, so the tap does not follow the loaded body.
15. **pluginval strictness 10 fails at the default timeout** (B-12, parameter thread safety > 30 s), and no pluginval runs in CI on HEAD.
16. **Content updates, update download, preset migration backups and the migrated-load banner** are all branch:visual only.
17. **No APVTS parameter groups.** About 700 flat params make host automation lists unusable. The spec asks for `parameters/techniques/<t>/*`.
18. **No per-pedal eco/Quality option** (performance-budget 2).
19. **`double_thump_enabled` defaults to off** (spec: the rebound is on by default for thumb). **`scrape_string_mask` defaults to "held strings"** (spec: wound only).
20. **Loud idle noise floor at high gain, no gate** (B-04). This fails qa-polish 5 "DC null -100 dBFS".

### Unspecified gaps noticed

- **No AAX build.** qa-polish 1 lists Pro Tools 2024 in the host matrix, but CMake has no AAX format, and the specs never cover the AAX SDK or PACE signing.
- **No keyswitch legend or on-screen keyboard map.** Techniques use KS 12-21, plus the older articulation keyswitches. Nothing in the UI shows which low keys do what, and a keyboard player will hit C0-A0 by accident.
- **No per-technique demo phrase or audition.** The AUDITION button plays a generic phrase, so a buyer cannot hear scrape, slap, tap or bend without MIDI programming.
- **No MIDI file examples or DAW templates** that show how to drive techniques (channel-2 taps, CC sweeps).
- **No noise gate** on the amp path (see B-04). This is standard in every $200 amp-sim or guitar plugin.
- **No licence activation or copy-protection flow is visible in this group**, beyond the notification grace-period text. Worth confirming in the licensing spec.
- **No crash reporter or symbol upload on HEAD.** qa-polish 13 assumes one exists.

### Small glue candidates

These are cheap wins where engine code or params exist and only need a UI attachment or a preset entry. The panel is where the spec puts each control. The "interim" note applies if the techniques branch cannot merge soon.

| Item | Param IDs / symbol | Panel it should go on | Effort |
|---|---|---|---|
| Scrape controls | `scrape_armed`, `scrape_trigger`, `scrape_trigger_cc`, `scrape_direction`, `scrape_sweep_source`, `scrape_sweep_cc`, `scrape_start_mm`, `scrape_end_mm`, `scrape_duration`, `scrape_pressure`, `scrape_tool`, `scrape_angle`, `scrape_string_mask`, `scrape_retrigger` | TECHNIQUES > SCRAPE (`ScrapePage` exists on branch:techniques). Interim: CHARACTER > NoiseGroups beside `pick_scrape_amount` | low: attach only |
| Scrape "fire" button | `ScrapeEngine::requestTrigger()` | TECHNIQUES > SCRAPE + Easy Playing strip | trivial |
| Slap technique controls | `slap_armed`, `slap_type`, `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`, `slap_ghost_cc`, `slap_force`, `slap_palm_position_mm`, `slap_string_mask`, `slap_ghost_mode`, `slap_rebound_gap`, `slap_snap_back`, `slap_body_part` | TECHNIQUES > SLAP (branch `SlapPage`) | low |
| Bass slap basics | `slap_strength`, `slap_position_mm`, `slap_thumb_hardness`, `slap_fret_contact`, `pop_strength`, `pop_position_mm`, `double_thump_enabled`, `double_thump_up_ratio`, `ghost_level`, `ghost_damping`, `ghost_auto`, `ghost_velocity_threshold` | CHARACTER > SLAP group, bass only (`SlapGroup` ready on branch:model-gaps) | merge only |
| Slap "fire" button | `TechniqueTriggers::request(TechniqueId::slap, …)` | TECHNIQUES > SLAP + Easy strip | trivial |
| Scrape factory presets ×5 | `ScrapeSettings::fromPreset(ScrapePreset::*)` | FactoryPresets "Techniques" category / preset browser | low |
| Slap factory presets ×5 | `SlapSettings::fromPreset(SlapPreset::*)` | FactoryPresets "Techniques" category | low |
| Hammer-on velocity threshold exposed | `TechniqueEngine::setLegatoVelocityThreshold` (new param, default MIDI 40) | Advanced PERFORMANCE next to `legato_window` (later TECHNIQUES > TAP `tap_hammer_threshold`) | low |
| Default fixes | `double_thump_enabled` → true; `scrape_string_mask` default → wound strings | Parameters.cpp | trivial; changes preset defaults |
| CPU-limit / relief opt-out toggle | branch:visual `CpuRelief` step 7 | Options > Diagnostics | merge only |
| Panel "?" help buttons | branch:tune-help `PanelHelpButton` | every panel header | merge only |

---

## Group H



Base: `origin/claude/luthier-cloud-session-5lzlix` checkout at /home/user/luthier (read-only). Helper branches checked: visual, tune-help, model-gaps, realism-a/b/c, techniques (also review, audit, release). Where a row says "in progress on X", the work is only on that branch.
`docs/spec-coverage.md` sections 33-41 were used as a starting point and re-checked. Several of its rows are stale: the Tune, WorkshopBench, `.midprofile` and AU build now exist.

Most important finding for this group: **a saved `.luthierpreset` file does not contain the modulation matrix, snapshot bank, MIDI Learn mappings, rhythm-engine state, character seed/wear, tone-match IR references or routing.** `PresetManager::toVar` writes only parameters, ranges, guitar, strings and the CC-target map. Everything else goes only into the host session blob (`LuthierAudioProcessor::getStateInformation`). So "Save preset" loses snapshots and modulation, and a setlist entry's snapshot index gets recalled from whatever bank happens to be loaded (`applyCurrentSetlistEntry`).

### file-formats.md

Summary: 53 requirements. **yes 20, partial 22, no 10, n/a 1.** Parts, guitars, tunes and `.midprofile` follow the spec closely. The preset format has drifted: it uses a flat root with `schemaVersion`, has no `meta` block, and leaves out modulation, snapshots, rhythm, MIDI Learn and routing. Setlist, pattern and loop files have no magic or schema. `.luthiercontent` exists only on visual. Migration backups are on model-gaps. There are no guitar backups, no kill-mid-save test and no 200-fixture migration test.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Every file UTF-8 JSON | 0.1 | yes | all writers (`JSON::toString`) | n/a | - | |
| Top-level `schema` int, missing = 1 | 0.2 | partial | Part.cpp, PartLibrary.cpp, TuneFile, MidiProfiles use `schema`; PresetManager uses `schemaVersion` | n/a | - | Preset with no schemaVersion is refused (not treated as 1); setlist/pattern/loop have none |
| Unknown fields preserved on load and written back | 0.3 | partial | PresetManager::fromVar `unknownFields`; TuneFile | n/a | Presets::unknownFieldsSurviveARoundTrip; TuneBuilder::unknownFieldsAreKeptAndWrittenBack | Top level only for presets; not setlist/pattern/guitar/host blob |
| Migrations documented per format | 0.4 | partial | PresetManager ranges derivation, pickup placement, doubler, strum speed | n/a | RangeTests, WorkshopPresetTests | Not listed in the spec's migration table |
| Canonical extension + magic marker | 0.5 | partial | preset/guitar/part/tune/midprofile magic | n/a | Presets::aFileWithoutTheMagicMarkerIsRefused | Setlist `format:"luthierset"`; pattern and loop have none |
| Portable relative paths + registered folders in user-global config | 0.6 | partial | IrSlot relative paths; guitar `Factory/`/`User/` refs | n/a | ToneMatch::presetPathsAreRelativeUnderTheLibraryRoot | No registered-folders list; setlist `preset_path` absolute |
| `.luthierpreset` type | 1 | yes | PresetManager kFileExtension/kMagic | Header / browser | - | |
| `.luthierguitar` type | 1 | yes | PartLibrary.h kMagic | Workshop Save As | Workshop::everyFactoryGuitarLoadsAndRoundTrips | |
| `.luthierpart` type | 1 | yes | Part.h | Workshop | Workshop::theFactoryLibraryIsThere | |
| `.luthiertune` type | 1 | yes | TuneFile.h | TUNE tab open/save | TuneBuilder tests | |
| `.luthierpattern` type | 1 | partial | Patterns.cpp RhythmPattern::saveTo | RHYTHM tab save | RhythmPatterns::patternsRoundTripThroughJson | No magic/schema |
| `.luthierset` type | 1 | partial | Setlist.cpp | LiveStrip menu (open only) | LiveSetlist::roundTripsThroughJson | `Setlist::saveTo` is never called from UI, so there is no way to save a setlist |
| `.luthierloop` type | 1 | partial | Looper::save (folder + loop.json) | PRACTICE drawer | - | Folder rather than a single file |
| `.luthiercontent` type | 1 | no | - | no | - | In progress on visual (Updates/ContentPackage.*, ContentPackageTests) |
| `.midprofile` type | 1 | yes | Export/MidiProfiles.h kProfileMagic | MIDI OUT panel Save/Load profile | MidiExportTests | spec-coverage row stale |
| `.mid` Luthier/Generic | 1 | yes | Export/MidiPerformance, MidiProfiles | Export | MidiExportTests | |
| wav/aiff/flac audio | 1 | yes | AudioExporter Format::Wav/Aiff/Flac | Export | - | |
| `.mp3` backing tracks in | 1 | partial | BackingTrack `registerBasicFormats` | PRACTICE | - | JUCE_USE_MP3AUDIOFORMAT not set, so no MP3 on Linux and only via OS codecs elsewhere |
| Preset `meta` block (name, author, category, tags, created, modified, version_created/modified, notes) | 2 | partial | PresetManager::toVar | Save As overlay | - | Flat root; `author` always ""; no dates or version_created |
| Preset `guitar {reference, override}` | 2 | yes | PluginProcessor::getGuitarBlock | n/a | WorkshopPresets::aMissingGuitarFileFallsBackToItsType | |
| Preset `parameters` | 2 | yes | PresetManager::toVar | n/a | Presets::stateRoundTripsExactly | Stores normalised 0..1 values, not plain values |
| Preset `ranges` block | 2 | yes | RangeState::toVar | Options → Ranges | Ranges::theRangesBlockRoundTripsAndDerivesWhenAbsent | |
| Preset `modulation` (sources + routes) | 2 | no | only in getStateInformation | n/a | - | **Lost when a preset file is saved or loaded** |
| Preset `snapshots` | 2 | no | only in getStateInformation | n/a | - | **Lost when a preset file is saved**; also breaks setlist snapshot recall |
| Preset `midi_mappings` | 2 | partial | `midiMap` (CC → MidiTarget) | n/a | - | MIDI Learn mappings (`midiLearn.toVar`) only in host blob |
| Preset `rhythm_engine` | 2 | no | only host blob (`rhythm`) | n/a | - | |
| Preset `effects_state` pre/post rack | 2 | partial | slot params inside `parameters` | n/a | - | Not a separate block; equivalent content |
| Preset `midi_out_profile` | 2 | no | routing only in host blob | n/a | - | |
| Migration schema 1 → add ranges | 2 | yes | fromVar `deriveFromCurrentValues` | n/a | RangeTests | Derived, not "stock" (advanced-ranges supersedes) |
| Migration schema 2 `guitar.name` → reference via migration.json | 2 | yes | PartLibrary migration.json, PluginProcessor guitar block | n/a | GuitarMigration::aPresetNamingAnOldGuitarLoadsItsReplacement | |
| Backup original on migration to `Presets/Backup/<date>/<name>-v<schema>` | 2 | no | - | n/a | - | In progress on model-gaps (`backupMigratedOriginal`) |
| `.luthierguitar` schema fields | 3 | yes | PartLibrary.cpp WorkshopGuitar::toVar/fromVar | Workshop | Workshop::everyFactoryGuitarLoadsAndRoundTrips | |
| `.luthierpart` schema | 4 | yes | Part.cpp | Workshop | Workshop::theFactoryLibraryIsThere | |
| `.luthiertune` per tune-builder 11 | 5 | yes | Tune/TuneFile.cpp | TUNE tab | TuneBuilder::aHundredRandomTunesRoundTripByteIdentical | |
| `.luthierpattern` per rhythm-engine 6 | 6 | partial | Patterns.cpp | RHYTHM | patternsRoundTripThroughJson | No magic |
| `.luthierset` schema (schema, magic, meta, entries{preset,snapshot,notes}) | 7 | partial | Setlist::toVar | LiveStrip | LiveSetlist::roundTripsThroughJson | Keys `preset_path`/`snapshot_index`; flat; load doesn't check magic |
| `.luthierloop` contents | 8 | partial | Looper::save | PRACTICE | - | |
| `.luthiercontent` signed zip manifest | 9 | no | - | no | - | In progress on visual (RSA-signed manifest.sig, not PGP) |
| `.midprofile` schema | 10 | yes | MidiProfiles | MIDI OUT panel | MidiExportTests | |
| Common meta rules | 12 | partial | parts/guitars/tunes | n/a | - | Presets/setlists lack ISO dates and versions |
| Atomic save: temp, fsync, rename | 13 | partial | PresetManager::writeToFile (TemporaryFile + flush), TuneFile::save, Part/PartLibrary `.tmp`+move | n/a | - | Others use `replaceWithText` |
| Previous version → backup (preset, guitar, tune) | 13 | partial | PresetManager::backupBeforeOverwrite; TuneFile `.backup` | n/a | Presets::savingBacksUpTheVersionItReplaces | No guitar backup |
| Backups > 30 days pruned on startup | 13 | partial | PresetManager::pruneOldBackups (ctor) | n/a | - | Tune `.backup` never pruned; ErrorLog::pruneOldLogs never called |
| Load path: read, UTF-8/JSON/magic, schema, migrate, required fields, references, refuse with banner | 14 | partial | PresetManager::loadPreset; PartLibrary load report; TuneFile | banner "preset-load" | Presets::mutatedPresetsNeverCrashTheLoader | Setlist/pattern loaders skip magic and fail silently |
| Missing reference → factory fallback + named notification | 14 | yes | takeGuitarNotices, IrSlot lastError | banners "missing-part", "ir-missing" | Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce | |
| Corrupt file never overwritten; refused load prompts user | 14 | yes | loadPreset leaves file | banner | ErrorLog::arefusedPresetLoadIsRecordedAndChangesNothing | |
| Version discipline | 15 | n/a | process | n/a | - | |
| Test: every factory file round-trips byte-identical | 16 | partial | - | n/a | Workshop (spec equality), Presets::stateRoundTripsExactly | Not byte-identical for guitars/parts |
| Test: 10 000 mutated bytes | 16 | partial | - | n/a | Presets::mutatedPresetsNeverCrashTheLoader | Presets only |
| Test: migrations across 200 fixtures | 16 | no | - | n/a | - | |
| Test: missing guitar → banner + fallback | 16 | yes | - | n/a | WorkshopPresets::aMissingGuitarFileFallsBackToItsType | |
| Test: kill mid-save 100× | 16 | no | - | n/a | - | |
| Test: 60-day backup pruning | 16 | no | - | n/a | - | |

### factory-content.md

Summary: 34 requirements. **yes 9, partial 10, no 15.** Guitars (27), parts (148) and IRs (720) are shipped and exceed the spec. The factory preset list is still the old 33-preset M19 set, not the 36 named presets, and "Modern Overdrive" (the first-run default) does not exist. None of these ship on main: the 6 factory tunes, pattern/kit files, 10 setlists, 6 backing tracks and 12 MIDI clips. The clips are in progress on tune-help, at a different path.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| No trademarks in shipped names; legal review | 0.1 | partial | TrademarkTests; Tools/trademark_scan.py | n/a | Trademarks::* | Preset "Fuzz Face Lead" (FactoryPresets.cpp:265) still carries a trademark; no legal sign-off recorded |
| Tonal spread (36 covering the map) | 0.2 | partial | FactoryPresets.cpp (33) | Preset browser | Presets::everyFactoryPresetLoadsAndPlays | |
| Difficulty ladder (easy/medium/showcase per category) | 0.3 | no | - | - | - | |
| ≥2 presets per genre (12 guitar + bass genres) | 0.4 | no | - | - | - | Missing reggae, latin, indie, punk, and bass funk/reggae/punk/jazz |
| Every factory guitar playable at every preset | 0.5 | partial | guitar-block fallback by type | n/a | - | No cross-product test |
| Every factory tune loops, sounds finished | 0.6 | no | - | - | - | No factory tunes |
| No copyrighted third-party content | 0.7 | yes | IRs synthesised (make_irs.py) | n/a | - | |
| ≤ 200 MB compressed | 0.8 | yes | Resources ≈ 30 MB | n/a | manual du | |
| 36 named presets (Fresh Strings Clean … Jazz Walking Bass) | 1 | no | FactoryPresets.cpp ships a different 33 | Browser | - | Only "Modern Metal Chug" and "Flamenco Rasgueado" match |
| "Modern Overdrive" is the default first-run preset | 1 | no | - | - | - | |
| 15 named factory guitars | 2 | yes | Resources/Guitars/* (27 files) | Workshop / Guitar combo | Workshop::everyFactoryGuitarLoadsAndRoundTrips | "Selmer-Style" ships as "Gypsy Jazz" |
| Each guitar has a designed finish and distinct parts | 2 | yes | `.luthierguitar` finish blocks | Workshop | - | |
| ~90 parts across categories | 3 | yes | Resources/Parts (148) | Workshop | Workshop::theFactoryLibraryIsThere | |
| Named parts per category list | 3 | partial | generate_factory_parts.py | Workshop | - | Not diffed name by name |
| 6 factory tunes (Fingerstyle Etude … Funk Slap Groove) | 4 | no | Resources/Tunes/Templates (10 skeleton templates) | TUNE | - | tune-help adds a single Examples/04-iron-riff |
| ~28 `.luthierpattern` files incl. bass patterns | 5 | partial | Patterns.cpp (compiled in) | RHYTHM | RhythmPatterns::factoryPatternsAreWellFormed | No files; no bass patterns (Walking, Tumbao …) |
| ~28 `.luthierkit` files with voicer/humanize/pick/setup/noise style | 6 | partial | GenreKit.cpp (compiled in) | RHYTHM | GenreKits::factoryKitsAreWellFormed | No file format; no pick/setup/noise styles |
| 10 example setlists | 7 | no | - | - | - | |
| 6 royalty-free backing tracks in Resources/Practice/BackingTracks | 8 | no | - | - | - | Needs produced audio |
| 12 example MIDI clips in Resources/Examples/MIDI | 9 | no | - | - | - | In progress on tune-help (Resources/Examples/*.mid, 12 files, no MIDI/ subfolder) |
| 720 IRs (216 body + 504 cab) | 10 | yes | Resources/BodyIRs, CabIRs | TONE MATCH | - | |
| Post-release `.luthiercontent` packs | 11 | no | - | - | - | Deferred; format in progress on visual |
| Every user-facing name in locale catalog | 12 | partial | Localisation.cpp | n/a | - | Preset/guitar names not catalogued |
| Preset names are noun phrases | 12 | partial | - | - | - | "Shred Lead", "Drop C Riff" ok; "Fretless Mwah" borderline |
| Guitar names `[Style] [Family]` | 12 | yes | Resources/Guitars | - | - | |
| Part names state physical fact | 12 | yes | Resources/Parts | - | - | |
| Tune names < 32 chars | 12 | yes | templates | - | - | |
| Test: every preset in every host, no missing-ref banner | 13 | partial | - | - | Presets::everyFactoryPresetLoadsAndPlays | In-process only |
| Test: every guitar matches spectrum-delta fixture ±0.2 dB | 13 | no | Workshop/SpectrumDelta exists | - | - | No fixtures |
| Test: every factory tune plays end to end | 13 | no | - | - | - | |
| Test: every factory setlist resolves | 13 | no | - | - | - | |
| Test: backing tracks stream at 48 kHz | 13 | no | - | - | - | |
| Legal review sign-off per entry | 13 | no | - | - | - | |
| Content-size gate ≤ 200 MB | 13 | partial | - | - | manual | No automated gate |

### error-recovery.md

Summary: 82 requirements. **yes 16, partial 31, no 34, n/a 1.** The error log format, the preset load/refuse path, missing-reference banners and sample-rate/block-size changes are solid. Missing: newer-schema refusal (the build loads and logs instead), migration banner and backup (in progress on model-gaps/visual), MIDI flood/Learn timeout, the CPU relief banner, crash minidumps, corrupt-config rename, standalone device polling, licence demo mode, a 3-banner stack, a verbose-log toggle and log pruning. The only `ErrorLog::write` callers are PresetManager and PluginProcessor (16 sites).

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Never crash on bad input | 0.1 | partial | loaders | n/a | Presets::mutatedPresetsNeverCrashTheLoader | Presets only fuzzed |
| Never silently degrade (visible notice) | 0.2 | partial | NotificationCentre | banner strip | Editor::aFailedPresetLoadAndAMissingIrEachRaiseABannerOnce | Setlist load, A/B empty, save failures silent |
| Never destroy user work | 0.3 | partial | PresetManager backups | n/a | Presets::savingBacksUpTheVersionItReplaces | No guitar backup; no migration backup |
| Prefer partial success | 0.4 | yes | IR/part fallbacks | banners | Workshop::aMissingPartFallsBackAndSaysSo | |
| Every failure logged to errors-yyyymm.log | 0.5 | partial | Support/ErrorLog | n/a | ErrorLog::failuresAreLoggedAsReadableJsonLines | Only preset + processor paths log |
| Severity → banner / banner+halt / modal | 0.6 | partial | Notifications (info/warning) | banner | - | No modal path |
| File not found → named banner, keep state | 1 | yes | loadPreset "Preset not found: X" | banner | Editor test | Setlist/tune-other paths differ |
| Permission denied banner | 1 | partial | "could not be read, or is empty" | banner | - | Not distinguished |
| Not UTF-8 banner | 1 | partial | falls into JSON failure | banner | - | |
| Not JSON banner + suggest re-save | 1 | partial | "is not a Luthier preset" | banner | - | No re-save hint |
| Magic missing/wrong banner | 1 | yes | fromVar BAD_MAGIC + "was refused" | banner | Presets::aFileWithoutTheMagicMarkerIsRefused | Wording differs |
| Newer schema → refuse "made by a newer Luthier" | 1 | no | fromVar logs NEWER_SCHEMA and loads | no | - | Deviates (C-20) |
| Older schema with no migration refused | 1 | partial | schema ≤ 0 refused | banner (generic) | - | |
| Migration: backup original + 5 s info banner | 1 | no | - | no | - | In progress on model-gaps (backup) and visual (noteMigration banner) |
| Migration fails partway → original preserved | 1 | no | - | - | - | |
| Referenced file missing → info banner, load succeeds | 1 | yes | IrSlot, guitar parts | banner "ir-missing" (Tone Match action), "missing-part" | Editor test; WorkshopPresets test | |
| Referenced file corrupt → same | 1 | partial | IR decode failure falls back | banner | - | Not tested |
| Cyclic reference refused | 1 | no | - | - | - | Structure makes cycles unlikely |
| Save: folder not writable → banner | 2 | partial | writeToFile SAVE_UNWRITABLE log | Save As overlay stays open | - | No named banner |
| Save: disk full → same | 2 | partial | same | - | - | |
| Save: rename failed → temp deleted, banner | 2 | partial | TemporaryFile dtor; log | - | - | No banner |
| Concurrent save → later wins + warning banner | 2 | no | - | - | - | |
| Session recorder picks unique name | 2 | yes | Looper timestamped names | n/a | - | |
| SR change → reprepare, info banner | 3 | yes | claimSampleRateChange | banner "sample-rate" | Editor::aSampleRateChangeIsAnnouncedOnce…; Engine::sampleRateChangesAreSurvived | |
| Block-size change → reprepare | 3 | yes | prepareToPlay | n/a | Engine::blockSizeChangesAreSurvived | |
| Bus layout not advertised → false | 3 | partial | isBusesLayoutSupported | n/a | Routing::everyLayoutRendersCleanly | Accepts mono main |
| NaN/denormal guard + log module id + Diagnostics counter | 3 | partial | LuthierEngine `sanitise` | no | Engine::fastSlidesProduceNoNansOrDenormals | No log or counter |
| CPU overrun relief ladder + "CPU limit reached" banner | 3 | no | - | - | - | |
| Sustained overrun → offer to disable heaviest module | 3 | no | - | - | - | |
| Host underrun → log + counter | 3 | no | - | - | - | |
| Convolution failure → bypass + banner | 3 | partial | procedural cab fallback | no banner | Cabinet::procedualFallbackRemovesTheFizz | |
| Malformed MIDI → log, drop | 4 | partial | MidiInterpreter drops | n/a | - | Not logged |
| CC out of range → clamp | 4 | yes | MidiMessage 7-bit | n/a | - | |
| Unknown SysEx ignored | 4 | yes | MidiInterpreter | n/a | - | |
| Corrupt Luthier SysEx → log, drop | 4 | partial | - | n/a | - | Not logged |
| MIDI flood > 5000/s → throttled banner | 4 | no | - | - | - | |
| MIDI Learn 30 s timeout → disarm + banner | 4 | no | MidiLearn has no timer | no | - | |
| Part swap under CPU → queue to block boundary | 5 | partial | WorkshopBench swap parking | Workshop | WorkshopSwap::aPartSwapDuringANoteIsClickFree | |
| Incompatible part → warning / refuse via API | 5 | partial | advisory warning (C-21) | Workshop card | Workshop::incompatiblePartsFitWithAWarning | Spec's refusal rejected by decision |
| Family change during playback → info banner | 5 | no | - | - | - | |
| Shadow audition timeout 200 ms → revert | 5 | no | - | - | - | |
| Invalid coefficients → refuse swap + banner | 5 | no | - | - | - | |
| Invalid `.luthierguitar` save refused with reason | 5 | partial | PluginEditor save-guitar failed banner | banner | - | Generic reason |
| Malformed chord text → red token + status line | 6 | partial | TuneHarmony named parse errors | TUNE (text) | TuneBuilder::malformedShorthandIsRefusedWithANamedError | No red token highlight on main |
| Melody generation yields nothing → banner | 6 | no | - | - | - | |
| Sung capture low confidence → banner | 6 | no | - | - | - | |
| Removing last section refused + banner | 6 | no | - | - | - | |
| Loop lookahead truncated indicator | 6 | no | - | - | - | |
| Snapshot recall during preset load queued | 7 | partial | both on message thread (serial) | n/a | - | |
| Snapshot recall while looper records → state-boundary event | 7 | no | - | - | - | |
| Preset load mid-tune → next section boundary + banner | 7 | no | - | - | - | |
| Undo with nothing → disabled / no-op | 7 | yes | HeaderBar::updateUndoRedoState; canUndo | Header Undo button | - | |
| A/B with empty B → banner | 7 | no | recallSlot silently no-ops | no | - | |
| Update check error → silent + log | 8 | partial | Telemetry | n/a | Telemetry::noNetworkIsSilentRatherThanAnError | Not written to ErrorLog |
| Update download interrupted → discarded | 8 | no | - | - | - | In progress on visual (UpdateDownloader) |
| Crash upload failure → banner with path | 8 | no | - | - | - | |
| Licence activation offline → offline flow | 8 | partial | Telemetry licence | Options | Telemetry::revalidationCountdownAndOfflineTolerance | |
| Revalidation failed in grace → countdown banner | 8 | yes | PluginEditor "licence-grace" | banner | same | |
| Grace expired → modal + demo mute 2 s / 60 s | 8 | no | - | - | - | |
| Host closes instance → save state, release | 9 | yes | JUCE lifecycle | n/a | - | |
| Host crash → restore from host state | 9 | yes | setStateInformation | n/a | Presets::stateRoundTripsExactly | |
| Standalone audio device lost → poll, auto-recover, banner | 9 | no | - | - | - | |
| Standalone MIDI input lost → same | 9 | no | - | - | - | |
| User config unreadable → rename `.corrupted-<ts>`, defaults, banner | 10 | no | UiPreferences::load treats it as absent | no | - | File is silently overwritten on next save |
| User config invalid schema → same | 10 | no | - | - | - | |
| Missing expected folder → create + log | 10 | partial | created lazily on write | n/a | - | Not logged |
| Content-update folder missing → log, not installed | 10 | n/a | no content updates on main | - | - | |
| No writable Documents → prompt for location | 11 | no | - | - | - | FirstRun is in Source/WIP (not built), moved to build on tune-help |
| OS below minimum → modal + exit | 11 | no | - | - | - | |
| No audio device (Standalone) → prompt | 11 | no | JUCE default | - | - | |
| Crash → minidump `crash-<ts>.dmp` | 12 | no | Telemetry only reads `crash-*.dmp`; no writer | - | - | No `setApplicationCrashHandler` |
| Troubleshooting bundle captured | 12 | partial | Diagnostics::writeTroubleshootingReport (manual) | Options → Diagnostics | - | Not at crash |
| Next launch, opted in → upload prompt | 12 | partial | "crash" banner → Privacy page | banner | - | Never fires (no dumps written) |
| Next launch, opted out → info banner with path | 12 | partial | same banner, no path | banner | - | |
| Log format: JSON lines ts/severity/module/code/message/context | 13 | yes | ErrorLog::write | n/a | ErrorLog::failuresAreLoggedAsReadableJsonLines | |
| debug/info only when Diagnostics verbose on | 13 | partial | ErrorLog::setVerbose | no | - | setVerbose never called; no toggle on Options → Diagnostics |
| Monthly rotation | 13 | yes | getLogFile(yyyymm) | n/a | - | |
| Old logs pruned by 30-day sweep | 13 | no | ErrorLog::pruneOldLogs | n/a | - | Never called |
| ≤ 3 banners visible, 4th replaces oldest | 14 | no | NotificationCentre shows 1, queues rest | banner strip | Editor::notificationBannersQueueDismissAndRespectTheirActions | Deviates (C-22) |
| Priority error > warning > info | 14 | partial | Level {info, warning} | - | - | No error level; no priority order |
| Auto-dismiss 5 s unless action | 14 | yes | autoDismissMs | - | same | |
| Every failure mode has fixture in Tests/Fixtures/Errors | 15 | no | - | - | - | Folder absent |

### state-model.md

Summary: 73 requirements. **yes 28, partial 23, no 22.** The "never touches" rules for preset load and snapshot recall hold and are tested. The structural deviations are these. Loads apply on the message thread (no command/result queue). Snapshots, modulation and MIDI mappings are not part of preset files, so a preset load does not swap them. There are no undo boundaries (in progress on visual) and no State Inspector. Almost none of the section-8 intersection behaviours are built: A/B clear, freeze/E-Bow clear, feedback damp, the tune section-boundary wait, the setlist override flag, looper state-boundary events and the bench save prompt. The only tests are the 4 in StateModelTests.cpp.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| Layers nest (guitar + snapshots inside preset) | 0.1 | partial | guitar block in preset | n/a | StateModel::recallingASnapshotStaysInsideThePreset | Snapshots are in host state, not in the preset file |
| Load atomic per layer at audio-thread swap | 0.2 | partial | message-thread apply + bridge.applyAllNow | n/a | StateModel::loadingAPresetWhileRenderingProducesNoGarbage | |
| Every "load X while Y" has a policy | 0.3 | partial | see 8.x | - | - | |
| Undo respects state boundaries | 0.4 | no | - | - | - | In progress on visual (UndoHistory boundary) |
| Structural state via command/result queue | 0.5 | no | - | - | - | Direct calls on the message thread |
| uiState not in presets | 0.6 | yes | UiState in host blob only | n/a | StateModel::loadingAPresetLeavesTheLayersAboveItAlone | Undo restores uiState as a side effect (fixed on visual) |
| User-global settings layer | 1 | yes | UiPreferences, Telemetry | Options | - | |
| Per-instance uiState (mode, tab, Live, Slide, bench A/B, drawer) | 1 | partial | UiState struct in "ui" | n/a | - | JSON, not a ValueTree; drawer/bench slots not all present |
| Session state (undo, arm, tap, recorder, A/B) not saved | 1 | yes | processor members | n/a | - | slotBActive flag is saved (not the buffers) |
| Setlist layer | 1 | yes | Live/Setlist | LIVE tab, LiveStrip | LiveTests | |
| Tune layer | 1 | yes | Tune/TuneSession | TUNE tab | TuneBuilder tests | |
| Snapshot up to 128 | 1 | yes | SnapshotBank | LIVE tab | LiveSnapshots::programChangeMapsAcrossAllOneTwentyEight | |
| Guitar / part layers | 1 | yes | PartLibrary, Part | Workshop | Workshop tests | |
| Loop sibling, references preset by name | 1 | partial | Looper | PRACTICE | - | |
| Preset load: parse, override/reference/fallback + banner | 2 | yes | PresetManager + onGuitarBlockLoaded | banner | WorkshopPresets::aMissingGuitarFileFallsBackToItsType | |
| Resolve referenced IRs and patterns | 2 | partial | - | - | - | IRs and patterns are not in preset files |
| Post LoadPresetCommand to audio thread | 2 | no | - | - | - | |
| Apply parameters | 2 | yes | fromVar setValueNotifyingHost | - | Presets::stateRoundTripsExactly | |
| Swap GuitarSpec atomically | 2 | partial | bridge / engine swap with fade | - | - | |
| Swap mod matrix, snapshot bank, MIDI mappings | 2 | no | not in preset file | - | - | |
| Apply ranges, clamp out-of-range | 2 | yes | RangeState::applyTo | - | Ranges::narrowingClampsAndReportsTheCount | |
| Crossfade 5-30 ms per module | 2 | partial | guitar 5 ms fade | - | - | |
| Push state boundary | 2 | no | plain "Load preset" entry from header/browser only | - | - | In progress on visual |
| Push missing-ref/clamp notices to banner | 2 | partial | missing-part/ir-missing banners | banner | - | No clamp banner |
| Never touches user-global/uiState/session/setlist/tune/loop | 2 | yes | - | - | StateModel::loadingAPresetLeavesTheLayersAboveItAlone | |
| Snapshot recall: apply params | 3 | yes | SnapshotBank | LIVE tab, LiveStrip | LiveSnapshots tests | |
| Snapshot recall: modulation diff | 3 | partial | - | - | - | |
| Snapshot recall: `snapshot_xfade_ms` 30 ms | 3 | yes | Snapshots.cpp crossfadeMs | LIVE tab | LiveSnapshots::recallCrossfadesContinuousAndStepsDiscrete | |
| Discrete params switch at crossfade midpoint | 3 | yes | Snapshots.cpp | - | same | |
| Delay/reverb tails preserved | 3 | partial | - | - | - | Not verified |
| Recall never touches file/ranges/guitar/recorder/upper | 3 | yes | - | - | StateModel::recallingASnapshotStaysInsideThePreset | |
| Recall pushes undo entry (not boundary) | 3 | no | - | - | - | In progress on visual |
| Tune load: lazy per-section presets | 4 | partial | TuneSession | TUNE | - | Snapshot sections on tune-help |
| Tune bundle extraction to temp | 4 | no | - | - | - | |
| Tune load stops playback | 4 | yes | TuneSession::newTune | TUNE | - | |
| Tune load pushes boundary | 4 | partial | TuneSession own undo stack | TUNE | - | Separate stack from the plugin's Ctrl-Z |
| Setlist load: verify refs, flag unresolved | 5 | no | Setlist::loadFrom | LiveStrip | - | Failure silent |
| Setlist in session state | 5 | yes | SetlistPlayer | - | LiveSetlist tests | |
| Load first entry's preset + snapshot | 5 | partial | applyCurrentSetlistEntry | LiveStrip | - | **Bug:** snapshot index recalled from the stale bank (preset data has no snapshots) |
| Guitar load: resolve parts with fallback banner | 6 | yes | PartLibrary load report | banner | Workshop::aMissingPartFallsBackAndSaysSo | |
| Guitar load: swap, refill, reprepare | 6 | yes | processor guitar loader | - | WorkshopPresets tests | |
| Guitar load: 30 ms crossfade | 6 | partial | 5 ms fade | - | - | |
| Preset params apply on top of new guitar | 6 | yes | - | - | WorkshopPresets::aStateLoadKeepsItsOwnRefinements | |
| Guitar load pushes boundary | 6 | no | - | - | - | In progress on visual (family switch) |
| Part swap: own undo entry, not boundary | 7 | yes | WorkshopBench "Fitted X (was Y)" | Workshop | WorkshopBenchTests | |
| 8.1 load supersedes pending recall | 8.1 | partial | serial on message thread | - | - | |
| 8.1 tune: pause at section boundary ≤ 4 s + banner | 8.1 | no | - | - | - | |
| 8.1 setlist "user override" flag | 8.1 | no | - | - | - | |
| 8.1 looper/recorder capture state-boundary event | 8.1 | no | - | - | - | |
| 8.1 MIDI Learn arm persists | 8.1 | yes | - | - | StateModel::loadingAPresetLeavesTheLayersAboveItAlone | |
| 8.1 bench unsaved → Save/Discard/Cancel prompt | 8.1 | no | - | - | - | |
| 8.1 Slide / Live / drawer persist | 8.1 | yes | - | - | same test (Live) | |
| 8.1 A/B clears + banner "A/B cleared by preset load" | 8.1 | no | - | - | - | Test asserts the A/B slot is kept (opposite of spec) |
| 8.1 Freeze layer clears | 8.1 | no | FreezeOverlay | - | - | |
| 8.1 E-Bow clears | 8.1 | no | - | - | - | |
| 8.1 feedback loop damps 100 ms | 8.1 | no | - | - | - | |
| 8.1 held notes decay through new params | 8.1 | partial | - | - | - | |
| 8.2 recall: held notes/tune continue | 8.2 | partial | - | - | LiveSnapshots::thousandRecallsNeverJumpAParameter | |
| 8.2 recall: setlist, looper, Freeze/E-Bow, A/B, feedback rules | 8.2 | no | - | - | - | |
| 8.3 tune load intersections (setlist stays, Live/Workshop persist, stop + bar 0) | 8.3 | partial | TuneSession | - | - | Looper boundary missing |
| 8.4 guitar load: string-count change, prompt, swap discard, looper, slide banner | 8.4 | partial | swap parking | - | - | No prompt/banner/boundary |
| 8.5 part swap: held-note crossfade, queue, audition drop | 8.5 | yes | WorkshopBench | Workshop | WorkshopSwap::aPartSwapDuringANoteIsClickFree | Queue/audition not tested |
| 8.6 ranges toggle clamps automated/modulated value | 8.6 | yes | RangeState clamp | Options → Ranges | Ranges::narrowingClampsAndReportsTheCount | Recorder boundary no |
| 8.7 MIDI Learn arm vs flood/recall/load | 8.7 | partial | - | - | - | "Disarm if target missing" not built |
| 8.8 undo while held notes/tune/looper | 8.8 | partial | snapshot undo | - | - | No tune boundary |
| 9 save: preset captures current snapshot bank | 9 | no | toVar omits snapshots | - | - | |
| 9 save: waits for in-flight part swap / committed not shadow / tune live edits | 9 | partial | bench parking; TuneSession | - | - | |
| 10 host state persists params, matrix, bank, mappings, ranges, guitar ref, uiState | 10 | yes | getStateInformation | n/a | Presets::stateRoundTripsExactly | |
| 10 not persisted: undo, A/B buffers, arm, recorder, freeze, feedback, shadow | 10 | yes | - | - | - | |
| 11 multi-instance independence | 11 | yes | per-instance objects | - | - | Untested; ExpressionCalibrationSet is global |
| 12 Options → Diagnostics "State Inspector" at 4 Hz | 12 | no | DiagnosticsPage has none | no | - | |
| 13 intersection tests in Tests/StateModel/Intersections | 13 | no | 4 tests only | - | StateModelTests.cpp | |
| 13 fuzz 10 000 random ops | 13 | no | - | - | - | |

### host-integration.md

Summary: 60 requirements. **yes 29, partial 21, no 9, n/a 1.** Formats, bus layouts A-D, the sidechain, latency, program enumeration and the Ableton program-change swallow are all present. Main defects: `NEEDS_MIDI_OUTPUT FALSE` in CMakeLists.txt means the VST3/AU wrappers expose **no MIDI out port**, even though `producesMidi()` returns true (JUCE's VST3 wrapper is gated on `JucePlugin_ProducesMidiOutput`). A mono main output is accepted. There are no parameter groups (VST3 units), no localised parameter names and no format-version tag in the state blob. Host time signature and record state are not read. `docs/HOST_COMPATIBILITY.md` is missing.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| No blocking on audio thread; no stdout | 0.2 | partial | - | n/a | pluginval (manual 2026-09-19) | updateLatency → routing.setLatencyReport runs every block |
| Announce capabilities correctly | 0.3 | partial | CMake flags | n/a | - | MIDI out not announced (NEEDS_MIDI_OUTPUT FALSE) |
| Host transport wins; free-run when stopped | 0.4 | yes | tapTempo.getEffectiveBpm | n/a | RhythmPatterns::silentWhenStoppedUnlessFreeRunning | |
| No IR blobs in state | 0.6 | yes | IrSlot path refs | n/a | ToneMatch tests | |
| VST3 all platforms | 1 | yes | CMake FORMATS | n/a | - | |
| AU on macOS | 1 | yes | CMake `if(APPLE)` AU | n/a | - | spec-coverage row stale |
| CLAP v1.1 / AAX v1.5 | 1 | n/a | CLAP optional via ThirdParty | n/a | - | Deferred by spec |
| Standalone everywhere | 1 | yes | CMake | n/a | - | |
| Version = LUTHIER_VERSION + build string | 1 | partial | JucePlugin_VersionString 1.0.0 | n/a | - | No build string |
| Layouts A-D (8 stereo aux, 12 mono per-string) | 2 | yes | buildBusesProperties (7 aux + 12 strings + Aux 8) | ROUTING tab | PluginBuses::*; Routing::everyLayoutRendersCleanly | |
| Any subset accepted | 2 | yes | isBusesLayoutSupported | n/a | same | |
| Mono main output rejected | 2 | no | accepts mono (PluginProcessor.cpp:285) | n/a | - | Deviates (C-26) |
| Optional stereo sidechain on all layouts | 2 | yes | `.withInput("Sidechain", stereo, false)` | ROUTING | - | Mono also accepted |
| Layout change → prepare, no crash | 2 | partial | JUCE | n/a | pluginval manual | |
| APVTS is the single source | 3 | yes | Parameters::createLayout | n/a | Parameters::everyParameterHasAUniqueIdAndSaneDefault | |
| Parameter count stable | 3 | yes | - | n/a | - | |
| Grouped by ParameterCategory | 3 | no | flat layout, no AudioProcessorParameterGroup | n/a | - | |
| Display names translated per locale | 3 | no | hard-coded English | n/a | - | |
| Range, default, text↔value converters | 3 | yes | floatParam/choiceParam | n/a | Parameters::everyParameterTextRoundTrips | |
| Internal changes notify host | 3.1 | yes | setValueNotifyingHost | n/a | - | |
| Only last write per block notified | 3.1 | partial | JUCE default | n/a | - | |
| Automation moves base, modulation on top | 3.2 | yes | ModMatrix | n/a | ModulationTests | |
| Discrete params integer + module crossfade | 3.3 | yes | choice params | n/a | Modulation::discreteDestinationsStepAtBoundaries | |
| State blob: format version tag (u32) | 4 | no | JSON root, no version | n/a | - | |
| APVTS XML + uiState XML | 4 | partial | JSON (params inside `preset`, `ui`) | n/a | Presets::stateRoundTripsExactly | Equivalent content, different encoding |
| Structural: matrix, snapshots, mappings, ranges, guitar ref/inline, circuit | 4 | yes | getStateInformation | n/a | same | |
| MIDI export profile in state | 4 | partial | routing midiOut config | n/a | - | |
| Size < 200 KB typical / < 2 MB | 4 | partial | - | n/a | - | Never measured |
| setStateInformation applies via swap pattern | 4 | partial | direct apply on message thread | n/a | StateModel::loadingAPresetWhileRenderingProducesNoGarbage | |
| Older build + newer blob: keep unknown sections on write-back | 4.1 | partial | preset unknownFields only | n/a | - | Unknown host-level keys dropped |
| Newer build + older blob: migrate + back up old blob | 4.2 | partial | preset migrations | n/a | - | No blob backup |
| getLatencySamples = main-out latency | 5 | yes | updateLatency → setLatencySamples | ROUTING shows per output | Engine::latencyIsReportedAndPlausible | |
| Latency change → updateHostDisplay | 5 | yes | JUCE setLatencySamples calls it | n/a | - | Called from audio thread |
| Per-output latency to host | 5 | partial | RoutingMatrix::LatencyReport (UI only) | ROUTING | - | JUCE cannot report per bus |
| Read tempo, isPlaying, ppq each block | 6 | yes | processSlice getPlayHead | n/a | RhythmScheduler tests | |
| Read time signature, isRecording, host sample rate | 6 | no | - | n/a | - | Metronome keeps its own signature |
| Missing playhead → internal transport | 6 | yes | tapTempo | n/a | - | |
| acceptsMidi true | 7 | yes | PluginProcessor.h | n/a | - | |
| producesMidi true, MIDI out usable in hosts | 7 | partial | producesMidi() true; CMake NEEDS_MIDI_OUTPUT FALSE | MIDI OUT panel | Routing::midiOutPassThroughIsSampleExact (in-process) | **VST3/AU have no MIDI output bus** |
| Clock/transport/PC/SysEx accepted | 7 | partial | PC → snapshot, CC0 → preset | n/a | - | No MIDI clock follow |
| MPE full support | 7 | yes | MidiInterpreter MPE | n/a | - | |
| Sample-accurate MIDI in/out | 7 | yes | MidiOutRouter | n/a | Routing::midiOutPassThroughIsSampleExact | |
| Instances independent | 8 | yes | - | n/a | - | Untested |
| Ableton: swallow first PC after state restore | 9.1 | yes | ignoreNextProgramChange | n/a | StateModel::aProgramChangeRightAfterAStateRestoreDoesNotWipeIt | Undo and A/B also set the flag, so the next real PC is swallowed (fixed for undo on visual) |
| Ableton MPE auto-detect | 9.1 | partial | MidiInterpreter | n/a | - | Not verified |
| Logic: state < 500 KB | 9.2 | partial | refs not blobs | n/a | - | Not measured |
| Logic: PC mapping mode in routing panel | 9.2 | no | - | no | - | |
| prepareToPlay idempotent + fast | 9.2 | partial | - | n/a | - | |
| Cubase/StudioOne/Reaper/FL/Bitwig notes | 9.3-9.7 | partial | - | n/a | - | No host matrix run |
| Standalone device polling, virtual MIDI-out, min size, native dialogs | 9.9 | partial | JUCE standalone; min 900×540 | n/a | - | No polling |
| pluginval strictness 10 every merge | 10 | partial | manual run | n/a | - | scripts/pluginval.sh on visual |
| Plugin undo not integrated with host undo | 11 | yes | processor stack | Header | - | |
| Factory presets via getNumPrograms/getProgramName | 12 | yes | PluginProcessor::getNumPrograms | host browser | - | User presets included, so the count changes on save |
| Threading contract (state save doesn't block audio) | 13 | partial | message-thread capture | n/a | - | |
| VST3 units per column-4 tab / Easy strip | 14.1 | no | - | n/a | - | Needs parameter groups |
| AU cocoa view standard | 14.2 | yes | JUCE | n/a | - | |
| State chunk + typed params | 14.3 | yes | - | n/a | - | |
| Tests: SR/block change mid-play | 15 | yes | - | n/a | Engine::sampleRateChangesAreSurvived / blockSize | |
| Tests: 32 instances, VST3↔AU switch, per-host round trip and MIDI I/O, transport follow | 15 | no | - | n/a | - | |
| `docs/HOST_COMPATIBILITY.md` | 16 | no | - | n/a | - | Missing |

### action-and-undo.md

Summary: 49 requirements on the integration base. **yes 17, partial 11, no 21.** The main build is a whole-state snapshot stack: max 200, one entry per host gesture, entries carrying only a description. There are no action classes, no 200 ms grouping, no boundaries, no Ctrl-Y / Ctrl-Alt-Z, no history dropdown and no undo depth readout. Mod-matrix, snapshot, MIDI Learn, pedal add/move/remove, setlist and practice edits push nothing. Undo also restores view state (mode, tab, Live Mode) and sets the "ignore next program change" flag. **Most of this is in progress on `luthier-visual`**: Support/UndoHistory.* has class, target, 200 ms merge and boundaries; `pushUndoBoundary` is used for preset, family and setlist; there is a HeaderBar history menu and Ctrl-Y / Ctrl-Alt-Z; UndoTests.cpp has 20 tests. That branch still lacks the "Show Undo Depth" counter, a search filter, and the family-switch warning.

| Requirement | Section | Implemented? | Where | GUI reachable? | Test | Notes |
|---|---|---|---|---|---|---|
| One user change = one entry | 0.1 | yes | parameterGestureChanged | Header Undo | Undo::stepsOneActionAtATimeBothWays (RangesUiTests) | |
| Boundaries; Shift-Ctrl-Z to cross | 0.2 | no | - | - | - | In progress on visual |
| 200 ms same-class/target grouping | 0.3 | no | gesture-based | - | - | In progress on visual (UndoHistory::kGroupWindowMs) |
| Never undoable: live audio/MIDI, recorder, banners | 0.4 | yes | gesture-only entries | - | - | |
| Per-instance stack | 0.5 | yes | processor member | - | - | |
| Not persisted across sessions | 0.6 | yes | - | - | - | |
| Entry: class, target, before/after, timestamp, description, boundary | 1 | partial | UndoEntry {state, redoState, description} | - | - | visual adds the rest |
| Undo History dropdown (View menu) | 1/9 | no | - | no | - | In progress on visual (HeaderBar::buildUndoHistoryMenu) |
| Max 200 entries, oldest dropped | 2 | yes | kMaxUndoSteps | - | - | Overflow test only on visual |
| Redo cleared on new action | 2 | yes | addUndoEntry | - | Undo::stepsOneActionAtATimeBothWays | |
| 3.1 param description "Change X from A to B" | 3.1 | partial | "Change " + name | tooltip | - | visual adds from/to |
| 3.1 pause > 200 ms splits a drag | 3.1 | partial | one entry per gesture | - | - | |
| 3.1 no entries from modulation/automation/MIDI CC | 3.1 | yes | gesture + message-thread check | - | - | visual: Undo::writesWithoutAGestureMakeNoEntries |
| 3.2 discrete switch rapid scroll → one entry | 3.2 | partial | one per combo gesture | - | - | |
| 3.3 toggles "Turn on/off X" | 3.3 | partial | gesture entries "Change X"; Slide Mode explicit | Header | - | visual: Undo::aToggleSaysTurnOnOrOff |
| 3.4 part swap: one entry "Change slot to part" | 3.4 | yes | WorkshopBench pushUndoState "Fitted X (was Y)" | Workshop | WorkshopBenchTests | Wording differs |
| 3.4 family switch = boundary | 3.4 | partial | "Change guitar family" plain entry | Workshop | - | Boundary on visual |
| 3.5 illustration drag "Move handle X mm → Y mm" | 3.5 | partial | GuitarBodyComponent "Detune string"; bench handles via params | Guitar illustration | - | No mm wording |
| 3.6 mod-matrix create/delete/edit/source entries | 3.6 | no | ModMatrixPanel pushes nothing | - | - | visual: Undo::rightClickModulationIsUndoable |
| 3.7 snapshot save/recall/rename/colour/delete/move | 3.7 | no | LivePanel pushes nothing | - | - | visual: Undo::snapshotSaveAndRecallAreEntries |
| 3.8 preset load = boundary "Load preset [name]" | 3.8 | partial | "Load preset" plain entry (HeaderBar, Overlays, PluginEditor) | Header | - | Host PC and setlist step push nothing; visual has named boundary |
| 3.8 save/rename not on stack | 3.8 | yes | - | - | - | |
| 3.9 tune entries + tune-load boundary | 3.9 | partial | TuneSession own undo (TuneModel classes) | TUNE | TuneBuilder tests | Separate from plugin stack; plugin undo overwrites tune state (fixed on tune-help/visual) |
| 3.10 setlist load/step/edit entries | 3.10 | no | - | - | - | visual: boundary on load/step |
| 3.11 ranges toggle entry | 3.11 | yes | changeRanges → pushUndoState | Options → Ranges | Undo::stepsOneActionAtATimeBothWays | |
| 3.12 MIDI Learn learn/delete entries | 3.12 | no | - | - | - | visual: Undo::aMidiLearnIsOneEntry |
| 3.13 pedal param/bypass entries | 3.13 | yes | slot attachments (gestures) | FX racks | - | |
| 3.13 pedal add/remove/move entries | 3.13 | no | PedalRack writes without gestures | - | - | visual: Undo::movingAPedalIsOneEntry |
| 3.14 practice entries; layer delete restorable | 3.14 | no | - | - | - | visual: Undo::aClearedLoopLayerComesBack |
| 3.15 character control entries | 3.15 | partial | APVTS character params via gestures | CHARACTER | - | CharacterEngine seed/wear edits push nothing |
| 3.17 UI state changes not undoable | 3.17 | partial | toggles push nothing | - | - | But undo restores advancedMode/tab/liveMode from the snapshot; visual: Undo::doesNotMoveTheViewOrTheTune |
| 4 merge rules | 4 | no | - | - | - | visual: Undo::gesturesGroupWithin200ms |
| 5 Ctrl-Z stops at boundary, boundary itself undoable | 5 | no | - | - | - | visual |
| 5 dropdown separator + "Preset: [name]" subtitle | 5 | no | - | - | - | visual (menu separators) |
| 6 multi-target actions one atomic entry | 6 | yes | whole-state snapshot | - | - | |
| 7 skip list (A/B flip, panic, tap, browsing …) | 7 | yes | - | - | - | |
| 8 family-switch undo warns about lost parts | 8 | no | - | - | - | |
| 8 undo after save leaves file unchanged | 8 | yes | - | - | - | |
| 9 Ctrl/Cmd-Z undo | 9 | yes | Accessibility "undo" | keyboard + Header | Accessibility::shortcutDefaultsMatchTheCanonicalTable | |
| 9 Ctrl-Shift-Z redo | 9 | yes | "redo" | keyboard + Header | same | |
| 9 Ctrl-Y redo (Windows) | 9 | no | - | - | - | visual adds "redoAlt" |
| 9 Ctrl-Alt-Z undo across boundary + confirm banner | 9 | no | - | - | - | visual adds "undoAcrossBoundary" + banner |
| 9 dropdown click-to-point + search filter | 9 | no | - | - | - | visual: click-to-point (Undo::theHistoryListsNewestFirstAndUndoesToAPoint); no search |
| 11 host automation not undoable | 11 | yes | gesture-only | - | - | |
| 12 Options → Diagnostics "Show Undo Depth" footer | 12 | no | getNumUndoSteps exists | no | - | Not on visual either |
| 13 per-class fixture tests | 13 | no | - | - | - | visual has 20 UndoTests |
| 13 overflow 250 → oldest 50 dropped | 13 | no | - | - | - | visual: Undo::overflowDropsTheOldest |
| 13 199/201 ms grouping test | 13 | no | - | - | - | visual |
| 13 undo mid-play no dropouts | 13 | no | - | - | - | visual: Undo::undoMidPlayProducesNoGarbage |

### Top gaps (group H)

Ranked by user impact:

1. **Preset files drop snapshots, the mod matrix, MIDI Learn, rhythm-engine state, character seed/wear, tone-match IRs and routing.** `PresetManager::toVar/fromVar`. A user who builds a sound with modulation or snapshots and hits Save loses them on reload. The same cause breaks setlist snapshot recall (`applyCurrentSetlistEntry`). This is also true on every helper branch.
2. **No MIDI output port in VST3/AU.** `CMakeLists.txt NEEDS_MIDI_OUTPUT FALSE` contradicts `producesMidi()`. Everything in MIDI OUT (tune parts to MIDI, live MIDI out, Luthier SysEx) is unreachable in a DAW.
3. **The undo model** (no boundaries, grouping, history list, Ctrl-Y/Ctrl-Alt-Z; mod-matrix, snapshot, learn and pedal-move edits are not undoable; undo moves the view). Largely done on `luthier-visual`, so merging it is the fix. The Undo Depth counter is still missing there.
4. **Factory preset set is not the spec's 36.** No "Modern Overdrive" default, no genre ladder, and "Fuzz Face Lead" is still a trademarked name.
5. **No factory tunes, setlists, backing tracks or pattern/kit files ship.** MIDI clips are only on tune-help, at `Resources/Examples/` instead of `Examples/MIDI/`.
6. **No setlist save.** `Setlist::saveTo` is never called from UI. The setlist file schema diverges (`format`, `preset_path`) and load failures are silent.
7. **No crash minidump writer.** The "crash" banner and upload flow can never fire.
8. **Newer-schema presets load silently (logged only)** instead of being refused with a banner. There is no migration banner or migration backup (in progress on model-gaps/visual).
9. **Section 8 intersections are missing.** On preset load: A/B clear + banner, Freeze/E-Bow clear, feedback damp, the tune section-boundary wait, the setlist override flag, the bench unsaved prompt, looper state-boundary events.
10. **Mono main output is accepted** by `isBusesLayoutSupported`, contrary to the spec.
11. **No VST3 units / parameter groups, and no localised parameter names.** Hosts show hundreds of flat, English-only parameters.
12. **Banner system shows one banner at a time with only info/warning levels.** The spec wants up to 3, with error priority.
13. **Corrupt user config is silently discarded and later overwritten.** There is no `.corrupted-<ts>` backup and no banner.
14. **Error log:** `pruneOldLogs` is never called, and the verbose toggle is missing from Options → Diagnostics. Only preset and processor paths log; there is no NaN or underrun counter.
15. **MIDI Learn has no 30 s timeout.** No MIDI flood throttle or banner.
16. **No guitar-file backup on overwrite.** Tune `.backup` is never pruned.
17. **Standalone doesn't poll for or recover from a lost audio/MIDI device.** There is no demo-mode behaviour on licence-grace expiry.
18. **Host transport:** time signature and record state are ignored. There is no program-change mapping mode for Logic. `docs/HOST_COMPATIBILITY.md` is missing.
19. **No Options → Diagnostics "State Inspector".**
20. **Missing tests:** kill-mid-save atomicity, 200-fixture migration, 60-day prune, state-model intersection tests and fuzz, and the per-guitar spectrum-delta fixture.

### Unspecified gaps noticed

- **Program list is unstable for hosts.** `getNumPrograms` includes user presets, so a host's program numbers shift whenever the user saves a preset. Buyers expect stable factory program numbers.
- **Undo, A/B recall and setlist steps all go through `setStateInformation`.** Each of these sets `ignoreNextProgramChange`, so a real host or MIDI program change right after one is swallowed. visual fixes this only for undo.
- **Undo restores the whole state, tune included, on main.** An undo can silently revert the tune being edited (fixed on tune-help/visual).
- **No preset "dirty" prompt before loading another preset or closing.** Users of a $200 plugin expect an "unsaved changes" guard for presets, not only for Workshop guitars.
- **No preset import/export of a bundle** (preset + guitar + IRs) for sharing. The spec only covers tunes ("bundle").
- **No factory-content repair or restore** (re-install factory presets or guitars after the user deletes them). `ensureFactoryPresetsInstalled` only runs if the folder is missing.
- **MP3 backing tracks don't decode on Linux** (JUCE_USE_MP3AUDIOFORMAT is not set).

### Small glue candidates

- **CMakeLists.txt:** `NEEDS_MIDI_OUTPUT TRUE`. One line, and it unlocks the entire MIDI OUT panel in hosts.
- **PluginProcessor::isBusesLayoutSupported:** return false for a mono main output. One line.
- **PresetManager::toVar/fromVar:** add hooks like the existing `captureGuitarBlock`/`onGuitarBlockLoaded` that write and read `modulation` (`modMatrix.toVar`), `snapshots` (`snapshots.toVar`), `midiLearn`, `rhythm` (`getRhythmEngine().toVar`), `character`, `toneMatch` and `routing`. The keys are already in fromVar's known list and the serialisers already exist in getStateInformation.
- **LiveStrip setlist menu:** add a "Save setlist…" item calling `Setlist::saveTo`, plus a banner when `processor.loadSetlist` returns false.
- **Options → Diagnostics page (`DiagnosticsPage`):** add a "Verbose log" toggle → `ErrorLog::setVerbose`, and a "Show undo depth" toggle → footer label from `getNumUndoSteps()` and `canRedo`.
- **PresetManager constructor:** call `ErrorLog::pruneOldLogs()` beside `pruneOldBackups()`. Add a `TuneFile .backup` prune.
- **PluginEditor::pollForNotifications:** post an "A/B: B slot is empty" banner when `setSlotBActive(true)` finds an empty `slotB`. Post an "A/B cleared by preset load" banner and reset `slotB` in the preset-load path.
- **HeaderBar / PluginEditor keymap:** add Ctrl-Y → redo (visual already has `redoAlt`).
- **UiPreferences::load:** on a parse failure, rename to `.corrupted-<ts>` and post a "Preferences reset" banner.
- **MidiLearnManager:** start a 30 s timer on `setArmed(true)` that disarms and posts "MIDI Learn cancelled".
- **FactoryPresets.cpp:** rename "Fuzz Face Lead" (e.g. "Vintage Fuzz Lead"), add a legacy-name mapping, and add a "Modern Overdrive" recipe (Vintage Single-Cut) as the default first-run preset.
- **Parameters::createLayout:** wrap `add(...)` calls in `AudioProcessorParameterGroup`s per column-4 tab. The IDs are unchanged, so this is safe for saved state.
- **Merge `luthier-visual`'s UndoHistory:** it covers roughly 25 of the "no/partial" rows in action-and-undo.

---

## Group I



Audited on `claude/luthier-audit` @ `51e40a3`, which is `origin/claude/luthier-cloud-session-5lzlix` plus 11 audit commits. Helper branches checked with `git diff origin/claude/luthier-cloud-session-5lzlix...origin/claude/luthier-<name>`: realism-a, realism-b, realism-c, tune-help, visual, techniques, model-gaps. I only read the code. Nothing was built or run. "in progress on X" means the item exists only on helper branch X. Test names use the `Suite::test` form or the test file.

### DECISIONS.md

**Summary.** DECISIONS.md is a list of judgement calls. Of the 94 rows below, 72 are yes, 14 partial and 8 no. Most decisions are in the code. The file itself is out of date in three places:
- It still says the nine phase-2b specs are "not on disk / blocked" (C-02). They are on disk now, and their engines exist only on realism-a/b/c.
- C-32 asks for 14 column-4 tabs. The integration branch has 13; TECHNIQUES is only on the techniques branch.
- C-19 asks for `.luthierkit` files. They were never implemented.

Open work that DECISIONS names and that is still open: the part fields nothing consumes, reset consistency (B-05), the tune undo stack being separate, and the MIDI chuck key range.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Test harness owns its own RangeState; PresetManager takes it by ref | 09-22 | yes | Presets/PresetManager, PhysicalRange:RangeState | n/a | RangeTests | |
| Right-click "Restrict to stock range" only for per-control unlocks | adv-ranges 4 | yes | UI/Widgets.cpp context menu | right-click any ranged control | RangesUiTests | |
| Locked-range notice is a bubble at the control | adv-ranges 6.3 | yes | Widgets.cpp BubbleMessageComponent | at control | RangesUiTests | |
| RANGES master toggle clears per-control unlocks | adv-ranges | yes | UI/OptionsPages RangesPage | Options > RANGES | RangesUiTests | |
| Controls re-attach on a range-generation counter (4 Hz editor timer) | adv-ranges | yes | PluginEditor.cpp seenRangeGeneration / RangeState::getGeneration | n/a | RangesUiTests | |
| Undo stack off-by-one fixed; each entry holds before and after | action-undo | yes | PluginProcessor undo stack | Ctrl+Z / header | StateModelTests | |
| No separate "advanced-range clamped" banner | gui-int 15 | yes (by decision) | n/a | n/a | n/a | deliberate deviation |
| Tab-header padlocks on CHARACTER and WORKSHOP | gui-int 21 | partial | AdvancedPanel.cpp: RangeTabButton (CHARACTER only) | CHARACTER tab | RangesUiTests | WORKSHOP padlock missing; realism-a adds strings/environment/body families |
| GuitarCircuit is an MNA solver with trapezoidal companions; replaces CableSim | vki 1.1 | yes | DSP/Circuit/GuitarCircuit | Adv col 2 CIRCUIT | CircuitTests | |
| Coil resonance moved into GuitarCircuit; 50s wiring on the wiper; active tone corner; bypass neutral | vki 1.4/3.1 | yes | GuitarCircuit | CIRCUIT | CircuitTests | |
| Capacitor params in nF | vki | yes | Parameters.cpp:1343 circuit_tone_cap*1e-9 | CIRCUIT | CircuitTests | |
| Pot/cap StandardValueChoice combo beside a knob | vki | yes | UI/CircuitPanel, AdvancedPanel StandardValueChoice | CIRCUIT | CircuitTests | |
| Param count +11 (363 at the time) | vki | yes | Parameters.cpp | n/a | IntegrationTests (now 450) | |
| Advanced amp ranges: tone stack stays 0-1, extra shelving beyond it | adv-ranges | yes | AmpEngine | Amp knobs (advanced) | CircuitTests / RangeTests | |
| pick_material keeps 12 choices; pick_material/use_fingers read every block | pick-noise 2.1 | yes | ParameterBridge, PlayingNoise | CHARACTER PICK | NoiseTests | |
| pick_thickness/angle declared 0-1, mapped to mm/deg | adv-ranges | yes | Parameters/PlayingNoise | CHARACTER PICK | NoiseTests | |
| Chirp at pluck; squeak replaces glide noise; noise calibrated at output | pick-noise 4, squeak | yes | DSP/Noise/PlayingNoise, NoiseEngine | CHARACTER STRING NOISE | NoiseTests | |
| Engine reset restarts noise sequence (repeatable render) | midi-export 12 | partial | LuthierEngine::reset | n/a | MidiExportTests round-trip | BETA B-08: not bit-reproducible across instances for 3 presets |
| Winding selector edits string_material until Workshop exists | squeak 9 | partial | UI/NoiseGroups.cpp:181 attaches ParamIDs::stringMaterial | CHARACTER | NoiseTests | Workshop exists now; should edit the string part (still the param) |
| Multi-parameter actions are one undo step (ScopedUndoAction) | action-undo | yes | PluginProcessor::ScopedUndoAction; HeaderBar/NoiseGroups/SetupGroup | n/a | StateModelTests | |
| Fret buzz +13 params; fret_action superseded (inert) | fret-buzz 7 | yes | DSP/Noise/FretBuzz | SETUP group | BuzzTests; GuiReach excludes fret_action | |
| Buzz calibration 2.4 mm/unit; relief test high nut | fret-buzz 6.1 | yes | FretBuzz | SETUP | BuzzTests | |
| slide_guitar re-pointed as slide_enabled; hybrid one string under bar; vibrato mm; linear bar | slide-guitar 3/7 | yes | DSP/Slide/SlideEngine | CHARACTER SLIDE group | SlideTests | |
| Per-note sustain scale (fix for per-block reset) | char-wear | yes | LuthierEngine per-note scale | n/a | CharacterTests | |
| Phase-2b specs "not on disk, blocked" | 09-23 / C-02 | no (stale) | specs on disk; engines only on realism-a/b/c | no | realism branches: StringAging/Environment/BodyCoupling/HarmonicRealism/StringInteraction/FingerstyleAttack/NoiseFloor/SustainDecay/TuningStability Tests | in progress on realism-a/b/c |
| CharacterPanel sizes itself to content | ui | yes | UI/CharacterPanel | CHARACTER | EditorTests | |
| SLIDE group carries mode/damping/assist "(aid)"; mode change resets damping | slide-guitar 4/7 | yes | UI/SlideGroup | CHARACTER SLIDE | SlideTests | |
| Low-action warning offers "Use Slide setup" button (never automatic) | slide-guitar 6 | yes | UI/SlideGroup | CHARACTER SLIDE | SlideTests | |
| Factory guitars: 27 files | guitar-workshop 0.6 | yes | Resources/Guitars (27 .luthierguitar) | Workshop / type menu | WorkshopTests | |
| Part names avoid `:` and `/` | factory-content | yes | Resources/Parts | n/a | WorkshopTests | |
| WorkshopGuitar = spec's GuitarSpec; mapSpec re-points | guitar-workshop 3 | yes | Model/Workshop | n/a | WorkshopTests | |
| Unconsumed part fields: tuners, nut friction/width, radius/thickness, coil_turns, pole shape, spring_count, plies | part-acoustics 11 | no | tracked only | no | PartAcousticsTests covers the mapped fields only | tuners/nut are in progress on realism-c (tuning-stability); others are open |
| Termination brightness normalised to nickel-silver/bone | part-acoustics | yes | Workshop mapSpec | n/a | PartAcousticsTests | |
| Magnet pull formula | part-acoustics | yes | PickupEngine / mapSpec | n/a | PartAcousticsTests | |
| Luthier guitar-shop theme overrides theme.md | visual-polish 6 | yes | UI/Theme, Palette::remap | everywhere | ThemeTests | |
| Model-specific knob caps | visual-polish 3 | yes | UI/Faces/KnobCaps | amp/pedal faces | FacesTests | |
| User / follow-the-guitar accent colour | visual-polish 5 | partial | OptionsPages says "not built" | no | none | in progress on visual (Options accent box with "Follow the guitar") |
| Default guitars ship Player-friendly (1.6/2.0/0.20) | fret-buzz 6.1 | yes | factory guitar files | n/a | BuzzTests | |
| cpuStaysWithinBudget best of three | perf-budget | yes | Tests | n/a | IntegrationTests | |
| State load keeps its own params; only a type pick writes the guitar's | state-model | yes | PluginProcessor guitar loader | n/a | WorkshopPresetTests | |
| Preset guitar.reference "Factory/<Family>/<Name>" or "User/<Name>"; missing file falls back and raises a notice | file-formats | yes | PresetManager, PluginEditor "missing-part" banner | banner | WorkshopPresetTests | |
| Migrated pickup placements become guitar.override | file-formats | yes | PresetManager | n/a | GuitarMigrationTests | |
| Reset loads default type's factory guitar under defaults | state-model | partial | resetEverything | Ctrl+Shift+R | none for parity | BETA B-05: Reset leaves woods/magnets/pots/cap unlike a fresh instance |
| Save As Guitar second button "Save with parts" | guitar-workshop 6 | yes | PluginEditor.cpp:254 saveGuitarAs(bundleParts) | Workshop header / Ctrl+G | WorkshopPresetTests | |
| Save As Part fits the saved part | guitar-workshop | yes | WorkshopPanel savePartButton | Workshop inspector | WorkshopPanelTests | |
| Workshop swap overwrites SETUP tweaks (known limit) | ui-wiring 6 | partial | known limit | Workshop | none | limit still stated; not re-checked against the bench setup strip |
| Guitar change parks audio 5 ms out/in; notes queued while parked | C-09 | yes | PluginProcessor park | n/a | WorkshopSwap::aNotePlayedWhileParkedIsKeptNotDropped | |
| Part swaps that keep the string count: off-thread build, block-boundary swap | C-09 / TODO 6e | no | none on integration | n/a | PartSwap::* on model-gaps | in progress on model-gaps (swapPartsAtBlockBoundary) |
| Per-string arrays index 0 = high E; partial capo mask [F,F,T,T,T,F] | engine 1, amb-res 4.5 | yes | TuningEngine::setCapoStringMask; PluginProcessor.cpp:492 | Workshop capo part | RubricVoicerTests:1077, WorkshopPresetTests | GAPS B1 still calls it open (stale) |
| Assistant agents; docs/spec-coverage.md | process | yes | docs/spec-coverage.md | n/a | n/a | |
| C-16: ten tune templates | tune-builder 10 | yes | Resources/Tunes/Templates (10) | TUNE new-tune | TuneBuilderTests | |
| C-19: genre kits become `.luthierkit` files (magic luthier.kit) | factory-content 6 | no | kits compiled in Rhythm/GenreKit.cpp | no | none | no read/write of .luthierkit anywhere |
| C-32: column-4 strip has 14 tabs, TECHNIQUES before HELP | gui-tech-updates 0.2 | partial | AdvancedPanel.cpp:1037 (13 tabs) | col 4 | EditorTests everyWorkspaceTab... | TECHNIQUES in progress on techniques |
| Illustration: lighting, burst along outline, strings over neck, no bolts, fanned frets | guitar-illus / visual-polish 1 | yes | UI/Guitar/GuitarRenderer | Easy + Advanced guitar | GuitarRendererTests | |
| Family templates; family switch keeps seed and suitable parts | guitar-illus 12.2 | yes | PartLibrary::switchFamily, switchGuitarFamily | Workshop Guitar category | WorkshopTests | amp defaults per family (12.3) not done (TODO G); visual branch adds FamilyDefaults |
| Picking a guitar changes tuning only if needed | amb-res | yes | PluginProcessor guitar loader | type menu | WorkshopPresetTests | |
| Pickup heights clamp 0.8-6 mm | guitar-illus 19 | yes | WorkshopBench.cpp:391 kMinPickupHeight | Workshop bench drag/scroll | WorkshopBenchTests | |
| Bench drag moves the pickup live, commits once | workshop-ui | yes | WorkshopBench live pickup moves | Workshop bench | WorkshopBenchTests | |
| IRs reloaded only when the file changes; cab reset clears tails | perf | yes | BodyEngine/CabinetEngine | n/a | WorkshopBenchTests | known first-note IR onset diff (~0.02) |
| Spectrum delta after amp and cab, 4096-sample pluck, 60 dB floor | workshop-ui 6 | yes | Workshop/SpectrumDelta | Workshop spectrum pane | WorkshopBenchTests | |
| Easy mode keeps five macro knobs + Humanize + Character | gui-int 3 | yes | UI/EasyPanel attack/body/drive/tone/space | Easy Playing strip | EasyLayoutTests | |
| Wet/dry before limiter; width M/S at same point | gui-int 3.4 | yes | output_mix, stereo_width, MasterBus | Easy/Advanced master | EngineTests | |
| Character macro is a parameter (macro_character) | gui-int | yes | Parameters macroCharacter | CHARACTER + Easy | CharacterTests | |
| New params appended at the end of the layout | host-int | yes | Parameters.cpp | n/a | IntegrationTests (450) | |
| Trademarks out of shipped names; old names via rename maps | factory-content 0.1 | yes | PartLibrary::renamedFactoryPart/Guitar, migration.json | n/a | TrademarkTests | final legal review still a ship gate |
| Reset makes the next render repeat exactly | midi-export 12 | partial | Pedal::resetBase, reseeds | n/a | MidiExportTests | BETA B-08 open |
| Poly chord sounds one chord-window after its first note | controllers | yes | MidiInterpreter | n/a | Controllers::chordGroupsSoundOneWindowAfterTheyWerePlayed | |
| Live noise events as SysEx; Workshop fits via a 16-entry queue | midi-export | yes | MidiOutRouter | MIDI OUT / ROUTING | MidiExportTests | |
| Export defaults user-global in UiPreferences (.midprofile) | midi-export 8 | yes | UI/MidiExportDefaults | Options > MIDI, MIDI OUT | MidiOutPanelTests | |
| Aux 8 stereo noise bus after the 12 string buses | pick-noise 1.3 | yes | PluginProcessor buses | ROUTING aux strip 8 | PluginBusTests | routing-io.md table still lists 7 aux buses |
| Output buses matched by name, not position | routing-io | yes | PluginProcessor | n/a | PluginBuses::perStringLayoutPutsEachStringOnItsOwnBus | |
| Feedback is a physical loop (FeedbackLoop); feedback_on/threshold/speed inert | amb-res 1 | yes | DSP/Feedback/FeedbackLoop kInjectionGain | Adv col 3 FEEDBACK | FeedbackTests | chambering term only on model-gaps |
| E-Bow self-driven narrowband; Silenced damping; own ebow_intensity | amb-res 2 | yes | DSP/Feedback/EBowDriver, StringEngine Silenced | SUSTAIN section | EBowTests | |
| Doubler = PedalType::Doubler appended; legacy doubler_on migrates into a slot | amb-res 3 | partial | DSP/Effects PedalsMod; PresetManager.cpp:648 | pedal rack | DoublerTests | BETA B-07: migration runs on every load, not version-gated |
| migration.json (luthier.guitar-migration) | amb-res 7 | yes | Resources/Guitars/migration.json | n/a | GuitarMigrationTests, Trademarks::sourceTreeHasNoUnmarkedBrandNames | |
| Performance capture fed from string activity; technique marks missing | notation-export 6 | partial | PluginProcessor.cpp:1239 captureStringActivity | NOTATION tab | CaptureTests | technique/chord/slide hooks in progress on model-gaps |
| Preset morph: whole-preset switch at 0.5, continuous interpolation, position not in presets | amb-res 5 | yes | Presets/PresetMorph | LIVE tab | PresetMorphTests | |
| Pedal settings that arrive with a pedal are kept | state-model | yes | ParameterBridge structural path | n/a | PresetPedalTests | |
| Engine direct MIDI for TUNE melody/bass/layers | tune-builder 8 | yes | LuthierEngine::setDirectMidi | TUNE | TuneProcessorTests | |
| Tune keeps its own undo stack (fold into plugin-wide later) | action-undo | partial | Tune/TuneSession undoStack | TUNE | TunePanelTests | not folded into the processor undo |
| Click routing: monitor bus / CLICK TO MAIN / main when no monitor bus | practice 0.2 | yes | PluginProcessor clickToMain | Practice > Metronome | PracticeTests | |
| Processor owns routine runner, history, activity tracker | practice 10-12 | yes | Practice/PracticeRoutine* | PRACTICE tab + drawer | PracticeRoutineTests, PracticeDrawerTests | |
| Session recorder ring length on the PRACTICE tab only | practice 11.2 | yes | PracticePanel.cpp:1409 ringMinutes | PRACTICE tab | PracticeSetupPanelTests | |
| Rubric voicer replaces ChordVoicer at runtime; bass-pattern setting | amb-res 4 | partial | Model/Playing/RubricVoicer | n/a | RubricVoicerTests | no bass-pattern setting (Bass voices the root); selectNotesForStyle not retired. Both in progress on model-gaps |
| Rubric unisons listening pass | amb-res 4.2 | no | n/a | n/a | n/a | still open (TODO 2d) |
| HELP is one surface (tab + Easy overlay); F1 context pins topic | gui-int 4.4 | yes | UI/HelpTab | col 4 HELP, F1, header ? | HelpTabTests | |
| Help English only; no "Take the tour" button | accessibility 7 / onboarding 2 | no | Localisation.cpp: English catalog only | no | none | tour in progress on tune-help (Onboarding.cpp) |
| String scraping: mm from saddle; keyswitches 12/13/14; MPE ch16; pick_scrape_amount trims both | string-scraping | partial | DSP/Noise/ScrapeEngine | no (14 scrape_* have no control) | ScrapeTests | UI in progress on techniques |
| Strum dynamics: inverse smoothstep, up x0.85, misses, crossing precedence, USE KNOB | strum-dynamics | partial | Rhythm/StrumGesture; UI/StrumGroup | RHYTHM STRUM (lower 6 rows zero height, BETA B-11) | StrumGestureTests | layout bug hides strum_acceleration...chuck_damping |
| strum_speed migrates at 1000/ms | strum-dynamics | yes | PresetManager.cpp:631 | n/a | StrumGestureTests | |
| MIDI chuck key range | strum-dynamics | no | deferred | no | none | no range specified |
| Easy Feel scales crossing and evenness | strum-dynamics 6.3 | yes | EasyPanel | Easy Feel | EasyLayoutTests | UX tension flagged for review |
| Phase-2b range families strings/environment/body appended | adv-ranges | no | not on integration | no | RangeTests on realism-a | in progress on realism-a (PhysicalRange.h:42) |
| Chuck damps every string | strum-dynamics 6.1 | yes | StringEngine Damping::Chuck | n/a | StrumGestureTests chuckKillsPitch | |

### GAPS.md

**Summary.** GAPS.md is the least accurate file in the set, and it says so itself. Almost every "blocked / not built / absent" row has since been built. Of the 37 rows below, 25 are yes (built, so the entry is stale), 5 partial and 7 no. Still open for real:
- Drag-to-assign modulation.
- Per-string detune has no parameter, so it cannot be automated or learned.
- Expression calibration is not on the LIVE tab.
- W / New Tune shortcuts are missing on integration; they are in progress on visual and tune-help.
- Space still auditions instead of driving the tune transport.
- The TECHNIQUES tab.
- The "Not audited yet" list has been superseded by docs/spec-coverage.md.

The file should be rewritten or retired.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Eleven realism specs written | B0 | yes | spec/*.md | n/a | n/a | |
| GuitarCircuit replaces CableSim; CIRCUIT panel | What this blocked / A1 | yes | DSP/Circuit, UI/CircuitPanel | Adv col 2 CIRCUIT | CircuitTests | GAPS still says "CIRCUIT is still CABLE" (stale) |
| Workshop parts model + bench + WORKSHOP tab | blocked | yes | Model/Workshop, Workshop/, UI/WorkshopPanel | col 4 WORKSHOP / Easy overlay | WorkshopTests, WorkshopPanelTests, WorkshopBenchTests | per-string overrides, nut drag, pick/slide/capo drags open (TODO 7) |
| SlideEngine / Slide Mode | blocked | yes | DSP/Slide | CHARACTER SLIDE, `S` key | SlideTests | |
| NoiseEngine (pick/squeak/buzz), noise strip, Aux 8 | blocked | yes | DSP/Noise | CHARACTER, ROUTING | NoiseTests, BuzzTests | |
| StrumDynamics / RHYTHM STRUM group | blocked | partial | Rhythm/StrumGesture, UI/StrumGroup | RHYTHM (lower rows clipped) | StrumGestureTests | BETA B-11 layout |
| Advanced ranges: Options RANGES, padlock, warning arc | blocked / A3 | yes | PhysicalRange, UI/RangesUi, OptionsPages RangesPage | Options > RANGES; header padlock | RangeTests, RangesUiTests | A3 still says ten tabs (stale; 11 built, Overlays.cpp:457) |
| Bass techniques: SLAP group, bass step grid | blocked | partial | DSP/Slap/SlapEngine | no (25 slap/pop/ghost params unattached) | SlapTests | in progress on model-gaps (SlapGroup, BassGridGroup) and techniques |
| A1 four columns, 4.5 widths, 1000-pt Advanced minimum with notice | A1 | yes | UI/AdvancedPanel buildColumn1/2/3, InlineNotice | Advanced | Editor::advancedModeIsRefusedBelowItsMinimumWidth | |
| Extra sections kept on nearest column | A1 | yes | AdvancedPanel | Advanced | EditorTests | |
| A2 13 canonical col-4 tabs | A2 | yes | AdvancedPanel.cpp:1037 | col 4 | Editor::everyWorkspaceTabSelectsAndPaints | GAPS says 6 absent (stale) |
| TUNE tab | A2 | yes | UI/TunePanel | col 4 TUNE | TunePanelTests | richer editor in progress on tune-help |
| PRACTICE setup tab | A2 | yes | UI/PracticeSetupPanel | col 4 PRACTICE | PracticeSetupPanelTests | decided in GAPS "Decisions" 3 |
| NOTATION tab + live capture | A2 | yes | UI/NotationPanel, Capture/PerformanceCapture | col 4 NOTATION | NotationPanelTests, CaptureTests | technique marks missing (in progress on model-gaps) |
| MIDI OUT tab + profiles | A2 | yes | UI/MidiOutPanel, Export/ | col 4 MIDI OUT | MidiOutPanelTests, MidiExportTests | |
| HELP as a tab | A2 | yes | UI/HelpTab | col 4 HELP | HelpTabTests | |
| Expression-pedal calibration on LIVE tab | A2 / gui-int 4.4 | no | only Options EXPRESSION page | Options > EXPRESSION | none | deliberate; spec conflict unresolved |
| Last-used tab persists (UiPreferences) | A2 | yes | UI/UiPreferences | n/a | Editor::theWorkspaceTabWraps... | |
| CONTROLLERS moved to col 4 | A2 | yes | AdvancedPanel ControllersPage | col 4 | Editor::everyOptionsPageSelectsAndPaints | |
| Options: 11 tabs in section 5 order | A3 | yes | UI/Overlays.cpp:451-461 | Options overlay | EditorTests | |
| APPEARANCE accent tint / data-stream / noise-strip toggles | A3 | no | OptionsPages.cpp:801 "not built" label | Options > APPEARANCE (placeholder) | none | accent in progress on visual |
| UPDATES changelog viewer | A3 | partial | shows manifest links only | Options > UPDATES | none | visual adds UpdateDownloader |
| DIAGNOSTICS Workshop/Slide/ranges flag mirror | A3 | no | not built | no | none | |
| FILE LOCATIONS Guitars/ and Parts/ | A3 | partial | FileLocationsPage | Options > FILE LOCATIONS | none | not re-checked for Guitars/Parts rows |
| Easy headstock tuning popover, bridge whammy popover | A4 | yes | UI/GuitarBodyComponent | Easy guitar | EasyLayoutTests | |
| Per-string detune automatable / learnable | A4 | no | writes TuningEngine directly; no param | headstock popover only | none | needs 12 params + preset field |
| Right-click > Modulate | A4 | yes | buildParameterContextMenu/applyParameterMenuResult | right-click | Editor::rightClickOffersModulationAndBuildsTheRoute | |
| Drag-to-assign modulation | A4 | no | no DragAndDropContainer | no | none | |
| Section 15 banners (crash, licence, policy, update, SR change, preset error, IR missing, missing part/guitar) | A6 | yes | PluginEditor.cpp:826-1009 NotificationCentre | banner strip | EditorTests (4 banner tests) | the comment at PluginEditor.cpp:805 is stale (says unreachable) |
| Capo: one capo in TuningEngine, capo_fret param, 3 UI homes | B1 | yes | TuningEngine, ParamIDs::capoFret | col 1 GUITAR, headstock popover, fretboard right-click | GenreKits::capoRemovesFretsBelowItAndMovesThePitch | |
| Partial capo (string mask from capo part) | B1 | yes | TuningEngine::setCapoStringMask | Workshop capo part | RubricVoicerTests, WorkshopPresetTests | GAPS says open (stale); capo drawing on the illustration is open (TODO G) |
| Shortcut registry single source of truth; Ctrl+/ table | A5 | yes | Accessibility.cpp:494-560 | Options > ACCESSIBILITY | Accessibility::everyShortcutHasADescriptionInTheCatalog | |
| Workshop `W` shortcut | A5 | no | not in registry | no | none | in progress on visual (toggleWorkshop) |
| Slide `S`, Save As Guitar Ctrl+G | A5 | yes | Accessibility.cpp:499,539 | keys | EditorTests | |
| New Tune Ctrl+T | A5 | no | not in registry | no | none | in progress on tune-help |
| Space drives the tune transport | A5 / gui-int 17 | partial | Space = audition globally; TUNE handles Space only while focused | TUNE focused | TunePanelTests | |
| Ctrl+N Init, Ctrl+Alt+E reveal, Ctrl+[ / ] tabs | A5 | yes | Accessibility.cpp | keys | Editor::newPresetLoadsInit... | |

### PROGRESS.md

**Summary.** Of the 32 rows below, 21 are yes, 6 partial and 5 no. The file is badly out of date. Its "Current state" still gives 310 tests, 349 params, 36 presets and "eleven realism specs not on disk". Its "Not done" list still names CLAP/Linux and drag-out export: Linux builds and a CLAP target now exist, and drag-out is done. Its last line says scrape/strum are "committed UNBUILT"; TODO now says they are built and green. The WIP it names is still in Source/WIP, with Muting in progress on techniques and FirstRun in progress on tune-help. The environment section (Windows/MSVC) does not match the current Linux/clang build. Unsigned installers and the manual per-host matrix are still open.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| M1-M30 core milestones (engine, presets, UI, MIDI learn, export, help/options, IRs, tests, debug, easter egg, docs) | Milestones | yes | Source/* | yes | 60+ suites | M12 CableSim since replaced by GuitarCircuit |
| M31 routing-io (multi-out, sidechain, re-amp, MIDI out) | Milestones | yes | Routing/, PluginProcessor buses, producesMidi()=true | ROUTING | RoutingTests, PluginBusTests | CMake NEEDS_MIDI_OUTPUT FALSE vs producesMidi true (check AU/VST3 MIDI out) |
| M32 mod matrix | Milestones | yes | Modulation/ | MOD tab | ModulationTests | offsets clamp to +-4 bug fixed only on model-gaps |
| M33 rhythm engine, 37 patterns, 28 kits | Milestones | yes | Rhythm/ | RHYTHM | RhythmTests, GenreKitTests | BETA B-02 self-excitation after stop (critical) |
| M34 live performance | Milestones | yes | Live/ | LIVE tab + strip | LiveTests | |
| M35 controllers | Milestones | yes | Controllers/ | col 4 CONTROLLERS | ControllerTests | |
| M36 practice tools | Milestones | yes | Practice/ | drawer + PRACTICE tab | PracticeTests | BETA B-14: panic/reset do not stop players |
| M37 tone match | Milestones | yes | ToneMatch/ | TONE MATCH | ToneMatchTests | capture feed fixed 9577b47 |
| M38 notation export | Milestones | yes | Notation/, Capture/ | NOTATION | NotationTests | |
| M39 character wear | Milestones | yes | Character/ | CHARACTER | CharacterTests | |
| M40 accessibility incl. localisation | Milestones | partial | Accessibility/ | Options | AccessibilityTests | 15 locales listed, only the English catalog exists |
| M41 updates/telemetry/licence | Milestones | yes | Updates/ | Options UPDATES/PRIVACY | TelemetryTests | |
| Current-state table (310 tests, 349 params, 36 presets, 25 guitars) | Current state | no (stale) | now 450 params, 27 guitars, 743+ tests | n/a | IntegrationTests count 450 | rewrite |
| "Eleven realism specs not on disk" | Phase 2 | no (stale) | specs present, implemented | n/a | n/a | |
| Freeze and E-Bow split | Phase 2 | yes | DSP/Master/FreezeOverlay, EBowDriver | SUSTAIN | EBowTests | |
| Options overlay matches section 5 | Phase 5 | yes | Overlays.cpp | Options | EditorTests | text says RANGES absent (stale) |
| ErrorLog JSON lines | Phase 5 | yes | Support/ErrorLog | Options DIAGNOSTICS | error tests | |
| Newer-schema preset loaded, logs NEWER_SCHEMA | Phase 5 | yes | PresetManager | n/a | HostStateTests | deviation from error-recovery 1 |
| Not done: CLAP and Linux builds | Not done | partial | CMakeLists.txt:20 optional CLAP; scripts/setup_linux.sh | n/a | pluginval/clap-validator run (BETA) | Linux packaging in progress on visual |
| Not done: signed installers | Not done | no | none | n/a | none | visual adds Linux packaging only |
| Not done: manual per-host test matrix | Not done | no | none | n/a | none | qa-polish ship gate |
| Not done: drag-out export | Not done | yes | MidiOutPanel drag (Alt=Generic) | MIDI OUT | MidiOutPanelTests | session-recorder drag in progress on model-gaps |
| pluginval strictness 10 passes | Not done | partial | BETA_TEST_REPORT | n/a | external | BETA B-12: thread-safety test exceeds the 30 s default timeout |
| Editor run-verified by the test target | Not done | yes | Tests/EditorTests | n/a | EditorTests, GuiReachabilityTests | |
| A4 "Still open": drag-to-assign; 3 notification routes | A4 | partial | routes built; drag missing | | EditorTests | |
| Autonomous run items (ranges UI, GuitarCircuit, noise, setup, slide, workshop, presets carry guitar, park swap) | 09-22/23 | yes | as listed | yes | respective suites | |
| Partial capo "written not built" | 09-22/23 | yes (stale) | TuningEngine | Workshop | RubricVoicerTests | |
| Scrape + StrumGesture "committed UNBUILT" | 09-23/24 | yes (stale) | ScrapeEngine, StrumGesture | scrape: no GUI; strum: partial | ScrapeTests, StrumGestureTests | TODO says green on Linux |
| SlapEngine + TechniqueTriggers in WIP | 09-23/24 | yes (stale) | DSP/Slap, Model/Playing/TechniqueTriggers (in build) | no GUI | SlapTests | UI in progress on model-gaps/techniques |
| Muting (muting-rhythm.md) | 09-23/24 | partial (WIP, not built) | Source/WIP/Rhythm/Muting, WIP/UI/MuteGroup | no | WIP/Tests/MutingTests | in progress on techniques (moved out of WIP) |
| FirstRun / FirstEncounterHint (onboarding) | 09-23/24 | partial (WIP, not built) | Source/WIP/UI/FirstRun* | no | WIP/Tests/FirstRunTests | in progress on tune-help (UI/Onboarding.cpp) |
| Environment section (Windows 10/MSVC) | Environment | no (stale) | current build is Linux/clang (TODO) | n/a | n/a | |

### INDEX.md

**Summary.** INDEX.md is the reading and build order. Every file it names is on disk (checked). Of the 21 rows below, 4 are yes, 9 partial and 8 no; phases 1-3 and most of 2 and 5 are in the code. Phase 2b (9 specs) and most of phase 5b (slide-technique-controls, muting, tapping, microtonal bends, cascade, Techniques GUI, engine technique layer) exist only on helper branches. Phase 6 (bug bash, final human check) has not been done. The global realism rule "every new file's Tests section in LuthierTests" holds for phases 1-5 and fails for 2b/5b on integration.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Phase 1: 11 original extension specs | Order 1-11 | yes | Routing/…Updates/ | yes | per-suite | |
| Phase 2: 12 realism specs | Order 12-23 | yes | PhysicalRange, GuitarCircuit, Noise, Slide, Workshop, StrumGesture, SlapEngine, Export | mostly | per-suite | slap/bass UI and strum rows are gaps |
| 23a string-aging | Phase 2b | no | none | no | none | in progress on realism-a |
| 23b environment | Phase 2b | partial | Character/ temp/humidity only | CHARACTER | CharacterTests | full spec in progress on realism-a |
| 23c body-coupling (wolf, tap tones) | Phase 2b | no | none | no | none | in progress on realism-a |
| 23d harmonic-realism | Phase 2b | no | none | no | none | in progress on realism-b |
| 23e string-interaction | Phase 2b | partial | CouplingMatrix (sympathetic) | coupling knob | EngineTests | in progress on realism-b; BETA B-03 over-ringing |
| 23f fingerstyle-attack | Phase 2b | no | none | no | none | in progress on realism-b |
| 23g noise-floor | Phase 2b | partial | hum_noise, noise_amp_buzz only | CHARACTER | NoiseTests | in progress on realism-c |
| 23h sustain-and-decay | Phase 2b | partial | sustain_scale, release | SUSTAIN | SustainTests | in progress on realism-c |
| 23i tuning-stability | Phase 2b | partial | tuning drift (character-wear) | col 1 | CharacterTests | in progress on realism-c |
| Phase 3 tune-builder | 24 | yes | Tune/, UI/TunePanel | col 4 TUNE | TuneBuilder/TunePanel/TunePlayer/TuneProcessorTests | piano roll, export dialog, setlist in progress on tune-help |
| Phase 4 gap-fills (amb-res, gui-int, ui-wiring, onboarding, perf, qa, installer, brief) | 25-32 | partial | see group reports | | | onboarding WIP; installers unsigned; perf idle over budget (BETA B-13) |
| Phase 5 deep-integration (9 specs) | 33-41 | yes | various | | | .luthierkit missing (C-19) |
| 42 string-scraping | 5b | partial | ScrapeEngine | no | ScrapeTests | UI in progress on techniques |
| 43 slide-technique-controls | 5b | no | none | no | none | in progress on techniques (SlideTechniqueTests) |
| 45 muting-rhythm | 5b | partial (WIP, not built) | WIP/Rhythm/Muting | no | WIP MutingTests | in progress on techniques |
| 44/46/47 slap-technique, two-hand-tapping, microtonal-bends | 5b | partial | slap: SlapEngine yes; tap/bend no | no | SlapTests | TapEngine/BendEngine in progress on techniques |
| 48/50 technique-cascade, engine-technique-layer | 5b | no | none | n/a | none | CascadeResolver in progress on techniques |
| 49 gui-techniques-updates (Techniques tab, pills, overlays) | 5b | no | none | no | none | in progress on techniques |
| Phase 6 bug bash + final human check | Phase 6 | no | docs/audit/BETA_TEST_REPORT.md (automated only) | n/a | Combo tests | 11 findings open |

### REVIEW.md

**Summary.** REVIEW.md lists 10 gaps and 5 ambiguities in the original specs. Of the 16 rows below, 14 are yes and 2 partial: every gap now has a spec and an implementation, and every ambiguity has been resolved in code (RubricVoicer, FeedbackLoop, FreezeOverlay vs EBowDriver, Doubler pedal, PresetMorph). The partial items are localisation (English catalog only) and the capture recording techniques.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| 1 Rhythm/pattern engine | Gaps | yes | Rhythm/ | RHYTHM | RhythmTests | |
| 2 Live-performance layer | Gaps | yes | Live/ | LIVE | LiveTests | |
| 3 Modulation matrix | Gaps | yes | Modulation/ | MOD | ModulationTests | no drag-to-assign |
| 4 Multi-out, sidechain, MIDI out | Gaps | yes | Routing/ | ROUTING | RoutingTests | |
| 5 User IR / tone match | Gaps | yes | ToneMatch/ | TONE MATCH | ToneMatchTests | |
| 6 Notation output | Gaps | partial | Notation/, Capture/ | NOTATION | NotationTests | techniques not captured (in progress on model-gaps) |
| 7 Controller integration | Gaps | yes | Controllers/ | CONTROLLERS | ControllerTests | |
| 8 Character/aging | Gaps | yes | Character/ | CHARACTER | CharacterTests | string-aging depth in progress on realism-a |
| 9 Accessibility / localisation | Gaps | partial | Accessibility/ | Options | AccessibilityTests | no translated catalogs, no RTL |
| 10 Updates/telemetry/crash | Gaps | yes | Updates/ | Options | TelemetryTests | |
| Chord auto-fingering rubric | Ambiguities | yes | RubricVoicer | n/a | RubricVoicerTests | |
| Feedback: physical loop | Ambiguities | yes | FeedbackLoop | FEEDBACK | FeedbackTests | |
| Freeze vs E-Bow | Ambiguities | yes | FreezeOverlay, EBowDriver | SUSTAIN | EBowTests | |
| Doubler defaults | Ambiguities | yes | Doubler pedal | pedal rack | DoublerTests | |
| Preset morph | Ambiguities | yes | PresetMorph | LIVE | PresetMorphTests | |
| Milestone order | Recommended | yes | PROGRESS M31-M41 | n/a | n/a | |

### JUCE_CLAUDE_GUIDELINES.md

**Summary.** Of the 38 rows below, 22 are yes, 10 partial and 5 no; the context7 rule (1 row) is n/a. The code breaks several rules:
- No CI and no warnings-as-errors.
- JUCE is cloned by a script at tag 8.0.10, not pinned as a submodule.
- Tests use a custom framework, not juce::UnitTest.
- The audio path uses try-locks (SpinLock / CriticalSection try) and a parameter-bridge engine lock. It never waits, but it is not lock-free.
- dynamic_cast appears in EffectsChain, off the render path.
- IR paths outside the library root are stored as absolute paths.
- Some GUI controls are hand-wired: the detune popover, pedal-slot custom attachments.

The context7 rule cannot be verified from the code.

| Requirement | Section | Implemented? | Where (file:symbol) | GUI reachable? | Test covering it | Notes |
|---|---|---|---|---|---|---|
| Query context7 before JUCE code | 1 | n/a (process) | n/a | n/a | n/a | not verifiable |
| No alloc in audio callback | 2 | partial | fixes 9383a34, 6a0d737; ReverbPedal lines pre-sized (PedalsMod.cpp:744) | n/a | CaptureTests/TunePlayer alloc checks compiled out (LUTHIER_ALLOCATION_COUNTER undefined) | counter enabled only on model-gaps |
| No locks on audio thread | 2 | partial | ScopedTryLock/SpinLock try in EffectsChain, BodyEngine, CabinetEngine, TunePlayer, BackingTrack; bridge engine lock (f1eb720) | n/a | Combo stress | try-locks only, silence on contention |
| No dynamic_cast / exceptions on audio thread | 2 | yes | EffectsChain.cpp:68,238 are message-thread | n/a | none | |
| No GUI calls from audio thread | 2 | yes | editor polls on timers | n/a | EditorTests | |
| Pre-allocate in prepareToPlay | 2 | yes | PluginProcessor::prepareToPlay | n/a | | |
| Project layout DSP free of GUI headers | 3 | yes | Source/DSP includes DspCommon only | n/a | build (UI excluded from engine targets) | |
| JUCE pinned as a submodule at a tag | 4 | partial | not a submodule; setup_linux.sh clones tag 8.0.10; ThirdParty/JUCE gitignored | n/a | n/a | |
| Explicit FORMATS (VST3 Standalone [+AU mac]) | 4 | yes | CMakeLists.txt:29 | n/a | n/a | CLAP optional |
| COPY_PLUGIN_AFTER_BUILD off in CI | 4 | yes | CMakeLists.txt:46 FALSE | n/a | n/a | |
| Stable PLUGIN_CODE / MANUFACTURER_CODE | 4 | yes | Lthr / Ltha | n/a | n/a | |
| Static MSVC runtime | 4 | yes | CMakeLists.txt:12 | n/a | n/a | |
| cxx_std, no GNU extensions | 4 | yes | CXX_STANDARD 17, EXTENSIONS OFF | n/a | n/a | |
| Warnings as errors in CI (-Wall -Wextra -Werror) | 4 | no | MSVC warnings suppressed (/wd4244…); no -W flags; no CI | n/a | none | |
| Single APVTS | 5 | yes | PluginProcessor apvts | n/a | IntegrationTests | |
| Param IDs in one header | 5 | yes | Parameters.h ParamIDs | n/a | Parameters::everyParameterHasAUniqueId… | pedal-slot IDs generated |
| Cached raw param pointers on audio side | 5 | yes | ParameterBridge | n/a | | |
| Attachments, no hand-wired GUI listeners | 5 | partial | most via attachTo; detune popover writes TuningEngine directly | | GuiReach | |
| copyState/replaceState for state | 5 | partial | custom JSON (PresetManager::toVar) | n/a | HostStateTests | equivalent, not APVTS XML |
| No GUI from parameterChanged | 5 | yes | AsyncUpdater in ParameterBridge | n/a | | |
| juce::dsp modules / prepare+reset every module | 6 | yes | reset() on every module (DECISIONS reset fixes) | n/a | MidiExport round-trip | |
| SmoothedValue on continuous params | 6 | yes | smoothers in engine | n/a | | not exhaustively checked |
| ScopedNoDenormals at top of processBlock | 6 | yes | PluginProcessor.cpp:984, LuthierEngine.cpp:1510 | n/a | | |
| Variable block sizes | 6 | yes | processSlice | n/a | pluginval | |
| isBusesLayoutSupported explicit | 6 | yes | PluginProcessor.cpp:279 | n/a | PluginBusTests | |
| Clear unused output channels | 6 | partial | not seen at top of processBlock; buses written by name | n/a | PluginBusTests | verify |
| Oversampling factor a parameter | 6 | yes | ParamIDs oversample | Options AUDIO + Advanced | | |
| MIDI at sample position | 6 | yes | processSlice slicing | n/a | Controllers tests | |
| Background to audio via atomic swap | 7 | partial | IR/guitar swap parks audio instead; block-boundary swap only on model-gaps | n/a | WorkshopSwap | |
| GUI caches paths, 30 Hz timers, setBufferedToImage | 8 | partial | timers 4/20/30 Hz; no setBufferedToImage anywhere | | | FeedbackLed 20 Hz (30 on model-gaps) |
| juce::UnitTest suites for every DSP class | 9 | partial | custom LuthierTests framework (TestFramework.h) | n/a | 743+ tests | not juce::UnitTest |
| pluginval strictness 10 in CI on every PR | 9 | no | no CI; scripts/pluginval.sh only on visual | n/a | manual runs | |
| Host test matrix (Reaper, Live, FL, Cubase, Logic) | 9 | no | none | n/a | none | |
| State versioned (pluginVersion) | 10 | yes | PresetManager.cpp:358 | n/a | HostStateTests | |
| No absolute paths in state | 10 | partial | IrLibraryPaths::toPresetPath falls back to full path outside root | n/a | ToneMatchTests | |
| Factory presets as BinaryData / ValueTree | 10 | no | presets compiled in code (FactoryPresets.cpp); Resources copied beside the binary | n/a | | deviation |
| AAX only with PACE | 11 | yes | not built | n/a | n/a | |
| Definition-of-done checklist enforced | 12 | no | no CI gate | n/a | n/a | |

### Top gaps (group I)

Ranked by user impact:

1. **B-02 (critical): strings self-excite after the rhythm engine stops, and panic does not silence them.** Rhythm engine plus string coupling. Open.
2. **B-03: released notes keep ringing through sympathetic coupling** at the default `coupling_amount`. This conflicts with SUS-08. Owned by realism-b/c.
3. **Phase-2b realism (9 specs) is not on integration.** It exists only on realism-a/b/c; DECISIONS/PROGRESS still say "blocked".
4. **Techniques layer is not on integration**: TECHNIQUES tab, tap, bend, slide-technique controls, cascade resolver, muting. All in progress on techniques.
5. **45 automatable parameters have no visible control** (B-11): 14 `scrape_*` and 25 slap/pop/ghost. Slap UI is in progress on model-gaps and techniques.
6. **The RHYTHM STRUM group's lower six rows render at zero height**: strum_acceleration, strum_up_velocity_ratio, strum_tilt, strum_miss_probability, chuck_amount, chuck_damping.
7. **Idle CPU is about equal to playing CPU** (B-13), 6-12x over the idle budget. Nothing short-circuits on silence.
8. **Reset leaves the default guitar's parts inconsistent** (B-05). The UI shows acoustic woods on the Strat.
9. **Onboarding (FirstRun/tour) is WIP and not built.** "Take the tour" is absent. In progress on tune-help.
10. **Localisation is English only** despite 15 listed locales.
11. **Panic and Reset do not stop the looper, backing track, tune player or metronome** (B-14).
12. **Click at a preset switch** (B-06). A preset change also loses the rest of the performance: the capture records no technique marks (in progress on model-gaps).
13. **No CI, no warnings-as-errors, pluginval not automated.** B-12: pluginval's thread-safety test times out at the default 30 s.
14. **Part fields nothing consumes**: tuners, nut friction, radius, coil_turns, spring_count, plies. The Workshop shows parts that change nothing.
15. **Per-string detune has no parameter**, so no automation and no MIDI Learn.
16. **Shortcuts `W` (Workshop) and Ctrl+T (New Tune) are missing, and Space is not the transport.** W and Ctrl+T are in progress on visual and tune-help.
17. **Drag-to-assign modulation is missing.**
18. **`.luthierkit` genre-kit files are missing (C-19)**, so users cannot save or share kits.
19. **Legacy `doubler_on` migrates on every load** (B-07), which changes a rig on reload.
20. **Signed installers and the manual host matrix are not done.**

### Unspecified gaps noticed

Things a buyer of a $200 guitar plugin would expect that no spec in group I covers:
- **Noise gate.** A NoiseGate pedal type exists, but factory high-gain presets do not load one, and the idle floor is -22 to -30 dBFS at high gain (B-04).
- **Tuner.** There is no chromatic tuner or tuning-meter display.
- **Out-of-range notes.** Notes below the instrument's range are silently dropped: no octave-fold option and no notice (B-09).
- **Deterministic offline bounce.** The render is not bit-identical across instances (B-08). Freeze/bounce users will see this.
- **Preset count and browsing.** There are 36 factory presets and no tags or favourites search (not checked in depth). That is thin for the price.
- **Undo history view.** There is none; action-and-undo defines the stack but no list UI.
- **MIDI output flag.** CMake has `NEEDS_MIDI_OUTPUT FALSE` while `producesMidi()` returns true. The MIDI OUT features may not reach hosts that read the plugin descriptor.

### Small glue candidates

An engine parameter or feature exists and only lacks UI or a preset field.

| Item | Param IDs / symbol | Should go on | Notes |
|---|---|---|---|
| Macro 7/8 knobs | `macro_assign_a`, `macro_assign_b` | MOD tab, MACROS row (beside macros 1-6) | only usable as mod sources today |
| STRUM group rows clipped | `strum_acceleration`, `strum_up_velocity_ratio`, `strum_tilt`, `strum_miss_probability`, `chuck_amount`, `chuck_damping` | RHYTHM tab STRUM group | lay out RhythmPanel at `preferredHeight()` inside its viewport |
| Bass SLAP group | `slap_strength`, `slap_position_mm`, `slap_thumb_hardness`, `slap_fret_contact`, `pop_strength`, `pop_position_mm`, `double_thump_enabled`, `double_thump_up_ratio`, `ghost_level`, `ghost_damping`, `ghost_auto`, `ghost_velocity_threshold` | CHARACTER tab SLAP group (bass only) | ready on model-gaps `UI/SlapGroup.cpp`; merge |
| Slap technique triggers | `slap_armed`, `slap_type`, `slap_trigger`, `slap_velocity_zone`, `slap_trigger_cc`, `slap_ghost_cc`, `slap_force`, `slap_palm_position_mm`, `slap_string_mask`, `slap_ghost_mode`, `slap_rebound_gap`, `slap_snap_back`, `slap_body_part` | TECHNIQUES > SLAP sub-tab | ready on techniques `TechniquePages.cpp`; interim: a CHARACTER group |
| Scrape controls | `scrape_armed`, `scrape_trigger`, `scrape_direction`, `scrape_sweep_source`, `scrape_trigger_cc`, `scrape_sweep_cc`, `scrape_start_mm`, `scrape_end_mm`, `scrape_duration`, `scrape_pressure`, `scrape_tool`, `scrape_angle`, `scrape_string_mask`, `scrape_retrigger` | TECHNIQUES > SCRAPE (interim: CHARACTER PICK group) | engine is live; ready on techniques |
| Pickup blend | `pickup_blend` | Adv col 2 PICKUPS | engine does not read it (`PickupEngine::setBlend` unused in process), so it needs a 1-line engine hook as well |
| Options APPEARANCE accent | `UiPreferences` accent / follow-guitar | Options > APPEARANCE | Palette supports it; ready on visual |
| Expression calibration mirror | `ExpressionCalibrationSet` (singleton) | LIVE tab (read-only summary + "Calibrate..." opening Options EXPRESSION) | avoids a second writer |
| Workshop / New Tune shortcuts | registry ids `toggleWorkshop` ('w'), `newTune` (Ctrl+T) | Accessibility.cpp registry | one line each; on visual / tune-help |
| WORKSHOP tab padlock | `RangesUi::RangeTabButton` with the pickup-height family | col 4 WORKSHOP tab header | same pattern as CHARACTER (AdvancedPanel.cpp:1060) |
| Doubler migration gate | `doubler_on` in `PresetManager::fromVar` (line 648) | n/a (preset loader) | gate on the preset schema/pluginVersion |
| Stale comment | PluginEditor.cpp:805 ("unreachable" banners) | n/a | the banners are all wired now |
