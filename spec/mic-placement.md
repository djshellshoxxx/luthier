# MIC PLACEMENT SPEC

Drag the microphone across the speaker the way an engineer does with a
real cabinet: centre of the dust cap, cap edge, out across the cone, off
the edge. Pull it back, angle it off-axis, and hear the change smoothly
and continuously as you move it. Acoustic guitars get the same thing:
mics placed around the body (12th fret, soundhole, bridge, lower bout)
at a distance.

Added 2026-09-24 at the product owner's request. It adds to
`gui-integration.md`, `engine.md` 13 and `tone-match.md`. It replaces
the discrete Position and Distance choices as the way you edit mic
placement. Those choices stay in the parameter list for compatibility
(section 4).

Test ID prefix: **MP-**.

## 0. Ground rules

1. **The placement is continuous, and moving it never swaps an IR.**
   Today, changing `mic_position` or `mic_distance` is a structural
   change. `ParameterBridge::applyStructural` reloads an IR, and the
   procedural fallback fills in during the swap. Under this spec, each
   mic's convolution always loads one *anchor* IR for its (cabinet,
   speaker, mic model): the shipped `..._cap_edge_close.wav`. Everything
   placement does is a continuous, parametric correction applied after
   the anchor. Why not crossfade between IRs on a grid? The shipped grid
   has only 3 positions x 2 distances. Covering x, y, distance and angle
   finely would take thousands of IRs per combination. Crossfading
   between two IRs also costs a third convolution and produces phasey
   midpoints.
2. **Identity at the anchor.** At the anchor placement (Cap Edge,
   2.5 cm, 0 degrees, front, speaker 1), the placement stage passes the
   signal through unchanged. Old presets therefore land exactly on
   today's sound or within the fidelity bounds of MP-03.
