# mxr-distortion-plus — dev-ready spec (MXR Distortion+ M104, inspired by)

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `mxr-distortion-plus`, **paramPrefix `mxr_distortion_plus_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/mxr_distortion_plus.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/mxr_distortion_plus_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_mxr_distortion_plusTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `mxr_distortion_plus_bypass`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims `mxr_distortion_plus_in_trim` -> `ref_s{nn}_in_db/out_db`; footswitch-mode params `mxr_distortion_plus_stomp_mode` -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `mxr_distortion_plus_variant`, `mxr_distortion_plus_quality`, `mxr_distortion_plus_stereo_mode`, `mxr_distortion_plus_noise`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): (none).
> - Host lanes `ref_s{nn}_p01..` in this order: `mxr_distortion_plus_dist`, `mxr_distortion_plus_out`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Status SPEC v1 2026-10-21. Id prefix `mxr_distortion_plus_`. Legal: behaviour/controls only; "inspired by MXR Distortion+" descriptive; no copied text/photos; art = original vector.
Evidence tags: D=DOCUMENTED, M=MEASURED, E=ESTIMATED. Research note: electrosmash.com / effectsdatabase.com not fetchable from this sandbox (DNS); manual copy + search snippets only. All component values below = E (recalled from public schematic/clones; verify vs ElectroSmash before freezing). No M values exist; no hardware capture available.

Sources
- Dunlop M104 manual rev C (spec table): https://www.tubesoundbcn.com/img/manual/M104.pdf (D)
- https://jimdunlop.com/content/manuals/RR104.pdf (RR104 variant, 11 mA, NOT M104)
- https://electrosmash.com/mxr-distortion-plus-analysis (circuit analysis; claim ~46 dB max gain)
- https://www.jimdunlop.com/mxr-distortion/ ; https://www.effectsdatabase.com/model/mxr/dunlop/m104
- diystompboxes MXR Dist+ threads (741, 1N270, 250-300 mV clip)

## 1 Identity / variants
| field | value |
|---|---|
| model_id | `mxr-distortion-plus` |
| revision | `M104-LM741-1N270` (modern M104 reissue topology) |
| category | Distortion, mono, 1 in / 1 out |
| supply | 9 V DC (center-neg) or 9 V battery; 2.5 mA (D, M104) |
| Zin | 1 MΩ (D) |
| Zout | 25 kΩ at OUTPUT mid (D) |
| bypass | hardwire (true) bypass (D), via 3PDT/DPDT stomp + LED |
Variants (selectable `mxr_distortion_plus_variant` enum, state only; default 0):
- 0 `M104_REISSUE` LM741 + 1N270 (default)
- 1 `SI_CLIP` 1N914 silicon (common mod, E; clip ~0.6 V)
- 2 `LED_CLIP` red LED (E; ~1.7 V, louder/harder) 
- 3 `OP07_MOD` lower noise/higher GBW op-amp (E; GBW 0.6 MHz vs 741 1 MHz? both E)
Variants 1-3 optional P2; only 0 required for v1. Do not claim vintage 1970s script-logo unit identical (different pot values reported: 500k rev-log dist, 50k log out vs 1M; E, source conflicting).

## 2 Controls
| control | type | range | taper | default | real behaviour |
|---|---|---|---|---|---|
| DISTORTION | knob 300° | 0..1 UI | reverse-log (hardware "500k/1M rev log"), see §8 | 0.5 | sets feedback R of non-inverting gain stage; gain 1x..~213x (0..46.6 dB) |
| OUTPUT | knob 300° | 0..1 | audio/log (50k A) | 0.5 | passive attenuator after clipper; 0 = silence, max ≈ +0 dB re clip node |
| STOMP | footswitch | on/off | - | off (bypass) | true bypass; LED red on when effect on |
| LED | indicator | - | - | - | lit when engaged |
| IN jack | 1/4" TS | - | - | - | |
| OUT jack | 1/4" TS | - | - | - | |
| DC jack | 2.1 mm | - | - | - | 9 V; plugging battery clip... no UI |
No tone control, no clipping switch (do not add fake knobs; Advanced overlay may expose variant + input trim, flagged non-physical).
Real behaviour note: DISTORTION at 0 is still mildly clean-gain (x~1..few); at max, strong compression/fizz. OUTPUT interacts: volume increases greatly with DIST because clip level is fixed ~±0.3 V (Ge) before output pot.

## 3 Signal path / circuit model (all E)
Stage list (non-inv gain op-amp topology, per public schematic):
1. Input: C_in ≈ 4.7 nF? series + R_in ≈ 10 kΩ to bias (Vb = 4.5 V). HPF corner fc_in ≈ 1/(2π·R·C) ≈ 3 Hz–35 Hz depending; model one-pole HP fc=`30 Hz`(E) with Zin=1 MΩ ignoring pickup load (optional loading LPF if routing impedance emulation on: source Rs model off by default).
2. Gain stage: LM741 non-inverting. Gain G = 1 + (R_f + Pd) / Z_g, Z_g = R_g ‖(1/sC_g series): R_g=4.7 kΩ, C_g = 47 nF? (E). Gain HP corner f_g = 1/(2π·4.7k·47n) ≈ 720 Hz (E) → low end progressively cut as gain rises; +Pd up to 1 MΩ max (E) → Gmax=1+1M/4.7k≈213.8 = 46.6 dB (matches "up to 46 dB" claim, D-ish). Feedback cap C_f ≈ 47 pF across Pd+R_f → LPF fc = 1/(2π·(Pd+Rf)·47p): at Pd=1M ≈ 3.4 kHz; at Pd=10k ≈ 340 kHz (E). Combine with 741 GBW (E 1 MHz) → closed-loop BW = GBW/G: G=213 → 4.7 kHz; G=10 → 100 kHz. Take min of both (cascade LPFs).
3. Op-amp limits: output swing ±(Vs−~1.5 V) with 9 V rail ⇒ ±3 V about 4.5 V (E); slew 0.5 V/µs (E). Model as soft clip `tanh`-like at ±3.0 V (rarely hit due to diode clip).
4. Clipping: series R_c ≈ 10 kΩ? (E) into node, antiparallel 1N270 ×2 to Vb/ground. Ge I-V: Is = 200 nA? Use Ge: Is = 1.0e-6 A (E), n = 1.3 (E), Vt=25.85 mV; knee ~0.25-0.3 V (D, forum). Series Rs ≈ 5 Ω for diode. Asymmetry: none (matched).
   Also small post-clip RC: C ≈ 1 nF + output pot → mild LPF fc ≈ 1/(2π·10k·1n)=15.9 kHz (E).
5. Output pot (50k log) → output coupling C ≈ 100 nF–1 µF into Zout. Output Z modelled as fixed 25 kΩ at mid (D); in plugin only apply optional Zout/Zload LPF if "load" option on (default off).
6. Bypass: hardwire. Transition = instantaneous mechanical contact; plugin: 5 ms equal-power-ish crossfade dry/effect (avoid click) — noted as inaccuracy. Latency: 0 (dist stage), ≤ oversampling filter latency (compensated, report via getLatencySamples()). Bypassed path: exact copy of input (bit-exact when off, gain 1.0, no filter).
Signal levels: reference 0 dBFS = +4 dBu? NO. Set calibration `mxr_distortion_plus_in_ref_v` = 0.25 V peak per 1.0 FS... default guitar 0.1–0.5 V peak (E): digital x → volts v = x · 0.5 V (E, default, user trim ±12 dB, non-physical).

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
- Footswitch gestures: MIDI note-on (learnable) toggles `mxr_distortion_plus_bypass` (latch); velocity > 100 hard-press: no differences; momentary mode `mxr_distortion_plus_stomp_mode` 0 latch,1 momentary (hold while key down; E non-hardware).
- Velocity -> footswitch press feel: map velocity to 0.. 1 press depth (UI animation only) + optional bypass xfade time 2..12 ms (soft press slower). Aftertouch: optional learn target DISTORTION (±) or OUTPUT.
- CC learn: any CC to DIST/OUT (14-bit pair supported); default suggested map CC1 (mod)→DIST? no defaults; CC 20 DIST, CC 21 OUT, CC 22 stomp (>=64 on) when user enables map.
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
| mxr_distortion_plus_bypass | Stomp | 0 | 1 | 1 (bypassed) | bool | - | 5 xfade | yes |
| mxr_distortion_plus_dist | Distortion | 0 | 1 | 0.5 | norm | rev-log: Pd=1e6·(10^(−2·(1−d))) clipped; Pd(d=0)=0? use Pd=1e6·(exp(k·d)−1)/(exp(k)−1) k=5 (E) | 20 | yes |
| mxr_distortion_plus_out | Output | 0 | 1 | 0.5 | norm | audio taper: gain=(10^(3(o−1))) ; 0→−inf(0 at o<0.001) , 1→0 dB, mid=-30dB*... Specifically g = o<0.001?0:10^(−60(1−o)/20)^(?) use pot 10%@mid → g=0.1 at o=0.5 (A-taper), linear interp between points {0:0,0.5:0.1,1:1} | 20 | yes |
| mxr_distortion_plus_variant | Variant | 0 | 3 | 0 | enum | - | - | no |
| mxr_distortion_plus_in_trim | Input trim (non-physical) | −12 | +12 | 0 | dB | lin | 20 | yes |
| mxr_distortion_plus_quality | Quality | 0 | 1 | 1 | enum eco/hq | - | - | no |
| mxr_distortion_plus_stereo_mode | Stereo mode | 0 | 1 | 0 | enum | - | - | no |
| mxr_distortion_plus_noise | Noise | 0 | 1 | 0 | bool | - | - | no |
| mxr_distortion_plus_stomp_mode | Stomp mode | 0 | 1 | 0 | enum | - | - | no |
Mapping notes: pot taper values E; calibrate vs real unit when measured. Gain at d: G(d)=1+(Pd+Rf)/Rg; Rf (fixed series) ≈ 0? treat 0.

## 9 Tests / acceptance
Provisional (no hardware reference; E; replace with ±measured):
- Bypass off: output bit-identical to input (null ≤ −300 dB). Bypass/engage xfade: no step > 0.01 FS.
- Silence in → |out| < 1e-12 (noise off), no DC drift |DC|<1e-6 after 10 s.
- Gain: d=0, 1 kHz −30 dBFS sine: small-signal gain ≈ 1+Pd/Rg·... ~0 dB ±2 dB (before OUT). d=1 small-signal (−60 dBFS in, 1 kHz≈ above f_g): +44..+47 dB ±3 dB (E) before OUT/clipping (use −70 dBFS to avoid clip). HP corner at d=1: gain at 100 Hz ≥ 15 dB below 1 kHz-ish (f_g 720 Hz shelf); LPF: −3 dB point at d=1 ≈ 3–5 kHz (+/-30%).
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
- All component values/pot tapers E (no schematic fetched, conflicting 500k vs 1M sources). 
- 741 non-linear internals (slew, crossover distortion, rail clipping asymmetry) simplified; real Ge diode tolerance/temperature variance not modelled (each real unit sounds different).
- Impedance interaction with pickups (1 MΩ load) not modelled by default; Zout 25 kΩ not applied.
- Bypass crossfade vs true mechanical contact; no switch pop.
- No power-supply sag/noise; battery state not modelled.
- Vintage 70s variants not modelled. RR104/other MXR designs not covered. No legal claim of equivalence.
- Looper section: N/A (not a looper).
