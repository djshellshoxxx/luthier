# 05 Soundhole feedback coupling

STG-04, STG-05, STG-06. Source roadmap: 3.2.

**Summary.** Feedback through the soundhole: the existing feedback loop's coupling
is weighted by the body's air-mode response, and a new `sh_feedback` control sets how
much of that weighting applies.

**Status.** Partial.
- `Source/DSP/Feedback/FeedbackLoop.{h,cpp}`: `k_couple(s)`, `H_s(f)` peak at each
  note, delay padded to a block (header lines 5-10); a `feedback_amount` already
  multiplies `k_couple` (line 98-100).
- Helmholtz is already computed: `Source/Model/Guitar/BodyModels.h` line 133
  (Helmholtz air resonance, Hz), and the air mode in `BodyEngine`
  (`BodyEngine.cpp` ~266, `isAir` in `BodyModels.h` 89). The roadmap's formula is
  therefore a check, not a new model.
- Missing: any soundhole-specific coupling and the `sh_feedback` control.

## User-facing behaviour
- Feedback card: "Acoustic feedback" slider `sh_feedback` (0-1, default 0).
- Off by default on every preset. The player turns it on.
- In Free, the slider caps at 0.3 (the value above it is kept in the preset but
  not applied, and the UI says "Pro: full range").

## Engine / DSP
Air-mode factor for each string `s` at its frequency `f_s`, using the body's air
mode `(f_a, Q_a)` from `BodyEngine` (the modal state, not the formula):
```
r   = f_s / f_a
A(f_s) = 1 / ( Q_a * sqrt( (1 - r^2)^2 + (r / Q_a)^2 ) )      A(f_a) = 1
```
Effective coupling (sh_feedback = 0 is an exact multiply by 1.0):
```
k_eff(s) = k_couple(s) * ( (1 - sh) + sh * A(f_s) )
```
- `sh = sh_feedback` (after the Free cap).
- `A(f_s)` is computed when a string's pitch changes (per note, not per sample).
  Cost is per string, not per sample.
- The loop is the same loop: `k_eff` replaces `k_couple` in
  `excitation_feedback[s] = k_eff(s) * H_s(f) * amp_out(t - delay)`.
- **Clip guard.** After the feedback sum, a feed-forward gain
  `g = min(1, 0.5012 / |x|)` (ceiling -6 dBFS), with 1 ms release and no attack
  lag, is applied to the feedback return. It bounds the loop for any gain, so
  STG-06 holds even when loop gain exceeds 1 at the air mode.
- RT: delays and per-string factors allocated in `prepareToPlay`; `reset()` clears
  delay lines and the guard. Double precision throughout.

## Data model and parameters
In `// ==== BEGIN V2-STAGE params ====`:
- `sh_feedback`: 0..1, default 0, automatable, visible on the Feedback card.

## State / file format / migration
- Stored in the `stage` block. Missing: 0 (v1 behaviour).
- Existing `feedback_amount` is unchanged.

## Edition gating
- Free: `sh_feedback` capped at 0.3. Pro: full 0..1.

## Performance budget
- Per-string lookup: 0.01 units. The loop's own cost is unchanged (roadmap 7).

## Test plan
- **STG-04.** `BodyModels` Helmholtz for `V=0.0175 m^3, r=0.05 m, t=0.003 m` gives
  123 Hz within 5 Hz; `r=0.04 m` gives 110 Hz within 5 Hz. (Checked against the
  existing function. Formula recomputed in this file: 123.3 Hz and 109.8 Hz.)
- **STG-05.** `sh_feedback` = 0: output bit-identical to the v1 render (same seed).
  `sh_feedback` = 1: a note at `f_a` feeds back more than a note one octave away,
  by at least 6 dB (measured as loop-return RMS over 2 s).
- **STG-06.** `sh_feedback` = 1 for 60 s: output never exceeds -6 dBFS (peak
  check every sample).
- **STG-14 (partial).** No audio-thread allocation over the 60 s run.

## Effort and dependencies
- **ED 6.**
- Depends on: `spec/ambiguity-resolutions.md` 1.2 (`feedback_amount`);
  `spec/engine.md` (body, feedback); `spec/body-coupling.md`;
  `spec/environment.md` 4 (air modes follow speed of sound).
- Open question 2 (roadmap 10): measure whether the `BodyEngine` air mode is
  accurate enough. Build `k_eff` on it first, and add a dedicated Helmholtz term only
  if STG-05 fails.
