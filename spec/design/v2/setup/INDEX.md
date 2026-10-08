# Setup and ergonomics v2: design index

Source: `spec/roadmap/v2/setup-and-ergonomics-v2.md`. Ordered by ED ascending; ties by roadmap section. Total 37 ED (roadmap section 9).

| ID | Title | ED | Status | File |
|---|---|---|---|---|
| 3.3 / SU-04, SU-05 | Gauge and bend hold | 3 | partial | [01-gauge-bend-hold.md](01-gauge-bend-hold.md) |
| 3.4 / SU-06 | Saddle material | 3 | partial | [02-saddle-material.md](02-saddle-material.md) |
| 3.6 / SU-08 | Finger reach | 4 | partial | [03-finger-reach.md](03-finger-reach.md) |
| 3.2 / SU-03 | String-change wizard | 5 | partial | [04-string-change-wizard.md](04-string-change-wizard.md) |
| 3.7 / SU-09, SU-10 | Partial capo | 5 | partial | [05-partial-capo.md](05-partial-capo.md) |
| 3.1 / SU-01, SU-02 | Intonation compensation | 6 | partial | [06-intonation-compensation.md](06-intonation-compensation.md) |
| 3.5 / SU-07 | Left-handed mode | 6 | missing | [07-left-handed-mode.md](07-left-handed-mode.md) |

Cross-cutting, not a feature file:

| ID | Title | ED | Status | Where |
|---|---|---|---|---|
| 5 / SU-M01 | Setup block, file format, migration | 3 | missing | this file, below |
| 8 / SU-11 | UI polish and keyboard path | 2 | partial | this file, below |

Sum: 3+3+4+5+5+6+6 = 32 (features) + 3 + 2 = **37**.

## Shared setup block

One structural block, saved with the instrument's setup and, as an optional `setup` block, in presets. A preset without it loads defaults. Version field `setup_version = 1`. Not host-automatable.

| Field | Type | Range | Default | Owner file |
|---|---|---|---|---|
| `saddle` | string enum | bone, brass, graphite, tusq, steel | bone | 02 |
| `intonation_comp[1..6]` | float, mm | -4 to +4 | 0 | 06 |
| `capo_mask` | 6-bit int | 0 to 63 | 63 | 05 |
| `handedness` | enum | right, left | right | 07 |
| `string_label_order` | enum | mirror, left_strung | mirror | 07 |
| `reach_mm` | float, mm | 80 to 150 | 110 | 03 |
| `reach_set` | bool | | false | 03 |

Not saved: `stretch_timer_min` (04, UI state, 0 to 120, default 30).

Parameters: no new automatable parameter. Existing IDs unchanged (`setup_action_treble`, `setup_action_bass`, `setup_relief`, `capo_fret`, `pick_thickness`, `pick_angle`, `intonation_error`, `string_gauge`, `string_age`). `everyAutomatableParameterHasAVisibleControl` is unaffected.

Migration: v1 presets load with the defaults above. Bit-identical renders (SU-M01): every factory preset renders identically with the block at defaults. Shared with PB-M01.

Edition: setup stays Free, except saddle material (Pro beyond bone). Per `editions.md`.

SU-11 (keyboard path): every new control is in the setup panel and reachable by keyboard. Manual check against `accessibility.md`. Owner: this file.

## Errata against the roadmap

Found while checking the roadmap against source and the formulas. Each is corrected in the feature file named.

1. **Intonation sign (06, SU-02).** Lengthening a string flattens it. The roadmap says +10.6 ct sharp for +2 mm. Correct value: -10.65 ct (flat).
2. **Gauge factor for 9-42 (01, SU-04).** The roadmap gives 1.2. The computed value from d^2 is 1.10 to 1.11. 11-49 (0.90) matches. SU-04 band set to [1.08, 1.14].
3. **Reach values (03, SU-08).** The formula gives 3, 4 and 6 frets at p = 1, 5 and 12 for R = 110 mm on 648 mm. The roadmap says 4, 5 and 8. Fret spacing figures in the roadmap are also off (fret 1 to 2 is 34.3 mm, not 36).
4. **Hand span status (03, 3.6).** A player setting already exists: `RhythmEngine` `handSpan` (3 to 7 frets, default 5), with a slider in `RhythmPanel`. The roadmap says "no player setting". The new mm reach must not change rhythm output, so v1 behaviour is kept while `reach_set` is false.
5. **Partial capo status (05, 3.7).** `TuningEngine` already has the per-string mask and `getCapoFretFor`, and `StringEngine::setStoppedFret` exists. The `Part.h` mask field in the roadmap is not needed. The work is UI, persistence and tests.
6. **Intonation status (06, 3.1).** A per-string intonation slope exists (`TuningEngine.h` 76, 112). The new work is the saddle length offset.
7. **Thread safety of the capo mask (05).** `setCapoStringMask` stores a plain `uint32`. Make it atomic or precompute if the audio thread reads it.
