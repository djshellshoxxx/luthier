# COMPOSITE (GRAPHITE / CARBON-FIBRE) NECK SPEC

A neck *material* option, not an instrument: carbon/epoxy and
wood-plus-carbon hybrid necks as used by Zon, Modulus, Status,
Steinberger and graphite-reinforced wood necks. Players describe them
as "even, bright, long sustain, no dead spots". This spec gives that a
physical path in the parts model. Research:
`docs/research/INSTR_composite_neck_bass.md` §2, §5 (Fleischer dead-spot
data snippet-level; CFRP loss factors assumed - see its §9).

Tags: **M** documented, **I** inferred, **D** design default.

## 0. Ground rules

1. **A dead spot is neck mobility, not a random number.** Fleischer:
   the string loses energy into the neck where a neck bending resonance
   meets the note frequency at a fret with neck motion (M, research §5).
   Today dead spots are drawn from a seeded beta distribution
   (`character-wear.md` 2) with no link to the neck part. This spec
   derives them from the neck; the seeded randomness becomes the
   per-instrument *variation* around the derived positions.
2. **The claims must fall out of physics or not be made.** "Even
   response" = neck modes moved above most fundamentals and narrower;
   "sustain" = lower termination loss off resonance; "bright" = lower
   loss at high frequency. No tone-stack, no EQ.
3. **Honest magnitudes** (`part-acoustics.md` 0.4). The string's own
   losses dominate; a composite neck is a modest audible change except
   at the former dead spot, where it is large. The spectrum-delta panel
   must show exactly that.

## 1. Material rows (`lookUpWood`, `README.md` 1.2)

| id | ρ kg/m³ | E‖ GPa | tanδ | Derivation | Tag |
|---|---|---|---|---|---|
| `maple_hard` (existing) | 705 | 12.6 | 6.0e-3 | table | M |
| `cfrp_ud` | 1550 | 130 | 2.0e-3 | ρ, E: UD standard-modulus 60 % Vf range midpoints; tanδ: textbook 1–3e-3 midpoint, "significantly less damping than tonewood" qualitative M | ρ,E I; tanδ D |
| `cfrp_woven` | 1500 | 60 | 4.0e-3 | 0/90 fabric per axis 55–70; η higher than UD, higher still for VBO cure (M trend) | I / D |
| `cfrp_hybrid` | 832 | 30.2 | 3.4e-3 | §1.1 rule of mixtures, maple + 15 % UD by area | I |
| `phenolic_birch` (fingerboard, "Phenowood") | 1350 | 15 | 5.0e-3 | none found | **D** |

`engineWood` maps all `cfrp_*` → `Maple` (only used for the body
enum, which a neck does not reach).

### 1.1 Hybrid rule of mixtures (I)

Area fraction `φ` of UD carbon rods/skins in a wood neck (default
0.15, D; field `composite_fraction` on the neck part):

```
E_eff  = (1−φ) E_wood + φ E_cf              (bending, parallel)
ρ_eff  = (1−φ) ρ_wood + φ ρ_cf
tanδ_eff = [(1−φ) E_wood tanδ_wood + φ E_cf tanδ_cf] / E_eff   (strain-energy weighted)
```

Maple + 15 % UD: E 30.2 GPa, ρ 832, tanδ 3.4e-3. Zon's own neck is
described as "wood, graphite, spectra and others" (M snippet), i.e. a
hybrid; the factory Zon-style neck uses `cfrp_hybrid` with φ = 0.30 (D)
→ E 47.8 GPa, ρ 958, tanδ 2.7e-3.

## 2. Neck modal model (new, `mapSpec` step)

### 2.1 Mode frequencies

Anchor: a maple bolt-on bass neck's first dead-spot-producing mode at
**f₁ = 140 Hz** (M, Fleischer 130–150 Hz). Higher operating deflection
shapes at **3.2, 6.8, 11.6 × f₁** (M, Fleischer).

For another material, same geometry (beam, `f ∝ √(E/ρ)`):

```
f₁ = 140 Hz × sqrt( (E_eff/ρ_eff) / (12.6e9/705) ) × g_scale × g_joint
```

- `g_scale = (864 / scale_length_mm)^1` (D: first-order, a longer neck
  is a longer beam; replace when a second scale is measured).
- `g_joint`: bolt 1.00, set 1.05, through 1.10 (D; stiffer joint raises
  the clamped-end stiffness).

Results (I, bolt joint, 864 mm): maple 140 Hz; hybrid 15 % 200 Hz;
hybrid 30 % 234 Hz; woven 209 Hz; UD 303 Hz. These agree with research
§5's equal-geometry scaling (×1.59 woven, ×2.17 UD). Real UD necks are
thinner than wood ones, which lowers f₁ again, but `profile` is feel-only
in `part-acoustics.md` 3, so this spec does not invent a thickness term
(data gap).

### 2.2 Mode Q

`1/Q_k = tanδ_eff + η_joint + η_hand`, with `η_joint` bolt 2e-3, set
1e-3, through 0.5e-3 (D) and `η_hand = 1.5e-3` (D: Fleischer measured
with the instrument held). Maple bolt: Q ≈ 105; hybrid 30 % set:
Q ≈ 190; UD through: Q ≈ 250. Research's material-only bounds (170 vs
330–1000) are upper limits; installed Q is lower, as required.

