# Composite / graphite-neck bass and extended-range (5/6-string, multi-scale) bass

> Research note for the physically-modelled guitar/bass plugin (waveguide strings + body-mode bank + pickup models).
> **Method caveat:** in this session WebFetch was blocked by the network egress proxy for every domain tried
> (acoustics.org, mdpi.com, semanticscholar, jstage, wikipedia, springer, researchgate, zonguitars.com,
> mulhern.com, arxiv.org, patents.google.com, conforg.fr, wolfterminator.com). **Every sourced number below
> therefore comes from web-search result snippets and is marked "(snippet)".** Nothing here was read from the
> full text of a paper. Values labelled INFERRED are my own calculations, with the formula shown.

---

## 1. Summary

- A **composite-neck bass** is a solid-body electric bass. Its neck is either all carbon-fibre/epoxy ("graphite"), which may be hollow as in Modulus or moulded in one piece with the body as in the Steinberger L2, or a **hybrid** of wood and carbon fibre. Zon calls its hybrid neck "composite": it combines "wood, graphite, spectra and others" and is "tuned" to sound "organic". These necks usually have **no truss rod**.
- For a physical model, the neck's material matters mainly because it sets how strongly the neck vibrates where it holds the string (the "neck conductance"). **Dead spots** happen where a neck bending mode lines up with the fundamental of a fretted note. On wooden basses this is typically **130–150 Hz**, i.e. G-string frets 5–7 (C3–D3) (Fleischer, snippet). A stiffer, lighter carbon-fibre neck moves those modes higher and probably loses less energy in them, so dead spots are expected to be weaker or to move out of the main playing range.
- **Extended-range bass** adds a low B0 (30.87 Hz) and/or a high C3 string. It is often built with a 35" scale, or a fanned 34"→37" scale (Dingwall/Novax), to raise B-string tension and reduce its inharmonicity.
- Main representative instruments: Zon Sonus, Legacy Elite and Hyperbass; Modulus Quantum; Steinberger L2; Status Graphite KingBass; Alembic (the first graphite neck in 1977, built with Modulus/Gould).

## 2. Geometry & materials

### 2.1 Instruments (specs)

| Instrument | Scale | Strings | Nut width | Neck | Fingerboard | Body | Source |
|---|---|---|---|---|---|---|---|
| Zon Sonus 5 | 34" (864 mm) | 5 | 1.875" (47.6 mm); 2.875" (73.0 mm) at fret 24 | Composite, bolt-on | Phenowood, 12" radius, 24 medium frets, graphite nut | — | [2] (snippet) |
| Zon Sonus Special 5 | 34" | 5 | 1.875"; 2.875" at fret 24 | Composite carbon-fibre | Phenowood | — ; 17.5 mm bridge spacing | [3] (snippet) |
| Zon Sonus 5/2 | **35"** (889 mm) | 5 | — | Carbon/graphite, **no truss rod** | Phenowood | 18 mm bridge spacing (the Zon default is 17 mm) | [3] (snippet, Reverb listing) |
| Zon Legacy Elite | 34" | 4/5 (6 exists) | — | Composite **set** neck | Phenowood, 12" radius, 24 medium frets | Two-piece mahogany core + figured top | [4] (snippet) |
| Zon Hyperbass (Manring) | 34" | 4 | — | Composite set neck; depth **0.72" (18.3 mm) at fret 1, 0.86" (21.8 mm) at fret 12** | Fretless Phenowood, 3 octaves (to "fret 32"), 10–16" compound radius | Poplar core + bookmatched curly-maple top | [5] (snippet) |
| Steinberger L2 | 34" | 4 (5-string XL/L2-5 exist) | — | Neck and body moulded as one piece from carbon- and glass-fibre-reinforced epoxy; no truss rod; headless; double-ball-end strings | Composite | ~8 lb (3.6 kg) | [6][7] (snippet). Note: one snippet gave "25.5 in", which belongs to the GL guitar, not the bass. |
| Modulus Quantum | 34" and 35" versions (from the literature; not confirmed this session) | 4/5/6 | — | Hollow carbon-fibre neck-through, based on the Turner patent US 4,145,948 | — | — | [8][9] (snippet) |
| Status Graphite KingBass | 34" (864 mm); 32" (812 mm) option | 4/5 | — | Woven carbon-graphite neck-through | Phenolic, 24 stainless medium-jumbo frets | — ; 19 mm spacing standard, 16.5 mm option | [10] (snippet) |
| Dingwall (Combustion etc.) | **Fanned 37"→34"** (B→G) | 4/5/6 | — | Maple (wood; included here for the multi-scale geometry) | 240 mm radius | — | [11] (snippet) |