3. **Honest physics, honest magnitudes.** Each effect comes from a named
   mechanism with the size a real session shows:
   - proximity effect against distance and pickup pattern;
   - HF loss against radius (cone beaming) and against angle (the
     capsule's polar response);
   - dust-cap and surround resonances;
   - the whole cone blending together as the mic moves away;
   - inverse-distance level;
   - the floor bounce;
   - the time of arrival between two mics.
   None of these is a "tone" EQ with a made-up curve.
4. **Click-free under any motion.** Inputs are smoothed. Coefficients
   are recomputed at control rate (every 32 samples) into TPT
   state-variable filters, which stay stable and zipper-free under
   modulation (engine.md 0.4). Delays glide under a slew limit.
   Discrete switches (rear, speaker index) crossfade their derived
   targets.
5. **One model, three consumers.** The engine, the live frequency plot
   and the tests all evaluate the same pure function,
   `MicPlacementModel::evaluate`. The plot therefore shows exactly what
   you hear.
6. **No samples.** The anchor IRs are the existing synthesised
   `scripts/make_irs.py` output. The placement stage is DSP.

## 1. User stories

- *Rock player:* "My 4x12 is too fizzy." They drag Mic 1 from Cap Edge
  toward Cone. The fizz rolls off as they drag, with no dropouts, and
  the plot shows the 4-6 kHz shelf falling.
- *Engineer:* puts a dynamic on the cap edge at 2.5 cm and a ribbon at
  30 cm. With ToF set to Physical, they hear the comb filtering. They
  switch to Aligned, and the plot's sum curve flattens.
- *Bass player:* with an 8x10, clicks the lower-left speaker to mic it,
  then backs off to 20 cm for a rounder low end.
- *Acoustic player:* moves the mic from 12th Fret toward Soundhole and
  hears the boom grow. Adds Mic 2 at the lower bout.
- *Easy Mode beginner:* one pad, "bright <-> warm" across and
  "close <-> far" down. They hear the change as they drag.
- *Producer:* automates Mic 1 X from the host for a slow "filter sweep"
  that sounds like a moving mic, not an EQ.

## 2. Exact behaviour: electric and bass cabinets

### 2.1 Coordinates

Each mic is placed over one speaker of the cabinet, `mic_speaker`
(1..N, numbered left to right and top to bottom as seen from the
front). Position is `(mic_x, mic_y)` in **cone-landmark units** `u`:
- `u = 0`: dust-cap centre.
- `u = 0.35`: Cap Edge, on every speaker size.
- `u = 1.0`: cone edge (where the surround starts).
- `u = 1.12`: outer edge of the surround.
- up to `u = 1.4`: the baffle.

The radius is `u = hypot(x, y)`. Azimuth has no acoustic effect,
because a cone is axisymmetric. It is stored so the drawing is
faithful.

Landmark units keep the default and the legacy mapping exact on
10", 12" and 15" speakers alike. Drawing maps `u` to mm per speaker
size:

| Size | Effective cone radius `R` | Cap radius |
|---|---|---|
| 12" | 110 mm | 38 mm |
| 10" | 90 mm | 30 mm |
| 15" | 140 mm | 42 mm |

Cabinet to speaker count and size, with speaker-centre heights above the
floor used for the floor bounce:

| Cabinet | Speakers | Heights |
|---|---|---|
| 1x12 open / closed | 1 x 12" | 0.45 m |
| 2x12 open / closed | 2 x 12" | 0.35 m |
| 4x12, 4x12 vintage | 4 x 12" | 0.62 m top, 0.25 m bottom |
| 1x15 bass | 1 x 15" | 0.30 m |
| 4x10 bass | 4 x 10" | 0.50 m, 0.22 m |
| 8x10 bass | 8 x 10" | 0.90 m, 0.66 m, 0.42 m, 0.18 m |

If `mic_speaker` exceeds the cabinet's count, the engine and the view
use speaker 1. The stored value is kept, so switching back restores it.

**Snap points** (labelled rings):

| Label | u |
|---|---|
| Cap | 0 |
| Cap Edge | 0.35 |
| Cone | 0.62 |
| Edge | 0.90 |

**Distance** `mic_dist`, in cm, is measured from the grille plane.
Stock range is 0-100, advanced 0-200. The maths clamps to at least
0.5 cm.

**Angle** `mic_angle`, in degrees off-axis. Stock range is 0-90,
advanced 0-180. Past 90 degrees, a figure-8's rear lobe answers with
inverted polarity.

**Rear** `mic_rear` places the mic behind the cabinet.

### 2.2 The model (`MicPlacementModel::evaluate`)

`evaluate` is a pure, `noexcept`, allocation-free function. Its inputs
are the placement, the speaker voice, the mic voice and the cabinet
geometry.

The speaker and mic tables `kSpeakers` and `kMics` move out of
`CabinetEngine.cpp`'s anonymous namespace into a new
`Source/DSP/Amp/CabinetVoices.h`. Two tables are added there:
- `kMicPolar[]`:
  - pattern coefficient `a`: 0.5 cardioid for every mic, except the
    Ribbon, which is 0 (figure-8);
  - HF directivity scale `s`: Classic, Broadcast and Wide Dynamic 1.0;
    Large Condenser 1.25; Ribbon 0.8; Studio Condenser 1.2;
    Kick Dynamic 1.1.
- `kCabGeometry[]`: the table in 2.1, plus the cabinet depth from
  make_irs.py `CABS`.

Terms. Every one is expressed as a delta, `f(p) - f(anchor)`:

- **Beaming weight.** `w(d) = 1 / (1 + (d / (1.6 R))^2)`. Close up, the
  mic hears its own spot on the cone. Far away, it hears the whole cone.
  Positional terms blend toward their area mean over `u` in [0, 1],
  written with an overbar: `X_eff = w X(u) + (1 - w) X̄`. The means are
  computed once in `prepare`.
- **Presence and axis, `A_r(u)` (dB).** Piecewise-linear through these
  knots:

  | u | 0 | 0.35 | 0.9 | 1.0 | 1.12 | 1.4 |
  |---|---|---|---|---|---|---|
  | dB | +2.5 | 0 | -4.5 | -5.5 | -6.5 | -9 |

  It is applied at `speaker.presenceHz`, Q 1.3.
- **HF corner, `T_r(u)`.** Knots:

  | u | 0 | 0.35 | 0.9 | 1.0 | 1.12 | 1.4 |
  |---|---|---|---|---|---|---|
  | value | 1.15 | 1.0 | 0.65 | 0.60 | 0.55 | 0.45 |

  These knots reproduce make_irs.py `POSITIONS` exactly at the legacy
  points.
- **Angle.** Let `k = (1 - cos θ) / (1 - cos 45°)`.
  - `A_θ = -2.5 s k` dB.
  - `T_θ = max(0.3, 1 - 0.2 s k)`.
  - At 45 degrees with `s = 1`, these give the legacy OffAxis45 values
    (-2.5 dB, 0.80).
  - Broadband polar level: `L_θ = 20 log10(max(0.03, |a + (1 - a) cos θ|))`.
  - Polarity is `sign(a + (1 - a) cos θ)`.
- **Proximity, `P(d)`.** Log-distance interpolation through these knots:

  | d (cm) | 0.5 | 2.5 | 15 | 30 | 100 | 200 |
  |---|---|---|---|---|---|---|
  | P | 1.35 | 1.0 | 0.45 | 0.10 | 0 | 0 |

  The 2.5, 15 and 30 cm values are the legacy `DISTANCES`. The delta is
  `mic.proximityDb × (P(d) - 1)` at `mic.proximityHz`, Q 0.9.
- **Dust cap.** `C(u) = +3 dB × (1 - smoothstep(0, 0.5, u))` at
  `0.85 × speaker.topRollHz`, Q 2.
- **Surround dip.** `S(u) = -2.5 dB × smoothstep(0.8, 1.05, u)` at
  `0.6 × speaker.presenceHz`, Q 1.5. Both C and S are beaming-weighted.
- **HF corner shift.** `T = T_eff × T_θ`. The legacy roll-off is
  `1 / sqrt(1 + (f / f0)^8)`, so moving the corner by a factor T changes
  the level above it by `80 log10(T)` dB. This is realised as two
  cascaded high shelves, each carrying half of
  `G = clamp(80 log10(T / T_anchor), -30, +6)` dB, centred at
  `f_ref × sqrt(T / T_anchor)`, where `f_ref = min(topRollHz, mic.topHz)`.
- **Rear.**
  - Replaces the axis and HF terms with the legacy Rear values: -6 dB
    and a corner factor of 0.55.
  - Inverts polarity: the back of the cone.
  - Adds 0.12 m to the path length.
  - On closed-back cabinets, adds a 600 Hz 2-pole low-pass: sound
    through the back panel.
- **Level.** `L_d = -20 log10((d + 0.35R) / (2.5 + 0.35R))`, with d and
  R both in cm.
- **Level match** (`mic_level_match`, default on). Adds
  `-(L_d + L_θ)`, capped at +18 dB. This mirrors an engineer setting
  each mic's preamp gain. Off gives the raw physical level, which is
  what makes blends of near and far mics realistic.
- **Floor bounce.**
  - Path to the floor image: `p_f = sqrt(d^2 + (2h)^2)`.
  - Reflection gain: `g = ρ × d / p_f`, where ρ comes from the room
    material:

    | Room material | ρ |
    |---|---|
    | Dry | 0.20 |
    | Wood | 0.45 |
    | Tile | 0.70 |
    | Stone | 0.60 |
    | Room off | 0.35 |

  - The reflection is added with gain `max(0, g - g_anchor)`, at delay
    `(p_f - d) / c`, through a one-pole 4 kHz low-pass.
  - The anchor IR already holds the close-mic bounce, so the far mic
    gains the audible comb and the close mic gains nothing.
- **Per-speaker variation.** Speaker 1 is exactly the reference. Every
  other speaker k draws three values ρ1-3 in [-1, 1] from
  `RtRandom(hash(cabinet, k))`. They shift presenceHz by `3% ρ1`,
  A_r by `0.6 ρ2` dB and the cap frequency by `4% ρ3`. Real 4x12
  speakers differ this much, and the variation is deterministic.

### 2.3 Two mics: time of arrival

- Each mic's path is `p = sqrt(d^2 + (0.5 u R)^2)`, plus 0.12 m when
  rear.
- `mic_tof_mode = Physical`:
  - The farther mic is delayed by `Δp / c`.
  - The nearer mic is not delayed, so no latency is added.
  - Comb filtering is audible, as it is on a real session.
- `mic_tof_mode = Aligned` (the default):
  - Both mics get zero relative delay.
  - A rear mic's polarity is re-inverted, the way an engineer flips it.
- The existing `mic_phase_align` (0-300 mm) stays a manual extra delay
  on mic 2 in both modes. That keeps its current meaning, so legacy
  presets are unchanged.
- The delay is a 4-point Lagrange fractional line with a slew limit of
  0.0025 samples per sample (at most 4.3 cents of Doppler while dragging
  fast).
- With a single mic, no ToF delay is applied.

## 3. Exact behaviour: acoustic guitars

This applies when `spec.category == GuitarCategory::Acoustic`.

Coordinates are portable landmark units, measured along the guitar
centreline from tail to neck. `ac_mic_along` runs from 0 to 4:

| Value | Landmark |
|---|---|
| 0 | Tail (end pin) |
| 1 | Lower Bout (centre) |
| 2 | Bridge (saddle) |
| 3 | Soundhole (centre) |
| 4 | 12th Fret |

Upper Bout is labelled at 3.5. In between, positions interpolate
linearly in mm.

`ac_mic_across` runs from -1 to 1, as a fraction of the half-width of
the lower bout (+ is the treble side). Axes follow guitar-illustration.md
1: origin at the saddle, +X toward the headstock.

Landmarks come from a new pure function
`computeAcousticLandmarks(const GuitarSpec&)` in
`Source/Model/Guitar/AcousticLandmarks.h`, which uses `BodyOutlines.h`.
The 12th fret is at scale / 2. A 12-fret guitar's body join sits at the
same place. With no soundhole (`soundHoleMm == 0`: resonator, f-hole
bodies), the Soundhole landmark stays at the geometric centre of the
upper-lower bout, but its air term is 0.

`AcousticMicModel` in `Source/DSP/Body/AcousticMicModel.{h,cpp}` works
per mic.

**Radiator weights.** Five radiators sit at along = 1, 2, 3, 3.5 and 4,
all on the centreline. The mic's point on the top weights them by a 2D
Gaussian over mm distance, with `σ = 60 mm + 0.9 d(mm)`, normalised to
sum to 1. Close up, the mic hears one region. At 1 m, it hears the whole
guitar.

**Composite filter.** Each band's gain is `Σ w_i g_i`:

| Radiator | Air peak at `f_air` (Q 2.5) | 220 Hz (Q 1) | 2.5 kHz (Q 1) | 6 kHz shelf | String-direct send |
|---|---|---|---|---|---|
| Lower Bout | +3 | +3 | -1 | -3 | 0.05 |
| Bridge | +1 | +1 | +2 | 0 | 0.20 |
| Soundhole | +8 | +2 | -2 | -4 | 0.05 |
| Upper Bout | +2 | 0 | +1 | +1 | 0.15 |
| 12th Fret | 0 | -2 | +1 | +3 | 0.35 |

- `f_air = BodyEngine::getAirResonanceHz()`, or 100 Hz when that
  returns 0.
- The string-direct send is `stringSumBuffer` high-passed at 300 Hz.

**Mic voice, then shared terms.**
- The mic voice comes from the same `kMics` row as the electric path:
  proximity `proximityDb × P(d)`, presence `presenceDb × 0.5`, and a
  low-pass at `topHz`. `mic_type` and `mic_type_2` are shared with the
  cabinet. Acoustic presets already set the cabinet to `AcousticDI`,
  which ignores the mic model.
- Then the same angle, polar, level (d0 = 10 cm), level-match and ToF
  terms as section 2. The floor bounce uses h = 0.7 m (seated player).
- A calibration constant K, computed at prepare, makes the default
  placement's level at 1 kHz match the internal-mic path within ±1 dB.
  Mixing mic and pickup therefore never jumps in loudness.

**Mix.**
- `ac_mic_mix` (0 = today's piezo/internal blend alone, 1 = external
  mics alone) mixes linearly after `circuit.process` and before the
  input gain. A microphone does not go through the guitar's
  electronics.
- The mix fades over 20 ms.
- At 0 (and once smoothed there), `AcousticMicModel` is skipped
  entirely. This costs nothing and is bit-identical to today (MP-20).
- Mic 2 (`ac_mic_2_on`) sums with `ac_mic_blend`, equal-power, in mono.
  The instrument path is mono up to the pre-effects. A stereo spaced
  pair is out of scope.

When the cabinet is `AcousticDI`, `CabinetEngine` bypasses its placement
stage. Placement lives upstream, in `AcousticMicModel`.

## 4. Compatibility: legacy parameters and migration

`mic_position`, `mic_distance`, `mic_position_2` and `mic_distance_2`
stay in the parameter list at their current indices, with the same
ranges. The engine no longer reads them directly.

**Legacy to continuous mapping.** Every new mic is on speaker 1, with
`mic_tof_mode` = Aligned and `mic_level_match` = on.

| Legacy position | x | y | angle | rear |
|---|---|---|---|---|
| OnAxisCentre | 0 | 0 | 0 | off |
| OnAxisCapEdge | 0.35 | 0 | 0 | off |
| OffAxis45 | 0.35 | 0 | 45 | off |
| OffAxisEdge ("Cone Edge") | 0.90 | 0 | 0 | off |
| Rear | 0.35 | 0 | 0 | on |

| Legacy distance | cm |
|---|---|
| Close | 2.5 |
| Medium | 15 |
| Far | 30 |

**Continuous to legacy (the "mirror").**
- Position:
  - Rear if rear is on;
  - otherwise Cone Edge if u ≥ 0.62;
  - otherwise OffAxis45 if the angle is ≥ 22.5 degrees;
  - otherwise Centre if u < 0.175;
  - otherwise Cap Edge.
- Distance: Close below 6.1 cm, Medium below 21.2 cm, Far above
  (geometric midpoints).
- The mirror is written **only into serialised output**: preset JSON
  from `PresetManager` save, and the APVTS XML in
  `getStateInformation`. APVTS values are never touched. An older
  Luthier that loads a new preset therefore gets the nearest discrete
  choice (file-formats.md 0.3).

**When migration runs.** In
`MicPlacementMigration::apply (juce::ValueTree& paramsOrVar)`, called
from:
- the `PresetManager` load path;
- `LuthierAudioProcessor::setStateInformation`;
- snapshot recall (`Snapshots`);
- `PresetMorph` endpoint load.

Migration runs when the incoming data has `mic_position` but lacks
`mic_x` (mic 1), and separately `mic_position_2` without `mic_x_2`.

There is **no schema bump**. Presence of the key is the test. It also
covers host chunks and snapshots, which carry no schema, and the legacy
fields keep their meaning of a discrete placement.

For an old acoustic preset, a missing `ac_mic_mix` takes its default of
0, so it sounds as it does today.

**Legacy host writes.** Old sessions may still automate `mic_position`.
When `ParameterBridge` sees a legacy position or distance change, and
none of that mic's continuous parameters was written within ±250 ms
(the `lastWrite` stamps that `writtenSinceGuitarType` already uses), it
writes the mapped continuous values on the message thread. A restore
writes both at once and is left alone.

`readStructuralValues` no longer treats the four legacy parameters as
structural. The IR reload in `applyStructural` keys only on the cabinet,
speaker and mic type.

## 5. Engine design and insertion points

**New class `MicPlacementStage`.** Files:
`Source/DSP/Amp/MicPlacement.{h,cpp}`, which also hold
`MicPlacementModel`. `CabinetEngine::MicPath` owns one.

- **Filter chain.** Seven TPT SVFs: proximity peak, presence peak, cap
  peak, surround peak, two HF shelves, and the rear low-pass. Rear
  enters as a wet/dry `rearAmount` crossfade.
- **Gain and polarity.** A signed gain, smoothed over 20 ms, so a
  polarity flip passes through zero without a click.
- **Delays.** A floor-bounce delay tap, and the ToF line (dual-mic
  only).
- **New filter class.** `TptSvf` is added to `DspCommon.h` because the
  existing `Biquad` is not modulation-safe.
- **Allocation.** Delay buffers are sized in `prepare` for 12 ms at the
  current rate, which covers 200 cm of path plus the floor at 192 kHz.

The processing order in `CabinetEngine::processBlock` becomes:
1. convolution or fallback;
2. `MicPlacementStage::process` (bypassed for AcousticDI and for a user
   IR, see 9);
3. ToF, then `mic_phase_align`;
4. taps;
5. blend.

`prepareFallback` always uses the anchor terms, so the fallback and the
IR path share one placement stage.

Inputs `x`, `y`, `d` and θ are smoothed with `ExpSmoother` (30 ms).
`evaluate` runs every 32 samples, and SVF coefficients update from its
output.

Rear and speaker index are discrete switches. Their derived targets
glide over 20 ms: rearAmount 0 to 1, and the variation values ρ.

**New API:**
- `CabinetEngine::setMicPlacement (int slot, const MicPlacement&)`,
  where `MicPlacement` is a POD: `x`, `y`, `distCm`, `angleDeg`,
  `speaker`, `rear`.
- `setTimeOfFlightMode (TofMode)`.
- `setLevelMatch (bool)`.
- `setRoomMaterialForFloor (RoomMaterial, bool roomOn)`.
- `CabinetConfig` loses nothing. `LuthierEngine::reloadCabinetIrs`
  passes an anchor copy of the config (position OnAxisCapEdge, distance
  Close) to `IrLibrary::findCabIr`. Placement never triggers a reload
  (MP-13).

**ParameterBridge::applyToEngine** (block rate, in the cabinet
section): reads the new parameters through the existing `value()`, so
the mod matrix applies. It pushes them to `CabinetEngine`,
`AcousticMicModel` and `RoomEngine`.

**LuthierEngine**, step 4, acoustic branch: after `circuit.process`,
add `instrument = lerp (instrument, acMic.process (bodyData[i], stringSum[i]), mixSmoothed)`.
`acMic` is prepared in `LuthierEngine::prepare`, and
`rebuildBodyFromSpec` passes it the landmarks and `f_air`.

**RoomEngine interplay:**
- New `RoomEngine::setCloseMicDistance (double metres)`, fed the
  blend-weighted distance of the active mics.
- Close-mic room bleed: `b = clamp(0.5 (d / d_c)^2, 0, 0.5)`. The
  critical distance `d_c` per room size:

  | Room | d_c (m) |
  |---|---|
  | IsoBooth | 0.3 |
  | SmallBooth | 0.5 |
  | SmallStudio | 0.9 |
  | LargeStudio | 1.4 |
  | LiveRoom | 2.0 |
  | ConcertHall | 2.8 |
  | Cathedral | 3.0 |

- Effective wet is `1 - (1 - blend)(1 - b)`, smoothed.
- A mic backed off to 1 m hears the room, as a real one does. The Room
  Blend control still means the room mics.
- Aux 5 (the room tap) excludes the bleed.
- With the room off, there is no bleed.

**Routing:**
- Aux 3 and 4 carry each mic after its placement and ToF, as before.
- On acoustic guitars, Aux 3 and 4 carry the acoustic external mics.
  routing-io.md must note this (coordination).

## 6. UI

Location is per gui-integration.md. No Column-4 tab is added, because
that tab order is fixed.

### 6.1 Advanced: Column 3 CAB section (canonical)

In `AdvancedPanel`, the "Cabinet and Mic" section replaces the Position
and Distance combos with a new `MicPlacementView`, sized 236 x 190 px.
Top to bottom:

```
+-- CABINET AND MIC ------------------------------ [⤢] [v] --+
| Cabinet [4x12      v]  Speaker [British 60 W Modern v]  Age (o)    |
| Mic 1 [Classic Dyn v]  [Mic 2 ■]  Mic 2 [Ribbon v]         |
| +--------------------------------------------+ +--------+  |
| |   cone face of the miked speaker           | |cab thmb|  |
| |   rings: Cap / Cap Edge / Cone / Edge      | | 2x2    |  |
| |   (1) handle   (2) handle                  | +--------+  |
| +--------------------------------------------+             |
| Dist 1 (o) Angle 1 (o) [Rear]  | Dist 2 (o) Angle 2 (o)    |
| Quick: [Cap Edge v][Close v]   ~ -3 dB @5k mini-plot ~~~~  |
| Blend (o) Width (o) Phase (o)  ToF [Aligned|Physical] [LvL]|
+------------------------------------------------------------+
```

- **Face.** The face shows the miked speaker. The thumbnail picks
  another speaker. Clicking a speaker in it moves the focused mic there.
- **Quick combos.** These are the legacy parameters' canonical controls
  (gui-integration.md 0.1). Picking one writes the legacy value and the
  mapped continuous values in one gesture.
- **Mini-plot.** 236 x 36. The placement delta for mic 1, and for mic 2
  when it is on.
- **Acoustic guitars.** The same section is titled "MICROPHONES". The
  face becomes a top-down outline of the current body, drawn with
  `GuitarRenderer`'s cached body path, with landmark ticks. The row
  gains a **Pickup <-> Mic** knob (`ac_mic_mix`) and a Mic 2 toggle
  (`ac_mic_2_on`).

### 6.2 Expanded editor

`MicPlacementEditor` opens from the section header's ⤢ button, or by
double-clicking the face. It takes over Columns 3 and 4 the way the
Workshop does (gui-integration.md 6), and Column 4's tab strip stays
visible. Escape or ⤢ closes it and returns focus to the button.

```
+-- MIC PLACEMENT  [4x12 · British 60 W Modern]   [Grille ○] [Reset] [x] --+
|                                        | SIDE VIEW               |
|   full cabinet front: tolex, piping,   |  cone profile ) (mic)-> |
|   grille cloth (toggle to see through),|  ruler 0 1 2.5 5 10 15  |
|   every speaker drawn; miked speaker   |        30 50 100 cm     |
|   highlighted; mic 1 / mic 2 handles   |  angle handle on tail   |
|   drawn as the mic model seen head-on  +-------------------------+
|                                        | MIC 1 card | MIC 2 card |
|                                        | model, rear, speaker,   |
|                                        | u / cm / deg readouts   |
+----------------------------------------+-------------------------+
| RESPONSE  20 Hz ........................................ 20 kHz  |
|  mic 1 (accent), mic 2 (secondary), sum (text), drag-start ghost |
+-------------------------------------------------------------------+
```

**Rendering.** Static layers are cached per (cabinet, size, scale, grille
state). Only handles and the plot repaint while dragging.

- **Cabinet.** Tolex grain and piping, a basket-weave grille
  (procedural pattern, colour per cabinet: salt-and-pepper for the
  vintage 4x12, black for modern, oxblood for open-back 2x12). The
  grille draws at 25% opacity when "Grille" is off.
- **Cone.** Radial-gradient paper with concentric ribs, a surround roll,
  frame and bolts, and a dust-cap dome with a specular highlight.
- **Mics.** Each is drawn at its true relative size, as a *generic*
  silhouette per `MicType`:
  - Classic Dynamic: small ridged round grille;
  - Broadcast Dynamic: foam ball;
  - Wide Dynamic: oval slotted body;
  - Large Condenser: large rounded mesh;
  - Ribbon: rectangular slotted body;
  - Studio Condenser: squared mesh;
  - Kick Dynamic: egg.
  No trade dress or brand marks (TrademarkTests).
- **Angle.** Shown by elliptical foreshortening plus an aim arrow.
- **Labels.** Handles carry the digits "1" and "2", as well as colour
  (accent and secondary), so they read without colour vision.

**Interactions.**
- **Drag a handle.** Magnetic snap to ring radii within 6 px, with a
  60 ms ease. Hold Alt to disable snapping.
- **Scroll wheel over a handle.** Changes distance (Shift for fine).
- **Side view.** Drag the mic horizontally for distance, or drag its
  tail for angle.
- **Double-click a speaker.** Moves the focused mic there.
- **Right-click a handle.** Opens the standard parameter menu
  (gui-integration.md 16) for X. The Y, Distance and Angle items are in
  a submenu.
- **Reset.** Returns the focused mic, or both, to the defaults in
  section 8.

**Response plot.** 256 log-spaced points, -24 to +12 dB, computed on the
message thread from `MicPlacementModel` at 30 Hz while dragging. When
Physical ToF is on and both mics are active, the sum curve shows the
comb notches. A readout reads, for example, "first notch 1.9 kHz".
When the anchor IR is loaded, the mic curves include its magnitude,
FFT'd once on load on the message thread and cached. Otherwise they
show the delta alone, titled "Change from Cap Edge, 2.5 cm".

**Status chips:**
- "Mic in its null": a figure-8 near 90°, or a cardioid past 150°.
- "Driven by automation": a lane on any of the mic's parameters wrote
  within 500 ms.
- "Placement baked into your IR": a user IR is in the slot (9).

### 6.3 Easy Mode

The rig strip's Cabinet card (gui-integration.md 3.2 item 5) gains a
120 x 72 pad labelled **Mic: bright <-> warm**:
- Horizontal: electric `u` = 0 to 0.9, keeping the handle's current
  direction; acoustic `along` = 4 to 1.
- Vertical: distance 1 to 100 cm on a log scale. The top is labelled
  Close.
- The pad moves mic 1. Mic 2, when on, shows as a hollow non-draggable
  ghost dot, so Easy never hides it (gui-integration.md 0.3).
- On acoustics, a **Pickup <-> Mic** knob sits beside the pad.
- Double-clicking opens the expanded editor as an overlay, as the
  Workshop does in Easy.

### 6.4 Empty states and errors

| Condition | Message and behaviour |
|---|---|
| Cabinet off | "Cabinet is off. Turn it on to place mics." The face is still drawn. |
| AcousticDI with a non-acoustic guitar | "Acoustic DI has no speaker to mic. Pick a cabinet." |
| Acoustic with `ac_mic_mix` = 0 | Handles drawn hollow: "External mics are silent. Raise Pickup <-> Mic to hear them." |
| User IR in a slot | That mic's handle hidden: "Placement is baked into your IR." |

### 6.5 Options

Options -> APPEARANCE gains:
- "Snap mics to landmarks" (default on);
- "Show mic response plot" (default on).

Both are `UiPreferences` (`ui_prefs`), not preset data. The grille
toggle and editor-open state are `UiState` (per window, not persisted
in presets). Add `micGrilleVisible` and `micFocusedHandle`.

## 7. Parameters

All are appended **at the end of the list**, in this order, after
whatever is last at merge time (currently the MODEL-GAPS block ending
`aux1_pre_circuit`).

| # | id | name | range (stock / advanced) | default |
|---|---|---|---|---|
| 1 | `mic_x` | Mic 1 X | -1.4 to 1.4 u (both) | 0.35 |
| 2 | `mic_y` | Mic 1 Y | -1.4 to 1.4 u (both) | 0 |
| 3 | `mic_dist` | Mic 1 Distance | 0 to 100 / 0 to 200 cm, skew 0.4 | 2.5 |
| 4 | `mic_angle` | Mic 1 Angle | 0 to 90 / 0 to 180 deg | 0 |
| 5 | `mic_speaker` | Mic 1 Speaker | int 1 to 8 | 1 |
| 6 | `mic_rear` | Mic 1 Rear | bool | false |
| 7 | `mic_x_2` | Mic 2 X | as #1 | 0.35 |
| 8 | `mic_y_2` | Mic 2 Y | as #2 | 0 |
| 9 | `mic_dist_2` | Mic 2 Distance | as #3 | 15 |
| 10 | `mic_angle_2` | Mic 2 Angle | as #4 | 45 |
| 11 | `mic_speaker_2` | Mic 2 Speaker | int 1 to 8 | 1 |
| 12 | `mic_rear_2` | Mic 2 Rear | bool | false |
| 13 | `mic_tof_mode` | Mic Time of Flight | Aligned, Physical | Aligned |
| 14 | `mic_level_match` | Mic Level Match | bool | true |
| 15 | `ac_mic_mix` | Pickup / Mic Mix | 0 to 1 | 0 |
| 16 | `ac_mic_along` | Ac Mic 1 Along | 0 to 4 landmarks | 3.6 |
| 17 | `ac_mic_across` | Ac Mic 1 Across | -1 to 1 | 0 |
| 18 | `ac_mic_dist` | Ac Mic 1 Distance | 0 to 100 / 0 to 300 cm, skew 0.4 | 20 |
| 19 | `ac_mic_angle` | Ac Mic 1 Angle | 0 to 90 / 0 to 180 deg | 15 |
| 20 | `ac_mic_2_on` | Ac Mic 2 | bool | false |
| 21 | `ac_mic_along_2` | Ac Mic 2 Along | 0 to 4 | 1.2 |
| 22 | `ac_mic_across_2` | Ac Mic 2 Across | -1 to 1 | -0.4 |
| 23 | `ac_mic_dist_2` | Ac Mic 2 Distance | as #18 | 30 |
| 24 | `ac_mic_angle_2` | Ac Mic 2 Angle | as #19 | 0 |
| 25 | `ac_mic_blend` | Ac Mic Blend | 0 to 1 | 0.5 |

**Defaults.** Mic 2's defaults equal the mapping of the current legacy
mic 2 defaults (OffAxis45, Medium). A fresh instance therefore sounds
as it does today.

**Ranges.** Distances and angles are `PhysicalRange`s in a new
`RangeFamily::mic`. The preset `ranges.families` gains `"mic"`
(coordinate with advanced-ranges.md and file-formats.md 2).

**Automation.** Every parameter is automatable. The continuous ones
(1-4, 7-10, 15-19, 21-25) are mod-matrix destinations. They are
automatable in Free too (section 12).

## 8. Serialization, undo, accessibility

**State.**
- All 25 parameters round-trip in presets, host state, snapshots and
  morph endpoints through the existing parameter maps.
- Section 4 covers migration and the mirror.
- The IR anchor path is derived, never stored.
- Reset defaults are the table in 7.

**Undo** (action-and-undo.md):
- A handle drag is one class 3.5 entry, multi-target (x and y, section
  6), described as "Move Mic 1 from Cap Edge, 2.5 cm to Cone +0.04,
  2.5 cm". It is taken inside one gesture on both parameters.
- Side-view drags and wheel changes are class 3.1, and group within
  200 ms.
- Keyboard nudges group within 200 ms per parameter.
- A Quick combo is a class 3.2 entry that includes its mapped writes.
- Speaker change and rear are toggles (3.3).
- Reset is one multi-target entry.
- Legacy-automation mappings, modulation and host writes make no
  entries (11 of that spec).

**Accessibility.**
- Each handle is focusable and in Tab order: mic 1, then mic 2, then
  the side view, then the cards.
- Each handle exposes an accessible group, "Mic 1 placement", with four
  child sliders bound to X, Y, Distance and Angle.
- The value text reads, for example, "Mic 1, Classic Dynamic, near Cap
  Edge, 2.5 centimetres, 0 degrees, speaker 2 of 4".
- Keys while a handle has focus:

  | Key | Action |
  |---|---|
  | Arrows | x/y by 0.01 u (Shift 0.002, Ctrl 0.05) |
  | PageUp / PageDown | Distance ±1 cm (Shift 0.1, Ctrl 5) |
  | Alt+Up / Alt+Down | Angle ±1° (Shift 0.2°) |
  | N / Shift+N | Next or previous snap point |
  | R | Toggle rear |
  | Home | Reset this mic |
  | Enter | Opens the expanded editor from the compact view |

  None of these collides with a global binding (gui-integration.md 17).
- Under reduced motion there are no snap eases. The plot updates on
  release instead of live.
- The Easy pad is one focusable control, with arrows for its two axes.

## 9. Interactions with other features

- **Tone Match.** A user IR in cab slot 1 or 2 already has a placement
  baked in. That mic's placement stage is bypassed (identity) and its
  handle is hidden. Cab Match measures the whole rig, placement
  included. Non-anchor factory IRs are no longer auto-loaded. They stay
  browsable in the TONE MATCH IR library and serve as the fidelity
  fixtures for MP-03.
- **Snapshots and morph.** The continuous parameters morph smoothly: a
  morph *is* a mic move. Speaker index and rear switch per PresetMorph's
  discrete rule, crossfaded by the engine.
- **Mod matrix, MIDI Learn and host automation.** All work, with the
  click-free guarantee of 0.4 (MP-11, MP-36).
- **Randomize.** The header dice keeps `u` ≤ 1.0 and distance ≤ 30 cm
  when "randomize respects stock range" is on (coordinate with the
  randomizer).
- **No audio-path interaction:**
  - Techniques, rhythm engine, tune builder and strum dynamics: the
    string path is unaffected.
  - The feedback loop: it taps the amp before the cabinet.
  - MIDI export: no new event class. Mic parameters are rig state,
    exported as midi-export.md already treats amp parameters.
  - Workshop: body swaps move the acoustic landmarks and `f_air`. Mics
    keep their landmark units, so "12th fret" stays at the 12th fret.
- **Noise floor.** Amp hiss enters before the cabinet and is shaped by
  placement, correctly.
- **Room.** Covered in section 5. The floor ρ follows the room
  material.
- **Factory content.** Acoustic factory presets should set `ac_mic_mix`
  to about 0.5 with sensible placements (factory-content.md
  coordination). Free's IR subset needs only the anchors (editions.md
  2.2 row "Factory IRs" to be restated).

