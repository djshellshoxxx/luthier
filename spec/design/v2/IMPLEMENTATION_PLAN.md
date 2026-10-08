# Luthier v2 — Implementation Plan

Detailed design specs for every unimplemented ("to-do") v2 feature now live
under `spec/design/v2/<area>/`, one file per feature, each numbered by
ascending effort within its area, with a per-area `INDEX.md`. This plan
sequences them across all areas.

Source roadmap: `docs/ROADMAP_V2.md` (scope, milestones). Area specs:
`spec/roadmap/v2/*.md`. Effort is engineer-days (ED), one engineer with an AI
pair; totals match the roadmap (313 ED).

**Do not start building until the v1.0.0-beta1 release build is complete.**
This plan is design + sequencing only.

## Effort by area

| Area | Files | ED | Dir |
|---|---|---|---|
| Cables & brands | 9 | 35 | `spec/design/v2/cables/` |
| Setup & ergonomics | 7 | 37 | `spec/design/v2/setup/` |
| Techniques | 11 | 50 | `spec/design/v2/techniques/` |
| Acoustic/electric & stage | 9 | 55 | `spec/design/v2/acoustic-stage/` |
| Instruments | 6 | 67 | `spec/design/v2/instruments/` |
| Pedalboard | 18 | 69 | `spec/design/v2/pedalboard/` |
| **Total** | **60** | **313** | |

## A. Lowest-effort-first index (pure ED order)

The features the user asked for first — cheapest independent wins. ED ascending;
ties grouped. Each row → its spec file.

### Tier 0 (≤1 ED) — trivial, ship first
| ED | ID | Feature | File |
|---|---|---|---|
| 0 | TQ 3.10 | Whole-strum dynamics | `techniques/01-whole-strum-dynamics.md` |
| 0.5 | PB-07 | Latency defect (bypass must not change latency) | `pedalboard/01-latency-defect-pb07.md` |
| 0.5 | — | Pedalboard source layout/seams | `pedalboard/02-files-and-seams.md` |
| 1 | PB-P01 | Split-level parameters | `pedalboard/03-split-level-params.md` |
| 1 | CB-06 | Microphonic scale + coiled flag | `cables/01-microphonic-scale-and-coiled-flag.md` |
| 1 | STG-01 | Body-tap tooltip + body-dependence | `acoustic-stage/01-body-tap-tooltip.md` |

### Tier 1 (2 ED)
| ED | ID | Feature | File |
|---|---|---|---|
| 2 | TQ 3.8 | Push and pull | `techniques/02-push-pull.md` |
| 2 | CB-08 | True-bypass pop | `cables/02-true-bypass-pop.md` |
| 2 | IN-07 | Seven- and eight-string | `instruments/01-seven-eight-string.md` |
| 2 | PB (UI) | Cable popover | `pedalboard/04-cable-popover.md` |
| 2 | PB-E01 | Edition gating (Free vs Pro) | `pedalboard/05-edition-gating.md` |

### Tier 2 (3 ED)
| ED | ID | Feature | File |
|---|---|---|---|
| 2.5 | PB-U02 | Easy-mode board strip | `pedalboard/06-easy-mode.md` |
| 3 | TQ 3.7 | Arm vs wrist strokes | `techniques/03-arm-wrist-strokes.md` |
| 3 | TQ 3.9 | Fret-hand damping between phrases | `techniques/04-fret-hand-damping.md` |
| 3 | TQ 3.11 | Mistakes (late/wrong notes) | `techniques/05-mistakes.md` |
| 3 | SU-04/05 | String gauge + bend hold | `setup/01-gauge-bend-hold.md` |
| 3 | SU-06 | Saddle material | `setup/02-saddle-material.md` |
| 3 | CB-E01/M01 | Cable state/migration/Free rules | `cables/03-state-migration-free-rules.md` |
| 3 | CB-13 | Cable RT checks + tests | `cables/04-rt-checks-and-tests.md` |
| 3 | STG-13 | Tuner stage mute | `acoustic-stage/02-tuner-stage-mute.md` |
| 3 | PB-03 | Mono/stereo routing | `pedalboard/07-mono-stereo.md` |
| 3 | PB-00 | New pedal types (Buffer, Ground Isolator, Preamp) | `pedalboard/08-pedal-types.md` |
| 3 | PB (UI) | Pedal face | `pedalboard/09-pedal-face.md` |
| 3 | PB-10/11 | Instance binding + footswitches | `pedalboard/10-footswitches-instances.md` |

