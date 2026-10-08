# Tenor guitar: research notes for a physical model

> **Method note.** WebFetch was blocked by the egress proxy for every host tried (wikipedia, daddario, stewmac, unsw, arxiv, savartjournal, elixir, blogspot, and others). So **every externally sourced number below comes from a search-result snippet** and is marked "(snippet)". Numbers I derived are marked **INFERRED** and show their formula. The calculation script was a scratch file. Its formulas are reproduced here in full.

## 1. Summary

The tenor guitar is a 4-string steel-strung guitar with a short scale (about 21-23 in / 533-584 mm). It appeared in the 1920s so that tenor-banjo players could switch to guitar. Its standard tuning is the tenor-banjo tuning in fifths, C3-G3-D4-A4. Flat-top examples are the Martin 5-17T, 5-18T and 0-18T. Archtop examples are the Gibson TG-50 and the Epiphone Blackstone tenor. The National Triolian tenor is a single-cone resonator. From the 1960s onward, Irish and folk players have tuned it GDAE (G2-D3-A3-E4), an octave below the fiddle or mandolin. Jazz and pop players often use "Chicago" tuning, DGBE (the top four strings of a guitar). Its sound is a small-body steel-string guitar sound: a higher air mode than a 6-string, a narrow and bright spectrum, and usually a flat pick.

**Disambiguation: plectrum guitar.** The plectrum guitar is a separate 4-string relative with a longer **26-27 in** scale. It is tuned like the plectrum banjo, **CGBD** (C3 G3 B3 D4) or DGBD. It is rarer than the tenor (snippet, [9], [10]).

## 2. Geometry and materials

| Model | Scale | Nut width | Lower bout | Depth | Other | Source |
|---|---|---|---|---|---|---|
| Generic tenor | 21-23 in (533-584 mm) | n/a | n/a | n/a | n/a | [1] (snippet) |
| Martin 0-18T (1957-66 examples) | **23 in (584 mm)** | 1 1/4 in (31.8 mm) | 13 1/2 in (343 mm) | 4 1/8 in (105 mm) at end block | overall length 35 1/2 in (902 mm); spruce top, mahogany back/sides (Style 18) | [2] (snippet) |
| Martin size 5 (5-17T / 5-18T) | 5-17T: 23 in, some 21 in; **5-18T: 21.4 in (544 mm)** | 1 1/4 in | 11 1/4 in (286 mm) | 3 7/8 in (98 mm) | body length 16 in (406 mm), 12 frets to body | [3] (snippet) |
| Gibson TG-50 (archtop) | **23 in** | 1 1/8 in (28.6 mm) | **16 in (406 mm)** | n/a | carved spruce top, arched maple back/sides, 1-piece mahogany neck; based on the L-50 | [4] (snippet) |
| National Triolian tenor (1928-38) | **23 in (584 mm)** | 1 1/8 in (29 mm) | 12 3/4 in (324 mm) | 3 1/4 in (83 mm) | overall length 34 in (864 mm); steel body with a single cone. The Style O is brass-bodied and brighter | [5] (snippet) |
| Epiphone Blackstone tenor (1932-49) | about 23 in (typical tenor figure given) | n/a | n/a | n/a | carved spruce top, figured maple back/sides | [6] (snippet) |
| Eddie Freeman Special (Epiphone) | 25.5 in | n/a | larger body | n/a | re-entrant CGDA | [10] (snippet) |

On the brief's "22.9 in" for the Martin 0-18T: listings consistently give 23 in (584 mm) [2]. 22.9 in (581.7 mm) is probably a nominal nut-to-saddle figure. **Use 584 mm as the default model value.**

**Body volume (INFERRED).** V ~ k_shape x lower bout x body length x mean depth, with k_shape ~ 0.78 (a guess, not sourced) and 1 in³ = 16.387 cm³.
- 0-size: 0.78 x 13.5 x 17.4 x 3.9 in ~ 715 in³ ~ **11.7 L**. The body length of 17.4 in and the mean depth of 3.9 in are both assumed.
- Size 5: 0.78 x 11.25 x 16 x 3.6 in ~ 505 in³ ~ **8.3 L**.
- Reference dreadnought: assumed **17.5 L** (not sourced).

