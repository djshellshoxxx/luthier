# PEDALBOARD V2 SPEC

Today a rig is two fixed racks of eight slots, one before the amp and one
after it. Order is slot order, and a pedal can only sit in its own rack. That
is a rack, not a pedalboard. A player thinks in patch cables: jack, cable,
pedal, cable, jack. Splits, parallel paths, a fuzz feeding only the amp, a
stereo delay into a stereo rig cannot be said in a slot list.

v2 replaces the racks with a **board**: any number of pedals, each with
jacks, wired by patch cables. The wiring is the signal order. The v1 racks
become the board's first sixteen pedal instances and keep their parameter
IDs, so automation, MIDI Learn and mod-matrix routes in existing presets
land on the same pedal after migration.

## 0. Ground rules

1. **Wiring is the order.** Moving a pedal on the canvas changes no sound
   unless it changes a cable.
2. **Structure is not automation.** Adding, removing, re-wiring or retyping a
   pedal is an edit compiled on the message thread (3.3). No host parameter
   adds a pedal.
3. **Instance identity survives moves.** Automation addresses the instance,
   not the place.
4. **Bypass never changes latency** (3.5, PB-07).
5. **No allocation or lock on the audio thread.** The program is a flat op
   array swapped at a block boundary, as `EffectsChain` swaps pedals today.
6. **Migration is lossless.** Every v1 preset renders bit-identically (PB-M01).
7. **Nothing is added by default.** New pedals start soft-bypassed with a tier
   that changes nothing until chosen.

## 1. User stories

- **U1.** Drop a delay after my overdrive and join their jacks. No slot numbers.
- **U2.** Split my guitar: fuzz on one path, clean on the other, merged before the amp.
- **U3.** A stereo chorus and reverb feed a stereo rig (`acoustic-electric-and-stage-v2.md` 5).
- **U4.** A compressor before the amp and a reverb in the loop, on one board.
- **U5.** A footswitch puts the delay in true bypass; the tail stops in 10 ms and latency holds.
- **U6.** Buffered bypass keeps the cable before a pedal off my pickup (`cables-and-brands.md` 4).
- **U7.** Save a whole board as a preset. **U8.** A v1 preset opens as a board, sounding the same.
- **U9.** Wire, move and bypass everything from the keyboard (2.5).

## 2. UI

A **BOARD** tab in the Advanced panel. Easy Mode shows a simplified strip.
Standalone uses the same panel.

### 2.1 Easy Mode

One row per pedal in topological order (splits indent). Each row has the name,
a footswitch LED, a bypass button ("Latency does not change."), a mix knob and,
on split rows, level A and B. Cables are hidden; anything audible that Easy
cannot show gets a summary line (`gui-integration.md` rule 3). Notices (empty
board, dead end, unmerged split, over budget) are one line each; PB-13 pins them.

### 2.2 Advanced: canvas

- **Jacks.** Mono pedal: one in, one out. Stereo pedal: L and R on each side.
  Split: one in, two out. Merge: two in, one out. AMP IN is one mono jack;
  OUT L and OUT R are the stereo pair.
- **Cables.** Beziers from output to input, each with a tier badge.
- **Lanes.** Columns by depth. A layout hint, not signal order.
- **Amp divider.** A dashed line at the AMP IN lane. Left is pre-amp, right
  post-amp. Not draggable. In-loop pedals sit on the loop segment (3.4).
- **Zoom.** Ctrl+wheel or pinch, 50-200 %.

Library: a drawer grouped Dynamics, Drive (incl. Preamp Stage), Filter and
pitch, Modulation, Time and space, EQ, Utility (Buffer, Ground Isolator). Click
adds an instance; drag onto a cable inserts it. Items locked by edition read
"Pro".

### 2.3 Cable popover

Tier (Unmodelled, Economy, Standard, Pro, Boutique; Standard for new cables,
Unmodelled for migrated), length (0.3-10 m), balanced (bool) and delete. Tier
meaning is in `cables-and-brands.md`. The guitar input cable's popover edits
the existing `cable_quality` and `cable_length` instead.