### Tier 3 (4 ED)
| ED | ID | Feature | File |
|---|---|---|---|
| 4 | TQ 3.2 | Whammy pre-bend | `techniques/06-whammy-prebend.md` |
| 4 | TQ 3.3 | Volume swell | `techniques/07-volume-swell.md` |
| 4 | TQ 3.6 | Thumb-over bass | `techniques/08-thumb-over.md` |
| 4 | SU-08 | Finger reach | `setup/03-finger-reach.md` |
| 4 | CB-01/02 | Cable tiers + resonance model | `cables/05-tiers-cable-model-resonance.md` |
| 4 | CB-07 | Buffer + Ground Isolator DSP | `cables/06-buffer-and-ground-isolator.md` |
| 4 | CB-U01 | Cable/pedal tier popovers | `cables/07-popovers.md` |
| 4 | STG-11 | DI capture + re-amp from file | `acoustic-stage/03-di-capture-reamp.md` |
| 4 | IN-08/09 | Fretless-bass glide | `instruments/02-fretless-glide.md` |
| 4 | PB-U01 | Drag/drop/keyboard editing | `pedalboard/11-drag-drop-keyboard.md` |

### Tier 4 (5 ED)
| ED | ID | Feature | File |
|---|---|---|---|
| 5 | TQ 3.1 | Tapped harmonic | `techniques/09-tapped-harmonic.md` |
| 5 | TQ 3.4 | Hybrid/chicken picking | `techniques/10-hybrid-chicken.md` |
| 5 | SU-03 | String-change wizard | `setup/04-string-change-wizard.md` |
| 5 | SU-09/10 | Partial capo | `setup/05-partial-capo.md` |
| 5 | PB-08/09 | Bypass modes (true/buffered) + pop | `pedalboard/12-bypass-modes.md` |
| 5 | PB-M01/02/03 | Board block, file format, v1 migration | `pedalboard/13-board-block-migration.md` |

### Tier 5 (6 ED)
| ED | ID | Feature | File |
|---|---|---|---|
| 6 | TQ 3.5 | Sweep picking | `techniques/11-sweep.md` |
| 6 | SU-01/02 | Intonation compensation | `setup/06-intonation-compensation.md` |
| 6 | SU-07 | Left-handed mode | `setup/07-left-handed-mode.md` |
| 6 | CB-03/04/05/12 | Hum, ground count, power mode | `cables/08-hum-ground-count-power.md` |
| 6 | STG-02/03 | Piezo/mic/magnetic blend | `acoustic-stage/04-piezo-mic-magnetic-blend.md` |
| 6 | STG-04/05/06 | Soundhole feedback coupling | `acoustic-stage/05-soundhole-feedback.md` |
| 6 | STG-07/08 | Speaker excursion limit | `acoustic-stage/06-speaker-excursion-limit.md` |
| 6 | STG-10 | Double-track take/replay | `acoustic-stage/07-double-track-takes.md` |
| 6 | STG-12 | Drum practice trainer | `acoustic-stage/08-drum-practice-trainer.md` |
| 6 | PB-01/02/13 | Board graph + validation | `pedalboard/14-board-graph.md` |
| 6 | PB-12 | In-loop effects: AmpEngine split | `pedalboard/15-in-loop-amp-split.md` |

### Tier 6 (7–8 ED)
| ED | ID | Feature | File |
|---|---|---|---|
| 7 | PB-04/05/06 | Compile, swap, retire | `pedalboard/16-compile-swap.md` |
| 7.5 | PB (UI) | Advanced canvas + library drawer | `pedalboard/17-canvas-library.md` |
| 8 | PB (test) | Board shared test harness | `pedalboard/18-test-harness.md` |
| 8 | CB-09/10/11 | Pedal tiers: voicing, noise, loading | `cables/09-pedal-tiers.md` |

