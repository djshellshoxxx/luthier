# TOUCH BOARD (CHAPMAN-STICK-STYLE) SPEC

A solid beam with 8–12 strings split into two **zones** - a *bass* zone
tuned in ascending fifths and a *melody* zone tuned in descending
fourths, lowest strings of both zones in the centre of the board - played
**only by tapping** with both hands approaching from opposite sides, and
wired to a stereo bass/melody output. Factory name "Touch Board"
(`factory-content.md` 0.1: no trademarks).

Research: `docs/research/INSTR_chapman_stick.md`. **All sourced values
are search-snippet level; octave placement of every tuning is inferred;
no Stick has published measurements.** See §12.

Tags: **M** documented, **I** inferred (formula given), **D** design
default.

## 0. Ground rules

1. **Every note is a tap.** There is no pick and no pluck on this
   family by default. The tap is a physical boundary event
   (§3), not a scaled pluck: `Excitation::Kind::Tap` today is a pluck
   with `pluckLen × 0.50, gain 0.42` (`Source/DSP/String/Excitation.cpp:114`),
   and this spec replaces it for every family.
2. **Two zones, two instruments, one beam.** Zones have their own
   string subset, tuning logic, hand, pickup, tone and output channel.
   They share the beam (and so its dead spots) and cross-talk only
   through it.
3. **The illustration is authoritative** (`guitar-illustration.md`
   0.5): the drawn zone split, damper and pickup module are what the
   engine models.
4. **Builds on, does not duplicate, `two-hand-tapping.md`.** That spec's
   `TapEngine`, `TapGesture`, pull-off and multi-finger rules are
   reused verbatim; this spec supplies the physical excitation they
   call, the zone router in front of them and the `touch` family
   defaults.
5. **Honest magnitudes.** A tap is quieter than a pluck and has less
   dynamic range (§3.3). The pickups are "very sensitive" (M snippet)
   because the instrument needs them to be; the model reproduces the
   low source level and makes it up in the pickup gain, not in the
   excitation.

## 1. Family `touch` (`README.md` 1.3)

| Field | Value |
|---|---|
| `family` | `touch` (new; `guitar-illustration.md` 3 table row) |
| Default strings | 10 (5 melody + 5 bass) |
| Supported | 8, 10, 12 (`kMaxStrings` = 12, `DspCommon.h:21`) |
| Default scale | 914.4 mm (36", M); 863.6 mm (34", M, older) |
| Default excitation | `tap`, both hands |
| Template | `touch_default_template.luthierguitar` = Touch Board 10 |
| Body | none: a `body` part of chambering `beam` (§5) |
| Techniques shown | Tapping sub-tab only; strum, pick, slap, slide groups hidden (`gui-integration.md` 0.7 rule, same as bass-only groups) |

`baseTypeFor` (`PartAcoustics.cpp:100`) gains `touch → GuitarType::TouchBoard`
(new compiled fallback entry: 10 strings, 914 mm, fretless = false,
bodyShape `SolidThin`, no amp, DI cabinet bypass).

## 2. Zone model

### 2.1 Data: guitar-level `zones`

Physical string order across the board, string 0 = the melody-side
edge (engine convention "string 0 = treble side", `README.md` 1.4).
This makes the tuning **non-monotonic in index** (C1 sits beside F#2 in
the middle), which the `tuning` field already allows.

```json
"zones": [
  { "id": "melody", "strings": [0, 4], "hand": "left",
    "interval_semitones": -5, "output": "right", "pickup": "neck" },
  { "id": "bass",   "strings": [5, 9], "hand": "right",
    "interval_semitones": 7,  "output": "left",  "pickup": "bridge" }
]
```

- `strings` is an inclusive index range; ranges must not overlap and
  must cover every string (validator error otherwise).
- `interval_semitones` is informational (voicer hint, UI label); the
  pitches come from `tuning.open_hz`.
- `hand` is the default for that zone; either hand may play either zone
  (§2.3 hand overlap). Left hand on melody and right hand on bass is the
  conventional Free Hands posture (D; research §6 describes both hands
  from opposite sides but not the assignment).
- `pickup` names the pickup slot that senses this zone (§4).
- A `zones` array on a non-`touch` guitar is legal and ignored except by
  the router (a guitarist may split a 12-string for fun; ground rule 4
  of `guitar-workshop.md`).

### 2.2 Router: incoming note → zone → string/fret

New `ZoneRouter`, message-free, audio thread, in front of
`TechniqueEngine`. Modes (`touch_input_mode`):