### 2.4 Pedal face

Each `PedalParam` (`Pedal.h` 22-79) is a knob or selector. The header holds
name, bypass mode (Soft, True, Buffered; Soft for migrated, True for new),
pop (on by default), tier, footswitch (MIDI Learn target) and nominal latency
(read only). Each control has a tooltip that states its effect in one sentence.

### 2.5 Drag, drop, keyboard

Drag a pedal to move it (no sound change), onto a cable to insert it, or
drag a cable end to empty canvas to delete it. Jack to jack draws a cable;
output to output is refused. Keyboard: Tab walks pedals in graph order, arrows
move a pedal one lane, Enter opens the face, Ctrl+arrow moves a cable end,
Space toggles bypass. Every drag has a keyboard path (`accessibility.md`).
Every edit is one undo entry (`action-and-undo.md` 3.13).

## 3. Engine and data model

### 3.1 What exists (the delta starts here)

- `PedalType` (`Source/DSP/Effects/Pedal.h`): `None` plus 23 types (eleven
  flagged pre-amp, ten post-amp, then Doubler and Gater). `editions.md` 2.2
  says "of 22" (section 10).
- `Pedal::isPreAmpPedal` / `isPostAmpPedal` are advisory; only search reads
  them (`SearchProviders.cpp` ~936).
- Two `EffectsChain` instances (`LuthierEngine.cpp` 127-132), eight slots each
  (`kNumSlots = 8`), in series. `moveSlot` works only inside one chain.
- Processing (`LuthierEngine.cpp` ~3459-3525): DI, `preEffects`,
  `fx_saturation`, `AmpEngine`, `postEffects`, cabinet, room, master.
- Bypass: a 10 ms `SwitchCrossfade` only (`Pedal.cpp` line 81). No true or
  buffered bypass.
- Parameters (`Parameters.cpp` 735-760): `preN_type/_bypass/_mix/_p0-_p9`, 16
  slots x 13 = 208 host parameters.
- **Latency defect.** `EffectsChain::getLatencySamples()` sums only non-bypassed
  slots (`EffectsChain.cpp` 268-277), and `updateLatency()` runs every block
  (`PluginProcessor.cpp` ~2352). A bypass toggle changes the reported latency.
- CPU: `performance-budget.md` 1-2. MIDI Learn and the mod matrix reach bypass.

Not present: patch cables, splits, merges, jacks, true or buffered bypass,
board presets, bypass-independent latency, an in-loop insert inside the amp.
The amp takes one mono signal (`amp.processSample(mono)`, ~3492).

### 3.2 The board graph

```
GUITAR ●─c1─[Comp]─c2─┬─[Drive]────c3─┐
                      │               ├─c5─● AMP IN ─[AMP]─┬─[LOOP]─c6─[Reverb st.]─c7─● OUT L/R
                      └─[Delay st.]─c4┘   (merge)          │   (in-loop, 3.4)
                                                           └──────────────────────────────┘
```

| Node | In | Out | Notes |
|---|---|---|---|
| Source | 0 | 1 mono | Guitar DI (Aux 1, `routing-io.md` 2) |
| Pedal | 1 | 1 | One instance; kind is the PedalType; mono or stereo |
| Split | 1 | 2 | Copies; each output has a level (4) |
| Merge | 2 | 1 | Sums; rules in 3.4 |
| AmpIn | 1 mono | 0 | Exactly one per board |
| Sink | 2 (L, R) | 0 | OUT L and OUT R |
| LoopSend / LoopReturn | 1 / 1 | 0 / 1 | In-loop only (3.4), Pro |

### 3.3 Compile and swap

1. **Edit** (message thread) on a plain `BoardModel` value type
   (`Source/Model/Board/BoardModel.*`), validated against PB-13.
