# EXTENDED-RANGE BASS SPEC (5/6-string, 35", multi-scale)

Research: `docs/research/INSTR_composite_neck_bass.md` §2.1, §3, §4.
Question asked: confirm 5/6-string coverage in `Source/`, spec any gap.

## 1. Coverage audit (verified in `Source/` and `Resources/`)

| Item | Status | Where |
|---|---|---|
| 5-string B0–G2 tuning | **present** | `TuningEngine.cpp:65` `BassFiveString` |
| 5-string compiled type | **present** | `GuitarLibrary.h` `FiveStringBass`; `GuitarLibrary.cpp:354` |
| 5-string factory guitar | **present** | `Resources/Guitars/Bass/Five-String Bass.luthierguitar` |
| 5-string neck / bridge / strings | **present** | `Bass 5-String Neck` (864 mm), `Bass 5-String Bridge`, `45-130 Five-String Nickel` |
| 5-string high-C (E1–C3) | **missing** | no preset |
| 6-string B0–C3 tuning | **missing** | no preset |
| 6-string guitar / neck / bridge / strings | **missing** | none in `Resources` |
| 6-string via parts | **bug** | `baseTypeFor` → `FiveStringBass` (`PartAcoustics.cpp:105`); `getPresetFrequencies` fills 5 of 6 (`TuningEngine.cpp:559`), so string 5's `open[5]` stays **0 Hz** in `mapSpec`'s tension loop (`PartAcoustics.cpp:478-484`) |
| 35" / 36" scale | data only | `scale_length_mm` is free; no factory part |
| Multi-scale (fan) in audio | **missing** | renderer draws fans, audio uses one scale (`README.md` 1.1) |
| `kMaxStrings` | 12 | fine |
| Bass techniques on 5/6 strings | fine | `bass-techniques.md` 1: "nothing here assumes four" |

## 2. Fixes

1. **Guitar-level `tuning`** (`README.md` 1.4) is the general fix; in
   addition, `baseTypeFor` gains `strings >= 6 → SixStringBass` so a
   parts guitar with no `tuning` field still gets six frequencies.
2. New `GuitarType::SixStringBass` compiled entry (copy of
   `FiveStringBass`, `numStrings 6`, tuning `BassSixString`), so the
   compiled fallback (`guitar-workshop.md` 3.1) exists.
3. Multi-scale in audio per `README.md` 1.1.

## 3. Tuning presets (`TuningEngine`, string 0 = treble side)

| Preset | Notes (string 0 →) | Hz | Tag |
|---|---|---|---|
| `BassSixString` | C3 G2 D2 A1 E1 B0 | 130.813, 97.999, 73.416, 55.000, 41.203, 30.868 | M |
| `BassFiveHighC` | C3 G2 D2 A1 E1 | 130.813, 97.999, 73.416, 55.000, 41.203 | M |
| `BassFiveDropA` | G2 D2 A1 E1 A0 | 97.999, 73.416, 55.000, 41.203, 27.500 | D (common alt; not in research) |

## 4. Parts and guitars

| Part | Fields | Tag |
|---|---|---|
| Neck `Bass 6-String Neck` | `maple_hard`, `scale_length_mm: 864`, `strings: 6`, `frets: 24`, `joint: through` | M (34", 24 frets typical) |
| Neck `Bass 5-String 35 Neck` | `scale_length_mm: 889`, `strings: 5` | M (35") |
| Neck `Bass Fanned 5 Neck` | `scale_length_mm: 940` (B side), `scale_length_treble_mm: 864` (G side), `strings: 5`, tag `fanned`, `perpendicular_fret: 7` | M (37"→34") |
| Bridge `Bass 6-String Bridge` | `mass_g: 190`, `coupling: 0.62`, `strings: 6` | D (5-string bridge scaled) |
| Bridge `Bass Fanned 5 Bridge` | individual saddles, `strings: 5` | D |
| Strings `32-130 Six-String Nickel` | `[0.032, 0.045, 0.065, 0.085, 0.105, 0.130]` | M |
| Nut `Bone Bass 6 54mm` | `width_mm: 54` | D (research has 47.6 for 5-string) |

Existing `Multi-Scale` neck part (8-string guitar) gains an explicit
`scale_length_treble_mm: 660.5` so the audio matches the renderer's
current implicit `698.5 − 38` default.

Guitars: **Six-String Bass** (neck-through, two soapbar pickups using
existing `Bass Ceramic Humbucker 13k` ×2, active preamp) and **Fanned
Five Bass** (bolt-on, `Bass Fanned 5 Neck`).

Presets: **Six-String Chord Bass** (fingerstyle, chordal voicer allowed
on bass), **Fanned Five Low B** (pick, drop tuning option).

## 5. Physics notes (from research §4, I)

- B-string μ = 0.0540 kg/m (back-computed from XLB130 34.5 lb @ 34", M).
  Same string: 34" 153.5 N, 35" 162.6 N, 37" 181.7 N. `mapSpec` must
  reproduce this table ±3 %.
- Inharmonicity ∝ 1/(T L²) at fixed d ⇒ ∝ 1/L⁴ at fixed pitch and μ:
  37" vs 34" lowers B by (34/37)⁴ = 0.71 (−29 %). This is the audible
  reason for long B strings and the test of the fan being real.

## 6. Tests

1. **Bug regression**: a parts 6-string bass with no `tuning` field
   yields six non-zero tensions; string 5 at C3.
2. `BassSixString` open strings within 1 cent.
3. B-string tension table ±3 % at 34/35/37".
4. Fanned 5: B string B coefficient ≤ 0.74 × the same string on a
   straight 34" neck; G string unchanged within 1 %.
5. 8-String Modern re-render: string 7 and string 0 tensions differ by
   the fan ratio; renderer and `DerivedAcoustics` report the same per-
   string scale (±0.5 mm) - the "illustration is authoritative" test.
6. Bass techniques on string 5 of a 6-string: slap/pop/ghost fire and
   rest-stroke damps string 4 → 5 correctly (`bass-techniques.md` 12).
7. Existing 4/5-string guitars: bit-identical renders.

## 7. Illustration

Existing catalogue entries suffice: **Multi-Scale Extended Bass** and
**Headless Modern Bass** (`guitar-illustration.md` 4.4). Needed: 3+3
and 4+2 bass headstocks (6-string), 3+2 (5-string) in §6's headstock
list; renderer fan already implemented.

## 8. Data gaps

Full per-string tension tables for 5/6-string sets; B-string core
diameters (stiffness B not computable, only its scaling); nut width of
6-strings. None block implementation: tensions are computed from
gauges and the μ fixture above. Owner-sourced: string mass per metre of
the actual B string used.
