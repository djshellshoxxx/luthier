# ehx-big-muff-nyc : Fuzz/sustainer ("inspired by Electro-Harmonix Big Muff Pi, NYC reissue") - dev-ready spec

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `ehx-big-muff-nyc`, **paramPrefix `ehx_big_muff_nyc_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/ehx_big_muff_nyc.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/ehx_big_muff_nyc_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_ehx_big_muff_nycTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `ehx_big_muff_nyc_enabled`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims `ehx_big_muff_nyc_input_trim_db` -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params `ehx_big_muff_nyc_soft_press`, `ehx_big_muff_nyc_midi_chan` -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `ehx_big_muff_nyc_quality`, `ehx_big_muff_nyc_stereo_mode`, `ehx_big_muff_nyc_revision`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): (none).
> - Host lanes `ref_s{nn}_p01..` in this order: `ehx_big_muff_nyc_sustain`, `ehx_big_muff_nyc_tone`, `ehx_big_muff_nyc_volume`, `ehx_big_muff_nyc_supply_v`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Version 0.1 | 2026-10-21 | model_id `ehx-big-muff-nyc` | param prefix `ehx_big_muff_nyc_` | status SPEC
Evidence tags: DOCUMENTED (manual/analysis text seen in search excerpts), MEASURED (none yet), ESTIMATED (derived from typical Big Muff schematics / own math; MUST be verified vs. a pinned schematic + hardware capture before any fidelity claim).
Research note: direct page fetch (ehx.com, electrosmash.com, guitarscience.net) failed in sandbox (DNS ENOTFOUND). Values come from search-result excerpts + well-known Big Muff topology. All component values below = ESTIMATED unless tagged. Do not advertise "authentic/identical".

Sources
- S1 ElectroSmash analysis (4 cascaded common-emitter stages: input booster, 2 diode-feedback clipping stages, passive tone LP/HP blend, tone-recovery output; mids reduced at mid tone): https://www.electrosmash.com/big-muff-pi-analysis
- S2 EHX product page (controls, power, bypass): https://www.ehx.com/products/big-muff-pi/
- S3 Guitar Science paper (circuit analysis): https://www.guitarscience.net/papers/bigmuff.pdf
- S4 Wikipedia history (NYC reissue 1992+/2000 US production; Sovtek line separate): https://en.wikipedia.org/wiki/Big_Muff
- S5 EffectsDatabase (variant lineage; tone CCW=bass, CW=treble): https://www.effectsdatabase.com/model/eh/bigmuffpi
- S6 Reverb listing of EHX Op-Amp Big Muff reissue (55 kOhm Zin, ~5 mA, true bypass, 9V batt / 9.6DC-200, >=25 mA supply) - DIFFERENT variant, only used as order-of-magnitude: https://reverb.com/item/99859623-electro-harmonix-op-amp-big-muff-pi-reissue-fuzz-w-original-box
- S7 Nano Big Muff Pi 2 (2.7k max Zout, 15 mA, relay true bypass) - neighbour only: https://www.effectsdatabase.com/model/eh/nano/bigmuffpi/2
- S8 internal prior notes: spec/reference-pedalboard/models/ehx-big-muff-nyc.md

## 1 Identity / variants
- Real unit: EHX Big Muff Pi, mono, all-analog discrete-transistor fuzz/sustainer, 3 knobs + 1 footswitch + LED. No MIDI, no expression jack, no tempo/sync, no I/O beyond in/out/DC. DOCUMENTED (S2,S8).
- Power: 9V battery or centre-negative 9.6V DC (EHX 9.6DC-200 PSU). Draw ESTIMATED 5-10 mA (S6 op-amp ver. ~5 mA, S7 15 mA). Zin ESTIMATED 70 kOhm (S6 op-amp ver. 55k; transistor Muff input base network ~ 50-100k). Zout ESTIMATED <= 25 kOhm (volume pot dependent; S7 2.7k is a different buffered design). Bypass: true bypass (S6/S2 family; NYC-reissue confirmation OUTSTANDING -> ESTIMATED).
- Variant enum `ehx_big_muff_nyc_revision` (hidden in v1): 0=nyc_reissue (DEFAULT, this spec), 1=triangle, 2=rams_head, 3=russian, 4=opamp. Never alias. Revision changes only the constant block `MuffConsts` (sec 3.1). v1 ships rev 0; 1-4 PHASE 2, UI hides selector.
- Display name: "Sustainer (Muff-style, NYC)". No logos, no pi glyph copy of trademark art: draw generic Greek-pi-like original mark or omit.

## 2 Controls (every physical element)
| Control | Range | Taper | Default | Real behaviour |
|---|---|---|---|---|
| SUSTAIN (knob) | 0..10 (UI 0..1) | 100k "A" audio (log) ESTIMATED; model `g(x)=(e^{kx}-1)/(e^k-1), k=4.394` (50% rotation = 10% of travel) | 0.5 | Variable divider between input booster and 1st clip stage. CCW = low gain, CW = max drive, long sustain, compression. At 0 not silent: floor g>=0.005 |
| TONE (knob) | 0..10 | 100k "B" linear ESTIMATED | 0.5 | CCW = LP side (bass, dark), CW = HP side (treble), centre = mid scoop ~1 kHz notch (-13.8 dB rel. passband, sec 3.3). DOCUMENTED direction (S5) |
| VOLUME (knob) | 0..10 | 100k "A" audio ESTIMATED (same g(x)) | 0.5 | Output level after recovery stage. Unity-ish near 0.5 at typical guitar input (calibrate sec 9 T-LEVEL) |
| Footswitch (stomp, SPST momentary-latch) | on/off | n/a | off (bypassed) | True bypass toggle, red LED lit when ON |
| LED | on/off | n/a | off | Red, follows `enabled` |
| INPUT jack | 1/4" mono | - | - | Zin ~70k ESTIMATED |
| OUTPUT jack | 1/4" mono | - | - | Zout ~<=25k ESTIMATED |
| DC jack | 9.6V centre-neg | - | - | Non-modelled except advanced `supply_v` |
| Battery | 9V | - | - | Non-modelled except `supply_v` sag |
No other switches, no jumpers. Anything else (input trim, quality, supply) lives in a clearly-labelled "Advanced" overlay, not on the face.

## 3 Signal path & circuit/DSP model
Signal order (rev 0): INPUT -> [true-bypass switch] -> C_in/HPF -> Q1 input booster -> SUSTAIN divider -> clip stage 1 (Q2, diode feedback) -> coupling -> clip stage 2 (Q3, diode feedback) -> coupling -> passive TONE LP/HP blend -> Q4 recovery -> VOLUME -> OUTPUT.

### 3.1 Constants block `MuffConsts` (rev 0, ALL ESTIMATED; verify vs schematic)
| Name | Value | Note |
|---|---|---|
| Vt | 25.85 mV | 300 K |
| Diode 1N914-like: Is, n | 2.52 nA, 1.752 | Shockley; 1 diode each polarity (antiparallel pair) |
| Rf (clip feedback) | 470 kOhm | both stages |
| Cf (clip feedback) | 470 pF | LP corner of Rf*Cf = 720 Hz in linear region |
| Rs1 / Rs2 (series base R) | 10 kOhm / 10 kOhm | plus divider Thevenin for stage 1 |
| Rbe (base input R) | 25 kOhm | hfe*Vt/Ic estimate |
| A (open loop) | 150 | -A, ~43.5 dB |
| C_in | 100 nF | with Zin 70k -> HPF 23 Hz |
| C_cpl (inter-stage) | 100 nF | HPF fc = 1/(2*pi*(Rs+Rbe)*C_cpl) ~ 45 Hz stage1 |
| Q1 gain | +20 dB (x10), soft-clip rail 3.0 V | |
| Q4 recovery gain | +20 dB (x10), soft-clip rail 3.0 V | |
| Tone: R_lp, C_lp | 39 kOhm, 10 nF | LP corner 408 Hz |
| Tone: R_hp, C_hp | 22 kOhm, 4 nF | HP corner 1.81 kHz |
| Tone pot Rp | 100 kOhm | |
| Output HPF / LPF | 10 Hz / 20 kHz 1-pole | coupling cap / Q4 Miller |
| Vcc nominal | 9.0 V (`supply_v`) | rail scales with supply_v/9 |

### 3.2 Gain/clipping per clip stage (nonlinear implicit, per sample)
Unknown vd = voltage across feedback element (base node minus collector). vn = vd/(1+A), vo = -A*vd/(1+A).
KCL at base: `vin/Rs = vd/(1+A)*(1/Rs + 1/Rbe) + i_f(vd)`, with
`i_f(vd) = vd/Rf + i_c + 2*Is*sinh(vd/(n*Vt))`, `i_c = (2*Cf/T)*(vd - vd[n-1]) - i_c[n-1]` (trapezoidal Cf, T=1/fs_os).
Solve F(vd)=0 by Newton, warm start vd[n-1], max 4 iterations, tol 1e-6 V, damped (step clamp 0.2 V). Derivative uses g_d=(2Is/(nVt))*cosh(...). Clamp |vd| <= 1.3 V (arg of exp <= 30 -> no overflow). `sinh` = (e^x - e^-x)/2 using one `expf` and reciprocal. Output `vo` then soft-clamped by rail: `vo_r = Rail*tanh(vo/Rail)` Rail=3.0 V*(supply_v/9).
Result: output limited ~+-0.55..0.65 V (diode knee), gain at small signal ~Rf/Rs=47 (33 dB) with 720 Hz LP at high gain; two stages in series -> dense fuzz with heavy intermod (low-E chord muddies, DOCUMENTED character S1).
Stage-1 Rs = Rs1 + Rth, `Rth = Rp*g(1-g)` (Thevenin of sustain divider, Rp=100k, g=g(sustain), driven by Q1 Rout assumed 1k). Input to stage 1 = Q1_out * g (loaded divider ignored; fold load into g via `g' = g*Rbe/(Rbe+Rp*(1-g)+Rs1)` ESTIMATED; document the choice, keep consistent in tests).

