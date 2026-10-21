# ibanez-ts808 : Overdrive pedal ("inspired by Ibanez TS808 Tube Screamer") - dev-ready spec

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `ibanez-ts808`, **paramPrefix `ibanez_ts808_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/ibanez_ts808.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/ibanez_ts808_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_ibanez_ts808Tests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `ibanez_ts808_on`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims `ibanez_ts808_input_trim_db`, `ibanez_ts808_output_trim_db` -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `ibanez_ts808_hq`, `ibanez_ts808_stereo_mode`, `ibanez_ts808_noise`, `ibanez_ts808_chip`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): (none).
> - Host lanes `ref_s{nn}_p01..` in this order: `ibanez_ts808_drive`, `ibanez_ts808_tone`, `ibanez_ts808_level`, `ibanez_ts808_xfade_ms`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Version 0.1 | 2026-10-21 | model_id `ibanez-ts808` | param prefix `ibanez_ts808_` | status SPEC (not implemented)
Code: `Source/ReferencePedals/Models/ibanez_ts808.h/.cpp`, test `Source/Tests/ReferencePedals_ibanez_ts808Tests.cpp`. No change to `PedalType`/`EffectsChain`.
Tags: DOCUMENTED (vendor listing / analysis excerpt), MEASURED (none; no hardware capture), ESTIMATED (derived from known schematic topology / chosen by us, calibrate).
Research note: direct fetch of electrosmash.com and ibanez.com FAILED in sandbox (DNS/proxy). Values come from web-search excerpts + well-known TS808/TS9 topology. Every ESTIMATED value must be verified against a schematic and a hardware capture before release.

## Sources
- S1 ElectroSmash TS analysis (blocks: input buffer, clipping amp w/ HP+LP in feedback, tone+volume with passive LPF + active tone, output buffer; hump from filtering; designed 1979-80 S. Tamura): https://electrosmash.com/tube-screamer-analysis
- S2 DIYstompboxes SPICE thread (C4/R5 corner 720 Hz; tone moves hump upward): https://diystompboxes.com/smfforum/index.php?msg=617977 and ?msg=618156
- S3 Open-source model (nodal tone via bilinear, WDF clipper, cites S1): https://github.com/JamesStubbsEng/TS-808-Ultra
- S4 Vendor listings (input 500 kohm, output 10 kohm, max gain +30 dB, EIN -100 dBm IHF-A, max out 0 dBm, 9 V battery or adaptor ~8 mA, 125 D x 70 W x 52 H mm, JRC4558D): https://www.fullcompass.com/prod/198328-ibanez-ts808-original-tube-screamer-overdrive-pedal , https://reverb.com/uk/item/3431499-ibanez-ts808-tube-screamer-overdrive-effects-pedal , https://www.effectsdatabase.com/model/ibanez/808/ts808
- S5 Ibanez product page (controls, photo; no photo copied): https://www.ibanez.com/eu/products/detail/ts808_99.html
- S6 existing notes: `spec/reference-pedalboard/models/ibanez-ts808.md`

## 1 Identity / variants
- Real unit: Ibanez TS808 Tube Screamer Overdrive Pro. Mono, analog, 9 V DC (centre-negative) or 9 V battery. No MIDI, no expression jack, no sync, no extra jacks. DOCUMENTED (S4,S5).
- Original 1979-81 (Maxon-built, JRC4558D) = target topology. Reissue TS808 (2004+, same chip + analogue circuit per S4) shares it.
- Variants NOT in v1: TS9 (different clip-stage values/tone, 1982), TS-10, TS808HW/HWV2 (hand-wired, true bypass, extra boost per S6), Mini, 808 clones with mods. Selector `ibanez_ts808_chip` {0=jrc4558d (default),1=tl072_mod,2=opa2134_mod}: PHASE 2, hidden in v1 (changes only op-amp rail/slew/GBW constants, sec 3.3).
- UI name: "Overdrive (TS808 style)". No Ibanez/Maxon logos, no "Tube Screamer" in artwork text; product-info overlay may say "inspired by".

## 2 Controls
| Control | Range | Taper | Default | Real behaviour |
|---|---|---|---|---|
| DRIVE (knob) | 0..10 (UI 0..1) | 500k log/A pot, ESTIMATED; pot position `p = D^2.5` (50% -> ~18%) | 0.5 | clip-stage feedback resistor Rf = 51k + 500k*p. Mid-band gain 1+Rf/4.7k: 11.9x (21.5 dB) at 0 .. 118x (41.4 dB) at 10. Note vendor sheet says "+30 dB max" (S4); use circuit value, mark mismatch (sec 10). Bass below 720 Hz stays near unity -> "tight low end" |
| TONE (knob) | 0..10 | 20k linear/B, ESTIMATED | 0.5 | CCW dark, CW bright. Passive LPF corner sweeps ~723 Hz -> ~3.2 kHz (S1, DOCUMENTED-ish) plus active shelf. Hump moves up with TONE (S2) |
| LEVEL (knob) | 0..10 | 100k audio/A, ESTIMATED | 0.5 | output volume after tone. Unity near 11 o'clock-12 (calibrate T6). Max ~ +12 dB over unity (ESTIMATED) |
| Footswitch | latch | - | OFF | toggles effect. Electronic (JFET/transistor) switch, BUFFERED bypass (S6: "JFET switching"; ESTIMATED, original used buffered switching; true bypass only on HW variant) |
| LED | - | - | off | red, lit when ON, fixed brightness (~3 mA). Hardware shows LED lit even when battery low; not modelled |
| INPUT jack | 1/4" TS | - | - | Zin 500k (S4). Inserting plug connects battery (not modelled) |
| OUTPUT jack | 1/4" TS | - | - | Zout 10k nominal (S4) in effect-on state |
| DC jack | 9 V | - | - | tooltip only; draw 8 mA (S4) shown in board power overlay (optional board PSU budget) |
No other knobs/switches on face. Advanced overlay (not on face): `input_trim_db`, `output_trim_db`, `hq`, `stereo_mode`, `noise`.

## 3 Signal path and circuit model
Order (all component values ESTIMATED from standard TS topology unless tagged):
1. Input buffer: emitter follower (2SC1815-class), Zin 500k, gain ~ -0.1 dB. Input coupling C 0.02 uF with bias ~1 M => HPF ~8 Hz; model 1-pole HPF 8 Hz. Output of buffer drives clip stage via 1 uF / 10k bias (HPF ~16 Hz, model 1-pole 16 Hz).
2. Clipping amp (op-amp half A, non-inverting, JRC4558D), the TS signature:
   - Gain leg Zg = 4.7k + 1/(s*47nF) (series R-C to bias ref). Corner `fc_hp = 1/(2*pi*4.7k*47nF) = 720.5 Hz` (S2, DOCUMENTED). Below it gain -> 1 (unity).
   - Feedback: Rf = 51k + 500k*p in PARALLEL with Cf = 51 pF and with antiparallel diode pair (1N914/1N4148 class).
   - Small-signal H(s) = 1 + Zf/Zg, Zf = Rf || 1/(s*Cf). LP corner `fc_lp = 1/(2*pi*Rf*Cf)`: p=0 -> 61.2 kHz; p=0.177 (D=.5) -> 22 kHz; p=1 -> 5.66 kHz. Combined with the 720 Hz HP it makes the mid-hump.
   - Clip-stage output = vin + vf where vf = voltage across the feedback network (limited by diodes to ~ +-0.5..0.7 V). So output = clean input + soft-clipped gained component; this is why bass stays clean and clipping is "soft" with dynamic cleanup. DOCUMENTED (S1/S3 topology).
   - Diode: Is = 2.52 nA, n = 1.752, Vt = 25.85 mV (1N914/4148 fit, ESTIMATED). Pair current `i_d = 2*Is*sinh(vf/(n*Vt))`.
   - Op-amp supply 9 V, bias 4.5 V; output swing +-3.8 V about bias (JRC4558 not rail-to-rail; ESTIMATED), GBW 3 MHz, slew 1.7 V/us (datasheet class; ESTIMATED). At gain 118 and 1 kHz GBW limit = 3 MHz/118 = 25 kHz: negligible in band; apply as extra 1-pole LP at `GBW/(1+Rf/4.7k)` at OS rate (S3-style).
3. Tone + volume section (op-amp half B):
   - Passive 1-pole LPF R 1k / C 0.22 uF (fc 723 Hz) with tone-dependent R from pot, plus active stage. TOPOLOGY NOT VERIFIED -> v1 uses behavioural model fitted to S1/S2: `fc_tone(T) = 723 * (3200/723)^T` Hz (log sweep, ESTIMATED), 1-pole LP, followed by a high shelf on the active stage: shelf corner 1.6 kHz, gain `gS(T) = -6 + 12*T` dB (ESTIMATED) => net 10 kHz level re 1 kHz: T=0 about -14 dB, T=0.5 about -7 dB, T=1 about -1 dB. Final topology (nodal, trapezoidal/bilinear, as S3) replaces this once schematic confirmed; keep the behavioural filter as fallback `tone_mode` internal.
   - Mid hump: peak ~720 Hz (T=0) rising to ~1.0-1.5 kHz at T=1 (S2 qualitative), peak boost vs 100 Hz and vs 5 kHz about +10..+14 dB at DRIVE=1 (ESTIMATED).
   - Level: 100k audio pot; `g_level = 4.0 * L^2` (x^2 ~ audio taper; 0 -> -inf, 0.5 -> 1.0 (0 dB), 1 -> 4.0 (+12 dB)), ESTIMATED.
4. Output buffer: emitter follower, Zout 10k nominal (S4), HPF 5 Hz. Output-load interaction ignored unless board 'impedance' flag on (then 1-pole LP from 10k * cable C 500 pF if flagged).
- Calibration: analog volts = DI sample * 0.5 V/FS (guitar peak ~0.25 V at -6 dBFS; matches boss-ds1 convention) scaled by `input_trim_db`. Max output 0 dBm (S4) = 0.775 V rms ~ 1.1 V pk: output hard limit `tanh` soft at +-3.0 V in volts.
- Latency: 0 samples algorithmic; OS filters latency-compensated or linear->IIR min-phase with latency reported 0 (use IIR half-band, group delay < 1 sample at base rate, ignored).
- Bypass: BUFFERED. Input buffer + output buffer stay in path (HPF 8 Hz, LPF >30 kHz, transparent). Transition: equal-power crossfade `ibanez_ts808_xfade_ms` default 5 ms; hardware JFET switch has slight audible pop - optional `thump` off.
- Noise: EIN -100 dBm IHF-A (S4) -> hiss shaped, ESTIMATED wideband -90 dBFS when DRIVE>0.5, default ON in product, OFF for tests.

## 4 Plugin algorithm
```
x -> *trim -> HPF8 -> HPF16 -> [OS up] -> clipAmp(HP720 leg, Rf||Cf||diode, Newton) -> + vin -> railclip(+-3.8V) -> GBW LP
  -> [OS down] -> toneLP(fc(T)) -> shelf -> *g_level -> softlimit -> HPF5 -> y
