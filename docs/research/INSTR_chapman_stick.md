# Chapman Stick (Stick Enterprises): research notes for physical modelling

Covers: The Stick (10-string), Grand Stick (12-string), Railboard (aluminium), Stick Bass (8-string). Proxy: Warr / Megatar touch guitars.

> **Method caveat.** In this session WebFetch was blocked for every host (stick.com, the Stick Enterprises store, Wikipedia, arXiv, IRCAM, Google Patents). **Everything below comes from search-result snippets**, and each such item is marked **(snippet)**. Primary pages were not read in full. Numbers marked INFERRED are my own estimates, and the formula is shown next to each.

---

## 1. Summary

The Chapman Stick is a solid, beam-shaped electric instrument that Emmett Chapman devised around 1969–74. It has no body cavity and is played only by two-handed tapping ("Free Hands"): both hands come at the fretboard from opposite sides, fingers perpendicular to the strings. The strings are split into two groups:
- **Melody group:** tuned in descending 4ths.
- **Bass group:** tuned in ascending 5ths.

In both groups the lowest string sits in the middle of the neck. Each group has its own pickup and output, so the instrument is stereo (bass and melody).

Every note starts with a hammer-on onto a very flat, low-action fretboard. A string damper (foam) near the nut stops the open segment from ringing. The note sustains only while the finger holds the string down.

Current models are 36" scale. Older production models were 34". Materials over the years: ironwood → injection-moulded polycarbonate → laminated hardwood, laminated bamboo, graphite (XG) → CNC-machined aluminium (Railboard).

## 2. Geometry & materials

| Item | Value | Status / source |
|---|---|---|
| Scale length, current Stick / Grand Stick / Stick Bass | 36 in = 914.4 mm | (snippet) [3],[6] |
| Scale length, older production | 34 in = 863.6 mm | (snippet) [6] |
| 34" and 36" instruments share the same pitch-to-inlay relationships | — | (snippet) [3] |
| "Guitar-scale" Stick instruments (Alto / SG-type) | 26.5 in = 673 mm nut to bridge; 25 in = 635 mm from the first tappable "X-fret" to the bridge | (snippet) [3] |
| String count | 10 (5+5), 12 Grand (6+6 or 7+5 melody+bass), 8 Stick Bass | (snippet) [2],[3] |
| Fretboard | Very flat, very little relief, "stainless steel pyramidal fret rails", very low action | (snippet) [13] |
| Railboard neck beam | 3/4 in × 3 1/2 in (19.05 × 88.9 mm) aluminium block. Frets are CNC-machined integral with the board, 90° pointed profile, hard-anodised. Patent US8324489B1 | (snippet) [9],[10] |
| Nut width / string spacing | NOT FOUND | gap |
| Bridge | Individual height adjustment per string. Bridge sets the height and spacing at the first support point, which "principally determines the action over the entire fretboard" | (snippet) [13] |
| Truss rod | Adjustable since 1989. Pre-1989 ironwood and polycarbonate Sticks have none, so light gauge only | (snippet) [2],[4] |
| Materials, by era | Super-hardwoods, mostly ironwood (some ebony), until early 1980s → injection-moulded polycarbonate until early 1990s → laminated hardwoods and laminated bamboo (current) → "Stick XG" structural graphite / continuous-strand carbon fibre (2001 to late 2000s, discontinued) → Railboard aluminium | (snippet) [3],[11] |
| Overall length, mass | NOT FOUND | gap |

**Material constants.** These are generic handbook values, not measured on Sticks (INFERRED):

| Material | ρ kg/m³ | E (along grain/axis) GPa | c = √(E/ρ) m/s |
|---|---|---|---|
| Aluminium 6061 | 2700 | 69 | 5055 |
| Ironwood / dense tropical hardwood | ~1000–1200 | ~18–22 | ~4200 |
| Laminated bamboo | ~650–750 | ~10–14 | ~4000 |
| Polycarbonate (unfilled) | 1200 | 2.4 | 1414 |
| Unidirectional carbon composite | ~1550 | 100–130 | ~8500 |

## 3. Tunings (A4 = 440 Hz, 12-TET)

