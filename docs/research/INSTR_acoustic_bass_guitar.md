# Acoustic Bass Guitar (ABG)

> Research note for the physically-modelled guitar plugin. Compiled 2026-09-26.
> **Method caveat:** the session's network egress policy blocked WebFetch/curl to *every* research domain tried (Wikipedia, UNSW newt/phys, arXiv, ResearchGate, AIP/JASA, savartjournal, euphonics.org, mwguitars, DTU orbit, vintageguitar.com, manufacturer sites). **All cited facts come from web-search result snippets**, marked "(snippet)". No full paper text was read. Values marked **INFERRED** are my own scaling or estimates, and the formula is shown for each.

---

## 1. Summary

The acoustic bass guitar is a flat-top, steel-strung, four-string (sometimes five- or six-string) bass with a guitar-style hollow body, soundhole and pin or tie bridge. It is tuned like an electric bass (E1 A1 D2 G2), one octave below the lowest four guitar strings. The first modern ABG was the Ernie Ball Earthwood (1972). George Fullerton helped design it, and it had a very large, deep body. Later instruments (Martin B-40/BC-15E, Guild B-30, Tacoma Thunderchief, Fender Kingman, Warwick Alien, Taylor GS Mini Bass) are mostly jumbo or dreadnought-sized, and many carry an under-saddle piezo pickup. Without amplification, ABGs are notoriously quiet, and their low fundamentals come out weakly. The cause is physical. The strings' fundamentals (41–98 Hz) sit at or below the body's air (Helmholtz/A0) resonance, which is typically ~85–100 Hz. Below that resonance, the soundhole and top radiate out of phase and cancel, and the body is acoustically tiny compared with the ~3.5–8.3 m wavelengths involved.

---

## 2. Geometry & materials

### 2.1 Model table (all dimensions from snippets unless marked)

