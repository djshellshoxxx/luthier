# Accuracy audit: existing instrument and part models

Scope: the instruments and parts that already exist. The factory guitars
(`Resources/Guitars`, 28 files), the compiled types (`GuitarLibrary.cpp`, 25),
the body model (`BodyModels.cpp`), the parts→engine mapping
(`PartAcoustics.cpp`), strings (`StringMaterials.cpp`), pickups
(`PickupEngine.cpp`, the pickup parts) and the bridge coupling bank
(`BodyCouplingBank.cpp`). New exotic instruments are out of scope.

Method: every "modelled value" below is what the engine actually computes,
not what the part file says. `AccuracyAudit.dumpDerivedValues`
(`Source/Tests/AccuracyAuditTests.cpp`) prints it for every factory guitar
when `LUTHIER_AUDIT_DUMP=1` is set, so this table can be regenerated:

```
LUTHIER_AUDIT_DUMP=1 ./LuthierTests dumpDerived
```

Verdicts:

- **wrong-model**: the wrong physical model, or a model that cannot produce
  the right behaviour whatever its coefficients.
- **off**: the right model with a coefficient or data value measurably away
  from the reference.
- **accurate**: within the reference's own spread.
- **unverified**: plausible, but no reference in hand. These are listed
  again under "Data gaps".

Counts: 28 factory guitars, 25 compiled types, 20 bodies, 6 tops, 15 necks,
19 pickups, 18 bridges, 3 tailpieces, 18 string sets, 13 string materials and
19 woods were checked. That is **184 items**. There are **9 wrong-model**
findings (A-01, A-02, A-03, A-04, A-06, A-07, A-09, A-10, A-12) and **13 off**;
the rest are accurate or unverified. **5 model fixes are applied** (marked
FIXED), plus one GCC build fix. 19 items are proposals or owner decisions.

## 1. Findings, worst first