```
- Clip stage per OS sample (trapezoidal, T=1/fs_os): leg state: Zg R=4.7k, C=47nF, cap voltage vc. `i_g = (vin - vc)/4.7k` (vin = op-amp input = inverting node); update vc via trapezoid.
  Feedback node eq. (vf = vout - vin): `g(vf) = vf*(1/Rf + 2*Cf/T) + 2*Is*sinh(vf/(n*Vt)) - (i_g + hist) = 0`, hist = cap history current (`2*Cf/T*vf_prev + icf_prev` sign per trapezoid). Newton max 4 iterations, warm start vf_prev, clamp step to 0.1 V, `sinh` via fast exp-based approx (error < 1e-4 rel); fallback closed-form Lambert-W (Parker/D'Angelo) when |vf|>0.35 or non-convergent. vout = vin + vf.
  Coefficients (Rf, 1/Rf term) recomputed every 16 samples from smoothed D.
- Oversampling: 4x polyphase IIR half-band cascade (>100 dB stop) when DRIVE > 0.2, 2x otherwise (10 ms crossfade); `hq` off = 2x always; sr >= 96 kHz default 2x; offline render 8x. Tone/level/buffers at base rate.
- Aliasing: 1-pole post-clip LP 20 kHz at OS rate (mirrors Cf pole, already explicit at high DRIVE) before decimation. Target alias < -60 dBc (4x), < -48 dBc (2x) for 1 kHz and 5 kHz sines at DRIVE=1.
- Smoothing: one-pole `a = 1-exp(-1/(tau*fs))`. DRIVE/TONE tau 20 ms (coefs updated /16 samples), LEVEL 15 ms per-sample gain ramp.
- Denormals: `juce::ScopedNoDenormals` + add 1e-18 DC before IIR feedback + flush `|s|<1e-15`.
- NaN guard: if `!isfinite(y)` reset all states, output 0 for that block.
- RT safety: no alloc/lock/IO in process(); all buffers in prepare(); params via `std::atomic<float>`; bypass `atomic<bool>` read once per block; Newton bounded.
- CPU budget: <= 0.40 % of one 3 GHz core per stereo instance at 48 kHz 4x OS (~140 flop/OS sample); 10 instances < 4 % in CI benchmark. Block 1..8192, 44.1-192 kHz.

## 5 I/O and routing
- 1 in / 1 out mono. Stereo host: `ibanez_ts808_stereo_mode` {0 dual-mono (default, independent states), 1 sum-to-mono then dup}.
- No extra jacks, send/return or control jack on hardware.
- Board tricks (via BOARD_CORE.md, not pedal-specific): series before amp (classic mid-boost into gain: DRIVE 0-3, LEVEL high, "clean boost with 720 Hz tightening"); stack two TS (board 2 slots, second as boost); before/after fuzz or distortion; parallel/blend through board splitter + board dry/wet (keeps bass clean); in board FX-loop with feedback path (loop needs board safety limiter + 1-block delay) for self-oscillating mid-hump feedback; placement in amp loop (input level differs +-12 dB, use `input_trim_db`); DI-vs-amp placement label required.
- Impedance interaction: Zin 500k; with board 'impedance matching' flag after a fuzz-type pedal apply load model; otherwise ignore.
- Universal wet/dry: board-level (not on face).

## 6 MIDI / sync / expression
Real unit: none. Emulation through Luthier mapping layer (`specs/MIDI_EXPRESSION.md`), labelled "Virtual MIDI (not on hardware)":
- Default CC map (all learnable, omni): CC16 DRIVE, CC17 TONE, CC18 LEVEL, CC19 ON (>=64 on). 7-bit -> 0..1; optional 14-bit CC+32. Program Change = board preset, not pedal.
- Footswitch gesture: note-on toggles `ibanez_ts808_on` (latch) or on-while-held (momentary, selectable). Velocity -> bypass crossfade 2 ms (vel 127) .. 25 ms (vel 1). Poly/channel aftertouch -> DRIVE offset 0..+0.5 (default off, 30 ms smoothing); hard stomp (vel>110) optional pop (thump) if enabled.
- Expression: any CC / pitchbend / aftertouch / slider / board knob -> DRIVE, TONE or LEVEL with min/max + curve (lin/log). Default expression target LEVEL (like a volume/boost pedal into the TS).
- Tap/clock: N/A (no time effect); clock ignored.
- All params automatable by host.

## 7 Visuals (original vector art; no logos or trade dress copy)
Enclosure: die-cast stomp box, face 70 W x 125 D mm (S4: 125 D x 70 W x 52 H), corner radius 4 mm, matte green (#2F8F3C approx, ESTIMATED), black skirted knobs 14 mm dia with white pointer line, white silkscreen. Face origin top-left, mm:
- Knobs, top row L->R: DRIVE (12, 18), TONE (35, 18), LEVEL (58, 18); labels 5 mm above in caps "DRIVE" "TONE" "LEVEL"; tick marks 0..10 on 270 deg sweep (min 7:30, max 4:30).
- Name plate centred y=40: generic "OVERDRIVE" 8 mm tall, small "808 STYLE" line (no logo).
- Red LED dia 4 mm at (35, 52) with glow when ON.
- Footswitch: hinged plate 56 x 38 mm at y=72..110, stomp travel 3 mm, 80 ms ease-out; chrome-ish gradient.
- Jacks on side faces in board view: INPUT left, OUTPUT right, DC 9 V rear.
- Mounting footprint 70 x 125 mm for UI_BOARD_STYLES.md (velcro, screws both sides, clamps, routed pocket, loose).
- Vector notes: bevel gradient, 2 base screws hidden, battery door not drawn.

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | automatable |
|---|---|---|---|---|---|---|---|---|
| ibanez_ts808_drive | Drive | 0 | 1 | 0.5 | norm | pot A500k (p=D^2.5) | 20 | yes |
| ibanez_ts808_tone | Tone | 0 | 1 | 0.5 | norm | linear | 20 | yes |
| ibanez_ts808_level | Level | 0 | 1 | 0.5 | norm | audio x^2 | 15 | yes |
| ibanez_ts808_on | Footswitch on | 0 | 1 | 0 | bool | step | 5 (xfade) | yes |
| ibanez_ts808_hq | HQ oversample | 0 | 1 | 1 | bool | step | - | no |
| ibanez_ts808_stereo_mode | Stereo mode | 0 | 1 | 0 | enum | step | - | no |
| ibanez_ts808_input_trim_db | Input trim (adv) | -24 | 24 | 0 | dB | linear | 20 | yes |
| ibanez_ts808_output_trim_db | Output trim (adv) | -24 | 24 | 0 | dB | linear | 20 | yes |
| ibanez_ts808_xfade_ms | Bypass crossfade (adv) | 1 | 30 | 5 | ms | linear | - | no |
| ibanez_ts808_noise | Noise on | 0 | 1 | 1 | bool | step | - | no |
| ibanez_ts808_chip | Op-amp variant (phase 2, hidden) | 0 | 2 | 0 | enum | step | - | no |

## 9 Tests / acceptance (provisional until hardware captures)
- T1 impulse + silence: no DC (|mean| < 1e-4 FS); exact 0 on digital silence with noise off.
- T2 small-signal FRF (-40 dBFS sweep, DRIVE=0.5): HP corner of gain leg 720 Hz +-10 %; unity gain +-1 dB below 100 Hz relative input; peak 0.6-1.5 kHz; DRIVE=0 mid gain 21.5 dB +-1.5 over unity region (clip stage only); DRIVE=1 41.4 dB +-2 (clip stage only, below diode knee).
- T3 TONE sweep: LP -3 dB corner T=0 within 723 Hz +-20 %, T=1 within 3.2 kHz +-25 %; 10 kHz vs 1 kHz level T=0/0.5/1 = -14/-7/-1 dB +-3 dB (ESTIMATED).
- T4 clipping: 1 kHz, -20 dBFS, DRIVE=1: THD(2..10) 12-35 %; DRIVE=0.2: 0.5-5 %; H3 > H2 by >= 6 dB (symmetric diodes). Bass-clean test: 80 Hz + 2 kHz two-tone, DRIVE=1: 80 Hz component amplitude change < 2 dB vs DRIVE=0 (low end stays unity).
- T5 aliasing: 5 kHz sine DRIVE=1 at 48 kHz: < -60 dBc (4x), < -48 dBc (2x).
- T6 level calibration: 1 kHz -20 dBFS, D=T=L=0.5 -> output within +-1.5 dB of unity target (set after capture).
- T7 bypass/null: bypassed output null vs input < -80 dB above 100 Hz; on/off crossfade max |dy| < 0.05 FS.
- T8 cross-rate: FRF at 44.1/48/96/192 within 0.5 dB below 10 kHz.
- T9 block sizes 1/4/16/64/512 bit-exact, deterministic with noise off.
- T10 CPU < 0.40 % core/stereo instance; 30 min randomised param/MIDI soak, no NaN, no xruns.
- T11 state round-trip of all params; old projects without block unchanged bitwise.
- T12 stability: DRIVE sweep 0->1 in 10 ms at full-scale input, no NaN/inf, |y| < 6 V equivalent.
- Reference: null/spectral comparison vs recorded same-revision TS808 at 5 knob positions x 3 input levels; target (provisional): spectral envelope within 3 dB 100 Hz-8 kHz, THD within 20 % relative.

## 10 Known inaccuracies
- Tone-section topology is behavioural (fit to S1/S2 qualitative data), not nodal; component values (1k, 220 nF, 20k, shelf) ESTIMATED.
- Clip-stage R/C values (4.7k, 47 nF, 51k, 500k, 51 pF) from standard TS topology; fc 720 Hz DOCUMENTED (S2), others unverified in this session.
- Vendor "+30 dB max gain" (S4) conflicts with circuit gain 41.4 dB max; likely marketing/measurement-convention. Capture needed.
- Pot tapers, diode Is/n, op-amp rails/slew, output limit, noise floor ESTIMATED. JRC4558 vs reissue chip tolerance and diode matching not modelled.
- Bypass switch pop, battery sag, supply ripple, enclosure microphonics, temperature not modelled.
- Hardware-wired bypass switch type is not confirmed here; HW variants are true bypass and are out of scope.
- Not "authentic"/"identical"; label "inspired by". No copyrighted manual text or photos copied.
