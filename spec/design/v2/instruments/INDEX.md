# Instruments V2: design index

Source: `spec/roadmap/v2/instruments-v2.md`. Ordered by ascending ED. Status is
from the roadmap's 3.1 table.

| ID | Title | ED | Status | File |
|---|---|---|---|---|
| IN-07 (3.5) | Seven- and eight-string | 2 | exists (single scale); multi-scale spec only | [01-seven-eight-string.md](01-seven-eight-string.md) |
| IN-08, IN-09 (3.6) | Fretless bass glide | 4 | partial | [02-fretless-glide.md](02-fretless-glide.md) |
| IN-01, IN-02 (3.2) | Lap steel and dobro/resonator | 10 | partial | [03-lap-steel-resonator.md](03-lap-steel-resonator.md) |
| IN-10, IN-11 (3.7) | Flamenco golpe and rasgueado | 10 | partial | [04-flamenco-golpe-rasgueado.md](04-flamenco-golpe-rasgueado.md) |
| IN-03, IN-04 (3.3) | Ukulele | 12 | missing | [05-ukulele.md](05-ukulele.md) |
| IN-05, IN-06 (3.4) | Mandolin | 14 | missing | [06-mandolin.md](06-mandolin.md) |
| IN-12, IN-13, IN-14 | Cross-cutting: compat, visible controls, fallback | 4 | missing (shared suite) | this file |
| (roadmap 9) | Research files: lap steel, ukulele, mandolin, flamenco | 6 | missing | this file |
| (roadmap 9) | Tests IN-01 to IN-14 harness | 5 | missing | this file |

**Feature ED total: 52. Shared ED: 15. Area total: 67 ED (matches roadmap 9).**

Shared scope (IN-12, IN-13, IN-14), referenced from each feature's test plan:
- **IN-12:** every existing guitar and bass preset renders bit-identically with all
  new parameters at defaults. Migration defaults in each file (for example, fretless
  glide off on load) make this true.
- **IN-13:** `everyAutomatableParameterHasAVisibleControl` passes for the automatable
  IDs.
- **IN-14:** a preset naming an absent instrument loads the nearest instrument and
  shows a notice naming the original.

External dependency (not in this roadmap, not counted): multi-scale 7/8-string,
`extended-range-bass.md` 1.1 (file 01).

## Corrections to the roadmap

| # | Where | Issue | Resolution in these specs |
|---|---|---|---|
| C1 | 4, 2 | `seven_low_b`, `eight_low_fsharp` duplicate existing presets (`TuningEngine.cpp` 61-62, 527-528) | Not appended; the existing preset is the control (file 01). New-ID count 10 becomes 8 |
| C2 | 3.6, 4 | Glide default 90 changes existing fretless presets, against IN-12 | Load default 0 (off) for old presets; new presets default 90 (file 02) |
| C3 | 2, 3.7, 4 | Rasgueado speed "per string" vs "total" | Total first-to-last, 20-120 ms (file 04) |
| C4 | 3.7 vs 4 | Golpe keyswitch: `bodyTap` 17 or 24 | 24 (file 04) |
| C5 | 2 | Dobro "cone on/off" has no parameter ID | Proposed `dobro_cone` bool, default on (file 03). New-ID count becomes 9 with C1 |
| C6 | 3.4 | Mandolin "reentrant" is wrong; it is standard G3 D4 A4 E5 | Corrected (file 06) |
| C7 | 3.4 | "Octave-up" pairs; octave mandolins are usually an octave down | Roadmap wording kept; direction flagged (file 06) |
| C8 | IN-05 | "Beats at 3 cents" is not a rate | Test uses `f*(2^(c/1200)-1)` (file 06) |
| C9 | 3.2 | Lap tooltip "frets are damped" contradicts U1 "no fret-hand damping" | Tooltip: "No fretting hand: the bar is the only contact" (file 03) |
| C10 | 3.2 | "Add a `lap` family" vs ground rule 3 | Recommend no new family; decide in research (file 03) |
| C11 | 3.3 | Baritone scale not given | About 480-500 mm (D), confirm in research (file 05) |
| C12 | 6 | Dobro Pro while the mode and body exist | Verify against `Source/Edition.h`; keep Free if already Free (file 03) |

## Open items for the owner

1. Confirm C1: drop the two tuning IDs and use the existing presets.
2. Confirm C5 (`dobro_cone`) and C10 (no lap family).
3. Confirm C7: octave-course direction.
4. Live trigger for rasgueado: event-class mapping, or a keyswitch (file 04).
5. Research files: `docs/research/INSTR_lap_steel.md`, `INSTR_ukulele.md`,
   `INSTR_mandolin.md`, `INSTR_flamenco.md` (none exists; all D values wait on them).
   Roadmap open question 5 (who supplies measured values) is still open.
6. Confirm the 7-string base tuning and the mandolin body values with the research
   files.