**Woods.** Top: Sitka or Adirondack spruce. Back/sides: mahogany (Martin 17/18), rosewood (Martin 21, 28), maple (Gibson and Epiphone archtops). I found no density or E data for these specific instruments (see Gaps). Typical literature values should come from the project's general tonewood notes.

## 3. Tunings (A4 = 440 Hz, 12-TET)

| Name | Notes | Hz | Notes on use |
|---|---|---|---|
| **Standard (tenor-banjo)** | C3 G3 D4 A4 | 130.81, 196.00, 293.66, 440.00 | Fifths. Chord-melody and jazz rhythm [1][10] |
| **Chicago / baritone-uke** | D3 G3 B3 E4 | 146.83, 196.00, 246.94, 329.63 | Top four guitar strings. Tiny Grimes [10] |
| **Irish GDAE / octave mandolin** | G2 D3 A3 E4 | 98.00, 146.83, 220.00, 329.63 | One octave below fiddle or mandolin. Used in Irish trad from about 1960 [11] |
| Plectrum (other instrument) | C3 G3 B3 D4 | 130.81, 196.00, 246.94, 293.66 | 26-27 in scale [9] |
| Freeman re-entrant CGDA | C3 G3 D4 A3 (A string dropped an octave) | 130.81, 196.00, 293.66, 220.00 | 25.5 in scale [10] |

Other tunings that players mention (GDGD, open tunings) are in [8]. I did not verify them further.

Range: the open lowest string is 98 Hz (GDAE) or 130.8 Hz (CGDA). The top string at about the 19th fret reaches roughly 1.3 kHz (E4 + 19 semitones = B5, 987.8 Hz; A4 + 19 = E6, 1318.5 Hz). **The fret count is unverified.**

## 4. Strings

### Published set gauges (all snippets)

| Set | Tuning | Gauges (low to high, in) | Material | Source |
|---|---|---|---|---|
| **D'Addario EJ66 Tenor Guitar** | CGDA | .032w .022w .014 .010 | 80/20 bronze wound, plain steel | [7] |
| Eagle Puretone Tenor Guitar "Standard" | CGDA | .032w .024w .014 .010 | PB or nickel wound | [12] |
| Eagle Puretone Tenor Guitar "Irish" | GDAE | .038w .028w .018w .012 | PB or nickel wound | [13] |
| Pyramid Tenor Guitar GDAE | GDAE | .042 .027 .017 .012 (PB; which strings are wound is not stated) | phosphor bronze | [8] |
| Eastwood/GHS electric tenor | CGDA | .036 .024 .013 .009 | nickel (electric) | [8] |
| Eastwood/GHS electric tenor | DGBE | .032 .024 .016 .011 | nickel | [8] |
| Eastwood/GHS electric tenor | GDAE | .042 .032 .018 .011 | nickel | [8] |
| Forum recommendation | DGBE | .032 .024 .016 .012 | n/a | [10] |
| Forum recommendation (The Session) | GDAE, 23 in | .044w .028w .019p .012 → "about 19 lb per string, about 76 lb total" | PB | [14] |

**D'Addario EJ63 is not a tenor-guitar set.** It is a *tenor banjo* set (.010 .016 .023w .030w, nickel, loop end, CGDA) [15]. The brief assumed otherwise. Do not use it for tenor-guitar defaults.

A rule of thumb from the same forum sources: about 20 lb per string, about 80 lb total, for a typical tenor guitar [14]. About 25 lb per string is quoted for acoustic tenors and about 15 lb for short-scale electrics [8].

### Tensions (INFERRED)

I could not fetch D'Addario's unit-weight chart (see Gaps).

- Formula: T = mu (2 L f)² / 386.09, with T in lbf, mu in lb/in, L in in and f in Hz.
- Plain steel: mu = 0.2836 x pi d² / 4 (steel density 0.2836 lb/in³).
- Wound strings: mu ~ 0.82 x the plain-steel mu of the same outer diameter. This fill factor is my assumption.
- **Validation:** for the 12-19p-28w-44 GDAE set on 23 in, this method gives 76.0 lb total. The published figure is about 76 lb [14], so they agree.

