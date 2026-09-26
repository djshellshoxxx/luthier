# INSTRUMENT EXTENSION SPECS

Seven instruments realised inside the existing parts model
(`guitar-workshop.md`, `part-acoustics.md`, `guitar-illustration.md`).
Research with sources and data gaps lives in
`docs/research/INSTR_<name>.md`; each spec here cites it and never
invents a number the research file does not carry or derive.

Status: **spec only**. No DSP in this pass.

| Spec | Research | Family | New code needed |
|---|---|---|---|
| `chapman-stick.md` | `INSTR_chapman_stick.md` | `touch` (new) | Yes: family, `ZoneRouter`, physical tap excitation, per-zone pickup masks, stereo zone wiring, beam modes |
| `guitarron.md` | `INSTR_guitarron.md` | `bass` | Small: convex-back body shape, octave-pair gesture |
| `chitarra-sarda.md` | `INSTR_chitarra_sarda.md` | `acoustic` | Data only (parts + presets) |
| `composite-neck.md` | `INSTR_composite_neck_bass.md` | any (neck material) | Small: non-wood neck materials in `lookUpWood` + neck mobility |
| `tenor-guitar.md` | `INSTR_tenor_guitar.md` | `acoustic` | Tuning presets only |
| `acoustic-bass-guitar.md` | `INSTR_acoustic_bass_guitar.md` | `bass` | Body shape row + radiation high-pass |
| `extended-range-bass.md` | `INSTR_composite_neck_bass.md` §3–4 | `bass` | Tuning presets, 6-string bug, multi-scale in audio |

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

**Finding.** The *renderer* already draws fans:
`Source/UI/Guitar/GuitarRenderer.cpp:488-532` treats a neck as fanned
when the guitar is tagged `fanned` or the neck is named "multi-scale",
takes `scale_length_mm` as the **bass (longest) side**, reads an
optional `scale_length_treble_mm` (default `scale − 38`), and makes
fret 7 perpendicular. The *audio* ignores all of it: `mapSpec` uses one
`scale_length_mm` for every string (`PartAcoustics.cpp:350`). So the
factory 8-String Modern is drawn fanned 27"→28" but every string is
modelled at 698.5 mm - a breach of `guitar-illustration.md` 0.5
("the illustration is authoritative to the ear").

**Change.** Formalise the renderer's fields as part fields and consume
them in `mapSpec`:

| Field | Type | Default | Engine effect |
|---|---|---|---|
| `scale_length_mm` | mm | — | Bass-side (last string) scale, as the renderer already reads it |
| `scale_length_treble_mm` | mm | = `scale_length_mm` | Treble-side (string 0) scale |
| `perpendicular_fret` | 0–24 | 7 | Renderer only (already hard-coded 7) |

Per-string scale, linear fan (how fanned boards are laid out):
`L_i = L_treble + (L_bass − L_treble) × i / (N − 1)`, string 0 treble.
`L_i` replaces the single scale in `T = (2 L f)² μ`, in the fret-position
table and in `B ∝ d⁴E/(T L²)` per string. Field absent ⇒ bit-identical
to today, **except** the renderer's implicit `scale − 38` default for
necks named "multi-scale", which must become explicit in those part
files so audio and picture agree (`extended-range-bass.md` 4).

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

### 1.4 Tuning: a guitar-level `tuning` field (required by 5 of 7)

**Finding.** A parts guitar has no tuning of its own. `mapSpec` takes
`d.spec.tuning` from the compiled base type chosen by `baseTypeFor`
(`Source/Model/Workshop/PartAcoustics.cpp:100`) and reads at most six
preset frequencies (`PartAcoustics.cpp:478`); strings past six are
"each a fourth below the last". Consequences today:

- a 4-string `acoustic` (tenor) resolves to `Dreadnought` ⇒ 6-string
  E-standard tension maths on a 4-string neck;
- a 6-string `bass` resolves to `FiveStringBass`, whose preset has 5
  entries, so string index 5 reads an unset slot of `open[]` (0 Hz ⇒
  tension 0) - a latent bug, tested for in `extended-range-bass.md` 6;
- a re-entrant tuning (guitarrón A2 on string 1, Stick melody/bass
  zones) cannot be expressed at all.

**Change.** `.luthierguitar` gains an optional top-level object:

```json
"tuning": {
  "name": "Guitarrón standard",
  "open_hz": [110.0, 164.814, 130.813, 97.999, 73.416, 55.0],
  "string_order": "high_to_low_index"
}
```

- `open_hz[i]` is string `i` using the engine's convention (string 0 is
  the first entry in `TuningEngine::kPresets`; re-entrant tunings are
  legal, so "string 0 = highest-pitched" becomes "string 0 = the
  treble-side string").
- Length must equal the guitar's string count; mismatch ⇒ fall back to
  the base type's preset and raise the `missing part`-style notice
  (`error-recovery.md`).
- When present it (a) feeds `mapSpec`'s tension/inharmonicity loop
  instead of `getPresetFrequencies`, and (b) is pushed to `TuningEngine`
  as a `Custom` preset on load. Absent ⇒ bit-identical to today.
- `file-formats.md` 3 gains the field; schema version unchanged (it is
  optional and additive).

New named presets for `TuningEngine` (menu shortcuts; the guitar file is
authoritative) are listed in each instrument spec. `kMaxStrings` is 12
(`Source/DSP/Common/DspCommon.h:21`), which fits every instrument here
including the 12-string touch board.

## 2. Build order

1. `extended-range-bass.md` (tuning rows + multi-scale fields; smallest,
   unblocks 8-string correctness too)
2. `composite-neck.md` (materials + neck mobility)
3. `tenor-guitar.md`, `chitarra-sarda.md` (data only)
4. `acoustic-bass-guitar.md`, `guitarron.md` (body rows)
5. `chapman-stick.md` (largest; depends on 1.1–1.3 and `two-hand-tapping.md`)