### Tier 7 (10+ ED) — heaviest, do last
| ED | ID | Feature | File |
|---|---|---|---|
| 10 | STG-09 | Stereo pair + two-amp rigs | `acoustic-stage/09-stereo-rig.md` |
| 10 | IN-01/02 | Lap steel + dobro/resonator | `instruments/03-lap-steel-resonator.md` |
| 10 | IN-10/11 | Flamenco golpe + rasgueado | `instruments/04-flamenco-golpe-rasgueado.md` |
| 12 | IN-03/04 | Ukulele | `instruments/05-ukulele.md` |
| 14 | IN-05/06 | Mandolin | `instruments/06-mandolin.md` |

## B. Dependency-aware build sequence (what effort order cannot override)

Pure ED order (section A) is the priority list, but hard dependencies force
some order. The real build sequence follows `docs/ROADMAP_V2.md` milestones:

1. **PB-07 latency fix** — zero deps, ships as a v1.x patch before v2 starts.
2. **Pedalboard core** (`13-board-block-migration`, `14-board-graph`,
   `16-compile-swap`) — everything on the board needs it; do before any board
   UI/tiers even though some board UI items are cheaper.
3. **Cable/pedal tiers, buffer, ground isolator, true-bypass pop** — need the
   board core for cable/pedal state.
4. **Board UI** (canvas, library, popovers, pedal face, keyboard) — needs core + tiers.
5. **AmpEngine split for in-loop** (`15-in-loop-amp-split`) — isolated (touches amp tests).
6. **Setup items** — independent; can run alongside 2–4.
7. **Techniques** — tapped-harmonic depends on `harmonic-realism.md`; sweep +
   arm/wrist on rhythm/strum; mistakes on humanize; rest independent.
8. **Instruments** — lap steel/dobro reuse `SlideEngine`; golpe needs
   `BodyEngine`; 7/8-string multi-scale depends on `extended-range-bass.md` 1.1
   (out of this roadmap — schedule separately or ship single-scale).
9. **Acoustic & stage** — stereo pair needs board stereo last stage; two-amp
   needs board budget; tuner owned by `tuner-and-tuning-reference.md`.

Milestones (from the roadmap): **M1** board foundation + setup core + first
techniques (70 ED) · **M2** board you can play + cable/pedal tiers (68) · **M3**
feel & setup + remaining techniques + lap steel/dobro (69) · **M4** instruments
& stage (106).

## C. Open issues to resolve before implementation

Flagged by the spec pass (detail in the named files):

1. **CB-07** as written is physically wrong — a Buffer *after* a 10 m cable
   does not isolate that cable's capacitance from the pickup. Reword to
   "10 m Economy *after* a Buffer." (`cables/06-buffer-and-ground-isolator.md`)
2. **v1 render compatibility (cables):** `cable_quality = Vintage` gaining the
   +9 dB microphonic penalty changes existing v1 renders, conflicting with
   ground-rule 4 / CB-M01 — needs a per-preset mapping version.
   (`cables/01`, `cables/08` — input-cable hum needs G=1 on v1 boards.)
3. **Free loads a Pro board** is unspecified. (`cables/03-state-migration-free-rules.md`)
4. **Free pedal-count inconsistency** (`editions.md` 2.2: 15 of 22 vs 23 types)
   — settle before M2 (gates PB-E01, CB-E01).
5. **Measured ("D") values** — pop, shield, excursion, pickup inductance,
   bend-hold exponent must be measured or shipped with a "design value" label.
6. **Multi-scale dependency** (7/8-string fanning) sits outside this roadmap
   (`extended-range-bass.md` 1.1) — schedule first or ship single-scale.
7. New parameters across all areas must stay **append-only**; the roadmap
   targets a param count of 216 added across v2 with
   `everyAutomatableParameterHasAVisibleControl` still passing.