The **octave placement is INFERRED**. Here is how I placed it:
- Wikipedia (snippet) gives the range of a 36" Stick in Matched Reciprocal as **C1–C6**.
- That fits a lowest bass string of C1 (32.70 Hz) and a highest melody string of C4, reaching C6 at fret 24.
- It also fits the string gauges and tensions in §4. The bass is too thin for a C2 low string: a .095 at 36" would need about 97 lbf at C2, against about 24 lbf at C1.
- A Stick Enterprises page (snippet) says the Classic tuning "retains … a low E at the very low end as on the customary 4-string bass guitar" [15], and the matching snippet quotes the bass group as "6-low C, 7-G, 8-D, 9-A and 10-E" [15].

**Please confirm the octaves against stick.com tuning charts.**

String numbering: 1–5 are melody, with 1 the highest, on the outer (treble) edge. 6–10 are bass, with 6 the lowest, near the centre (snippet) [1],[15].

### 10-string Classic (the original tuning)

Melody descends in 4ths from the edge; bass ascends in 5ths from the centre (snippet) [1],[5].

| Str | Note | Hz | | Str | Note | Hz |
|---|---|---|---|---|---|---|
| 1 | D4 | 293.66 | | 6 | C1 | 32.70 |
| 2 | A3 | 220.00 | | 7 | G1 | 49.00 |
| 3 | E3 | 164.81 | | 8 | D2 | 73.42 |
| 4 | B2 | 123.47 | | 9 | A2 | 110.00 |
| 5 | F#2 | 92.50 | | 10 | E3 | 164.81 |

### 10-string Matched Reciprocal

The bass is the same as Classic. The melody is a whole step lower: C G D A E (snippet) [14]. Each fret then carries the same note letters on both sides.

| Melody | C4 261.63 | G3 196.00 | D3 146.83 | A2 110.00 | E2 82.41 |
|---|---|---|---|---|---|

Bass: C1 32.70, G1 49.00, D2 73.42, A2 110.00, E3 164.81.

### Baritone Melody and Deep Baritone Melody

- **Baritone Melody:** the "first variation" on Classic. The bass is unchanged and the melody is lowered (snippet) [14]. The exact interval was NOT FOUND (gap).
- **Deep Baritone Melody:** a further whole step below Baritone Melody (snippet) [14].

### 12-string Grand Stick

- **6+6:** Classic plus one extra *high* string on each side (snippet) [2]. INFERRED from the interval pattern:
  - melody adds G4 (392.00 Hz) above D4;
  - bass adds B3 (246.94 Hz) above E3.
- **7+5** is also offered (snippet) [2].
- A **Matched Reciprocal** variant exists for the Grand: bass up in 5ths, melody in 4ths (snippet) [2].
- "Dual Bass Reciprocal" is another tuning (YouTube title) [2].

### Stick Bass 8-string

A snippet says it "can be strung in standard bass tuning in 4ths starting with a low B" (snippet) [16]. That would be B0 30.87, E1 41.20, A1 55.00, D2 73.42, … and is INFERRED as a 4ths stack.

A book title refers to "8-String Chapman Stick in ADGCF#BEA" (snippet) [16]. That string of notes is ambiguous for 8 strings.

The exact factory tuning was NOT FOUND (gap).

### Railboard

The Classic tuning is offered for Railboard, The Stick and Grand Stick (snippet) [8]. So Railboard tunings = Stick tunings.

## 4. Strings

- **Stick Enterprises sets:** Light, Medium and Heavy gauges for each standard tuning. Nickel-plated steel wraps over hex cores (the Newtone snippet describes this for its Stick-style sets) (snippet) [4],[7].
- **Pre-1989 instruments** without a truss rod (ironwood, polycarbonate): Light gauge only (snippet) [4].
- **Per-string gauges from Stick Enterprises:** NOT OBTAINED. The store pages [7],[12] were blocked.

Proxy gauge tables from other touch-instrument makers (snippet):

