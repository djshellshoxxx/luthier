# ehx-deluxe-muff : Fuzz/sustainer + gate + parametric mids ("inspired by Electro-Harmonix Deluxe Big Muff Pi") - dev-ready spec

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `ehx-deluxe-muff`, **paramPrefix `ehx_deluxe_muff_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/ehx_deluxe_muff.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/ehx_deluxe_muff_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_ehx_deluxe_muffTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `ehx_deluxe_muff_bypass`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims `ehx_deluxe_muff_input_trim_db`, `ehx_deluxe_muff_output_trim_db` -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `ehx_deluxe_muff_hq`, `ehx_deluxe_muff_true_bypass`, `ehx_deluxe_muff_stereo_mode`, `ehx_deluxe_muff_revision`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): `ehx_deluxe_muff_exp`, `ehx_deluxe_muff_exp_connected`.
> - Host lanes `ref_s{nn}_p01..` in this order: `ehx_deluxe_muff_volume`, `ehx_deluxe_muff_tone`, `ehx_deluxe_muff_sustain`, `ehx_deluxe_muff_attack`, `ehx_deluxe_muff_gate`, `ehx_deluxe_muff_mids_on`, `ehx_deluxe_muff_mids_level`, `ehx_deluxe_muff_mids_freq`, `ehx_deluxe_muff_mids_q`, `ehx_deluxe_muff_bass_boost`, `ehx_deluxe_muff_gate_link`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Version 0.1 | 2026-10-21 | model_id `ehx-deluxe-muff` | param prefix `ehx_deluxe_muff_` | status SPEC
Tags: DOCUMENTED (manual/retail/official text), MEASURED (none yet), ESTIMATED (derived/from memory; must calibrate).
RESEARCH LIMITS (honest, updated 2026-10-22): sandbox proxy still blocks page fetch (ENOTFOUND www.ehx.com, archive.ehx.com, manuals.plus, electrosmash.com, bigmuffpage.com, images.static-thomann.de; web.archive.org refused). Only WebSearch excerpts (EHX manual/product-page text, retailer spec lists) reached; manual/schematic not read in full. Remaining ESTIMATED values keep their derivation. No claim of "authentic/identical" until capture + tolerance report (see 9). See sec 11 Verification log.

## Sources
- S1 EHX product page: https://www.ehx.com/products/deluxe-big-muff-pi/ (archive manual: https://archive.ehx.com/products/deluxe-big-muff-pi/instructions)
- S2 manual copies: https://manuals.plus/electro-harmonix/deluxe-big-muff-pi-manual ; https://usermanual.wiki/Document/deluxebigmuffpi.2015209889/html
- S3 retailer listing (feature set): Thomann https://www.thomannmusic.com/electro_harmonix_deluxe_big_muff_pi.htm
- S4 ElectroSmash Big Muff analysis (4 cascaded CE stages + passive tone; input booster / clipping / tone / output): https://electrosmash.com/big-muff-pi-analysis
- S5 Tone-stack tutorial (HP C1/R2 || LP C2/R1 blend, notch vanishes if corners equal): PedalPCB forum "Tone Stacks - Part 1a" https://forum.pedalpcb.com/threads/tone-stacks-part-1a-passive-filters.8245/latest
- S6 Premier Guitar Russian Big Muff mods (D1/D2 + C6 clipping): https://www.premierguitar.com/gear/electro-harmonix-russian-big-muff-pi-pedal-mods
- S7 effectsdatabase: https://www.effectsdatabase.com/model/eh/xo/bigmuffpi/2/deluxe
- Related spec: spec/reference-pedalboard/models/ehx-deluxe-muff.md, models/ehx-big-muff-nyc.md

## 1 Identity / variants
- Real unit: EHX Deluxe Big Muff Pi, mono in/out, analog, 9 V battery (included per Adorama listing) or 9.6VDC-200mA adapter (optional, not included), centre-negative, max 12 V DC, draw 22 mA @ 9 V; true bypass. DOCUMENTED (retailer spec lists: https://www.adorama.com/ehdlxbm.html ; https://fr.audiofanzine.com/fuzz-guitare/electro-harmonix/deluxe-big-muff-pi/ ; not the EHX manual), 2 footswitches. No MIDI, no tap, no clock. Only external control = EXP jack. DOCUMENTED (S1,S3).
- Core: "NORMAL" mode circuit identical to NYC Big Muff Pi (4 CE transistor stages, 2 diode-clipping stages, passive tone crossfade). DOCUMENTED (S1 manual text).
- Added: ATTACK path, GATE, BASS BOOST switch, footswitchable parametric MIDS (LEVEL, FREQ, Q hi/lo), EXP pedal sweeps MIDS FREQ. DOCUMENTED.
- NOT this pedal: Deluxe Big Muff Pi 2 (2026: Wicker switch, BLEND, dual op-amp), Deluxe Bass Big Muff, Sovtek Deluxe (variant-specific exp jack text). Do not mix specs. Original has NO blend knob (control list in EHX manual excerpts has none; DOCUMENTED by absence). Sovtek-branded version has an extra internal true/buffered bypass slide switch and trim pots (https://modulargrid.net/p/offers/view/1170676); not modelled.
- Variant enum `ehx_deluxe_muff_revision`: {0=default}. Single variant v1; selector hidden. The earlier "dlx_2014" year label was unverified: renamed generic `deluxe_guitar`; no production year is claimed.
- Display name: "Deluxe Muff (Big Muff style)". No logos; original vector art.

## 2 Controls
| Control | Range | Taper | Default | Real behaviour |
|---|---|---|---|---|
| VOLUME | 0..10 | audio A (x^2) ESTIMATED | 0.5 (unity ~ 0.45, ESTIMATED) | output level after tone/mids; range -inf..+~20 dB over unity |
| TONE | 0..10 | linear B 100k (ESTIMATED) | 0.5 | passive LP/HP crossfade; CCW dark (LP), CW bright (HP), centre = mid notch ~ 1 kHz. DOCUMENTED topology (S4,S5) |
| SUSTAIN | 0..10 | audio A 100k ESTIMATED | 0.5 | input-stage/pre-clip gain; CW = more fuzz/compression. DOCUMENTED function |
| ATTACK | 0..10 | linear ESTIMATED | 0 | blends an enhanced version of the dry signal to add pick punch; 0 = off. DOCUMENTED (EHX manual excerpt). Circuit unpublished -> behavioural model sec 3.6, ESTIMATED |
| GATE | 0..10 | linear ESTIMATED | 0 | CW raises threshold (input must be stronger to open); full CCW = gate disabled. DOCUMENTED (EHX manual; Thomann copy https://images.static-thomann.de/pics/atg/atgdata/document/manual/454336_en.pdf is the Pi 2 manual, wording matches). Threshold dB values ESTIMATED sec 3.7. |
| MIDS LEVEL | -10..+10 dB (UI 0..1, centre 0.5=0 dB) | linear dB | 0.5 | parametric boost/cut up to +/-10 dB. DOCUMENTED (S3,S1) |
| MIDS FREQ | 310 Hz..5.0 kHz | log (exp sweep) ESTIMATED taper | 0.5 (~1.25 kHz geometric centre) | centre freq; also driven by EXP jack. Range DOCUMENTED (EHX manual text: "approximately 310Hz to 5.0kHz"; https://www.ehx.com/products/deluxe-big-muff-pi/) |
| Q switch | HI / LO | 2-pos | LO | LO = broader/rounder, HI = sharper/peakier (DOCUMENTED, EHX manual). Numeric Q ESTIMATED: LO 0.7, HI 2.5 (no published values; HI/LO ratio chosen >=3 for audible contrast) |
| BASS BOOST switch | NORMAL / BASS BOOST | 2-pos | NORMAL | NORMAL = NYC circuit; BASS BOOST raises bass through the distortion circuit; most useful w/ TONE > noon. DOCUMENTED |
| BYPASS footswitch | latching | - | OFF | engages whole effect |
| MIDS footswitch | latching | - | OFF | engages mids EQ (independent of bypass; mids only audible when pedal ON -- ESTIMATED; real unit may let mids-only? Assume NO). |
| LEDs | 2 | - | off | one per footswitch, red (ESTIMATED), fixed brightness |
| INPUT | 1/4" mono | - | - | Zin 130 kOhm DOCUMENTED (retailer spec: Adorama, Audiofanzine; not EHX manual) |
| OUTPUT | 1/4" mono | - | - | Zout 300 Ohm DOCUMENTED (same retailer sources) |
| EXP | 1/4" TRS | - | - | expression pedal sweeps MIDS FREQ (heel = low end, toe = high end ESTIMATED). DOCUMENTED existence/target (search excerpts). |
| DC | barrel 9 V centre-neg | - | - | tooltip only |
No hidden knobs. Input/output trims are slot-level (`ref_s{nn}_in_db/out_db`), not pedal params (per reconciled header; stale rows removed from sec 8).

## 3 Signal path and circuit/DSP model
Order: In -> inbuf -> [gate sidechain tap] -> input booster (Q1) -> clip1 (Q2+diodes) -> clip2 (Q3+diodes) -> tone stack -> [BASS BOOST network] -> output booster (Q4) -> mids EQ (if MIDS on) -> gate VCA -> + attack path -> VOLUME -> Out. Placement of mids/gate/attack in real unit is NOT documented: ESTIMATED order above (mids after output booster as clean EQ; gate before volume).
Calibration: 0.5 V analog peak per full-scale (trim), rails 9 V, bias 4.5 V.

3.1 Input booster (S4) ESTIMATED: HPF 1-pole ~ 30 Hz in; CE stage gain Av ~ +20 dB at SUSTAIN max... NYC values (ESTIMATED): sustain pot sets stage-1 gain ~ 6..32 dB. Clip y = 4.5*tanh((x + 0.06 x^2)/4.5) (asym k=0.06).
3.2 Clip stages (x2) ESTIMATED: each = CE gain stage ~ +30 dB mid-band (HF rolled by feedback C ~ 470 pF || 100k => fc ~ 3.4 kHz), then antiparallel silicon diode pair (1N914/1N4148: Is=2.52 nA, n=1.752, Vt=25.85 mV) in feedback path -> soft clip ~ +/-0.6 V. Model per stage: `vout = Av*LPF(x)`, then diode node in feedback: implicit solve `(vin-v)/Rf = 2 Is sinh(v/(n Vt)) + v/Rfb`, Newton 3 iters warm start, tanh fallback for |v|>0.3 (Lambert-W approx).  Inter-stage coupling HPF ~ 20 Hz (ESTIMATED).
3.3 Tone stack (NYC) ESTIMATED values: LP branch R=39k, C=10 nF -> fc 408 Hz; HP branch R=22k... use HP C=3.9 nF, R=22k -> fc 1855 Hz (verify vs schematic; classic notch ~ 1 kHz sqrt(408*1855)=870 Hz at centre). `y = (1-T)*LP(x,408) + T*HP(x,1855)`; passive insertion loss ~ -20 dB at centre, restored by output booster (+~22 dB, ESTIMATED). Solve exact 2-node nodal system with pot loading; recompute at control rate.
3.4 BASS BOOST (NORMAL=NYC): ESTIMATED = extra shelf: +6..+9 dB low shelf at 150 Hz (via altered coupling C / LP branch C change). Implement as RBJ low shelf, f0=150 Hz, +8 dB, S=0.7, crossfade 20 ms on switch. Since real change is inside distortion circuit, alternative impl: swap interstage HPF 20 Hz->5 Hz and tone LP C 10 nF -> 15 nF; choose shelf for v1, flag in 10.
3.5 MIDS parametric (semi-parametric peak): RBJ peaking biquad, f0 = 310*(5000/310)^F Hz (F 0..1), gain G in [-10,+10] dB, Q {0.7, 2.5}. Footswitch on/off: crossfade peak gain to 0 dB over 10 ms (gain -> 0, not signal mix). Real unit analog bypass of section, no latency.
3.6 ATTACK (behavioural, ESTIMATED): reproduces pick-definition by mixing a lightly-clipped, high-passed transient path with the fuzz: `a = HP(x,300Hz) -> tanh(3x)->envelope-driven: tr = x - LP(x_env_slow)`; practical: `attack_path = softclip(HPF(inbuf_out, 250 Hz) * 4)`; transient emphasis `env_fast (1 ms atk, 30 ms rel)` minus `env_slow (20 ms)` clipped >=0, multiply: `p = attack_path * clamp((env_fast-env_slow)/env_slow,0,1)`. Mixed: `y += ATTACK_UI^1.5 * 0.7 * p` before VOLUME. Document as BEHAVIOURAL, not circuit.
3.7 GATE (behavioural, ESTIMATED): detector on input buffer output (before fuzz): full-wave |x| -> 1-pole env (atk 1 ms, rel 15 ms). Threshold dBFS = -75 + 45*G (G 0..1; G==0 -> disabled; ESTIMATED), hysteresis 4 dB (open at thr, close at thr-4 dB), hold 20 ms, gain attack 2 ms, release 80 ms (smoothed gain, applied post-fuzz to kill sustain tail/hiss). State machine: CLOSED -(env>thr)-> OPEN; OPEN -(env<thr-4dB for hold 20 ms)-> CLOSING -> CLOSED; CLOSING -(env>thr)-> OPEN. Closed floor -60 dB (not infinite; ESTIMATED).
3.8 Bypass: TRUE BYPASS (DOCUMENTED, retailer specs Adorama/Chicago Music Exchange; Sovtek variant adds internal buffered option, not modelled) -> default `ehx_deluxe_muff_true_bypass`=true: bypassed path is bit-exact dry, crossfade 5 ms equal-power (click-free). Set false for buffered behaviour (emulation option). Mids FS: gain-ramp 10 ms. Latency 0 samples reported (OS filter latency compensated via linear-phase-free minimum-phase IIR halfbands; reported latency 0 with mild phase shift) . Mono; stereo host: process L,R independent with linked params or sum per board.
3.9 Noise (default off in tests; on in product): fuzz hiss -88 dBFS at SUSTAIN>0.5, gated by GATE.

## 4 Plugin algorithm
```
x -> inTrim -> inbuf(HPF2Hz) -+-> gateDet
                              +-> boost(HPF30,gain(S)) -> sc1 -> HPF20 -> clip1 -> HPF20 -> clip2 -> tone(T)
 -> bassShelf(sw) -> outBoost -> midsPeak(F,G,Q,sw) -> *gateGain -> (+ attack p) -> *volume -> outHPF5 -> y