2. **Compile** (message thread). `BoardCompiler` topologically sorts the graph,
   assigns buffers from a pool sized for maximum depth and width, and emits a
   `BoardProgram`: a flat op array (`RunPedal`, `Copy`, `Sum`, `Gain`, `Sink`).
   Surviving instances are reused. Tier and bypass filter coefficients are
   computed here, never in `process`.
3. **Hand over.** The program is published through the try-lock swap
   `EffectsChain::applyPendingSwaps` uses, and adopted at the next block.
4. **Retire.** Removed pedals go to the existing `retired` list and are freed
   off the audio thread. A removed pedal fades out over 50 ms before it leaves
   the program, so deleting a delay does not click.

`BoardProgram::process` loops over a fixed array. Buffer pool:
`maxDepth x maxWidth x 2` channels; 24 pedals bound depth, 8 bound width.

### 3.4 Mono, stereo and the loop

- Mono into a stereo input: duplicated, L = R.
- Stereo into a mono input (Merge input, AMP IN): `(L + R) x 0.7071`, equal
  power. A notice shows when stereo reaches AMP IN.
- Stereo through the amp: `acoustic-electric-and-stage-v2.md` 5.
- **In-loop.** `AmpEngine` (`AmpEngine.h`) is one object: preamp, tone stack,
  phase inverter, power amp, sag. In-loop needs the signal to leave after the
  tone stack and re-enter before the phase inverter. v2 splits `processSample`
  into `processPreamp` and `processPower` with LoopSend and LoopReturn
  between. Until then in-loop pedals are placed post-amp, as in v1, and the UI
  says so.

### 3.5 Bypass and latency

- **Soft (v1).** The pedal runs; wet crossfades against dry over 10 ms.
  Kept for migrated presets.
- **True.** A switch takes the pedal out of the path. With `popEnabled` it adds
  a modelled 4 ms pop (`cables-and-brands.md` 5.4). The cable is not isolated:
  the cable and the next input both load the source.
- **Buffered.** The pedal is off, but a unity buffer (1 MOhm in, 1 kOhm out,
  design values, `cables-and-brands.md` 4.2) sits at its input, so the cable
  before it no longer loads the source. Processing is skipped.

**Latency rule.** `BoardProgram::latencySamples` is the maximum over all
paths of the sum of nominal pedal latencies, counting every pedal on the path
whether on or off. Split and merge add zero. It changes only on a structural
edit. The reporting call is unchanged; only the value it reads changes.

### 3.6 Files

New: `Source/Model/Board/*`, `Source/DSP/Board/*`, `Source/UI/Board/*`. Small
marked edits (`docs/HANDOFF.md`): `LuthierEngine.cpp` (processing calls),
`EffectsChain.*` (legacy adapter), `Parameters.*`, `PresetManager.cpp`,
`PluginProcessor.cpp` (latency read).

## 4. Parameters

Existing and frozen: `preN_*` and `postN_*`, N = 1..8, 208 parameters.

New, appended inside `// ==== BEGIN V2-BOARD params ====` /
`// ==== END V2-BOARD params ====`:

| ID | Name | Range | Default | Automatable | Visible control |
|---|---|---|---|---|---|
| `board_split{k}_level_a` | Split k A | -60 to 0 dB | 0 dB | yes | Board row slider (Easy and Advanced) |
| `board_split{k}_level_b` | Split k B | -60 to 0 dB | 0 dB | yes | Board row slider |

k = 1..4: eight IDs. A split takes its index in creation order; deleting it
leaves its levels unused. Host count becomes 216. The literal sum in
`Source/Tests/IntegrationTests.cpp` is updated per `docs/HANDOFF.md`.

**Not parameters** (state, 5): pedal type, tier, bypass mode, pop, footswitch,
cable tier and length, balance, topology, loop position, names, power mode.

### 4.1 New pedal types (appended after Gater)