| Source | Melody set (high → low) | Bass set (low → high) |
|---|---|---|
| Megatar, "inverted fifths" Stick-style [17] | .009 .011 .012 .016 .029W .040W | .095W .080W .060W .030W .016 .010 |
| Clic Music "Medium fingers" 12-str [18] | .009 .009 .011 .011 .014 .018 .027W .038W | .110W .080W .048W .032W .028W .016 |
| Clic Music "Power+" 12-str [18] | .010 .010 .013 .013 .019 .026 .036 .048 | .120 .090 .052 .030 .016 .012 |

**Tension estimates (INFERRED).** Formula: T[lbf] = UW[lb/in] · (2·L[in]·f)² / 386.4. Unit weights (UW) are approximate D'Addario-style values.

| String | Gauge | Unit weight (lb/in) | Tension at 36", 32.7 Hz (bass C1) | Tension at 36", 293.66 Hz (melody D4) |
|---|---|---|---|---|
| Plain steel | .010 | 2.2e-5 | — | ≈ 25.6 lbf (114 N) |
| Nickel wound | .095 | ~1.7e-3 | ≈ 24 lbf (108 N) | — |

- Both tensions are low compared with a normal guitar or bass. That suits tapping: a low-tension string is easy to drive onto a low fret, and the tapping force stays small.
- If the low string were C2 instead, the same .095 would need about 97 lbf. That is implausible, and it is why I place the bass at C1.

**Inharmonicity (INFERRED).** Formula: B = π³·E·d_core⁴ / (64·T·L²), with E_steel = 200 GPa.
- Melody .010 (d = 0.254 mm), T = 114 N, L = 0.914 m: **B ≈ 4×10⁻⁶** on the open string. Fretted at fret n, B scales by 4^(n/12), so it is 4× at fret 12.
- Bass .095 with an assumed hex core of about 0.040" (1.02 mm), T = 108 N: **B ≈ 1.1×10⁻³** open, and about 4.5×10⁻³ at fret 12. That is audible stretched partials in the bass zone.

## 5. Body / structural resonances

This is a solid beam instrument with **no air cavity**, so there is **no Helmholtz mode**. Structural modes matter only through the bridge/nut admittance, which shows up as dead spots and sustain variation. They are not a radiating body filter. In a plugin, model them as a few weak, low-Q notches or extra decay on partials near those frequencies, not as a body EQ.

No measured modal data for any Stick was found. All rows below are **INFERRED**.

Assumptions:
- Free-free Euler-Bernoulli beam: f_n = (β_nL)²/(2πL²) · √(E/ρ) · h/√12, with (β_nL)² = 22.37, 61.67, 120.9.
- Overall length L ≈ 1.2 m (assumed). The beam is bending through its thickness h.
- Real boundary conditions: strapped to a belt hook and leaned on the body, which lowers the Q.

| Mode | Railboard Al (h = 19.05 mm) | Laminated hardwood/bamboo (h ≈ 25 mm assumed) | Polycarbonate (h ≈ 25 mm) | Graphite XG (h ≈ 25 mm) | Q | Status |
|---|---|---|---|---|---|---|
| Bending 1 (thickness) | ≈ 69 Hz | ≈ 70–75 Hz | ≈ 25 Hz | ≈ 145 Hz | Al: high (≥100, metal) until damped by player contact; wood ~30–60 | INFERRED |
| Bending 2 | ≈ 190 Hz | ≈ 200 Hz | ≈ 70 Hz | ≈ 400 Hz | " | INFERRED |
| Bending 3 | ≈ 370 Hz | ≈ 390 Hz | ≈ 135 Hz | ≈ 780 Hz | " | INFERRED |
| Torsion 1 | ≈ 500 Hz | — | — | — | — | INFERRED (see calculation below) |
| Width-direction bending 1 | ≈ 320 Hz | — | — | — | — | INFERRED (scales by b/h ≈ 4.7 × bending 1) |
| Helmholtz / plate modes | none (solid) | none | none | none | — | by construction |

Railboard Torsion 1 calculation: f₁ ≈ c_t/(2L), with c_t = √(G·J/(ρ·I_p)), G = 26 GPa, J ≈ 0.29·b·h³, I_p = b·h·(b² + h²)/12. This gives c_t ≈ 1210 m/s.

