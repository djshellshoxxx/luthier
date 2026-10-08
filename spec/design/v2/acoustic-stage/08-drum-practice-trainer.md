# 08 Drum practice trainer

STG-12. Source roadmap: 3.8 (the ED table labels it 3.9; 3.9 is the tuner).

**Summary.** A trainer mode in the practice panel: an accented 8th or 16th pattern,
a count-in, a subdivision click and a tempo ramp. It reuses the metronome's timing.

**Status.** Partial.
- Metronome exists: `Source/Practice/Metronome.{h,cpp}`; panel `Source/UI/PracticePanel.h`.
- Jam drum kit exists: `Source/Jam/JamEngine.h`.
- Missing: the trainer scheduler, pattern editor and count-in.

## User-facing behaviour
- Trainer mode in the Practice panel (`spec/practice-tools.md` 4 layout, not a new panel).
- Pattern: one bar of 8ths or 16ths, each step with an accent level (off, normal,
  accent).
- Count-in: 1 to 4 bars (default 1). STG-12 uses 4 as stated in the roadmap.
- Tempo ramp: start BPM, end BPM, bars per step.
- Click output: **Phones** (default) or **Main**. Stage rule: the trainer never reaches
  the main output unless the player sends it there (`spec/live-performance.md`).

## Engine / DSP
- Scheduler on the audio thread, sample-accurate, no drift:
  `samplesPerStep = sr * 60 / (bpm * subdiv)` in double;
  event `k` at `round(start + k * samplesPerStep)`.
- Tempo changes apply at bar boundaries only.
- Click voice: synthesised (short decaying sine + noise burst), preallocated.
  Accent is a level, not a new voice.
- Pattern type reused from the rhythm engine (`spec/rhythm-engine.md`).
- RT: all buffers in `prepareToPlay`; no allocation; `reset()` stops and rewinds.

## Data model and parameters
- No new automatable parameters. Trainer settings are practice-tool state, stored
  in the practice data location (`spec/practice-tools.md` 10), not in presets.

## State / file format / migration
- Practice settings file only. Nothing in the preset.

## Edition gating
- **Pro** (roadmap 6). The Metronome stays Free.

## Performance budget
- 0.01 units (scheduler; no voices beyond the click) (roadmap 7).

## Test plan
- **STG-12.** Pattern: 16ths, 120 BPM, count-in 4 bars. Event onsets after the
  count-in fire every 125 ms within 1 ms. Count-in onsets fire every 500 ms (quarter)
  within 1 ms.
- Assert the click reaches Phones and not Main by default.
- **STG-14 (partial).** No allocation during a tempo ramp.

## Effort and dependencies
- **ED 6.**
- Depends on: `spec/practice-tools.md` (1 metronome, 4 layout, 10 data location);
  `spec/rhythm-engine.md` (pattern type); `spec/live-performance.md` (stage safety).
