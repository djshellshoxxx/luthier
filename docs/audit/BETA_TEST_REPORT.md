# Beta test report

Auditor branch `claude/luthier-audit`. Code under test: the integration branch
`claude/luthier-cloud-session-5lzlix` at `6bda872` (review merged), plus the
fixes on this branch. None of the seven helper branches had landed on the
integration branch when this was written; each one gets the harness below
when it does.

## How to reproduce

```
scripts/setup_linux.sh
ninja -C build -j$(nproc) LuthierTests Luthier_VST3 Luthier_CLAP
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests Combo GuiReach
LUTHIER_COMBO_SCALE=0.1 ...     # quick pass (about 3 min instead of 29)
LUTHIER_COMBO_VERBOSE=1 ...     # every configuration and the full reachability table
```

Every finding below names the test, the exact settings and the seed. The
harness is `Source/Tests/ComboHarness.h`, `CombinationTests.cpp` (group
`Combo`) and `GuiReachabilityTests.cpp` (group `GuiReach`).

What each render is asked (`combo::Verdict`): output finite; peak under +12
dBFS; fewer than 64 subnormal samples; audible (above -80 dBFS) while notes
play; after release the last 300 ms is under -60 dBFS, or 30 dB under the
note, or within 3 dB of the rig's own idle floor; the idle floor itself
under -30 dBFS; CPU under 60% of real time; no denormal storm (tail blocks
4x slower than the mean). Sustain features (E-Bow, freeze, feedback, rhythm
engine, delay/reverb pedals) are exempt from the decay check.

## Coverage

| Test | What it covers | Size |
|---|---|---|
| `Combo.pairwiseAcrossMajorSettings` | all-pairs over 25 factors: guitar type, tuning, capo, amp, cab, mic, room size, room on, pre and post pedal type, oversampling, playing mode, slide, slap armed, slap type, E-Bow, freeze, feedback amount, scrape armed, bridge, pickup selector, string material, body mode, fretless, phrase | 614 rows, seed 20260924 |
| `Combo.everyGuitarTypePlaysEveryPhrase` | 24 guitar types x 8 phrases | 192 |
| `Combo.everyFactoryPresetPlaysEveryPhrase` | 36 factory presets x 8 phrases | 288 |
| `Combo.presetSwitchUnderARingingNoteDoesNotClick` | each preset -> the next, mid-note | 36 |
| `Combo.everyRhythmPatternAndGenreKit` | 37 patterns + 28 kits, on then off | 65 |
| `Combo.modulationRoutesAtFullDepth` | LFO/random routes at +-1 to all 331 float destinations, 8 at a time | 42 |
| `Combo.snapshotsAndPresetMorph` | snapshot recall mid-note, morph sweep while playing | 12 pairs |
| `Combo.advancedRangesUnlockedAtExtremes` | every family unlocked, 32 ranged params at advanced min and max, all and per family | 43 |
| `Combo.renderIsDeterministicAfterReset` | every preset, twice after reset | 36 |
| `Combo.sessionStateRoundTripReproducesAudio` | state into a fresh instance, audio compared | 46 |
| `Combo.everyParameterSurvivesTheSessionStateRoundTrip` | all 450 params random, state round trip | 5 seeds |
| `Combo.sustainFeaturesStayBounded` | E-Bow/freeze/feedback/secret at max on every other amp; high gain with and without coupling | 63 |
| `Combo.seededRandomConfigurations` | every parameter random, random phrase | 2000, seed 1234567 |
| `Combo.structuralChangesWhileAudioRuns` | audio thread running while the message thread changes guitar/pedals/amp/cab/body/oversampling and loads presets | 300 bursts, seed 99 |
| `Combo.unisonStringsNeverGrowAndPanicSilencesThem` | after every strum pattern; two strings at one pitch | 38 |
| `Combo.cpuPerFactoryPreset` | CPU table, idle and playing | 36 |
| `GuiReach.*` | editor walked in Easy, Advanced (13 tabs), 6 overlays, 11 Options pages, practice drawer, Live, Slide, headstock and bridge popovers, the hidden-effect pixel, every toggle's sub-view; contexts default, bass, whammy+slide+slap, 3 pedal fills; 2043 controls operated | 265 views |