| Index | Type | Parameters | Notes |
|---|---|---|---|
| 23 | Buffer | Mix | `cables-and-brands.md` 3.3 |
| 24 | GroundIsolator | Lift: Off, Lift, Isolate | `cables-and-brands.md` 3.3 |
| 25 | PreampStage | Gain, Bass, Mid, Treble, Channel (Clean, Crunch, Lead) | Amp-front pedal: tube gain stage from `AmpEngine` 11.2 |

Appended so saved slot types keep their meaning (`ambiguity-resolutions.md` 3).

### 4.2 Instance binding and footswitches

Instances 0-7 are `pre1`-`pre8`; 8-15 are `post1`-`post8`: the only
host-automatable instances in v2.0 (display names change to "Pedal 1"; IDs do
not). Instances 16-23 are saved and footswitchable but not automatable
(section 10, item 1). Deleting a pedal frees its instance and clears its
automation after confirmation.

Footswitch table (state): up to 24 entries, MIDI note (on/off) or CC (above 64
is on). Instances 0-15 write a standard MIDI Learn mapping to `preN_bypass`.
A reserved note range (MIDI 0-23 by default) is excluded from Learn.

## 5. State, file format and migration

### 5.1 Board block

Presets gain one optional block (`file-formats`):

```json
"board": { "version": 1,
  "pedals": [ { "instance": 0, "type": "Overdrive", "name": "Drive", "tier": "Unmodelled",
                "bypassMode": "soft", "popEnabled": true, "mix": 1.0 } ],
  "splits": [ { "id": 1 } ], "merges": [ { "id": 1, "inputs": ["c4", "c3"] } ],
  "cables": [ { "id": "c1", "from": "source", "to": { "pedal": 0, "in": 0 },
                "tier": "Unmodelled", "lengthM": 3.0, "balanced": false } ],
  "powerMode": "daisy", "inLoop": null }
```

Instances 0-15 keep values in their `preN_`/`postN_` keys; instances 16-23 keep
ten normalised values in `pedals[i].params`. A linear board with only v1
features is written in v1 format, so v1 readers keep working.

### 5.2 Migration from v1

A preset with no `board` block becomes:

1. Pre slots 1-8 in order, empty slots dropped; instance = slot - 1; soft
   bypass; tier Unmodelled.
2. Source, pre pedals, `fx_saturation` (a fixed stage, not a board node), AmpIn.
3. The amp, post slots 1-8 (instance = slot - 1 + 8), Sink.
4. One cable per adjacency, tier Unmodelled. `cable_quality` and `cable_length`
   stay on the input cable.

The migrated board renders bit-identically (PB-M01). Unmodelled applies no
voicing, noise or loading; soft bypass is the v1 crossfade.

### 5.3 Back-compatibility and board presets

A v2-only board writes the block; a v1 reader loads the pedals it knows and
shows "This preset uses board features." A board preset is a preset whose
board block is the whole board, tagged "Board" in the browser
(`preset-browser-previews.md`).

## 6. Edition

Free (`editions.md` 2.2, today 4 + 4 slots): 8 pedals, host-automatable
instances 0-7, soft bypass only, Standard tiers only, no splits, merges,
in-loop, Buffer or Ground Isolator, no board save (load only). Pro: 24 pedals,
instances 0-15 automatable, all bypass modes, all tiers, all utilities, board
save, in-loop from M3. Pro features on a Free board show read-only with "Board
features need Pro." A Free board renders exactly as Pro with the same pedals.

## 7. Performance budget

Units per `performance-budget.md` 0.

- Per pedal: the existing per-type cost (1), capped at 0.5 (2). New: Buffer
  0.01, GroundIsolator 0.02, PreampStage 0.2 (oversampled, like Overdrive).
- Routing: 0.005 units per op; 40 ops add about 0.2.
- Board budget: pedals plus routing capped at **6 units**. Over budget still
  plays, with an amber notice. The "Heavy board" case (two reverbs, two delays,
  four drives, eight modulation pedals) sums to about 2.0 units, inside the
  Heavy preset total of 22.
