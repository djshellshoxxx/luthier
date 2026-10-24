# mxr-distortion-plus — dev-ready spec (MXR Distortion+ M104, inspired by)

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `mxr-distortion-plus`, **paramPrefix `mxr_distortion_plus_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/mxr_distortion_plus.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/mxr_distortion_plus_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_mxr_distortion_plusTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `mxr_distortion_plus_bypass`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims `mxr_distortion_plus_in_trim` -> `ref_s{nn}_in_db/out_db`; footswitch-mode params `mxr_distortion_plus_stomp_mode` -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `mxr_distortion_plus_variant`, `mxr_distortion_plus_quality`, `mxr_distortion_plus_stereo_mode`, `mxr_distortion_plus_noise`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): (none).
> - Host lanes `ref_s{nn}_p01..` in this order: `mxr_distortion_plus_dist`, `mxr_distortion_plus_out`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Status SPEC v1 2026-10-21. Id prefix `mxr_distortion_plus_`. Legal: behaviour/controls only; "inspired by MXR Distortion+" descriptive; no copied text/photos; art = original vector.
Evidence tags: D=DOCUMENTED, M=MEASURED, E=ESTIMATED, OMIT=not modelled. Research note (v1.2 re-check 2026-10-22): WebFetch DNS-fails and the egress proxy 403s electrosmash.com, web.archive.org and tubesoundbcn.com; only WebSearch snippets were reachable (manual spec table via snippet, forum/clone parts tables via snippet). Component values: those confirmed by search snippets are tagged D(snippet) with URL; the rest stay E. No M values exist; no hardware capture available. See sec 11.

Sources
- Dunlop M104 manual rev C (spec table): https://www.tubesoundbcn.com/img/manual/M104.pdf (D)
- https://jimdunlop.com/content/manuals/RR104.pdf (RR104 variant, 11 mA, NOT M104)
- https://electrosmash.com/mxr-distortion-plus-analysis (circuit analysis; snippet only: 1 op-amp + 2 Ge diodes, no tone, max boost up to 46 dB, 'mild hump' response; component values NOT seen)
- https://forum.pedalpcb.com/threads/distortion-250-parts-values.10835/ and https://vero-p2p.blogspot.com/2026/02/dod-mxr-250-distortion-component-values.html (clone parts tables; snippet only: R1 10k, R2 1M, R3 4k7, C1 1n, C3 47n, D2/D3 1N270 or 1N914, IC LM741; 500k vs 1M conflict across schematics)
- https://www.jimdunlop.com/mxr-distortion/ ; https://www.effectsdatabase.com/model/mxr/dunlop/m104
- diystompboxes MXR Dist+ threads (741, 1N270, 250-300 mV clip)

## 1 Identity / variants
| field | value |
|---|---|
| model_id | `mxr-distortion-plus` |
| revision | `M104-LM741-1N270` (modern M104 reissue topology) |
| category | Distortion, mono, 1 in / 1 out |
| supply | 9 V DC (center-neg) or 9 V battery; 2.5 mA (D, M104 manual) |
| levels (D, manual) | nominal in -22 dBV, max out -14 dBV, noise floor -74 dBV (A-wtd, controls max), distortion gain 10..45 dB at 1 kHz |
| Zin | 1 MΩ (D, M104 manual rev C) |
| Zout | 25 kΩ at OUTPUT mid (D) |
| bypass | manual lists 'Hardwire' (D); treated as true bypass (E interpretation), stomp + LED |
Variants (selectable `mxr_distortion_plus_variant` enum, state only; default 0):
- 0 `M104_REISSUE` LM741 + 1N270 (default)
- 1 `SI_CLIP` 1N914/1N4148 silicon (D(snippet): clone tables list 1N270 or 1N914; clip ~0.6 V E)
- 2 `LED_CLIP` OMIT (no source for stock use; common mod only, unverified)
- 3 `OP07_MOD` OMIT (unverified mod; enum slot kept reserved, plugin does nothing different from 0)
Only 0 (and optionally 1) implemented; variant range 0..1, values 2-3 reserved/clamped to 0. Do not claim vintage 1970s script-logo unit identical (pot values conflict between schematics: one lists 1M, two list 500k, per forum snippet; modern M104 = 1M chosen because it matches manual 45 dB max gain, see sec 11).

## 2 Controls
| control | type | range | taper | default | real behaviour |
|---|---|---|---|---|---|
| DISTORTION | knob 300° | 0..1 UI | reverse-log (hardware "500k/1M rev log"), see §8 | 0.5 | sets feedback R of non-inverting gain stage; gain 10..45 dB at 1 kHz (D, manual), model ~8.7..~45 dB (E, see sec 3) |
| OUTPUT | knob 300° | 0..1 | audio/log (50k A) | 0.5 | passive attenuator after clipper; 0 = silence, max ≈ +0 dB re clip node |
| STOMP | footswitch | on/off | - | off (bypass) | hardwire bypass (D); LED red on when effect on (E) |
| LED | indicator | - | - | - | lit when engaged |
| IN jack | 1/4" TS | - | - | - | |
| OUT jack | 1/4" TS | - | - | - | |
| DC jack | 2.1 mm | - | - | - | 9 V; plugging battery clip... no UI |
No tone control, no clipping switch (do not add fake knobs; Advanced overlay may expose variant + input trim, flagged non-physical).
Real behaviour note: DISTORTION at 0 still gives about +10 dB at 1 kHz (D, manual); at max, strong compression/fizz. OUTPUT interacts: volume increases greatly with DIST because clip level is fixed ~±0.3 V (Ge) before output pot.

## 3 Signal path / circuit model (values D(snippet) where marked, else E)
Stage list (non-inv gain op-amp topology, per public schematic):
1. Input: C_in ≈ 4.7 nF? series + R_in = 10 kΩ (R1, D(snippet) pedalpcb) to bias (Vb = 4.5 V). HPF corner fc_in ≈ 1/(2π·R·C) ≈ 3 Hz–35 Hz depending; model one-pole HP fc=`30 Hz`(E) with Zin=1 MΩ ignoring pickup load (optional loading LPF if routing impedance emulation on: source Rs model off by default).
2. Gain stage: LM741 non-inverting. Gain G = 1 + (R_f + Pd) / Z_g, Z_g = R_g ‖(1/sC_g series): R_g=4.7 kΩ (R3 4k7, D(snippet)), C_g = 47 nF (C3, D(snippet)). Gain HP corner f_g = 1/(2π·4.7k·47n) ≈ 720 Hz (derived from D values) → low end progressively cut as gain rises; +Pd up to 1 MΩ (D(snippet) R2 1M) plus fixed R_f ≈ 10 kΩ (E: fits manual 10 dB min). |Z_g|(1 kHz)=√(4.7k²+3.39k²)=5.8k, so G(1k) = 1+(10k+Pd)/5.8k: Pd=0 -> 8.7 dB (manual 10 dB, D), Pd=1M -> 44.8 dB (manual 45 dB, D; ElectroSmash 'up to 46 dB' = HF asymptote 1+1.01M/4.7k=46.6 dB). Model reproduces manual within 1.3 dB. Feedback cap C_f ≈ 47 pF (E; only a 10 pF anti-oscillation mod seen in snippets, no stock value found) across Pd+R_f → LPF fc = 1/(2π·(Pd+Rf)·47p): at Pd=1M ≈ 3.4 kHz; at Pd=10k ≈ 340 kHz (E). Combine with 741 GBW (E 1 MHz) → closed-loop BW = GBW/G: G=213 → 4.7 kHz; G=10 → 100 kHz. Take min of both (cascade LPFs).
3. Op-amp limits: output swing ±(Vs−~1.5 V) with 9 V rail ⇒ ±3 V about 4.5 V (E); slew 0.5 V/µs (E). Model as soft clip `tanh`-like at ±3.0 V (rarely hit due to diode clip).
4. Clipping: series R_c ≈ 10 kΩ (E), antiparallel 1N270 ×2 (D(snippet)) to Vb/ground. Ge I-V: Is = 200 nA? Use Ge: Is = 1.0e-6 A (E), n = 1.3 (E), Vt=25.85 mV; knee ~0.25-0.3 V (D, forum). Series Rs ≈ 5 Ω for diode. Asymmetry: none (matched).
   Also small post-clip RC: C1 = 1 nF (D(snippet) value, position E) + R_c → mild LPF fc ≈ 15.9 kHz (derived).
5. Output pot (50k log, E) → output coupling C ≈ 100 nF–1 µF into Zout. Output Z modelled as fixed 25 kΩ at mid (D); in plugin only apply optional Zout/Zload LPF if "load" option on (default off).
6. Bypass: hardwire. Transition = instantaneous mechanical contact; plugin: 5 ms equal-power-ish crossfade dry/effect (avoid click) — noted as inaccuracy. Latency: 0 (dist stage), ≤ oversampling filter latency (compensated, report via getLatencySamples()). Bypassed path: exact copy of input (bit-exact when off, gain 1.0, no filter).
Signal levels: manual nominal input -22 dBV (D) = 79 mV rms = 112 mV peak; max output -14 dBV (D) = 200 mV rms = 282 mV peak. Calibration: digital x -> volts v = x · 0.5 V peak (E choice; -22 dBV nominal sits at about -13 dBFS), user trim ±12 dB, non-physical.

## 4 Algorithm (plugin)
Block: `x -> HP_in(30Hz) -> [pre-gain scaling] -> gain stage (TPT HP/LPF + gain) -> LPF_cf/GBW -> clipper (Newton) -> RC LPF 16k -> OUTPUT gain -> y`
Per-sample (volts, Fs_os = 4× host sr, min 176.4 kHz at 44.1):
```
v   = x*Vref
v1  = hp1(v)                                   // TPT one-pole, fc 30 Hz
G   = 1 + (Rf + Pd(dist))/Rg                   // Pd(dist)= 1e6*taper_rev_log(dist)
// frequency-dependent gain: split v1 into LF path (gain 1 below f_g) and gain path
vlow = lp(f_g,720Hz)(v1)                       // 1-pole
vhi  = v1 - vlow
g1   = vlow*1 + vhi*G                          // approx of Z_g=Rg+1/sCg (shelf gain: 1 + (Pd/Rg)*sRC/(1+sRC))
fc_lp= min( 1/(2π (Pd+Rf) 47p), GBW/G )        // update per block, smoothed
v2   = lp(fc_lp)(g1)
v2   = softclip_rail(v2, ±3.0)
vd   = solve_diode(v2)                         // below
v3   = lp(15.9k)(vd)
y    = v3 * Gout(out) / Vref
```
Diode node (implicit): (v2 − vd)/Rc = 2·Is·sinh(vd/(n·Vt)) ; solve vd with Newton ≤4 iters, warm start from previous sample, damped step, clamp |vd|≤1.0 V; fallback to closed-form `vd = n·Vt·asinh(...)` ( Wright-ω / lambert) if not converged. Alternative cheap mode (`quality=eco`): 
`vd = Vk·tanh(v2/Vk)`-style with Vk=0.28 V (E), + 1st order ADAA of that function.
- Oversampling: 4× (polyphase FIR/IIR halfband cascade, 2×2). Aliasing: ≥-70 dB alias at max gain, 1 kHz sine, fs=44.1 k. Eco path: 2× + ADAA1. Latency reported; board compensates.
- Smoothing: DIST and OUT: 1-pole 20 ms (exp), recompute coefficients at 32-sample sub-blocks or per sample when ramping; footswitch xfade 5 ms.
- Denormals: `juce::ScopedNoDenormals` + DC offset 1e-18 in filters; flush states |s|<1e-20.
- CPU budget: ≤0.8% of one core @ 48 kHz/256 blk/i7 (4× OS, Newton); eco ≤0.3%. No alloc/locks/IO in `processStereo`; filters + OS preallocated in `prepare`. Mono processing; stereo L/R summed? -> process L only if R identical, else dual mono instances (2 states).
- Noise: optional 741 input noise 20 nV/√Hz (E) + Rg thermal; off by default for determinism (`mxr_distortion_plus_noise` bool).
- NaN guard: if !isfinite(y) reset state, y=0.

## 5 I/O & routing
- Mono in/out, 1 in 1 out; stereo host: two independent instances (dual mono) or L+R sum → mono → duplicate (`mxr_distortion_plus_stereo_mode` 0 dual,1 sum). Default dual.
- No extra jacks, no expression, no aux/send/return, no MIDI. 
- Board tricks supported by board (not pedal): series any order (esp. fuzz->DP, DP->delay), parallel via board split/mixer if available, effects-loop feedback via board feedback send/return with limiter; MXR itself has no loop. Typical user tricks: boost into DP (raises gain; input headroom clip at 741 rail), DP before amp, two DP in series (stack), DP + OD pre (tight). No self-oscillation path in pedal.
- Placement PRE (recommended) or LOOP; POST-CAB disabled by default (fizz).

## 6 MIDI / sync / expression
Real unit: none (no MIDI, no tempo, no EXP). Emulation = comparable "controller" layer only, not pedal features:
- Real hardware has no MIDI: any MIDI chart entry is OMIT (no documented CC map exists). Plugin default: no CC defaults (earlier suggested CC20/21/22 removed as invented). Board layer provides generic MIDI-learn for DIST/OUT/bypass; footswitch latch/momentary mode and softPress are FootController features (MIDI_EXPRESSION 9.2), not pedal params.
- Program change: ignore unless preset map enabled. MIDI clock/tap: N/A (not applicable, no tempo parameters).
- Expression pedal: virtual EXP target = any of DIST/OUT with min/max heel/toe range (board-level), curve linear. All smoothing via param smoothing 20 ms.
All MIDI handled in board's lock-free queue; pedal reads snapshot.

## 7 Visuals (original vector art)
- Enclosure: ~59 x 111 mm footprint (E, small die-cast style), height 3 mm top-line; colour yellow `#F2C200` (modern M104 reissue; 70s script-logo original also yellow) with black text; subtle powder-coat noise texture. Corner radius 3 mm.
- Layout (origin top-left, mm): knobs side by side upper area: DISTORTION centre (15, 22) , OUTPUT (44, 22) — Ø 15 mm black skirt knobs, white indicator line, tick ring Ø 21 mm with 11 ticks; LED red Ø 4 mm at (29.5, 40); footswitch round chrome Ø 14 mm at (29.5, 82); IN jack left-side (0, 55), OUT jack right-side (59,55) as side stubs; DC jack top edge (12, 0).
- Labels (original lettering, sans, black): "DISTORTION" under left knob, "OUTPUT" under right; centre/bottom text "Distortion +" in script-style original font substitute; small "INSPIRED BY" legend NOT drawn on pedal; model name in info panel only.
- Animations: knob rotation 300° (min −150°, max +150°); stomp press depth 1.5 mm with velocity; LED glow bloom.
- Do not include MXR logo or exact trademark artwork.

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | auto |
|---|---|---|---|---|---|---|---|---|
| mxr_distortion_plus_bypass | Stomp | 0 | 1 | 0 (bypassed; engaged = 1 per reconciled header) | bool | - | 5 xfade | yes |
| mxr_distortion_plus_dist | Distortion | 0 | 1 | 0.5 | norm | Pd=1e6·(exp(k·d)−1)/(exp(k)−1), k=5 (E; resolves earlier duplicate formulas); Pd(0)=0, Pd(1)=1 MΩ (D(snippet)) | 20 | yes |
| mxr_distortion_plus_out | Output | 0 | 1 | 0.5 | norm | A-taper (E): piecewise-linear gain through {0:0, 0.5:0.1, 1:1} (single definition; earlier duplicate formulas removed) | 20 | yes |
| mxr_distortion_plus_variant | Variant | 0 | 1 | 0 | enum | - | - | no |
| mxr_distortion_plus_quality | Quality | 0 | 1 | 1 | enum eco/hq | - | - | no |
| mxr_distortion_plus_stereo_mode | Stereo mode | 0 | 1 | 0 | enum | - | - | no |
| mxr_distortion_plus_noise | Noise | 0 | 1 | 0 | bool | - | - | no |
Mapping notes: in_trim and stomp_mode rows removed (moved to slot/board layer per reconciled header). Pot tapers E. Gain: G(d)=1+(Pd+Rf)/|Z_g|, Rf=10k (E).

