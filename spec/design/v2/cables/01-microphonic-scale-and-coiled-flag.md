# CB-06: Microphonic scale and coiled flag

**Items:** CB-06
**ED:** 1
**Source:** `cables-and-brands.md` 2, 3.4, 10 (CB-06), 11

## Summary

Scale the existing cable-movement noise source by the cable tier, and add a `coiled` construction flag that adds +3 dB.

## Status

Partial. `noise_cable_movement` exists (`Source/DSP/Noise/NoiseFloor.cpp`, `kCablePeakDb = -42` dB for a 3 m Standard cable, `kSaltCable = 0xCAB1`). There is no tier scale and no coiled flag. v1 `cable_quality = Vintage` (220 pF/m) has no microphonic penalty.

## User-facing behaviour

- Each patch cable (`pedalboard-v2.md` 2.3 popover) has a `coiled` toggle. Turning it on adds crackle; the popover shows "coiled" in the tier label.
- The input cable's `cable_quality` choice sets the same scale through the mapping in section 2: Cheap (Economy) is +6 dB, Studio (Pro) is -6 dB, Standard is 0 dB, Vintage is Economy construction plus coiled (+6 dB + 3 dB = +9 dB total).
- Audible effect only on movement events. No change to the event rate.

## Engine / DSP design

- Base level stays as v1: `base = kReferencePluckPeak * dbToLinear(kCablePeakDb)`.
- New pure function (message thread, not `process`):

  ```
  cableGainLin(tier, coiled) = base * dbToLinear(tierDb(tier) + (coiled ? 3.0 : 0.0))
  tierDb: Economy +6, Standard 0, Pro -6, Boutique -12, Unmodelled -inf (source off)
  ```

- `NoiseFloor::cableGain()` gains an overload taking the computed linear gain. The v1 zero-arg form remains and returns `base`, so v1 is unchanged.
- The cable-movement source is the one already in `NoiseFloor`. Each patch cable with `tier != Unmodelled` contributes one event stream, seeded `seed ^ kSaltCable ^ cableIndex` (the salt scheme already used in `NoiseFloor.cpp:300`). Streams are summed in power.
- Design decision: cable length does not scale this source. The spec gives no length term for 3.4 and the 3 m reference is already the tier value. Recorded as an open point below.
- Gain changes are applied through the existing per-block smoothing used for noise levels (no zipper, no new smoother).
- Double precision for the gain math. Event amplitudes are stored as `float` only where `NoiseFloor` already does.

### RT safety

- The linear gain is computed on the message thread when the board compiles (ground rule 5). `process` reads a precomputed `double`.
- No allocation: event streams are preallocated per cable slot at compile.
- `reset()`: clears event timers and the active-event list; does not change gains. A reset does not start a crackle burst.

## Data model and parameters

- No new host parameter. Ground rule 6 and section 6.1 apply.
- Patch cable fields (board state): `coiled: bool`, default `false`. `tier` is shared with file 05.
- Input cable: no new field. `cable_quality` mapping is the only input.

## State, file format and migration

- Board block: patch cable `coiled` (bool, default false). Append-only with the board schema version (file 03).
- v1 presets: `cable_quality` keeps its value and meaning. Migrated patch cables are Unmodelled and produce no movement events (CB-M01 holds).
- Open point (blocking for migration): v1 `cable_quality = Vintage` gains the +9 dB penalty described above. That changes the render of existing v1 presets that use Vintage, which conflicts with ground rule 4 and CB-M01. Proposed resolution: store a per-preset `cableMappingVersion` and keep v1 values for presets saved before v2. Decide before this file ships.

## Edition gating

| Feature | Free | Pro |
|---|---|---|
| Patch-cable tier scale | Standard only (0 dB) | all tiers |
| Patch-cable `coiled` | no (greyed, "Pro") | yes |
| Input cable `cable_quality` | all four | all four |

## Performance budget

No separate line. Event generation is already in `NoiseFloor`. Covered by the cable line of section 9 (0.005 units per cable with hum on); the microphonic source adds no per-sample arithmetic beyond the existing source.

## Test plan

- **CB-06 (microphonic).** For each tier, measured cable-movement RMS equals v1 times the section 2 scale within 0.5 dB. Coiled adds 3 dB within 0.5 dB. Unmodelled produces zero events over 60 s.
- Added: `cableGainLin` is called only from the compile path (checked with the probe from file 04).

## Effort and dependencies

- ED 1.
- Depends on: `noise-floor.md` (additive change: tier scale and coiled input); `character-wear.md` 1 (seed); `cables-and-brands.md` 2 (tier table); file 03 (state field); file 05 (`tier` field).
