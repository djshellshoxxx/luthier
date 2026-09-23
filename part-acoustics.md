# PART ACOUSTICS SPEC

Every field of every part, and exactly what it does to the engine.

`guitar-workshop.md` says a guitar is a bill of parts. This file is the
other half: the function from part fields to engine coefficients. Per
`CLAUDE_CODE_BRIEF.md`'s conflict list item 6, **this file overrides on
part-to-engine mappings**. If a panel or another spec implies a different
effect, this one is right.

It exists so that no realism spec has to invent its own materials table,
and so that "physical deltas only" (`INDEX.md`) is enforceable: there is
one place where a physical field becomes a DSP number, and a reviewer can
check it.

## 0. Ground rules

1. **One function, evaluated once per swap.** `mapSpec(GuitarSpec) →
   DerivedAcoustics` (`guitar-workshop.md` 3.2). Nothing reads raw part
   fields from the audio path.
2. **Physical units in, DSP units out.** Inputs are kilograms, millimetres,
   henries, newtons. Outputs are coefficients, frequencies and Qs.
3. **Monotonic and continuous.** A small change in a field produces a small
   change in the output. Discontinuities are allowed only where the physics
   has one (a chambered body is not a continuum from a solid one).
4. **Honest magnitudes.** Swapping rosewood for ebony is a real, small
   difference. The mapping produces a real, small difference, and
   `workshop-ui.md`'s spectrum delta shows how small.
5. **Every constant is named and sourced.** A magic number with no comment
   saying where it came from is a bug in this file.

## 1. Wood

The one table the rest of the file leans on. Values are along-grain, at
12% moisture content, from standard timber references.

| Wood | Density kg/m³ | E‖ GPa | Damping (tanδ ×10⁻³) | Character |
|---|---|---|---|---|
| Alder | 420 | 9.5 | 8.5 | Balanced, scooped mids |
| Ash (swamp) | 480 | 11.0 | 7.5 | Bright, open, ringing |
| Ash (northern) | 680 | 13.0 | 6.5 | Hard, bright, heavy |
| Basswood | 420 | 9.0 | 11.0 | Soft, midrange, damped |
| Mahogany (Honduran) | 550 | 10.5 | 9.0 | Warm, strong low mids |
| Mahogany (African) | 530 | 9.8 | 9.5 | Slightly softer |
| Maple (hard) | 705 | 12.6 | 6.0 | Bright, tight, sustaining |
| Maple (soft) | 545 | 10.0 | 7.5 | Between maple and alder |
| Korina | 480 | 10.0 | 8.5 | Mahogany-like, more top |
| Poplar | 455 | 10.9 | 10.0 | Neutral, cheap, fine |
| Walnut | 610 | 11.5 | 7.0 | Dark and tight |
| Rosewood (Indian) | 830 | 12.0 | 6.0 | Dense, complex overtones |
| Ebony | 1040 | 16.0 | 4.5 | Hardest, brightest attack |
| Pau ferro | 860 | 13.5 | 5.5 | Between rosewood and ebony |
| Spruce (Sitka) | 400 | 11.0 | 7.0 | Acoustic top standard |
| Cedar (western red) | 350 | 8.0 | 9.0 | Softer acoustic top |
| Koa | 610 | 10.5 | 8.0 | Acoustic, midrange |

### 1.1 What density and damping do

- **Body / top density** sets the body modes' frequencies:
  `f_mode ∝ sqrt(E / ρ) × (thickness / area)`.
- **Damping** sets each mode's Q: `Q ≈ 1 / (2 × tanδ)` before geometric
  losses. Ebony at 4.5e-3 gives Q≈111; basswood at 11e-3 gives Q≈45.
- **Neck and fretboard density** feed the neck's contribution to the
  coupling matrix and to dead-spot placement (`character-wear.md` 2).

## 2. Body

| Field | Engine effect |
|---|---|
| `wood` | Table 1: density, E, damping |
| `density_kg_m3` | Overrides the table if the user set it |
| `thickness_mm` | Mode frequency ∝ thickness for a plate; mass ∝ thickness |
| `area_cm2` | Mode frequency ∝ 1/area; air volume for hollow bodies |
| `chambering` | Mode count and air resonance, below |
| `bracing` | Acoustic only: mode splitting pattern |

### 2.1 Chambering

| Value | Modes | Air resonance | Sustain | Feedback |
|---|---|---|---|---|
| `solid` | 3 weak plate modes | none | longest | lowest |
| `chambered` | 5 modes, +3 dB | 180-240 Hz, Q 8 | -5% | low |
| `semi_hollow` | 7 modes, +6 dB | 140-190 Hz, Q 12 | -12% | medium |
| `hollow` | 9 modes, +10 dB | 90-140 Hz, Q 18 | -20% | high |
| `acoustic` | 12 modes, +14 dB | 90-110 Hz, Q 20 | -25% | n/a |

