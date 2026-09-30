# GUITARRÓN MEXICANO SPEC

The mariachi bass: a fretless, six-string acoustic with a very deep
V-arched back, nylon trebles and metal-wound basses, tuned
A1 D2 G2 C3 E3 A2 (re-entrant) and played mostly in **octave pinches**.
Research: `docs/research/INSTR_guitarron.md` (snippet-level; no measured
acoustics exist, see its §9).

Tags per `README.md` 0.1: **M** documented, **I** inferred (formula
stated), **D** design default.

## 0. Ground rules

1. **Family `bass`.** Its role, range (55–~440 Hz) and rhythm engine
   (bass step grid, `bass-techniques.md` 9) are a bass's. Bass-family
   defaults that are wrong for it are overridden in the guitar file
   (§4), not by special-casing the family.
2. **Octaves are a gesture, not a chorus.** Two strings are plucked
   together by thumb and finger; each string is its own physical string
   with its own intonation, decay and beating. No doubling effect.
3. **Fretless is already modelled.** `Fretless (unlined)` frets part
   and `d.spec.fretless` (`PartAcoustics.cpp:352`) exist; nothing new.
4. **Nylon + metal in one set is already expressible** via per-string
   material overrides (`DerivedAcoustics::stringMaterialOverride`,
   `PartAcoustics.h`).

## 1. Parts