The unfilled polycarbonate Sticks may have had metal reinforcement. That is unknown, and the figures assume none.

Implication for the plugin: bass notes near 33–75 Hz can couple with Bending 1 and 2. Expect note-dependent sustain in the lowest bass register. That is the classic "dead spot" behaviour.

## 6. Excitation / playing technique

**Free Hands technique** (snippet) [19],[20]:
- Chapman worked it out in August 1969.
- Both hands tap perpendicular to the neck from opposite sides, which gives them equal counterpoint capability.
- He published it as the method book *Free Hands: A New Discipline of Fingers on Strings* in 1976.
- After committing to tapping, Chapman "added a string damper by the nut and lowered the action for the lightest touch" (snippet) [19].
- Current instruments have pads near the nut to mute open strings (snippet) [19].

**Sound production, as a physical sequence** (mechanism from general string physics; INFERRED where not cited):
1. The fingertip strikes the string just behind (nut side of) the target fret and drives it through the action gap. That gap is very small, but no value was found (gap).
2. **The note starts at string-fret contact.** The vibrating segment (fret → bridge) is suddenly clamped while it still has the finger's transverse velocity near the fret. This is effectively a velocity/displacement excitation located *a few mm from the new termination*.
   - Modal amplitudes follow ∝ sin(nπx₀/L) with x₀ ≈ finger-to-fret distance (≈ 3–15 mm).
   - So the low harmonics are weakly excited, and the spectrum rises about 6 dB/oct in velocity up to n ≈ L/(2x₀). For L = 600 mm and x₀ = 10 mm, n ≈ 30.
   - Result: a weak fundamental relative to a pluck and a "thin, even" attack with no pluck-position comb notches in the audible range.
   - Waveguide implementation: inject a short velocity pulse (or a step in displacement) right next to the fret end of the delay line. Do not use a triangular pluck shape at 1/5 of the length.
3. **Low excitation energy.** Travel is limited to roughly action height plus fret height (about 1–2 mm, INFERRED), and the finger impact is soft. Output level is therefore lower than a pluck, which is why Stick pickups are described as "very sensitive" (snippet) [13].
   - Dynamic range comes from tapping velocity. Finger contact is compliant, like a felt hammer. A Hunt-Crossley / power-law contact F = K·δ^α with α ≈ 1.5–2.5 is a reasonable finger model. The piano-hammer literature uses p ≈ 2.5–4 for felt (snippet) [22].
4. **The nut-side segment** (fret → nut) is also set moving by the impact. On a Stick it is killed by the finger pad plus the foam damper at the nut. Model it as a very short, very lossy secondary waveguide, or ignore it with a small "thump" component.
5. **Sustain lasts only while the finger holds.** The finger stays on the string as a soft damping point just behind the fret, which adds a little loss.
6. **Release** (pull-off) acts like a mini pluck of the open string, but the nut foam damps it, so it is heard as a short, low-level muted blip. This is characteristic of tapped instruments.
7. **Low action and string-fret collisions** add buzz and brightness at high tapping velocity. See the collision models below.

**Stereo zones.** The bass and melody groups each have a pickup and their own volume and tone controls:
- "The Block" (1998) is a removable module, active or passive, with a TRS stereo output; bass is on the tip.
- A stereo/mono switch folds melody into the bass output.
- Internal trim pots set the mono balance (snippet) [21].
- Pickup options: EMG ACTV-2; Villex passive quad PASV-4 (four selectable Villex pickups, four-position filters, stereo/mono); R-Block Villex (passive or active, XLR adaptor cables for DI); the original "Stickup" (snippet) [21],[23],[24].
- Pickup position from the bridge, inductance and resonance: NOT FOUND (gap).

**Relevant physical-model literature** (touch, fret collision, hammer-on):
- **Evangelista 2011, "Physical model of the string-fret interaction" (DAFx-11)** [25] (snippet).
  - Vertical polarisation: governed by clamping against the fret.
  - Horizontal polarisation: governed by Coulomb friction. "Very moderate amounts of friction" give realistic results.
