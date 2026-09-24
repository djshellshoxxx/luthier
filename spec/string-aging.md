# STRING AGING SPEC

A fresh set of strings and a dead set sound like two different guitars.
Fresh strings are bright, zingy and long-sustaining, with a metallic
shimmer that lasts a few hours. Dead strings are dull and short, and they
play slightly out of tune up the neck. Every player knows the sound, and
most recorded guitar sits somewhere between the two.

Luthier already has a crude version: the `string_age` choice (Fresh /
Broken In / Old), which picks one row of a three-row table in
`StringMaterials.cpp` (`kAgeEffects`: sustain, brightness, detune,
squeak) and applies it to all strings at once. This file replaces that
table with a continuous, per-string model built from four real
mechanisms. It keeps the three old rows as exact anchor points, so no
preset changes sound.

The design rule is the one from `INDEX.md`: **physical deltas only**.
Aging never adds a filter. It changes the loop-filter cutoff, loss gain,
dispersion and tuning that `StringEngine` already has.

## 0. Ground rules

1. **Age is measured in hours of playing.** String makers and players
   both count life this way. Calendar time only matters through
   corrosion, which `environment.md` scales by humidity.
2. **Four mechanisms, three multipliers.** Contamination, oxidation and
   corrosion pits, core fatigue, and coating all end up as a brightness
   multiplier, a sustain multiplier and a dispersion multiplier per
   string, plus a tuning offset. Nothing else in the engine changes.
3. **Per-string, deterministic.** Each string ages at its own rate
   (wound strings go dull faster, plain strings rust faster), with
   per-string variation hashed from the character seed
   (`character-wear.md` 1). The same seed and the same hours always give
   the same set.
4. **Legacy anchors are exact.** With `string_age_detail` at 0, 0 h, 12 h
   and 120 h reproduce the Fresh, Broken In and Old rows of `kAgeEffects`
   exactly.
5. **Honest magnitudes.** An uncoated set is audibly duller after 5 to
   20 hours and "dead" by 50 to 100. Coated sets last 3 to 5 times
   longer. Sustain loss on the fundamental is 10 to 35 %, not 90 %.
6. **Block rate, never per sample.** Aging is folded into coefficients
   the string already recomputes (`StringEngine::updateLoopCoefficients`).

## 1. What exists today, and what moves

| Today | After this spec |
|---|---|
| `string_age` choice, structural, applied in `StringMaterials::computeSpec` through `kAgeEffects` | Kept in the layout, because automation is indexed, but inert after load. Read only by the preset loader for migration (section 8), in the same way as `fret_action` |
| `refreshStringPhysics()` passes `stringAge` into `computeSpec` | Always passes `StringAge::Fresh`. Aging comes from `StringAging` (section 5) |
| `ageDetuneCents`: a random ± fine-tune per string from `RtRandom{0xA6E0000 + i}`, set in `refreshStringPhysics` | Moves into `StringAging`, with the same draw, so detail 0 matches exactly |
| `StringNoiseInfo::fromSpec(spec, material, age)`: `ageRoughness` 1.0 / 1.15 / 1.4 | Takes a continuous `ageRoughness` from `StringAging` |
| `StringSpec.squeak`'s age factor (1.2 / 1.0 / 0.62) on the in-loop slide noise | Replaced by section 3.5, which reconciles it with `string-squeak.md` 10 |

## 2. The mechanisms

| Mechanism | Physical cause | Mostly affects | Engine term |
|---|---|---|---|
| Contamination | Skin oil, dead skin and grime pack the gaps between winding wraps. This adds mass and friction between wraps where the string bends | Wound strings, high partials | Brightness `B` |
| Oxidation and corrosion pits | Surface rust makes the mass per length uneven along the string | Plain steel strings | Dispersion `P`, intonation `I` |
| Core fatigue | The core work-hardens and creeps, raising internal loss | All strings, in proportion to playing | Sustain `S` |
| Coating | A polymer film (thin "nano" or thick "poly") keeps grime and moisture out. It does not stop fatigue | Rate of contamination and corrosion | Rate factor `r_c`, fresh brightness `B_0` |

