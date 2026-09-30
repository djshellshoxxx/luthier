# CHITARRA SARDA SPEC

The large, steel-strung, tailpiece-and-floating-bridge baritone flat-top
that accompanies *cantu a chiterra* in northern Sardinia. Research:
`docs/research/INSTR_chitarra_sarda.md` (all sourced values are
search-snippet level; see its §9).

Value tags per `README.md` 0.1: **M** measured/documented, **I**
inferred by a stated formula, **D** design default with no data.

## 0. Ground rules

1. **It is a build, not a preset.** Distinct body, distinct bridge
   system, distinct tuning. It gets its own factory guitar, body part
   and string part; everything else reuses existing parts.
2. **Family `acoustic`.** No new family and no new code path except the
   guitar-level `tuning` field (`README.md` 1.4) and one body-shape row.
3. **It is not Paolo Angeli's prepared guitar.** Hammers, pedals and
   18–25 strings are one player's instrument and are out of scope.

## 1. Parts

| Slot | Part | Fields | Tag |
|---|---|---|---|
| `body` | **new** `Sarda Large Body` | `wood: mahogany`, `density_kg_m3: 550`, `chambering: acoustic`, `thickness_mm: 110`, `area_cm2: 1960`, `bracing: ladder` | depth, bout M; area I; bracing D |
| `top` | existing `Sitka Spruce (acoustic)` | `thickness_mm: 2.9` | wood M (one maker); thickness D |
| `neck` | **new** `Sarda Long Neck` | `wood: mahogany`, `scale_length_mm: 690`, `joint: set`, `strings: 6`, `frets: 19` | wood M (one maker); scale I (midpoint of 680 M / 705 M); frets D |
| `fretboard` | existing `Rosewood` | | D |
| `frets` | existing `Vintage Small Nickel-Silver` | | D |
| `nut` | existing `Bone 43mm` | | D |
| `bridge` | existing `Floating Archtop Bridge` | `coupling` override **0.80**, `mass_g` 22 | type M; values D |
| `tailpiece` | existing `Trapeze` | `break_angle_deg: 10` | type M; angle D |
| `tuners` | existing `Vintage Open-Back` | | D |
| `strings` | **new** `16-70 Baritone Bronze` | `gauges_in: [0.016, 0.022, 0.030, 0.047, 0.059, 0.070]`, `winding: round`, `winding_material: bronze_8020`, `core: hex` | proxy set I (research §4) |
| `pickups` | none; optional `Soundhole Magnetic` | | — |
| `pickguard` | **new** `Inlaid Floral Guard` | `mass_g: 18` | visual M; mass D |

### 1.1 Area derivation (I)

Research §2 gives lower bout 440 mm (M) and depth 110 mm (M). With
`PartAcoustics.cpp`'s own plan-area rule (bout² × 1.3 × 0.78):
`44² × 1.3 × 0.78 = 1963 cm²` → `area_cm2: 1960`. That yields
`scaleWidth = 1960 / (39.7² × 1.3 × 0.78)` → √ → **1.108** against the
Dreadnought row, and `scaleDepth = 110 / 121 = 0.909`, so the engine's
volume = 17.5 L × 1.108² × 0.909 = **19.5 L**. The research estimate is
16.7 L ± 20 %; 19.5 L sits at the top of that band. To land on the
research figure, the new shape row (1.2) carries its own volume.

### 1.2 New `BodyShape` row

`Source/Model/Guitar/BodyModels.cpp:35` table, one row, and
`shapeFor` maps `body_style` containing `"sarda"` to it:

| name | lowerBout | depth | soundHole | topThk | backThk | volume L | acoustic |
|---|---|---|---|---|---|---|---|
| `Sarda` | 440 (M) | 110 (M) | 100 (D) | 2.9 (D) | 2.8 (D) | 16.7 (I) | true |

With that row `scaleWidth`/`scaleDepth` are 1.0 and
`computeAirResonance` gives the rigid-box value; the model's coupled A0
is then expected at **95–105 Hz** (research §5, I). Test 6.3 asserts it.

### 1.3 Floating bridge + tailpiece: what changes physically

The top is driven by vertical bridge force only (no pin-bridge torque),
research §7 (I). Mapped with **existing** fields only:

- `bridge.coupling` 0.80 versus the pin bridge's 0.92
  (`part-acoustics.md` 5): less energy into the top, longer string
  sustain, fewer low-frequency radiated fundamentals.
- `tailpiece.break_angle_deg` 10 → downforce term already in
  `fretBrightness` (`PartAcoustics.cpp:448`).
- **Afterlength resonance** (bridge→tailpiece segment) is *not*
  modelled anywhere today. It is noted, not added: no measurement exists
  to size it (Data gaps).