## 9 Tests / acceptance
Provisional (no hardware reference; E; replace with ±measured):
- Bypass off: output bit-identical to input (null ≤ −300 dB). Bypass/engage xfade: no step > 0.01 FS.
- Silence in → |out| < 1e-12 (noise off), no DC drift |DC|<1e-6 after 10 s.
- Gain: d=0, 1 kHz −30 dBFS sine: small-signal gain at 1 kHz = +10 dB ±2 dB (D, manual 10 dB min). d=1: +45 dB ±2 dB at 1 kHz (D, manual 45 dB max), HF asymptote up to ~46 dB (D(snippet) ElectroSmash); use −70 dBFS in to avoid clip. Noise-floor check (optional noise on): about -74 dBV A-wtd at controls max (D) ±6 dB. HP corner at d=1: gain at 100 Hz ≥ 15 dB below 1 kHz-ish (f_g 720 Hz shelf); LPF: −3 dB point at d=1 ≈ 3–5 kHz (+/-30%).
- Clip level: sine 500 mV in at d=1: peak at clipper node 0.2..0.35 V (Ge); Si variant 0.5..0.7 V.
- THD (1 kHz, 100 mV, OUT=1): d=0.25 : 5–15%? ESTIMATED gates: 2nd+3rd dominant; d=0.5 : 10–35%; d=1 : 25–60%. (Pass = within bands; fail only if outside.)
- Alias: 4× HQ, 1 kHz & 5 kHz & 10 kHz sines at max gain: alias comps ≤ −70 dBFS (44.1k), ≤ −80 (96k).
- IMD two-tone 800/1100 Hz: monotonic increase with d (±2%).
- Monotonic: output rms non-decreasing vs d at fixed input and vs OUT.
- Step response: no ringing > 5% (stable), no limit cycles.
- Block sizes 1/4/16/64/512/2048 + odd (37, 509): output equal within −120 dB. Sample rates 44.1/48/88.2/96/192: freq-response checkpoints within 1 dB below 8 kHz.
- Param smoothing: automation sweep no zipper (step ≤0.5 dB/sample ms-equivalent).
- CPU hq ≤0.8%, eco ≤0.3% (see §4). No allocations (ASan/RT sanitizer check), 30 min random footswitch/parameter soak, NaN-free.
- Save/restore round-trip exact; variant switch crossfades 20 ms.
- Null-test vs future hardware capture: RMS error ≤ −30 dB at 5 knob grid {0,.25,.5,.75,1} × 4 input levels (target after measure; thresholds provisional, record measured uncertainty).