History (snippet [8]): Geoff Gould, an aerospace engineer, built the first hollow carbon-fibre bass neck with Alembic, shown in 1977 and bought by John McVie. Modulus Graphite then supplied necks to Alembic and to Fender replacement-neck buyers before making complete instruments. The Turner patent US 4,145,948 (later assigned to Modulus) describes a lower laminate of **channel cross-section with longitudinally oriented graphite-epoxy prepreg**, closed by a neck plate to form a hollow neck. It claims a "high stiffness to density ratio" [9] (snippet).

Zon materials (snippet [12]): the "composite neck is made of a number of materials including wood, graphite, spectra [UHMW-PE fibre] and others, and has been 'tuned' to sound 'organic', rather than sterile and harsh". The necks give playing relief without a truss rod. The fingerboard is Phenowood: birch impregnated with phenolic resin and pressed under heat [3] (snippet). **No layup, stiffness, density or mass figures were found for Zon necks.**

### 2.2 Material constants (for neck-mode scaling)

| Material | ρ (kg/m³) | E along grain/fibre (GPa) | tan δ (loss factor) | Status / source |
|---|---|---|---|---|
| Hard maple (Acer saccharum) | 705 | 12.6 | Not found for this species. Snippet [14]: maple has *higher* damping than spruce. Spruce longitudinal tan δ ranges **0.006–0.100** (lower end ≈ 0.006–0.008 is typical) | ρ and E: [13] (snippet, Wood Database). tan δ: [14] (snippet) |
| Carbon/epoxy, **unidirectional** (~60 % Vf) | ~1500–1600 | ~120–140 (standard-modulus fibre, 0°) | Of order 1e-3 to 3e-3 along the fibre (textbook range; **not verified this session**) | INFERRED/assumed. The T700/T300 UD datasheets I found did not show these values in snippets [15] |
| Carbon/epoxy, **woven fabric** (0/90) | ~1500 | ~55–70 per axis | Higher than UD. VBO (vacuum-bag-only) cure **raises η** compared with autoclave; loss modulus about 4.2× higher near 700 Hz for some modes | η trend: [16] (snippet, Martínez et al., Appl. Sci. 9:4615, 2019, GG280T HTA-3k/DT806R). Absolute η not in snippet |
| CFRP vs spruce, qualitative | — | — | "Carbon fiber composites have significantly less damping compared to tone woods such as spruce"; graphite-laminate soundboards show a "smaller apparent loss coefficient over the entire frequency range" than spruce | [17] (snippet), [18] (snippet, patent) |
| Phenolic-impregnated birch (Phenowood) | not found | not found | not found | Gap |

Specific-modulus ratio (INFERRED): for the same geometry, a beam's bending frequencies scale as √(E/ρ).
- Maple: E/ρ = 12.6e9/705 = 17.9 × 10⁶ m²/s².
- UD CFRP: E/ρ = 130e9/1550 = 83.9 × 10⁶ m²/s², so **f_CF / f_maple ≈ 2.17**.
- Woven CFRP (70 GPa): ratio ≈ 1.59.
- A hybrid neck (wood core + CF skins or rods) sits somewhere between 1.0 and 2.2. The exact value depends on the layup and could not be determined.

## 3. Tunings (A4 = 440 Hz, 12-TET)