- **Evangelista & Eckerholm 2010, "Player–instrument interaction models for digital waveguide synthesis of guitar: touch and collisions", IEEE TASLP 18(4)** [26] (snippet).
  - Scattering junctions in a waveguide for finger/pick touch, neck-side fingers (harmonics), and string-fret or fingerboard collision.
  - **This is the closest ready-made framework for a tapped-string waveguide.**
- **Bilbao & Torin 2014, "Numerical simulation of string/barrier collisions: the fretboard" (DAFx-14, best paper)** [27] (snippet).
  - FDTD with tension modulation, distributed collision against fretboard and individual frets, and a finger-stopping collision model.
  - Parameters K_f, α_f and loss Ξ_f; values not retrieved.
  - Follow-up: "Numerical modeling and sound synthesis for articulated string/fretboard interactions" (JAES 2015).
- **Bilbao, Torin & Chatziioannou 2015, "Numerical modeling of collisions in musical instruments", Acta Acustica u. Acustica 101:155–173** (arXiv 1405.2589) [22] (snippet).
- **Issanchou, Le Carrou, Touzé, Fabre, Doaré: "String/frets contacts in the electric bass sound: simulations and experiments" (JASA/Applied Acoustics)**, and **"Nonsmooth contact dynamics for the numerical simulation of collisions in musical string instruments", JASA 143(5):3195 (2018)** [28],[29] (snippet).
  - Measured and simulated bass slap/pop, where the string hits frets.
  - The best available experimental validation of string-fret contact for a low-tension, low-action bass-register string.
- **Kramer, Abeßer, Dittmar & Schuller, ICASSP 2012, "A digital waveguide model of the electric bass guitar including different playing techniques"** [30] (snippet).
  - 11 techniques in a modular waveguide.
  - Code: github.com/jakobabesser/bass_guitar_waveguide_model [31].
  - Reusable for fret-side excitation and dead-note/muting modules.
- No paper was found that directly measures **hammer-on/tap versus pluck spectra**, and no acoustic paper on the Chapman Stick itself (searched, nothing found).
- The Hammer-on Wikipedia article [32] and PMC12609871 (sensor comparison of "struck" vs plucked strings on a simplified electric guitar) [33] are the nearest; I did not read the latter's content.

## 7. Distinctive spectral traits

Nothing here is measured. INFERRED unless cited.

- **Attack.**
  - Softer and less "picky" than a plucked note.
  - Excitation sits right at the termination, so there is no comb-notch pattern and the fundamental is relatively weak at onset (§6).
  - A short broadband "tap" thump comes from finger impact and fret contact.
  - The attack gets brighter with tapping velocity: harder impact means a shorter contact and more fret/string collision.
- **Spectral centroid.** Expect a lower centroid than a pick on guitar and higher than a thumb on bass.
  - Magnetic pickups are close to the bridge. Position unknown; if placed in the last ~30–50 mm, the first pickup comb notch lands at n ≈ L/d ≈ 12–25.
  - Low action with buzz adds energy at high tapping velocity.
- **Inharmonicity.** Negligible in the melody zone (B ≈ 10⁻⁶–10⁻⁵). Significant in the bass zone (B ≈ 10⁻³ open, rising up the neck). See §4.
- **Sustain / T60.** Not measured. Two things shorten it compared with a picked electric:
  - lower string tension, so a relatively larger fraction of energy goes to the supports;
  - the finger damping point at the fret while the note is held.

  A tapped note also stops when released: effectively zero release tail, beyond the damped open-string blip. Suggested plugin defaults to tune by ear (pure estimate): T60 of about 4–8 s for bass-zone low notes and about 2–4 s for melody-zone mid notes.
- **Formants.** No body formants, since it is a solid beam with direct magnetic pickup. Timbre is shaped by pickup position, pickup resonance and the Block's tone filters. The PASV-4 has "four-position filters" (snippet) [23].
- **Two independent timbral zones.** The bass group is in 5ths from C1 and the melody group in 4ths from about D4 down. They overlap around E2–E3 and have separate stereo outputs. In a plugin, treat them as two instruments with independent EQ and pan.