Air resonance frequency scales as `1/sqrt(V)` with body volume, so a big
jazz box resonates lower than a thinline. Feedback coupling feeds
`ambiguity-resolutions.md` 1's feedback path gain.

## 3. Neck and fretboard

| Field | Engine effect |
|---|---|
| `scale_length_mm` | String tension for a given pitch and gauge: `T = (2 L f)² μ`. Sets brightness, feel and fret spacing. |
| `wood`, `density` | Neck mass; dead-spot frequency (`character-wear.md` 2) |
| `profile` | Mass distribution only; no tonal claim. A C versus a V is a feel difference and the model says so rather than inventing one. |
| `joint` | Coupling strength to body: bolt 0.55, set 0.80, through 0.95 |
| `fretboard.wood` | Damping at the fretted termination: harder wood, brighter attack |
| `fretboard.radius_mm` | Geometry for `fret-buzz.md` clearance across the board |

Scale length is the single most audible neck field: 628 mm (24.75") versus
648 mm (25.5") at the same pitch and gauge is about 10% tension
difference, which is why a Strat sounds tighter than a Les Paul on the
same strings.

## 4. Frets and nut

| Field | Engine effect |
|---|---|
| `frets.material` | Termination brightness: nickel-silver 0.70, stainless 0.90, gold-evo 0.80, brass 0.60. Also `fret-buzz.md`'s buzz spectrum. |
| `frets.height_mm` | `fret-buzz.md` 1 clearance; taller frets raise buzz level |
| `frets.width_mm` | Contact area; wider is slightly duller |
| `frets.count` | Playable range |
| `nut.material` | **Open strings only**: bone 0.75, brass 0.85, graphite 0.70, plastic 0.60, Tusq 0.72. This is why an open string sounds different from the same note fretted at 12 on the octave-down string. |
| `nut.slot_depths_mm` | `fret-buzz.md` open-string clearance |
| `nut.friction` | Tuning stability under bends (`character-wear.md` 4) |

## 5. Bridge and tailpiece

| Field | Engine effect |
|---|---|
| `bridge.mass_g` | Termination impedance: heavier is less lossy, more sustain, less body coupling |
| `bridge.coupling` | 0-1, how much string energy reaches the body |
| `bridge.type` | Preset mass and coupling defaults |
| `bridge.has_tremolo` | Enables `WhammyEngine`; `gui-integration.md` 3.1's bridge popover |
| `bridge.tremolo_type` | Spring count and return behaviour |
| `bridge.piezo` | Adds a piezo pickup source at the saddle |
| `tailpiece.mass_g` | Adds to termination mass |
| `tailpiece.break_angle_deg` | Downforce: steeper is tighter and brighter |

| Bridge type | Mass g | Coupling | Note |
|---|---|---|---|
| Tune-o-matic + stopbar | 95 | 0.55 | Sustain-oriented |
| Hardtail (strings through) | 110 | 0.70 | Most body coupling |
| Vintage tremolo (6-screw) | 165 | 0.45 | Spring losses |
| Two-point tremolo | 150 | 0.48 | |
| Floyd Rose | 320 | 0.30 | Heavy, locked, least body |
| Bigsby | 480 | 0.35 | Very heavy, short sustain |
| Acoustic pin bridge | 28 | 0.92 | Almost everything reaches the top |
| Resonator spider | 45 | 0.88 | Into the cone, not the top |

## 6. Pickups

| Field | Engine effect |
|---|---|
| `inductance_h` | With cable and pot load, sets resonant peak (`volume-knob-interaction.md` 1) |
| `dc_resistance_k` | Series resistance; damps the resonance |
| `capacitance_pf` | Self-capacitance, parallel with cable |
| `magnet` | Pull strength → string damping, plus a small nonlinearity |
| `coil_turns` | Output level ∝ turns; also raises inductance |
| `pole_piece_material` | Eddy losses: steel dulls, alnico is neutral, ceramic is brightest |
| `cover` | Nickel cover: -0.8 dB at 4 kHz from eddy currents. Real and famously argued about. |
| `position_mm` | **Aperture and comb filtering.** The dominant field. |
| `height_*_mm` | Output level and magnetic damping |

### 6.1 Position

A pickup senses the string over a finite width at a point. Position from
the bridge produces the comb filter that is most of a pickup's character:
nulls at `f = n × v / (2 × position)`. A bridge pickup at 38 mm nulls
much higher than a neck pickup at 152 mm, which is the entire reason they
sound different.

### 6.2 Magnet pull

| Magnet | Pull | Damping | Character |
|---|---|---|---|
| Alnico 2 | 0.55 | 0.020 | Soft, warm, loose lows |
| Alnico 3 | 0.45 | 0.016 | Weakest pull, most open |
| Alnico 4 | 0.65 | 0.024 | Balanced |
| Alnico 5 | 0.80 | 0.032 | Tight, scooped, standard |
| Alnico 8 | 0.95 | 0.040 | Hot and aggressive |
| Ceramic | 1.00 | 0.045 | Brightest, strongest, most damping |
| Neodymium | 1.20 | 0.055 | Very hot; can choke sustain |

**Magnet pull damps the string.** A pickup raised too close to a strong
magnet shortens sustain and pulls the pitch flat - "Stratitis" - and the
model reproduces it from `height_mm` and this table without a special
case. That is the best single demonstration that the parts model is doing
physics rather than presets.

## 7. Wiring

Maps directly onto `volume-knob-interaction.md`'s components:
`volume_pot_ohm` → `Rvol`, `tone_pot_ohm` → `Rtone`, `tone_cap_f` →
`Ctone`, `taper` → the wiper law, `treble_bleed` → `Rbleed`/`Cbleed`,
`active` → the buffer.

`switching` selects the pickup-combination topology (3-way, 5-way,
independent volumes, series/parallel, coil tap) and is the only wiring
field that is not a component value.

## 8. Strings

| Field | Engine effect |
|---|---|
| `gauges_in[]` | Mass per length μ; with scale length gives tension |
| `winding` | round / flat / half / coated → `string-squeak.md` 4 |
| `winding_material` | Squeak spectrum and brightness (`string-squeak.md` 4) |
| `core` | Round core is warmer and more flexible; hex is brighter and stiffer |
| `winding_pitch_per_mm[]` | `string-squeak.md` 1's squeak fundamental; `pick-noise.md` 4's chirp |
| `tension_kg[]` | Computed, not stored, unless overridden |

`μ` for a wound string is computed from core and winding geometry rather
than from gauge alone, because two 0.046" strings with different cores
have different masses and different tensions at pitch.

**Inharmonicity** rises with stiffness: `B ∝ d⁴ E / (T L²)`. A heavy
plain third is noticeably inharmonic and this is why it sounds sour on
chords - a real effect the model should produce.

## 9. Pickguard, hardware, finish

| Field | Engine effect |
|---|---|
| `pickguard.mass_g` | Small damping term on the top; audible on acoustics and thinlines, negligible on a solidbody |
| `hardware_color` | **None.** Visual only, and this file says so explicitly so nobody adds one. |
| `finish.gloss` | Thick poly damps a top slightly: up to -0.5 dB and Q -8% on acoustic modes. Nitro and satin are negligible. |
| `finish.aging` | Feeds `character-wear.md`'s body break-in |

## 10. Composition rules

When several parts affect one engine value:

- **Masses add.** Bridge + tailpiece + pickguard is one termination mass.
- **Couplings multiply.** Neck joint × bridge coupling is the fraction of
  string energy reaching the body.
- **Dampings add in the loss domain**, not the Q domain: `1/Q_total =
  Σ 1/Q_i`.
- **The string's own losses dominate.** Part effects modify a string model
  that already loses most of its energy internally, which is why the
  effects are small - and why exaggerating them to make the Workshop feel
  responsive would be dishonest.

## 11. Tests

- **Every field moves something.** For each part type and each numeric
  field, perturb by 10% and assert the rendered spectrum changes by more
  than the noise floor. A field that fails this is a field that should not
  exist (ground rule 3 of `guitar-workshop.md`).
- **Monotonicity.** For density, mass, thickness, inductance and position:
  sweep across the range and assert the mapped output is monotonic.
- **Scale length changes tension correctly.** 628 mm versus 648 mm at the
  same pitch and gauge produces a tension ratio within 1% of `(648/628)²`.
- **Magnet pull shortens sustain.** A ceramic pickup at 1.5 mm produces at
  least 15% shorter 60 dB decay than alnico 3 at 3.5 mm, and a measurable
  flat pitch pull.
- **Pickup position sets the comb.** The first null for a pickup at 38 mm
  is within 5% of the theoretical `v / (2 × 0.038)`.
- **Cover costs top end.** A nickel cover produces -0.8 ± 0.2 dB at 4 kHz.
- **Chambering raises air resonance correctly.** Each chambering value
  produces an air mode inside its table row's range.
- **Hardware colour is silent.** Changing it produces bit-identical audio.
- **Composition rules hold.** Two parts each halving coupling produce a
  quarter, not a half.
- **Mapping runs once per swap**, measured, not per block.
- **Determinism.** The same `GuitarSpec` produces byte-identical
  `DerivedAcoustics` across runs and platforms.
