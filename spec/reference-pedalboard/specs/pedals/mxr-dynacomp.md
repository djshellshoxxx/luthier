# mxr-dynacomp — MXR Dyna Comp (inspired by) — dev spec

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `mxr-dynacomp`, **paramPrefix `mxr_dynacomp_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/mxr_dynacomp.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/mxr_dynacomp_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_mxr_dynacompTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `mxr_dynacomp_bypass`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims (none) -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `mxr_dynacomp_variant`, `mxr_dynacomp_noise`, `mxr_dynacomp_quality`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): `mxr_dynacomp_expr_target`.
> - Host lanes `ref_s{nn}_p01..` in this order: `mxr_dynacomp_sensitivity`, `mxr_dynacomp_output`, `mxr_dynacomp_attack`, `mxr_dynacomp_mix`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Tags: DOCUMENTED = seen in a retrieved source. ESTIMATED = from recalled circuit knowledge, not re-verified. MEASURED = none yet.
Research note: web-fetch of electrosmash.com / jimdunlop.com FAILED (DNS ENOTFOUND in sandbox). Only WebSearch snippets retrieved. Every circuit number below is ESTIMATED until checked against ElectroSmash page. Task 1 for implementer: re-fetch and fix the values marked ESTIMATED.
Sources:
- https://electrosmash.com/mxr-dyna-comp-analysis (DOCUMENTED structure only: input buffer, CA3080 OTA stage, envelope detector, output stage, power)
- https://www.jimdunlop.com/content/manuals/M291.pdf (M291 Mini manual; not retrieved)
- https://www.zzounds.com/item--DAVM102 (M102: ~4.25x2.25x1.25 in, 0.9 lb, red, 9V batt or ECB03; DOCUMENTED)
- https://www.CottonwoodMusicEmporium.com/collections/compression-pedals/products/mxr-dyna-comp-compressor-mini-m291 (Mini: attack switch slow/fast, CA3080; Z in 800k, Z out 15k; DOCUMENTED via snippet)
- https://www.rockboard.de/en/pedalPedia/MXR/CSP028-1976-Handwired-Dynacomp/68982597/ (Script reissue Z in 470k, Z out 10k; DOCUMENTED via snippet)
- https://groupdiy.com/threads/dynacomp-improvements-any-suggestions.34181/ (CA3080 obsolete; NJM13600/13700 substitutes; DOCUMENTED)
Legal: "inspired by" name only. Use original vector art; no Dunlop/MXR logos or script logo; label as "DYNA COMP-style" in UI with trademark disclaimer.

## 1 Identity / variants
| id | variant | notes |
|---|---|---|
| mxr-dynacomp | default = M102 (Script-era look, red) | 2 knobs, no switch, CA3080 |
| mxr-dynacomp-mini | M291 Mini style | adds ATTACK switch FAST/SLOW; Zin 800k, Zout 15k |
| mxr-dynacomp-script76 | 1976 Script reissue style | Zin 470k, Zout 10k, orange |
Implement one DSP class, variant selects constants (Section 3 table V). Default variant = M102.
Category: compressor. Mono in / mono out. 9V DC (battery or adapter), ~5-6 mA ESTIMATED.

## 2 Controls
| control | range | taper | default | real behaviour |
|---|---|---|---|---|
| OUTPUT | 0..1 (pot 0-10 scale) | audio (log, A100k-ish) ESTIMATED | 0.5 | post-OTA make-up gain, about -inf..+~20 dB ESTIMATED; at 12 o'clock approx unity-ish with moderate sensitivity |
| SENSITIVITY | 0..1 | audio ESTIMATED | 0.5 | sets how much of OTA output feeds detector => lower threshold + more ratio + more make-up. Not a ratio knob, no threshold knob. Max = heavy squash, noisy |
| ATTACK sw (Mini only) | SLOW / FAST | 2-pos | FAST | changes detector attack RC; SLOW lets transient through |
| STOMP footswitch | on/off | latching | off | true bypass (M102 reissue DOCUMENTED); LED red ESTIMATED when active |
| IN jack | 1/4" TS | | | instrument in |
| OUT jack | 1/4" TS | | | |
| DC jack | 2.1mm centre-neg 9V | | | UI-only (power indicator, optional "battery sag" toggle) |
Real unit has NO gain-reduction meter, NO threshold, NO ratio, NO attack/release pots (non-Mini).

## 3 Signal path and circuit/DSP model
Stage-by-stage (ESTIMATED topology, matches ElectroSmash block names):
1. Input buffer: JFET/Transistor follower, Zin per variant V, HPF ~ 1/(2π·Zin·Cin); assume Cin 47nF-ish => fc ≈ 4-7 Hz (ESTIMATED). Model: 1-pole HP 5 Hz.
2. OTA gain cell (CA3080): iout = Iabc·gm·... ; gm = Iabc/(2·VT), VT=26mV. Input attenuated by resistive divider to keep Vin <~ 20mV for linearity; output current into load/op-amp virtual ground. Large inputs saturate via tanh(Vd/2VT). Model: y = Gctl·Vin_div·... implemented as `tanh(x/ (2VT_scaled))` soft nonlinearity with x pre-scaled so that guitar-level (-20 dBFS, 0.1 V peak-ish) is slightly nonlinear.
3. Envelope detector: output of OTA gain stage -> amplifier (sensitivity pot sets gain into detector) -> full-wave/ half-wave diode rectifier (2 diodes, Si 1N4148 Vf≈0.6 V) -> RC smoothing -> converts to Iabc via bias network. Sensitivity pot scales detector input amplitude: higher = detector triggers on smaller signals. Feedback compressor topology (control derived from gain cell output).
4. Output stage: op-amp/ transistor, OUTPUT pot, Zout per variant. Optional LPF at ~12-20 kHz ESTIMATED.
Time constants (ESTIMATED; typical Dyna Comp behaviour reported fast attack, ~ms; moderate release ~100-300 ms; program-dependent):
- attack tau: 1.5 ms (FAST/M102 default); 10 ms (SLOW, Mini) ESTIMATED
- release tau: 150 ms at mid sens, shorter at high input level (nonlinear RC through diode) 60..300 ms ESTIMATED
Gain reduction range: 0..~30 dB ESTIMATED; max make-up ~+20 dB at max OUTPUT.
Latency: 0 samples (analog); oversampled path adds FIR latency, report via setLatencySamples() = 0 using IIR/polyphase minimum-phase option, else compensated.
Bypass: true bypass (relay/ mechanical switch). Model: dry passthrough, 5 ms equal-power crossfade (hardware has click-free relay-ish ~0; mechanical switch has tiny pop). Optional "buffered" toggle off.
Noise: CA3080 hiss; at high gain audible. Noise floor ≈ -75 dBFS ref 0 dBu at SENS=max, ≈ -90 at min ESTIMATED; deterministic seeded PRNG when `noise_enabled`.

## 4 Algorithm for plugin
Block diagram:
`in -> HP5Hz -> inGain(variant Zin load) -> [OTA: g(env) * tanh] -> out_pre -> *OUTPUT -> outLP -> mix(bypass xfade) -> out`
`env path: out_pre -> *sensGain -> |.| (diode: max(|x|-Vf,0)) -> asym one-pole (attack/release) -> env -> map to g`
Per-sample (fs internal = host rate × OS):
```
x1 = hp5.process(in*inTrim)
g  = 1 / (1 + kc * env)            // gain law, kc≈ 20..60 ESTIMATED; smoothed
v  = x1 * g
ota= tanh(v * drive) / drive       // drive≈1.5 ESTIMATED; asymm. bias 2% for even harmonics
rect = max(|ota * sensGain| - Vf, 0)
a = rect > env ? aAtk : aRel      // aAtk = 1-exp(-1/(tauAtk*fs)); aRel dependent: tauRel = tauRel0 / (1 + 4*rect)
env += a*(rect - env)
out = ota * outGain
sensGain = 1 + 99*sens^2 ESTIMATED(0.. ~40 dB)
outGain  = dB2lin(-40 + 60*output^0.5)... choose: outDb = lerp(-60,+20,output) w/ log taper; output=0 -> -inf (mute)
```
- Oversampling: 2x (default) / 4x (HQ) polyphase IIR (juce::dsp::Oversampling) around OTA tanh + detector; non-linear section only.
- Aliasing: tanh drive modest => 2x enough; check alias at 5 kHz sine +0 dBFS < -60 dBc.
- Smoothing: sensGain, outGain, bypass mix via juce::SmoothedValue 20 ms (linear for mix, multiplicative for gains).
- Denormals: juce::ScopedNoDenormals + add 1e-18 DC-free flush in env/hp states (`if(fabs(s)<1e-20) s=0`).
- CPU budget: <=0.5% single core @48k/128 block 2x OS on 3 GHz; <=40 flops/sample base.
- RT safety: no alloc/locks in process; parameters via std::atomic<float> read once per block; preallocate OS in prepare().
- Stability: env clamped [0, 1e3]; g in (0,1]; NaN guard -> reset states.

## 5 I/O and routing
Real: 1 in, 1 out, no loop/aux. Board integration:
- mono; stereo host input summed to mono before pedal unless board `stereo_dual_mono` => two instances with linked params.
- Tricks users can do: parallel (wet/dry blend via board wet/dry; true parallel compression), series before drive (sustain into fuzz/dist), after drive (squash), board effects-loop feedback: send Dyna Comp out to a delay feedback path (compressed repeats). No native feedback jacks.
- Insert slot before/after any pedal; order = chain order, Dyna Comp is order-sensitive (detector level-dependent; place before/after drives changes behaviour).
- Variant Zin: model load interaction with preceding pedal Zout (if board models impedance) via 1-pole LP with Zout·Cpar; default off.

## 6 MIDI / sync / expression (emulation; real unit has none)
Real unit: NO MIDI, NO expression jack, NO tempo. Emulation is board-level and optional:
| source | target | map |
|---|---|---|
| CC 11 (expr) default / learn any CC | SENSITIVITY or OUTPUT (user chooses `expr_target`) | 0..127 -> 0..1 linear in knob domain, 10 ms slew |
| CC 7 | OUTPUT | |
| CC 80 | stomp on/off (>=64 on) | |
| Program Change | board preset recall | |
| Note velocity (footswitch gesture) | stomp press: velocity -> press speed; vel<30 = slow soft press => bypass xfade 20 ms; vel>100 = hard press => xfade 2 ms (+tiny click if `mech_click`) | |
| Channel aftertouch / poly AT | SENSITIVITY offset 0..+0.3 while held (foot pressure emulation) | smoothed 30 ms |
| Mod wheel CC1 | SENSITIVITY | optional |
| MIDI learn | any knob via CC/slider/knob | |
| MIDI clock / tap tempo | not applicable (no time-based param); ignore | |
| ATTACK sw (Mini) | CC 81 (<64 FAST) | |
All mappings stored in board state, applied off audio thread -> atomics; MIDI parsed in processBlock without alloc.

## 7 Visuals
M102-style: enclosure red (#C0262D-ish original art), 4.25x2.25x1.25 in => face 108 x 57 mm (DOCUMENTED dims). Layout (origin top-left of face):
- two knobs on top row: SENSITIVITY centre (28, 17) mm, OUTPUT centre (80, 17) mm, knob dia 14 mm, white pointer line, 270 deg sweep, tick marks 0..10.
- LED: dia 3 mm at (54, 9) mm; red glow when active.
- Footswitch: dia 14 mm chrome at (54, 40) mm; stomp animation 1 mm travel.
- Labels: "DYNA COMP" (use generic custom font, not logo), "OUTPUT", "SENSITIVITY", scale numerals 0..10. Original-art: stylized script lettering is trademark -> use plain italic sans.
- Jacks on sides (hidden in face view): IN left, OUT right; DC top.
Mini variant: 2 knobs + mini switch with FAST/SLOW label, enclosure 3.9x1.9 in ESTIMATED.
Script76: orange body, black script-like text.

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | automatable |
|---|---|---|---|---|---|---|---|---|
| mxr_dynacomp_sensitivity | Sensitivity | 0 | 1 | 0.5 | norm | 0.5 (log-ish) | 20 | yes |
| mxr_dynacomp_output | Output | 0 | 1 | 0.5 | norm | 0.5 | 20 | yes |
| mxr_dynacomp_attack | Attack (Mini) | 0 | 1 | 0 | enum FAST/SLOW | step | 10 xfade | yes |
| mxr_dynacomp_bypass | Bypass | 0 | 1 | 1 | bool | step | 5 | yes |
| mxr_dynacomp_variant | Variant | 0 | 2 | 0 | enum | step | - | no |
| mxr_dynacomp_noise | Noise | 0 | 1 | 1 | bool | step | - | no |
| mxr_dynacomp_quality | Oversampling | 0 | 2 | 0 | enum 2x/4x/off | step | - | no |
| mxr_dynacomp_expr_target | Expr target | 0 | 2 | 0 | enum | step | - | no |
| mxr_dynacomp_mix | Pedal wet/dry (board) | 0 | 1 | 1 | norm | linear | 20 | yes |

## 9 Tests / acceptance (provisional; no hardware reference captured)
1. Null: bypass=true => output == input bit-exact after crossfade settles.
2. Silence in => |out| < 1e-6 (noise off); no DC (|mean|<1e-5).
3. Static curve: 1 kHz sine, levels -50..0 dBFS, at SENS 0.25/0.5/0.75/1.0; expect monotone output, slope <1 above knee; GR 0-30 dB. Tolerance +-3 dB vs ElectroSmash simulation until a real capture exists.
4. Burst: -30 dBFS->-10 dBFS step; attack time to 63% of GR: FAST 1.5 ms (+-50%), SLOW 10 ms; release 63% ~150 ms (+-50%) ESTIMATED.
5. FRF at -30 dBFS, SENS 0.5: -3 dB at ~5 Hz and ~15-20 kHz, flat +-1.5 dB 100 Hz..8 kHz.
6. THD @1 kHz -20 dBFS: 0.1-1% at SENS 0.5; up to 3-5% max sens ESTIMATED.
7. Two-tone IMD, 60 Hz+7 kHz 4:1: IMD < 3% at SENS 0.5.
8. Block sizes 1/4/16/64/512 and rates 44.1/48/96/192 produce equal results within 0.1 dB.
9. Alias: 5 kHz sine 0 dBFS, aliases < -60 dBc at 2x.
10. 30 min randomized knob/bypass soak: no NaN/inf, no xrun, CPU <0.5%.
11. Save/restore state round trip exact. MIDI: CC sweep gives zipper-free output (<-70 dB step energy).
12. Bypass toggle click: peak step < 0.01 FS with 5 ms xfade.

## 10 Known inaccuracies
- All time constants, gain law, drive, pot values ESTIMATED; sources not fully fetched (DNS failure).
- No hardware capture; CA3080 unit-to-unit variance and obsolete-part substitutes (NJM13600) not modelled.
- Battery sag, 9V rail dependence not modelled (optional later).
- Reissue variant circuits differ (Zin/Zout DOCUMENTED, internals not).
- MIDI/expression/velocity features are board emulation, not present in hardware.
- Previous note: M291 Mini image in earlier doc is not identical to classic M102.
