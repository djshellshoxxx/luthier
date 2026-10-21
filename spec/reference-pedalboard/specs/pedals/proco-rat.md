# SPEC: proco-rat (inspired by Pro Co RAT distortion)

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `proco-rat`, **paramPrefix `proco_rat_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/proco_rat.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/proco_rat_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_proco_ratTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `proco_rat_bypass`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims `proco_rat_in_trim` -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `proco_rat_variant`, `proco_rat_in_mode`, `proco_rat_noise`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): `proco_rat_at_depth`, `proco_rat_expr_target`.
> - Host lanes `ref_s{nn}_p01..` in this order: `proco_rat_distortion`, `proco_rat_filter`, `proco_rat_volume`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

model_id `proco-rat` | cat Distortion | status DEV-READY (circuit values ESTIMATED from standard public RAT schematic; see s10) | path `Source/ReferencePedals/Models/proco_rat.h/.cpp`
Tags: MEASURED = bench measurement; DOCUMENTED = vendor/datasheet/public analysis; ESTIMATED = derived/recalled, verify vs real unit.
Legal: behaviour model + original vector art. "Inspired by Pro Co RAT" descriptive only. No vendor photos/logo.

## Sources
- S1 ElectroSmash RAT analysis https://www.electrosmash.com/proco-rat (search-confirmed BOM + topology; page body NOT fetchable this session -> values below cross-checked via S2/S3 only)
- S2 Tonepad RAT layout/BOM https://www.ultimate-guitar.com/static/storage/forum/attachment/a/f/94034__tonepad_rat-1.pdf
- S3 LTSpice RAT write-up https://cushychicken.github.io/ltspice-proco-rat/ (gain eq: Av=1+Rgain/(560||47) ~2300 V/V ~67 dB)
- S4 Pro Co RAT2 listing (B&H sound_rat2): Zin 1 Mohm, Zout 1 kohm, gain 60 dB, EIN -110 dB, LPF 6 dB/oct, max out 1.2 Vpp (DOCUMENTED retailer-quoted)
- S5 Wikipedia Pro_Co_RAT (variants/dates; LM308 -> OD07DP swap ~1996, disputed: PedalPCB says early 00s)
- S6 guitar.com RAT2 review (filter max ~ just under 500 Hz)
- S7 PedalPCB "Rat Circuit Theory" https://forum.pedalpcb.com/threads/rat-circuit-theory.30300/
- S8 National LM308 datasheet (SR ~0.3 V/us @30 pF comp, GBW ~1 MHz @30 pF) ESTIMATED from recall.

## 1 Identity / variants
| id | variant | diodes | opamp | notes |
|---|---|---|---|---|
| `classic` (default) | 1978-era RAT / RAT2 "vintage" | 2x 1N914 antiparallel (hard-ish) | LM308 | S1,S2 DOCUMENTED |
| `op07` | later RAT2 (OD07DP/OP07) | 1N914 | OP07-class, GBW ~0.6 MHz, SR ~0.3 V/us, comp internal | S5 DOCUMENTED date disputed; model params ESTIMATED |
| `led` | Turbo-RAT-style clipping | 2x red LED antiparallel (Is~1e-18..1e-20, n~1.9..2.0 ESTIMATED -> Vf~1.7V) | LM308 | S5 DOCUMENTED. Turbo-RAT extras (extra tone switch) NOT modelled |
Variant is a stepped selector in an Advanced overlay, NOT a physical control. Default `classic`. One footswitch bypass pedal, mono.
Unmodelled: Fat RAT, You Dirty RAT, RAT mods (clipping toggles) = future (s10).

## 2 Controls
| control | type | range | taper | default | real behaviour |
|---|---|---|---|---|---|
| DISTORTION | knob 0..10 | 0..10 | 100k audio (log "A") DOCUMENTED S2 | 5.0 | pot is feedback resistor Rd of op-amp. Rd(x)=100k * taper(x), taper(x)=(10^(a*x/10)-1)/(10^a-1)... use a=2 ESTIMATED -> 10% rotation ~ 1k order |
| FILTER | knob 0..10 | 0..10 | 100k "A" wired so CW = darker (reverse treble) DOCUMENTED | 5.0 | Rf = pot in series with 1.5k fixed to 3.3nF shunt. CCW(0)=Rf 0 -> bright (32 kHz); CW(10) -> Rf 100k, ~475 Hz |
| VOLUME | knob 0..10 | 0..10 | 100k audio | 5.0 | divider at output stage; unity near ~ 3-4 ESTIMATED; plenty of boost at max (+~12 dB vs unity) ESTIMATED |
| Footswitch | 3PDT latching | on/off | - | off | true bypass DOCUMENTED (3PDT) |
| LED | red | - | - | follows ON | indicator, no PWM |
| IN jack | 1/4" mono | - | - | - | Zin 1 Mohm DOCUMENTED S4 |
| OUT jack | 1/4" mono | - | - | - | Zout ~1 kohm DOCUMENTED S4 (JFET follower + volume pot, varies) |
| DC jack | 2.1mm, 9V centre-neg | - | - | - | no audible effect; plugin: optional "sag" off by default |
Pot position -> normalized plugin param p in [0,1]; display = p*10 with 1 decimal. Taper applied inside DSP, host param is linear 0..1.

## 3 Signal path / circuit model
Stage order (ESTIMATED from S1-S3, standard schematic):
1. Input: C1 22nF series, R 1M to Vb(4.5V bias) -> HPF fc=1/(2pi*1M*22nF)=7.2 Hz (effectively DC block). Series 1k + (input RF cap ~ 1nF to ground, fc 160 kHz, ignore). Zin 1M.
2. Op-amp U1 LM308 non-inverting gain stage. Ground leg is two parallel branches: Rg1=560 ohm + C=4.7uF, Rg2=47 ohm + C=2.2uF. Feedback: Rd (DISTORTION pot 0..100k) || Cf 100pF; Cc 30pF comp.
   - Zg(s) = (560 + 1/(s*4.7u)) || (47 + 1/(s*2.2u)).
   - Gain Av(s) = 1 + Zf/Zg, Zf = Rd || 1/(s*100p).
   - Branch corners: 560 + 4.7u -> fc=60.5 Hz; 47 + 2.2u -> fc=1539 Hz. Low-end rolls off, gain rises ~ +20dB mid bump at ~1.5k+ (the "RAT" mid hump). Max |Av| (Rd=100k, f>>1.5k) = 1+100k/(560||47)=1+100k/43.4=2304 (67.2 dB) DOCUMENTED S3. S4 quotes 60 dB (retailer, different def.).
   - Feedback cap pole: fc=1/(2pi*Rd*100p) = 15.9 kHz @Rd=100k; 159 kHz @10k.
   - Op-amp finite GBW: closed-loop -3dB ~ GBW/Av -> @Av=2304, GBW=1MHz -> 434 Hz. Dominant effect at high gain: bandwidth collapse. ESTIMATED (S8).
   - Slew limit: SR 0.3 V/us (LM308 +30pF) ESTIMATED -> triangulates large signals; max slope 0.3V/us => 3.77V pk at ~12.7 kHz... at 1 kHz: pk>47V (n/a); matters when high-gain output edges. Model as slew-limited integrator.
   - Output swing: clamp to rails Vb +- ~3.5 V (9V supply, op-amp headroom ~1.5V) -> soft rail clip ESTIMATED. Asymmetry: +3.4/-3.6 V ignore, symmetric.
3. Coupling C 4.7uF + R 1k series into diode clipper: D1/D2 antiparallel to ground (Vb-referenced). Vf(1N914, ~1mA) ~0.55-0.65 V.
   Diode eq: i = Is*(exp(v/(n*Vt)) - exp(-v/(n*Vt))), Is=2.52e-9 A, n=1.752, Vt=25.85 mV (1N4148 SPICE ESTIMATED). `led`: Is=1e-18..2.5e-16 (choose 3e-18), n=1.95.
   Clipper node: (vin - v)/Rs = i(v). Rs=1k. Output of op-amp can be rail-clipped before diodes.
4. Tone: Rt=1.5k + FILTER pot (Rf 0..100k) series -> 3.3nF to ground. fc = 1/(2pi*(1.5k+Rf)*3.3n): Rf=0 -> 32.2 kHz; Rf=100k -> 477 Hz (S6 "just under 500 Hz", ElectroSmash 475). 1st-order, 6 dB/oct DOCUMENTED S4.
5. Output: JFET (2N5458-class) source follower -> 1uF coupling -> VOLUME 100kA -> OUT. Gain of follower ~0.9 (-0.9dB) ESTIMATED. Zout ~1k.
Latency: 0 samples analog; plugin adds oversampling filter latency (compensate or report via `getLatencySamples`; see s4).
Bypass: TRUE (DOCUMENTED 3PDT). Input to output hard-wired. Transition: mechanical switch, tiny pop on real unit when pulldown absent; plugin: 5 ms equal-power crossfade, no pop. Dry path = exact input; wet path latency-compensated (delay dry by oversampler latency).
Noise: real EIN -110 dB (S4). Plugin optional hiss `noise_on`, default OFF (deterministic tests): white noise at -110 dBu->mapped -> add before gain stage amplitude 3.2uV rms equivalent scaled by DI calibration.
Level calibration: 0 dBFS = +4 dBu nominal => 1.228 V rms ; set `kRefVolts = 0.775*10^(4/20)... =1.228`. Guitar level assumed -20 dBFS = ~123 mV rms hmm: real DI ~0.1-0.3 Vrms. Input scale param `in_trim_db` default 0 (hidden, Advanced) range -12..+12.

## 4 Algorithm
Block diagram:
`in -> [os up x4/x8] -> HPF(7Hz) -> OpAmpGain(Zg,Zf,GBW,SR,rail) -> HPF(4.7u,1k..) -> Diode clipper (Newton/ADAA) -> LPF(tone) -> follower gain -> VOL -> [os down] -> bypass xfade -> out`
Per-sample (at fs_os = 4*fs, 8*fs when distortion>0.7 and fs<=48k):
- Coeffs recomputed at control rate (every 32 samples or on param change), smoothed.
- Gain stage linear core: two branch-parallel ground impedance -> implement as TF H(s)=1+Zf/Zg. Zf/Zg is rational: build via bilinear transform (prewarp at 1 kHz) into 2nd-order biquad cascade (2 zeros, 3 poles with GBW): Av(s)=1+Rd(1+sRgC...) Compute numerically: derive polynomial coefficients in code from R/C; test vs analytic response.
- GBW as extra pole: H_total = H(s) * 1/(1 + s/(2pi*GBW/Av_dc)) (single-pole closed-loop ESTIMATED approx; use |Av_hf| for pole). Fold into biquad cascade.
- Slew limiter: y[n] = y[n-1] + clamp(x - y[n-1], -SRdt, +SRdt), SRdt = 0.3 V/us / fs_os. Apply AFTER gain stage, BEFORE rail clip. Hard rail clip: y = Vc*tanh(y/Vc), Vc=3.5 V ESTIMATED (reduces to soft clamp).
- Clipper: solve vin - v = Rs*2*Is*sinh(v/(nVt)). Use Wright-omega closed form (Pa-Pa / Werner): v = vin - Rs*Is*... implement `diodeClipSym(vin)`:
  b = Is*Rs/(nVt); x = vin/(nVt) -> v = vin - nVt*sgn*... (use `wrightOmega` approx; fallback 2 Newton iters from previous output, tolerance 1e-6). ADAA 1st-order optional via antiderivative of numerically defined fn, NOT required with 4x OS at -> default disabled.
- Tone LPF: bilinear 1-pole, coefficient smoothed.
- Follower: y *= 0.9. VOL: gain G_v = 10^((v_db)/20), v_db from taper: p=0 -> -inf, p=0.5 -> ~ -4 dB, p=1 -> +12 dB ESTIMATED; use `G_v(p)=g_max * (10^(2*(p-1))...)` equivalent: G_v = 4.0 * (10^(2p) - 1)/(10^2 - 1) (log taper a=2). Check p=0.5: 4*(10-1)/99=0.36 (~ -9 dB); p=1:4(+12dB). Set unity at p~0.72. ESTIMATED.
Oversampling: 4x polyphase IIR half-band (JUCE `dsp::Oversampling` filterHalfBandPolyphaseIIR) preallocated in prepare(); 8x switch when high gain & fs<=48k only if CPU permits. Alias target: -60 dB for 1 kHz sine at gain 10, 4x.
Aliasing control: HPF at 7 Hz input, tone LPF final; clipper nonlinear before tone LPF (hf content reduced), diode node smoothed by 1k+3.3n anyway.
Smoothing: `juce::SmoothedValue<float, Multiplicative>` for gains, linear for coeff-mix; DISTORTION/FILTER/VOLUME all 20 ms; bypass xfade 5 ms. Coefficient update every 16 os-samples ok.
Denormals: `juce::ScopedNoDenormals` at block start + add 1e-18f DC offset in each recursive section; flush states < 1e-20 on bypass.
CPU budget: <=1.2% single core @ 48 kHz/4x/512 on 3 GHz x86 (about 25 flops/sample osr incl. 2 Newton iterations); <=2.5% @ 8x. Mono; stereo host in -> sum-to-mono option `in_mode`. Memory: static, zero alloc/lock on audio thread; params via atomics; variant switch crossfaded 20 ms through double-buffered state, not realloc.
RT safety: no heap, no locks, no `std::exp` inside hot loop other than diode (use fast exp polynomial with error <1e-4 relative).

## 5 I/O & routing
- 1 in mono, 1 out mono; no extra jacks (no loop, exp, MIDI, aux) DOCUMENTED.
- Plugin stereo: `in_mode` {mono-sum, L-only, R-only, dual-mono (2 instances, linked controls)} default dual-mono. Dual-mono processes L and R independently with shared params.
- Board user tricks (host board): series/parallel (board handles), RAT in effects loop of another pedal (board loop block), feedback via board send/return with delay -> RAT is high-gain: board feedback tap goes through gain trim; limit loop gain to < 1 by soft-clip at +6 dBFS. Parallel: RAT wet_mix is not on pedal; board universal wet/dry applies.
- Stereo split: board splits to two RATs, one with Filter CCW/other CW (allowed, not pedal-native).
- Impedance interaction: input Zin 1M, board "source impedance" model option (high Z pickup, 10k default) forms 7.2 Hz HPF + minor LPF with 1k||cable. ESTIMATED.

## 6 MIDI / sync / expression emulation (real unit has none; plugin-layer, flagged EMU)
- No tempo/tap/clock content. MIDI clock ignored.
- CC map (EMU; default channel omni, user-remappable; `midi_learn`):
  | CC | target | |
  | 20 | DISTORTION | 0..127 -> 0..1 |
  | 21 | FILTER | |
  | 22 | VOLUME | |
  | 23 | bypass | >=64 on, <64 off (also footswitch emulation) |
  | 24 | variant | 0-42 classic,43-85 op07,86-127 led |
- Program Change: loads preset slot 0..127 (board preset system), 5 ms crossfade.
- Expression pedal (CC11 default, EMU): user target {DISTORTION (default), FILTER, VOLUME}, heel/toe range min/max params `expr_min`, `expr_max`.
- Velocity / aftertouch / footswitch gestures (EMU): Note On velocity -> bypass toggle with press speed gesture: velocity>=100 = instant (1 ms ramp), velocity<100 maps to 5..40 ms crossfade (slower press); channel aftertouch -> DISTORTION modulate (`at_depth` 0..1, default 0); poly AT same, per-pad. Sustain pedal CC64 = momentary bypass (hold = on). Slider/knob mapping: any CC via learn, 14-bit pairs (CC +32) supported.

## 7 Visuals (original vector art, ESTIMATED proportions)
Enclosure: matte black rounded-rect diecast box, **W 120 x H 95 mm**, corner radius 4 mm, edge highlight 1 px #3a3a3a, base #0e0e10. Text white.
- Top row knobs (centres, origin top-left of face, y=28mm): DISTORTION x=24, FILTER x=60, VOLUME x=96. Knob dia 18 mm, white pointer line, chicken-head or round black knob with white indicator, skirt marks 0..10 ticks (11 ticks, 270 deg sweep, -135..+135 deg), numerals 0 and 10 at the extremes, labels "DISTORTION" "FILTER" "VOLUME" 3 mm caps above... below knob at y=44, label font 3.2 mm bold sans.
- Footswitch centre x=60,y=72, dia 14 mm chrome. LED red dia 4 mm at x=60,y=50 (above switch) with glow when ON.
- Jacks: IN left side y=48, OUT right side y=48, DC top-right. Draw as small circle dia 6 mm.
- Wordmark: original vector "RAT-style" rodent glyph (own drawing, NOT vendor logo); text "RAT-INSPIRED". Footer small text "DISTORTION".
- Colour variants optional: `classic` black; `led` same box + red accent. Drawer key `proco-rat`; no change to `PedalType`.

## 8 Parameters (all prefix `proco_rat_`)
| id | name | min | max | default | unit | skew | smooth ms | automatable |
|---|---|---|---|---|---|---|---|---|
| proco_rat_distortion | Distortion | 0 | 1 | 0.5 | norm(0-10) | log "A" a=2 inside DSP | 20 | yes |
| proco_rat_filter | Filter | 0 | 1 | 0.5 | norm(0-10) | log "A" a=2 (fc inverse) | 20 | yes |
| proco_rat_volume | Volume | 0 | 1 | 0.5 | norm(0-10) | log "A" a=2 | 20 | yes |
| proco_rat_bypass | Bypass | 0 | 1 | 1 (off) | bool | step | 5 | yes |
| proco_rat_variant | Variant | 0 | 2 | 0 | enum | step | 20 | no (Advanced) |
| proco_rat_in_trim | Input Trim | -12 | 12 | 0 | dB | linear | 20 | yes (Advanced) |
| proco_rat_in_mode | Input Mode | 0 | 3 | 3 | enum | step | - | no |
| proco_rat_noise | Noise | 0 | 1 | 0 | bool | step | - | no |
| proco_rat_at_depth | Aftertouch Depth | 0 | 1 | 0 | norm | linear | 20 | yes |
| proco_rat_expr_target | Expr Target | 0 | 2 | 0 | enum | step | - | no |
| proco_rat_expr_min / _max | Expr range | 0 | 1 | 0 / 1 | norm | linear | 20 | yes |
All physical knobs in UI; Advanced items under overlay (not physical).

## 9 Tests / acceptance (thresholds PROVISIONAL until verified vs a hardware unit of same revision; do not claim "authentic")
1. Null-test: render model with 100 uV/1 mV/10 mV/100 mV, 1 kHz sine at 5 knob positions vs ngspice/LTSpice reference netlist from S1/S3 BOM; error <= -40 dBFS RMS (linear region), <= -30 dBFS (clipped).
2. Linear response (Rd small=1k, input 1 mV): |H| matches analytic Zg/Zf to +-0.5 dB 20 Hz..8 kHz. Checkpoints: at DISTORTION=10, input 1 mV, 1 kHz: |Av| 52-60 dB expected (rolloff by 60Hz/1.5k branches, and GBW); 5 kHz: GBW limited, -3 dB near ~430 Hz with gain 2304 (S8 estimate; ±30%).
3. FILTER sweep: -3 dB freqs: p=0 -> 32 kHz (±5%), p=0.5 -> ~ (Rf=? log) , p=1 -> 477 Hz ±5%; monotonic decreasing; 6 dB/oct slope above corner.
4. THD (1 kHz, 100 mV in, Distortion 10, Filter 0): >25% (hard clip); at Distortion 0, 10 mV in: THD < 1%; at Distortion 5, 100 mV: 5..30% ESTIMATED. Record real unit; replace.
5. Aliasing: 1 kHz sine, distortion 10: alias components below fs/2 <= -60 dBc at 4x OS.
6. Slew: 10 kHz square at max gain -> rise time >= 3 us (0.3 V/us ESTIMATED).
7. Silence: input 0 -> |out| < 1e-9, no DC (|DC| < 1e-5). Impulse: finite, no NaN. Denormal test: 1e-30 input for 10 s no CPU spike >2x.
8. Bypass: null vs input (dry) bit-exact within 1e-7 after xfade; toggle click < -80 dBFS.
9. IMD two tone 800+1200 Hz present; parameter smoothing no zipper (step 0->1: max sample delta bounded).
10. Block sizes 1/4/16/64/512, sample rates 44.1/48/96/192 equal results within 1e-4 relative. Stereo semantics per in_mode.
11. CPU budget (s4); 30 min randomized param/action soak, no xruns; save/restore state round-trips exactly; UI knob reachability (all 3 reach 0 and 10, double-click default).
12. Deterministic with noise off, fixed seed.

## 10 Known inaccuracies / unknowns
- Page body of ElectroSmash and others NOT fetched (network blocked); R/C values recalled from standard RAT schematic and matched against search-result BOM snippets (S2: 30pF, 100pF, 2x1uF?, 2.2uF, 2x4.7uF, 47 ohm x2, 560, 1k x2; 22nF, 1nF mentioned) -> value placement ESTIMATED, verify against a schematic.
- Conflicts: S2 BOM lists 2x47 ohm and 2x1 uF; model uses single 47 ohm ground branch. LM308 vs OD07DP date disputed (S5 vs PedalPCB).
- Op-amp internal model simplified (1-pole GBW + SR + soft rail), no input bias/offset, no noise shape, no supply sag.
- Pot tapers/volume curve ESTIMATED (a=2); real pots vary +-20%.
- JFET follower modelled as linear gain 0.9; real output has tiny soft saturation at max volume.
- Turbo RAT and others omitted; `led` variant diode params ESTIMATED.
- MIDI/expression is EMU only. Real unit has none.
- Looper section N/A (not a looper).
