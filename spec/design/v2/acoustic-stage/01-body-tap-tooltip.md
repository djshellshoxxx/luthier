# 01 Body-tap percussion: tooltip and body-dependence test

STG-01. Source roadmap: `spec/roadmap/v2/acoustic-electric-and-stage-v2.md` 3.3.

**Summary.** The body tap already exists. This adds a stage tooltip and a test
that proves the tap's voice follows the body's top modes.

**Status.** Exists. `TechniqueKeyswitch::bodyTap` (= 17, `Source/Model/Playing/TechniqueTriggers.h`
line 51); tap voice in `Source/DSP/Slap/SlapEngine.{h,cpp}`; tests in `Source/Tests/SlapTests.cpp`.

## User-facing behaviour
- Tooltip on the Body Tap slap type: "The body tap uses this guitar's top."
- No new control. The tap sounds different on each body (dreadnought vs parlor) without any setting.

## Engine / DSP
- No DSP change. The tap voice is already driven by the body model's modes.
- RT: unchanged (no new allocation or state).

## Data model and parameters
- None. No new IDs.

## State / file format / migration
- None.

## Edition gating
- Free (no gate).

## Performance budget
- 0 units.

## Test plan
- **STG-01.** Render a 1 s body tap on two `BodyModels` entries (different top
  thickness/mode sets). Window: Hann, 8192-point FFT, 1 Hz bins.
  Measure `E_mode` = peak energy within +-2 bins of the top's lowest mode, and
  `E_ref` = median energy 200-2000 Hz. Compute `R_tap = 10 log10(E_mode/E_ref)`.
  Compute `R_body` the same way from the body's own modal transfer function at the
  same frequencies. Assert `|R_tap - R_body| <= 3 dB` for both bodies, and
  `R_tap(A) != R_tap(B)` by at least 3 dB.
  (Spec decision: the roadmap did not define the measure; this definition is the
  one used.)

## Effort and dependencies
- **ED 1.**
- Depends on: `spec/string-slap-technique.md` 30-35 (Body Tap type); `BodyModels`.

## Status of open items
- None.
