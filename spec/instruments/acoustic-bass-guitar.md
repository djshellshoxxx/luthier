# ACOUSTIC BASS GUITAR SPEC

A hollow flat-top body (jumbo/0000-sized or larger) on a 34" bass neck,
E1 A1 D2 G2, phosphor-bronze strings, usually an under-saddle piezo.
Research: `docs/research/INSTR_acoustic_bass_guitar.md` (spec sheets
and luthier tap values snippet-level; the below-A0 radiation physics
from Caldersmith 1977 / Christensen & Vistisen 1980 via abstracts).

Tags: **M** documented, **I** inferred, **D** design default.

## 0. Ground rules

1. **Family `bass`, chambering `acoustic`.** Today `shapeFor` returns
   `BassSolid` or `BassHollow` for every bass
   (`Source/Model/Workshop/PartAcoustics.cpp:140`), so an acoustic bass
   is currently modelled as a thinline hollow bass (`Bass Hollow` row,
   11 L, 70 mm deep). That is the gap this spec closes.
2. **The instrument's defining acoustic fact is what it fails to
   radiate.** Fundamentals E1–D2 sit below A0, where soundhole and top
   radiate out of phase (~24 dB/oct asymptote, research §5.3). The
   model must reproduce that, and the piezo path must *not* have it.
3. **The body engine's dry path is the finding.** `BodyEngine` mixes the
   raw string sum with the body response (`BodyEngine.h:87`); the raw
   sum carries full-strength fundamentals into the "acoustic" signal, so
   no acoustic in the product currently loses its sub-A0 fundamentals.
   §3 fixes it for this instrument and proposes it product-wide.

## 1. Parts (factory guitar `Acoustic Jumbo Bass`)

