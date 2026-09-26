# INSTRUMENT EXTENSION SPECS

Seven instruments realised inside the existing parts model
(`guitar-workshop.md`, `part-acoustics.md`, `guitar-illustration.md`).
Research with sources and data gaps lives in
`docs/research/INSTR_<name>.md`; each spec here cites it and never
invents a number the research file does not carry or derive.

Status: **spec only**. No DSP in this pass.

| Spec | Research | Family | New code needed |
|---|---|---|---|
| `chapman-stick.md` | `INSTR_chapman_stick.md` | `touch` (new) | Yes: family, zone model, `TapEngine`, split stereo |
| `guitarron.md` | `INSTR_guitarron.md` | `bass` | Small: convex-back body shape, octave-pair gesture |
| `chitarra-sarda.md` | `INSTR_chitarra_sarda.md` | `acoustic` | Data only (parts + presets) |
| `composite-neck.md` | `INSTR_composite_neck_bass.md` | any (neck material) | Small: non-wood neck materials in `lookUpWood` + neck mobility |
| `tenor-guitar.md` | `INSTR_tenor_guitar.md` | `acoustic` | Tuning presets only |
| `acoustic-bass-guitar.md` | `INSTR_acoustic_bass_guitar.md` | `bass` | Body shape row + radiation high-pass |
| `extended-range-bass.md` | `INSTR_composite_neck_bass.md` §ext | `bass` | Tuning presets, true multi-scale neck fields |

## 0. Ground rules (in addition to `part-acoustics.md` 0)

1. **Measured beats inferred beats invented.** Every numeric field in a
   factory part here is tagged in the spec as `M` (measured, cited in the
   research file), `I` (inferred by a stated formula from measured data)
   or `D` (design default, no data; flagged in "Data gaps"). A `D` value
   must be replaced when owner-sourced measurements arrive and the spec's
   test fixtures are then re-baselined.
2. **No new part slots unless the instrument cannot be expressed
   otherwise.** Six of the seven need none. The Stick needs zone
   metadata, which is a *field on the neck and strings parts*, not a slot.
3. **Reference-style names only** (`factory-content.md` 0.1). "Touch
   Board 10", not a trademark.

## 1. Shared model extensions

These are used by more than one instrument spec. They are defined once
here; the instrument specs reference this section.

### 1.1 Neck: true multi-scale fields

`Source/` today has a factory `Multi-Scale` neck part with a single
`scale_length_mm` (698.5); no fan is modelled (verified by grep for
`fan`/`multi` in `Source/Model` and `Source/DSP`: only the classical
fan *brace* exists). Extended-range bass and the 8-string need:

| Field | Type | Default | Engine effect |
|---|---|---|---|
| `scale_length_mm` | mm | — | Treble-side (string 0) scale, as today |
| `scale_length_bass_mm` | mm | = `scale_length_mm` | Bass-side (last string) scale |
| `neutral_fret` | 0–24 | 7 | Fret perpendicular to the centre line (illustration only) |

Per-string scale: `L_i = L_treble + (L_bass − L_treble) × i / (N − 1)`
(linear fan, which is how every fanned-fret maker lays it out). `L_i`
replaces the single scale in `T = (2 L f)² μ` and in the fret-position
table per string. Absent field ⇒ bit-identical to today.

### 1.2 Neck: non-wood materials

`lookUpWood` (`Source/Model/Workshop/PartAcoustics.cpp:10`) already
holds `steel` as a non-wood row. Add rows (values and sources in
`INSTR_composite_neck_bass.md`):

| id | ρ kg/m³ | E‖ GPa | tanδ | Tag |
|---|---|---|---|---|
| `cfrp_ud` (unidirectional carbon/epoxy neck) | see research | | | M/I |
| `cfrp_hybrid` (wood core + carbon rods / skins) | rule-of-mixtures, see `composite-neck.md` 2 | | | I |
| `polycarbonate` (Stick option) | see `INSTR_chapman_stick.md` | | | M |
| `bamboo_laminate` (Stick option) | see `INSTR_chapman_stick.md` | | | M/I |

The neck's material today feeds only mass and dead-spot placement
(`part-acoustics.md` 3). `composite-neck.md` adds the one missing
physical path: **neck-mobility-driven string damping at the nut/fret
termination**, which is what a dead spot *is* (Fleischer), and which is
where stiff, low-loss necks differ audibly.

### 1.3 New family `touch`

`guitar-illustration.md` 18 says new families need code. The Stick needs
one: `touch`, default strings 10, default scale 34" (864 mm), default
excitation `tap`. Family switch rules (`guitar-illustration.md` 12) add a
`touch_default_template.luthierguitar`. All other instruments here reuse
`bass` or `acoustic`.

### 1.4 Tuning presets to add to `TuningEngine`

`Source/Model/Playing/TuningEngine.h:33` has 17 presets; 4- and 5-string
bass only. Each instrument spec lists the rows it adds; the union is in
`extended-range-bass.md` 3 and the per-instrument specs. `kMaxStrings`
is 12 (`Source/DSP/Common/DspCommon.h:21`), which fits every instrument
here including the 12-string touch board.

## 2. Build order

1. `extended-range-bass.md` (tuning rows + multi-scale fields; smallest,
   unblocks 8-string correctness too)
2. `composite-neck.md` (materials + neck mobility)
3. `tenor-guitar.md`, `chitarra-sarda.md` (data only)
4. `acoustic-bass-guitar.md`, `guitarron.md` (body rows)
5. `chapman-stick.md` (largest; depends on 1.1–1.3 and `two-hand-tapping.md`)