| Slot | Part | Key fields | Tag |
|---|---|---|---|
| `body` | **new** `Guitarrón Cedro Body` | `wood: cedro` (new row, §1.2), `chambering: acoustic`, `thickness_mm: 280`, `area_cm2: 2250`, `bracing: ladder` | depth M (max, V apex); area I; bracing D |
| `top` | **new** `Tacote Top` | `wood: tacote` (new row), `thickness_mm: 3.0` | wood M; thickness D |
| `neck` | **new** `Guitarrón Neck` | `wood: cedro`, `scale_length_mm: 660`, `joint: set`, `strings: 6`, `frets: 0` | scale I (M 26" snippet); wood M |
| `fretboard` | existing `Rosewood` (cocobolo ≈ rosewood density) | `radius_mm: 0` flat | M/D |
| `frets` | existing `Fretless (unlined)` | | M |
| `nut` | existing `Bone Bass 38mm`, `width_mm` → 60 | | width D |
| `bridge` | **new** `Guitarrón Tie Bridge` (from `Classical Tie Bridge`) | `mass_g: 40`, `coupling: 0.90` | type M (loop/tie); values D |
| `tuners` | existing `Bass 20 to 1` | | D |
| `strings` | **new** `Guitarrón Nylon/Bronze` | see §2 | M (gauges) |
| `pickups` | none; optional `Under-Saddle Piezo` | | — |

### 1.1 New `BodyShape` row `Guitarron`

`computeAirResonance` treats the body as a box of `volumeLitres`; the
V-back is represented only through volume and the back's mode (§3).

| lowerBout | depth | soundHole | topThk | backThk | volume L | acoustic |
|---|---|---|---|---|---|---|
| 470 (M upper bound, packed dims) | 280 (M, apex) | 120 (D) | 3.0 (D) | 3.0 (D) | 35 (I, 25–45) | true |

`shapeFor` maps `family == "bass"` **and** `chambering == "acoustic"`
**and** `body_style` containing `"guitarron"` to it (today every bass is
`BassSolid`/`BassHollow`, `PartAcoustics.cpp:140`; the acoustic-bass
spec adds the `"acoustic"` branch this rides on).

Expected coupled A0 from research §5: **~72 Hz (58–92)**, i.e. near D2;
the body supports the fundamental of every string except A1.

### 1.2 New wood rows (`lookUpWood`, `part-acoustics.md` 1)

| id | ρ kg/m³ | E‖ GPa | tanδ | Tag |
|---|---|---|---|---|
| `cedro` (*Cedrela odorata*) | 480 | 8.0 | 8.5e-3 | ρ M (420–610 range midpoint-low); E M (6.2–10 range); tanδ D (≈ western red cedar row) |
| `tacote` (*Gyrocarpus*) | 240 | 4.5 | 10e-3 | ρ M (SG 0.24); E I (spruce specific modulus 27.5 GPa/(g/cc) × 0.24 × 0.7 conservative); tanδ D |

`engineWood` maps `cedro` → `Cedar`, `tacote` → `Cedar` (closest body
enum). The part-level density still moves modes via `resonanceTrim`
(`PartAcoustics.cpp:394`), so the very light top is audible.

## 2. Strings and tuning

`Guitarrón Nylon/Bronze` (D'Addario MG10N-style, research §4):

| String | Note | Hz | Gauge in | Material | Tag |
|---|---|---|---|---|---|
| 0 | A2 | 110.00 | .110 | nylon (wound) | M |
| 1 | E3 | 164.81 | .082 | nylon | M |
| 2 | C3 | 130.81 | .098 | nylon (wound) | M |
| 3 | G2 | 98.00 | .060 | phosphor bronze, steel core | M |
| 4 | D2 | 73.42 | .075 | phosphor bronze, steel core | M |
| 5 | A1 | 55.00 | .098 | phosphor bronze, steel core | M |

Set-level `winding_material: phosphor_bronze`; strings 0–2 carry
`material: nylon` overrides. Gauge-to-string mapping is **I** (pitch
order; research §4).

Guitar-level `tuning` (`README.md` 1.4):
`open_hz: [110.0, 164.814, 130.813, 97.999, 73.416, 55.0]`. Named
`TuningEngine` preset `Guitarron`. This is the first re-entrant tuning
in the product; `ChordVoicer`/`RubricVoicer` must not assume string
pitch is monotonic in index (test 6.4).

Tension check (I): nylon trebles 140–175 N at 660 mm (research §4) ≈
2× classical; the mapping must land within 30 % (the research's own
uncertainty).

## 3. Body modes (all I; replace on measurement)

| Mode | Hz | Q | Gain | Source |
|---|---|---|---|---|
| A0 (air, coupled) | 72 | 15 | 1.0 | research §5 formula |
| T(1,1)₁ top | 165 | 25 | 0.8 | 1.5–2 × A0 heuristic, light top ⇒ low end of band |
| Back (V-keel) | 230 | 30 | 0.5 | research §5 |
| Higher top | 300, 420 | 35, 45 | 0.35, 0.25 | classical ratios × 0.7 |

These come out of `BodyModels::buildModes` given the §1.1 row; this
table is the **acceptance band** for test 6.3, not a hand-entered bank.

## 4. Playing model

### 4.1 Octave pinch (new voicer mode, no DSP)

`ChordVoicer` gains `OctavePinch` for guitars tagged `octave_pinch`:

- A single incoming bass note `n` is voiced on **two strings**: the
  lowest string that can stop `n`, plus a string that can stop `n + 12`
  with the smallest left-hand stretch. Fixed shapes from research §6:
  A1 on s5 + A2 on s0 (both open); D2 on s4 + D3 on s2 (+2 st); G2 on
  s3 + G3 on s1 (+3 st).
- Onsets: thumb (low string) first, finger +3 ms (D), velocity of the
  upper string × 0.85 (D).
- Toggle `octave_pinch_auto` (bool, default **on** for this guitar,
  absent elsewhere). A chord or two simultaneous notes bypass it.
- Because the neck is fretless, each string's intonation carries the
  existing fretless finger-position jitter independently, so the pair
  beats slightly - the intended "thick" sound (research §7).

### 4.2 Apagón

Map to existing mechanisms: bass-step-grid step type `dead` +
`bass-techniques.md` 7 palm-mute profile with decay × 0.35 (D). A new
step label **"Apagón"** is an alias in the bass grid for this guitar.

### 4.3 Excitation

Right hand: `Fingertip` material, `nailVsFlesh 0.4`, pluck position
0.30 (research §6: 0.25–0.35 L). Slap/pop remain available (family
bass) but are not used by any preset.

## 5. Guitar-file overrides of bass defaults (`bass-techniques.md` 8)

| Default | Bass | Guitarrón | Why |
|---|---|---|---|
| Compressor | on 2:1 | **off** | acoustic, unamplified tradition |
| Output | amp + cab | **mic** (acoustic body path) | no pickup |
| Scale | 864 mm | **660 mm** | §1 |
| `ghost_auto` | on | **off** | apagón is explicit, not velocity-derived |

## 6. Tests

1. Round-trip of `Guitarrón.luthierguitar`.
2. Open strings sound A2 E3 C3 G2 D2 A1 within 1 cent (tuning field
   honoured; re-entrant order preserved).
3. Derived A0 in 58–92 Hz; first top mode in 140–190 Hz.
4. **Re-entrant voicing**: a C-major chord request never assigns C3 to
   s0 on the assumption that s0 is highest; voicer tests run with the
   guitarrón tuning.
5. Octave pinch: MIDI A1 alone produces exactly two string onsets
   (s5, s0) 3 ± 0.5 ms apart; with `octave_pinch_auto` off, one onset.
6. Nylon override: strings 0–2 render with the Nylon material's loop
   filter (lower centroid than an identically tuned bronze string).
7. Apagón step: T60 ≤ 0.35 × the free note's.

## 7. Illustration (`guitar-illustration.md` 4.4 Bass bodies)

**Guitarrón**
- Face outline: broad, short-waisted guitar outline, ~470 mm lower
  bout, ~580 mm body length (I); round soundhole Ø 120 (D), wide
  rosette ring.
- **Depth view**: the V-back is drawn in the side/edge silhouette the
  illustration already renders for depth (a triangular keel profile,
  280 mm at apex) - the only new geometry primitive in this spec.
- Tie bridge (classical style, wider), fretless board with no fret
  lines and side dots only, slotted-free solid headstock 3+3.
- Strings: three clear nylon (colour per §10 nylon), three bronze.

## 8. Data gaps

Blocking accuracy (not implementation): **all** body acoustics
(A0, top, back, Q), body volume, soundhole diameter, nut width, true
scale distribution across makers, string tensions/core diameters,
T60/centroid. Owner-sourced: tap test open/blocked soundhole; single
open plucks, octave pinches and apagón recorded dry; string mass per
metre; tape-measure of body and scale.
