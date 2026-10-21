# ehx-muff-wicker : Fuzz/sustainer ("inspired by Electro-Harmonix Big Muff Pi with Tone Wicker, 2008 NYC-line") - dev-ready spec

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `ehx-muff-wicker`, **paramPrefix `ehx_muff_wicker_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/ehx_muff_wicker.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/ehx_muff_wicker_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_ehx_muff_wickerTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `ehx_muff_wicker_enabled`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims `ehx_muff_wicker_input_trim_db` -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params `ehx_muff_wicker_soft_press`, `ehx_muff_wicker_midi_chan` -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `ehx_muff_wicker_quality`, `ehx_muff_wicker_auto_os`, `ehx_muff_wicker_stereo_mode`, `ehx_muff_wicker_noise_enable`, `ehx_muff_wicker_revision`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): `ehx_muff_wicker_howl`.
> - Host lanes `ref_s{nn}_p01..` in this order: `ehx_muff_wicker_sustain`, `ehx_muff_wicker_tone`, `ehx_muff_wicker_volume`, `ehx_muff_wicker_tone_in`, `ehx_muff_wicker_wicker`, `ehx_muff_wicker_wicker_morph`, `ehx_muff_wicker_tone_out_trim_db`, `ehx_muff_wicker_supply_v`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Version 0.1 | 2026-10-21 | model_id `ehx-muff-wicker` | param prefix `ehx_muff_wicker_` | status SPEC
Base: inherits ALL of `specs/pedals/ehx-big-muff-nyc.md` (call NYC-spec) sec 3.1-3.4 constants, Newton clip solver, tone formulas, oversampling, smoothing, denormals, tests. This file = DELTA (Wicker + Tone-bypass switches) + full tables. Share code: `Models/ehx_muff_core.h` (new, used by NYC + Wicker); Wicker = core + 2 switch states.
Evidence tags: DOCUMENTED / MEASURED (none) / ESTIMATED. Research limits: sandbox DNS blocked all direct fetches (ehx.com, juno PDF, mirosol.kapsi.fi, electrosmash) -> only search-excerpt facts; the Wicker circuit has NO published schematic (a builder blog states they had to trace their own unit, 2013 NYC unit, mostly SMD, ceramic tone caps, BC550C). Every Wicker-specific component value below = ESTIMATED and MUST be replaced by a pinned trace + hardware capture before any fidelity claim. Do not advertise "authentic/identical".

Sources
- S1 EHX manual doc 560380-01U (hosted https://es.juno.co.uk/downloads/manual/560380-01U.pdf/): battery or external adaptor; clean 9V DC, 50 mA, centre-negative; recommended 9.6DC-200 class PSU; 1-yr warranty. DOCUMENTED (via search excerpt, PDF not read).
- S2 Retail copy (Thomann https://www.thomann.de/ie/electro_harmonix_big_muff_pi_tone_wicker.htm , Long&McQuade https://www.long-mcquade.com/9890/Guitars/Guitar-Effects/Electro-Harmonix/Big-Muff-PI-with-Tone-Wicker.htm , Sweetwater https://www.sweetwater.com/store/detail/BigMuffPiW--electro-harmonix-big-muff-pi-with-tone-wicker-fuzz-pedal/reviews ): "Wicker switch opens up 3 high-frequency filters"; "Tone switch bypasses tone control"; Wicker off = original Big Muff sound; true bypass; made in USA; listed since Dec 2008. DOCUMENTED.
- S3 TDPRI owner review https://www.tdpri.com/threads/review-big-muff-with-tone-wicker.155139/ : Wicker brightens; noise much more noticeable with Wicker on at high sustain, esp. Tone off; Tone-off can feed back. Anecdotal -> qualitative targets only.
- S4 NYC-spec S1-S5 (ElectroSmash/GuitarScience/EffectsDatabase) for base circuit.
- S5 Mirosol blog trace note https://mirosol.kapsi.fi/?p=2018 (unfetched; lead only).
- S6 internal: spec/reference-pedalboard/models/ehx-muff-wicker.md (research-only note).
- Outstanding evidence: which 3 nodes/caps the Wicker switch touches; Tone-off path level compensation; exact bypass relay vs mechanical 3PDT. Action: obtain trace or buy unit and scope.

## 1 Identity / variants
- Real unit: EHX Big Muff Pi with Tone Wicker (2008+, NYC production, mono, discrete-transistor, 3 knobs + 2 mini toggles + 1 footswitch + LED). No MIDI, no expression, no tempo, no extra jacks, no loop. DOCUMENTED (S2,S6).
- Not to be confused with "Big Muff Pi 2 with Tone Wicker" (dual op-amp, late-70s unpublished Bob Myer design; DIFFERENT circuit) nor Deluxe/Nano. Enum `ehx_muff_wicker_revision`: 0=nyc_2008 (default, only v1 value), 1=bmp2_opamp (RESERVED, unimplemented). Revision changes only `MuffConsts` + Wicker node map.
- Power S1; draw ESTIMATED 5-10 mA; Zin ESTIMATED 70 kOhm; Zout ESTIMATED <=25 kOhm. Bypass: true bypass DOCUMENTED (S2).
- Display name "Sustainer (Muff-style, Wicker)". No EHX logo/pi glyph/trade dress copy; generic mark.

## 2 Controls (every physical element)
| Control | Range | Taper | Default | Real behaviour |
|---|---|---|---|---|
| SUSTAIN knob | 0..10 (UI 0..1) | 100k audio ESTIMATED; g(x)=(e^{kx}-1)/(e^k-1), k=4.394 (NYC-spec 2) | 0.5 | Gain into clip stage 1. floor g>=0.005 |
| TONE knob | 0..10 | 100k linear ESTIMATED | 0.5 | CCW dark / CW bright, centre scoop ~1 kHz (NYC-spec 3.3). Inactive (no audible effect) when TONE switch = OUT |
| VOLUME knob | 0..10 | 100k audio ESTIMATED | 0.5 | Output level after recovery stage |
| TONE switch (mini toggle, 2-pos, ON-ON) | IN / OUT | n/a | IN | IN = Muff tone stack in path. OUT = tone stack bypassed: signal goes clip-stage-2 -> recovery directly; no mid scoop, louder, more feedback/noise-prone (S2,S3). DOCUMENTED function; level ESTIMATED |
| WICKER switch (mini toggle, 2-pos) | OFF / ON | n/a | OFF | OFF = original Big Muff voicing (NYC-spec). ON = three high-frequency filters "opened" (S2) -> more top end, rawer sustain, more hiss. DOCUMENTED function; which 3 filters ESTIMATED sec 3.2 |
| Footswitch | latch | n/a | off | True-bypass toggle |
| LED | red | n/a | off | Follows `enabled` |
| INPUT / OUTPUT 1/4" mono, DC 9.6V centre-neg, 9V battery | - | - | - | Not on face except as side-view art. Advanced `supply_v` only |
Wicker + Tone interplay (4 states, all must be reachable): [IN,OFF]=classic Muff; [IN,ON]=bright Muff, scoop kept; [OUT,OFF]=flat, thick, loud; [OUT,ON]=widest/brightest, no scoop (S3 anecdote "huge, no scooped mids"), noisy, can self-oscillate near feedback.
Switch actions are instantaneous mechanical toggles; plugin smooths them (sec 4.4) because filter swap would click.

## 3 Signal path & circuit/DSP model
Order: IN -> true-bypass -> HPF23 -> Q1 boost (+20 dB, tanh rail) -> SUSTAIN -> HPF c1 -> CLIP1 (diode FB, Cf1) -> HPF c2 -> CLIP2 (diode FB, Cf2) -> [W3 LP] -> {TONE stack | direct} -> Q4 recovery -> VOLUME -> out.
### 3.1 Constants
All NYC-spec 3.1 values unchanged (Rf 470k, Cf 470 pF, A=150, diode Is 2.52 nA n 1.752, Rail 3.0 V*(supply_v/9), tone R/C, HPFs). Added (ESTIMATED):
| Name | Wicker OFF | Wicker ON | Note |
|---|---|---|---|
| Cf1 (clip1 feedback cap) | 470 pF (fc=1/(2pi*Rf*Cf)=720 Hz) | Cpar 22 pF (fc 15.4 kHz) | W1: Wicker removes HF-limiting feedback cap on stage 1; Cpar = layout/transistor Miller residue |
| Cf2 (clip2 feedback cap) | 470 pF | 22 pF | W2 |
| W3: post-clip LP before tone node | 1-pole LP fc3 = 6.0 kHz (R=2.7k, C=10 nF) | bypassed (fc3 -> 60 kHz, i.e. removed) | W3: third "HF filter"; ESTIMATED guess = shunt/series RC at Q3 collector. Replace on trace |
| Tone-OUT path gain Gt_out | - | - | 1.0 (0 dB) at node into Q4; vs tone stack passband loss 7..14 dB (NYC-spec 3.3 table @t=0.5) => TONE=OUT is ~+9..+13 dB hotter ESTIMATED; param `tone_out_trim_db` adv. default 0 |
Open question flagged: real unit may level-compensate. Provide adv `tone_out_trim_db` -12..+6 so builder can match scope capture.
### 3.2 Behaviour of the three Wicker filters
- Small-signal gain per clip stage with Cf removed: Rf/Rs ~47 (33 dB) flat to GBW-limited ~15 kHz instead of 33 dB falling above 720 Hz -> large HF gain increase (+20..+26 dB at 5-10 kHz per stage, ESTIMATED). Clipped signal has much more upper harmonic content; fuzz becomes brighter/buzzier ("raspy", S2).
- W3 removal adds a further ~+3 dB at 6 kHz, +10 dB at 20 kHz pre-tone.
- Noise: HF noise bandwidth rises ~x20 -> hiss +10..+15 dB ESTIMATED; model with optional `noise_enable` shaped noise (default off; amplitude ref in sec 9).
- With TONE=IN the tone-stack LP arm (408 Hz, 3.3) still removes much of the added HF when tone<0.5; at tone>0.5 HP arm passes it (bright). With TONE=OUT nothing downstream rolls off except Q4 20 kHz -> max bright.
### 3.3 Tone network and bypass
Tone IN: NYC-spec 3.3 biquad H(s) (t=tone). Tone OUT: H=1. Blend `mix_t` in [0,1] (0=OUT,1=IN) smoothed 10 ms: `y = (1-mix_t)*x*Gt_out + mix_t*H(x)`; both branches run every sample while mix_t in (0,1), only active branch otherwise (CPU).
### 3.4 Remaining stages/latency/bypass
As NYC-spec 3.4/3.5: Q1/Q4 tanh, volume Gcal, output HPF10/LPF20k, DC blocker. Latency = oversampler only (Eco 0 / Normal 24 / HQ 40 @1x samples), bypass delay-matched. TrueBypass, xfade 5 ms, footswitch-velocity speed lerp(12,2) ms, enable pre-roll 1024-sample from slot input ring. Wicker/Tone changes do not affect latency.
Wicker ON default quality: Normal(4x) minimum; recommend auto-bump to HQ 8x offline because HF content is far higher (sec 4.3).

## 4 Algorithm for plugin
### 4.1 Block diagram
```
in -> trim -> up xN -> HPF23 -> Q1 tanh -> *g_sus -> HPF c1 -> CLIP1(Cf1(w)) -> HPF c2 -> CLIP2(Cf2(w))
   -> W3 LP (fc3(w)) -> [mix_t: OUT*Gt_out | TONE biquad] -> Q4 tanh -> *g_vol*Gcal -> HPF10/LPF20k -> down -> xfade bypass -> out