## 10 Known inaccuracies
- Component values: R1 10k, R2 1M, R3 4k7, C1 1n, C3 47n, 1N270, LM741 are D(snippet) from clone tables; Rf, Rc, Cf, 50k output pot, tapers are E. No primary schematic or ElectroSmash page was readable (proxy-blocked). 500k vs 1M conflict resolved to 1M via manual gain range.
- 741 non-linear internals (slew, crossover distortion, rail clipping asymmetry) simplified; real Ge diode tolerance/temperature variance not modelled (each real unit sounds different).
- Impedance interaction with pickups (1 MΩ load) not modelled by default; Zout 25 kΩ not applied.
- Bypass crossfade vs true mechanical contact; no switch pop.
- No power-supply sag/noise; battery state not modelled.
- Vintage 70s variants not modelled. RR104/other MXR designs not covered. No legal claim of equivalence.
- Looper section: N/A (not a looper).

## 11 Verification log
Reach: WebSearch worked (snippets only). WebFetch/curl blocked (DNS ENOTFOUND; proxy 403 for electrosmash.com, web.archive.org, tubesoundbcn.com, wamplerdiy.com, forum.pedalpcb.com, vero-p2p.blogspot.com, tagboardeffects.blogspot.com). No MIDI chart exists for this analog pedal.

