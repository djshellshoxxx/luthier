# Luthier v2.0 to-do list

The v2.0 to-do list: every item in the six v2 specs under
`spec/roadmap/v2/`, with its estimate, dependency order, and four milestones
from 2027-03-01. It covers scope and sequencing only; each spec owns its own
physics, parameters and tests. The v1.x roadmap is in `docs/ROADMAP.md`.

Estimates are engineer-days (ED) for one engineer working with an AI pair.
A week is 5 ED of focused work. Each spec's section 9 holds the breakdown;
this file sums those numbers and does not re-estimate.

## Totals

| Area | Spec | ED | Weeks |
|---|---|---|---|
| Pedalboard | `spec/roadmap/v2/pedalboard-v2.md` | 69 | 14 |
| Cables and brands | `spec/roadmap/v2/cables-and-brands.md` | 35 | 7 |
| Techniques | `spec/roadmap/v2/techniques-v2.md` | 50 | 10 |
| Instruments | `spec/roadmap/v2/instruments-v2.md` | 67 | 13 |
| Setup and ergonomics | `spec/roadmap/v2/setup-and-ergonomics-v2.md` | 37 | 7.5 |
| Acoustic, electric and stage | `spec/roadmap/v2/acoustic-electric-and-stage-v2.md` | 55 | 11 |
| **Total** | | **313** | **about 63** |