### 2.3 From modes to string loss (replaces random dead spots)

For string `s` stopped at fret `k` (sounding `f`), extra loop loss per
second:

```
Δ(1/T60)(s,k) = D0 × Σ_m  w(x_k) × L_m(f)
L_m(f) = 1 / (1 + Q_m² (f/f_m − f_m/f)²)          (resonance line shape)
w(x_k) = sin²(π x_k / (2 L_neck))                   (neck motion at the fret; 0 at body joint, 1 at the nut end)
```

- `D0` is calibrated so maple bolt-on, G string at f₁ gives a **50 %**
  T60 reduction (D; the middle of `character-wear.md`'s 0.1–0.7 depth
  range, which was the prior design default).
- `x_k` measured from the body joint toward the nut.
- Also applies to the **open** string at the nut termination with
  `w = 1` scaled by `nut.material` hardness (a graphite nut already
  exists as a part).
- `CharacterEngine`'s seeded dead spots become a ±20 % jitter on
  `f_m` and depth instead of free placement (seeded, deterministic).
- Off resonance, the Lorentzian floor × `D0` is the "sustain" term: a
  composite neck's narrower, higher modes leave less loss at the
  common bass fundamentals (41–200 Hz). That is the whole "sustain" and
  "even response" claim, derived.

### 2.4 Brightness

Termination loss rises with frequency ∝ `tanδ_eff` (`part-acoustics.md`
1.1). Map: `d.fretBrightness *= 1 + (6e-3 − tanδ_eff) × 20`, clamped
0.9–1.1 - **the exact rule already used for fretboard wood**
(`PartAcoustics.cpp:445`) extended to the neck. No new constant.

## 3. Parts

| Part | Fields | Tag |
|---|---|---|
| `Bass Composite Bolt-On 34` | `wood: cfrp_hybrid`, `composite_fraction: 0.30`, `scale_length_mm: 864`, `joint: bolt`, `strings: 5`, `frets: 24`, `truss: none` | scale/frets M (Sonus 5), fraction D |
| `Bass Composite Set 35` | same, `joint: set`, `scale_length_mm: 889` | M (Sonus 5/2 35") |
| `Bass Carbon Through-Neck` | `wood: cfrp_woven`, `joint: through`, 864 mm | M (Status-style woven neck-through) |
| `Maple Neck + Carbon Rods` (guitar and bass variants) | `wood: maple_hard`, `composite_fraction: 0.15` → `cfrp_hybrid` | I |
| Fretboard `Phenolic Birch` | `wood: phenolic_birch`, `radius_mm: 305` | radius M (12"), material D |

Factory guitar **Composite 5-String Bass**: alder or mahogany core
body (existing `Bass P-Style Body` variant), `Bass Composite Bolt-On 34`,
phenolic board, `Graphite Locking` nut family, 45-130 strings, active
3-band preamp (existing). Presets: **Composite Fingerstyle**,
**Composite Slap** (bass-techniques unchanged).

## 4. Tests

1. Material rows load; `cfrp_hybrid` with φ produces §1.1 values ±1 %.
2. f₁: maple bolt 140 ± 2 Hz; hybrid 30 % set 234 × 1.05 = 246 ± 5 % Hz.
3. **Dead spot is where the neck says.** Maple bolt 34": G string, the
   fret whose pitch is nearest 140 Hz (fret 5-6, C#3/D3) has the
   shortest T60 on that string, ≥ 40 % shorter than frets ±3 away.
4. **Composite removes it.** Same guitar with `Bass Composite Bolt-On
   34`: T60 variation across frets 0–12 on the G string ≤ 15 %.
5. **Monotonic** (`part-acoustics.md` 11): sweep φ 0 → 0.5; f₁ rises
   monotonically, mean G-string T60 over frets 0–12 does not fall.
6. Every new field moves the spectrum (perturb 10 %).
7. With no `composite_fraction` field and a wood neck, the derived
   dead-spot map equals the seeded map within the jitter band, so
   existing factory fixtures move only by that documented change
   (**re-baseline required**; list the fixtures in the PR).
8. Determinism across platforms.

## 5. Illustration

Neck fill for `cfrp_*`: flat charcoal `#2A2C2E` with a **twill hatch**
(two families of 45° hairlines, 6 % opacity) for woven; parallel
hairlines along the neck for UD; for hybrids the wood fill plus two thin
dark stripes (the rods) along the centre line. Phenolic board: near-
black with no grain strokes. Headless variant (Steinberger-style) uses
the catalogue's existing **Headless Modern Bass** (§4.4).

## 6. Data gaps (block accuracy of magnitudes, not the mechanism)

- Absolute tanδ for CFRP layups and hard maple (assumed).
- Fleischer's per-instrument CF-vs-wood conductance curves (on
  ResearchGate; blocked this session) - these would replace `D0`, the
  3.2/6.8/11.6 ratios' amplitudes, and `η_joint`.
- Zon neck layup, mass and stiffness; Phenowood properties.
- No controlled sustain comparison wood vs composite exists in sources.
- Owner-sourced: free-free tap test of a composite and a maple neck
  (f₁, Q); per-fret T60 on the G string of both basses (the single most
  valuable measurement for this spec); neck mass on a scale.