## 8. Sources

WebFetch was blocked, so every item below was seen only as a search-result title or snippet, as noted.

1. https://stick.com/tunings-and-tech/stick-tunings/10-string-classic/ — Classic melody D-A-E-B-F# (4ths), bass C-G-D-A-E (5ths) (snippet)
2. https://stick.com/tunings-and-tech/stick-tunings/ — list of tunings; Grand Stick 6+6 or 7+5; Matched Reciprocal on Grand (snippet)
3. https://en.wikipedia.org/wiki/Chapman_Stick — 36"/34" scales, C1–C6 range, materials by era, guitar-scale 26.5"/25" (snippet)
4. https://newtonestrings.com/shop/tapping-string-sets-for-chapman-stick%EF%B8%8F-style-instruments/ — Light/Medium/Heavy sets, nickel-plated steel on hex core; pre-1989 light only (snippet)
5. https://www.stickist.com/tunings.php — Stick tuning reference (title only)
6. https://www.talkbass.com/threads/chapman-stick.738173/page-3 — "Stick, Grand Stick, Stick Bass are 36" scale, older production 34"" (snippet, via search summary)
7. https://stick-enterprises-store.myshopify.com/collections/string-sets — official string sets (blocked; snippet only)
8. https://stick.com/instruments/tunings/10/classic/ — Classic available on Railboard, Stick and Grand Stick (snippet)
9. https://stick.com/instruments/railboard/ — Railboard product page (title only)
10. https://patents.google.com/patent/US8324489B1/en — Railboard patent: aluminium, CNC-integral frets, hard-anodised, 3/4" × 3 1/2" beam (snippet)
11. https://musical-instruments.fandom.com/wiki/Chapman_Stick — XG graphite 2001 to late 2000s; laminated hardwood/bamboo (snippet)
12. https://stick-enterprises-store.myshopify.com/products/12-string-set-medium-gauge — Grand Stick medium set (title only)
13. https://stick.com/instruments/tech/setup/ — flat board, little relief, stainless pyramidal fret rails, very low action, per-string bridge height, truss behaviour (snippet)
14. https://stick.com/tunings-and-tech/stick-tunings/10-string-matched-reciprocal/ — MR = Classic bass plus melody a whole step lower; Baritone and Deep Baritone Melody (snippet)
15. https://stick.com/instruments/tunings/chapman_06_06/ — origin of the Classic tuning; "6-low C, 7-G, 8-D, 9-A, 10-E"; low-E note (snippet)
16. https://us.amazon.com/Visual-Guide-Scales-8-String-Chapman/dp/1541353544 — 8-string "ADGCF#BEA" title; 4ths from low B statement from search summary (snippet)
17. http://www.megatar.com/blog/2014/8/20/string-guages — Megatar inverted-5ths gauges (snippet)
18. https://www.clicmusic.be/index.php/shop/strings-for-the-tap-guitar/ — Clic Music tap-guitar gauges (snippet)
19. https://en.wikipedia.org/wiki/Emmett_Chapman — Aug 1969 technique; damper by nut; lowered action (snippet)
20. https://en.wikipedia.org/wiki/Free_Hands — Free Hands method, 1976 book (snippet)
21. https://stick.com/instruments/pickups/actv2/ — Block (1998), TRS stereo, mono switch, trim pots (snippet)
22. https://arxiv.org/pdf/1405.2589 — Bilbao/Torin/Chatziioannou collisions review; hammer power law p ≈ 2.5–4 (snippet)
23. https://stick.com/instruments/pickup-modules/ — EMG, Villex, Stickup; PASV-4 (snippet)
24. https://www.facebook.com/StickEnterprises/photos/a.10153513534590327/10153513545520327/?type=3 — R-Block Villex passive/active, XLR adaptors (snippet)
25. http://recherche.ircam.fr/pub/dafx11/Papers/96_e.pdf — Evangelista, string-fret interaction model (snippet)
26. https://ieeexplore.ieee.org/document/5446590/ — Evangelista & Eckerholm, IEEE TASLP 2010 (snippet)
27. https://www.dafx14.fau.de/papers/dafx14_stefan_bilbao_numerical_simulation_of_s.pdf — Bilbao & Torin DAFx-14 fretboard collisions (snippet)
28. https://www.researchgate.net/publication/319034313_Stringfrets_contacts_in_the_electric_bass_sound_Simulations_and_experiments — Issanchou et al. (snippet)
29. https://www.lam.jussieu.fr/Membres/LeCarrou/Articles/A24_Issanchou_NonsmoothContactDynamics.pdf — Issanchou et al., JASA 2018 (title/snippet)
30. https://ieeexplore.ieee.org/document/6287889/ — Kramer et al., ICASSP 2012 bass waveguide, 11 techniques (snippet)
31. https://github.com/jakobabesser/bass_guitar_waveguide_model — code for [30] (title)
32. https://en.wikipedia.org/wiki/Hammer-on — technique definition (title)
33. https://pmc.ncbi.nlm.nih.gov/articles/PMC12609871/ — sensor comparison of plucked vs struck string waveforms (snippet)
34. https://jedistar.com/pdf/warr_brochure_2003.pdf — Warr touch-style brochure (proxy); Warr features from Premier Guitar and Equipboard snippets: Bartolini stereo pickups, 14° headstock, graphite reinforcement (snippet)