Calendar: 63 focused weeks fit in the 70 weeks from 2027-03-01 to 2028-06-30,
with about 10 % left for review, measurement and slips. Measurement is a
real cost here: many values in the specs are tagged D and must be measured
before they are final (see each spec's open questions).

## Dependency order

The order below is the order in which work can start. Items on one line may
run in parallel.

1. **Latency defect (PB-07).** `EffectsChain::getLatencySamples()` counts only
   non-bypassed pedals. The fix is one function and a regression test. It has
   no dependency and can ship as a v1.x patch before v2 starts.
2. **Pedalboard core**: `BoardModel`, `BoardCompiler`, `BoardProgram`, legacy
   adapter, migration and bit-identity (PB-M01). Everything in the board needs it.
3. **Cable and pedal tiers, buffer, ground isolator, true-bypass pop**
   (`cables-and-brands.md`). Needs the board core for cable and pedal state.
   The buffered bypass in the board needs the buffer model.
4. **Board UI**: canvas, library, popovers, pedal face, keyboard path. Needs
   the core and tiers.
5. **AmpEngine split for in-loop** (`pedalboard-v2.md` 3.4). Needed only for
   in-loop placement (Pro, M3). Touches amp tests, so it is isolated.
6. **Setup items** (`setup-and-ergonomics-v2.md`): intonation, handedness,
   reach, the setup block. Independent of the board; can run alongside 2-4.
7. **Techniques** (`techniques-v2.md`). Tapped harmonic depends on
   `harmonic-realism.md`. Sweep and arm/wrist depend on the rhythm and strum
   code. Mistakes depends on humanize. The rest are independent.
8. **Instruments** (`instruments-v2.md`). Lap steel and dobro use existing
   `SlideEngine` modes. Golpe depends on `BodyEngine` and the body-tap voice.
   7- and 8-string multi-scale depends on `extended-range-bass.md` 1.1, which
   is not in this roadmap and must be scheduled first if multi-scale is wanted
   in v2.0.
9. **Acoustic and stage** (`acoustic-electric-and-stage-v2.md`). The stereo
   pair needs the board's stereo last stage (item 2). The two-amp rig needs the
   board budget. The tuner is owned by `tuner-and-tuning-reference.md`; the
   stage mute is added to that build, not built twice.

## Milestones

| Milestone | Target | ED | Scope |
|---|---|---|---|
| **v2.0-M1** Board foundation | **2027-06-11** | 70 | Pedalboard core, migration, latency fix; intonation, handedness, reach, setup block; tapped harmonic, whammy pre-bend; instrument research files |
| **v2.0-M2** The board you can play | **2027-09-17** | 68 | Board UI and keyboard path; AmpEngine split; cable and pedal tiers, hum, ground loop, buffer, ground isolator, true-bypass pop; PB suite |
| **v2.0-M3** Feel and setup | **2028-01-14** | 69 | Remaining techniques (sweep, swell, thumb-over, arm/wrist, push/pull, damping, mistakes, snaps); string-change wizard, gauge hold, saddle material, partial capo; lap steel and dobro |
| **v2.0-M4** Instruments and stage | **2028-06-30** | 106 | Ukulele, mandolin, 7/8-string tuning, fretless glide, golpe and rasgueado; soundhole feedback, blend, speaker excursion, stereo pair, double-track, DI capture and re-amp, drum trainer, tuner stage mute |

Milestone ED sums to 313, matching the totals table.

### Done when

**M1 is done when:**

- PB-07 passes: reported latency is the same at every bypass toggle in a
  24-pedal board. The fix has shipped (as a patch, or in this milestone).
- PB-M01 and PB-M02 pass on every factory preset: migrated boards render
  bit-identically, and v1 presets round-trip byte-identical.
- PB-04, PB-05 and PB-10 pass: deterministic compile, no audio-thread
  allocation, instance identity across moves.
- SU-01, SU-02, SU-07, SU-10 pass (intonation, handedness, full-capo compatibility).
- TQ-01, TQ-02, TQ-03, TQ-13 pass (tapped harmonic, pre-bend, compatibility).
- Parameter count is 216 and `everyAutomatableParameterHasAVisibleControl` passes.

**M2 is done when:**

- PB-01, PB-02, PB-03, PB-06, PB-08, PB-09, PB-11, PB-12, PB-13 pass.
- PB-U01 and PB-U02 pass as manual checks on Windows, macOS and Linux
  Standalone: every drag has a keyboard path, and the Easy summary shows.
- CB-01 to CB-13 pass, including the hum table (CB-03), ground count (CB-05)
  and pedal-tier noise (CB-11).
- CB-U01 passes: the popovers read correctly at 200 % UI scale.
- The Pedalboard and cable D values have a measured replacement, or the
  open-question entry says why it is still D.

**M3 is done when:**

- TQ-04 to TQ-12 and TQ-14, TQ-15 pass.
- SU-03 to SU-06, SU-08, SU-09 pass, and SU-11 passes as a keyboard check.
- IN-01, IN-02 and IN-08 to IN-09 pass (lap steel, dobro, fretless glide,
  no fret buzz).
- Lap steel and dobro presets ship as Pro, with research files in
  `docs/research/`.

**M4 is done when:**

- IN-03 to IN-07, IN-10 to IN-14 pass (ukulele, mandolin, 7/8-string, golpe,
  rasgueado, fallback notice).
- STG-01 to STG-14 pass: blend, soundhole coupling, excursion, stereo pair,
  double-track, DI capture and re-amp, drum trainer, tuner stage mute, and the
  RT stress run.
- Every spec's open questions are closed, or carried into v2.1 with a note.
- Full-plugin pass: `Steady state` stays at or under 8 units and `Heavy preset`
  at or under 22 units (`spec/performance-budget.md`) on the factory set.

## Risks

- **AmpEngine split** (in-loop, 6 ED) touches the amp's tests and the
  sag/standby path. It is isolated in M2 so that a slip does not hold the
  board UI.
- **Measured values.** Pop, shield, excursion, pickup inductance and bend-hold
  exponent are D in their specs. If measurement slips, ship the D value with a
  "design value" label, not a claimed measurement.
- **Multi-scale dependency** (7/8-string fanning) sits outside this list. If
  `extended-range-bass.md` 1.1 is not in v2.0, the 7/8-string items ship on a
  single scale, as the instruments spec already allows.
- **Board budget.** Two-amp rigs and 24-pedal boards can exceed the 6-unit
  board budget. The UI warning exists; the stereo pair ships before the
  two-amp rig for this reason.
- **Free and Pro limits.** Each spec gives its Free rules. The Free pedal count
  (`editions.md` 2.2) is inconsistent (15 of 22 vs 23 types). Settle it before
  M2, since the tests PB-E01 and CB-E01 depend on it.