| ID | Instrument / part | Modelled value | Reference value + source | Verdict | Recommended fix |
|---|---|---|---|---|---|
| A-01 | **Every magnetic pickup** (`PickupEngine::combSample`) | Comb delay = `2 × position × delaySamples`. `delaySamples` is `sr / f0`, the full period, which is already the round trip 2L/c. So every pickup behaves as if it sat at **twice** its distance from the bridge. The Strat neck pickup at 159 mm (0.245 L) acts as 0.49 L: it sits almost at the string's centre and notches every even harmonic | A pickup at distance d senses `x[n] − x[n − 2d/c]`. 2d/c = (d/L)·(2L/c) = **position × period**. The first null is at f0/position (part-acoustics.md 6.1: `v / (2 × position)`) | **wrong-model** | Proposal: `delaySamples * 2.0` → `delaySamples` in `combSample`. Then re-voice. The compiled `GuitarLibrary` positions (A-16) were evidently tuned by ear against the doubled comb and must be corrected in the same change. Add an engine-level test: the existing `pickupPositionSetsTheComb` checks only the helper formula and the part's position fraction, never the rendered comb, and `positionCombNullsTheExpectedHarmonic` passes with either delay because a doubled comb at 1/4 notches harmonics 2, 4, 6… (test the 2nd harmonic is **not** notched). Not applied: it changes the tone of every electric and bass, and the normalisation/voicing baselines. |
| A-02 | **Ordinary plucks** (`Excitation`, non-exact path) | Pluck-position comb delay = `2 × pluckLen` ≈ 2 × β × period (× contact stretch). The notch lands at f0/(2β), so a pick at 1/6 of the string acts like one at 1/3 | Plucked-string spectrum: harmonics n = k/β are missing, i.e. comb delay **β × period** (Karplus–Strong pluck-position filter `1 − z^(−βN)`). The engine's own harmonic path (`exactPluckComb`) already uses `delaySamples × pos`, which is correct | **wrong-model** | Proposal: use `delaySamples × pos` for every pluck, and keep `pluckLen` for the contact shape only. Re-voice the default pluck positions with A-01. |
| A-03 | **Baritone Electric** (factory) | `baseTypeFor` fell through on body style `offset` to the Jazzmaster type, so the guitar was tuned **E standard**: 14-68 strings on 686 mm at 36–41 lb (160–185 N) each | A baritone is tuned a fourth low (B standard). The compiled `BaritoneElectric` type, and the UI entry that loads this file, both say B. At B the same set is 19–23 lb (84–104 N), which is a normal baritone tension | **wrong-model** | **FIXED.** A body style or name containing "baritone" maps to `GuitarType::BaritoneElectric` (tuning `BaritoneB`). Test `AccuracyAudit.baritoneIsTunedToBStandard`. |
| A-04 | **Full Hollow Archtop, Semi-Hollow 335** (and compiled ES-335) | `BodyShape::Hollow` and `SemiHollow` have `soundHoleMm = 0`, so `computeAirResonance` returns 0 and `buildModes` takes the **solid-plank** branch: 163/236 Hz, Q 10–14. mapSpec computes an air mode (115 Hz archtop, 165 Hz 335) but `airResonanceHz`, `airResonanceQ`, `bodyGainDb` and `finishDampingDb` are never read by the engine. They are only unit-tested | An f-holed archtop has a Helmholtz A0 at about F2 to A♯2 (87–117 Hz). An L-5-style f-hole pair measures 115 Hz ([jazzguitar.be, "F holes in archtop guitars"](https://www.jazzguitar.be/forum/guitar-amps-gizmos/92111-f-holes-archtop-guitars.html)). part-acoustics.md 2.1 specifies 90–140 Hz (hollow) and 140–190 Hz (semi-hollow) | **wrong-model** (an archtop is driven by the solid-body plank modes) | Proposal: give `Hollow` and `SemiHollow` an equivalent f-hole aperture. About 55 mm and 40 mm diameter reproduce 115–125 Hz and about 160 Hz with the existing Helmholtz code. Alternatively, feed mapSpec's `airResonanceHz`/`Q` into `BodyConfig`. Either way, check the coupling bank: a 110 Hz air mode sits on the open A string. Wire or delete the dead `DerivedAcoustics` fields; the tests currently certify values nobody hears. |
| A-05 | **Phosphor-bronze, 80/20, silk & steel, pure-nickel and cobalt sets** | The plain trebles used the wrap metal's density (bronze 8800 kg/m³, silk 6200). Inharmonicity used the wrap metal's modulus (bronze 110 GPa, silk 90 GPa) for plain strings and for wound cores. So EJ16's .012 E was 26.1 lb and every string's B was 45% low | Plain strings and cores in these sets are **steel** (7850 kg/m³, 200 GPa). D'Addario EJ16: "Plain Steel .012, .016"; published E tension 23.36 lb at 25.5" ([stringsbymail](https://www.stringsbymail.com/acoustic-guitar-strings-7/daddario-15/phosphor-bronze-225/ej16-light-245/)) | **off** (12% tension, 45% B) | **FIXED.** Plain density is 7850 and E is 200 GPa for those five materials. Wound mass is unchanged. EJ16 .012 now models 23.3 lb. Test `AccuracyAudit.bronzeSetsHavePlainSteelTreblesAndSteelCores`. |
| A-06 | **Solid bodies with a top cap**: Vintage Single-Cut (16 mm flame maple), 7-String Modern (6 mm quilt); also the `Chambered` shape with no cap (Angular Korina) | `buildModes` treats a solid body as a clamped circular plate of the **cap's** thickness × 3.2 (`SolidBodyNone`). With no cap it defaults to 0.8 mm. Uncapped Strat top modes: 204/425/697 Hz. Single-Cut with its cap: **4065/8460/13878/15825 Hz**. Korina (chambered, 5 mm default): 1309/2725 Hz. A cap moves the body modes 20× | A solid body's structural resonances between 82 and 500 Hz are two whole-body bending modes ([Fleischer & Zwicker, "Mechanical vibrations of electric guitars"](https://www.researchgate.net/publication/282790879_Fleischer_H_und_Zwicker_T_Mechanical_vibrations_of_electric_guitars)). A 12–16 mm maple cap on a 44 mm mahogany back stiffens that beam; it is not a separate thin plate | **wrong-model** (violates part-acoustics.md ground rule 3, "monotonic and continuous") | Proposal: for non-acoustic shapes, derive the plank/plate modes from the **body** `thickness_mm` and wood (free-free beam or plate), and add the cap as extra thickness, stiffness and mass (a composite EI). Never use the cap's thickness alone. Needs measured solid-body modes to calibrate (Data gaps). |
| A-07 | **Flamenca Blanca** | `baseTypeFor` made it a Flamenco type by its name, but `shapeFor` checked only the body style (`classical`) and gave it the **Classical** body (depth 100 mm, 12 L) | A flamenca is shallower and lighter than a classical (the compiled Flamenco shape: 88 mm, 10 L) | **wrong-model** (inconsistent routing) | **FIXED.** `shapeFor` applies the same name test. Test `AccuracyAudit.flamencaGetsTheFlamencoBody`. |
| A-08 | **Full Hollow Archtop body** bracing | `"x"` → `Bracing::XBrace` (1.00): an acoustic flat-top pattern | Archtops use parallel tone bars (Gibson L-5, ES-175, Super 400) | **off** | **FIXED.** The part says `"parallel"`, which maps to `HollowParallel` (1.20). Top fundamental moves 157 → 188 Hz. Test `AccuracyAudit.archtopIsParallelBraced`. The top is still a flat 2.8 mm "acoustic" Sitka plate. A carved archtop top is 4–7 mm and arched; an ES-175 top is laminated maple. Proposal: add an archtop top part with arching stiffness. |
| A-09 | **Resonator Steel** | Body wood `steel` → `Wood::Maple`. Bracing `none` on an acoustic body → `SolidBodyNone` (×3.2). The Resonator shape has no hole, so there is no air mode. Result: maple-plate modes at 125/261/428 Hz plus solid-plank modes at 160/231 Hz. There is **no cone** | A biscuit resonator is a spun aluminium cone about 24 cm across and under 0.5 mm thick, loaded by the body air and radiating through the screen holes. The cone-plus-air system sets the voice ([Rau & Smith, ISMA 2019](https://ccrma.stanford.edu/~mrau/papers/ISMA2019_Rau_Smith.pdf); [Politzer, Caltech](https://www.its.caltech.edu/~politzer/resonator-guitar/resonator-guitar.pdf)) | **wrong-model** | Proposal: a cone mode set (from Rau & Smith's measured admittance), a screen-hole air mode and a metal body. Stop mapping steel to maple and "none" to solid-body bracing. |
| A-10 | **Hollow Violin-Style Bass** (factory) and compiled **"Violin-Style Bass"** (`GuitarType::Rickenbacker`) | Factory: 864 mm (34") neck, semi-hollow body part, ceramic 13k humbucker and split-P 11k. Compiled: "maple through-neck, bright and cutting", 844.6 mm, **hollow** body with a round 95 mm soundhole. The Rickenbacker type loads the violin-bass file | Höfner 500/1 violin bass: **30" (762 mm)**, fully hollow with no soundholes, flatwounds, low-output "staple" pickups ([Wikipedia: Höfner 500/1](https://en.wikipedia.org/wiki/H%C3%B6fner_500/1)). A Rickenbacker 4001/4003 is a **solid** maple through-neck at 33.25". The two instruments have been merged into one | **wrong-model** | Proposal: decide which instrument this is. For the violin bass: a 762 mm short-scale neck part, a hollow body with no hole and low-output pickups. For the Rickenbacker: a solid body shape and its own factory file. |
| A-11 | **P90 Alnico 5 8.2k** pickup part; `PickupSpec::makeDefault(P90)` | 4.0 H (the part and the compiled default); the default magnet is Alnico 2 | Vintage P-90s measure about 6 H; a typical 8.6k P-90 measures 6.5 H ([guitar.com, "All About P-90s"](https://guitar.com/guides/essential-guide/all-about-p-90s/); [Seymour Duncan, "Inductance"](https://www.seymourduncan.com/blog/latest-updates/inductance-what-it-is-and-why-it-matters)) | **off** (−38%) | **FIXED (part).** 6.5 H; no factory guitar uses the part. Test `AccuracyAudit.p90InductanceIsInTheMeasuredRange`. Proposal: the compiled default (used by the compiled Offset Modern type) should be 6.5 H and Alnico 5. That is left alone because it changes a compiled type's tone. |
| A-12 | **Angular Korina** (factory) | Body part "Mahogany Single-Cut": mahogany, `chambered`. There is no korina anywhere: `Wood::Korina` exists but no factory body uses it | Korina (limba) solid body | **wrong-model** (wrong wood, wrong chambering) | Proposal: a "Korina Angular" solid body part (korina, 480–550 kg/m³). |
| A-13 | **Mahogany Single-Cut body** (used by Vintage Single-Cut, Angular Korina, 7-String Modern) | `chambering: chambered` (tagged "weight-relieved"). This gives the coupling bank a lowest-mode mass of 2 kg instead of 10, feedback 0.25, sustain ×0.95 and the Chambered shape | "Vintage" single-cuts were solid until weight relief (9 holes, 1982) and chambering (2006). Weight relief removes little mass and is acoustically close to solid; part-acoustics.md's "chambered" row (5 modes, +3 dB, air 180–240 Hz) describes a true chambered body | **off** / owner decision | Proposal: `solid` for the Vintage Single-Cut and the 7-string, and a separate "Chambered Single-Cut" part. Not applied because the part was deliberately tagged. |
| A-14 | **Gypsy Jazz** (Selmer-style, oval hole) | 645 mm neck. The oval hole is modelled as the Auditorium's 100 mm round hole. Strings: 11-52 silk & steel | Petite-bouche (oval-hole) Selmer: **670 mm** scale; the hole is a small oval ([Wikipedia: Selmer guitar](https://en.wikipedia.org/wiki/Selmer_guitar); [reverb.com](https://reverb.com/news/selmer-guitar-a-look-at-the-instruments-that-defined-jazz-manouche)). Players use silver-plated-copper "Argentine"-type 10-45/11-46 sets | **off** | Proposal: a 670 mm gypsy neck part and a smaller oval aperture (`soundHoleScale` of about 0.6, equivalent area). A silver-plated-copper string material. |
| A-15 | **Two wood tables** | part-acoustics.md §1 / `lookUpWood` (used only for density trim and fretboard damping) and `BodyModels::kWoods` (which drives the audio) disagree. Alder loss factor 8.5e-3 vs 13e-3; hard maple 705 kg/m³, 12.6 GPa, 6e-3 vs 650, 12 GPa, 9e-3; rosewood E 12 vs 14 GPa; basswood loss 11e-3 vs 16e-3 | USDA *Wood Handbook* (FPL-GTR-190): red alder about 410 kg/m³ and 9.5 GPa; sugar maple 705 kg/m³ and 12.6 GPa; spruce loss tanδ_L about 0.006–0.010 ([Ono & Norimoto; Obataya et al., "Vibrational properties of wood along the grain"](https://link.springer.com/article/10.1023/A:1004782827844)). The spec table matches the handbook better | **off** | Proposal: one table (the spec's), with `BodyModels` reading it. Retune the 0.42 / 0.35 radiation factors in `buildModes` so the body Qs stay where they are voiced. Also, part-acoustics.md 1.1's "Q ≈ 1/(2 tanδ)" is wrong by a factor of 2: a material's Q is 1/tanδ. The code uses `0.42/η`, which is fine, but the spec text should be corrected. |
| A-16 | **Compiled `GuitarLibrary` pickup positions** | Strat 0.13 / 0.25 / 0.40 of the scale = 84 / 162 / **259 mm**. 259 mm is off the end of the fretboard (fret 22 is 182 mm from the saddle). LP 0.14/0.38, T 0.11/0.42, P-bass 0.28 | Strat 1.625" / 3.875" / 6.375" = 41 / 98 / 162 mm = 0.064 / 0.152 / 0.25 ([till.com, "Response Effects of Guitar Pickup Position and Width"](https://till.com/articles/PickupResponse/)). The factory files are right (41 / 99 / 159 mm) | **off** (compensates A-01) | Proposal: set compiled fractions from the factory files' mm values, in the same change as A-01. |
| A-17 | **Compiled electric bridges** (`BodyCouplingBank::bridgeFor`) | Every `Fixed` electric (LP, SG, 335, Explorer, 7/8-string) gets the hardtail string-through row {110 g, 0.70} | part-acoustics.md 5: tune-o-matic + stopbar = 95 g + 60 g, coupling 0.55 | **off** (compiled path only; parts guitars are right) | Proposal: key the compiled bridge on body shape too (arched/semi-hollow → TOM + stopbar). |
| A-18 | **Classical nylon tension distribution** (Normal Tension Nylon) | E1 .028 18.9 lb, B .032 13.8, G .040 13.6, D .029w 17.1, A .035w 14.0, E6 .043w 11.8 lb. The total, 89 lb, is plausible | Normal-tension classical sets run about 13–16 lb per string, roughly even. The treble E is too tight and the bass E too loose here | **off** (unverified in detail) | Proposal: calibrate nylon density and the wound-bass mass factor against a published nylon tension chart (Data gaps). |
| A-19 | **Full Hollow Archtop scale** | 628 mm (24.75", Maple Set-Neck) | factory-content.md 2 says 25.5". An ES-175 is 24.75"; an L-5 or Super 400 is 25.5" | **off vs spec** (physically defensible) | Owner: pick the reference instrument and align the spec or the neck part. |
| A-20 | **Offset Modern**: compiled vs factory | Compiled type: two P90s ("two wide single-coils"). Factory file and factory-content.md: two mini-humbuckers | — | **off** (inconsistent) | Proposal: make the compiled type match the factory file. |
| A-21 | **Vintage Single-Cut cap** | Flame Maple top part 16 mm, on a 50 mm body | A '50s single-cut maple cap is about ½" (12.7 mm) at the centre, thinner at the edge | **off** | Proposal: 12.7 mm, or use Plain Maple (12 mm). This only matters once A-06 is fixed. |
| A-22 | **8-String Modern** multi-scale | A single 698.5 mm (27.5") scale for all strings | factory-content.md: 27"–28" fan | **off** (approximation) | Proposal: a per-string scale in `mapSpec` (tension and inharmonicity per string). |
| A-23 | **Five-String Bass** bridge pickup | "Bass Ceramic Humbucker 13k" (a Music Man-style part) at 45 mm | A StingRay-style humbucker sits much further from the bridge than a J bridge coil | **unverified** | Owner: measure or source the placement. |
| A-24 | **Solid-body plank modes** (all solid electrics and basses) | Fixed 168 / 243 Hz ÷ width^0.8, Q 9–11, with a gain the same for alder, ash and basswood. The frequency does not depend on the wood or on the thickness | Fleischer & Zwicker: two body resonances below 500 Hz whose frequencies depend on the body and neck, and neck modes causing dead spots in 200–400 Hz | **unverified** (plausible magnitudes, no wood dependence) | Folds into A-06. |

## 2. Accurate (checked against a reference)

| Item | Modelled | Reference | Verdict |
|---|---|---|---|
| Electric NPS tensions (10-46, 9-42) | 10-46 @648: 16.4 / 15.6 / 16.8 / 17.2 / 18.5 / 16.9 lb. 9-42: 13.3 / 11.2 / 14.9 / 14.6 / 14.6 / 14.1 lb | D'Addario EXL110: 16.2 / 15.4 / 16.6 / 18.4 / 19.5 / 17.5 ([zZounds, EXL110-7 chart](https://www.zzounds.com/item--DADEXL1107)). EXL120 (from D'Addario's chart as recalled, to be confirmed): 13.1 / 11.0 / 14.7 / 15.8 / 15.8 / 14.7 | accurate (plain within 2%, wound 3–8% low: the 0.78 wound-mass factor is slightly light) |
| Acoustic PB 12-53 plain (after A-05) | .012 23.3 lb | EJ16 23.36 lb | accurate |
| Tension-pitch EA/T (sustain-and-decay.md 3) | .010 139, .017 393, .046w 577 | Spec: ~140 / ~400 / ~560 | accurate |
| Scale lengths | Strat/T 648, single-cut/SG/335 628, classical 650, bass 864, 7-string 673, baritone 686 (27"), resonator 635 | Fender 25.5", Gibson 24.75", classical 650, bass 34", 26.5", 27" baritone, 25" resonator | accurate |
| Factory electric pickup placements | Strat 41/99/159; T 36/165; single-cut 38/152; 335 40/150 mm | Strat 41/98/162 mm (till.com); part-acoustics.md 6.1: bridge 38, neck 152 | accurate |
| Acoustic Helmholtz (body air) | Dreadnought 104, GA 110, Jumbo 96, 12-string 96, Classical 114, Flamenca 124, Parlor 136 Hz | Flat-top A0 typically 95–125 Hz, higher for smaller bodies (part-acoustics.md 2.1: 90–110 Hz for full-size) | accurate (parlor slightly high, plausible for its volume) |
| Acoustic top fundamental | Dreadnought 123, GA 131, Classical 113 Hz | T(1,1) typically 100–250 Hz coupled | accurate |
| Single-coil / humbucker / bass pickup LCR | Vintage Strat 2.3 H/5.8k/110 pF; T bridge 3.3 H/8.5k; PAF 4.5–4.8 H/7.6–8.1k; J 3.5–3.8 H; split-P 7.0 H/11k; ceramic 15k 8.2 H | Typical measured ranges: Strat 2.2–2.6 H, Tele bridge 3.0–3.5 H, PAF 4–5 H, P 6–7 H, hot ceramic 8 H | accurate (unverified per model: see gaps) |
| 12-string courses | .010/.010, .014/.014, .023w/.008, .030/.012, .039/.018, .047/.027; top two courses unison | D'Addario EJ38 | accurate |
| Classical and flamenco top woods, bracing, 650 mm | Cedar/spruce, fan | Standard | accurate |
| Wound threshold | ≥ .0205" wound | .020 plain G, .021 wound D on 8-38 | accurate |
| Air-mode scaling with volume (mapSpec) | 1/√V, clamped to table rows | Helmholtz | accurate (but unused: A-04) |
| Coupling-bank chambering masses | Acoustic 0.10 kg, hollow 0.20, semi 0.60, chambered 2, solid 10 kg | Effective masses at the bridge for low top modes are tens of grams to 0.1 kg on acoustics | unverified (plausible) |

## 3. Fixes applied (this branch)

| ID | Change | Files | Test |
|---|---|---|---|
| A-03 | Baritone style or name → `BaritoneElectric` (B standard) | `Source/Model/Workshop/PartAcoustics.cpp` | `AccuracyAudit.baritoneIsTunedToBStandard` |
| A-05 | Plain strings and cores of PB, 80/20, silk & steel, pure-nickel and cobalt sets are steel | `Source/Model/Guitar/StringMaterials.cpp/.h` | `AccuracyAudit.bronzeSetsHavePlainSteelTreblesAndSteelCores` |
| A-07 | `shapeFor` uses the same flamenco test as `baseTypeFor` | `PartAcoustics.cpp` | `AccuracyAudit.flamencaGetsTheFlamencoBody` |
| A-08 | Archtop body bracing `parallel`, and `engineBracing` maps it to `HollowParallel` explicitly | `Full Hollow Archtop Body.luthierpart`, `PartAcoustics.cpp` | `AccuracyAudit.archtopIsParallelBraced` |
| A-11 | P90 part inductance 6.5 H | `P90 Alnico 5 8.2k.luthierpart` | `AccuracyAudit.p90InductanceIsInTheMeasuredRange` |
| — | Build fix: GCC rejects a local `constexpr` in a lambda's default argument | `Source/Tests/ReviewRegressionTests.cpp` | builds on GCC 13 |

Every fix changes only the instruments named in its row. A-05 changes the
string physics of every acoustic, resonator and 12-string guitar, which use
phosphor-bronze or 80/20 sets, and of any pure-nickel, cobalt or silk & steel
set.

## 4. Data gaps (owner to source)

1. **Solid-body modes.** Measured bridge admittance or modal frequencies
   and Qs for a single-cut and a Strat-type body. Fleischer & Zwicker's report
   has them. This is needed to calibrate A-06 and A-24.
2. **Archtop and semi-hollow air and top modes.** A0 and T(1,1) with
   f-holes, for an ES-175-type (laminated) and an L-5-type (carved) body, plus
   an ES-335. Needed for A-04 and A-08.
3. **Resonator cone admittance.** Rau & Smith's measured single-cone data,
   plus a biscuit-cone measurement. Needed for A-09.
4. **Published tension charts.** Nylon normal and high tension (for A-18),
   50-105 flatwound, 14-68 baritone, 10-74 eight-string and 45-130
   five-string. These calibrate `woundMassFactor` per material; wound NPS
   currently reads 3–8% light.
5. **Pickup LCR per part.** Measured L, R and self-C for the specific
   reference pickups: the active 10k pair (1.1–1.2 H, 60 pF looks like a
   low-impedance coil and should be confirmed), the floating jazz humbucker,
   the mini-humbucker and the soundhole pickup.
6. **Pickup placement** for the Music Man-style bass humbucker (A-23) and
   J-bass and P-bass centres (modelled at 42/160 mm and 170 mm).
7. **Magnet pull damping.** Measured sustain loss against pole height
   ("Stratitis"). part-acoustics.md 6.2's pull and damping table has no
   source.
8. **String aging curves.** Brightness and sustain against hours played,
   per material. string-aging.md's rates have no measured source.
9. **Termination brightness** of frets and nuts (nickel-silver 0.70,
   stainless 0.90, bone 0.75…) and **bridge hardware masses** (ABR-1,
   6-screw trem, Floyd, Bigsby). All are unsourced constants.
10. **Wood loss factors** per species at guitar-relevant frequencies, to
    settle A-15's two tables.
11. **Nickel cover loss** (−0.8 dB at 4 kHz). This comes from part-acoustics.md
    with no measurement cited.
