# Chitarra sarda (chiterra sarda, "chitarrone", "chitarra gigante")

> Research note for the physically-modelled guitar plugin. **Research conditions:** every WebFetch to the relevant
> domains (wikipedia.org, fingerpicking.net, lanuovasardegna.it, sardegnacultura.it, accordo.it, itenovas.com,
> academia.edu, musikalia.it, iris.unica.it, blogspot.com) was **blocked by the network egress proxy**. Everything
> below comes from **search-result snippets** (marked "(snippet)"), plus my own calculations, which are marked
> **INFERRED**. Before relying on any of it, read the primary pages (see §9).

Legend: **DOC** = stated in a published source (snippet-level) · **INFERRED** = my estimate/calculation ·
**FOLK** = popular or journalistic claim with no organological backing, or claims that contradict each other.

---

## 1. Summary

The chitarra sarda is a **real, distinct build and not just a technique**. It is a large-bodied, steel-strung **baritone guitar**
used to accompany the solo *cantadores* of *cantu a chiterra* (canto a chitarra), including the competitive *gara*,
in northern Sardinia (Logudoro, Goceano, Planargia, Gallura, Anglona). The body is bigger than a dreadnought and
the scale runs about 680–705 mm. The metal strings attach to a **tailpiece (cordiera) with a thin floating bridge**,
so it is closer to an archtop/Neapolitan layout than to a pinned flat-top. The whole instrument is tuned **a fourth
(sometimes up to a fifth) below standard**, typically B1–B3. After World War II the standardised model was supplied mainly by
semi-industrial Sicilian workshops (Carmelo Catania of Mascalucia; Gaetano Miroglio of Catania). The name is still
used for the older, smaller Spanish-derived guitars that preceded it. The guitarist accompanies with chords and, at
professional level, with elaborate arpeggios and counterpoint against the voice.

## 2. Geometry & materials

