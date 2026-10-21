# mxr-phase90-script — dev spec (Phase 90, 1970s "Script" logo circuit)

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `mxr-phase90-script`, **paramPrefix `mxr_phase90_script_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/mxr_phase90_script.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/mxr_phase90_script_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_mxr_phase90_scriptTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `mxr_phase90_script_bypass`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims (none) -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `mxr_phase90_script_variant`, `mxr_phase90_script_noise`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): `mxr_phase90_script_stage_mismatch`, `mxr_phase90_script_sync`, `mxr_phase90_script_sync_div`, `mxr_phase90_script_reset_on_stomp`.
> - Host lanes `ref_s{nn}_p01..` in this order: `mxr_phase90_script_speed`, `mxr_phase90_script_depth_trim`, `mxr_phase90_script_mix_trim`, `mxr_phase90_script_stereo_phase`, `mxr_phase90_script_ext_speed`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Pedal id: `mxr-phase90-script`. Param prefix: `mxr_phase90_script_`. Category: Phaser (4-stage, 1 knob).
Tags: MEASURED = from instrument measurement; DOCUMENTED = from cited source; ESTIMATED = engineering guess, to be replaced by bench data.
Name use: descriptive "inspired by" only. No trademark art/photos/text copied. Visuals = original vector description (sec 7).

## 0 Sources and research status
Fetch of electrosmash.com / dafx.de / dunlop PDFs was blocked by session proxy (403 CONNECT). Facts below come from web-search excerpts of those pages; anything not excerpted is ESTIMATED. Re-verify items marked `VERIFY`.
- S1 ElectroSmash Phase 90 analysis: https://www.electrosmash.com/mxr-phase90 (5 blocks: power, input buffer, phase-shift stage, LFO, output mixer; JFET 2N5952 x4, 2N4125 buffer)
- S2 Eichas/Zolzer, "Physical Modeling of the MXR Phase 90", DAFx-14: https://dafx.de/paper-archive/2014/dafx14_felix_eichas_physical_modeling_of_the_.pdf (4 allpass, JFET as VCR, R15 feedback)
- S3 Bernardini/Vergani et al., "Wave Digital Model of the MXR Phase 90 ... Time-Varying Resistor Approximation of JFET", DAFx-24: https://www.dafx.de/paper-archive/2024/papers/DAFx24_paper_13.pdf ; demo https://polimi-ispl.github.io/mxrphase90/ (input R1=10k, C1=0.01uF, R2=470k, Vref=5.1V; example LFO 0.365/1.925/7.675 Hz)
- S4 Faust vaeffects ve.mxrPhase90 (PR 254): https://github.com/grame-cncm/faustlibraries/pull/254 (asym triangle 65% rise, gate 3.25V +/-0.15V, sweep 330 Hz-3.3 kHz)
- S5 Cytomic note: https://cytomic.com/mxr-phase-90-jfet-based-phaser/
- S6 Dunlop M101 manual (50th-anniv M101GLD excerpt): https://jimdunlop.com/content/manuals/M101GLD.pdf (Zin 1 Mohm, Zout 10 ohm, +/-? nominal -20 dBV, noise -97 dBV A-wtd, true bypass, 5 mA, 9V)
- S7 Script vs Block: Dunlop history + Wikipedia "Phase 90": Block adds 24k feedback resistor (R15) and early Block has 4.7k in place of 10k in LFO (higher max rate). Script = no feedback. (Wikipedia tagged "better source needed" -> ESTIMATED/VERIFY.)
- Prototype notes: spec/reference-pedalboard/models/mxr-phase90-script.md

## 1 Identity / variants
| id | variant | feedback | LFO max rate | status |
|---|---|---|---|---|
| `mxr-phase90-script` (this spec, default) | 1974-77 Script logo | none (mxr_phase90_script_fb=0) | ~7.5 Hz | ESTIMATED |
| optional toggle `mxr_phase90_script_variant`=1 | "Block" | R15 24k -> g_fb ~0.35 (ESTIMATED) | ~16 Hz (4.7k LFO R) | S7 |
Default variant 0 (Script). Variant toggle is plugin extension (real EVH-style units have a switch; vintage doesn't); hide in "Advanced" overlay, not on face.
Real unit facts: single knob (SPEED), one footswitch, no LED on vintage Script (LED on modern reissues, red), 9V battery or DC jack on later units, mono in/mono out, true bypass (S6, modern; vintage ESTIMATED same).

## 2 Controls
| control | range | taper | default | real behaviour |
|---|---|---|---|---|
| SPEED knob | 0..1 (rotary 0-300 deg, 7 o'clock to 5 o'clock) | log-ish: rate = 0.3*25^x Hz (0.3..7.5) ESTIMATED; pot audio taper 'A' | x=0.5 (~1.5 Hz) | Sets LFO freq only. Min ~ 1 cycle per 3 s; max ~ 7-8 cycles/s. Changing knob does NOT reset LFO phase. |
| Footswitch (stomp, SPST momentary-latching) | on/off | - | off (bypass) | On = effect in, off = true bypass (hardwired). Click: mechanical, no electronics. |
| Input jack | 1/4" TS | - | - | Zin 1 Mohm (S6) |
| Output jack | 1/4" TS | - | - | Zout 10 ohm (S6) |
| DC jack (later units) | 9V, 5 mA | - | - | Plugin: none |
| (no depth, mix, feedback, level knobs) | | | | Constants, see sec 3 |
Plugin extras (NOT on face; Advanced overlay): `mxr_phase90_script_variant`, `mxr_phase90_script_depth_trim`, `mxr_phase90_script_mix_trim`, `mxr_phase90_script_lfo_shape`, sync controls (sec 6).

## 3 Signal path / circuit model
```
in -> buffer(JFET 2N4125-type follower, Zin 1M, HP 10k/10nF pole ~1.6 Hz) -> AP1..AP4 (first-order, JFET Rds in series leg, shared gate V)
   -> out_AP4; wet = AP4 ; mix = 0.5*(dry + wet) [ESTIMATED 1:1 sum, fixed]  -> output buffer -> out
   [Block only: feedback g_fb * AP4_out -> subtract/add at AP1 input]