| Set @ 23 in | Per string (low to high), lbf | Total |
|---|---|---|
| EJ66, CGDA | 17.5, 18.6, 20.6, 23.6 | 80.4 |
| Eagle standard, CGDA | 17.5, 22.1, 20.6, 23.6 | 84.0 |
| GHS, CGDA | 22.2, 22.1, 17.8, 19.1 | 81.3 |
| GHS, DGBE | 22.1, 22.1, 19.1, 16.0 | 79.4 |
| 32-24-16-12, DGBE | 22.1, 22.1, 19.1, 19.1 | 82.4 |
| Eagle Irish, GDAE | 13.9, 16.9, 15.7, 19.1 | 65.6 |
| Pyramid, GDAE (.017 assumed plain) | 17.0, 15.7, 17.1, 19.1 | 68.9 |
| 44w-28w-19p-12, GDAE | 18.6, 16.9, 21.3, 19.1 | 76.0 |

At 21.4 in (Martin 5-18T), tension scales by (21.4/23)² = 0.866. For example, EJ66 gives about 69.6 lb total.

For the model: 1 lbf = 4.448 N, so per-string tensions are **about 60-105 N**.

**Inharmonicity, plain strings (INFERRED).** B = pi³ E d⁴ / (64 T L²), with E = 200 GPa and L = 0.584 m:
- .010 at A4 (T = 105 N): B ~ 1.1e-5
- .012 at E4 (85 N): B ~ 2.9e-5
- .014 at D4 (92 N): B ~ 5.0e-5
- .016 at B3 (85 N): B ~ 9.1e-5

Wound strings need core diameters, which were not found. For bronze-wound strings, published studies show poor agreement between the simple formula and measured B [16]. **Measure these.** Higher tension lowers B [16].

## 5. Body and structural resonances

No published measurement of an actual tenor guitar body was found. The values below are anchored to measured full-size guitars and scaled with the Helmholtz relation f ∝ sqrt(A / (V · L_eff)). At constant soundhole area, f_air(V) = f_ref · sqrt(V_ref / V).