| # | item | old -> new | status | source |
|---|---|---|---|---|
| 1 | Min/max gain | 1x..213x (0..46.6 dB) -> 10..45 dB at 1 kHz | DOCUMENTED | M104 manual (via search snippet) https://www.tubesoundbcn.com/img/manual/M104.pdf |
| 2 | Nominal in / max out / noise floor | absent / "0.1-0.5 V E" -> -22 dBV / -14 dBV / -74 dBV A-wtd | DOCUMENTED | same manual |
| 3 | Zin 1 MΩ, Zout 25 kΩ (OUT mid), 2.5 mA | unchanged, now confirmed incl. mid-position footnote | DOCUMENTED | same manual; https://www.effectsdatabase.com/model/mxr/dunlop/m104 |
| 4 | Bypass | "true bypass (D)" -> manual says "Hardwire"; true-bypass reading is interpretation | ESTIMATED | manual |
| 5 | R_in 10k, R_g 4k7, R_fmax(Pd) 1M, C1 1n, C_g 47n, diodes 1N270 (alt 1N914), LM741 | E -> D(snippet) | DOCUMENTED (clone tables, snippet-level) | https://forum.pedalpcb.com/threads/distortion-250-parts-values.10835/ ; https://vero-p2p.blogspot.com/2026/02/dod-mxr-250-distortion-component-values.html |
| 6 | Dist pot 500k vs 1M | unresolved -> 1M | ESTIMATED (supported by manual 45 dB: 1+1M/5.8k=44.8 dB; 500k would give ~39 dB) | derived |
| 7 | Fixed R_f in gain leg | 0 -> 10 kΩ | ESTIMATED (derived: manual 10 dB min at 1 kHz needs ~12.5k; 8.7 dB with 10k) | derived |
| 8 | C_f 47 pF | kept | ESTIMATED (snippets show only a 10 pF anti-oscillation mod) | derived |
| 9 | R_c 10k, 50k output pot, output coupling C, Ge Is/n | kept | ESTIMATED | derived from topology / datasheet-typical Ge |
| 10 | Pot tapers (dist k=5 exp; out A-taper 10% at mid) | duplicate/messy formulas -> one formula each | ESTIMATED | derived |
| 11 | ElectroSmash "up to 46 dB", mild hump, no tone, 2 Ge diodes | claim -> confirmed in snippet; reconciled with item 1 (HF asymptote 46.6 vs 1 kHz 45) | DOCUMENTED (snippet) | https://electrosmash.com/mxr-distortion-plus-analysis |
| 12 | Variant 1 SI_CLIP | kept (1N914 alt in clone tables) | DOCUMENTED (part) / ESTIMATED (clip level) | pedalpcb table |
| 13 | Variant 2 LED_CLIP, variant 3 OP07_MOD | E -> OMIT; enum range 0..1 | OMIT (plugin does nothing; unverified mods) | none found |
| 14 | MIDI CC 20/21/22 defaults, note-on stomp, aftertouch map | invented defaults -> removed | OMIT (hardware has no MIDI; no chart exists) | manual |
| 15 | Params in_trim, stomp_mode | duplicate of slot/board layer -> removed from table | fixed inconsistency | reconciled header |
| 16 | bypass default | 1 (bypassed) vs header "1 = engaged" -> 0 | fixed inconsistency | reconciled header |
| 17 | Input calibration | 0.25 V vs 0.5 V per FS conflict -> 0.5 V peak/FS; -22 dBV nominal ≈ -13 dBFS | ESTIMATED choice built on D level | manual |
| 18 | Enclosure 59x111 mm, yellow, layout, 4.7 nF input cap, 30 Hz HP, op-amp GBW/slew, rail swing, output C | kept | ESTIMATED (not found) | - |
| 19 | Ge clip level 0.25-0.3 V | "D, forum" -> ESTIMATED (not re-confirmed) | ESTIMATED | - |