LFO: asym-triangle ~ -> gate bias 3.25V +/-0.15V -> JFET Rds(Vgs)
```
- Stages: 4 identical (matched 2N5952 per S1) first-order allpass: H(z) from analog H(s)=(s - wc)/(s + wc) (sign per stage topology), phase -2*atan(w/wc). Sweep wc/2pi = fc in [330, 3300] Hz (DOCUMENTED S4, measured on model, VERIFY vs hardware).
- Notch locations (all 4 stages equal fc), with 0.5 sum: total phase -180 deg => notch1 f = tan(22.5deg)*fc = 0.4142*fc; notch2 total -540 => f = 2.4142*fc. So at fc=330..3300: notch1 137..1367 Hz, notch2 797..7966 Hz. Depth is infinite for ideal 1:1; real: finite from stage mismatch (ESTIMATED -25..-40 dB). Param `mxr_phase90_script_stage_mismatch` default 0.02 (2%) per stage.
- Script: no feedback. Block: positive/negative feedback from AP4 to AP1 input g_fb ~0.35 (ESTIMATED) raising peaks, narrower notches.
- LFO: free-running relaxation, asymmetric triangle, 65% rise / 35% fall (DOCUMENTED S4), rate 0.3..7.5 Hz (ESTIMATED; S3 demos 0.365, 1.925, 7.675 Hz). Waveform smoothed ~ RC (round corners, 1-pole 20 ms on triangle) ESTIMATED.
- JFET law: Rds(Vgs) ~ Rds0/(1 - Vgs/Vp)^~1 near ohmic; plugin uses fc(v) = 330 * 10^(v) where v in [0,1] LFO normalized (exp sweep => 1 decade). Replace by tabulated curve if bench data obtained.
- Levels: nominal in/out -20 dBV (S6) = unity gain. DI cal: 0 dBFS = +4 dBu (adjustable board-wide). Noise floor -97 dBV A-wtd (S6) -> optional hiss `mxr_phase90_script_noise` default off.
- Distortion: JFET Vgs-modulated nonlinearity at high input (>~ -6 dBV); model soft shaper x/(1+|x|^2)^0.5 on each stage output at drive 0.2 (ESTIMATED), THD < 1% at -20 dBV.
- Latency: 0 samples (IIR). Oversampling does not add latency.
- Bypass: true hardwire bypass (S6). Transition: plugin crossfade 5 ms equal-power to avoid clicks; LFO phase frozen while bypassed (real unit: LFO stops/does not run in true bypass is ESTIMATED).

## 4 Algorithm for plugin
```
per block: read params (atomic), update smoothers
per sample:
  lfo phase p += rate/fs ; tri = p<0.65 ? p/0.65 : (1-p)/0.35   // 0..1
  tri_s += a_lfo*(tri - tri_s)                                   // 1-pole, tau 20 ms
  v = depth*(tri_s-0.5)+0.5   ; depth_trim default 1
  fc = 330 * 10^(v)  -> fc clipped [330,3300]
  per-stage fc_k = fc*(1+mismatch_k)        // mismatch fixed seeds per instance
  g_k = tan(pi*fc_k/fs) ; a_k=(g_k-1)/(g_k+1)    // TPT/bilinear coeff
  x0 = in_hp(in)                           // 1-pole HP 1.6 Hz (DC block)
  y=x0 + g_fb*fbState (variant 1 only)
  for k=1..4: out = a_k*y + s_k ; s_k = y - a_k*out ; y = out     // TDF-II allpass
