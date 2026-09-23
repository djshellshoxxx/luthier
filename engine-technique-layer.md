# ENGINE TECHNIQUE LAYER DELTA

Additive changes to `engine.md`. Introduces four new micro-modules
(`ScrapeEngine`, `SlapEngine`, `TapEngine`, `MuteEngine`) and one
resolver (`CascadeResolver`). Existing modules gain small
integration points but no behavioural changes to prior features.

This spec is deliberately narrow: minimum viable change to make
the six new techniques work without disrupting any code already
written.

## 0. Ground rules

1. **Additive only.** No existing module removed, renamed, or
   restructured.
2. **Every new module has a zero-cost idle path.** Idle = single
   flag check per block, no allocations, no state churn.
3. **Every new module follows engine.md's DSP rules**: double
   precision, no audio-thread allocations, DC blockers, NaN guards,
   `reset()` implemented.
4. **New modules attach via the existing command queue.** UI arms
   and configures via message-thread commands; audio thread reads
   parameter atomics.

## 1. New modules

Sit alongside existing per-block pipeline. Each is a JUCE-style
`processBlock`-capable class with `prepareToPlay`, `reset`,
`processBlock`.

- `ScrapeEngine` — per `string-scraping.md` 3.
- `SlapEngine` — per `string-slap-technique.md` 2. Also handles
  Body Tap (routes bypass to `BodyCoupling`).
- `TapEngine` — per `two-hand-tapping.md` 4.
- `MuteEngine` — new for `muting-rhythm.md`; feeds damping into
  `StringEngine` per beat.
- `CascadeResolver` — per `technique-cascade.md` 4.

## 2. Insertion point

Existing engine.md per-block pipeline (simplified):

```
processBlock:
  1. read MIDI + parameters
  2. TechniqueEngine.process
  3. RhythmEngine.process
  4. ModulationMatrix.process
  5. StringEngine.process (excitations + boundary conditions)
  6. PickupEngine, GuitarCircuit, AmpEngine, CabEngine
  7. Effects rack
  8. Master bus
```

New pipeline (added steps 2b, 2c):

```
processBlock:
  1. read MIDI + parameters
  2. TechniqueEngine.process
 2b. CascadeResolver.process     [NEW]
 2c. ScrapeEngine, SlapEngine, TapEngine, MuteEngine.process [NEW]
  3. RhythmEngine.process (now consumes MuteGrid)
  4. ModulationMatrix.process
  5. StringEngine.process        (now consumes tap/scrape/slap
                                  excitations and mute damping)
  6. PickupEngine, GuitarCircuit, AmpEngine, CabEngine
     (Body Tap events from SlapEngine feed BodyCoupling directly)
  7. Effects rack
  8. Master bus
```

## 3. Minor changes to existing modules

Small, non-breaking additions:

### 3.1 TechniqueEngine
- Add gesture types for scrape, slap, tap, body-tap.
- Add `emit(GestureEvent&)` method.
- No changes to existing techniques (palm mute tag, harmonic tag,
  etc).

### 3.2 StringEngine
- Excitation input already accepts multi-source impulses per
  `harmonic-realism.md` 2. Scrape catches, slap impacts, tap events
  reuse that interface. **No StringEngine internal changes.**
- Boundary conditions: tap-hold acts as a movable capo; the string
  engine already handles a movable damping+contact point (existing
  slide implementation). Extended to allow N concurrent contact
  points (was 1); other tests unaffected.
- Bend offset input: existing bend input already accepts a summed
  offset per string; microtonal bends layer additional sources into
  that sum via the mod matrix.

### 3.3 RhythmEngine
- Pattern step schema gains optional `mute_type` field. Default
  "open" for patterns that don't specify. No migration required.
- Emits `mute_type` alongside each note-on event.

### 3.4 BodyCoupling
- Existing bank of body modes gains a `driveDirect(impulse,
  position)` entry point used by Body Tap events (bypass strings).
- No changes to normal body-coupling behaviour.