w = wicker smoothing state 0..1 ; mix_t = tone-in smoothing state 0..1
```
### 4.2 Per-sample
As NYC-spec 4.2 with these replacements:
- Cf(w) = Cpar + (Cf_nom - Cpar)*(1-w)  (linear in C; recompute trapezoid coef 2*Cf/T per 16-sample sub-block while w moves; else cached).
- W3: 1-pole LP, fc3(w) = exp-interp 6 kHz (w=0) -> 60 kHz (w=1) (clamped < 0.45*fs_os); coef per sub-block.
- Newton solver unchanged (damped, <=4 iter, |vd|<=1.3 V, warm start). With Cf small, ic term shrinks; iteration stays stable (g_d dominated by diode). Add convergence test: if residual > 1e-3 after 4 iters, take 2 more (max 6) and count `newtonFallbacks`.
### 4.3 Oversampling & aliasing
Wicker ON removes the 720 Hz natural band-limit, so aliasing is worse than NYC. Targets (1.2 kHz sine, sustain 1.0, 48 kHz): Normal(4x) alias products >= 50 dB below fundamental when Wicker ON (>= 60 dB OFF); Eco 2x >= 38 dB ON; HQ 8x >= 70 dB ON. If Normal fails measured test, Wicker ON forces min 6x/8x polyphase (ratio per `quality` mapping 0=2x,1=4x,2=8x; add `auto_os`: ON by default => quality 1 becomes 8x while wicker>0.5 with 20 ms crossfade of oversampler ping-pong, prebuilt in prepare). Halfband specs as NYC-spec 4.3. Input to up-sampler: keep NYC.
### 4.4 Smoothing, denormals, RT safety
- Switch smoothing: w and mix_t one-pole/linear ramp 10 ms (`smoothing_switch_ms`); no hard swaps. Knob smoothing per sec 8. Ramps sampled per 16-sample sub-block at OS rate.
- Denormals: `juce::ScopedNoDenormals` + flush |z|<1e-20; add 1e-18 DC in feedback state. NaN guard resets stage (counter `nanResets`).
- RT-safety: all filter tables, oversampler, biquad state preallocated in `prepare`; params as std::atomic<float> read once per block; no alloc/lock/log in `process`; switch state read via atomic<bool>.
- Wicker/tone toggles during bypass: state ramps continue (cheap scalar) so re-enable has no jump.
### 4.5 CPU budget
NYC budget x1.15 for added LP + blend; HF-richer path may need 1-2 extra Newton iters. Per instance @48k: Normal 4x <=2.5%, Eco <=1.2%, HQ 8x <=5% of one 3 GHz core; `auto_os` 8x with wicker ON <=5%. Tone-OUT saves biquad (~-3%). Peak enable pre-roll <=8x steady once.

## 5 I/O & routing
- Hardware: 1 in, 1 out, mono. Plugin same as NYC-spec 5 (`stereo_mode`: dual_mono default | mono_sum).
- No loop/send/aux on unit. Board tricks (BOARD_CORE): series ahead/after delay, drive, EQ (order = chain order); parallel clean+Muff with board wet/dry; effects-loop feedback (delay/reverb out -> pre-Muff) with `fb_limit` hard clip +6 dBFS and DC block - essential here because Tone OUT + Wicker ON + high sustain self-oscillates (S3); stereo split two instances with different Wicker/Tone states (e.g. L = [IN,OFF] smooth, R = [OUT,ON] raw); stack two Muffs; Muff -> octave/reverb. Pedal-level feedback: "acoustic feedback" simulation optional `howl` (adv, default off): sidechain of output bandpassed 80-800 Hz at -50..-30 dB re-injected with delay 5 ms when Tone OUT and sustain>0.8; mimics S3 feedback; hard-limited, off in hardware-faithful preset.

## 6 MIDI / sync / expression (virtual; not on hardware)
- MIDI clock / tap / DAW sync: N/A (no time-based function) - state "none, same as hardware".
- CC map (ESTIMATED own allocation, per MIDI_EXPRESSION 4.3; channel `ref_board_midi_chan`): CC70 sustain, CC71 tone, CC72 volume, CC80 enabled (>=64 on; latch), CC81 tone_in (>=64 IN, <64 OUT), CC82 wicker (>=64 ON), CC11 -> expression target (default sustain). Soft-takeover ON for knobs; switches no takeover. 
- Program Change: board preset recall only (hardware has no memories).
- Expression target default `ehx_muff_wicker_sustain`, alt `volume`, alt `wicker_amount` (virtual continuous 0..1 mapped to w, NOT on hardware; label "virtual"; param `wicker_morph` below). Curve default log for sustain/volume, linear for wicker_morph.
- Footswitch gesture (FootController): note-on toggles `enabled` (latch) or momentary (held=ON, min hold 30 ms). Velocity -> bypass xfade ms lerp(12,2,speed). Mini-toggle keys: two learnable notes: Tone IN/OUT toggle, Wicker toggle (note-on flips; velocity ignored). Optional `softPress`: aftertouch (channel/poly, 0..1) -> wet/dry of slot while key held (virtual, default off). Aftertouch/mod wheel/pitch bend/sliders/knobs: any param via MIDI-learn.
- LED: follows `enabled`; 80 ms flash on MIDI-driven toggle.

## 7 Visuals (original vector art; no photo/logo copy)
- Enclosure: die-cast rectangle, brushed-chrome face (gradient #C9CDD2 -> #8E949B, 20% diagonal), 1.5 mm darker bevel. W 111 x D 121 mm face (same NYC-line chassis ESTIMATED), H 54 mm side view. Real unit colour/graphics not verified; use neutral silver with a red accent line #B3261E 1.2 mm under the title (ours, not EHX trade dress).
- Layout (mm, origin top-left, ESTIMATED): knobs row y=34: VOLUME x=27.5, TONE x=55.5, SUSTAIN x=83.5; dia 20, skirt 24, black plastic, white pointer 1.2x8, sweep -135..+135 deg, tick marks 0..10. Title "MUFF SUSTAINER" y=54 (6 mm cap), sub-line "TONE WICKER-STYLE" 3 mm y=59.
- Toggles (mini bat-handle, 6 mm bushing, chrome tip, handle ~8 mm): TONE switch centre (41.5, 72), labels "IN" above / "OUT" below; WICKER switch centre (69.5, 72), labels "ON" above / "OFF" below; caption under each "TONE", "WICKER" 3.5 mm cap, #151515. Handle animates 120 ms ease between up/down; lit switch-state is purely mechanical (no LED).
- LED red 5 mm dome (55.5, 14): on #FF2A2A + 6 mm glow, off #4A0A0A. Footswitch 12 mm chrome stomp (55.5, 98), 17 mm bezel (moved down to clear toggles).
- Side view only: INPUT/OUTPUT/DC jacks, non-clickable. Footprint 111x121 mm; mounting zones 4 corners (UI_BOARD_STYLES). `case_wear` 0..1 procedural, seeded.
- Text labels exact: "VOLUME","TONE","SUSTAIN","TONE","WICKER","IN","OUT","ON","OFF".

## 8 Parameter table
id | name | min | max | default | unit | skew | smoothing ms | automatable
---|---|---|---|---|---|---|---|---
ehx_muff_wicker_enabled | Footswitch | 0 | 1 | 0 | bool | - | xfade 5 | yes
ehx_muff_wicker_sustain | Sustain | 0 | 1 | 0.5 | norm (g(x) k=4.394) | audio | 20 | yes
ehx_muff_wicker_tone | Tone | 0 | 1 | 0.5 | norm linear pot | linear | 20 | yes
ehx_muff_wicker_volume | Volume | 0 | 1 | 0.5 | norm audio pot | audio | 20 | yes
ehx_muff_wicker_tone_in | Tone switch (1=IN) | 0 | 1 | 1 | bool | - | 10 | yes
ehx_muff_wicker_wicker | Wicker switch (1=ON) | 0 | 1 | 0 | bool | - | 10 | yes
ehx_muff_wicker_wicker_morph | Wicker morph (virtual) | 0 | 1 | 0 | norm | linear | 20 | yes (0 = follow switch; >0 overrides w = max(switch,morph))
ehx_muff_wicker_tone_out_trim_db | Tone-OUT trim (adv) | -12 | 6 | 0 | dB | linear | 20 | yes
ehx_muff_wicker_input_trim_db | Input trim (adv) | -12 | 12 | 0 | dB | linear | 20 | yes
ehx_muff_wicker_supply_v | Supply (adv) | 5 | 9.6 | 9.0 | V | linear | 50 | yes
ehx_muff_wicker_quality | Quality (adv) | 0 | 2 | 1 | enum eco2x/normal4x/hq8x | - | 0 rebuild | no
ehx_muff_wicker_auto_os | Auto OS on Wicker (adv) | 0 | 1 | 1 | bool | - | 0 | no
ehx_muff_wicker_stereo_mode | Stereo (adv) | 0 | 1 | 0 | enum dual_mono/mono_sum | - | 0 | no
ehx_muff_wicker_noise_enable | Noise (adv) | 0 | 1 | 0 | bool | - | 0 | no
ehx_muff_wicker_howl | Feedback howl (adv, virtual) | 0 | 1 | 0 | norm | linear | 50 | yes
ehx_muff_wicker_revision | Revision | 0 | 1 | 0 | enum (v1 locked 0) | - | 0 | no
ehx_muff_wicker_soft_press | Soft press mix | 0 | 1 | 0 | bool (virtual) | - | 5 | yes
ehx_muff_wicker_midi_chan | MIDI channel | 0 | 16 | 0 | int (0=omni) | - | 0 | no
Expression targets: sustain (default), volume, wicker_morph. Serialization: float norm + ints; missing key => default; unknown keys ignored. NYC-spec presets are not auto-imported (different model_id).

## 9 Tests / acceptance (P = provisional until MEASURED)
Reuse NYC-spec tests for the 4 switch states; deltas:
- T-STATES: all 4 [tone_in,wicker] states render finite, no DC (<1e-6 FS after 5 s silence), distinct spectra.
- T-FR (small signal 5 mV, sustain 0): state [IN,OFF] equals NYC-spec table +-1.0 dB (golden); [IN,ON]: above 1 kHz per stage gain increase, check tone=0.5 8 kHz level >= [IN,OFF] + 18 dB (P, ESTIMATED sec 3.2); [OUT,OFF]: flat within +-2 dB 200 Hz-700 Hz except Cf1/Cf2 slope, no notch (notch depth < 3 dB); [OUT,ON]: broadband, level at 8 kHz >= [OUT,OFF] + 20 dB (P).
- T-NOTCH: [IN,*] tone 0.5 notch 1017 Hz +-8%, -13.8 dB +-1.5 (OFF); with ON notch depth within -10..-14 dB.
- T-TONEOUT-LEVEL: 100 mV 1 kHz, sustain 0.5 volume 0.5: [OUT] level minus [IN,t=0.5] in 6..14 dB (P).
- T-THD (1 kHz, 100 mV, sustain 0.5): OFF 15-35% (P); ON high-order harmonic (H5-H15 sum) >= OFF + 6 dB (P); sustain 1.0 > 40%.
- T-ALIAS per sec 4.3; T-NULL: double-precision Python reference with same equations, residual < -90 dB at 48k/4x for each state.
- T-SWITCH-CLICK: toggle each switch during 1 kHz tone, sustain 0.8: peak |diff| in 10 ms ramp window < 0.1 FS above steady difference; no discontinuity > 0.05 FS between consecutive samples at 48k beyond signal slope (P). Rapid toggling 20 Hz 10 s: no NaN, no CPU spike > 2x mean.
- T-NOISE (noise_enable): ON-state noise floor (input shorted, sustain 1.0, vol 0.5) >= OFF-state + 10 dB (P, S3 qualitative).
- T-FEEDBACK: board loop with fb_limit, [OUT,ON], sustain 1.0, loop gain 0 dB: output bounded <= +6 dBFS, no NaN after 60 s.
- T-BYPASS/LAT/LEGACY/MIDI as NYC-spec; extra T-MIDI: CC81/CC82 toggles, mini-toggle notes flip once per note-on, velocity ignored; CC71 inert audibly when tone_in=0 but value retained.
- T-CPU per 4.5; 30 min random soak over all params and fs {44.1..192k}, blocks {1..512}: nanResets==0, newtonFallbacks < 0.01% samples.
- UI: both toggles hit-testable/keyboard (space flips), labels exact, snapshots 100%/150%.
- Fidelity score = weighted MEASURED pass; until hardware capture "model: unverified".

## 10 Known inaccuracies
- The three "high-frequency filters": identities (Cf1, Cf2, W3) are a reasoned GUESS from the generic Big Muff topology + retail wording; no schematic/trace verified. Wicker-ON voicing may differ materially.
- Tone-OUT level compensation unknown; trim param provided.
- All NYC-spec inaccuracies carry over (component values, Zin/Zout interaction, pot loading, hfe spread, supply sag, diode mismatch, noise, microphonics).
- Hardware feedback on Tone OUT is acoustic/electrical, modelled only by loop tricks or optional `howl`.
- Bypass type is DOCUMENTED true bypass; relay/3PDT and pop behaviour not modelled (xfade only).
- MIDI/expression/morph/softPress are plugin conveniences, not hardware.
- Not a loopers entry: no looper requirements apply.