## 2. Tuning

Guitar-level `tuning` (`README.md` 1.4). String 0 = treble side.

| Preset | open_hz (string 0→5) | Tag |
|---|---|---|
| **Sarda (a fourth down)** default | 246.94, 185.00, 146.83, 110.00, 82.41, 61.74 (B3 F#3 D3 A2 E2 B1) | M |
| Sarda low (a fifth down) | 220.00, 164.81, 130.81, 98.00, 73.42, 55.00 (A3 E3 C3 G2 D2 A1) | M (range edge) |

Also added to `TuningEngine` as named presets `SardaFourth`,
`SardaFifth`. Intermediate pitches are reached with the existing global
transpose/detune, because the pitch is set to the singer (research §3).

### 2.1 Tension check (I)

From research §4 at 690 mm: 115–175 N per string, ≈ 800 N total, i.e.
dreadnought-like. `mapSpec` must reproduce each string's tension within
10 % of the research table interpolated to 690 mm (test 6.2).

## 3. Playing defaults

Research §6: chordal accompaniment plus arpeggios and counterpoint;
pick versus finger is undocumented.

- Default right hand: **pick** (`Celluloid 0.73 Standard`), pluck
  position 0.20 (D). The inlaid pickguard implies plectrum use (I).
- Rhythm kit: new `Cantu a Chiterra` kit = existing arpeggio patterns
  (`Classical Arpeggio`, `Post-Rock Arpeggio` timing) re-voiced for a
  slow free-time accompaniment; **no new pattern engine**.
- Voicer: prefer open-position shapes; the canonical key is "in Re"
  (D shapes), sounding A with the default tuning (research §3; whether
  mode names mean shape or pitch is an open gap).

## 4. Presets

| Name | Guitar | Notes |
|---|---|---|
| **Cantu in Re** | Chitarra Sarda | Default tuning, D-shape arpeggio, small-room mic, no amp |
| **Sarda Low Accompaniment** | Chitarra Sarda | `Sarda low` tuning, strummed, warmer mic position |
| **Sarda Counterpoint** | Chitarra Sarda | Fingerstyle arpeggio (flesh 0.6), single-note lines |

All follow `factory-content.md` 0 naming; no player or maker names.

## 5. Illustration (`guitar-illustration.md`)

New acoustic body entry, §4.2:

**Sarda Large (chitarrone)**
- Outline: 440 mm lower bout, ~530 mm body length (I: 1080 overall −
  neck/head), 110 mm deep. Wider and shallower than the Dreadnought.
- Round soundhole (D 100 mm), **no pin bridge**: floating bridge drawn
  as the archtop bridge (§7) plus a trapeze tailpiece at the tail block.
- Pickguard: the inlaid-floral guard - a flat fill plus a low-opacity
  vine stroke pattern (the §6 "Vine" inlay generator reused), let into
  the top below the soundhole.
- Fret markers at III, V, VII, IX, XII (standard dot set).
- Headstock 3+3, slotted-free solid head.

No new drawing primitives; this is data for existing renderers.

## 6. Tests

1. **Round trip**: `Chitarra Sarda.luthierguitar` loads, serialises and
   reloads to an identical `GuitarSpec` (`guitar-workshop.md` 10).
2. **Tension**: each string's derived tension within 10 % of research
   §4 interpolated to 690 mm.
3. **Air mode**: coupled A0 of the derived mode bank in 90–110 Hz.
4. **Tuning field used**: open strings sound B1…B3 within 1 cent; with
   the `tuning` field deleted the guitar falls back to the Dreadnought
   base preset and the notice fires.
5. **Floating bridge is audible**: swapping `Floating Archtop Bridge`
   for `Acoustic Pin Bridge` changes the rendered spectrum by more than
   the noise floor and lengthens 60 dB decay of open B3.
6. **Low strings below A0**: rendered B1's fundamental is at least 6 dB
   under its 2nd partial at the default mic (research §5 guidance, I);
   re-baseline when a measured recording arrives.

## 7. Data gaps (blocking accuracy, not implementation)

Every body-mode number is **I** or **D**. The instrument can ship with
these, but its fixture must be re-baselined when any of these arrive:

- tap-test impulse response (strings damped; soundhole open and
  blocked) → A0, T1, back mode, Q;
- nut-to-saddle on a Catania/Miroglio and a modern instrument → scale;
- soundhole diameter, body length, depth taper → volume;
- bracing pattern (mirror photos);
- player string gauges;
- recordings of open B1/E2/A2/B3 → T60, centroid;
- afterlength resonance (tap the tailpiece segment).