### 3.3 Tone network (exact linear 2-node, solved once per control change)
Nodes A (LP branch) and B (HP branch), pot between A and B, wiper fraction `t` (0 = LP end). gl=1/R_lp, gh=1/R_hp, gp=1/Rp.
```
D(s)  = s^2*Cl*Ch + s*(Cl*(gh+gp) + Ch*(gl+gp)) + gl*gh + gl*gp + gp*gh
NA(s) = s*Ch*(gl+gp) + gl*(gh+gp)
NB(s) = s^2*Cl*Ch + s*Ch*(gl+gp) + gp*gl
H(s)  = ((1-t)*NA + t*NB) / D          (unloaded wiper; Q4 input load added as gp-parallel R_load=100k ESTIMATED -> ignore in v1, add in rev tweak)
```
Discretise: bilinear (TPT/biquad) at the OVERSAMPLED rate, no prewarp needed (corners << fs_os/4). Recompute 5 biquad coefs when `tone` changes: per 16-sample sub-block while smoothing, cap 1 recompute/16 samples.
Checkpoint magnitudes (computed from formulas above, dB, ESTIMATED; used by T-FR):
| t \ f | 80 | 160 | 320 | 500 | 800 | 1k | 1.25k | 2k | 4k | 8k |
|---|---|---|---|---|---|---|---|---|---|---|
| 0.0 | -2.5 | -2.8 | -3.7 | -5.0 | -7.1 | -8.3 | -9.7 | -12.7 | -17.6 | -23.2 |
| 0.25 | -4.5 | -4.8 | -5.9 | -7.4 | -10.1 | -11.6 | -13.0 | -14.5 | -13.3 | -12.4 |
| 0.5 | -7.1 | -7.5 | -8.7 | -10.6 | -13.2 | -13.8 | -13.3 | -10.5 | -7.5 | -6.4 |
| 0.75 | -10.8 | -11.3 | -12.7 | -14.2 | -13.7 | -12.0 | -10.1 | -6.7 | -3.9 | -2.9 |
| 1.0 | -17.4 | -17.6 | -17.3 | -15.0 | -10.8 | -8.8 | -6.9 | -3.8 | -1.3 | -0.3 |
At t=0.5 notch min at 1017 Hz, -13.8 dB (ESTIMATED; real units cited ~1 kHz scoop, S1).

