# Acoustic-electric and stage v2: design index

Source: `spec/roadmap/v2/acoustic-electric-and-stage-v2.md`. Ordered by ascending ED;
ties by roadmap section order.

| ID | Title | ED | Status | File |
|---|---|---|---|---|
| 3.3 (STG-01) | Body-tap tooltip and body-dependence test | 1 | exists (tooltip only) | [01-body-tap-tooltip.md](01-body-tap-tooltip.md) |
| 3.9 (STG-13) | Tuner stage mute (mute to audience) | 3 | missing (tuner not in code) | [02-tuner-stage-mute.md](02-tuner-stage-mute.md) |
| 3.7 (STG-11) | DI capture and re-amp from file | 4 | partial (re-amp unverified) | [03-di-capture-reamp.md](03-di-capture-reamp.md) |
| 3.1 (STG-02, 03) | Piezo, mic and magnetic blend | 6 | partial | [04-piezo-mic-magnetic-blend.md](04-piezo-mic-magnetic-blend.md) |
| 3.2 (STG-04, 05, 06) | Soundhole feedback coupling | 6 | partial | [05-soundhole-feedback.md](05-soundhole-feedback.md) |
| 3.4 (STG-07, 08) | Speaker excursion limit | 6 | partial | [06-speaker-excursion-limit.md](06-speaker-excursion-limit.md) |
| 3.6 (STG-10) | Double-track: stored take and replay | 6 | partial | [07-double-track-takes.md](07-double-track-takes.md) |
| 3.8 (STG-12) | Drum practice trainer | 6 | partial | [08-drum-practice-trainer.md](08-drum-practice-trainer.md) |
| 3.5 (STG-09) | Stereo pair and two-amp rigs | 10 | partial (two-amp missing) | [09-stereo-rig.md](09-stereo-rig.md) |

Feature ED total: 48. Area overhead (roadmap 9): parameters and state block 4,
STG-14 and RT checks 3. **Area total: 55 ED (about 11 weeks).**

## Area-wide rules

- New parameters go in `// ==== BEGIN V2-STAGE params ====` / `// ==== END V2-STAGE params ====`.
  The block does not exist yet in `Source/`. No `ac_*`, `sh_feedback`, `speaker_breakup`,
  `rig_mode`, `dt_take_offset_ms` or `tuner_mute_audience` IDs exist (checked by grep).
- Preset data goes in an optional `stage` block. A preset without it loads v1 behaviour.
- Free/Pro gates per roadmap 6 and `spec/editions.md`.
- Everything on the audio thread is preallocated in `prepareToPlay`, double precision,
  and cleared by `reset()`. Tuner, file I/O and pitch tracking run off the audio thread.
- Stage safety: anything audible to an audience (tuner, click, trainer) has a mute and
  defaults away from the main output.

## Evidence corrections to the roadmap

These were found by checking `Source/` while writing. Each affected file records its own fix.

1. **Piezo/mic blend exists.** `PickupEngine.h` line 194 `setPiezoMicBlend` (3.1).
2. **Helmholtz exists.** `Source/Model/Guitar/BodyModels.h` line 133 (3.2). The
   formula is a check, and STG-04 tests against it.
3. **Re-amp not found.** No `re-amp`/`reamp` symbol in `Source/` (3.7). Status is
   "to verify, else build".
4. **Dual-IR option not found.** `spec/engine.md` 13.3 is cited for stereo; no dual-IR
   symbol in `Source/` (3.5).
5. **Drum trainer section number.** The ED table says 3.9; the trainer is 3.8 (3.8).
6. **STG-03 direction.** The alignment delays the piezo path, not the mic (3.1).
7. **STG-10 vs humanize.** Full `humanize` jitter would break the 0.5 ms tolerance; the
   take's jitter is bounded to +-0.4 ms (3.6).
8. **speaker_breakup default.** The APVTS default is 0, not 0.3; 0.3 is the new-preset
   template value (3.4).
9. **Excursion formula.** `V/(2 pi f)` is replaced by a mass-controlled `1/f^2`, bounded
   below `f_s` (3.4).
10. **STG-09 "-3 dB".** True only for uncorrelated channels; replaced by a Mono-mode
    fold-down reference (3.5).
11. **Two-amp has no test ID.** STG-09b is proposed (3.5).
12. **Re-amp route status.** Roadmap 1 lists re-amp as existing; unverified (see 3).
13. **STG-12 count-in.** Four bars is kept as stated; it is long for a typical practice
    count-in and may be changed.

## Open questions (from roadmap 10, with the decision taken here)

1. Alignment slider stays, tagged D (file 04).
2. Use `BodyEngine` air mode first; add a Helmholtz term only if STG-05 fails (file 05).
3. `X_max` and knee are D until sourced (file 06).
4. Stereo pair in v2.0; two-amp follows the board budget (file 09).
5. Tuner owner is `tuner-and-tuning-reference.md`; the mute is an addition (file 02).