```
TDF-II allpass as above (denormal-safe with +1e-18 offset or FTZ/DAZ via ScopedNoDenormals).
- wet=out_AP4 ; `y_mix = 0.5*(x0 + wet)` ; fbState = out_AP4 (1-sample delay in fb loop; Block only, limit g_fb<0.6 for stability).
- Coefficient update: per-sample for fc (cheap: tan -> use rational approx or 16-sample block update + linear interp). Default: per-sample `tan` approx (Pade 3,2) accurate <0.1% to 0.45 fs.
- Oversampling: none needed (linear time-varying; shaper mild). Optional 2x polyphase when `mxr_phase90_script_drive`>0.5.
- Smoothing: speed 50 ms 1-pole on log-rate; trims 20 ms; bypass 5 ms xfade.
- Denormals: ScopedNoDenormals + states flushed if |s|<1e-20.
- CPU budget: <0.4% single core @48k stereo, 128 buf (est. 25 flops/sample/ch).
- RT safety: no alloc/lock; mismatch table, LFO, coeffs preallocated in prepare(); params via std::atomic<float>; UI -> engine via lock-free SPSC.
- Stereo: process L/R with independent states; single shared LFO unless `mxr_phase90_script_stereo_phase` (extra, default 0 deg) offsets R LFO 0..180 deg.

## 5 I/O and routing
- Real: mono TS in, mono TS out, no loop. Plugin: mono-sum or dual-mono in board; stereo in -> two instances sharing LFO.
- Board tricks supported by board graph (no pedal-level jacks): series before/after drive (classic: phaser before distortion = notch sweep clean; after = ring-like pitch), parallel send/return with other phaser (2 pedals, rates differing => beating), board-level feedback loop (out -> return -> in of an earlier pedal) caps gain 0.9 + soft clip tanh, wet/dry universal control from board applies to whole pedal (adds to internal 1:1).
- No expression/aux/control/MIDI jacks on real unit. Plugin adds virtual `mxr_phase90_script_ext_*` (sec 6).

## 6 MIDI / sync / expression (plugin-level emulation; real unit has none)
- Real unit: none. Comparable analogs: Phase 90 with external LFO-rate expression pedal mods (aftermarket) and tap tempo clones. Mark as EXTENSION.
- MIDI clock: `mxr_phase90_script_sync` 0=free,1=clock. Rate = tempo_bpm/60 * note_mult; note_mult list {1/1:0.25,1/2:0.5,1/4:1,1/8:2,1/8T:3,1/16:4}; default 1/4 (x1). Phase-lock to host PPQ (host transport) or MIDI clock 24 ppqn; resync on Start/Continue; free-wheel on lost clock after 500 ms. Allow 0.1..16 Hz in sync mode.
- Tap tempo: Advanced `mxr_phase90_script_tap` button (and mapped footswitch gesture double-tap); avg last 3 taps, timeout 2 s.
- CC map (ch1 default): CC1 mod wheel -> SPEED; CC11 expression -> SPEED (via `mxr_phase90_script_ext_speed` 0..127 -> 0..1, 5 ms smoothing); CC64 sustain -> bypass toggle (>=64 on, edge-triggered); CC7 -> board level (board-owned). PC 0..3 -> presets (4 speed positions: 0.1,0.35,0.6,0.9).
- Velocity gesture: footswitch note-on maps: vel<40 = soft tap (bypass fade 20 ms), vel 40..90 = normal (5 ms), vel>90 = hard stomp (1 ms click-equivalent + optional LFO phase reset `mxr_phase90_script_reset_on_stomp`). Aftertouch (channel pressure) -> SPEED offset +0..+0.5 (momentary rate ramp). Poly AT ignored.
- Mappable: any slider/knob via MIDI learn to mxr_phase90_script_speed (board-level mapping table; 0..1 normalized, linear or exp curve).

## 7 Visuals (original vector art; do not trace trademark graphics)
- Enclosure: powder-coat orange (#E8731A approx, ESTIMATED) die-cast box, 111 x 60 x 50 mm (ESTIMATED), rounded corners r=3 mm.
- Face (top view, origin top-left): knob center (55.5, 18) mm, dia 18 mm, black skirted aluminum-cap style with white pointer line; footswitch center (55.5, 44) mm, dia 20 mm chrome stomp; no LED on Script (optional red LED dia 3 mm at (55.5, 29) when `mxr_phase90_script_led_mod`=1).
- Text (own typeface, not the original logo): "PHASE 90" cursive-style script in black at y=34 mm height 8 mm; "SPEED" at y=8 mm 3 mm tall below/above knob; small brand placeholder "INSPIRED BY".
- Side jacks: input left, output right at y=30 mm. Battery door bottom.
- LED/animation: none; knob rotates 300 deg; footswitch depress 1.5 mm on click.
- Board fit: footprint 111 x 60 mm; mounting holes none.

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | automatable |
|---|---|---|---|---|---|---|---|---|
| mxr_phase90_script_speed | Speed | 0 | 1 | 0.5 | norm | exp rate 0.3..7.5 Hz | 50 | yes |
| mxr_phase90_script_bypass | Bypass | 0 | 1 | 1 | bool | - | 5 xfade | yes |
| mxr_phase90_script_variant | Script/Block | 0 | 1 | 0 | enum | - | none | no |
| mxr_phase90_script_depth_trim | Sweep depth | 0 | 1 | 1 | norm | lin | 20 | yes |
| mxr_phase90_script_mix_trim | Wet trim | 0 | 1 | 1 | norm (1=1:1) | lin | 20 | yes |
| mxr_phase90_script_stage_mismatch | Stage mismatch | 0 | 0.1 | 0.02 | ratio | lin | none | no |
| mxr_phase90_script_sync | Sync | 0 | 1 | 0 | enum | - | none | yes |
| mxr_phase90_script_sync_div | Division | 0 | 5 | 2 | enum | - | none | yes |
| mxr_phase90_script_stereo_phase | R LFO offset | 0 | 180 | 0 | deg | lin | 20 | yes |
| mxr_phase90_script_ext_speed | Ext speed (MIDI) | 0 | 127 | 64 | CC | lin | 5 | yes |
| mxr_phase90_script_reset_on_stomp | Reset LFO on stomp | 0 | 1 | 0 | bool | - | none | no |
| mxr_phase90_script_noise | Hiss | 0 | 1 | 0 | bool | - | none | no |
Mixing with board wet/dry (universal) applied outside pedal.

## 9 Tests / acceptance (provisional until bench data)
- Null test: speed=0.5, static LFO at v=0.5 (fc=1044 Hz): FRF magnitude must match analytic 0.5*|1+A^4(f)| within 0.1 dB; notches at 0.414fc=432 Hz and 2.414fc=2520 Hz (+/-2%).
- Sweep checkpoints: fc 330 -> notch1 137 Hz, notch2 797 Hz; fc 3300 -> 1367 / 7966 Hz.
- LFO: period at speed=0 = 3.33 s (+/-3%), speed=1 = 0.133 s (+/-3%); rise fraction 0.65 +/-0.01.
- Bypass: bit-exact passthrough (null < -140 dB) after fade; xfade no click (peak delta < 1 LSB 24b for sine).
- THD at -20 dBV 1 kHz < 0.5%; at -6 dBV < 3% (ESTIMATED).
- Noise off: silent in -> silent out, no DC (|DC|<1e-6). Impulse stability for variant 1 with g_fb=0.6 for 60 s.
- Block sizes 1/4/16/64/512; sample rates 44.1/48/88.2/96/192: notch freq error < 3%.
- CPU < 0.4% @48k stereo; 30 min random param soak with no NaN/denormal slowdown.
- MIDI: clock 120 bpm 1/4 -> 2.0 Hz (+/-0.5%); CC11 0->127 spans full speed range monotonic.
- Compare vs licensed same-revision unit when available; record uncertainty.

## 10 Known inaccuracies
- Allpass R/C values, exact JFET law, pot value and taper, LFO exact range, 1:1 mix, feedback value and Script-vs-Block specifics are not confirmed from primary schematic (source fetch blocked): VERIFY with S1/S2/S3.
- No JFET parameter spread (Idss/Vp) beyond `stage_mismatch`; real units vary much, including notch depth.
- Real LFO jitter, supply sag (battery) and temperature drift not modelled.
- Hardware has no MIDI/sync; all of sec 6 is extension.
- Not a looper: loop-spec requirement n/a to this pedal.
- Do not claim "authentic" until measured.