## 10. Failure modes

| Case | Response |
|---|---|
| No anchor IR found | Procedural fallback plus the same placement stage. Sounds continuous. The existing missing-IR banner. |
| NaN or out-of-range automation | `jlimit` in the bridge, `sanitise` on the stage output, SVF state reset if non-finite. |
| Ribbon at 90° with level match | Makeup capped at +18 dB. The "null" chip. |
| Guitar family or cabinet switch mid-drag | Gesture ends, undo entry commits, view rebuilds, handles re-resolve (MP-32). |
| Legacy lane automating while the user drags | The last writer wins. The chip shows who. |
| Sample rate change | Delay buffers re-prepared, `evaluate` re-run. The response is identical in Hz (MP-23). |

## 11. Performance budget

- `CabinetEngine` (2 mics, placement, ToF, floor) rises from 0.4 to
  **0.5 units**. The placement stage is at most 0.05 units per mic.
  `evaluate` runs about 40 transcendental operations per mic per 32
  samples.
- `AcousticMicModel`: **0.12 units** with 2 mics, and **0** at
  `ac_mic_mix` = 0.
- Room bleed: 0 extra, because it reuses the existing wet signal.
- No allocation or lock on the audio thread. Placement never swaps an IR
  (MP-12, MP-13).
- UI: plot at most 0.5 ms per frame. While dragging, the face repaints
  at 60 fps within gui-engine-dataflow.md's 2 ms overlay budget at
  1920x1080. Idle cost is 0.