### 3.5 ModulationMatrix
- Adds `PreBendEvent` as a modulation source class per
  `microtonal-bends.md` 4.
- Adds a "microtonal bend range" per-string scale factor on the
  existing bend destination.
- Existing routes unaffected.

### 3.6 MidiInterpreter
- Recognises new keyswitch numbers for scrape / slap type / tap /
  pre-bend triggers. New KS numbers reserved from an unused range
  (see `controllers.md`); no conflict with existing KS.
- MPE per-channel Y-axis and Z-axis routing extended to feed slide
  position and microtonal bends when the user assigns those sources.

## 4. Command / result queue additions

New command types:
- `ArmTechniqueCommand { technique, on }`
- `SetTechniqueParamCommand { technique, param_id, value }`
- `TriggerGestureCommand { gesture_data }` (for on-screen triggers)
- `LoadMuteGridCommand { grid_data }`

New result types (for UI):
- `TechniqueFiredResult { technique, string, timestamp, intensity }`
  for live overlay indicators.

All queue interactions follow the existing pattern from
`ui-wiring.md`; no threading changes.

## 5. Parameter tree additions

New APVTS parameters (all optional; presets without them work
unchanged):
- Per-technique arm booleans (6 params).
- Per-technique numeric controls per each technique's spec section
  on user controls. Grouped in APVTS under
  `parameters/techniques/<technique>/*`.

Total new parameter count: ~60. Well within the plugin's existing
budget. `state-model.md`'s state layer counts unaffected structurally
(these parameters live inside preset like every other parameter).

## 6. Migration for existing presets

- Presets saved before these specs: load with all techniques
  disarmed and default controls. Round-trip works.
- Old bass-slap presets: continue to work via
  `bass-techniques.md`'s existing SlapEngine interface, which is
  the same underlying module.

`file-formats.md` version bump not required; the new fields land in
existing schemas' extension areas (already-tolerant of unknown
fields per file-formats.md 0.3).

## 7. State model additions

Per `state-model.md`:
- Techniques arm state and controls live inside PRESET.
- No new state layers.
- Snapshot bank can capture technique arm state per snapshot
  (recall flips arm state accordingly).
- Live gesture triggers are session state (not persisted).

## 8. Undo taxonomy additions

Per `action-and-undo.md`:
- Arm / disarm technique: new class `technique-arm`. Grouping: no.
- Change technique parameter: new class `technique-param`. Grouping:
  200 ms same-param merge.
- Live gesture triggers: **not undoable** (audio events).
- Paint a mute-grid step: new class `mute-grid-paint`. Grouping:
  200 ms same-step merge.

## 9. Performance budget additions

Per `performance-budget.md`:
- Idle: < 0.1% total across all four new modules.
- Active single technique on busy passage: ~0.5-1% per module.
- All six techniques active (worst case cascade): ~2.5% total.
- CascadeResolver: negligible (O(strings)).

Budget accommodated within existing headroom on the mid CPU class.
Low CPU class defaults new modules to idle-ready-but-off; user can
enable with a CPU-warning banner.

## 10. Tests

Every test enumerated in the six technique specs plus:
- Pipeline order test: verify new modules execute at documented
  points (fixture instrumented with counters).
- Additive-only regression: run every existing test suite before
  and after this delta; zero regressions.
- Preset compatibility: load 100 pre-delta presets, verify byte-
  identical playback.
- Command queue: 1000 arm / disarm / trigger commands per second
  handled without allocation on the audio thread.
- Migration: old bass-slap preset loads with legacy fields
  correctly mapped to new SlapEngine gesture format.

## 11. What this delta does NOT change

- No changes to preset file format bumps.
- No changes to state serialization format.
- No changes to host integration.
- No changes to existing MIDI export event classes (new classes
  added, existing unchanged).
- No changes to file-formats.md 13 atomicity rules.
- No changes to error-recovery.md failure modes (new failure modes
  added, existing unchanged).

Everything already built stays built.
