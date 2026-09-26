# TENOR GUITAR SPEC

Four-string, short-scale (21–23") flat-top or archtop guitar, tuned in
fifths (CGDA), to the top four guitar strings (DGBE, "Chicago") or an
octave below fiddle (GDAE, Irish). Research:
`docs/research/INSTR_tenor_guitar.md` (spec sheets snippet-level; body
modes inferred - no tenor-guitar measurement exists in the sources).

Tags: **M** documented, **I** inferred, **D** design default.

## 0. Ground rules

1. **Family `acoustic`, 4 strings.** String count is already
   `min(neck.strings, bridge.strings)` (`guitar-workshop.md` 5.1); no new
   code for four strings. `kMaxStrings` 12 covers it.
2. **Tuning comes from the guitar file** (`README.md` 1.4). Without that
   field a 4-string acoustic resolves to the Dreadnought base type and
   computes tensions for E-standard - wrong by up to ±40 % on CGDA.
3. **One body, three tunings, three string sets.** The tunings are not
   interchangeable on one set (tension table §2); each preset names its
   set.

## 1. Parts (factory guitar `Tenor 0-Size`)

| Slot | Part | Key fields | Tag |
|---|---|---|---|
| `body` | **new** `Tenor 0-Size Mahogany Body` | `wood: mahogany`, `chambering: acoustic`, `thickness_mm: 105`, `area_cm2: 1170`, `bracing: x`; `body_style: parlor` | bout 343 M, depth 105 M, area I (343² × 1.3 × 0.78 = 1193 → trimmed to 1170 for the narrower upper bout, D) |
| `top` | existing `Sitka Spruce (acoustic)` | `thickness_mm: 2.6` | wood M; thk D |
| `neck` | **new** `Tenor Mahogany Neck` | `scale_length_mm: 584`, `strings: 4`, `frets: 19`, `joint: set`, `profile: "slim C"` | scale M (0-18T 23"); frets D |
| `fretboard` | existing `Rosewood` | | D |
| `frets` | existing `Vintage Small Nickel-Silver` | | D |
| `nut` | **new** `Bone Tenor 32mm` | `width_mm: 31.8` | M |
| `bridge` | **new** `Acoustic 4-String Pin Bridge` (clone of `Acoustic Pin Bridge`, 4 saddle positions) | `mass_g: 22`, `coupling: 0.92` | mass I (fewer pins, shorter bridge: 28 × 4/6 + 3), coupling = pin bridge row |
| `tuners` | existing `Vintage Open-Back` | | D |
| `strings` | **new** ×3, §2 | | M |
| `pickups` | none | | — |

### 1.1 Why the Parlor shape row (I)

Existing `Parlor` row: bout 336, depth 95, 9.5 L. With the part above,
`scaleWidth ≈ 1.0` and `scaleDepth = 105/95 = 1.105` ⇒ engine volume
≈ 10.5 L, against research's 11.7 L estimate for a 0-size (−10 %).
That is inside the research band; **no new shape row**. Expected coupled
A0 **110–122 Hz** (research §5, I); test 5.3.

An optional second body, `Tenor Size-5`, uses `area_cm2: 830`
(286 mm bout, M), `thickness_mm: 98` (M), 21.4" neck (544 mm, M);
expected A0 130–145 Hz (I).

## 2. String sets and tunings

| Part | Tuning (string 0 → 3) | open_hz | Gauges in (string 0 → 3) | Tension lbf @ 23" (I, research §4) |
|---|---|---|---|---|
| `Tenor CGDA 10-32 Bronze` (EJ66-style) | A4 D4 G3 C3 | 440.00, 293.66, 196.00, 130.81 | .010 .014 .022w .032w | 23.6, 20.6, 18.6, 17.5 |
| `Tenor DGBE 12-32 Bronze` | E4 B3 G3 D3 | 329.63, 246.94, 196.00, 146.83 | .012 .016 .024w .032w | 19.1, 19.1, 22.1, 22.1 |
| `Tenor GDAE 12-44 Bronze` (Irish) | E4 A3 D3 G2 | 329.63, 220.00, 146.83, 98.00 | .012 .019 .028w .044w | 19.1, 21.3, 16.9, 18.6 |

String 3 on the EJ66-style set is wound (`w`); plain strings are
`core: round` steel, wound are 80/20 bronze on hex core. Named
`TuningEngine` presets: `TenorCGDA`, `TenorChicago`, `TenorIrish`.

Inharmonicity (I, research §4): plain strings B ≈ 1e-5 – 9e-5; the
plain A4 .010 at 584 mm is the stiffest-relative and the brightest
string in the product at its open pitch.

## 3. Playing defaults

- Right hand: **pick** (`Celluloid 0.73 Standard`) - tenor is a
  plectrum instrument in jazz rhythm and Irish accompaniment (research
  §6). Pluck position 0.22 (D).
- Rhythm kits: CGDA → existing `Freddie Green` / jazz comp (four-to-the-
  bar chords in fifths voicings); GDAE → new kit `Irish Backing`
  = existing strum patterns at jig 6/8 and reel 4/4 feels (data only).
- Voicer: fifths tuning means guitar chord shapes do not apply;
  `ChordVoicer` must search the 4-string fretboard, not map guitar
  shapes (it already searches by string tuning - test 5.5 guards it).

## 4. Presets

| Name | Set/tuning | Notes |
|---|---|---|
| **Tenor Jazz Rhythm** | CGDA | Four-to-the-bar comp, small room |
| **Tenor Chicago Folk** | DGBE | Open chords, fingerstyle optional |
| **Irish Tenor Backing** | GDAE | Jig/reel strum, bright pick |

## 5. Tests

1. Round-trip of `Tenor 0-Size.luthierguitar`; string count resolves to
   4 with a 4-saddle bridge, and to 4 with a 6-saddle bridge (mismatch
   reported).
2. Per-set tension within 10 % of §2 (the research's own computation).
3. Coupled A0 in 105–125 Hz (0-size) and 125–150 Hz (size 5).
4. Each tuning preset sounds its open strings within 1 cent.
5. Voicer: a C-major chord on CGDA voices within frets 0–5 on four
   strings (e.g. 0-0-2-3 or 5-4-2-0), never a 6-string shape.
6. No 5th/6th string state is allocated (per-string arrays sized 4 in
   `SoundingNotes`, fretboard overlay draws 4).

## 6. Illustration

**Tenor 0-Size** (`guitar-illustration.md` 4.2): parlor-like outline
scaled to 343 × ~460 mm, 12 or 14 frets to body; narrow 32 mm nut;
headstock **2+2** (existing layout from §6 headstocks); 4-pin bridge;
round soundhole Ø 88 (D, 0.95 × Parlor 92 scaled by bout). The archtop
TG-50 variant reuses the `Full Hollow Archtop` outline scaled to a 16"
bout with f-holes, floating bridge and trapeze.

## 7. Data gaps

No measured tenor-guitar body modes, Q, T60 or centroid; Martin tenor
factory set gauges unseen; wound-string core diameters (stiffness)
unknown; archtop and resonator tenors unmeasured. Owner-sourced: tap
test (A0, T1, Q), body volume + soundhole diameter, string mass/length,
dry recordings of open strings per tuning.
