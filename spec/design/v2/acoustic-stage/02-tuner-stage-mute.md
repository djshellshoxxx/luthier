# 02 Tuner stage mute (mute to audience)

STG-13. Source roadmap: 3.9. The tuner itself is owned by
`spec/tuner-and-tuning-reference.md`; this file adds only the stage mute to it.

**Summary.** A **Mute to audience** switch on the tuner routes its input monitor to
the headphone bus only, so the main output never carries tuner signal.

**Status.** Missing in code. No `Tuner` class or tuner panel exists in `Source/`
(only tuner-drift and tuning-string symbols in `Character/`, `Parameters.cpp`).
Blocked on the owner spec building the tuner. Stage mute is an addition to that
build, not a second tuner.

## User-facing behaviour
- Switch "Mute to audience" on the tuner panel.
- On by default when the tuner is opened in performance mode; off otherwise.
  Changing the value is allowed in either mode and is remembered.
- While on: tuner monitor audio goes to the headphone bus only. Main out receives
  nothing from the tuner. Tuner display works as normal.

## Engine / DSP
- Tuner input is the capture ring buffer (the `notation-export.md` 6.2 pattern).
  The tuner reads from it on its worker thread; the audio thread only writes.
- Monitor path: one gain pair in the stage bus.
  `g_main = mute ? 0 : 1`, `g_phones = 1`.
  Gains are smoothed with a 20 ms linear ramp in double precision to avoid clicks.
- Pitch tracking runs on the worker thread (0.1 units). Nothing on the audio thread
  except the ring write and two gain multiplies.
- RT: no allocation on the audio thread; ring sized in `prepareToPlay`; `reset()`
  zeroes ring and smoothers.

## Data model and parameters
- `tuner_mute_audience` (bool, **not automatable**, default on in performance mode,
  off in studio). Stored as a UI/stage setting (see State).

## State / file format / migration
- Saved in the preset's optional `stage` block (roadmap 5). Missing block: unmuted
  (v1 behaviour).
- Mode (performance vs studio) is an app setting, not preset state.

## Edition gating
- Free. Stage tools stay in Free (`editions.md` 2).

## Performance budget
- Tuner worker: 0.1 units (owned by the tuner spec). Mute itself: about 0.

## Test plan
- **STG-13.** In performance mode with the tuner open and muted, play a 10 s
  input. Assert the main output equals the same run with the tuner closed,
  bit-identical. Assert the phones bus contains the tuner's monitor signal
  (RMS > -60 dBFS).
- **STG-14 (partial).** No audio-thread allocation during the mute toggle, checked
  in the 60 s stress run.

## Effort and dependencies
- **ED 3** (integration with the owner spec).
- Depends on: `spec/tuner-and-tuning-reference.md` (tuner owner, open question 5);
  `spec/live-performance.md` (stage safety); `spec/routing-io.md` (headphone bus).