### 3.4 Remaining stages
- Input: HPF 1-pole 23 Hz; Zin source interaction ignored (board passes ideal source) unless `input_load_enable` (advanced, default off).
- Q1: `y = Rail*tanh(10*x/Rail)` after HPF (soft-clip at 3 V).
- Inter-stage couplings: 1-pole HPF each, fc per sec 3.1 (stage1 recomputed with Rth: update per control change).
- Q4: `y = Rail*tanh(10*x/Rail)`. Then VOLUME gain `gv = g(volume)*Gcal` where Gcal set so T-LEVEL passes (default ESTIMATED 4.0 incl. recovery). Output HPF 10 Hz, LPF 20 kHz.
- Bias/DC: all stages AC-coupled; no DC offset modelled; output DC blocker mandatory.

### 3.5 Latency, bypass, transitions
- Latency: from oversampler only: Normal(4x halfband FIR cascade) L=24 samples @1x; Eco(2x IIR elliptic) L=0 (min-phase, reports 0); HQ(8x FIR) L=40. ESTIMATED; reported via `getLatencySamples()`, fixed per (quality, sr), independent of bypass (bypass path delay-matched, BOARD_CORE 5.1).
- Bypass mode: `TrueBypass` (BOARD_CORE enum True). No buffered colouration. No tails (analog fuzz stops instantly); wet path continues only through xfade. Default xfade 5 ms; FootEvent speed gives lerp(12,2) ms. `bypassClick` option default off.
- State on enable: coupling caps stay charged in hardware -> no thump. Software: when pedal enabled after >50 ms bypassed, pre-roll last 1024 base-rate samples from a per-slot input ring through the wet path (output discarded) inside the enabling block; peak cost 1 block of extra ~1024*OS samples ONCE (budget sec 4.5).