## 9. Data gaps / paywalled

- **Exact per-string gauges and tensions for Stick Enterprises sets** (Light, Medium, Heavy × Classic, MR, Baritone; Grand; Stick Bass). The store was blocked.
  - Fix: fetch the store product pages, or read the gauge label on a set packet.
- **Octave placement of the tunings.** Inferred from the C1–C6 range and from tension. Baritone Melody interval and the Stick Bass 8 tuning were not confirmed.
  - Fix: stick.com tuning chart pages [1],[2],[14].
- **Nut width, string spacing, overall length, mass, beam cross-section** for the wood, bamboo and graphite models.
  - Fix: owner measurement with calipers and a scale, or the spec pages.
- **Action height and fret height** (only "very low" found).
  - Fix: owner feeler-gauge measurement at frets 1, 12 and 24 for strings 1, 5, 6 and 10.
- **Structural modes and Q of any Stick.** All inferred.
  - Fix: tap test. Hang the instrument free-free on elastic, tap with a light hammer, record with an accelerometer or phone mic at the bridge, and read peaks and −3 dB widths. Repeat with the instrument strapped on.
  - Also check dead spots: sweep a chromatic scale on the lowest bass string and measure T60 per fret.
- **Pickups:** position from the bridge, inductance, resonant peak, Block filter curves.
  - Fix: measure the distance from the bridge saddle to the pickup centre. Measure DC resistance and inductance with an LCR meter, or read the impedance spectrum with a sweep through the TRS jack.
- **Tapping excitation measurements** (force, velocity, contact time; tap vs pluck spectra; attack time).
  - No published measurement found for the Stick, the Warr, or guitar hammer-ons.
  - Fix: record DI (both channels) of the same note tapped at soft, medium and hard, and plucked at 1/5 L. Compare the onset spectrum over the first 20 ms, the partial amplitudes after 100 ms and the attack time.
  - A piezo film under a fingertip, or a force-sensing resistor, could give contact time and force.
- **Sustain / T60 per string and register.** Not measured.
  - Fix: DI recordings of held taps on every string at frets 0 (if possible), 5, 12 and 17. Fit a per-partial exponential decay.
- **Paywalled or not read in full:**
  - Evangelista & Eckerholm, IEEE TASLP 18(4), 2010, doi via https://ieeexplore.ieee.org/document/5446590/
  - Issanchou et al., JASA 143(5):3195, 2018
  - Kramer et al., ICASSP 2012
  - Bilbao & Torin, JAES 2015, "Numerical modeling and sound synthesis for articulated string/fretboard interactions"

  The DAFx-11 and DAFx-14 PDFs and arXiv 1405.2589 are open access but were blocked here. They are the first things to read for the collision stiffness and exponent values (K, α) and friction coefficients.
- **No academic acoustic study of the Chapman Stick or Warr guitar was found.**