Phrases: single note, 6-note chord, whole-tone bend up and back, hammer-on
and pull-off, palm-muted chugs (CC67), natural and pinch harmonics
(CC73/CC72), 16 fast repeats at 40 ms, sustain pedal (CC64).

## External validators

| Validator | Result |
|---|---|
| pluginval 1.x, strictness 10, VST3, before B-01's fix | **abort**: "double free or corruption" in the Automation test (B-01) |
| pluginval, strictness 10, `--timeout-ms 900000`, after | **SUCCESS**, every test |
| pluginval, strictness 10, default timeout, after | Parameter thread safety **times out at 30 s** (B-12) |
| clap-validator 0.3.2, CLAP, first run (own CLAP target) | 12 pass, 6 fail: preset-morph position lost from state (B-10), Pick Tip Radius text round trip, param events with a foreign namespace applied, state flush not reproducible |
| clap-validator 0.3.2 on the integration branch's CLAP target (after merging review) | **18 pass, 0 fail**, 3 skipped |

## Findings

Status: FIXED (on this branch, commit named), OPEN (for the coordinator), or
IN PROGRESS (a helper branch covers it).

### B-01 Crash: structural rebuilds raced processBlock. FIXED (`f1eb720`)

- Found by: pluginval strictness 10, Automation test (`double free or corruption (!prev)`), then reproduced by `Combo.structuralChangesWhileAudioRuns` (seed 99): `pure virtual method called` / `free(): invalid pointer` within seconds.
- Backtraces (audio thread): `ReverbPedal::rebuildLines` <- `ReverbPedal::parameterChanged`, and `RoomEngine::rebuild` <- `RoomEngine::setDecayScale` <- `ParameterBridge::applyToEngine`, while the message thread ran `ParameterBridge::applyStructural` (via `handleAsyncUpdate` / `applyAllNow`, which also calls `applyToEngine`).
- Fix: `ParameterBridge::getEngineLock()`; the structural pass holds it, `processSlice` try-locks it around `applyToEngine` and the engine render, and outputs silence for a block that loses the race (the audio thread never waits). 6 clean stress runs after.
- Still open behind it: `ReverbPedal::rebuildLines` calls `std::vector::assign` on the audio thread when the Size parameter changes (allocation when the new length exceeds capacity) - RT-safety, not a crash any more.

### B-02 Strings self-excite after the rhythm engine stops; panic does not silence them. OPEN, critical