## 4 Algorithm for plugin
### 4.1 Block diagram
```
in -> trim -> [upsample xN] -> HPF23 -> Q1 tanh -> *g_sus -> HPF(c1) -> STAGE1 (implicit diode FB)
   -> HPF(c2) -> STAGE2 -> tone biquad x2 (H(s)) -> Q4 tanh -> *g_vol -> HPF10/LPF20k
   -> [downsample] -> xfade(bypass) -> out
```
### 4.2 Per-sample (at OS rate) summary
1. x = u(n) * (Vref_scale) ; Vref_scale: 0 dBFS = 1.0 V peak (`ehx_big_muff_nyc_ref_vpk`, advanced, default 1.0 V) so input = x_FS * ehx_big_muff_nyc_ref_vpk * 10^(trim_dB/20).
2. y1 = HPF(x); q1 = Rail*tanh(10*y1/Rail).
3. s1 = q1*g_sus'; c1 = HPF_c1(s1).
4. vd1 = Newton(c1; Rs1+Rth); o1 = softrail(-A*vd1/(1+A)); (sign inversion ignorable; keep for phase accuracy per stage: two inversions cancel, keep absolute polarity same as input -> net non-inverting after Q1/Q4 inversions: define net polarity = non-inverting.)
5. c2 = HPF_c2(o1); vd2 = Newton(c2; Rs2); o2 = softrail(...).
6. tn = Tone(o2) (2 cascaded-free biquad; one biquad, 2nd order); q4 = Rail*tanh(10*tn/Rail).
7. out = LPF20k(HPF10(q4*g_vol*Gcal)) / ehx_big_muff_nyc_ref_vpk -> back to FS.
### 4.3 Oversampling & aliasing
Default 4x (Normal) via two cascaded polyphase halfband FIR (>=100 dB stopband, passband flat 0.1 dB to 0.45*fs). Eco 2x allowed. HQ 8x for offline render (`quality`). Reason: two cascaded hard-ish diode clippers at up to 66+ dB gain; aliasing spec: with 1.2 kHz sine, sustain 1.0, 48 kHz, all alias products >= 60 dB below fundamental (Normal), >= 48 dB (Eco), >= 75 dB (HQ). Insert 1st-order ADAA NOT used (implicit solve). Stage-local LP from Cf (720 Hz at high gain) already band-limits drive; measured with test.
### 4.4 Smoothing & denormals
- Params smoothed: sustain, tone, volume exp/lin per sec 8 (ms); gains computed once per 16-sample sub-block at OS rate; Newton Rs update per sub-block.
- Discrete params (quality, revision, enabled): immediate + 5 ms xfade where audible; quality change triggers oversampler rebuild OFF audio thread (prepare path) with double buffer swap.
- Denormals: `juce::ScopedNoDenormals` per block + flush filter states `if (fabs(z) < 1e-20f) z = 0`; add 1e-18 DC to feedback integrators.
- NaN guard: after Newton, if `!isfinite(vd)` -> reset stage state vd=0, ic=0, count `nanResets` (atomic, UI diag).
### 4.5 CPU budget
Per OS sample: 2 stages x <=4 Newton iters x (1 exp + ~12 flops) + 2 tanh + 2 biquad ~ 220 cycles; Normal 4x -> ~900 cycles/base sample ~ 1.4% of one 3 GHz core @48 kHz. Budget: <= 2% (Normal), <= 1% (Eco), <= 4% (HQ) per instance; peak enable pre-roll block <= 8x steady cost once. Zero alloc, zero locks in `process`; oversampler/biquad tables preallocated in `prepare`; tanh via fast rational approx (max err 1e-4) allowed with `JUCE_USE_VDSP`/SIMD optional.

