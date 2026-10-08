# New pedal types: Buffer, Ground Isolator, Preamp Stage

**IDs:** PB-00 (audit of all types). **ED:** 3.0. **Status:** partial (23 types exist in `Pedal.h`; the three new types are missing).

**Summary.** Append three types after Gater and audit every type's impulse response, mono and stereo.

## User-facing behaviour
- **Buffer** (index 23): a unity buffer. Mix only. Use where a cable must not load the source (`cables-and-brands.md` 3.3).
- **Ground Isolator** (24): Lift off / Lift / Isolate. Models the ground-lift options in `cables-and-brands.md` 3.3.
- **Preamp Stage** (25): an amp-front gain stage from `AmpEngine` 11.2, controls Gain, Bass, Mid, Treble, Channel (Clean, Crunch, Lead). Pre-amp placement.
- All three are Pro (file 05).

## Engine and data model
- `PedalType` gains 23, 24, 25 appended after Gater. Saved slot types keep their meaning (`ambiguity-resolutions.md` 3).
- Buffer: `Pedal` subclass with a unity-gain path and the buffer impedances (1 MOhm in, 1 kOhm out) as coefficients, computed at prepare time.
- Ground Isolator: a mode-selected path; mode is a selector, computed in `prepare`/parameter change, no allocation.
- Preamp Stage: a tube gain stage reusing the `AmpEngine` 11.2 model, oversampled like Overdrive (file 16 counts it as an oversampled type).
- Pedal width declaration (mono/stereo) per type, used by file 07.
- PB-00: a test that runs an impulse through every type, mono and stereo. Copy-L-to-R types are listed in the test's documentation.

## Parameters and data
- Buffer: `Mix`.
- Ground Isolator: `Lift` (Off, Lift, Isolate).
- Preamp Stage: `Gain`, `Bass`, `Mid`, `Treble`, `Channel` (Clean, Crunch, Lead).
- Instance-bound as any pedal (file 10); host-automatable only on instances 0-15.

## State and migration
Appended enum values only. v1 presets have no new types and are unaffected. Old readers see an unknown type and fall back per `file-formats`.

## Edition
Pro only (file 05). Free boards see the types as "Pro" in the library.

## Performance budget (file 14/16 numbers)
Buffer 0.01, Ground Isolator 0.02, Preamp Stage 0.2 (oversampled).

## Tests
- **PB-00.** Impulse through every non-None type (26 after this item), mono and stereo. Output finite, no NaN, no denormal flush issues, peak bounded. Copy-L-to-R types documented in the test.

## Effort and dependencies
ED 3.0. Depends on the `AmpEngine` 11.2 model (existing). Blocks 07 (width) and 05 (gating).