- Test: `Combo.unisonStringsNeverGrowAndPanicSilencesThem`, and `Combo.everyRhythmPatternAndGenreKit` ("keeps sounding after off", 27 of 65).
- Settings: defaults (`resetEverything`), factory pattern (any; e.g. #2 "Classic Strum"), rhythm engine free-running, chord C3-E3-G3 (48, 52, 55) held 2 s, then `RhythmEngine::setEnabled(false)`, all-notes-off on 16 channels and `LuthierAudioProcessor::panic()`.
- Symptom: with nothing played the output **grows** from -42 dBFS to -14 dBFS over 8 s (peak 0.36). `panic()` called again leaves it at -45 dBFS one second later.
- State at 8 s: strings 0-1 at 330 Hz, 2-3 at 196 Hz, 4-5 at 130.8/130.9 Hz (each chord tone voiced on two strings), all `Damping::None`, loop gain 0.989-0.992, strings 4-5 level 0.23. The unison pair is the one growing (f0 of the output 130.9 Hz).
- Suspected cause: (a) the rhythm engine's voiced strings are never released when it is disabled, and `LuthierEngine::panic()` chokes then `reset()`s the strings, after which something re-excites them; (b) the sympathetic coupling between two strings 0.1 Hz apart has a combined loop gain above 1 (the same pitch held on two strings in Guitar Controller mode, plucked and released, does NOT grow, so the energy must be coming from a driven source - check the feedback loop and the strum/chuck state the rhythm engine leaves behind). Owner: rhythm engine / string interaction (realism helpers).

### B-03 Released notes keep ringing through sympathetic coupling. OPEN, high

- Tests: `Combo.everyGuitarTypePlaysEveryPhrase` (27 of 192), pairwise (86 of 614), factory presets (32 of 288), snapshots (3).
- Settings (simplest): defaults, note 52 (a fretted E3) for 0.6 s. With `coupling_amount` 0.85 (default) the output 250 ms after note-off is only 6-9 dB under the note and decays at about 11 dB/s (-43 dBFS 2.2 s after release). With `coupling_amount` at its minimum (0.05) it drops 32 dB in 0.5 s.
- sustain-and-decay.md SUS-08 wants the released string more than 40 dB down by 250 ms. The released string is; the undamped open strings it excited carry most of the note on. Real sympathetic ring is roughly -25 to -40 dB relative.
- Suspected cause: coupling gain into undamped open strings too high at the default (string-interaction.md). Owner: realism helpers.

### B-04 Loud idle noise floor at high gain; no gate. OPEN, medium

- Test: `Combo.sustainFeaturesStayBounded` ("loud idle noise floor", 14), also 7 factory-preset renders and 6 advanced-range renders.
- Settings: defaults with `amp_gain` 1.0, `amp_master` 1.0, `sustain_scale` 1.0, `amp_model` 1, 4, 5, 8, 10, 11, 13. Idle floor -22.3 to -29.8 dBFS with nothing played (`hum_noise` 0.25, `noise_amp_buzz` 0.12 at their defaults, amplified by the amp).
- A NoiseGate pedal type exists, but no high-gain factory preset loads one and there is no gate on the amp path or the Easy tone strip (see GAPS_AUDIT U-2).

### B-05 Reset to defaults leaves the default guitar's body inconsistent. OPEN, medium

- `LuthierAudioProcessor::resetEverything()` vs a fresh instance: `body_bracing` Solid Body -> X-Brace, `body_top_wood` Alder -> Sitka Spruce, `body_back_wood` Alder -> Rosewood, `pickup0-2_magnet` Alnico 3 -> 5, `circuit_volume_pot`/`tone_pot` 250k -> 500k, `circuit_tone_cap` 47 -> 22 nF, `setup_fret_height`, `body_width/depth/age` also differ. The fresh instance has the default Strat's parts written into the parameters; Reset writes raw parameter defaults and the guitar reload does not overwrite them, so the UI shows an acoustic's woods on the Strat.

### B-06 Click at a preset switch. OPEN, medium

- Test: `Combo.presetSwitchUnderARingingNoteDoesNotClick`.
- Settings: preset #9 "J-Style Fingerstyle", note 52 ringing, switch to preset #10 "P-Bass Flatwound" at 0.5 s: sample step 0.311 in the 20 ms after the switch vs 0.034 before. The other 35 transitions are clean.

### B-07 Legacy `doubler_on` migrates on every load. OPEN, low

- `PresetManager::fromVar`: any state with `doubler_on` > 0.5 adds a Doubler pedal and sets `doubler_on` to 0, regardless of the preset's version. A host automating the (hidden) parameter, or a session saved with it on, changes its own rig on reload. Excluded from `everyParameterSurvivesTheSessionStateRoundTrip` with a pointer here. Suggest gating the migration on the preset's format version.

### B-08 Render not bit-reproducible after reset / across instances. OPEN, low

- `Combo.renderIsDeterministicAfterReset` (first run: 9 presets, max diff up to 0.016 on #25 "Shred Lead"; after merging review: passes) and `Combo.sessionStateRoundTripReproducesAudio` (after merge: #14 "8-String Djent" 0.00016, #18 "Drop C Riff" 0.0016, #21 "Modern Metal Chug" 0.00053; parameters all identical). Some noise or humanisation source is seeded per instance rather than from the state. Matters for offline bounce and freeze-track reproducibility.

### B-09 Notes below the instrument's range are silent. OPEN, info

- Preset #4 "Nashville High-Strung", palm-mute phrase (E2, note 40): noteRms -82 dBFS. Also one pairwise row. Out-of-range notes are dropped without transposing and without a notice; a user playing a keyboard part hears nothing.

### B-10 Preset-morph position lost from session state. FIXED (`748aeb9`, superseded by the integration branch's identical fix; `91f946f`)

- clap-validator state-reproducibility (3 tests): `preset_morph_position` 0.295 -> 0.0 after reload. Now covered by `Combo.everyParameterSurvivesTheSessionStateRoundTrip`.

### B-11 GUI: 45 automatable parameters have no control. OPEN / IN PROGRESS

- Test: `GuiReach.everyAutomatableParameterHasAVisibleControl`.
- No control anywhere: `scrape_*` (14), slap/pop/ghost/double-thump (25) - **in progress on `claude/luthier-techniques`** (TECHNIQUES tab); `macro_assign_a`, `macro_assign_b` (Macro 7/8, mod sources only); `pickup_blend` (also not read by the engine: `PickupEngine::setBlend` value unused in process).
- Control exists but never on screen: `strum_acceleration`, `strum_up_velocity_ratio`, `strum_tilt`, `strum_miss_probability`, `chuck_amount`, `chuck_damping`. Cause: the RHYTHM tab panel is laid out at the viewport height, not `RhythmPanel::preferredHeight()`, so the STRUM group's lower rows get zero height (at 1600x1000 the panel is 80 px tall on the integration branch; `claude/luthier-visual` makes it fill the viewport but still not its preferred height).
- Pedal slots: 69 slot `pN` knobs never appeared in the three pedal fills; p9 is used by no pedal type (max 9 parameters), the rest depend on which type sits in which slot. Summarised, not failed.
- Intentionally hidden (documented in the test): `feedback_on/threshold/speed`, `strum_speed`, `doubler_on/amount`, `fret_action`.

### B-12 pluginval Parameter thread safety exceeds its 30 s timeout. OPEN, low

- Passes with `--timeout-ms 900000`; times out at the default 30 s on a shared 4-core container. Likely each structural parameter write (guitar type, pedal type) triggers a full structural pass. Re-measure on an idle machine before acting.

### B-13 Idle CPU is nearly the playing CPU. OPEN, medium

- `Combo.cpuPerFactoryPreset` (48 kHz / 256, this container, not the reference CPU): playing 8.3-18.6% of a core, idle 8.3-19.0%. performance-budget.md: idle <= 1.5 units, heaviest preset <= 22. Playing cost is inside budget even here; idle is 6-12x over it. There is no silence short-circuit (strings, body, amp, cab and room all run on silence).
- Heaviest: #21 "Modern Metal Chug" 18.6%, #14 "8-String Djent" 18.3%, #25 "Shred Lead" 17.9% (idle 19.0%).

### B-14 Panic and Reset do not stop the transport-side players. OPEN, medium

- From the gap audit (A): `LuthierAudioProcessor::panic()` stops the audition and the engine only; the looper, backing track, tune player, metronome, progression looper and rhythm engine keep going.

## Passed

- 2000 seeded random rigs (every parameter random): finite, bounded, no subnormal storm, affordable - all pass.
- Modulation at full depth to all 331 float destinations: bounded and finite (6 decay failures are B-03).
- E-Bow, freeze, feedback and the hidden effect at maximum on every amp: bounded and finite.
- Advanced ranges at every extreme: bounded and finite.
- Every parameter survives a session state round trip (after B-10).
- 265 editor views, 2043 controls operated at min/max/restore: no crash, every attached control writes its parameter.
- Baseline suite (743 tests) green at `f63a7f7`.