## 3. The model

### 3.1 Effective hours per string

For string `i`:

```
H_i  = max(0, string_age_hours − base_i) + accrued_i        // section 6
hc_i = H_i · wc_i · r_c · (0.5 + 0.5·ρ)                     // contamination
hk_i = H_i · wk_i · r_c · ρ · k_RH                           // corrosion
hf_i = H_i                                                   // fatigue: coating does not help
```

- `ρ` = `string_corrosivity` (the player's sweat chemistry, 1 = average).
- `k_RH` = `clamp(1 + 0.03·(RH_a − 45), 0.5, 3.0)`, where `RH_a` is
  `environment.md`'s acclimatised humidity. It is 1 when that spec is
  absent or at its default.
- `r_c` = coating rate factor: none 1.0, thin 0.33, thick 0.25. These are
  the midpoints of the 3x to 5x life figures coated-string makers quote.
- Per-string weights, scaled by detail `d` = `string_age_detail`:
  `wc_i = lerp(1, w_c(wound_i), d) · (1 + 0.10·d·j_i)` with
  `w_c(wound) = 1.4`, `w_c(plain) = 0.6`.
  `wk_i = lerp(1, w_k(wound_i), d) · (1 + 0.10·d·j_i)` with
  `w_k(wound) = 0.85`, `w_k(plain) = 1.15`.
  `j_i ∈ [−1, 1]` comes from the character seed through
  `CharacterEngine::hashed(kCategoryStringAge, i)`, mapped to bipolar.
  `hashed` is private today, so it is exposed as a public `const`
  accessor under a new category constant that no other value uses. A 3-plain / 3-wound set averages to weight 1, so the
  set's overall age matches the legacy table.

### 3.2 Physical curves

Named constants, fitted so that 12 h and 120 h land within 1 % of the
legacy anchors:

```
B_phys(h) = B_0 · (1 − 0.525·(1 − e^(−h/25)))                 h = hc_i
S_phys(h) = 1 − 0.375·(1 − e^(−h/50))                         h = (hf_i + hk_i)/2
D_phys(h) = 5.17 · (1 − e^(−h/35))   cents                    h = (hc_i + hk_i + hf_i)/3
P_phys(h) = 1 + 0.10 · (1 − e^(−h/50))                        h = hk_i
C_n(h)    = (1 − e^(−h/25)) / (1 − e^(−120/25))               h = hc_i  (0 fresh, 1 at 120 h)
```

`B_0` is the fresh-coating brightness: none 1.00, thin 0.97, thick 0.92.
`StringMaterial::Coated` already carries its own darker table brightness
(4900 Hz), so `B_0` is 1.0 for that material and only the rate factor
applies. Choosing that material forces coating to at least thin.

### 3.3 Legacy piecewise and the blend

`B_leg`, `S_leg` and `D_leg` interpolate linearly through
`(0 h, Fresh)`, `(12 h, BrokenIn)` and `(120 h, Old)` of `kAgeEffects`,
and stay flat past 120 h. The factor applied is

```
factor = lerp(legacy(H_i), physical(h_i), d)
```

so `d = 0` is the legacy table exactly (with `P = 1`, no intonation term,
and legacy squeak). `d = 1` is the full model.

### 3.4 Tuning

- **Open-string offset:** `D_i · r_i` cents, where
  `r_i = RtRandom{0xA6E0000 + i}.nextBipolar()`. This is the same draw
  `refreshStringPhysics` makes today.
- **Intonation:** old strings play sharp up the neck because the mass
  along them is uneven. The term is added to
  `TuningEngine::setIntonationSlope` as `d · D_i · 0.5 / 12` cents per
  fret, which is about +2.6 cents at fret 12 at 120 h. It **adds to**
  the slope the `intonation_error` path sets; it does not overwrite it.

### 3.5 Squeak

`string-squeak.md` 10 says old strings squeak more. The removed
`StringSpec.squeak` factor said fresh strings squeak more. Both are
partly right. Fresh wraps have sharp edges (a bright squeak); grime adds
stick-slip friction (a louder but duller squeak). With `C = d · C_n`:

```
ageRoughness   = lerp(R_leg(H_i), 1 + 0.4 · C_n, d)   (level; string-squeak 10's 1.4 at 120 h)
squeakCentroid = 1 − 0.30 · C                        (band-pass centre multiplier)
```

`R_leg` is `PlayingNoise`'s current 1.0 / 1.15 / 1.4 table, interpolated
through the same three anchors.

The in-loop slide noise (`StringSpec.squeak`'s old age factor, 1.2 / 1.0 /
0.62) keeps its legacy table at `d = 0`; at `d = 1` it is the Broken In
level, since the physical squeak lives in `PlayingNoise`. (*As built.*)

This file comes later in `INDEX.md` than `string-squeak.md`, so under the
brief's rule 22 it governs the spectrum. The level rule is unchanged.

### 3.6 What is deliberately not modelled

False beating (a partial splitting in two because of uneven mass) would
need a second delay line per string, which doubles the waveguide cost. It
goes to `proposals/` rather than being faked here.

## 4. Parameters

Appended at the end of the layout (`DECISIONS.md`: hosts index
automation by position). Net **+5** (see `body-coupling.md` 4 for this
build's totals).

| ID | Name | Stock | Advanced | Default | Unit |
|---|---|---|---|---|---|
| `string_age_hours` | String Age | 0 – 200 | 0 – 2000 | 12 | h (skew centre 24 h) |
| `string_corrosivity` | Hand Corrosivity | 0.5 – 2.0 | 0 – 5.0 | 1.0 | × |
| `string_age_detail` | Aging Detail | 0 – 1 (non-physical, single range) | – | 1.0 (legacy loads: 0) | ratio |
| `string_coating` | Coating | choice: None / Thin / Thick | – | None | – |
| `string_age_accrual` | Age While Playing | choice: Off / Real time / x10 / x100 | – | Off | – |

The two ranged parameters form a new range family, **`strings`**. This
amends `file-formats.md` 2 and `advanced-ranges.md` 2, which fix seven
keys. The change is backward compatible: an absent key reads as stock
(`advanced-ranges.md` 4), and older builds treat an unknown key as absent
(`rangeFromName` returns `none`). `RangeFamily::strings` is appended after
`modulation`, so no existing index moves. `RangeRegistry` gains the two
rows. The declared ranges are the stock pair (`advanced-ranges.md` 1.0).
2000 h is past any real string's life, and the model is still stable
there because every curve saturates.

## 5. Engine insertion

- **New `StringAging`** (`Source/DSP/String/StringAging.h/.cpp`). It owns
  the per-string state (section 6) and computes
  `AgingFactors { brightness, sustain, dispersion, detuneCents,
  intonationCentsPerFret, roughness, squeakCentroid }` for each string.
  It is POD, has fixed-size `std::array<…, kMaxStrings>` storage and
  does no allocation.
- **`StringEngine`** gains
  `setAgingFactors(double brightness, double sustain, double dispersion) noexcept`.
  It stores the factors and sets `needsLoopUpdate`. In
  `updateLoopCoefficients`:
  `open = physical.openBrightnessHz · agingBrightness · terminationBrightness`
  and `t60 = physical.sustainSeconds · agingSustain · sustainScale · …`.
  `updateDispersion` uses `physical.inharmonicityB · agingDispersion`.
  The factors are separate from `sustainScale`, which carries per-note
  character, slide and magnet values (`DECISIONS.md`: "Per-note sustain
  scale"), so neither can overwrite the other.
- **`LuthierEngine::processBlock`**, next to `character.advance(...)`
  (the tuner-drift site): call `aging.advance(seconds, levels)`. If any
  input changed (hours by more than 1e-4 h, detail, corrosivity,
  coating, `k_RH`, a restring, or an accrual tick), recompute and push
  `setAgingFactors` to each string, `tuning.setFineTuneCents(s, …)`, and
  the intonation slope.
- **`refreshStringPhysics()`** passes `StringAge::Fresh` to `computeSpec`
  and replaces its `ageDetuneCents` block with a synchronous write of
  `StringAging`'s detune for the target hours (*amended in the build:*
  anything a structural change builds from the tuning must see the right
  pitch, or the first render after a preset load differs from the second -
  the MIDI-export round trip caught it). `reset()` pushes the factors too. It calls
  `aging.setStringInfo(i, wound, material)` so that the weights know which
  strings are wound.
- **Noise:** `StringNoiseInfo::fromSpec` takes the roughness from
  `StringAging` for the playing-noise and scrape pools. `NoiseEngine`'s
  squeak band-pass centre is multiplied by `squeakCentroid`.
- **`ParameterBridge::applyToEngine`** reads the five parameters. The
  `lastStringAge` structural path is removed.

## 6. Per-string state

```
StringAgingState {
    std::array<double, kMaxStrings> baseHours;            // set-age when this string was replaced
    std::array<std::atomic<double>, kMaxStrings> accruedHours;
}
```

- **Restring one string** (a command through `ui-wiring.md` 6's queue)
  sets `base_n = string_age_hours` and `accrued_n = 0`. Only that string
  becomes fresh, which is what happens after a player replaces a broken
  string.
- **Restring all** writes `string_age_hours = 0` as a normal parameter
  gesture and posts a command that clears every `base` and `accrued`
  value. This is one undo entry (`LuthierAudioProcessor::ScopedUndoAction`).
- **Accrual:** when it is not Off, each block adds
  `dt · rate / 3600` h to every string whose `getLevel()` is above 1e-3
  (about −60 dBFS). This is played time, not wall-clock time. Accrual
  never writes the parameter. `reset()` does not clear accrued hours,
  because they are instrument state. The offline renderer and
  `midi-export.md` 12's round-trip tests run with accrual Off (the
  default), so they stay deterministic.

## 7. UI

Per `gui-integration.md`:

- **Advanced Column 1 → STRINGS:** the existing "age slider" binds to
  `string_age_hours`, with tick labels Fresh (0), Broken in (12) and
  Old (120).
- **Column 4 CHARACTER → new STRING AGING group** (placed after STRING
  NOISE): hours (a mirror), coating, hand corrosivity, aging detail, age
  while playing. Below them is one row per string: effective hours `H_i`,
  a brightness bar (`B` as a %), and a **Restring** button. A
  **Restring all** button sits at the end. The readouts drain at 4 Hz and
  grey out after 2 s stale (`gui-engine-dataflow.md`).
- **Workshop:** the strings part inspector mirrors the per-string rows.
  A strings part with `winding: coated` sets `string_coating` to at
  least Thin (`part-acoustics.md` 8).
- The CHARACTER tab padlock covers the `strings` family
  (`gui-integration.md` 21).

## 8. Serialization and migration

- The five parameters are stored in `parameters`, like any other.
- The per-string state is stored in the preset's `character` block as
  `"aging": {"base_hours": [...], "accrued_hours": [...]}`, written by
  `StringAging::toVar` next to `CharacterEngine::toVar`. An absent block
  means all zeros.
- **Legacy load** (`PresetManager`, beside the pickup-placement legacy
  path): if `parameters` has no `string_age_hours`, the loader sets it
  from `string_age` (Fresh 0, Broken In 12, Old 120), sets
  `string_age_detail = 0` and sets coating to None. The preset sounds
  exactly as it did.

## 9. Performance and realtime safety

- The cost is block-rate only: about 6 `exp()` per string when an input
  changes, and nothing otherwise. The budget is **0.02 units** at 12
  strings with hours under LFO modulation (`performance-budget.md` gains
  a `StringAging` row). The per-sample cost is zero, because the factors
  fold into coefficients the string already computes.
- All math is in `double`. There is no allocation, no lock, and nothing
  on the audio thread touches the filesystem. The accrued hours are
  relaxed atomics read by the message thread on save.
- **Smoothing:** the hours value the audio thread uses goes through a
  200 ms one-pole at block rate, so an automation jump from 0 to 200 h
  becomes a glide. Brightness and loss gain step once per block (at 128
  samples, well under a cent of pitch and inaudible). Detune goes
  through `TuningEngine`, which the string's pitch glide already smooths.

## 10. Tests

- **SA-01 Legacy anchors.** With `d = 0`, hours 0, 12 and 120 give
  brightness, sustain and detune within 1e-9 of `kAgeEffects` rows 0, 1
  and 2.
- **SA-02 Legacy null.** A preset with no `string_age_hours` and
  `string_age = Old` renders (4 s, 6-string chord) within −60 dBFS RMS
  of a reference made with the pre-spec `computeSpec(…, Old, …)` path.
- **SA-03 Curves meet anchors.** With `d = 1` and uniform weights
  (`j = 0`, a 3/3 set average), `B_phys` and `S_phys` at 12 h and 120 h
  are within 1 % of the legacy rows.
- **SA-04 Monotonic.** Sweeping hours 0 → 2000 in 1 h steps: `B` and `S`
  never increase, and `D` and `P` never decrease, for every string.
- **SA-05 Sustain.** Measured T60 of an open low E at 120 h (`d = 1`,
  uncoated) is 28 % to 40 % shorter than at 0 h.
- **SA-06 Brightness.** The spectral centroid of the first 500 ms of an
  open high-E pluck at 120 h is at least 15 % lower than at 0 h.
- **SA-07 Wound ages faster.** At 24 h with `d = 1` and the seed jitter
  zeroed, the wound low E's `B` is lower than the plain high E's by at
  least 0.10.
- **SA-08 Coating is a rate.** A thick-coated set at 100 h has exactly
  the contamination and corrosion hours (within 1e-9) of an uncoated set
  at 25 h, while fatigue hours stay at 100.
- **SA-09 Restring one.** After restringing string 3, its factors equal
  the 0 h factors, and every other string's factors are bit-identical
  to their values before.
- **SA-10 Determinism.** The same seed gives byte-identical per-string
  factors across runs. A different seed changes at least one `j_i`.
- **SA-11 Accrual.** Off: a 10 min render leaves every `accrued` at 0.
  x100: a 36 s render of one sustained string (re-plucked every 2 s)
  accrues 1.0 h ± 3 % on that string and 0 on silent strings.
- **SA-12 Humidity hook.** At 50 h with `k_RH` for 80 % RH (2.05) versus
  45 %, corrosion hours double and `S` is lower. With `environment.md`
  absent, `k_RH == 1`.
- **SA-13 Squeak reconciliation.** At 120 h with `d = 1`, on a string
  with unit weights and zero jitter, the squeak
  level multiplier is 1.40 ± 0.01 and the centroid multiplier is
  0.70 ± 0.01. At `d = 0` the centroid multiplier is exactly 1.
- **SA-14 No clicks.** Automating hours 0 → 200 within one block: the
  largest sample-to-sample difference in the output is no more than
  1 dB above that of an unautomated render of the same notes.
- **SA-15 Ranges.** The `strings` family satisfies the
  `advanced-ranges.md` 10 invariants. A legacy preset loads with
  `strings` = stock.
- **SA-16 Budget and safety.** Under a profile at 12 strings with hours
  on an LFO, `StringAging` costs ≤ 0.02 units. The heap hook shows zero
  audio-thread allocations.
