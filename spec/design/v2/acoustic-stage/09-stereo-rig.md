# 09 Stereo rigs: stereo pair and two-amp

STG-09. Source roadmap: 3.5.

**Summary.** Three rig modes: Mono (current), **Stereo pair** (one amp, stereo
cabinet placement), and **Two-amp** (L and R each with a full amp and cabinet, Pro).

**Status.** Partial.
- Stereo outputs exist (`spec/routing-io.md` 1-3).
- Stereo mic placement exists: `CabinetEngine.h` line 107 `setStereoWidth`
  (two mics in the stereo field).
- The amp is mono: `AmpEngine.h` line 133 "Mono in, mono out".
- The dual-IR option cited in the roadmap (`spec/engine.md` 13.3) was not found in
  `Source/` (search for `dual`/`DualIR`). Stereo pair uses `setStereoWidth`, not dual IR.
- Two-amp: missing.

## User-facing behaviour
- Rig card: `rig_mode` choice: Mono, Stereo pair, Two-amp (Pro).
- Stereo pair: one amp, output split by the cabinet's stereo placement, width set by
  the existing control.
- Two-amp: two identical amp and cabinet chains (same settings on both sides in v2).
  Per-side amp settings are out of v2 scope (open question 4).

## Engine / DSP
- **Stereo pair.** Mono amp output -> `CabinetEngine` stereo placement -> L/R. No new
  DSP; the change is routing and the rig-mode switch.
- **Two-amp.** Board stereo output (`spec/pedalboard-v2.md` 3.4) feeds two
  `AmpEngine` instances (L, R), each with its own supply state (sag), each with its
  own `CabinetEngine`. Instances are created in `prepareToPlay`; switching modes
  crossfades 20 ms, double precision.
- Cost: two-amp is about 2x the amp budget, charged against the board budget
  (`spec/pedalboard-v2.md` 7).
- RT: all instances and buffers allocated in `prepareToPlay`; no allocation on mode
  switch; `reset()` resets both amps' state.

## Data model and parameters
- `rig_mode`: choice (mono, stereo pair, two-amp), default mono, **not automatable**,
  visible on the Rig card.

## State / file format / migration
- Stored in the `stage` block. Missing: mono.
- A two-amp preset loaded on a Free build plays as a stereo pair (noted in UI).

## Edition gating
- Free: Mono and Stereo pair (roadmap 6). Pro: Two-amp.

## Performance budget
- Stereo pair: 0.1 units (second cabinet and mic path) (roadmap 7).
- Two-amp: about 2x amp budget (`spec/performance-budget.md` 1, amp entries), charged
  to the board budget.

## Test plan
- **STG-09.** Mono source, mics 20 cm apart (`setStereoWidth` set to match).
  Assert correlation of L and R below 0.9.
  Fold-down: `(L+R)/2` RMS within 0.5 dB of the Mono-mode render at the same
  settings with the same centre mic. (Roadmap wording "-3 dB" is true only for
  uncorrelated channels; replaced by this reference.)
- **STG-09b (proposed; no ID in roadmap).** Two-amp with identical settings and
  mono input: L and R are equal within 0.1 dB and the board budget charge matches
  about 2x the amp entry. The roadmap has no test for two-amp; this adds it.
- **STG-14 (partial).** No allocation on rig-mode switch.

## Effort and dependencies
- **ED 10** (roadmap; one file for both modes).
- Depends on: `spec/pedalboard-v2.md` 3.4 (stereo last stage, a hard prerequisite
  for stereo output) and 7 (board budget, a prerequisite for two-amp);
  `spec/engine.md` 10-13; `spec/routing-io.md` (stereo outputs).
- Open question 4 (roadmap 10): ship stereo pair in v2.0; two-amp follows if the
  board budget allows.