- Update performance-budget.md 1's table accordingly.

## 12. Editions

**Both editions, in full.** The cabinet and its mics are "rig"
fundamentals (editions.md 0.2; row "Cabinet: model, 2 mics, blend,
phase, delay" is Both). Placement replaces controls Free already has.

Gating the parameters would also break editions.md 0.1. Free would
neutralise the placement, and a Free preset would sound different in
Pro. There is no `ProFeatureGuard` entry.

## 13. Tests

Unit and DSP tests go in a new `Source/Tests/MicPlacementTests.cpp`.
GUI tests run under xvfb in the style of `EditorTests.cpp`. Combination
tests use `ComboHarness.h`. All are in `LuthierTests`.

1. **MP-01** Layout: the 25 parameters sit at the end in table order,
   with the ids, ranges and defaults of section 7. Every pre-existing
   parameter keeps its index.
2. **MP-02** Identity: at the anchor, the stage's response is within
   ±0.05 dB from 20 Hz to 20 kHz. The rendered output nulls against the
   stage bypassed to ≤ -90 dBFS.
3. **MP-03** Legacy fidelity: covers every shipped (cabinet, speaker,
   mic) and each legacy (position, distance) with an IR file. Compare
   the anchor IR plus placement against that IR, both 1/3-octave
   smoothed and level-normalised at 1 kHz. Error must be ≤ 1.0 dB from
   100 Hz to 5 kHz at Close, ≤ 2.0 dB at Medium, and ≤ 3 dB from 5 to
   10 kHz.
4. **MP-04** Migration map: for all 5 x 3 legacy combinations on both
   mics, a preset lacking `mic_x` loads with exactly the values in the
   section 4 table.
5. **MP-05** Migration trigger:
   - present keys are never overwritten;
   - a legacy host write alone (more than 250 ms from any continuous
     write) maps;
   - a simultaneous restore does not map.
6. **MP-06** Mirror: saving at (u = 0.7, 20 cm) writes Cone Edge and
   Medium into the file and the state XML. The APVTS legacy values are
   unchanged.
7. **MP-07** Radius: with a pink-noise input, the 4-8 kHz / 1 kHz energy
   ratio falls strictly as `u` goes 0 → 0.35 → 0.62 → 0.9, at 2.5 cm,
   0°, for every speaker and mic.
8. **MP-08** Angle:
   - the cardioid HF ratio falls monotonically from 0 to 90°;
   - the ribbon at 90° is ≥ 20 dB down broadband with level match off;
   - the ribbon at 180° has inverted polarity (sign of the impulse
     peak).
9. **MP-09** Proximity: the 100 Hz / 1 kHz ratio falls monotonically
   from 0.5 to 100 cm. The difference between 2.5 and 30 cm is
   `0.9 × proximityDb` ±0.3 dB.
10. **MP-10** Beaming: at 100 cm, the HF-ratio difference between u = 0
    and u = 0.9 is < 1.5 dB. At 2.5 cm it is > 6 dB.
11. **MP-11** Click-free: with a 200 Hz sine input, sweep `mic_x` from
    -1.4 to 1.4 in 50 ms, `mic_dist` from 0 to 100 cm in 50 ms, and
    `mic_angle` from 0 to 90° in 50 ms. Energy from 8 to 16 kHz stays
    ≤ -80 dBFS throughout.
12. **MP-12** Real-time safety: with all 25 parameters automated every
    block for 60 s, the heap hook and `ThreadProbe` record zero
    allocations, locks and file accesses in `processBlock`.
13. **MP-13** `CabinetEngine::getIrLoadCount()` is unchanged across 1000
    placement changes. It increments once on a cabinet change.
14. **MP-14** ToF:
    - Physical, mic 1 at 2.5 cm and mic 2 at 30 cm: the mic-2 tap lags
      by Δp / c ±1 sample, and the first sum notch is at 1 / (2Δτ)
      ±3%;
    - Aligned, same placements: no notch deeper than 3 dB from 100 Hz
      to 5 kHz.
15. **MP-15** Slew: a jump from 2.5 to 100 cm in one block holds the
    delay rate at ≤ 0.0025 samples per sample. A 1 kHz sine deviates
    ≤ 5 cents.
16. **MP-16** Rear toggle during a 200 Hz sine:
    - the MP-11 energy bound holds;
    - Physical mode: polarity is inverted after settling;
    - Aligned mode: polarity is not inverted.
17. **MP-17** Level match on: the 1 kHz level holds within ±0.5 dB from
    0.5 to 100 cm. Off: it drops ≥ 18 dB at 100 cm on a 12" speaker.
18. **MP-18** Room bleed:
    - SmallStudio, room blend fixed: output wet energy at 100 cm is
      ≥ 6 dB above that at 2.5 cm;
    - room off: no difference beyond the placement terms;
    - the Aux 5 tap excludes the bleed.
19. **MP-19** Speaker variation:
    - speaker 1 nulls against the reference;
    - speaker k ≠ 1 deviates ≤ 1 dB and is bit-identical across runs;
    - index 5 on a 2x12 behaves as speaker 1 and keeps its stored value.
20. **MP-20** Acoustic off: with `ac_mic_mix` = 0, an acoustic factory
    preset renders bit-identical to the pre-feature build's reference
    render. `AcousticMicModel` process count is 0.
21. **MP-21** Acoustic position, at 10 cm:
    - Soundhole's energy around `f_air` (×0.8 to ×1.25), relative to
      1 kHz, is ≥ 6 dB above 12th Fret's;
    - 12th Fret's 4-8 kHz ratio is ≥ 3 dB above Soundhole's.
22. **MP-22** Landmarks:
    - along = 4 maps to scale / 2 within 1 mm on parlor, dreadnought
      and jumbo;
    - a no-soundhole body has no air peak at along = 3;
    - a Workshop body swap keeps `along`.
23. **MP-23** Determinism:
    - a `LuthierRender` of a preset with an LFO on `mic_x` is
      bit-identical over two runs;
    - `evaluate` curves at 44.1, 48 and 96 kHz agree within 0.2 dB
      below 16 kHz.
24. **MP-24** CPU (offline renderer, 60 s):
    - `CabinetEngine` with 2 mics ≤ 0.5 units;
    - `AcousticMicModel` with 2 mics ≤ 0.12 units;
    - a regression gate per performance-budget.md 0.3.
25. **MP-25** Plot equals engine: `PlacementResponse::magnitudeDb`
    matches a sine-sweep measurement of the stage within ±0.3 dB from
    40 Hz to 16 kHz.
26. **MP-26** GUI, Advanced CAB section:
    - `MicPlacementView` and the Quick combos exist and are attached;
    - a synthesized drag of handle 1 from Cap to Edge changes `mic_x`
      and `mic_y` and pushes exactly one undo entry;
    - undo restores the values exactly.
27. **MP-27** GUI keyboard, with a handle focused:
    - Right ×10 moves x by +0.10 (±1e-6); Shift moves by 0.002;
    - PageUp adds 1 cm; Alt+Up adds 1°;
    - N snaps to the next ring; Home resets;
    - nudges within 200 ms form one undo entry.
28. **MP-28** GUI accessibility:
    - each handle exposes a group with 4 child sliders and the value
      text of section 8;
    - the Tab order is as specified;
    - no global shortcut fires while a handle has focus for the keys of
      section 8.
29. **MP-29** GUI snap: a drag released within 6 px of the Cap Edge ring
    sets u = 0.35 exactly. With Alt held, it does not snap.
30. **MP-30** GUI Easy pad:
    - the pad is present in the Cabinet card;
    - a left-to-right drag raises u monotonically from 0 to 0.9;
    - a vertical drag maps to 1-100 cm on a log scale;
    - an acoustic guitar shows the along mapping and the Pickup <-> Mic
      knob;
    - the mic 2 ghost is drawn when dual mic is on.
31. **MP-31** GUI editor:
    - ⤢ opens `MicPlacementEditor` over Columns 3 and 4, and the
      Column 4 tab strip stays visible;
    - Escape closes it and returns focus to ⤢;
    - Easy double-click opens it as an overlay.
32. **MP-32** GUI switches:
    - acoustic ↔ electric swaps body and cabinet views with no stale
      handles;
    - a family switch mid-drag commits one entry;
    - 4x12 → 1x12 with speaker 3 draws speaker 1.
33. **MP-33** User IR: loading a user IR into cab slot 1 bypasses mic 1's
    stage (null ≤ -90 dBFS against an identity stage), and the view
    shows the baked notice.
34. **MP-34** Reflow and motion:
    - the view and editor do not clip at widths 1280, 1600 and 2560
      and scales 100, 150 and 200%;
    - with reduced motion on, there are no snap animations.
35. **MP-35** Combination:
    - a snapshot morph between two placements gives a continuous
      `mic_x` path and passes the MP-11 bound;
    - presets, host state and snapshots round-trip all 25 parameters
      exactly;
    - in the Free build, the parameters are automatable and audible.
36. **MP-36** Combination: an LFO at 5 Hz, depth 0.5, on `mic_x`, plus
    `mic_dist` automation, while the rhythm engine strums. This passes
    the MP-11 bound and the MP-24 CPU bound.
37. **MP-37** Every new user-visible string (mic silhouettes' names,
    chips, labels) passes `TrademarkTests` and exists in the
    localisation catalog.