| Mode | Rule | Default |
|---|---|---|
| `split` | MIDI note < `touch_split_note` → bass zone, else melody | **yes**, split 55 (G3) D |
| `channel` | channel `touch_bass_channel` (default 1) → bass, `touch_melody_channel` (default 2) → melody | — |
| `per_string` | existing `ControllerMode::perChannel` profile: channel → string directly (MIDI guitar/hex or a touch controller with string outputs) | — |
| `mpe` | existing MPE: member-channel notes routed by `split` rule; per-note bend/pressure retained | — |

Within the zone, fret/string choice uses the existing voicer restricted
to the zone's string subset plus one new cost term:

```
cost = voicerCost + w_hand × |fret − handPosition[zone]| + w_span × max(0, spanFrets − maxSpan)
```

- `handPosition[zone]` is a smoothed centroid of that hand's held frets
  (τ 400 ms, D).
- `maxSpan` 5 frets below fret 7, 6 above (D; longer scale, no body in
  the way, M "fingers perpendicular").
- **Finger budget**: at most 4 simultaneous held notes per hand (D:
  thumb not used in Free Hands, research §6). A 5th note steals the
  oldest in that zone (a real player must lift a finger); counted, never
  silent.
- A note unplayable in its zone (below the zone's lowest open string)
  is offered to the other zone if `touch_zone_overflow` (default on);
  otherwise dropped and counted.

### 2.3 Hand overlap

The ranges overlap roughly E2–E3 (research §7). A note in the overlap
is placed by the mode rule; in `channel` and `per_string` modes the
player decides. Both hands on one zone is legal (two-hand melody), and
then `handPosition` is tracked per hand, not per zone.

## 3. Tap excitation (replaces `Excitation::Kind::Tap` for all families)

### 3.1 Physics

The string rests at action height `h` above fret `k`. A fingertip lands
a distance `x0` behind the fret (nut side) and drives the string onto
the fret in a push time `τ`. The vibrating segment is fret → bridge,
length `L_k`.

1. **Boundary step (dominant).** Before contact the bridge-side segment
   lies on the line from height `h` at the fret to 0 at the saddle; after
   contact its equilibrium is the line from 0 to 0. Relative to the new
   equilibrium the initial shape is a ramp `u0(x) = h (1 − x / L_k)`
   (`x` from the fret). Its modal displacement amplitudes are
   `a_n = 2h / (nπ)` - **1/n, with no pluck-position comb** (I, exact
   for an instantaneous step). Velocity amplitude ∝ ω_n a_n is **flat
   in n**: brighter than any pluck, whose amplitudes are
   `∝ sin(nπp)/n²`.
2. **Finite push time.** The step is not instantaneous; the boundary
   moves over `τ = h / v_f` (v_f finger speed at contact). A boundary
   motion of duration τ low-passes the ramp's spectrum: one-pole
   `fc = 1 / (2π τ)` (I; first-order approximation of a linear ramp's
   sinc envelope). With `h` 1.0 mm (D) and `v_f` 0.3–2.5 m/s (D),
   `fc` ≈ 50–400 Hz: above fc the tap rolls off 1/n², i.e. like a pluck
   but **without comb notches**, and harder taps are brighter.
3. **Impact term.** The finger's momentum at contact adds a velocity
   distribution confined near the fret end; in the waveguide this is a
   short velocity pulse injected `x0` from the termination, whose modal
   weights `sin(nπ x0/L_k)` rise ~6 dB/oct up to `n ≈ L_k/(2 x0)`
   (research §6, I). Level ∝ `v_f` (D gain `k_impact` §3.4).
4. **Fret collision.** The string meets a hard fret at speed; that is
   `fret-buzz.md`'s contact with a small excess. It produces the tap
   "click" and rises with `v_f`. Reuse `NoiseEngine::FretBuzz` exactly as
   slap does (`bass-techniques.md` 2.1 step 3), excess
   `= k_click × v_f` (D).
5. **Nut-side segment.** Finger → nut is also struck, but the finger
   pad and the **foam damper at the nut** (M) kill it within a few ms.
   Modelled as a filtered noise "thump": 6 ms, low-pass 700 Hz, level
   −28 dB re the tone at `v_f` = 1 m/s (all D). No second waveguide.
6. **Hold.** While held, the finger sits behind the fret as a soft
   damper on the termination: termination reflection × (1 − ε_f),
   `ε_f = 0.004` per round trip (D). This gives the frequency-dependent
   sustain the research estimates (bass low notes 4–8 s, melody mid
   2–4 s; I): at 100 Hz the term alone would allow ~17 s, at 600 Hz
   ~2.9 s; the string's own loop filter sets the rest.
7. **Release.** Lifting the finger is the reverse step on the longer
   segment (next held fret on that string, or the open string). If
   another fret is held on the string: an ordinary pull-off
   (`two-hand-tapping.md` 2), lateral flick per its setting. If none:
   the open string is excited by the reverse ramp (amplitude `h`) but
   the **nut damper** is in the loop: loop gain × `(1 − g_damper)` with
   `g_damper` = 0.35 (D) ⇒ −ln(0.65) = 0.43 per round trip ⇒ 60 dB in
   ≈ 16 round trips: **≈ 55 ms on D4, ≈ 0.5 s on C1**. The short muted
   blip (research §6.6) is therefore a melody-zone fact; a low bass open
   string rings audibly longer through the foam unless `g_damper` is
   raised - which is a prediction the owner recording (§12.2) settles.

### 3.2 Waveguide realisation (cheap; no new string model)

`Excitation::trigger` for `Kind::Tap` fills the delay line with:

```
shape[i] = h_norm × (1 − i / D)                 // ramp, i = 0 at the fret end
shape    = onePoleLP(shape, fc = v_f / (2π h))  // push time
shape   += k_impact × v_f × pulse(i0 = x0/L_k × D, width 3 samples)  // impact
```

then DC-removes (the ramp's mean is the new equilibrium offset) and
hands on as today. `exactPluckComb` is **not** applied (there is no pluck
point). Fret click and nut thump go to `directInput`
(`harmonic-realism.md` 2 split), not through coupling.

### 3.3 Dynamics (the honest part)

Level ≈ `h × (1 + k_impact' × v_f)`: the ramp term does not scale with
velocity. Taps therefore have a **compressed dynamic range** versus
plucks: ~14 dB across MIDI velocity 1–127 (D) against the pluck path's
existing range. Loud taps get louder mainly through the impact and
click terms, i.e. they get brighter as much as louder. This is a
prediction to validate, not a measured fact (§12).

### 3.4 Parameters (Tapping sub-tab; family-independent)

| ID | Range | Default | Tag |
|---|---|---|---|
| `tap_action_mm` (h) | 0.3 – 3.0 | 1.0 (touch), from `setup` action elsewhere | D |
| `tap_finger_offset_mm` (x0) | 2 – 20 | 8 | I (research 3–15) |
| `tap_velocity_curve` | existing tap strength curve | — | — |
| `tap_v_min`, `tap_v_max` m/s | 0.1 – 5 | 0.3, 2.5 | D |
| `tap_impact` (k_impact) | 0 – 1 | 0.35 | D |
| `tap_click` (k_click) | 0 – 1 | 0.4 | D |
| `tap_hold_damping` (ε_f) | 0 – 0.02 | 0.004 | D |
| `nut_damper` (g_damper) | 0 – 0.9 | 0.35 on `touch`, 0 elsewhere | D |

On non-touch families `tap_action_mm` is read from the guitar's
`setup.action_*` interpolated to the fret, so a guitar with high action
taps louder and duller than a Touch Board - a real, derived difference.
Net new parameters: **+7** (`tap_velocity_curve` already exists).

## 4. Pickups, zones and stereo output

### 4.1 Per-zone sensing

Pickup placements gain an optional `string_mask` (array of 0/1, length
= strings; default all 1). The Touch Board fits **two** pickups at the
same `position_mm` (the module is one block, two coils/sections):
bridge slot = bass zone mask, neck slot = melody zone mask. A pickup
only sums strings in its mask. (Existing `PickupEngine` sums all strings;
the mask is applied at the string → pickup summation, block rate.)

| Field | Value | Tag |
|---|---|---|
| `position_mm` | 40 | D (research: last 30–50 mm, I) |
| Part | new `Touch Module Active` (per-zone, EMG-style active: `inductance_h` 0.1, `dc_resistance_k` 10, buffered `active: true`) | D; family M (EMG ACTV-2 option exists) |
| Alt part | new `Touch Module Passive` (Villex-style: 4 H, 7 k, 120 pF) | D; family M |
| Gain | `output_dbfs_reference` −6 (hotter than guitar pickups) | D, ground rule 5 |

### 4.2 Stereo zone split

New wiring `switching` value `zone_stereo` (`part-acoustics.md` 7 says
switching is the one topology field):

- **Stereo** (default): bass zone → left, melody zone → right, each
  with its own volume and tone (`volume_pot_ohm`, `tone_cap_f` per zone;
  the wiring part gains a second component set). TRS convention: bass
  on tip (M).
- **Mono**: sum with `mono_balance` trim (0–1, default 0.5, M: internal
  trim pots).
- Downstream: in `zone_stereo`, the amp/effects chain runs in **dual
  mono** (one instance per zone) when the rack supports it; otherwise
  the zones sum pre-amp and the split is re-applied post-rack as a hard
  pan. The dual-mono rack is a follow-up; v1 ships the post-rack pan
  and says so in the manual.
- `perString` / `full` bus layouts (`RoutingMatrix.h:33`) already give
  12 mono outs and need no change.

## 5. Beam, materials and dead spots

A Touch Board has no cavity and no plate: `chambering: "beam"` (new
value) ⇒ zero air modes, and the plate-mode bank is replaced by beam
bending modes that act **only through termination loss**
(research §5: "not a radiating body filter").

Beam modes (free-free Euler–Bernoulli, research §5, I):

```
f_n = (β_n L)² / (2π L²) × sqrt(E/ρ) × h / sqrt(12),   (β_n L)² = 22.37, 61.67, 120.9
```

Fields on the `body` part: `beam_length_mm` (1200, D), `beam_thickness_mm`
(19.05 aluminium M; 25 wood D). These f_n feed **`composite-neck.md`
2.3's** mobility-to-loss mechanism unchanged (w(x) = beam mode shape at
the fret; Q from §2.2 there). So the Touch Board's dead spots, and the
difference between materials, come from the same code as a bass neck's.

| Material id (`README.md` 1.2) | ρ | E GPa | tanδ | f₁ (I, research) | Tag |
|---|---|---|---|---|---|
| `aluminium_6061` | 2700 | 69 | 1e-4 | 69 Hz | ρ/E M (handbook); tanδ D |
| `ironwood` | 1100 | 20 | 5e-3 | ~75 Hz (h 25) | I |
| `bamboo_laminate` | 700 | 12 | 7e-3 | 70–75 Hz | I |
| `polycarbonate` | 1200 | 2.4 | 1.5e-2 | ~25 Hz | ρ/E I; tanδ D |
| `cfrp_ud` (Stick XG-style) | 1550 | 130 | 2e-3 | ~145 Hz | I |

Consequence to test: f₁–f₂ (69–200 Hz) sit on the bass zone's C1–G2
fundamentals and their 2nd–3rd partials, so the Touch Board has
note-dependent sustain in the bass zone that a composite beam moves up
the neck (§10 test 9).

## 6. Strings and tunings

### 6.1 Tunings (guitar-level `tuning`, engine order string 0 = melody edge)

| Preset | String 0→4 (melody, 4ths down) | String 5→9 (bass, 5ths up from centre) | Tag |
|---|---|---|---|
| **Classic 10** | D4 A3 E3 B2 F#2 = 293.66, 220.00, 164.81, 123.47, 92.50 | C1 G1 D2 A2 E3 = 32.70, 49.00, 73.42, 110.00, 164.81 | notes M; **octaves I** |
| **Matched Reciprocal 10** | C4 G3 D3 A2 E2 = 261.63, 196.00, 146.83, 110.00, 82.41 | as Classic | notes M; octaves I |
| **Grand Classic 12 (6+6)** | G4 + Classic melody = 392.00 … | Classic bass + B3 246.94 | extra strings I |
| Baritone Melody | — | — | **not shipped**: interval unknown (gap) |
| Touch Bass 8 | — | — | **not shipped**: tuning unconfirmed (gap) |

For the 12-string the zone ranges become `[0,5]`, `[6,11]`; for 7+5,
`[0,6]`, `[7,11]` (M: 7+5 exists; pitches D).

### 6.2 String sets (proxy, I)

Stick Enterprises gauges were not obtained. Proxy from the Megatar
inverted-fifths set (research §4):

| Set | Melody (s0→s4) | Bass (s5→s9) |
|---|---|---|
| `Touch 10 Medium (proxy)` | .011 .012 .016 .029w .040w | .095w .080w .060w .030w .016 |

`winding_material: nickel`, `core: hex` (M snippet, Stick-style sets).
Tension fixtures from research §4 (I): bass C1 on .095w ≈ 24 lbf
(108 N); melody D4 on .010 ≈ 25.6 lbf (114 N) - the set here uses .011
on D4, ≈ 31 lbf by the same formula. The octave inference rests on this:
a .095 at C2 would need ~97 lbf.

Inharmonicity (I): melody B ≈ 4e-6 open; bass B ≈ 1.1e-3 open,
×4 at fret 12 - the bass zone's audibly stretched partials must be
produced by the existing `B ∝ d⁴E/(T L²)` path, no special case.

## 7. Parts (factory guitar `Touch Board 10`)

| Slot | Part | Key fields |
|---|---|---|
| `body` | new `Touch Beam Bamboo` | `chambering: beam`, `wood: bamboo_laminate`, `beam_length_mm: 1200`, `beam_thickness_mm: 25` |
| `neck` | new `Touch Board 36 10` | `scale_length_mm: 914.4`, `strings: 10`, `frets: 24`, `joint: through`, `truss: dual` |
| `fretboard` | integral (`wood: bamboo_laminate`, `radius_mm: 0` flat, M) | |
| `frets` | new `Stainless Rail` | `material: stainless`, `height_mm: 1.0` (D), `width_mm: 1.5` (D), profile pyramidal (M) |
| `nut` | new `Touch Nut + Foam Damper` | `material: graphite`, `damper: true`, `damper_strength: 0.35` → `nut_damper` |
| `bridge` | new `Touch Bridge 10` | per-string height (M), `mass_g: 120` D, `coupling: 0.6` D, `strings: 10` |
| `tuners` | existing `Modern Sealed 18 to 1` | D |
| `pickups` | `Touch Module Active` ×2 (masks per zone) | §4 |
| `wiring` | new `Touch Stereo Zones` | `switching: zone_stereo` |
| `strings` | `Touch 10 Medium (proxy)` | §6.2 |

Variants: `Touch Rail 10` (body `Touch Beam Aluminium`, h 19.05, integral
aluminium frets `material: aluminium_anodised` brightness 0.85 D), and
`Grand Touch 12`.

## 8. Presets

| Name | Guitar | Notes |
|---|---|---|
| **Touch Board Classic** | Touch Board 10 | Classic tuning, split 55, stereo, clean DI + plate |
| **Matched Reciprocal Groove** | Touch Board 10 | MR tuning, bass zone compressed 3:1, melody chorus |
| **Grand Touch Duo** | Grand Touch 12 | `channel` mode (ch1 bass / ch2 melody) for two-track MIDI |
| **Touch Rail Lead** | Touch Rail 10 | Melody zone through crunch amp, bass zone DI (post-rack pan, §4.2) |
| **Touch Solo Bass** | Touch Board 10 | Melody zone muted, bass zone only |

## 9. Illustration (`guitar-illustration.md`; new family row in §3)

**Touch Board** is drawn along the same horizontal axis as other
families, origin at the saddle (§1 there):

- **Beam**: a long rectangle, 1200 × ~90 mm (width D, research beam
  88.9 mm M for Railboard), rounded ends; no body outline, no soundhole.
  Wood fill with grain strokes along the length (bamboo: short node
  marks every ~40 mm at 4 % opacity); aluminium: flat `#B8BCC2` with one
  highlight line (§0.2 metal rule); polycarbonate: translucent smoke
  fill at 70 %.
- **Strings**: 10 or 12, evenly spaced; the two zones separated by a
  slightly wider centre gap (D), each zone's lowest string adjacent to
  the centre. Wound strings per §10 colours.
- **Frets**: thin bright rails, flat board (no radius shading).
- **Inlays**: per-zone dot rows (melody side and bass side mirrored),
  drawn in the zone's accent colour so the split is legible at 128 px.
- **Nut damper**: a dark foam strip across all strings just above the
  nut; hit-target → nut part inspector (`nut_damper`).
- **Pickup module**: one rectangle near the bridge spanning all
  strings, divided at the zone gap into two halves; each half is a
  separate hit target (bass pickup / melody pickup) and pulses with its
  zone's level in live overlay.
- **Belt hook / strap**: small circle on the rear face (outline only).
- **Headstock**: in-line tuners along both edges (5+5), drawn as the
  existing tuner glyphs.
- **Live overlays**: taps render as `two-hand-tapping.md` 6 square
  markers, coloured per hand (left = melody accent, right = bass
  accent); `handPosition` drawn as a faint bracket per hand.
- Thumbnail at 128 × 256 px: beam + zone colours only.

Family switch (`guitar-illustration.md` 12): switching *to* `touch`
replaces body, neck, bridge, nut, pickups, wiring and strings with the
template's; switching *away* drops `zones` and restores the target
family's excitation defaults (banner lists it).

## 10. Tests

1. **Round trip** of `Touch Board 10`, `Touch Rail 10`, `Grand Touch 12`.
2. **Tuning & order**: open strings sound §6.1 within 1 cent; string 4
   (F#2) and string 5 (C1) are adjacent and the voicer never assumes
   monotonic pitch.
3. **Router `split`**: MIDI 36 → bass zone, 67 → melody; zone
   overflow on/off behaves as §2.2.
4. **Finger budget**: 5 simultaneous notes in one zone ⇒ 4 sounding,
   counter increments, oldest stolen.
5. **Tap has no comb**: tap at fret 5, 1/5-length pluck on the same
   string; over partials 1–30 the pluck shows ≥ 3 notches > 15 dB, the
   tap none > 6 dB.
6. **Ramp law**: with `tap_impact` = `tap_click` = 0 and τ → 0
   (v_f = 50 m/s test hook), partial displacement amplitudes follow 1/n
   within 1 dB for n = 1..20.
7. **Velocity → brightness, not just level**: v_f 0.3 → 2.5 m/s raises
   the first-20-ms spectral centroid ≥ 1.8× while peak level rises
   ≤ 16 dB (the §3.3 prediction; re-baseline on measurement).
8. **Release blip**: tap-off on string 0 (D4) with no other fret held ⇒
   open-string burst with T60 55 ± 15 ms at `nut_damper` 0.35; on
   string 5 (C1) 0.5 ± 0.15 s; with `nut_damper` 0 both ring > 1 s.
9. **Beam dead spots**: bamboo beam ⇒ shortest bass-zone T60 at the
   fret nearest f₁ or f₂ on strings 5–7; aluminium beam ⇒ narrower
   dip (higher Q); `cfrp_ud` ⇒ dip moves ≥ 1 octave up.
10. **Zone masks**: bass pickup output contains no energy from melody
    strings (≤ −90 dBFS) and vice versa; `zone_stereo` stereo puts bass
    in L only; mono = sum with `mono_balance`.
11. **Tap on a guitar**: the same Tap path on `Vintage Double-Cut`
    reads `h` from `setup` action; raising action 1 → 2 mm raises tap
    level ≥ 4 dB and lowers `fc` by ~2×.
12. **Family isolation**: non-touch guitars render bit-identically for
    every non-tap technique; tap renders change (documented fixture
    re-baseline, since ground rule 1 replaces the heuristic tap).
13. **CPU**: 12 strings all tapped at 16th notes, 120 bpm < the 12-
    string guitar's budget + 0.3 % (`performance-budget.md`).
14. **Realtime safety**: router and tap excitation allocate nothing on
    the audio thread (validator).

## 11. MIDI export

Luthier profile: `TAP` events (two-hand-tapping.md 9) gain `zone` and
`hand`. Generic profile: zone → channel (bass ch1, melody ch2), so a
round trip through a DAW in `channel` mode is lossless for pitch,
timing and zone.

## 12. Data gaps - **accurate modelling is blocked on these**

The mechanism is fully specified and implementable; the *numbers* that
make it sound like a Stick rather than a plausible touch instrument are
not available:

1. **Octave placement of every tuning** (inferred from a range snippet
   and a tension argument). One look at stick.com's tuning charts closes
   it; until then, Classic/MR ship with a "tuning to be verified" flag
   in the preset metadata.
2. **Tap excitation constants**: action `h`, finger offset `x0`, finger
   speed range, impact and click gains, hold damping, nut-damper loss.
   No published tap-vs-pluck measurement exists for any instrument.
   Owner: DI recordings (both channels) of one note tapped soft/medium/
   hard and plucked at 1/5 L; release blips; feeler-gauge action at
   frets 1/12/24 on strings 1/5/6/10.
3. **Exact string gauges** (proxy set used).
4. **Pickup position, inductance, resonance, Block filter curves**.
5. **Beam modes and Q** (all Euler–Bernoulli estimates) and per-fret
   T60 on the lowest bass string.
6. **Baritone Melody interval and Touch Bass 8 tuning** (not shipped).
7. Collision-model constants (Bilbao & Torin DAFx-14; Issanchou et al.
   JASA 2018) were blocked; the fret-click reuses `fret-buzz.md` rather
   than a sourced stiffness/exponent.
