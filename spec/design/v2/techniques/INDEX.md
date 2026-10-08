# Techniques v2: design specs index

Source: `spec/roadmap/v2/techniques-v2.md`. Ordered by ascending ED (engineer-days, one engineer with an AI pair).

| ID | Title | ED | Status | File |
|---|---|---|---|---|
| 3.10 | Whole-strum dynamics | 0 (in 3.7) | partial | [01-whole-strum-dynamics.md](01-whole-strum-dynamics.md) |
| 3.8 | Push and pull | 2 | missing | [02-push-pull.md](02-push-pull.md) |
| 3.7 | Arm versus wrist strokes | 3 | missing | [03-arm-wrist-strokes.md](03-arm-wrist-strokes.md) |
| 3.9 | Fret-hand damping between phrases | 3 | partial | [04-fret-hand-damping.md](04-fret-hand-damping.md) |
| 3.11 | Mistakes (late, wrong) | 3 | partial | [05-mistakes.md](05-mistakes.md) |
| 3.2 | Whammy bar pre-bend | 4 | partial | [06-whammy-prebend.md](06-whammy-prebend.md) |
| 3.3 | Volume swell | 4 | missing | [07-volume-swell.md](07-volume-swell.md) |
| 3.6 | Thumb-over bass | 4 | missing | [08-thumb-over.md](08-thumb-over.md) |
| 3.1 | Tapped harmonic | 5 | partial | [09-tapped-harmonic.md](09-tapped-harmonic.md) |
| 3.4 | Hybrid picking and chicken pickin' | 5 | partial | [10-hybrid-chicken.md](10-hybrid-chicken.md) |
| 3.5 | Sweep picking | 6 | missing | [11-sweep.md](11-sweep.md) |

Feature ED sums to 39. With cross-cutting parameters (5) and tests (6) the area total is 50 ED, matching roadmap section 9.

Dependency order: 02 needs rhythm only. 03, 04, 05 need rhythm, strum or humanize. 05 uses the `PhraseBoundaries` helper from 04, so build 04 first or let 05 own the helper. 09 is blocked on `spec/harmonic-realism.md`. 10 and 08 need `spec/fingerstyle-attack.md`. 11 needs the chord voicer and strum scheduler.

## Cross-cutting

**Parameters (V2-TECH block, roadmap section 4, plus one proposal).** 11 automatable: `bar_prebend_depth`, `bar_prebend_time`, `vswell_rise`, `chicken_snap_ms`, `chicken_mute`, `sweep_speed_ms`, `thumbover_weight`, `push_pull_ms`, `ftdamp_gap_ms`, `mistake_late_pct`, `mistake_wrong_pct`. Structural: `vswell_hold`, `sweep_direction`, `thumbover_string`, `strum_stroke_source`, `ftdamp_mode`. Proposed addition: `hybrid_split` (structural, 1-6, default 4). Keyswitches: 22 tapped harmonic, 23 bar pre-bend (roadmap); 24 sweep (proposed in file 11).

**Technique enum additions (appended):** `TappedHarmonic` (09), `Sweep` (11).

**Seeded streams.** Each draws from the character seed with its own salt, so streams do not share state:

| Stream | File | Salt |
|---|---|---|
| Mixed stroke draw | 03 | `0x5A17` |
| Mistake draws (late, wrong) | 05 | `0x0B57` |
| Swell drive noise | 07 | `0x5E11` |
| Tap burst noise | 09 | `0x7A9D` |
| Chicken snap noise | 10 | `0xC1C4` |

Sweep, thumb-over, pre-bend and damping are deterministic and draw nothing.

**TQ-13 (compat).** A v1 riff and preset render bit-identically with all new parameters at defaults. Two rules make this hold: "mixed" stroke source is the v1 path (03), and `hybrid_split` applies only under hybrid style (10), with v1 hybrid presets loading split 1.

**TQ-14 (seed).** The roadmap says different seeds change "only the mistake and snap-noise streams". The list is incomplete: the swell drive (07), tap burst (09) and mixed stroke draw (03) are seeded too. Amend TQ-14 to list all five streams.

**TQ-15 (visible controls).** `everyAutomatableParameterHasAVisibleControl` passes for the 11 automatable IDs above. Structural IDs are checked by the same test as not automatable.

**Proposed test location.** One file, `Source/Tests/TechniqueV2Tests.cpp`, for TQ-01 to TQ-12 and TQ-15, plus the existing compat suite for TQ-13 and TQ-14.

## Roadmap corrections found while writing

1. User stories U1, U2 and U3 cite the wrong sections: U1 (swell) is 3.3, not 3.6; U2 (sweep) is 3.5, not 3.7; U3 (thumb-over) is 3.6, not 3.8.
2. Swell (07) has an unresolved source model (decision D-1). The roadmap's "not applied to pick-attacked notes unless hold" rule conflicts with U1. Needs a decision before build.
3. TQ-06 and TQ-11 do not give a measurable threshold for "damped". The specs fix one (9 dB in 10 ms; 40 dB within 50 ms of engagement).
4. Section 3.1 says "over an open string" but the formula is 2 x fretted pitch. TQ-01 gets a fretted case (09).
5. Hybrid per-string split has no parameter in section 4. `hybrid_split` is proposed (10).
6. Sweep has no trigger in the keyswitch plan. Keyswitch 24 and `Technique::Sweep` are proposed (11).
7. Mistake wrong-note playability (same string, hand span) is not in the roadmap; added in 05.

## Open decisions (collected)

- D-1 Swell source model (07).
- Mixed stroke p(arm) = 0.3 (03).
- Mode 2 damping target = strings ringing at the boundary (04).
- Damping threshold for TQ-11 (04).
- Pre-bend sign convention (06).
- `chicken_mute` t60 mapping, 150 ms to 20 ms (10).
- Free/Pro for `hybrid_split` and chicken parameters (10).
- Sweep damping of previous strings, four-string minimum, keyswitch 24 (11).
- `tapharm` in MIDI export profile (09).
- Mistakes default off and scale-aware wrong notes (05).