| Slot | Part | Key fields | Tag |
|---|---|---|---|
| `body` | **new** `Jumbo Bass Rosewood Body` | `wood: rosewood`, `chambering: acoustic`, `thickness_mm: 124`, `area_cm2: 1670`, `bracing: x`; `body_style: acoustic_bass_jumbo` | bout 406, depth 124 M (Martin B-40); area I (406² × 1.3 × 0.78) |
| `top` | existing `Sitka Spruce (acoustic)` | `thickness_mm: 3.0` | M wood; thk D (heavier than guitar) |
| `neck` | existing `Bass P-Style Neck` → **new** `Acoustic Bass Mahogany Neck` | `wood: mahogany`, `scale_length_mm: 864`, `joint: set`, `strings: 4`, `frets: 23` | M (B-40: 34", 23 frets) |
| `fretboard` | existing `Ebony` | | M |
| `frets` | existing `Vintage Small Nickel-Silver` | | D |
| `nut` | **new** `Bone Bass 40mm` | `width_mm: 40.1` | M |
| `bridge` | **new** `Acoustic Bass Pin Bridge` | `mass_g: 45`, `coupling: 0.90`, `piezo: true` | mass I (pin bridge 28 g × bass length/width ratio ≈ 1.6), coupling D |
| `tuners` | existing `Bass 20 to 1` | | D |
| `strings` | **new** `45-100 Phosphor Bronze Bass` | `gauges_in: [0.045, 0.065, 0.080, 0.100]`, `winding_material: phosphor_bronze`, `core: hex` | M (EPBB170) |
| `pickups.bridge` | existing `Under-Saddle Piezo` + new fields §3.2 | | — |
| `wiring` | existing `Acoustic Preamp` + `input_impedance_ohm` §3.2 | | — |

Second body for the preset list: `Oversize Walnut Bass Body`
(Earthwood-class): bout 464, depth 168, walnut (all M), 35 L (I).

### 1.1 New `BodyShape` row `AcousticBass`

| lowerBout | depth | soundHole | topThk | backThk | volume L | acoustic |
|---|---|---|---|---|---|---|
| 406 (M) | 124 (M) | 102 (D, 4") | 3.0 (D) | 3.0 (D) | 18.5 (I) | true |

`shapeFor`: `family == "bass" && chambering == "acoustic"` →
`AcousticBass` (and to `Guitarron` when the style says so, see
`guitarron.md` 1.1). Expected coupled A0 **~90 Hz** (research §5.2 I,
consistent with the luthier-measured 87 Hz for an 84 mm hole, M
snippet); Earthwood body **65–70 Hz**; top mode ~170–200 Hz (M
snippet: 170 Hz on one build).

### 1.2 Tension check (M)

EPBB170 at 34": G 47.5, D 55.7, A 47.4, E 40.2 lbf. `mapSpec` must land
within 5 % (test 5.2); this is the best-sourced tension data in this
whole pass, so it is also the calibration fixture for bronze-wound bass
μ in `part-acoustics.md` 8.

## 2. Tuning

Standard `BassStandard` preset exists (E1 A1 D2 G2). 5-string `B0…G2`
and 6-string variants reuse `extended-range-bass.md` 3 rows; no new
preset here.

## 3. Engine changes

### 3.1 Radiation roll-off below A0 (acoustic output path)

A 4th-order high-pass at the body's derived coupled A0, applied to the
**dry (raw-string) component** of `BodyEngine`'s mix and to the body
response below A0, i.e. to everything the acoustic mic/body path emits:

```
H_rad(s) = s^4 / (s^2 + (w0/Q1) s + w0^2)(s^2 + (w0/Q2) s + w0^2)
w0 = 2 pi f_A0,   Q1 = 0.54, Q2 = 1.31   (4th-order Butterworth)
```

- Source of the slope: research §5.3 (two-mass / vented-box asymptote,
  24 dB/oct; I from Christensen & Vistisen). Butterworth alignment is
  **D**; the measured Christensen–Vistisen curve would replace it.
- Magnitudes it must produce (research §5.3, I): with A0 = 85 Hz,
  E1 −25 ± 4 dB, A1 −15 ± 3 dB, D2 −5 ± 2 dB relative to G2's level
  for equal string force.
- New body field `radiation_rolloff` (0–1, blend of `H_rad` against
  flat). **Default 1.0 when `family == bass && chambering == acoustic`.**
  For guitars the physics is identical (E2 82 Hz under a ~98 Hz A0), but
  turning it on changes every acoustic's fixture; default 0.0 elsewhere
  and logged in `docs/review` as an owner decision (**open decision**).
- Block-rate coefficients, recomputed on `mapSpec` only. Two biquads per
  voice-sum, not per string: < 0.02 % CPU.

### 3.2 Piezo high-pass from components

`PickupEngine::piezoHp` is a fixed 2nd-order 40 Hz high-pass
(`Source/DSP/Pickup/PickupEngine.cpp:134`). That already cuts B0
(30.9 Hz) by ~5 dB and does not respond to the part. Replace with the
physical RC:

`f_c = 1 / (2 π R_in C_piezo)`, **1st order** (research §6.2, M
snippet for the RC form and the ≤ 12 nF range).

| Field | Part | Default | Tag |
|---|---|---|---|
| `capacitance_pf` | pickup (field exists, `Under-Saddle Piezo` has 0) | **1000** | D (within ≤ 12 nF M) |
| `input_impedance_ohm` | wiring (new) | **10 MΩ** for `Acoustic Preamp`; 1 MΩ for passive | M (typical preamp), D |

1 nF × 10 MΩ → 16 Hz (passes B0); 1 nF × 1 MΩ → 159 Hz, which is the
real "piezo straight into an amp" thinness and a test (5.5).
`piezoRes` (3 kHz +4.5 dB) stays; it is unsourced (research §6.2) and
is flagged as a data gap rather than changed.

### 3.3 Mic/piezo blend

Existing `setPiezoMicBlend`. Default for this guitar **0.65 piezo**
(D): the realistic live ABG sound is mostly piezo because of §3.1.

## 4. Presets

| Name | Body | Notes |
|---|---|---|
| **Unplugged Bass** | Jumbo | Mic only (blend 0), shows the thin acoustic low end honestly |
| **Acoustic Bass DI** | Jumbo | Blend 0.65, preamp EQ flat |
| **Big Walnut Bass** | Oversize walnut | Mic-heavy (0.3); lower A0 gives more acoustic low end |

## 5. Tests

1. Round-trip; family `bass` + chambering `acoustic` resolves to
   `AcousticBass`, not `BassHollow`.
2. Tensions within 5 % of §1.2.
3. Derived A0: Jumbo 80–100 Hz; oversize walnut 60–75 Hz.
4. Radiation roll-off: at equal string velocity, mic-path level of open
   E1 relative to open G2 is −25 ± 4 dB (A0 85 Hz fixture); with
   `radiation_rolloff` 0 it is within ±3 dB of the piezo path.
5. Piezo RC: 1 nF / 1 MΩ gives −3 dB at 159 ± 5 Hz; 1 nF / 10 MΩ at
   16 ± 1 Hz; 1st-order slope (6 ± 1 dB/oct) below f_c.
6. Piezo path unaffected by `radiation_rolloff` (bit-identical).
7. Existing acoustics unchanged with `radiation_rolloff` default 0
   (bit-identical renders).

## 6. Illustration (`guitar-illustration.md` 4.4 "Acoustic Bass")

The catalogue already lists an Acoustic Bass outline (435 × 555 × 135).
Replace with the sourced sizes: **Jumbo** 406 × 511 × 124 (M) and
**Oversize** 464 × 622 × 168 (M). Round soundhole Ø 102, pin bridge
with 4 pins, 4-in-line or 2+2 headstock, optional single cutaway (BC-15E
style, existing cutaway geometry). Piezo drawn as the existing
under-saddle strip; preamp as a side-mounted panel rectangle on the
upper bass bout.

## 7. Data gaps

No peer-reviewed ABG modal data or Q; mode values are luthier-forum
snippets. Caldersmith 1977, Christensen & Vistisen 1980, Elejabarrieta
et al. 2002 (JASA, paywalled) unread: the true sub-A0 transfer curve is
**I**. Piezo transfer function, preamp EQ, T60, centroid unmeasured.
Owner-sourced: bridge tap test with soundhole open/covered; one
recording per open string with mic + piezo DI **simultaneously** (this
single measurement validates §3.1 and §3.2 together); LCR/capacitance
of the piezo; string mass per metre.