| Config | Strings (low → high) | Hz |
|---|---|---|
| 4-string standard | E1 A1 D2 G2 | 41.20, 55.00, 73.42, 98.00 |
| 5-string standard (low B) | B0 E1 A1 D2 G2 | 30.87, 41.20, 55.00, 73.42, 98.00 |
| 5-string, high-C variant | E1 A1 D2 G2 C3 | 41.20, 55.00, 73.42, 98.00, 130.81 |
| 6-string standard (Anthony Jackson "contrabass guitar", BEADGC; first built by Carl Thompson, 1975) | B0 E1 A1 D2 G2 C3 | 30.87, 41.20, 55.00, 73.42, 98.00, 130.81 [19] (snippet) |
| Manring/Hyperbass | Altered tunings via Zon/Hipshot detuners (not enumerated) | — [5] |

Highest fretted fundamentals at fret 24: G-string G4 = 392.0 Hz; C-string C5 = 523.25 Hz.

The typical dead-spot band of **130–150 Hz** corresponds to these fretted notes:
- G2 string, frets 5–7 (C3 130.81 Hz, C#3 138.59 Hz, D3 146.83 Hz)
- D2 string, frets 10–12
- A1 string, frets 15–17
- E1 string, frets 20–22
- B0 string, frets 25–27 (not reachable)
- On a 6-string, the **open C3 string** is exactly in this band, so an open-C dead note is possible on a wooden neck. (INFERRED)

## 4. Strings

- Typical 6-string gauges (high C → low B): **.032, .045, .065, .085, .105, .130** in [19] (snippet). Low-B gauges are commonly .125–.135 [20] (snippet).
- D'Addario **XLB130** (nickel-wound, long scale) B string: **34.5 lb (15.65 kg) at 34"** [21] (snippet).
- Standard 4-string set: .045 .065 .085 .105. A .105 E string is ~34 lb on short scale and 45.2 lb on extra-long scale [20] (snippet).
- Full per-string tension tables were not retrieved. Useful sources: the D'Addario technical reference PDF [22] (the "kevinkastning.com" mirror) and GHS/Rotosound charts [20].

**INFERRED B-string tension against scale length.** Unit mass is back-computed from XLB130 = 34.5 lb at 34":
μ = T/(2Lf)² = 153.5 N / (2·0.8636·30.87)² = **0.0540 kg/m**. At the same pitch T = μ(2Lf)², which gives:

| Scale | T (N) | T (lb) |
|---|---|---|
| 34" (864 mm) | 153.5 | 34.5 |
| 35" (889 mm) | 162.6 | 36.6 |
| 36" (914 mm) | 172.0 | 38.7 |
| 37" (940 mm) | 181.7 | 40.9 |

(This assumes the same string. In practice 35" and 37" sets use slightly different constructions.)

## 5. Structural / neck resonances

Solid-body basses have no air (Helmholtz) mode. The coupling that matters for the model is **neck driving-point conductance (Re{mobility})** at the nut and frets, out of the fretboard plane. Fleischer showed that decay time is **inversely related** to this conductance, and that the bridge is far less mobile than the neck. The string therefore loses energy mainly through the neck [23][24] (snippet).

| Mode / item | Freq (Hz) | Q / damping | Source / status |
|---|---|---|---|
| Wooden bass: neck resonance causing the typical dead spot (reported as the "second mode", neck + body) | **130–150** | Not in snippet | MEASURED, Fleischer, reported via [25] (snippet) |
| Wooden bass: an out-of-plane resonance | **430** | Not in snippet | MEASURED [25] (snippet). Instrument not identified |
| Ratios of the principal operating deflection shapes to the first bending mode f₁ | **≈3.2 f₁, 6.8 f₁, 11.6 f₁** | — | MEASURED/derived, Fleischer "Dead spots of electric basses 1: Structural vibrations" [26] (snippet) |
| Measurement band used by Fleischer | 20–320 Hz (covers the G string up to about fret 20) | — | [24] (snippet) |
| In-plane vs out-of-plane conductance | In-plane is lower | — | [24] (snippet) |
| Instruments Fleischer measured | Five structurally different solid basses, headed and headless, **wood and carbon fibre**, bolted and glued necks (Action Bass, Music Man StingRay 5, Carvin, Headway Riverhead…) | — | [26] (snippet). **The per-instrument CF vs wood numbers were not retrieved** |
| INFERRED: CF-neck equivalent of the 130–150 Hz mode, same geometry | UD CF: **≈280–325 Hz** (×2.17). Woven: **≈205–240 Hz** (×1.59). Hybrid: 150–300 Hz | — | f_CF = f_wood·√((E/ρ)_CF/(E/ρ)_maple). Geometry and mass distribution differ in real necks, so treat this as a rough scaling only |
| INFERRED: modal Q (material-limited upper bound) | — | Maple-like, tan δ ≈ 6e-3: **Q ≈ 170**. CFRP, tan δ ≈ 1–3e-3: **Q ≈ 330–1000** | Q ≈ 1/tan δ. Real installed Q will be lower because of joint friction, the player's hand, and strap/body contact. Fleischer measured with the bass held by a player |

Interpretation for the model (INFERRED):
- Represent the string's nut/fret termination as a frequency-dependent reflection filter. Its loss follows the neck conductance G(f) = Σ modal peaks.
- For a **wood neck**: one dominant peak at 130–150 Hz, Q ~ 30–100 in situ (player-damped; assumed), plus higher ODS peaks at 3.2f₁, 6.8f₁ and so on.
- For a **CF / hybrid neck**: move the peaks up by 1.5–2.2× and reduce the peak conductance, because the neck has lower mass and loss. This makes dead spots move towards or above the top of the fundamental range (392 Hz on a G string), and the reported "even response across the fingerboard" follows.
- A 6-string reaches 523 Hz on the C string, so a CF neck mode near 300 Hz can still fall inside the playing range of the upper strings.

## 6. Excitation / playing technique

- Techniques are fingerstyle (index/middle fingers, plucking near the neck pickup or over the pickups), pick, and slap/pop (thumb strike on the low strings, pull-and-release pop on the high strings). A fretless neck is used on the Hyperbass. Manring also detunes mid-note using Hipshot/Zon tuner levers [5].
- Headless designs such as the Steinberger use double-ball strings. The termination is at the body end, so there is no headstock mass and the neck's cantilever mass is lower. This further raises neck-mode frequencies (INFERRED).
- No attack or transient measurements specific to composite necks were found.

## 7. Distinctive spectral traits

- **Sustain / decay.** Manufacturers claim more sustain, more "brilliance", and "no dead spots" (Steinberger [6], Zon [12]) (snippet). **No peer-reviewed quantitative comparison of T60 for graphite vs wood necks was found**; forum claims only [27].
- INFERRED ceiling on neck-limited string T60 from material loss alone, using T60 ≈ 6.91/(π f tan δ): at 140 Hz, tan δ 6e-3 gives 2.6 s; 2e-3 gives 7.9 s; 1e-3 gives 15.7 s. These numbers describe the *structural mode's own* decay. The string's decay at a dead spot depends on how strongly the string couples into that mode.
- **Inharmonicity of the B string** (INFERRED). The formula is
  B = π³ E d_c⁴ / (64 T L²), with partial n at f_n = n f₀ √(1 + B n²).
  - Assumptions: steel core E = 200 GPa, effective core diameter d_c = 0.9–1.14 mm (not sourced), and T from §4.
  - At 34": B ≈ 5.6e-4 to 1.4e-3, so the 10th partial is **+47 to +116 cents** sharp.
  - At 35": B ≈ 5.0e-4 to 1.3e-3 (+42 to +104 c).
  - At 37": B ≈ 4.0e-4 to 1.0e-3 (+34 to +84 c).
  - At fixed pitch and string, T ∝ L², so **B ∝ 1/L⁴**. Going from 34" to 35" reduces B by 11 %; going to 37" reduces it by 29 %. This is the physics behind longer B-string scales and fanned frets [11][20].
- Wood necks are described as having a "round/organic" tone and CF as potentially "tinny/metallic". The CF description is attributed to the lower damping of CF in soundboard work [17] (snippet); it is not a measured neck result.

## 8. Sources (all seen as search results only; WebFetch was blocked)

1. (no URL) Proxy note: WebFetch was blocked for all domains in this session. No full texts were read.
2. https://www.zonguitars.com/zonguitars/sonus5.html — Sonus 5: 34", composite bolt-on neck, nut 1.875" and 2.875" at fret 24, Phenowood 12" radius (snippet)
3. https://reverb.com/item/75403708-zon-sonus-5-2-carbon-graphite-neck-5-string-bass-w-case-35-scale ; http://www.zonguitars.com/zonguitars/sonusspecial5.html — Sonus 5/2 at 35" with 18 mm spacing and no truss rod; Sonus Special 17.5 mm spacing; Phenowood description (snippet)
4. https://bass-review.blogspot.com/2011/03/zon-legacy-elite-4-4-string-bass.html ; https://www.guitar-list.com/zon/bass-guitars/zon-legacy-elite — Legacy Elite: 34", composite set neck, mahogany body (snippet)
5. http://www.mulhern.com/Zon2004/bits_and_pieces/catalog_sheets/hyperbass.pdf ; https://www.zonguitars.com/zonguitars/hyperbassspecs.html — Hyperbass: 34" fretless, 3 octaves, neck depths 0.72"/0.86", compound radius (snippet)
6. https://bassmusicianmagazine.com/2020/10/one-of-the-most-radical-instruments-ever-made-the-steinberger-l2/ — L2: one-piece carbon/glass-fibre moulding, no truss rod, "no dead spots" claim (snippet)
7. https://www.premierguitars.com.au/products/steinberger-l2-bass-usa-1982 — L2: 34" scale, ~8 lb (snippet)
8. https://en.wikipedia.org/wiki/Modulus_Guitars — Gould/Alembic 1977 prototype, Modulus history (pointer only, snippet)
9. https://patents.google.com/patent/US4145948A/en — Turner patent: hollow channel section with longitudinal graphite/epoxy (snippet)
10. https://www.johnfoxbass.com/product-page/status-kingbass-artist-4-string-32-scale-graphite-neck ; https://equipboard.com/items/status-graphite-kingbass-paramatrix — Status KingBass scales and spacing (snippet)
11. https://dingwallguitars.com/bass/combustion/ ; https://bassdirect.co.uk/bass_guitar_specialists/Dingwall_Combustion_5_SH.html — Novax fanned 37"→34", 240 mm radius (snippet)
12. https://www.zonguitars.com/zonguitars/faq.html ; https://www.zonguitars.com/zonguitars/necks.html — composite neck of "wood, graphite, spectra and others", "tuned" to sound organic, no truss rod (snippet)
13. https://www.wood-database.com/hard-maple/ — hard maple 705 kg/m³, E = 12.62 GPa (snippet)
14. https://arxiv.org/pdf/1902.10977 — spruce longitudinal tan δ 0.006–0.100; maple has higher damping than spruce (snippet)
15. https://agate.niar.wichita.edu/Materials/WP3.3-033051-132.pdf — Toray T700/#2510 UD tape datasheet (only resin content and areal weight appeared in the snippet)
16. https://doi.org/10.3390/app9214615 — CFRE prepregs for instruments: VBO raises η, loss modulus ×4.2 near 700 Hz (snippet)
17. https://www.academia.edu/85743028/An_Overview_of_Fibre_Reinforced_Composites_for_Musical_Instrument_Soundboards — Damodaran et al. 2015 review; CF has less damping than spruce; "tinny/metallic" (snippet)
18. https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/5469769 — graphite-laminate soundboard has a smaller loss coefficient than spruce (snippet)
19. https://www.guitarworld.com/artists/bassists/anthony-jackson-six-string-bass-origins ; https://basstuner.io/tuning/6-string-standard — BEADGC tuning, 1975 Carl Thompson, gauges .032–.130 (snippet)
20. https://www.cmuse.org/bass-string-gauge-calculator/ ; https://sevenstring.org/threads/long-scale-length-low-tuning-string-tension-flexibility-construction-and-inharmonicity.326034/ — B gauges .125–.135; E-string tension against scale; stiffness/inharmonicity discussion (snippet)
21. https://fretnation.com/products/daddario-xl-nickel-plated-steel-bass-single-string-long-scale-130-xlb130 — XLB130 at 34.5 lb (snippet)
22. http://www.kevinkastning.com/D'Addario_tension_chart.pdf — D'Addario tension reference (not read)
23. https://www.researchgate.net/publication/239291877_Diagnosing_dead_spots_of_electric_guitars_and_basses_by_measuring_the_mechanical_conductance — conductance "landscape"; out-of-plane conductance > in-plane (snippet)
24. https://www.researchgate.net/publication/281639288_Vibration_of_an_Electric_Bass_Guitar — Fleischer, Acta Acust. 91 (2005) 246–260; 20–320 Hz band; bridge less mobile than neck (snippet)
25. https://www.bestbassgear.com/ebass/gear/hardware/tuner/treating-your-basses-deadspot.html ; https://wolfterminator.com/dead-spot-eliminator-electric-bass/ — dead spot neck resonance 130–150 Hz, G string frets 5–7; a 430 Hz out-of-plane resonance (snippet; secondary, attributed to Fleischer)
26. https://www.researchgate.net/publication/281633135_Dead_spots_of_electric_basses_1_Structural_vibrations — five basses including carbon-fibre ones; ODS at 3.2/6.8/11.6 × f₁ (snippet)
27. https://www.talkbass.com/threads/graphite-neck-versus-wood-neck.54852/ ; https://premierguitar.com/articles/25479-bass-bench-neck-joints-science-and-sound-opinions — anecdotal sustain comparisons only
28. https://www.researchgate.net/publication/282790879_Fleischer_H_und_Zwicker_T_Mechanical_vibrations_of_electric_guitars — Fleischer & Zwicker, Acustica 84 (1998) 758–765 (snippet: energy flows via the neck and causes faster decay)

## 9. DATA GAPS / paywalled

- **Fleischer's per-instrument conductance values (s/kg), f₁ and Q, including his carbon-fibre basses.** These are in: Fleischer, "Dead spots of electric basses" parts 1–2 (UniBw München reports, 2000); Fleischer, *Acta Acustica* 91:246–260 (2005); Fleischer & Zwicker, *Acustica* 85:128–135 (1999). They are open on ResearchGate but could not be fetched. *To close this:* tap the nut and frets with an impact hammer (or a light pencil tap) while an accelerometer or a contact piezo sits at the same point, with the bass held in playing position. Take the FFT, compute Re{V/F}, and pick peaks and −3 dB Q between 20 and 500 Hz.
- **Absolute CFRP loss factor** (UD and woven) at audio frequencies: Martínez et al. 2019 [16] and Ono et al. (*Acoust. Sci. Tech.* 23(3):135–142, 2002) contain the values but were not readable here. The 1e-3 to 3e-3 in §2.2 is an assumed textbook range. *To close this:* do a free-free beam tap test on a CF strip (a spare neck rod), measure T60 of the first bending mode and use η = 6.91/(π f T60).
- **Maple tan δ:** a specific value was not retrieved (the 6e-3 in §5 is an assumption). *To close this:* same free-free strip test on a maple blank.
- **Zon neck construction** (layup, mass, stiffness, the Sonus vs Legacy differences, the "Hybrid neck" / "Composite Resin" wording): not published or not found. *To close this:* weigh a detached Zon bolt-on neck; measure static deflection under a known load at the nut to get EI; tap-test the free neck for f₁.
- **Graphite vs wood sustain:** no controlled study was found. *To close this:* record the same note (for example G-string fret 5, 6 and 7, and each open string) on a wood-neck and a CF-neck bass with the same strings, measure T60 per partial, and compare with the conductance map.
- **Full string-tension tables** for 5- and 6-string sets at 34/35/37" (D'Addario PDF [22], GHS [20]): not read. *To close this:* use the manufacturer unit weights with T = μ(2Lf)².
- **B-string core diameters** (needed for inharmonicity): not found. *To close this:* record the open B, fit f_n = n f₀√(1+Bn²) to the measured partials, and read B directly.
- **Modulus Quantum / Alembic graphite-neck specs** (scale options, neck mass): only historical snippets were found.