- Existing presets keep their totals; the refactor adds only routing overhead.

## 8. Tests

- **PB-00 (audit).** Impulse through every type, mono and stereo; copy-L-to-R types are documented.
- **PB-01 (wiring is order).** A to B and B to A differ by more than -60 dB RMS; a canvas move is bit-identical.
- **PB-02 (split/merge).** A 0 dB split re-merged matches the dry signal to -90 dB.
- **PB-03 (stereo).** Mono to stereo and back is within 0.1 dB; stereo into AMP IN is summed once with a notice.
- **PB-04 (determinism).** Two compiles of one `BoardModel` give identical ops and samples.
- **PB-05 (no allocation).** 30 s of 50 random edits under audio: zero allocations in `BoardProgram::process`.
- **PB-06 (click-free swap).** Largest sample step during a mid-note edit stays within 1.5 x steady-state maximum.
- **PB-07 (latency).** Toggle all 24 pedals in every mode, random order: latency stays at the nominal longest path. Fails on current code.
- **PB-08 (pop).** Peak -44 to -36 dBFS, 2-6 ms wide; off with `popEnabled` off.
- **PB-09 (buffer).** A 10 m Economy cable before a Buffer matches 0 m at 5 kHz within 0.5 dB (= CB-07).
- **PB-10 (identity).** Automate `pre3_p2`, move instance 2, save, reload: value and automation kept.
- **PB-11 (footswitch).** A note toggles only its instance; note-on and note-off within 5 ms do not double-toggle.
- **PB-12 (in-loop).** After the AmpEngine split, direct send-return matches the unsplit amp to -90 dB; before it, the approximation notice must show.
- **PB-13 (validation).** Each notice in 2.1 fires on a constructed board; cycles are rejected.
- **PB-M01 (migration).** Every factory preset renders bit-identically via v1 and migrated paths at 44.1, 48, 96 kHz.
- **PB-M02 (round trip).** A v1 preset saved without edits is byte-identical.
- **PB-M03 (forward).** `board.version` 2 loads the v1 racks with the notice.
- **PB-E01 (Free).** No ninth pedal, no split, Standard tiers only.
- **PB-P01 (layout).** Host count 216; `everyAutomatableParameterHasAVisibleControl` passes.
- **PB-U01 (manual).** Every drag in 2.5 completes by keyboard on Windows, macOS, Linux Standalone.
- **PB-U02 (manual).** A two-path board shows its Easy summary line.

## 9. Effort and dependencies

One engineer with an AI pair. Estimates in engineer-days (ED); a week is 5 ED.

| Work | ED |
|---|---|
| Core: BoardModel, compiler, program, bypass, latency fix | 25 |
| Migration, presets, parameter block | 11 |
| UI: canvas, library, popovers, face, keyboard | 19 |
| AmpEngine split (in-loop) | 6 |
| Tests PB-00 to PB-P01 | 8 |
| **Total** | **69 ED, about 14 weeks** |

Dependencies: `cables-and-brands.md` (tiers, buffered input, pop);
`preset-browser-previews.md` (thumbnail); `amp-cab-ir.md` (in-loop bypass).
The latency fix needs nothing and ships first.

## 10. Open questions

1. **Automation past 16 pedals.** Add `pb16`-`pb23` IDs (96 appended
   parameters) or keep the limit? Recommend M4, on demand.
2. **Free count.** `editions.md` says 15 of 22; the enum has 23 and v2 adds
   three. Write the Free list out in full there, marking Gater and Doubler.
3. **Pre/post flags.** Remove the advisory flags or keep them for search as
   `typicalPlacement`? Recommend keep and rename.
4. **Soft bypass.** Visible third mode, or hidden in Advanced? Recommend visible.
5. **Stereo amp.** Is a stereo amp input in scope, or does stereo always sum
   at AMP IN?
6. **Board size and pop value.** 24 pedals, 8 lanes and the 4 ms, -40 dBFS pop
   are design values; measure before fixing.
