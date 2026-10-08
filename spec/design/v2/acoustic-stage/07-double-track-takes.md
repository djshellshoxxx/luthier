# 07 Double-track: stored take and replay

STG-10. Source roadmap: 3.6.

**Summary.** The player records one take. The live performance plays on one side,
and the stored take replays on the other, with its own timing offset, level offset
and pan. It replaces the `Doubler` on that side only.

**Status.** Partial. `Doubler` pedal exists (`Source/DSP/Effects/PedalsMod.h`
and `Pedal.{h,cpp}`, `PedalType::Doubler`): one performance copied, panned, offset.
Missing: the take store, the replay voice, and the controls.

## User-facing behaviour
- Double-track card: **Record take** (stores the next performance's note stream),
  **Play take** (replays it on the other side), **Clear take**.
- Offset slider `dt_take_offset_ms` (10-30 ms, default 18).
- Pan: L or R (state, not a parameter).
- Level offset: fixed -1.5 dB (D, not a control in v2).

## Engine / DSP
- Recorder captures the performance's note stream (note on/off, velocity, string,
  time in seconds from take start). Preallocated buffer, 4096 events max per take;
  a full buffer stops recording and shows a notice.
- Replay: at each stored event time `t_e`, schedule
  `t_r = t_e + dt_take_offset_ms + j_e`, where `j_e` is the seeded timing jitter from
  `humanize` (`spec/rhythm-engine.md` 135), bounded to +-0.4 ms (D). Bounding the
  jitter is required by STG-10 (see Test plan).
- Replayed notes drive one extra voice on the take side (the take side's
  `Doubler` is bypassed while double-track is on).
- Scheduling is sample-accurate: event sample = round(t_r * sr), with `t_r` in double.
- RT: event buffer preallocated; replay cursor is an index; no allocation.
  `reset()` rewinds the cursor and stops held notes.

## Data model and parameters
- `dt_take_offset_ms`: 10..30, default 18, automatable (roadmap parameter table).
- Take pan and mode are state (`stage` block), not parameters.

## State / file format / migration
- Takes are stored as note events in the preset's `stage` block (roadmap 5), in the
  same encoding as the preset. Not audio.
- Event size is small; a 4096-event cap bounds the block to a few hundred kB at most
  (D).
- Missing block: no take, Doubler behaviour unchanged.

## Edition gating
- **Pro.** Free keeps `Doubler` as the single-performance option. Take controls show
  a Pro badge in Free.

## Performance budget
- 0.02 units: note events plus one voice (roadmap 7).

## Test plan
- **STG-10.** Record a take, then replay with `dt_take_offset_ms` = 18. Assert the
  onset difference between the live note and the replayed note is 18 ms within
  0.5 ms (jitter limit of +-0.4 ms is inside that margin). Assert the level
  difference between the two sides equals the -1.5 dB offset within 0.1 dB.
  Test runs with a fixed jitter seed.
- **STG-14 (partial).** No allocation during record and replay.

## Effort and dependencies
- **ED 6.**
- Depends on: `spec/rhythm-engine.md` 135 (`humanize`, seeded jitter);
  `spec/file-formats.md` (stage block); the performance note stream in the engine.
- Conflict resolved: the roadmap says replayed notes use the full `humanize` jitter.
  STG-10 requires a 0.5 ms onset tolerance, so the take's jitter is bounded to
  +-0.4 ms here.

## Open questions
- Whether a take should also store a re-pitch (capo) setting. Out of v2 scope.