| Parameter | Value | Status / source |
|---|---|---|
| Overall length | **108 cm** (42.5 in) | DOC (snippet) [1] |
| Body (lower-bout) width | **44 cm** (17.3 in) | DOC (snippet) [1]; dreadnought ≈ 39.7 cm for comparison (my figure, not sourced here) |
| Body depth ("altezza") | **11 cm** (4.3 in) | DOC (snippet) [1]; not stated whether this is at the heel or the tail |
| Scale length | **705 mm (27.76 in)** [1] vs **"about 680 mm" (26.8 in)** [2] | DOC (snippet), two sources disagree; probably real variation between makers. Use 680–705 mm |
| String count | 6 (single courses) | DOC (implied by the 6-note tuning [2]; Angeli's prepared version starts from "a six-string Sardinian guitar" [5]) |
| Strings | **Metal** | DOC (snippet) [1][3] |
| String anchoring | Metal strings fixed to a **cordiera (tailpiece) at the lower band/end**, passing over a **thin (movable/floating) bridge** on the top | DOC (snippet) [1]; the English rendering in the snippets varies ("lower string-holder combined with a thin bridge" / "thin mobile bridge") |
| Nut width | not found | GAP |
| Body length (body only) | not found. INFERRED ≈ 50–55 cm: 108 cm overall minus about 53–58 cm of neck and head for a 705 mm scale | INFERRED |
| Top | Solid spruce (Engelmann) on one current commercial example (Musikalia "Chitarra Sarda di liuteria – grandi dimensioni") | DOC (snippet) [6], for one maker only |
| Back / sides | Mahogany (Musikalia) | DOC (snippet) [6], one maker |
| Neck | 3-piece mahogany (Musikalia) | DOC (snippet) [6], one maker |
| Bracing ("incatenatura") | Only generic snippet text: spruce bars "according to the luthier's experience". **No Sardinian-guitar-specific bracing pattern was found.** With a tailpiece and floating bridge the top carries mostly downward bridge pressure rather than bridge torque, so ladder or light transverse bracing, as on period Italian/Sicilian steel-string "chitarre", is plausible, but this is **INFERRED** | GAP / INFERRED |
| Decoration | Plastic pickguard ("battipenna") let slightly into the top, inlaid with floral motifs in imitation mother-of-pearl | DOC (snippet) [1][4] |
| Fret markers | Markers at the III, V, VII, IX and XII spaces, described as placed to highlight the intervals most used in the cantu a chiterra repertoire. One blog calls them "squared-out" markers | DOC (snippet) [4][8]. The significance claim is FOLK-ish: these are also the standard positions |
| Density / E of woods | none published for this instrument | GAP; use generic spruce and mahogany values |

Body volume, **INFERRED**: V ≈ k·L·W·D. Calibrate k on a dreadnought (L 50.8, W 39.7, D 12.4 cm, commonly taken
as ≈ 16–17 L, which gives k ≈ 0.65). The chitarra sarda (L ≈ 53, W 44, D 11) then comes to V ≈ 0.65·53·44·11 ≈ **16.7 L**.
That is about the same as a dreadnought, because the body is wider but shallower. The uncertainty is ±20%, mostly from the unknown body length and depth taper.

Fret positions for the plugin (12-TET): x_n = L(1 − 2^(−n/12)). The 12th fret sits at 352.5 mm on a 705 mm scale and at 340 mm on a 680 mm scale.

## 3. Tuning(s)  (A4 = 440 Hz, 12-TET)

The accordatura "a sa sarda" is described as "on average a fourth below" [4]. Another source says it has "settled
between a fourth and a fifth below classical standard tuning" [1][3]. In practice the pitch is set to suit the singer.

| String (6→1) | Standard-a-4th-down (DOC [2][4]) | Hz | A-5th-down (DOC range edge [1]; INFERRED notes) | Hz | Intermediate, 1 semitone above the 5th-down (INFERRED example) | Hz |
|---|---|---|---|---|---|---|
| 6 | B1 (Si) | 61.74 | A1 | 55.00 | Bb1 | 58.27 |
| 5 | E2 (Mi) | 82.41 | D2 | 73.42 | Eb2 | 77.78 |
| 4 | A2 (La) | 110.00 | G2 | 98.00 | Ab2 | 103.83 |
| 3 | D3 (Re) | 146.83 | C3 | 130.81 | Db3 | 138.59 |
| 2 | F#3 (Fa#) | 185.00 | E3 | 164.81 | F3 | 174.61 |
| 1 | B3 (Si) | 246.94 | A3 | 220.00 | Bb3 | 233.08 |

Musical context: the core form is *canto in Re* ("boghe in re"). Other named modes include *Mi e La*, *Si bemolle*,
*Fa diesis*, *Filugnana* and *Gadduresa* [7]. These are named keys or chord-shapes. With the guitar a fourth down, a
"Re" (D) shape sounds in A. Whether the names refer to shapes or to sounding pitch is not settled in the snippets (GAP).
Note that the Wikipedia statement "B E A D F# B" [2] is simply standard tuning transposed down a fourth. Some players
may use a non-uniform "sarda" scordatura, but no such variant was found.

## 4. Strings

- **DOC:** metal strings [1][3]. No set marketed specifically for the chitarra sarda, and no maker spec sheet, was found.
- **INFERRED:** a 27–28 in baritone acoustic set fits the tuning and scale. Example gauges from search snippets:
  Elixir 80/20 baritone **.016 .022 .030w .047 .059 .070** [9] (snippet); D'Addario XTAPB1670 baritone "16–70" [10] (snippet).
- **Tension estimates, INFERRED.** Formula T = UW·(2·L·f)²/386.4 (lb, in, Hz). UW for plain steel is (π/4)d²·0.283 lb/in³, and for wound strings I used ≈ 0.85× the solid-steel UW, which is a rough bronze-wound fill factor. Tuning B1–B3 with the Elixir gauges:

| String | Gauge (in) | T @ 680 mm | T @ 705 mm |
|---|---|---|---|
| B1 | .070w | 26 lb / 116 N | 28 lb / 125 N |
| E2 | .059w | 33 lb / 147 N | 36 lb / 158 N |
| A2 | .047w | 38 lb / 167 N | 40 lb / 179 N |
| D3 | .030w | 27 lb / 121 N | 29 lb / 130 N |
| F#3 | .022 | 27 lb / 122 N | 29 lb / 131 N |
| B3 | .016 | 26 lb / 115 N | 28 lb / 123 N |

  The total is roughly 180 lb (≈ 800 N), similar to a medium-gauge dreadnought. That is consistent with the tailpiece
  design, which carries the pull without bridge torque. Older instruments tuned down to A were probably strung with ordinary
  guitar strings and ran at much lower tension: slacker, with a darker, more "jangly" sound. This is FOLK/INFERRED and not documented.
- **Inharmonicity, INFERRED:** B = π³·E·d⁴/(64·T·L²).
  - Plain B3 (.016 in, E = 200 GPa, T ≈ 123 N, L = 0.705 m): **B ≈ 4×10⁻⁵**.
  - Wound B1: the core diameter is unknown. A 0.026–0.030 in core gives **B ≈ 3–5×10⁻⁴**, and that figure is an upper bound, so treat it as order-of-magnitude.
  - Because the scale is longer than on a dreadnought, B for the same notes comes out lower than on a standard guitar tuned down.

## 5. Body / structural resonances

**No measured modal data exist for the chitarra sarda** (none found). All values below are **INFERRED**.

| Mode | Freq (Hz) | Q | Status / derivation |
|---|---|---|---|
| A0 (air/Helmholtz, coupled) | **≈ 90–105** | 15–30 | INFERRED. Rigid-box Helmholtz f = (c/2π)·√(A/(V·L_eff)) with V ≈ 16.7 L and an assumed Ø 100 mm soundhole (A = 78.5 cm², L_eff ≈ t + 1.7r ≈ 8.8 cm) gives ≈ 126 Hz. Coupling to the top lowers that by ~20% in flat-tops, which gives ~95–105 Hz, similar to a dreadnought. **Soundhole diameter is unknown.** If the body is 20% larger, A0 moves down by ×1/√1.2 ≈ −9% |
| T(1,1)₁ top "monopole" | ≈ 170–220 | 20–40 | INFERRED from dreadnought-class flat-top ranges scaled for a wider lower bout (f ∝ 1/W² for a plate of equal stiffness: (39.7/44)² ≈ 0.81). The tailpiece and floating bridge remove bridge rotation stiffness, which probably lowers this mode further |
| T(1,1)₂ (A0/T coupled upper) | ≈ 220–260 | 20–40 | INFERRED |
| Cross-dipole (0,2)/(1,2) | ≈ 280–380 | 30–60 | INFERRED |
| Back (1,1) | ≈ 180–240 | 20–50 | INFERRED (mahogany back, one maker) |
| Neck first bending | ≈ 100–180 | 20–50 | INFERRED. The long neck (~70 cm scale plus head) should put this lower than on a standard acoustic. It can couple with low B1 and E2 partials |

Plugin guidance: seed the mode bank as a dreadnought's, with **A0 ≈ 98 Hz** and **T1 ≈ 190 Hz**. Treat the open low
strings as sitting **below** A0: the B1 fundamental is at 61.7 Hz, so the body gives little support to the fundamentals of
strings 6 and 5. The perceived bass then comes mainly from the 2nd and 3rd partials, which is typical of baritone flat-tops.

## 6. Excitation / playing technique

- The guitar provides harmonic support: chords under the cantadore, **arpeggiated** figures, and at professional level
  "variety in arpeggiation techniques and extensive counterpoint with the voice" [7] (snippet).
- Solo guitar interludes and competitions exist. Aldo Cabitza (1929–2013) developed **note-against-note** accompaniment, building on
  the style of **Adolfo Merella**. Other canonical players: **Nicolino Cabitza** (Aldo's father) and Ignazio Secchi; later **Tore Matzau** [11] (snippet).
- The guitarist has to follow the singer's melodic variations in real time with chord changes [3] (snippet).
- Whether it is played with a pick or fingers is **not documented** in the snippets. The pickguard suggests plectrum strumming and flatpicked arpeggios at least historically (INFERRED).
- "Chitarra a pedale" / pedal mechanism: **no evidence** exists for this in the traditional instrument. Hammers, pedals, propellers and drone strings
  belong to **Paolo Angeli's prepared chitarra sarda** (18–25 strings) [5], a modern one-off and not the folk build. The only
  "pedal" in the tradition would be a musical pedal note, and even that was not documented in anything I found.
- Attack and transient measurements: none found (GAP).

## 7. Distinctive spectral traits

No acoustic or spectral study of the chitarra sarda was found. The traits below are **INFERRED** from the construction:
- **Tailpiece and floating bridge:** the top is driven mainly by vertical force at the bridge, with little torque. This gives
  lower radiation efficiency for the fundamentals, a faster initial decay, and a more mid-forward "archtop/Neapolitan" colour
  than a pinned flat-top of the same size. It is also likely to show a **tailpiece resonance** (the afterlength between bridge and tailpiece)
  in the few-hundred-Hz to kHz range.
- Baritone range: the fundamentals of the three lowest strings lie **below A0**. The spectral centroid of the low strings should be
  set by the body modes at 100–400 Hz rather than by the fundamental.
- Long scale combined with steel: moderate inharmonicity (see §4), long sustain on the plain strings, and a brighter
  upper-partial content than nylon.
- Sustain/T60, centroid and formant regions: **all GAP**.

## 8. Sources

All of these are search-result **snippets only**. WebFetch was blocked for every one of them.

1. https://www.fingerpicking.net/le-chitarre-della-tradizione-popolare-italiana/ — (snippet) 108 × 44 × 11 cm, diapason 70.5 cm, metal strings on tailpiece plus thin bridge, tuning a 4th–5th down, inlaid pickguard; post-war Sicilian lutherie (Carmelo Catania, Mascalucia; Gaetano Miroglio, Catania) standardised the "chitarrone"/"chitarra gigante".
2. https://en.wikipedia.org/wiki/Chiterra_sarda — (snippet) large-bodied baritone guitar, body larger than a dreadnought, scale ≈ 680 mm, tuned a 4th down B E A D F# B; names chitarrone and chitarra gigante.
3. https://www.itenovas.com/in-scena/491-strumenti-della-tradizione-sarda-sa-ghitarra-chitarra-musica-sardegna.html — (snippet, attribution among the results is uncertain) metal strings, larger than a classical guitar; "giant guitars" established around the 1930s for their lower pitch; guitarist follows the cantadores' variations.
4. http://chitarraedintorni.blogspot.com/2009/01/speciale-paolo-angeli-la-chitarra-sarda.html — (snippet) larger body, carved floral pickguard, "a sa sarda" tuning on average a 4th below, fret markers at III/V/VII/IX/XII chosen for the repertoire's intervals.
5. https://en.wikipedia.org/wiki/Paolo_Angeli — (snippet) prepared Sardinian guitar built on a 6-string chitarra sarda (18–25 strings, hammers, pedals, propellers); Angeli's thesis "La gara di canto. Il canto a chitarra nella Sardegna settentrionale" and his book "Canto in Re" (with 5 CDs of 1922–1967 recordings).
6. https://www.amazon.it/Musikalia-Chitarra-Sarda-liuteria-dimensioni/dp/B00SFJ44MI and https://www.musikalia.it/it/catalogo/scheda_strumento.asp?ID=44 — (snippet) Musikalia chitarra sarda: solid (Engelmann) spruce top, mahogany back and sides, 3-piece mahogany neck.
7. https://www.sardegnacultura.it/en/articles/canto-a-chitarra — (snippet) canto in Re and other modes (Mi e La, Si bemolle, Fa diesis, Filugnana, Gadduresa); guitar "a variant with larger dimensions"; arpeggiation and counterpoint; amateur vs professional levels, gara with 3–4 cantadores.
8. https://fingercooking.blogspot.com/2014/09/ho-incontrato-la-chitarra-sarda.html — (snippet) player's account: inlaid pickguard, "squared-out" fret markers, body larger than a dreadnought, used for canto in Re.
9. https://www.amazon.com/Elixir-Strings-Acoustic-Baritone-016-070/dp/B003BG7JIQ — (snippet) Elixir baritone gauges 16-22-30-47-59-70 (generic baritone set, used as a proxy).
10. https://www.daddario.com/products/guitar/acoustic-guitar/xt-phosphor-bronze/16-70-medium-baritone-xt-phosphor-bronze/ — (snippet) D'Addario XTAPB1670 baritone 16–70 (generic proxy).
11. https://it.wikipedia.org/wiki/Aldo_Cabizza and https://www.lacanas.it/novas/2015/tore-matzau-il-virtuoso-della-chitarra/ — (snippet) Aldo Cabitza (1929–2013), Nicolino Cabitza, Adolfo Merella, Ignazio Secchi, Tore Matzau; note-against-note accompaniment.
12. https://www.lanuovasardegna.it/tempo-libero/2018/06/09/news/arrivata-dalla-spagna-e-diventata-sarda-1.16945640 — (snippet) guitar of Spanish origin; guitars became "giant" to get a lower tone; fret-marker distribution changed to highlight tonal intervals; 16th-century "chitarrari" in the Oristano carpenters' guild statute.
13. https://www.lanuovasardegna.it/tempo-libero/2025/02/28/news/stefano-mura-il-liutaio-di-sassari-amato-dalle-star-della-musica-1.100668675 — (snippet) Stefano Mura (Sassari) described as the only current professional maker of chitarre sarde. The article also claims the baritone guitar was "imported from Sicily in the 1960s" (see the conflict below).
14. https://www.academia.edu/4356745/La_Chitarra_Sarda_preparata_La_chitarra_Angeli — Stefano Aresu, organological essay on Angeli's prepared guitar. Only the title and abstract snippet were seen.

**Documented vs folklore / conflicts:**
- **Scale:** 705 mm [1] vs ≈ 680 mm [2]. Both are plausible; neither gives a measured sample.
- **Dating of the "giant" guitar:** ~1930s [3] vs "imported from Sicily in the 1960s" [13] vs "post-war Sicilian standardisation" [1]. The most coherent reading is that larger guitars appeared from the 1930s and the Catania/Mascalucia factory model became standard after WWII. The recordings in Angeli's "Canto in Re" (1922–1967) would settle this.
- **"Markers placed to highlight intervals"** [4][12]: III/V/VII/IX/XII is the ordinary modern marker set. The claim may reflect older guitars that had fewer or no markers. Treat it as FOLK until checked against photos.
- **Spanish origin** and 16th-century chitarrari [12] are a documented archival reference (Oristano guild statute), but they concern guitars in general, not this build.

## 9. DATA GAPS / paywalled

- **Primary organology not accessed.** Key works (probably in print or paywalled; only titles or snippets seen):
  - P. Angeli, *Canto in Re. La gara a chitarra nella Sardegna settentrionale* (book with 5 CDs), plus his DAMS Bologna thesis.
  - Pietro Sassu's writings on *La musica sarda*; nothing specific on the guitar was found.
  - I. Macchiarella (Univ. Cagliari) publications, which focus on multipart singing; no guitar organology found.
  - Andrea Deplano: nothing found.
  - Renato Meucci, "Chitarra – Chitarrone" (academia.edu), which may cover the naming.

  → Obtain Angeli's *Canto in Re* for measurements, a dated typology and the tuning practice.
- **Scale length conflict (680 vs 705 mm).** → Owner measurement: nut to 12th fret × 2 on a Catania/Miroglio instrument and on a modern Mura instrument.
- **Nut width, body length, depth taper, soundhole diameter** (the soundhole drives A0). → Measure with calipers; for volume, do a rice or bead fill, or model it from a photo traced over the outline.
- **Bracing pattern.** → Inspection-mirror photos through the soundhole, or maker (Mura) documentation.
- **Wood density and E.** → Maker specs; tap and weigh an offcut.
- **Actual string gauges and tensions used by players.** → Ask the players or Mura; mic the strings. Also check whether any Italian maker (Galli, Dogal) sells a "set per chitarra sarda".
- **All resonance data (A0, T1, back, neck, tailpiece).** → **Tap-test impulse response** at the bridge, with the strings damped, recorded by a mic about 30 cm from the soundhole. Take a second tap with the soundhole blocked to separate A0 from T1. Take a neck tap at the headstock for the neck modes.
- **Sustain/T60, spectral centroid, attack.** → Record open B1, E2, A2 and B3 plucked with a pick and with a finger in a dry room. Compute T60 per partial and the centroid over time. Also note the pick or finger technique used in a gara (video study).
- **Tuning practice per key/mode** (whether "Re" means a D shape or sounding D; how the pitch is chosen for each singer). → Source this from Angeli's book or from interviews with players.
