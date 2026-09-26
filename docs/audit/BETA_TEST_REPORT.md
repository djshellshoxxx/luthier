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

### B-02 Strings grew after the rhythm engine stopped; panic did not silence them. RESOLVED

- `Combo.unisonStringsNeverGrowAndPanicSilencesThem` and `Combo.everyRhythmPatternAndGenreKit` (all 37 patterns and 32 kits) pass on the current branch: the integration branch's review fixes plus class 5 here (pedal-up release, released-string damping) left no growing or undamped string after the engine stops and panic. Both tests stay in the suite as the regression guard.

### B-03 Released notes rang on. FIXED (class 5: release cap + pedal-up release; test corrected)

- Tests: `Combo.everyGuitarTypePlaysEveryPhrase`, pairwise, factory presets, snapshots ("does not decay after release"); `Combo.releasedStringIsDampedQuickly`, `Combo.liftingTheSustainPedalReleasesItsNotes`.
- What was wrong in the TEST: it demanded the mix fall 30 dB within ~2.5 s of the release. Measured per string, the sympathetic ring the played note leaves on the open strings is 26-37 dB under it at the default `coupling_amount` (the engine spec's weak coupling; real guitars 20-40 dB), and those undamped strings decay at their own open T60 of several seconds, exactly as a real guitar's do until a hand mutes them. The earlier "6-9 dB down at 250 ms, coupling too strong" reading in this report was wrong. The check is now: tail 20 dB under the note (or under -60 dBFS, or at the rig's floor) AND still falling (2 dB over the last second).
- What was wrong in the ENGINE (two things):
  1. `StringEngine` `Released` damping scaled the open T60 by 0.13, so a released note on a 4.5-7 s string took 0.6-0.9 s to die (21-28 dB down at 250 ms; SUS-08 asks > 40, and a lifted fingertip stops a string in 0.2-0.4 s). The released T60 is now capped at 0.3 s (geometric in the damping amount, so SUS 5.1's release ramp still eases in). At the output with coupling at minimum: 36-40 dB at 250 ms for every guitar type (the test allows 35 at the output for amp/cab/coupling residue).
  2. `MidiInterpreter` pedal-up was a stub (a loop that only `continue`d): notes let go under the sustain pedal were never damped when it lifted, and all-notes-off skipped them too. Implemented for CC 64 and CC 66 (sostenuto), plus all-notes-off. This was every "does not decay" case on the basses (phrase sustainPedal).

### B-04 Loud idle noise floor. FIXED (class 4: wrong pickups on every factory preset; threshold made physical)

- Root cause of most reports: factory presets (and Reset) carried the layout defaults for the parameters a guitar's parts own, and a preset's values beat the parts - so every SG, Les Paul and 335 preset played three single coils (and an X-braced spruce top) and hummed like a Strat. "Octave Fuzz Stoner" (an SG) idled at -25 dBFS. Fixed with B-05 (below).
- The threshold: the absolute -30 dBFS ceiling was physically wrong. The idle floor is entirely the pickup's mains hum (`noise_amp_buzz`; with it at 0 the floor is -100 dBFS: no DC, no oscillation, no hiss). At the pickup it sits about 50 dB under a clean chord (real single coils: 30-50 dB). An amp with gain and master both on 10 gives the hum its full small-signal gain while the chord saturates, so it lands 3.5-18 dB under the playing depending on the model (British 800 3.5, Boutique Lead 4.3, Crunch 120 5.6, Plexi 7.6, the rest 11-24) - which is what a real dimed high-gain amp does with single coils, and why players gate. The check is now signal-to-noise: any rig's floor below its own playing level; a shipped factory sound's 20 dB below (above -60 dBFS).
- Fuzz Face Lead (single coil, germanium fuzz, Plexi) sat 12-18 dB under: it now sets a quieter `noise_amp_buzz` (0.04, a shielded guitar) rather than a gate that would cut the fuzz's decay.
- Still open: no gate on the amp path or the Easy tone strip (GAPS_AUDIT U-2); the four hottest amp models at full gain/master are physically plausible but noisy.

### B-05 Reset and every factory preset ignored the guitar's own parts. FIXED (class 4)

- Factory presets now carry a guitar block with `partsWin` and the recipe's own values (`keep`); the load writes the guitar's parts even over the just-loaded layout defaults (bypassing the `writtenSinceGuitarType` guard, which took the preset's own writes for the host's), then restores only the recipe values the parts replaced - so "Drop C Riff" keeps its tuning and a host's later edits are untouched. Reset passes `partsWin` too. Generated factory files carry `factoryRevision` (3); `writeAll` regenerates an older generated file (never a user edit: PresetManager writes no revision, and older generated files had no guitar block while every saved preset has one).
- Test: `Combo.factoryPresetsAndResetUseTheGuitarsOwnParts`.

### B-06 Click at a preset switch. FIXED (class 2)

- A preset switch under a ringing note cut it mid-cycle: J-Style Fingerstyle -> P-Bass Flatwound stepped 0.32 of full scale 72 samples into the next block. The processor now fades out (5 ms) before a preset load or structural pass and back in after, the message thread waiting up to 60 ms for the audio thread's fade (outermost call only; skipped when no other thread is rendering). The fade length first collapsed to one sample because `getSampleRate()` is 0 until a host sets it; it uses the prepared rate.
- The test now renders on its own thread while the message thread loads, as a host does; all 36 factory transitions pass.

### B-07 Legacy `doubler_on` migrates on every load. OPEN, low

- `PresetManager::fromVar`: any state with `doubler_on` > 0.5 adds a Doubler pedal and sets `doubler_on` to 0, regardless of the preset's version. A host automating the (hidden) parameter, or a session saved with it on, changes its own rig on reload. Excluded from `everyParameterSurvivesTheSessionStateRoundTrip` with a pointer here. Suggest gating the migration on the preset's format version.

### B-08 Render depended on what was played before. FIXED (class 3)

- "State round trip differs" (8-String Djent, Drop C Riff, Modern Metal Chug, up to 0.0016) was not missing state: the saved state was identical. The source instance had played other presets first, and `StringEngine::reset()` kept each string's last-note termination brightness (nut or fret material, part-acoustics 4) and sustain scale - a loop cutoff 0.9% high on four strings. Both are now reset. Test: `Combo.renderDoesNotDependOnWhatWasPlayedBefore` (bit-identical).

### B-09 Notes below the instrument's range are silent. BY DESIGN (class 1); notice still missing

- The voicer drops pitches no string can sound (RubricVoicer, exact mode). Nashville High-Strung's low strings are an octave up, so the palm-mute phrase's E2 is not the instrument's to play. The harness now moves each phrase up by whole octaves to the guitar's lowest open string, and `Combo.notesBelowTheRangeAreDroppedAndInRangeNotesSound` pins the behaviour. A notice or an octave-fold option is still worth having (GAPS_AUDIT U-7).

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

### B-15 `string_age` Old nearly silences a bass above E3. OPEN (realism-a owns string aging)

- Found while fixing B-05: with the P-Bass's real parts, "P-Bass Flatwound" (`string_age` Old) plays G3 at -52 dBFS where the same bass with Fresh strings plays it at -12; notes above about A3 are silent. Old strings go dull and short, not mute. `Combo.everyFactoryPresetPlaysEveryPhrase` fails 5 renders of this preset on it; left failing for the owner.

## Passed

- 2000 seeded random rigs (every parameter random): finite, bounded, no subnormal storm, affordable - all pass.
- Modulation at full depth to all 331 float destinations: bounded and finite (6 decay failures are B-03).
- E-Bow, freeze, feedback and the hidden effect at maximum on every amp: bounded and finite.
- Advanced ranges at every extreme: bounded and finite.
- Every parameter survives a session state round trip (after B-10).
- 265 editor views, 2043 controls operated at min/max/restore: no crash, every attached control writes its parameter.
- Baseline suite (743 tests) green at `f63a7f7`.