| Mode | Instrument | Freq (Hz) | Q | Status and source |
|---|---|---|---|---|
| Air (Helmholtz, coupled A0/T(1,1)1) | Dreadnought (reference) | 90-110 (prewar Martin D: about 90 Hz, F-F#; typical D: about G2, 98 Hz) | n/a | MEASURED, forum luthier reports (snippet) [17] |
| Air | Classical guitar (reference) | 90-110 | n/a | MEASURED, forum (snippet) [18] |
| Air | Tenor **ukulele** (much smaller body, upper bound) | 211, 214 | n/a | MEASURED, luthier (snippet) [18] |
| Air | **Martin 0-size tenor** (0-18T, V ~ 11.7 L) | **~110-122** | est. 10-25 | INFERRED: (90-100) x sqrt(17.5/11.7). Matches the brief's 100-120 Hz |
| Air | **Martin size-5 tenor** (V ~ 8.3 L) | **~130-145** | est. 10-25 | INFERRED: (90-100) x sqrt(17.5/8.3). A smaller hole would lower this |
| Air | Gibson TG-50 archtop (16 in bout, f-holes) | ~95-120 | n/a | INFERRED, weak. f-holes have less area than a round hole of the same body, which lowers f. No data |
| Air / cone | National Triolian | unknown | n/a | GAP. A resonator-guitar measurement paper exists (ISMA 2019) [19] but was not fetched |
| Top T(1,1)2 ("main top") | Generic flat top | ~Helmholtz + 100 Hz, about an octave above air. The back is within about a semitone of it | n/a | MEASURED rule (snippet) [18][17] |
| Top T(1,1)2 | 0-size tenor | **~210-240** | est. 20-40 | INFERRED: about 1.9-2.0 x f_air |
| Back (0,0) | 0-size tenor | ~200-250 | est. 20-40 | INFERRED, within about a semitone of the top |
| Top T(1,1)2 | size-5 tenor | ~250-290 | n/a | INFERRED |
| Generic flat-top peaks | 6-string | about 110 and about 200 Hz peaks, with an anti-resonance near 125 Hz | n/a | Patent literature (snippet) [20] |
| First duct/cavity mode | Martin D-28 | 383 Hz | n/a | MEASURED (snippet) [17]. A 0-size should be higher: ×(20/17.4) ≈ 440 Hz, INFERRED from body-length ratio |

**Q values** have no source here. The values in the table are placeholders for the mode bank. Replace them with fitted values from an owner tap test (see Gaps).

**Coupling:** the air and the first top mode form a coupled pair. Measured air frequencies already include this coupling, so treat the numbers above as *coupled* peaks [21].

Consistency check: tenor ukulele at about 3 L gives 95 x sqrt(17.5/3) ~ 229 Hz. The measured values are 211-214 Hz, so the V^-1/2 scaling is plausible over this range.

**Model implication.** With CGDA, the 0-size air mode (about 115 Hz) sits **below the lowest open string (C3, 131 Hz)**, so it reinforces only the fundamentals of the lowest notes weakly. With GDAE, G2 (98 Hz) is below the air mode, and the 2nd harmonic of G2 (196 Hz) falls near the top mode. This helps explain why GDAE on small tenors is often called thin or "floppy" at the bottom [15].

## 6. Excitation and playing technique

- **Mostly flat pick (plectrum).** The instrument came from tenor-banjo practice. Styles include rhythm chording (jazz and dance bands of the 1920s-30s, often archtops), chord-melody in fifths, and single-note melody with ornamentation in Irish GDAE (triplets are common, as in tenor-banjo practice) [1][11].
- **Plectrum mechanics:** the string does not slide along the plectrum surface. It skips over the edge (Zollner, plucking an E4 string with a pick 125 mm from the bridge) [22] (snippet). Sound is highly sensitive to plucking position, plectrum thickness and plucking angle. At low plucking depth the string is not fully excited [23] (snippet). Two-polarization string motion under a robotic plectrum has been measured with LDV [24].
- **Pluck position for the model (INFERRED):** a typical position is about 1/5 to 1/7 of the string length from the bridge. On 584 mm that is about 85-120 mm, consistent with Zollner's 125 mm on a 6-string.
- **Attack:** a pick attack is brighter and shorter than a finger attack. I found no rise-time numbers (see Gaps).

## 7. Distinctive spectral traits

- **Measured tenor-specific data:** none found (no spectral centroid, T60 or formant data).
- **Short scale and lower tension** (60-105 N versus about 70-120 N for a 6-string's matching strings) combined with a small body give:
  - weaker low-frequency radiation below about 110-140 Hz;
  - a relatively stronger 200-400 Hz body region.

  (INFERRED from sections 4 and 5.)
- Plain-string B values of 1e-5 to 1e-4 are typical of steel guitars. Inharmonicity is perceivable mainly on the lowest (wound) strings of steel guitars [16].
- Higher partials, which are the most inharmonic, decay faster than lower partials [16] (snippet). No tenor T60 values were found.
- **Resonator (National):** a strongly different spectrum driven by the cone. See [19]. The data is a gap.

## 8. Sources

All are search-result snippets or titles seen in search results. None were fetched, because WebFetch was blocked.

1. https://en.wikipedia.org/wiki/Tenor_guitar : tenor scale 21-23 in; CGDA standard; TG-50 introduced, based on the L-50 (snippet).
2. https://www.retrofret.com/product.asp?ProductID=10224 (and ProductID=9706, 11892; https://www.mandolincafe.com/ads/234027) : Martin 0-18T scale 23 in, nut 1 1/4 in, 13 1/2 in bout, 4 1/8 in depth, overall length 35 1/2 in (snippet).
3. https://jakewildwood.blogspot.com/2017/03/1929-martin-5-17t-tenor-guitar.html ; https://www.vintageguitar.com/19313/martin-5-18/ ; https://umgf.com/questions-about-martin-tenor-guitars-t165466.html : size 5 body 11 1/4 x 16 x 3 7/8 in; 5-18T 21.4 in; 5-17T 23 in, some 21 in; nut 1 1/4 in (snippet).
4. https://jakewildwood.blogspot.com/2016/04/1960-gibson-tg-50-archtop-tenor-guitar.html ; https://folkwaymusic.com/museum/gibson-guitars/1954-gibson-tg50-0717 : TG-50 16 in bout, 23 in scale, 1 1/8 in nut, spruce/maple/mahogany (snippet).
5. https://notomguitars.com/products/1930-national-triolian-tenor ; https://www.creamcitymusic.com/1928-national-triolian-tenor-resonator-in-walnut-sunburst/ : Triolian tenor 23 in scale, 12 3/4 in bout, 3 1/4 in depth, 1 1/8 in nut, 1928-38 (snippet).
6. https://jakewildwood.blogspot.com/2023/07/1937-epiphone-blackstone-carved-top.html ; https://guitarhq.com/epiphon2.html : Blackstone 1932-49, tenor about 23 in, spruce/maple (snippet).
7. https://www.sweetwater.com/store/detail/EJ66--daddario-ej66-80-20-acoustic-tenor-guitar-strings : EJ66 gauges .010 .014 .022w .032w, 80/20 bronze (snippet).
8. https://eastwoodguitars.com/products/eastwood-ghs-custom-strings-tenor-guitar ; https://www.thomannmusic.com/pyramid_tenor_string_set_gdae.htm ; https://papadafoe.com/tenor-guitar-tunings : GHS gauges per tuning, Pyramid GDAE gauges, 15-25 lb per string guidance, alternate tunings (snippet).
9. https://www.jazzguitar.be/forum/guitar-amps-gizmos/39807-tenor-plectrum-guitar.html ; https://groups.google.com/g/rec.music.makers.guitar.acoustic/c/fkiykDJ_ZOQ : plectrum guitar 26-27 in, CGBD/DGBD (snippet).
10. https://en.wikipedia.org/wiki/Eddie_Freeman_(musician) ; https://forum.ukuleleunderground.com/threads/what-steel-string-set-for-chicago-tuning-on-a-short-scale-tenor-guitar.118276/ : Freeman Special 25.5 in, re-entrant; Tiny Grimes DGBE; Chicago gauges 32-24-16-12 (snippet).
11. https://thesession.org/discussions/26868 ; https://en.wikipedia.org/wiki/Barney_McKenna ; https://www.banjohangout.org/archive/254930 : GDAE is an octave below fiddle, Irish use from about 1960 (snippet).
12. https://www.eaglemusicshop.com/tenor-guitar-strings-c-tuning : Eagle CGDA 32w 24w 14 10 (snippet).
13. https://www.eaglemusicshop.com/prod/acoustic-guitar-strings/Tenor-guitar-strings-G-tuning.htm : Eagle Irish GDAE 12 18w 28w 38w, PB or nickel (snippet).
14. https://thesession.org/discussions/21455 ; https://www.mandolincafe.com/forum/threads/152120-tenor-guitar-strings-for-GDAE-tunning : 12-19p-28w-44 at 23 in is about 19 lb/string and about 76 lb total; about 20 lb/string (80 lb) typical (snippet).
15. https://www.daddario.com/products/ej63-tenor-banjo-strings-nickel-9-30 ; https://folkstrings.com/banjo-string-gauges/ : EJ63 is a tenor banjo set, .010 .016 .023w .030w; CGDA sets are floppy in GDAE (snippet).
16. https://www.researchgate.net/publication/233604568_Perceptibility_of_Inharmonicity_in_the_Acoustic_Guitar ; https://pubs.aip.org/asa/jasa/article/71/S1/S9/716559/Inharmonicity-of-wound-guitar-strings ; https://pubs.aip.org/aapt/ajp/article/90/7/487/2820160/Inharmonicity-in-plucked-guitar-strings : B formula versus measurement for wound strings, perceptibility on low steel strings, higher partials decay faster (snippet).
17. https://umgf.com/resonant-frequencies-of-the-pre-war-martin-dreads-t77729.html ; https://umgf.com/dreadnought-change-in-helmholz-frequency-from-regu-t202207.html : dreadnought air 90-110 Hz, prewar about 90 Hz, D-28 first duct resonance at 383 Hz, two (0,0) body modes about 100 Hz above Helmholtz (snippet).
18. https://ukuleles.com/applying-technology-to-building/understanding-controlling-instrument-resonances/working-with-basic-instrument-resonances/ ; https://www.classicalguitardelcamp.com/viewtopic.php?t=150669 : classical air 90-110 Hz, top 180-200 Hz; tenor ukulele air 211/214 Hz; top about an octave above air, back within a semitone (snippet).
19. https://pub.dega-akustik.de/ISMA2019/data/articles/000038.pdf : "Measurement and modeling of a resonator guitar", ISMA 2019 (title only).
20. https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/6787688 : acoustic guitar peaks about 110 and 200 Hz, anti-resonance about 125 Hz (snippet).
21. https://pubs.aip.org/asa/jasa/article/139/4_Supplement/2012/707480/Measuring-the-low-frequency-response-of-an : air/top coupled oscillator (snippet).
22. https://www.gitec-forum-eng.de/wp-content/uploads/2020/08/poteg-1-5-picking-process.pdf : Zollner, "The plucking process"; the string skips the plectrum edge; E4 plucked 125 mm from the bridge (snippet).
23. https://arxiv.org/pdf/2606.24356 : pluck-trajectory micro-changes; the effect of plectrum material and depth (snippet).
24. https://acta-acustica.edpsciences.org/articles/aacus/full_html/2020/03/aacus200019/aacus200019.html : LDV measurement of plectrum plucking in two polarizations (snippet).

## 9. Data gaps and paywalled material

- **No measured tenor-guitar body response** (air, top or back modes, Q) found anywhere. All tenor mode values are INFERRED.
  - *Fix:* tap test on the owner's instrument. Tap the bridge, put a mic about 10 cm from the soundhole, strings damped, and take the FFT. Then fit f and Q (−3 dB width) for the first 3-5 peaks. Repeat with the soundhole covered to separate the air mode from the top mode.
- **Q / damping** of any small-body guitar mode: not obtained.
  - *Fix:* the same tap test (half-power bandwidth), or an exponential fit to the ring-down of a band-passed impulse response.
- **Body volume and soundhole diameter** of the 0-18T and 5-18T: not found. Body length is assumed at 17.4 in for the 0 size.
  - *Fix:* the owner measures body length, the depth at heel and tail, and the soundhole diameter. Better still, fill the body with a known volume of beads or foam to measure V directly.
- **D'Addario unit weights and tension chart** (daddario.com tension_chart_13934.pdf; mirror at kevinkastning.com) were blocked. Tensions are INFERRED with a 0.82 wound fill factor, validated against one set only.
  - *Fix:* fetch the chart when network access allows, or weigh 1 m of each string (mu = mass / length).
- **Martin tenor set specs:** no official Martin tenor-guitar set was confirmed. The site lists part number 41Y18MA170T, but no gauges were seen.
- **Wound-string core diameters** are needed for B on wound strings.
  - *Fix:* measure the partial frequencies of open strings from recordings and fit f_n = n f0 sqrt(1 + B n²).
- **T60, spectral centroid and attack time** of tenor-guitar notes: none found.
  - *Fix:* record each open string, pick at about 100 mm from the bridge, in a dry room. Compute per-partial decay rates (for the loop filter), spectral centroid versus time, and 10-90% rise time.
- **Archtop (TG-50) and resonator (Triolian) acoustics.** Archtop plate modes: "Modal analysis of free archtop guitar top plates", JASA 150(2):1505 (2021), https://pubs.aip.org/asa/jasa/article-pdf/150/2/1505/15352680/1505_1_online.pdf, possibly paywalled and not fetched. Resonator: ISMA 2019 paper [19], open access but not fetched.
- **Tonewood density and E** for these specific instruments: not found. Use generic spruce, mahogany and maple literature values.
- **Paywalled or unfetched:** Rossing and Fletcher-style tables of steel-string guitar modes (The Science of String Instruments, Springer 2010; a PDF was seen at logosfoundation.org but not fetched); French, "Engineering the Guitar" (Springer); Zollner's full picking chapter [22] is open but was not fetched.