## 5 I/O & routing
- Hardware: mono in, mono out. Plugin: IPedal mono-processing; stereo bus: process L and R through two independent instances (stateful dual-mono, shared params) when `stereo_mode=dual_mono` (default), or sum-to-mono then duplicate when `stereo_mode=mono_sum`. Dual-mono nonlinear: different content per channel intentionally decorrelates (same as two pedals).
- No extra jacks on real unit -> no send/return, no aux, no expression, no MIDI on hardware.
- Board-level tricks available to user (BOARD_CORE routing, not pedal features): (a) series ahead/behind other pedals (order switchable, chain accurate); (b) parallel (split to Muff + clean, mix with board wet/dry or parallel-merge slot); (c) effects-loop feedback: Muff inside a board loop where a delay/reverb output returns pre-Muff (feedback routing, user-gain-controlled; limiter `fb_limit` required to stop runaway since Muff has huge gain); (d) stereo split: L->Muff A, R->Muff B with different settings (two instances via board split slot); (e) stack two Muffs in series for double fuzz. Document runaway safety: board feedback path hard-clips at +6 dBFS and has DC blocker.

## 6 MIDI / sync / expression emulation (Virtual MIDI, not on hardware; off in "hardware-faithful" preset mode)
- MIDI clock / tap tempo / DAW sync: N/A (no time-based function). `ehx_big_muff_nyc_*` params never tempo-synced. Documented as "none, same as hardware".
- Default CC map (ESTIMATED own allocation, free pool BOARD MIDI_EXPRESSION 4.3; channel via `ref_board_midi_chan`): CC70 -> sustain, CC71 -> tone, CC72 -> volume, CC80 -> enabled (>=64 on, <64 off, latch), CC11 -> `@sel.expression` target (default sustain, range 0..1). All soft-takeover ON for knobs, OFF for CC80. 14-bit optional via CC70/102 not needed.
- Program Change: board-level preset recall only; no pedal memories.
- Expression pedal target: `ehx_big_muff_nyc_sustain` (flag `expressionTarget`), alt `volume` (boost-on-heel-toe). Curve default log (matches audio taper) lo=0.0 hi=1.0. Hardware has no jack -> label "virtual expression".
- Footswitch gestures (BOARD_CORE 9.6 FootController): Note-on key -> latch toggle `enabled` (default key mapped via learn); Momentary mode: note held = ON, release = OFF (min hold 30 ms). Velocity -> `pressSpeed` -> bypass xfade ms = lerp(12,2,speed); NO velocity-to-sustain by default. Optional `softPress` (default off): channel/poly aftertouch force (0..1) drives wet/dry mix of the Muff slot continuously (0 = dry, 1 = wet) while key held; this emulates half-engaged foot only as artistic feature, labelled "not on hardware".
- Aftertouch/pitch-bend/mod-wheel/slider/knob mapping: any param mappable via MIDI-learn (BOARD MIDI spec sec 6); CC1 default unmapped.
- LED: follows `enabled`, 80 ms flash on MIDI activity bound to the footswitch.