| Model | Scale | Body L × lower bout × depth | Top / back & sides / neck | Nut | Notes |
|---|---|---|---|---|---|
| Ernie Ball Earthwood (1972–74, ~1970s–1985) | 34" (863.6 mm) | 24.5" × 18.25" × 6.625" (622 × 464 × 168 mm); another source: 25" × 18.5" (upper bout 14") × 6.5" | spruce top, walnut body ("oversized walnut body") | n/a | Neck joins at 12th fret; "Hot Dot" pickup. Depth quoted up to 8.25" in one listing (unverified) |
| Martin B-40 (1988–96) | 34" | 20.125" × 16" × 4.875" (511 × 406 × 124 mm); "0000/M or J shape, full dreadnought depth" | solid spruce / solid East Indian rosewood / mahogany neck, ebony board & bridge | 1.58" (40.1 mm) | 2 1/8" (54 mm) string spacing at bridge; 17 frets clear, 23 total |
| Martin BC-15E (1999–2009) | 34" | 20.125" × 16" × 4.875" | solid mahogany top, back & sides; mahogany neck, rosewood board | 1 9/16" (39.7 mm) | Single cutaway; Fishman Prefix bass preamp and under-saddle pickup |
| Guild B-30 | 30" (762 mm) short scale | "full size" body, 6" (152 mm) deep | solid spruce top, solid mahogany sides, arched back | n/a | Lower-bout width not found |
| Fender Kingman Bass (current) | short scale: "30.3" (770 mm)" in one snippet, "30"" in another | "mid-sized auditorium" with cutaway; depth not found | X-braced solid spruce top, sapele back & sides | n/a | An older 34" "Kingman dreadnought acoustic bass" also exists (snippet) |
| Warwick Alien (4/5/6-string) | 34" | n/a | solid spruce top, laminated ovangkol back & sides | n/a | Fretless six-string version exists |
| Tacoma Thunderchief CB10C/CB28C (1999–) | 34" | "oversized body" | n/a | n/a | Kidney-shaped soundhole in upper bass bout, asymmetric bridge |
| Taylor GS Mini(-e) Bass | 23.5" (597 mm) | 17.625" × 14.375" × ~4.44" (448 × 365 × 113 mm) | solid Sitka spruce (also koa and maple variants) / layered sapele; ebony board & bridge | 1 11/16" (42.9 mm) | 20 frets; ES-B electronics; nylon-core strings (§4) |

String count is 4 by default, with 5 (B0) and 6 (B0 or C3 extensions, e.g. Warwick Alien 6) also made.

### 2.2 Body volume (INFERRED)

No published internal volumes were found. Estimate: `V ≈ k · L_body · W_lower · d_mean`, with outline fill factor k ≈ 0.72. That k is an assumed value for a guitar-waisted outline. Check it with a sanity case: a dreadnought (20" × 15.625" × ~4.5" mean depth) gives ≈ 16.5 L, which matches the commonly quoted mid-to-high-teens figure for a dreadnought. The 16.5 L result is itself inferred.

| Model | V (INFERRED) |
|---|---|
| Earthwood | ≈ 35 L (0.2115 m² × 0.168 m) |
| Guild B-30 (17" width assumed, 21" length assumed, 6" depth) | ≈ 25 L |
| Martin B-40 / BC-15E | ≈ 18.5 L (0.150 m² × 0.124 m) |
| Taylor GS Mini Bass | ≈ 13 L (0.118 m² × 0.113 m) |
| Reference dreadnought | ≈ 16.5 L |

**Owner measurement closes this:** fill the body with packing beads or measure it by water displacement in a bag, or take the volume from a CAD outline.

### 2.3 Wood properties (typical handbook values; NOT fetched, treat as INFERRED defaults)

| Wood | ρ (kg/m³) | E_L (GPa) |
|---|---|---|
| Sitka spruce (top) | ~400–450 | ~10–13 |
| Mahogany | ~500–600 | ~9–11 |
| East Indian rosewood | ~800–850 | ~11–13 |
| Walnut | ~600–650 | ~11 |
| Sapele | ~620–670 | ~12 |

These come from general tonewood knowledge. None of them came from a fetched source in this session.

### 2.4 Bracing

Standard X-bracing on steel-string ABGs. The Kingman is explicitly "X-braced solid spruce top" (snippet). Gore's design volume discusses bracing that raises top "monopole mobility" (∝ 1/√(K·m)), thick rigid top linings, and mass-loaded ribs (snippet). One luthier ABG build on ANZLF used a Gore "falcate" top voiced to ~170 Hz (snippet).

---

## 3. Tuning (A4 = 440 Hz, 12-TET)

| Tuning | Notes | Hz |
|---|---|---|
| Standard 4-string | E1 A1 D2 G2 | 41.20, 55.00, 73.42, 98.00 |
| 5-string (low B) | B0 E1 A1 D2 G2 | 30.87, 41.20, 55.00, 73.42, 98.00 |
| 6-string | B0 E1 A1 D2 G2 C3 | … + 130.81 |
| Drop D | D1 A1 D2 G2 | 36.71, 55.00, 73.42, 98.00 |
| Taylor GS Mini Bass | same pitches, EADG, despite the 23.5" scale | as standard |

Highest fretted notes: G2 at fret 20 = D#4 ≈ 311 Hz, and at fret 23 = F#4 ≈ 370 Hz. Formula: `f = f_open · 2^(n/12)`.

---

## 4. Strings

### 4.1 D'Addario EPBB170, phosphor bronze, long scale, 45-65-80-100

Phosphor bronze wrap over a hexagonal high-carbon steel core (snippet, daddario.com and dealers). Per-string tension at 34", from D'Addario tension-chart data (snippet):

| String | Gauge (in) | Tension (lbf) | Tension (N) | Unit weight (lb/in) | μ (kg/m) |
|---|---|---|---|---|---|
| G2 | .045 | 47.5 | 211 | 0.00041330 (snippet) | 0.00738 |
| D2 | .065 | 55.7 | 248 | 0.000863 (INFERRED back-calc) | 0.0154 |
| A1 | .080 | 47.4 | 211 | 0.001309 (INFERRED back-calc) | 0.0234 |
| E1 | .100 | 40.2 | 179 | 0.00197902 (snippet) | 0.0353 |

Check: `T[lbf] = UW · (2·L[in]·f)² / 386.4`. For G: 0.0004133 · (2·34·98)² / 386.4 = 47.5 lbf ✓. For E: 40.2 lbf ✓. The D and A unit weights were back-computed from the snippet tensions. Total set tension is ≈ 191 lbf (≈ 849 N).

Wave speed: `c = 2·L·f`. E1 = 71.2 m/s, A1 = 95.0, D2 = 126.8, G2 = 169.3 m/s at L = 0.8636 m.

Fender 8060 is also a 45-100 phosphor-bronze long-scale set (snippet).

### 4.2 45-65-85-105 sets

These appear as Rotosound RS44LD "Bronze Bass", 45-105, medium tension, in the search results. One search result gave these tensions for a 45/65/85/105 set: 45.25 / 49.56 / 48.19 / 39.86 lbf (G/D/A/E). **The source attribution of that snippet is unclear. Treat it as unverified.**

### 4.3 Taylor GS Mini Bass strings

These are custom D'Addario strings: a stranded nylon core with a heavy coated phosphor-bronze wrap. Gauges are .037 / .050 / .062 / .090. Tension is low but unpublished, and they are the only strings that work at 23.5" (snippet). A plugin should model them with a much lower μ-to-stiffness ratio: a nylon core means low bending stiffness and very low inharmonicity, plus higher internal damping (INFERRED).

### 4.4 Inharmonicity (INFERRED)

`B = π³·E·d_core⁴ / (64·T·L²)`, taking only the steel core and E = 200 GPa.

- E1 with an assumed 1.0 mm core: B ≈ 7×10⁻⁴.
- G2 with an assumed 0.56 mm core: B ≈ 6×10⁻⁵.

Partial n sits at `f_n = n·f₀·√(1+B·n²)`. Core diameters are not published, so these are order-of-magnitude figures only.

---

## 5. Body / structural resonances

### 5.1 Measured and reported values (guitars and ABGs)

| Mode | Freq (Hz) | Q / damping | Instrument | Status / source |
|---|---|---|---|---|
| Main air A0 (T(1,1)₁) | **87** | n/a | luthier ABG, 84 mm soundhole | MEASURED (snippet), ANZLF [S16] |
| Main air A0 | **~115** | n/a | ABG with a large soundhole | MEASURED (snippet), ANZLF [S16] |
| Main air A0 (design target) | **~60** | n/a | ABG with a tornavoz (internal soundhole tube) | Design target (snippet), ANZLF [S16] |
| Main top (T(1,1)₂) | **~170** | n/a | same falcate-top ABG build | Design / measured (snippet), ANZLF [S16] |
| A0 | ~95 | n/a | Martin dreadnought (15.75" bout, deep sides) | Reported (snippet), Premier Guitar [S11] |
| A0 | < 90 | n/a | Gibson 16" jumbo / round-shoulder | Reported (snippet) [S11] |
| A0 | 100–115 | n/a | OM / L-00 mid-size bodies | Reported (snippet) [S11] |
| A0, typical target | 92.5–110 (F#2–A2) | n/a | steel-string guitars generally | Reported (snippet) [S3, S2] |
| Lowest peak / main top peak | 90–110 / 180–200 | n/a | general guitar tap spectra | Reported (snippet) [S13] |
| Coupled pair | ~110 and ~200 (antiresonance ~125) | n/a | acoustic guitar vibration response | Reported (snippet) [S2] |
| Helmholtz | expected 123, peak particle velocity 110.5 | computed in report | student-measured acoustic guitar | MEASURED (snippet), Illinois PHYS406 [S2] |
| Coupled box resonances | 81 and 116 | n/a | acoustic guitar | MEASURED (snippet), "Modal Analysis of an Acoustic Guitar" [S19] |
| Air / top / back | 97 / 199–200 / 291 | n/a | classical guitar, falcate | MEASURED (snippet), ANZLF [S16] |
| A0 | 155 | n/a | guitar box (Elejabarrieta et al.; context unclear, possibly an intermediate construction stage) | Reported (snippet) [S20] |
| Two (0,0) body modes | ~100 Hz above Helmholtz | n/a | guitar generally | Reported (snippet) [S3] |

Other snippet findings:

- A classical guitar's main resonances "close to an octave apart, near G~98 and ~196 Hz" (Carruth, snippet [S15]).
- Radiation from the soundhole dominates up to ~200 Hz; above that the soundboard dominates (snippet [S19]).

### 5.2 Predicted ABG modes (INFERRED)

**Helmholtz, rigid walls:** `f_H = (c/2π)·√(A / (V·L_eff))`, with `L_eff = t + 1.7·r`, c = 343 m/s and t = 3 mm.

**Coupled A0:** a flexible top pulls A0 below the rigid-wall f_H. Calibration: the reference dreadnought (16.5 L, 4" hole) gives f_H ≈ 128 Hz against a reported A0 ≈ 95 Hz, so `A0 ≈ 0.75·f_H`.

| Model | V | Hole Ø (assumed) | f_H rigid | A0 predicted |
|---|---|---|---|---|
| Earthwood | 35 L | 4"–4.5" | 87–93 | **~65–70 Hz** |
| Guild B-30 | 25 L | 4" | 104 | **~78 Hz** |
| Martin B-40 / BC-15E | 18.5 L | 4" | 121 | **~90 Hz** |
| Taylor GS Mini Bass | 13 L | 3.5" | 133 | **~100 Hz** |

Soundhole diameters are assumed; none were found. The ANZLF measured 87 Hz with an 84 mm hole falls inside this range.

**Other modes (INFERRED):**

- Top T(1,1)₂: ~170–200 Hz, from typical steel-string values and the ABG build above.
- Back / T(1,1)₃: ~250–300 Hz.
- Q for any mode: no measured values found. Suggested plugin defaults are A0 Q ≈ 15–30 and top Q ≈ 20–40. These are unsourced guesses and should be tuned by ear or measurement.
- Neck modes: no data found. Bass-neck bending modes (the "dead spot" mechanism) likely fall in the ~150–400 Hz range (INFERRED, unsourced).

### 5.3 Why ABGs lack projected low end (the core physics)

1. **Fundamentals fall below A0.** Below the Helmholtz resonance, the air in the soundhole and the top move out of phase, so their radiation cancels. Above it they are more nearly in phase and the output is enhanced (snippet, on Caldersmith, JASA 61(2):588, 1977, "Physics of the guitar at the Helmholtz and first top-plate resonances" [S21]). The guitar behaves as a loudspeaker in a bass-reflex (vented) enclosure (snippet [S1, S21]). Christensen & Vistisen's two-mass model (JASA 68(3), 1980 [S18]) predicts the SPL quantitatively from f₋, f₊ and f_H (snippet).
   - **Roll-off (INFERRED):** in the two-piston model under a force drive, the net volume displacement → 0 as ω → 0, because the hole air cancels the top displacement. Net volume velocity then goes as ~ω³, and far-field pressure ∝ ω·U ~ ω⁴. That is a **~24 dB/octave asymptote below A0**, the same slope as a 4th-order vented box.
   - **Consequence with A0 = 85 Hz:** E1 (41.2 Hz, 1.05 octaves below) loses ≈ 25 dB relative to the asymptote at A0. A1 (55 Hz) loses ≈ 15 dB and D2 (73.4 Hz) ≈ 5 dB, while G2 (98 Hz) is above A0 and reinforced.
   - **With the Earthwood's estimated A0 ≈ 68 Hz:** E1 ≈ −17 dB and A1 ≈ −7 dB. This is why the big, deep Earthwood was reputedly louder in the low register.
2. **The source is compact.** The E1 wavelength is ≈ 8.3 m (snippet: "about 8 meters" [S10]). A radiator needs a diameter of roughly λ/4 or more to radiate efficiently (Carruth, snippet [S15]).
   - With an effective body radius a ≈ 0.2 m, **ka = 0.15 at E1, 0.20 at A1, 0.27 at D2 and 0.36 at G2** (INFERRED: `ka = 2πfa/c`).
   - The radiation efficiency of a compact piston is σ ≈ (ka)²: ≈ 0.023 at 41 Hz versus 0.13 at 98 Hz, about 7.5 dB less efficient at E1 than at G2 for equal surface velocity (INFERRED).
3. **Hearing.** Bassists need "roughly 10 times more power" for lows to be heard as loudly as higher instruments (Premier Guitar, snippet [S10]). This fits the equal-loudness contours.
4. **Many ABGs are too small.** Most ABGs "don't have a low enough body resonance to support the low E string" (ANZLF, snippet [S16]). The pitch of low notes is heard mainly through harmonics 2–4, the "missing fundamental" effect (INFERRED).

**Plugin implication:** the acoustic (mic) path should apply a strong high-pass around A0: roughly a 4th-order (24 dB/oct) shape at f ≈ A0, combined with a resonant A0 peak. The piezo path should not (see §6.2).

---

## 6. Excitation / playing technique

### 6.1 Playing technique

- Players pluck with the fingers (index/middle "two-finger" plucking or thumb), sometimes with a pick; slapping is rare on ABGs.
- There is no measured attack data for ABGs. INFERRED:
  - The plucking point is typically 80–150 mm from the saddle, which comb-filters the partials: notches at harmonics n ≈ L/x_pluck.
  - Heavy phosphor-bronze wrap gives a bright initial transient that fades quickly.
- The low-mobility stiff top at low frequency means less energy leaves the string below A0. That should lengthen sustain of the low fundamentals relative to partials near body modes, where energy drains faster (INFERRED; standard string–body coupling behaviour).

### 6.2 Under-saddle piezo (the usual ABG pickup; e.g. Fishman Prefix in the BC-15E)

- **Signal:** the piezo senses the dynamic vertical string force at the saddle, before the body filters it. The output therefore contains the full-strength fundamentals that the body fails to radiate (INFERRED from the transducer location).
- **Electrical:** the pickup is a capacitive source. Effective capacitance is "no more than 12 nF, many a great deal less" (snippet, ESP/sound-au.com [S22]). It forms an RC high-pass with the preamp input: `f_c = 1/(2π·R_in·C)`.
  - C = 1 nF, R_in = 1 MΩ → f_c = 159 Hz, which cuts all bass fundamentals.
  - 10 MΩ → 16 Hz.
  - For f_c < 41 Hz with 1 nF, R_in must be ≥ 3.9 MΩ.
  - The "well known" 1 MΩ minimum (snippet [S22]) is inadequate for low-capacitance under-saddle strips on a bass (INFERRED).
- **Tone:** the characteristic "quack" or hard attack, often attributed to under-saddle piezos, comes from the force-sensing transfer function lacking body resonances and from saddle–slot contact nonlinearity. This is INFERRED; no measured transfer function was found.
- **Plugin model:** string bridge-force → optional 1st-order HP at `1/(2πRC)` → mild presence emphasis (2–5 kHz, unsourced) → optional preamp EQ.

---

## 7. Distinctive spectral traits

- **Weak fundamentals when heard acoustically.** Below-A0 cancellation (§5.3) means the radiated spectrum of E1/A1 notes peaks at harmonic 2–3 near A0 and the top mode (~85–200 Hz) (INFERRED).
- **Formant-like regions:** A0 (~65–100 Hz depending on body), top mode (~170–200 Hz), back mode (~250–300 Hz). The first two come from snippets, the back mode is inferred.
- **Brightness:** phosphor-bronze wrap is described as "bright" and "clear, even response" (manufacturer copy, snippet [S7]). A brightened Rotosound bronze set can clear up "an otherwise muddy sound from certain hollow-bodied basses" (snippet [S9]).
- **Spectral centroid, T60, measured inharmonicity:** no ABG-specific measured data found (see §9).

---

## 8. Sources

No page could be fetched because of egress blocks. Every item below was seen as a search result, and every fact taken from it is a snippet.

1. [S1] https://www.researchgate.net/publication/239632785_Air_Cavity_Modes_in_the_Resonance_Box_of_the_Guitar_the_Effect_of_the_Sound_Hole — Elejabarrieta/Ezcurra/Santamaría, J. Sound Vib. 2002; three (0,0) body modes, two lying ~100 Hz above Helmholtz (snippet).
2. [S2] https://courses.physics.illinois.edu/phys406/sp2017/Student_Projects/Spring13/Dan_Gualandri_P406_Final_Project_Report_Sp13.pdf — Helmholtz expected 123 Hz, peak 110.5 Hz; response peaks ~110 and ~200 Hz with antiresonance ~125 Hz (snippet).
3. [S3] https://en.wikibooks.org/wiki/Acoustics/How_an_Acoustic_Guitar_Works — makers tune air resonance to F#2–A2 (92.5–110 Hz) (snippet).
4. [S4] https://en.wikipedia.org/wiki/Acoustic_bass_guitar — Earthwood history: Fullerton, 1972, first modern ABG, large deep body, production dates (snippet).
5. [S5] http://guitarz.blogspot.com/2009/11/ernie-ball-earthwood-acoustic-bass.html and https://www.creamcitymusic.com/vintage-1972-ernie-ball-earthwood-acoustic-bass-guitar-natural-finish/ — Earthwood dims 24.5×18.25×6.625" / 25×18.5 (14 upper)×6.5", 34" scale, 12th-fret neck joint (snippet).
6. [S6] https://www.guitar-list.com/martin/acoustic-guitars/martin-b-40 and https://umgf.com/martin-b-40-bass-specs-t44064.html — B-40 specs: woods, 20 1/8×16×4 7/8", nut 1.58", bridge spacing 2 1/8", 1988–96 (snippet).
7. [S7] https://www.daddario.com/products/epbb170-phosphor-bronze-acoustic-bass-strings-long-scale-45-100 — EPBB170 gauges 45/65/80/100, PB on hex steel core (snippet).
8. [S8] http://www.kevinkastning.com/D'Addario_tension_chart.pdf and https://www.daddario.com/globalassets/pdfs/accessories/tension_chart_13934.pdf — PBB045 47.5 lb and UW .00041330; PBB100 40.2 lb and UW .00197902; PBB065 55.7 lb; PBB080 47.4 lb (snippet).
9. [S9] https://www.rotosound.com/product/rs44ld/ — RS44LD 45-105 bronze, "brightens muddy hollow-bodied basses" (snippet). The 45/65/85/105 tension values appeared in the same search, source unclear.
10. [S10] https://www.premierguitar.com/bass-bench-the-inherent-limitations-of-acoustic-bass-guitar — ABG lack of volume; "10 times more power"; E ~8 m wavelength (snippet, partly mixed with other results).
11. [S11] https://www.premierguitar.com/acoustic-soundboard-the-shape-of-things — dreadnought air ~95 Hz, Gibson 16" <90 Hz, OM/L-00 100–115 Hz (snippet).
12. [S12] https://www.taylorguitars.com/guitars/acoustic/gs-mini-e-bass and https://www.guitarchalk.com/taylor-gs-mini-dimensions/ and https://www.taylorguitars.com/support/strings/changing-strings-gs-mini-bass — GS Mini Bass 23.5" scale, 17.625×14.375×~4.44", nut 1 11/16", woods, nylon-core PB strings .037–.090 (snippet).
13. [S13] https://www.mimf.com/phpbb/viewtopic.php?t=2428 — lowest peak (main air) usually 90–110 Hz, main soundboard peak 180–200 Hz (snippet).
14. [S14] https://www.creamcitymusic.com/2005-martin-bc-15e-acoustic-electric-bass-natural-finish/ and https://umgf.com/martin-bc-15e-acoustic-bass-t82958.html — BC-15E: 34", 4 7/8" depth, 16" width, 20 1/8" length, mahogany, nut 1 9/16", Fishman Prefix, 1999–2009 (snippet).
15. [S15] https://www.classicalguitardelcamp.com/viewtopic.php?t=106385 — "Considerations for an Acoustic Bass" (Carruth): bass-reflex analogy, A0 near G~98 Hz, λ/4 radiator-size rule, coupled modes ~98/196 Hz (snippet).
16. [S16] https://anzlf.com/viewtopic.php?t=4649 and http://www.anzlf.com/viewtopic.php?t=5157 and https://anzlf.com/viewtopic.php?start=25&t=6652 — ABG builds: main air 87 Hz with 84 mm hole; ~115 Hz with large hole; falcate top 170 Hz with tornavoz targeting ~60 Hz air; "most ABGs lack low enough body resonance for low E"; classical 97/199/291 Hz (snippet).
17. [S17] https://goreguitars.com.au/design/ — monopole mobility ∝ 1/√(K·m); bracing, linings, rib mass-loading (snippet).
18. [S18] https://backend.orbit.dtu.dk/ws/files/3580072/Vitsten.pdf — Christensen & Vistisen, "Simple model for low-frequency guitar function", JASA 68(3) 1980; two-mass model predicting SPL from f₋, f₊, f_H (snippet).
19. [S19] https://www.researchgate.net/publication/234179149_Modal_Analysis_of_an_Acoustic_Guitar and https://onlinelibrary.wiley.com/doi/10.1155/2016/6084230 — coupled box modes 81 and 116 Hz; soundhole radiation dominates <200 Hz (snippet).
20. [S20] https://pubs.aip.org/asa/jasa/article-abstract/111/5/2283/551378/Coupled-modes-of-the-resonance-box-of-the-guitar — Elejabarrieta, Ezcurra & Santamaría, JASA 111(5):2283–2292, 2002; A0 155 Hz mention (snippet, context unclear).
21. [S21] https://pubs.aip.org/asa/jasa/article-abstract/61/2/588/778018/Physics-of-the-guitar-at-the-Helmholtz-and-first — Caldersmith 1977: out-of-phase soundhole/top radiation below Helmholtz; bass-reflex analogue (snippet).
22. [S22] https://sound-au.com/project202.htm and https://groupdiy.com/threads/getting-1-megohm-input-impedance-for-a-piezo-pickup.80784/ — piezo capacitance ≤12 nF, RC high-pass, 1 MΩ "minimum" folklore, 200 pF → 20 MΩ for 40 Hz (snippet).
23. [S23] https://www.zzounds.com/item--FENKMB and https://www.fender.com/products/kingman-bass — Kingman: short scale (30"/30.3"), X-braced solid spruce, sapele, cutaway (snippet).
24. [S24] https://shop.warwick.de/en/instruments-acoustic-basseswarwick-alien-6-string-fretless-natural-transparent-satin — Alien: 34", solid spruce, laminated ovangkol (snippet).
25. [S25] https://www.premierguitar.com/trash-or-treasure-tacoma-thunderchief-cb10ce4 and https://bluebookofguitarvalues.com/guitar-values/acoustic-guitars/manufacturer/tacoma/category/tacoma-acoustic-bass-thunderchief-series/models — Thunderchief: 34", oversized body, upper-bout soundhole (snippet).
26. [S26] https://www.talkbass.com/threads/guild-acoustic-bass-review-b-30.1088256/ and https://en.audiofanzine.com/acoustic-electric-bass/guild/B30/ — Guild B-30: 30" scale, 6" depth, spruce/mahogany, arched back (snippet).

---

## 9. DATA GAPS / paywalled

- **No ABG-specific peer-reviewed modal or radiation measurement was found.** Luthier-forum measurements (87 Hz, ~115 Hz) are the only direct ABG A0 data.
  - *To close:* tap-test the owner's ABG. Hold the instrument, place a mic ~10–20 cm from the soundhole, knock the bridge with a knuckle or soft mallet, and take the FFT of the impulse. This gives A0, T(1,1)₂, T(1,1)₃ and −3 dB bandwidths, and therefore Q = f/Δf.
  - Repeat with the soundhole covered to get the uncoupled top mode, and with the top damped to approximate the rigid-wall f_H.
- **Paywalled or unreadable in this session:**
  - Elejabarrieta, Ezcurra & Santamaría, "Coupled modes of the resonance box of the guitar", JASA 111(5):2283 (2002), and "Air cavity modes…", J. Sound Vib. (2002).
  - Caldersmith, JASA 61(2):588 (1977).
  - Christensen & Vistisen, JASA 68(3):758 (1980). A DTU PDF link exists but was blocked.
  - Gore & Gilet, *Contemporary Acoustic Guitar Design and Build* (book, not online).
  - Premier Guitar ABG article (full text blocked).
  - UNSW guitar acoustics pages (blocked).
- **Q / damping of body modes:** none found. Close with the tap test above, or from sine-sweep bridge admittance using a small shaker or impact hammer and accelerometer.
- **Body volumes and soundhole diameters:** none published. Measure directly (§2.2).
- **Radiation roll-off below A0:** the 24 dB/oct figure is my model-based inference, not a measurement.
  - *To close:* record open E1, A1, D2 and G2 with a mic at 1 m, alongside a DI piezo signal. The harmonic-1 level difference (mic − piezo) versus frequency directly measures body radiation efficiency below and above A0.
- **String data:** EPBB170 D/A unit weights are back-computed. Core diameters are unknown, so inharmonicity is only an order of magnitude. GS Mini Bass nylon-core tensions are unpublished.
  - *To close:* weigh a measured length of each string to get μ, and measure the frequencies of partials 1–15 of each open string from a DI recording, then fit B.
- **Sustain / T60 and spectral centroid:** not found for any ABG. *To close:* record each open string plucked at a fixed point via DI and mic, then compute per-partial decay rates (T60) and the centroid over time.
- **Neck modes:** not found. *To close:* tap the headstock and neck with the body clamped, or map dead spots with an accelerometer on the headstock.
- **Piezo transfer function:** no measured under-saddle frequency response or capacitance for bass models (Fishman Prefix, Taylor ES-B). *To close:* measure pickup capacitance with an LCR meter, and compare piezo DI against a bridge-force or accelerometer reference.
- **Model-spec gaps:** lower-bout widths for the Guild B-30, Kingman and Warwick Alien. Kingman scale is inconsistent across snippets (30" vs 30.3"). Earthwood depth varies by listing (6.5–6.625", one claim of 8.25").