```
- Oversampling: 4x (2x halfband cascade of 8th-order IIR elliptic, >100 dB stop) around [boost..clip2]; 2x if SUSTAIN<0.2; 8x offline; `ehx_deluxe_muff_hq` Off=2x/On=4x default On. Tone/mids/gate at base rate.
- Aliasing control: OS + tanh/Lambert-W (ADAA 1st order optional on fallback tanh); test alias floor < -70 dB at 1 kHz sine, SUSTAIN max (see 9).
- Smoothing: one-pole 10 ms on all knobs (sustain gain, tone T, volume); 20 ms for filter coefficients (update every 16 samples with linear interp of coefficients); switch crossfades per 3.4/3.5/3.8. EXP input 5 ms smoothing, log mapping.
- Denormals: ScopedNoDenormals + add 1e-18 DC-free alternating offset in filter states; flush state < 1e-30.
- RT-safety: no alloc/locks; preallocated OS buffers sized maxBlock*8; params via atomics -> snapshot at block start; Newton bounded 3 iters; no std::exp per-sample except lookup table (sinh LUT 4096 pts, interp) or fast approx.
- CPU budget (ESTIMATED): <= 1.2% single core @ 48 kHz/4x/mono (i7-class), <= 2.5% stereo; warn and fall back to 2x if board budget exceeded.

## 5 I/O and routing
- Real: mono in, mono out, EXP in. Stereo plugin: default dual-mono (L,R processed independently, mids/gate/attack linked; gate detectors per channel unless `ehx_deluxe_muff_gate_link` on = max of both). Option `stereo_mode`: 0 dual-mono, 1 sum-to-mono in -> dual out (default per board).
- Series: any position in board chain; typical before amp. Gate placed inside pedal after fuzz; to gate externally use board effects-loop.
- Feedback tricks (real-world, user may emulate through board graph, not pedal internals): (a) Input from a send of a delay/reverb return pedal = fuzz-in-feedback-loop; (b) fuzz in a delay's feedback path (board "loop" block) -> self-oscillating gated tails; (c) parallel: split dry/fuzz with board wet/dry; (d) stereo split: two instances, hard-pan, different TONE/MIDS FREQ; (e) EXP+MIDS Q HI with sweep for wah-like vocal sweeps. The pedal itself has no loop jacks; do NOT invent internal send/return. Board graph must prevent unbounded feedback gain (clamp loop gain <= 0.98, soft limiter at -0.1 dBFS in loop).

## 6 MIDI / sync / expression emulation
Real unit has NO MIDI/clock/tap; sync features not applicable. Emulation scope = EXP jack + footswitch gestures + generic controller mapping (board-level MIDI learn, spec/reference-pedalboard/specs/MIDI_EXPRESSION.md).
- EXP: virtual jack `ehx_deluxe_muff_exp` 0..1 (heel..toe). Hardware: expression pedal or CV can sweep MIDS FREQ (DOCUMENTED, EHX manual). Knob/EXP interaction and heel/toe polarity are unpublished: ESTIMATED v1 = when EXP connected, `F_eff = exp` (overrides knob; heel = 310 Hz, toe = 5 kHz). Bit `exp_connected` param.
- CC map: the hardware has NO MIDI, so no manufacturer CC chart exists. OMIT any claim of an official MIDI map. The CCs below are a plugin-local convenience profile (user re-mappable, 14-bit optional; NOT hardware, NOT documented): CC4 -> EXP; CC7 -> VOLUME; CC14 SUSTAIN; CC15 TONE; CC16 ATTACK; CC17 GATE; CC18 MIDS LEVEL; CC19 MIDS FREQ; CC20 Q (>=64 hi); CC21 BASS BOOST (>=64 on); CC22 MIDS FS (>=64 on); CC23 BYPASS/engage FS (>=64 on). Program change: board-preset recall only (real unit has no presets).
- Footswitch gestures: OMIT hold-for-momentary-mids and velocity/press-speed crossfade gestures (no hardware basis; plugin implements plain latching toggles only, fixed crossfades per 3.5/3.8). Velocity -> LED brightness is cosmetic only.
- Aftertouch -> EXP mapping and generic MIDI-learn with takeover (jump/pickup/scale) are host/board-layer conveniences, not hardware features; implemented by the board layer, not this pedal.
- MIDI clock: ignored (no tempo features). Present but disabled "Not applicable" in sync UI.

## 7 Visuals (original vector art; ESTIMATED dimensions)
- Enclosure: ~ 145 W x 120 H(depth) mm plan view (two-footswitch chassis; ESTIMATED from product class ~ 5.75 x 4.7 in -- verify), graphite-silver brushed-metal body, black legends, red accent LEDs. Original artwork, no EHX logo/Pi mark; use generic "Pi-like" glyph? NO -> use text "DELUXE MUFF".
- Layout (mm from top-left of 145 x 120 face, ESTIMATED):
  Row1 y=20: SUSTAIN x=25, TONE x=72, VOLUME x=119 (knob dia 16 mm).
  Row2 y=45: ATTACK x=25, GATE x=72 (dia 13), BASS BOOST toggle x=119 (mini toggle).
  Row3 y=70: MIDS LEVEL x=25, MIDS FREQ x=62 (dia 13), Q toggle x=99, EXP jack silhouette on top edge x=100.
  Footswitches y=100: MIDS x=40, BYPASS x=105 (dia 14 mm); LEDs y=88 above each, dia 3 mm.
  Side jacks: INPUT right-side top, OUTPUT left-side? (use INPUT left, OUTPUT right, DC top-left, EXP top-right).
- Text labels: VOLUME, TONE, SUSTAIN, ATTACK, GATE, MIDS (LEVEL, FREQ), Q HI/LO, NORMAL/BASS BOOST, BYPASS, MIDS, EXP.
- Knob visuals: skirted knurled knobs, 0..10 tick ring 270 deg sweep (-135..+135), pointer line. LED glow only when ON.
- Accessibility: names above; MIDS FS and BYPASS shown with state text for screen readers.

## 8 Parameter table
| id | name | min | max | default | unit | skew | smoothing ms | automatable |
|---|---|---|---|---|---|---|---|---|
| ehx_deluxe_muff_bypass | Engage | 0 | 1 | 0 | bool(1=effect engaged, per reconciled header; was mislabelled 1=bypassed) | - | 5 xfade | yes |
| ehx_deluxe_muff_volume | Volume | 0 | 1 | 0.5 | norm | A (x^2) | 10 | yes |
| ehx_deluxe_muff_tone | Tone | 0 | 1 | 0.5 | norm | lin | 10 | yes |
| ehx_deluxe_muff_sustain | Sustain | 0 | 1 | 0.5 | norm | A | 10 | yes |
| ehx_deluxe_muff_attack | Attack | 0 | 1 | 0 | norm | lin | 10 | yes |
| ehx_deluxe_muff_gate | Gate | 0 | 1 | 0 | norm | lin (0=off) | 10 | yes |
| ehx_deluxe_muff_mids_on | Mids FS | 0 | 1 | 0 | bool | - | 10 xfade | yes |
| ehx_deluxe_muff_mids_level | Mids Level | -10 | 10 | 0 | dB | lin | 10 | yes |
| ehx_deluxe_muff_mids_freq | Mids Freq | 310 | 5000 | 1245 | Hz | log (geo-mean) | 20 | yes |
| ehx_deluxe_muff_mids_q | Q | 0 | 1 | 0 | enum LO/HI | - | 20 | yes |
| ehx_deluxe_muff_bass_boost | Bass Boost | 0 | 1 | 0 | bool | - | 20 xfade | yes |
| ehx_deluxe_muff_exp | Expression | 0 | 1 | 0 | norm | lin | 5 | yes |
| ehx_deluxe_muff_exp_connected | Exp connected | 0 | 1 | 0 | bool | - | - | yes |
| ehx_deluxe_muff_hq | HQ OS | 0 | 1 | 1 | bool | - | - | no |
| ehx_deluxe_muff_true_bypass | True bypass | 0 | 1 | 1 | bool | - | - | no |
| ehx_deluxe_muff_gate_link | Gate link | 0 | 1 | 0 | bool | - | - | no |
| ehx_deluxe_muff_stereo_mode | Stereo mode | 0 | 1 | 0 | enum | - | - | no |
| ehx_deluxe_muff_revision | Revision | 0 | 0 | 0 | enum | - | - | no |
Persist normalised values; unknown fields preserved.

## 9 Tests / acceptance (provisional until hardware capture)
- T1 null: bypass ON -> output == input within -120 dB (buffered: HPF 2 Hz tolerance -90 dB).
- T2 silence in -> |out| < -100 dBFS (noise off), no DC (< 1e-5).
- T3 FRF checkpoints, linear region (-40 dBFS in), SUSTAIN 0.1, TONE 0.5: notch depth at 800..1000 Hz >= 12 dB below 200 Hz level (ESTIMATED; verify). TONE 0: -3 dB point 350..500 Hz; TONE 1: HP corner 1.5..2.2 kHz.
- T4 MIDS: peak centre within 3% of f0 at 310, 1245, 5000 Hz; gain +10 dB +/-0.5; cut -10 +/-0.5; Q HI -3 dB BW narrower than LO by factor >= 3. Bypassing MIDS: flat +/-0.1 dB.
- T5 THD @1 kHz, -20 dBFS: SUSTAIN 0.1 THD 5..20%; SUSTAIN 1.0 THD 25..60% (square-ish, odd harmonics dominant, ESTIMATED).
- T6 alias floor: 2.5 kHz-4 kHz sines at SUSTAIN 1.0, alias products < -70 dBc (4x), < -55 dBc (2x).
- T7 gate: GATE=0 -> no attenuation; GATE=0.5: signal -50 dBFS decays to < -70 dBFS tail within 150 ms; hysteresis: open at thr, close at thr-4 dB +/-1 dB; no chatter on 60 Hz tremolo-level input.
- T8 attack: ATTACK=0 -> output identical to model with path removed (null < -120 dB); ATTACK=1 raises first 20 ms peak vs sustain by >= 3 dB for pluck.
- T9 click: toggling BYPASS/MIDS/BASS BOOST at zero-crossing and peak: max |dy/dt| bounded, no step > 0.1 FS.
- T10 block sizes 1/4/16/64/512/2048 + non-power-two: bit-identical output (deterministic, noise off); SR 44.1/48/96/192 within 1 dB at checkpoints.
- T11 EXP: sweep 0..1 in 1 s: MIDS FREQ monotonic, no zipper (> -60 dB steps); log mapping 310..5000 Hz.
- T12 30 min random action soak (knobs, FS, bass boost, MIDI CC flood 1 kHz): no NaN/inf, no alloc (RT allocator trap), CPU < budget.
- T13 save/restore: all params round-trip; old preset w/o block bit-identical.
- Hardware reference: licensed same-revision unit, 0/25/50/75/100% grid, 4 input levels, 44.1/48/96 k capture; thresholds set after measurement.

## 10 Known inaccuracies
- Still ESTIMATED: tone-stack and clip-stage component values (sec 3.1-3.3; 39k/10nF and 22k/3.9nF corner arithmetic is correct but source tone-stack values not reached; Sovtek/Deluxe may differ), tapers, numeric Q, gate thresholds/timings, ATTACK algorithm, BASS BOOST implementation, mids placement, LED colour, enclosure dims/colour. Now DOCUMENTED: bypass type, Zin/Zout, power, MIDS range, control semantics.
- ATTACK and GATE are behavioural, not circuit-accurate; true hardware likely uses VCA/FET detector.
- Bass boost as shelf approximates in-circuit change.
- Exp heel/toe polarity and EXP-vs-knob interaction unknown (ESTIMATED v1 behaviour in sec 6).
- Battery: retailer listing says battery included; battery sag still not modelled.
- No transistor-level hFE/ tolerance variation; no supply sag; no battery.
- MIDI CC map is plugin-local, not hardware (sec 6).
- Next evidence: read official manual (S1/S2), ElectroSmash NYC values, capture from real unit.

## 11 Verification log
Run 2026-10-22. Reached (WebSearch excerpts only; all direct fetches blocked): EHX product page + manual text (ehx.com), Adorama (https://www.adorama.com/ehdlxbm.html), Audiofanzine FR listing, Coast Sonic, ModularGrid listing, Thomann manual (Pi 2), effectsdatabase. Not reached: ElectroSmash, bigmuffpage.com, archive.org, manuals.plus (fetch blocked).
| # | Item | Old -> New | Status | Source |
|---|---|---|---|---|
| 1 | Input impedance | ~100k..470k -> 130 kOhm | DOCUMENTED (retailer) | Adorama, Audiofanzine |
| 2 | Output impedance | ~1k-10k -> 300 Ohm | DOCUMENTED (retailer) | Adorama, Audiofanzine |
| 3 | Bypass type | buffered (est.) -> true bypass; true_bypass default 0 -> 1 | DOCUMENTED (retailer) | Adorama, Chicago Music Exchange |
| 4 | Power | adapter only, no battery -> battery included, 9.6VDC-200mA optional, 22 mA, max 12 V, centre-neg | DOCUMENTED (retailer) | Adorama, Audiofanzine |
| 5 | MIDS freq range 310 Hz-5.0 kHz | est. -> confirmed | DOCUMENTED | EHX manual/product page |
| 6 | MIDS level +/-10 dB, Q HI/LO semantics, EXP sweeps MIDS FREQ, GATE CW=higher threshold / CCW off, ATTACK = enhanced dry blend, BASS BOOST normal=NYC | confirmed text | DOCUMENTED | EHX manual/product page, Thomann |
| 7 | No blend knob | est. -> confirmed by control list | DOCUMENTED | EHX manual excerpts |
| 8 | Numeric Q (0.7 / 2.5) | unchanged | ESTIMATED | no published Q; HI/LO ratio >=3 |
| 9 | Tone stack 39k/10nF, 22k/3.9nF; clip stage, sustain gain, diodes | unchanged | ESTIMATED | topology from ElectroSmash/PedalPCB excerpts; values unverified |
| 10 | Tapers, gate thresholds/timings, ATTACK algo, BASS BOOST shelf, mids placement, LED colour, enclosure dims, EXP polarity | unchanged | ESTIMATED | derivations in 3.x |
| 11 | Revision label "dlx_2014" | -> generic `deluxe_guitar`, no year | ESTIMATED (year unverified, removed) | - |
| 12 | MIDI CC chart (CC4..23) | presented as defaults -> declared plugin-local, no hardware MIDI | OMIT (official chart) | hardware has no MIDI |
| 13 | Hold-for-momentary-mids, velocity press-speed gestures | present -> removed | OMIT | no hardware basis |
| 14 | Param table: `bypass` polarity/default (1=bypassed) | -> Engage 1=engaged, default 0 | fixed inconsistency | reconciled header |
| 15 | Duplicate trim params in sec 8 | removed rows | fixed inconsistency | reconciled header |