## 7 Visuals (original vector art; no photo copy)
- Enclosure: rectangular die-cast box, polished/brushed silver-chrome face (gradient #C9CDD2 -> #8E949B diagonal 20%), 1.5 mm darker bevel edge, 4 hidden corner screws NOT shown. Dimensions ESTIMATED W 111 x D 121 mm (face), H 54 mm (side view only).
- Layout (face, origin top-left, mm, ESTIMATED): three knobs in one row at y=34; centres x = 27.5 (VOLUME), 55.5 (TONE), 83.5 (SUSTAIN) -> text labels right-to-left reading order matches classic arrangement (Volume | Tone | Sustain). Knob dia 20 mm, skirt dia 24 mm, black plastic with white pointer line 1.2 mm x 8 mm, 11 o'clock to 5 o'clock sweep (-135 deg .. +135 deg).
- LED: 5 mm red dome at (55.5, 14); lit = #FF2A2A + 6 mm glow; off = #4A0A0A.
- Footswitch: 12 mm dia chrome stomp, centre (55.5, 92), 17 mm bezel ring.
- Jacks drawn on side view only: INPUT right side, OUTPUT left side (or per rack side convention), DC jack top edge. Not clickable.
- Text labels (our own typography, sans condensed bold, black #151515): "VOLUME", "TONE", "SUSTAIN" 4.5 mm cap height under each knob; title "MUFF SUSTAINER" (generic) 6 mm centred at y=62; sub-line "NYC-STYLE FUZZ" 3 mm. Tick marks 0..10 around knob, minor each 1.
- Wear options (cosmetic): `case_wear` 0..1 scratches/dirt overlay procedurally from seed; deterministic.
- Footprint for board placement: 111 x 121 mm, footprint for screws/velcro/clamp rendering uses 4 corner-adjacent mounting zones (see UI_BOARD_STYLES).

## 8 Parameter table
id | name | min | max | default | unit | skew | smoothing ms | automatable
---|---|---|---|---|---|---|---|---
ehx_big_muff_nyc_enabled | Footswitch | 0 | 1 | 0 | bool | - | xfade 5 | yes
ehx_big_muff_nyc_sustain | Sustain | 0 | 1 | 0.5 | norm (maps to g(x) k=4.394) | audio (UI linear) | 20 | yes
ehx_big_muff_nyc_tone | Tone | 0 | 1 | 0.5 | norm (linear pot) | linear | 20 | yes
ehx_big_muff_nyc_volume | Volume | 0 | 1 | 0.5 | norm (audio taper) | audio | 20 | yes
ehx_big_muff_nyc_input_trim_db | Input trim (adv) | -12 | 12 | 0 | dB | linear | 20 | yes
ehx_big_muff_nyc_supply_v | Supply (adv) | 5 | 9.6 | 9.0 | V | linear | 50 | yes
ehx_big_muff_nyc_quality | Quality (adv) | 0 | 2 | 1 | enum 0=eco2x 1=normal4x 2=hq8x | - | 0 (rebuild) | no
ehx_big_muff_nyc_stereo_mode | Stereo (adv) | 0 | 1 | 0 | enum 0=dual_mono 1=mono_sum | - | 0 | no
ehx_big_muff_nyc_revision | Revision | 0 | 4 | 0 | enum (v1 locked 0) | - | 0 | no
ehx_big_muff_nyc_soft_press | Soft press mix | 0 | 1 | 0 | bool (virtual) | - | 5 | yes
ehx_big_muff_nyc_midi_chan | MIDI channel | 0 | 16 | 0 | int (0=omni) | - | 0 | no
Expression targets: sustain (default), volume. Serialization: float norm + enum ints; unknown future keys ignored; revision key absent => 0.

## 9 Tests / acceptance
Provisional until MEASURED capture; thresholds marked P.
- T-IMPULSE: unit impulse at tone=0.5,sustain=0 -> finite, tail < -80 dB after 100 ms. No DC after 5 s silence (|mean| < 1e-6 FS).
- T-FR (small signal, input 5 mV sine, sustain=0): tone network vs sec 3.3 table, tol +-1.0 dB at 5 tone positions x 10 freqs (the table is the golden; includes interstage/in/out HPFs: compare after dividing out their analytic response). Null test vs offline double-precision MATLAB/Python reference (same equations) -> residual < -90 dB at fs=48k, 4x.
- T-NOTCH: tone=0.5 notch freq 1017 Hz +-8%, depth -13.8 dB +-1.5 dB.
- T-SUS-SWEEP: 1 kHz, 100 mV pk; sustain 0..1 in 11 steps: output RMS rises monotonic then saturates; at 1.0 clip ceiling per stage within 0.5-0.7 V; output crest factor falls < 2.2 (P); THD(1 kHz, 100 mV, sustain 0.5, volume 0.5): 15-35% (P); sustain 1.0: > 40% (P).
- T-LEVEL: at 100 mV pk 1 kHz, sustain=0.5,tone=0.5,volume=0.5, output within -6..+6 dB of input (P) - tune Gcal.
- T-IMD: two-tone 110 Hz + 165 Hz (low-E power chord), sustain 0.8: intermodulation products at 55 Hz and 220 Hz present within 20 dB of fundamentals (documents muddy chord behaviour, P).
- T-STEP/pluck decay: synthetic decaying pluck (A=0.3 V, tau 1.5 s) -> output envelope falls < 6 dB over first 1.0 s at sustain 1.0 (sustainer behaviour, P).
- T-ALIAS: sec 4.3 targets; swept 0.2-8 kHz sines.
- T-BYPASS: enabled=false -> output == input delayed by L (bit-exact after latency pad); toggle click-free (|diff| peak < 0.05 FS over xfade).
- T-NAN/STRESS: random params/steps 30 min soak, fs {44.1,48,88.2,96,192}k, block {1,4,16,64,512}: no NaN/Inf, nanResets == 0, CPU within sec 4.5, deterministic with noise disabled.
- T-LAT: impulse through board with this pedal bypassed/enabled aligns (BOARD_CORE T-LAT).
- T-MIDI: CC70 0..127 maps monotonic to sustain via audio taper; soft-takeover; CC80 latch; velocity->xfade length lerp(12,2); aftertouch soft_press mix; learn round trip.
- T-LEGACY: board off -> bit-identical legacy path.
- UI: knob reachability (0 and 1 hit-testable), keyboard step 0.01, labels exact strings from sec 7, snapshot images at 100%/150% scale.
- Fidelity score = weighted pass of MEASURED tests; until hardware capture, mark "model: unverified".

## 10 Known inaccuracies
- All component values ESTIMATED; real NYC-reissue board revisions differ (C values, transistor types 2N5088/BC239 hFE spread). Source fetch blocked => no direct schematic verification.
- Bypass type of NYC reissue unconfirmed (assume true).
- Impedance interaction guitar<->Zin, pot loading of tone/sustain, transistor hfe/Ic spread, power-supply sag, supply noise, microphonics, battery vs adaptor tonality, per-unit pot tolerance, transistor noise and hiss: not modelled (noise floor optional later `noise_enable`, default off).
- Tone-stack loading by Q4 input ignored in v1.
- Diode model: simple Shockley with series R=0; real clipping asymmetry (diode mismatch) absent. Possible Phase 2 `diode_mismatch` 0..5%.
- Variants (Triangle, Rams Head, Russian, Op-Amp) not modelled; do not label as such.
- MIDI/expression/gestures are plugin conveniences, not hardware behaviour.
