# boss-sd1 — "SUPER OverDrive" (inspired by Boss SD-1), dev-ready spec

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `boss-sd1`, **paramPrefix `boss_sd1_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/boss_sd1.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/boss_sd1_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_boss_sd1Tests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `boss_sd1_bypass`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims `boss_sd1_input_trim_db` -> `ref_s{nn}_in_db/out_db`; footswitch-mode params `boss_sd1_footsw_mode` -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `boss_sd1_quality`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): `boss_sd1_vel_to_level`, `boss_sd1_vel_to_drive`, `boss_sd1_at_target`, `boss_sd1_at_depth`, `boss_sd1_exp_target`.
> - Host lanes `ref_s{nn}_p01..` in this order: `boss_sd1_drive`, `boss_sd1_tone`, `boss_sd1_level`, `boss_sd1_diode_set`, `boss_sd1_lowcut_hz`, `boss_sd1_buffer_colour`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Version 1.1 | 2026-10-21 | Status: SPEC (verification pass done, see sec 11). Pedal id: `boss_sd1` (model_id `boss-sd1`, per models/boss-sd1.md).
Tags: [D]=DOCUMENTED (retailer spec copies of Boss spec sheet / Boss text), [M]=MEASURED (none available here), [E]=ESTIMATED (from memory of service schematic + community analyses; verify vs ref unit).
Research caveat (retry 2026-10-21): WebFetch to every host tried (electrosmash, electricdruid, barbarach, hobby-hour, freestompboxes, cdn.shopify manual, modulargrid, wikipedia, web.archive.org) failed (DNS/ blocked). Only WebSearch snippets reachable; they quote secondary sources (retailer spec copies, forum posts, Barbarach/hobby-hour text). No schematic image values were readable. Tag [S]=secondary snippet (forum/analysis text, not primary). Every circuit value marked [E] MUST be reconciled against the service schematic before fidelity claim. Do not claim "authentic".
Sources: 
- Boss product page https://www.boss.info/global/products/sd-1/ (not fetched; snippet only)
- Owner's manual PDF https://cdn.shopify.com/s/files/1/0523/4648/6935/files/Boss_SD-1_Super_Overdrive_Owner_s_Manual.pdf (not fetched)
- Schematic: https://www.hobby-hour.com/electronics/s/sd1-super-overdrive.php ; https://www.scribd.com/document/475021949/Boss-SD-1-2 ; https://atomiumamps.tumblr.com/post/133551441206/boss-sd-1-mods-notes-heres-the-service-diagram ; https://stompboxschematics.com/circuits/boss-super-overdrive-sd-1-project-by-gm-arts/
- Diode/mod notes: https://www.tdpri.com/threads/sd-1-clipping-diode-s.160348/ ; Wikipedia https://en.wikipedia.org/wiki/Boss_SD-1
- Waza Craft variant: https://www.freestompboxes.org/viewtopic.php?t=29267
- Spec listings (retailer copies of Boss spec): https://www.alamomusic.com/products/boss-sd-1w-waza-craft-super-overdrive-pedal ; https://modulargrid.net/p/boss-sd-1

## 1 Identity / variants
- Maker/series: Boss (Roland) compact pedal, overdrive. Introduced 1981 [D]. Evolved from OD-1 (same asymmetric clipping core) + added active TONE and more low-end body [D, Wikipedia/Mixdown].
- Character [D]: asymmetrical clipping, even+odd harmonics; "tube-like" mid-forward OD; classic stacking-in-front-of-amp use.
- Variants (catalogue `revision` field; only `sd1-std` is modelled in v1, others are OMIT/unverified):
  - `sd1-std` (DEFAULT): current production SD-1. Electronic (buffered) bypass.
  - `sd1-vintage`: OMIT. No source shows a measurable difference (hobby-hour lists uPC4558C for the current board). Alias of `sd1-std`.
  - `sd1w-waza` (SD-1W Waza Craft, 15 mA [D]): has CUSTOM mode. OMIT: circuit undocumented here (freestompboxes thread t=29267 not readable). Plugin does nothing for it; no `sd1_mode` param.
  - Mod diode set is a user option `boss_sd1_diode_set` (sec 3.3), not a hardware variant.
- Not variants: OD-1 (no TONE), OS-2, BD-2.
- Naming: UI shows "SUPER OVERDRIVE"; never Boss/Roland logo or photo. Descriptor text "inspired by Boss SD-1".

## 2 Controls (physical, left to right on face)
| Control | Real | Range | Taper | Default | Behaviour |
|---|---|---|---|---|---|
| LEVEL | pot [S 10 kΩ (forum: SD-1 10k vs TS 100k); taper E audio] | 0..10 (0-1 norm) | audio (log-ish) | 5 (0.50) | Post-tone output volume. Unity ~ 0.5..0.6 [E]; max ≈ +14 dB vs dry [E]. |
| TONE | pot [E 20k-25k B (linear)] | 0..10 | linear | 5 (0.50) | Treble contour of active tone stage; centre = flat-ish presence; CCW darker, CW brighter. Not a cut-only LPF. |
| DRIVE | pot [S 1 MΩ linear (forum: SD-1 1M vs TS 500k); linear taper S: 'SD-1 linear, TS808 log'] | 0..10 | see §8 | 3 (0.30) | Sets op-amp feedback R. Min ≈ clean boost (+~4 dB), max ≈ +~36 dB local gain [E]. Asymmetric clip threshold hit progressively. |
| Footswitch (stomp) | latching SPST electronic [D] | on/off | - | OFF | Toggles effect via electronic switching (buffered bypass). Click-free. |
| CHECK LED | red LED | on/off | - | off | Lit = effect ON. Also dims/flashes low at low battery (not emulated; optional `sd1_battery_sag` v2). |
| INPUT jack | 1/4" mono | - | - | - | Z_in 1 MΩ effect ON [D]; (some listings 470k/1M: effect-off vs on) [D]. Plugging in connects battery. |
| OUTPUT jack | 1/4" mono | - | - | - | Z_out 1 kΩ [D], recommended load ≥10 kΩ [D]. |
| DC IN | 9 V DC, centre-negative, barrel (PSA) | - | - | - | 9 V; battery 9V 6F22/6LR61 optional [D]. Current: ModularGrid/Alamo list 10 mA [D]; Music Matter 6 mA, Fair Deal 4 mA - retailer conflict; use 10 mA, no DSP effect. |

Notes:
- Pot order LEVEL, TONE, DRIVE (left to right) [E from memory of the unit; no source reached].
- Pot ranges are 0..10 printed; UI value stored 0..1 normalized.
- No other switches/jacks exist (no expression, no MIDI, no loop). Anything beyond = virtual-board extras (§5/§6), flagged "VIRTUAL — not on hardware".

## 3 Signal path & circuit/DSP model
Order [E, per service diagram]: 
`IN → (bypass relay/switch network; input buffer Q1 emitter follower) → C coupling → Gain stage (IC1A, non-inv) with asymmetric diode clipper in feedback → Active tone stage (IC1B) → LEVEL → Output buffer (Q2 emitter follower) → OUT`.
IC [E]: dual op-amp uPC4558C / JRC4558 class (GBW ≈ 3 MHz, slew ≈ 1.7 V/µs, rails 0..9 V, bias at 4.5 V virtual ground).

### 3.1 Input buffer
- Z_in ≈ 1 MΩ [D]; coupling HPF ≈ 5-8 Hz [E] (inaudible; implement 1-pole HPF 7 Hz).
- Pickup/cable loading not modelled (plugin cannot). Optional `sd1_input_loading` (v2).

### 3.2 Gain stage (the voice) [E values]
Non-inverting: G(f) = 1 + Zf(f)/Zg(f).
- Zg: R_g = 4.7 kΩ [E] in series with C_g. One mod write-up (electric-safari) states stock C3 = 47 nF [S] (mod to 100 nF for more bass), so stock C_g = 47 nF => f_hp = 720 Hz [S+E]. Default `boss_sd1_lowcut_hz` = 720 (was 500 E); range 340..720 kept for mod/user. 
- Zf: parallel( R_f(Drive) , C_f ) with R_f = R_fix + P_drive(0..1 MΩ [S]); R_fix ≈ 27 kΩ [E, derived below]; C_f ≈ 100 pF [E] → LPF corner f_lp = 1/(2π·R_f·C_f): ~6 kHz at max drive … > 30 kHz at min. 
- Gain targets [S, Barbarach text]: SD-1 min ≈ 17 dB, max ≈ 47 dB (TS808 21/41 dB). Fit [E]: R_g = 4.7k, 1+R_fix/4.7k = 7.1 => R_fix ≈ 27 kΩ; max 1+(27k+1M)/4.7k ≈ 220 (46.8 dB) with 1 MΩ pot - consistent with [S] 47 dB and the 1 MΩ pot. Op-amp GBW (3 MHz) / 220 limits HF to ~14 kHz at max; model with finite-GBW 1-pole. Below f_hp gain falls toward 1 (0 dB). Supersedes earlier 41 dB / 500k figures.
- Pot→gain mapping is the biggest uncertainty. Calibrate: reference unit sine sweep at DRIVE 0/25/50/75/100%; tune `P_drive` (linear taper per [S]) to match.

### 3.3 Asymmetric clipper (in op-amp feedback, parallel with R_f/C_f)
- Topology [S, hobby-hour + forum]: 3 silicon 1S2473 switching diodes in the feedback path: two in one direction, one in the other (which side is the pair is not readable from snippets; model: single side clips first, polarity const). V_single ≈ 0.6 V [E]; V_double ≈ 1.2-1.3 V [E]. 
- So clip at ≈ +0.65 V (one polarity) vs ≈ −1.3 V (other) at the op-amp output, referenced to bias. Hence asymmetric waveform → 2nd harmonic + odd. Polarity: single-diode side clips first; mark `sd1_clip_polarity` const (do not expose; flip = 180° test).
- Model: Shockley, i = Is·(exp(v/(n·Vt)) − 1). 1S2473 SPICE params unpublished; use 1N4148-class values (same small-signal silicon switching class) [E]. Table:
  - `stock_1s2473`: Is=2.52e-9, n=1.752 (1N4148 proxy; Vf≈0.62@1mA) [E]
  - `1n4148`: Is=2.52e-9, n=1.752 [DOCUMENTED SPICE default 1N4148]
  - `1n34a` (germanium mod): Is=2e-7, n=1.3 (Vf≈0.25) [E]
  - `led_red`: Is=1e-12, n=2 (Vf≈1.7) [E]
- Op-amp saturation: soft clip to rails ±4.2 V (9 V single supply - headroom [E; 4558 swing ~±3.5-4 V on 9 V, uncertain]) via tanh(x/4.2)·4.2; applied after clipper (rarely hit).

### 3.4 Active tone stage (IC1B) [E]
- [S] Tone section nearly identical to Tube Screamer apart from component values and an added cap; one mod write-up gives stock C6 = 10 nF in the tone op-amp feedback [S, mod text - ambiguous], R6 mod from stock unknown. Exact corners unreadable => all numbers below are [E] behavioural fit, not circuit-derived. Model as:
  - H_tone(s) = (1 + s/ω_z)/(1 + s/ω_p), with ω_z, ω_p moving with TONE:
  - TONE=0: shelf −9 dB above ≈ 1.2 kHz [E]; TONE=0.5: ≈ flat ±1 dB, mild +2 dB @2.5 kHz [E]; TONE=1: +9 dB shelf above ≈ 1.8 kHz [E].
  - Implement as 1st-order shelving filter, gain_dB = (tone−0.5)·18, corner f_c = 1.2 kHz + tone·0.8 kHz; plus fixed 1-pole LPF 12 kHz (op-amp/C) [E].
- Output: LEVEL pot then Q2 buffer; Z_out 1 kΩ (ignore, or optional 1st-order RC with C_load 0).

### 3.5 Output
- Out coupling HPF ≈ 15 Hz [E] (1-pole). 
- Level taper audio: gain_dB(L) = 20·log10(L^2.0)·… use gain = (exp(k·L)−1)/(exp(k)−1) k=3; scaled so L=0.55 ⇒ 0 dB makeup relative post-tone; max +14 dB [E].

### 3.6 Latency, bypass
- Latency: 0 samples without oversampling-linear-phase; with minimum-phase/IIR halfband OS: 0 reported latency (use IIR polyphase halfbands, group delay ≈ 1.5 samples @ base rate ignored); FIR variant `sd1_hq`: report latency via getLatencySamples (≤ 64 @ 1×). Default IIR, 0 latency, report 0.
- Bypass: **buffered bypass** [D: Boss electronic switching]. When OFF, signal still passes the input buffer + output buffer: tiny LF/HF colouring: output HPF 15 Hz, LPF 40 kHz (inaudible), Z_out 1 kΩ. Model as unity with those two 1-poles (cheap) — keep behind `sd1_buffer_colour` default on.
- Transition: Boss uses FET/electronic switching with ~ms fade. Implement equal-power-less crossfade of wet/dry over 5 ms (raised-cosine), sample-accurate toggle; no click. Footswitch state change must reach DSP via atomic float 0/1 target; smoother reads per sample.
- Gain compensation: none (real unit has no automatic make-up).

## 4 Algorithm for plugin
### 4.1 Block diagram (per channel; board mono→ stereo handled in §5)
```
x → HPF_in(7Hz) → [pre-emph?no] → ×OS(4×) →
   GainStage: y = clipper-feedback solve (see 4.2) 
 → tone shelf → LPF(12k) → ÷OS → level → HPF_out(15Hz) → mix(bypass xfade) → out
```
### 4.2 Gain stage per sample (4× OS rate fs_o)
Op-amp feedback with diodes parallel R_f∥C_f. State-space (ZDF/trapezoid):
- Define v_in (non-inv input, post-HPF), v_g = voltage across Zg network, i_f feedback current.
- Non-inverting amp: v_out = v_in + v_f where v_f is voltage across feedback network, current i_f = v_in-leg current: i_g = G_g(v_in) via series R_g–C_g (+ fixed leg). Trapezoid for C_g: i_g = (v_in − v_cg)/(R_g) ; update v_cg. 
- Feedback node: i_f = i_g = v_f/R_f + C_f·dv_f/dt + i_D(v_f), with i_D = Is·(exp(v_f/nVt) − exp(−v_f/nVt·?)) asymmetric: i_D(v) = Is·(e^{v/nVt} − 1) − Is·(e^{−v/(2nVt)} − 1)·?? Use two branches: forward single: i1 = Is(e^{v/nVt}−1); reverse stack of two diodes: i2 = Is(e^{−v/(2nVt)}−1) (series diodes → half voltage each); i_D = i1 − i2.
- Solve v_f by Newton: f(v)= C_f·2/T(v−v_prev) − ... + v/R_f + i_D(v) − i_g = 0; initial guess previous v_f; max 4 iterations; clamp exponent arg ≤ 40; fall back to bisection on failure; deterministic.
- Optional cheaper (CPU_LOW): ADAA/waveshaper with Lambert-W closed form for symmetric case; asymmetric needs LUT: build 2-D? NO: use per-sample 1-D LUT of v_f(i_net) at prepare() (4096 entries, cubic interp) for fixed R_f? R_f varies with DRIVE → rebuild LUT on message thread when drive stable (>10 ms) and crossfade; keep Newton as primary (`sd1_quality`=`high`) and LUT as `eco`.
- v_out = v_in + v_f; soft rail via 4.2·tanh(v_out/4.2).
### 4.3 Oversampling / aliasing
- 4× OS (2× two-stage half-band IIR polyphase 12th order, > 90 dB stopband) for `high`; 2× for `eco`; 1× + ADAA(1st-order) for `lowcpu`. Base fs ≥ 88.2 kHz → OS factor 2 → 1 automatically (target fs_o ≥ 176.4 kHz).
- Aliasing target: all aliased components ≥ 60 dB below carrier for a 1 kHz sine at DRIVE=100%, +0 dBu in, 44.1 kHz, `high`.
### 4.4 Smoothing & zipper
- DRIVE: smooth in R_f domain, 1-pole 20 ms (coefficient exp(−1/(0.020·fs))).
- TONE: 20 ms. LEVEL: 10 ms (gain domain). Footswitch xfade 5 ms. Param thread → atomic<float> target; audio loads once per block, smoother per sample.
### 4.5 Denormals, safety
- ScopedNoDenormals + add 1e-18 flush on state vars; clamp NaN/inf: if !isfinite(y) { reset states; y=0 }. Clamp input ±8 V-eq.
- Calibration: 0 dBFS = +10 dBu? Choose `sd1_ref_dbu` default: −20 dBu nominal [D] ≡ −18 dBFS RMS (0.125) ⇒ 1.0 FS = 8 × 0.3 V? Define volts_per_FS = 1.0 V peak... default: 1.0 FS = 0.775 V·10^(+?): use FS ≡ ±2.0 V peak (≈ +9 dBu pk); −20 dBu RMS (77.5 mV rms,110 mV pk) ⇒ −25.2 dBFS peak. Expose `sd1_input_trim_db` −24..+24 dB (default 0). 
### 4.6 CPU budget (per instance, 48 kHz, 4× OS, stereo)
- Target ≤ 1.2% of one core @ 3 GHz; `eco` ≤ 0.5%; `lowcpu` ≤ 0.15%. Fail test if > 2× target. Newton avg ≤ 2.5 iters.
- Memory: states ≤ 2 KB; LUT 64 KB shared per prepare (not per instance).
- RT safety: no alloc/locks/logging in `process`; prepare() allocates OS buffers & LUT; quality change via message thread builds new state then atomic swap with 10 ms crossfade.

## 5 I/O & routing
- Hardware: mono in/out. Virtual: stereo-capable board bus; modes `sd1_stereo_mode`: `dual_mono` (default: 2 independent instances' state, shared params), `sum_in_mono_out` (L+R → mono → both), `left_only`. No stereo link, no other jacks.
- Tricks users can do on the virtual board (all supported by BOARD_CORE graph, not by pedal itself):
  - Series stacking (two SD-1: drive 0 + level boost into amp / second pedal) – works with state independent per instance ID.
  - Parallel with clean dry (via board universal wet/dry). `sd1` has NO mix knob; the board wet/dry (pedal-level `ref_board_mix`) is the only mixing control.
  - Feedback loop: via board effects-loop (send/return) — SD-1 can sit inside another pedal’s loop; feedback path must have ≥ 1 block delay + limiter (BOARD_CORE rule); document: SD-1 in a delay's feedback path gives "overdriven repeats" (requires delay with loop jacks).
  - Boost mode recipe (DRIVE=0, LEVEL=high, TONE=mid) = classic clean/mid boost into amp. Preset `sd1_boost` shipped.
  - Stack OD-into-SD-1 as in TS style: preset `sd1_ts_booster`.
- Bypass-state routing: when OFF stays in chain (buffered) so signal order unchanged.

## 6 MIDI / sync / expression
Hardware has NONE. All below is VIRTUAL, flagged "added by Luthier; not on hardware". Comparable-fidelity rule: emulate what a MIDI-capable OD/boost (e.g. Strymon/Source Audio-style) would offer, no more. Implementation via `specs/MIDI_EXPRESSION.md`; this section lists pedal-specific map.
- CC map (default, channel configurable, all 0..127→0..1):
  | CC | target | note |
  |---|---|---|
  | 14 | `sd1_drive` | |
  | 15 | `sd1_tone` | |
  | 16 | `sd1_level` | |
  | 17 | `sd1_bypass` | ≥64 = effect ON (matches Boss/Strymon convention) |
  | 18 | (unused) | OMIT: no CUSTOM mode |
  | 100/101 | expression target select (see below) | optional |
- Program change: recalls pedal snapshot slot 0..127 (stored in board preset), 5 ms parameter ramp, no click.
- MIDI clock / tap tempo: **not applicable** (no time-based effect). Ignore Start/Stop/Clock. Do NOT add fake sync.
- Expression pedal (virtual EXP): default target `sd1_drive`; selectable targets: drive | tone | level (one at a time) with HEEL/TOE range min..max (each 0..1 default 0..1), linear. Resolution 14-bit (CC 11 + 43 pair) or 7-bit with 20 ms smooth.
- Footswitch gestures (virtual stomp via UI/MIDI note):
  - Note-on (any key / assigned): toggle on press; **latching** (default) or **momentary** (`sd1_footsw_mode`): momentary = ON while key held, OFF on release (also: press<250 ms → latch toggle, hold ≥250 ms → momentary behaviour; Boss "momentary" convention [E]).
  - Velocity → LEVEL kick: `sd1_vel_to_level` (-12..+12 dB, default 0): level offset ∝ (vel/127 − 0.5)·depth, applies for the duration of the note (momentary boost, smoothed 10 ms attack/ 80 ms release).
  - Velocity → DRIVE: `sd1_vel_to_drive` (−1..+1, default 0): ±depth × (vel/127 − 0.5)·2 added to DRIVE (clamped 0..1).
  - Channel/poly aftertouch → TONE or DRIVE: `sd1_at_target` {off, drive, tone, level}, `sd1_at_depth` −1..+1, default off; smoothed 15 ms, deadband 2/127.
  - Hard-press detection: velocity ≥ 100 → instant switch (no ramp, 1 ms xfade); velocity <40 → slow 30 ms xfade ("soft press"). Documented-comparable, not on hardware.
- Mapping to sliders/knobs: MIDI learn on any of drive/tone/level/bypass; 14-bit NRPN optional; relative encoders supported (twos-complement).
- Priority: UI > MIDI learn > expression > aftertouch > velocity; combined offsets sum then clamp.

## 7 Visuals
Original vector art; do not trace Boss artwork. Compact-pedal proportions (real: W 73 × D 129 × H 59 mm [D, retailer listings/Boss]; weight 360 g incl. battery [D, Zedem] vs 400 g [D, Music Matter]; 410 g earlier figure UNSOURCED, dropped).
- Enclosure: die-cast shell, **yellow** `#F4D21F` base (E), matte; black plastic top-plate features absent; silver label strip top.
- Face (portrait 73 × 129 mm, scale 4 px/mm = 292×516):
  - Top zone: three knobs in one row centred y = 19 mm from top; centres x = 15.5, 36.5, 57.5 mm (E). Order L→R: LEVEL, TONE, DRIVE. Knob Ø 16 mm, black skirt, white indicator line, pointer-style; legend arcs 0–10 (7 pt) around each at radius 11.5 mm, sweep 7 o'clock to 5 o'clock (−135°..+135°).
  - LED: red Ø 3.5 mm at (36.5, 38) mm; glow on state; label "CHECK" 6 pt above-left (optional).
  - Footswitch: oversized rubber stomp plate 63 × 62 mm region from y = 47..109 mm, charcoal `#2B2B2B`, rectangular w/ rounded 4 mm corners, with fine ribbed lines; pressed travel visual −2 mm.
  - Labels: "SD-1" large black stencil sans on yellow band (bottom left, height 9 mm), "SUPER OverDrive" under/ beside (text 7 pt); small "inspired by Boss SD-1 – independent model" credit in info panel not face.
  - Jacks: INPUT right side, OUTPUT left side, DC IN top (real: side jacks); in 2-D drawer show small 6 mm circles at left/right edge y = 20 mm, labelled in info.
- Interactive: knob drag vertical 200 px = full sweep, shift = fine (×0.1), double-click reset to default, mouse wheel ±0.02, keyboard arrows ±0.01 (shift ±0.1); footswitch click toggles.
- Dark/light: static art, glow LED `#FF2A2A`; accessibility labels "Level", "Tone", "Drive", "Effect on/off".
- Face: `Source/UI/ReferencePedals/Faces/boss_sd1_face.cpp` (procedural `juce::Path`; the draft `.svg` asset is VOID, UI_BOARD_STYLES sec 0 'no bitmaps/assets').

## 8 Parameter table
IDs prefixed `boss_sd1_`. Normalized 0..1 for APVTS; ranges physical shown.
| id | name | min | max | default | unit | skew/taper | smooth ms | automatable |
|---|---|---|---|---|---|---|---|---|
| boss_sd1_drive | Drive | 0 | 10 | 3 | knob | R_f = 27k + 1 MΩ·x, linear taper [S pot 1M linear; R_fix E] | 20 | yes |
| boss_sd1_tone | Tone | 0 | 10 | 5 | knob | linear | 20 | yes |
| boss_sd1_level | Level | 0 | 10 | 5 | knob | audio: g=(e^{3x}−1)/(e^3−1) scaled | 10 | yes |
| boss_sd1_bypass | Effect on | 0 | 1 | 0 | bool | step | 5 (xfade) | yes |
| boss_sd1_quality | Quality | 0 | 2 | 1 | enum 0=lowcpu, 1=high, 2=eco | step | - | no (message thread) |
| boss_sd1_diode_set | Diode set | 0 | 3 | 0 | enum 0=stock_1s2473, 1=1n4148, 2=germanium, 3=led (mod options, not hardware) | step | 50 (xfade) | yes |
| boss_sd1_lowcut_hz | Low-cut (internal) | 340 | 720 | 720 | Hz | log | 20 | no (advanced overlay only) |
| boss_sd1_input_trim_db | Input trim | −24 | 24 | 0 | dB | linear | 10 | yes |
| boss_sd1_buffer_colour | Buffer colour | 0 | 1 | 1 | bool | step | - | no |
| boss_sd1_footsw_mode | Footswitch mode | 0 | 1 | 0 | latch/momentary | step | - | no |
| boss_sd1_vel_to_level | Vel→Level | −12 | 12 | 0 | dB | linear | 10/80 | yes |
| boss_sd1_vel_to_drive | Vel→Drive | −1 | 1 | 0 | norm | linear | 15 | yes |
| boss_sd1_at_target | Aftertouch target | 0 | 3 | 0 | enum off/drive/tone/level | step | - | no |
| boss_sd1_at_depth | Aftertouch depth | −1 | 1 | 0 | norm | linear | 15 | yes |
| boss_sd1_exp_target | EXP target | 0 | 2 | 0 | enum drive/tone/level | step | - | no |
| boss_sd1_exp_min | EXP min | 0 | 1 | 0 | norm | linear | 20 | yes |
| boss_sd1_exp_max | EXP max | 0 | 1 | 1 | norm | linear | 20 | yes |
Preset/state: store all of the above + `revision` string + schema version; unknown fields preserved. Advanced rows (low-cut, diode_set, buffer, trim) hidden under Advanced overlay, never drawn as physical knobs.

## 9 Tests / acceptance
Thresholds PROVISIONAL until reference unit measured; fix after capture. Each test runs deterministic (noise off) at 44.1/48/96 kHz, blocks 1/4/16/64/512/2048, non-pow2 e.g. 37.
1. Bypass null: effect OFF + `buffer_colour` off ⇒ output == input (≤ −120 dBFS error). With colour on: |gain| within ±0.1 dB 40 Hz-15 kHz.
2. On/off click: toggle at zero-crossing and at peak, max sample step ≤ 0.1 FS for −12 dBFS 440 Hz.
3. Frequency response (small signal −40 dBFS sweep) checkpoints, DRIVE=0 / TONE 0,0.5,1 [E targets, ±3 dB until ref]: TONE=0.5, DRIVE=0.3: gain @100 Hz ≈ +10 dB below @1 kHz? → flat low gain (≈ +21 dB at ≥ 1 kHz vs ≈ 0 dB at 50 Hz) — check monotone rise 50 Hz→500 Hz; −3 dB point of low-cut ≈ f_lowcut ±25%; HF: TONE=1 ≥ +6 dB @4 kHz vs TONE=0.5; TONE=0 ≤ −6 dB @4 kHz vs TONE=0.5.
4. Level monotone: LEVEL steps 0..1 (21 points) strictly increasing, LEVEL=0 ≤ −60 dB, mid ≈ unity ±3 dB (when DRIVE=0.0, TONE=0.5, steady 1 kHz −20 dBFS, post-OD).
5. Drive monotone: RMS out of 1 kHz −30 dBFS sine at LEVEL fixed rises monotonically with DRIVE.
6. Harmonics (1 kHz sine, −24 dBFS-equiv −20 dBu): THD+N at DRIVE 0/0.25/0.5/0.75/1: expected ranges [E]: 0.5–2 % / 3–8 % / 8–20 % / 15–30 % / 20–40 %. Asymmetry: H2 − H3 ≥ −6 dB (H2 within 6 dB of H3) for DRIVE ≥ 0.5; H2 present ≥ −30 dBc. Polarity flip changes sign of DC offset; DC after output HPF < −70 dBFS.
7. Two-tone IMD (700 Hz+1100 Hz): IMD3 raises with drive; no NaN.
8. Aliasing: 1 kHz & 7 kHz sine DRIVE 1.0, 44.1 kHz `high`: highest non-harmonic peak ≤ −60 dBc; `eco` ≤ −50; `lowcpu` ≤ −40 (documented limit).
9. Step/pluck: step response settles ≤ 20 ms; no ringing > 3 dB above steady state.
10. Param stress: random param jumps 10 000×, no zipper > −60 dB spurs; no NaN/inf over 30 min soak; no allocs (RT sanitizer / malloc hook).
11. SR invariance: output spectra at 44.1 vs 96 kHz within 1.5 dB 100 Hz–10 kHz.
12. MIDI: CC 14-17 move params; PC recalls; velocity/aftertouch mapping depth ±; expression min/max inverted works; save/restore round trips; clock messages ignored without error.
13. Diode set equivalents: `germanium` clips at lower level (Vclip −8 dB vs stock), `led` higher (+6 dB).
14. UI: all 3 knobs + footswitch reachable by keyboard; no hidden controls; face matches layout mm.
15. CPU: §4.6 budgets.
Reference null-tests (when unit available): ref capture DI sweep grid 0/25/50/75/100 % ×3 pots ×4 levels; error metric RMS spectral envelope ≤ 2 dB 100 Hz–8 kHz target; thresholds finalised from observed unit-to-unit variation (≥ 2 units).

## 10 Known inaccuracies / open questions
- Circuit R/C values still mostly [E]: R_g, R_fix, C_f, tone corners, buffer coloration (primary schematic not readable this pass). C_g 47 nF, pot 1M, gains 17/47 dB are [S]. Must be verified against service schematic (links in sources) before fidelity-level claim.
- Drive-pot value: [S] 1 MΩ linear (forum) fits the 47 dB max-gain figure; earlier 500k was TS value. Ship as parametric table `Sd1CircuitParams` in `Source/ReferencePedals/Models/boss_sd1_params.h`.
- Diode part (1S2473) spice params not published; approximated.
- Op-amp slew/rail interaction, 9 V sag/supply ripple, PSU noise, battery mode: not modelled (v2: `sd1_battery_sag`).
- Input loading by pickup impedance not modelled; transistor buffer coloration approximated by 1-poles.
- SD-1W CUSTOM mode: OMIT (undocumented here); no param.
- Current draw conflicting (4/6/10 mA across retailers) irrelevant for DSP.
- Pot tolerance/unit variance, noise floor (≈ −85 dBu typ [E]) are not reproduced; optional noise with fixed seed `sd1_noise` default OFF.
- MIDI/expression/velocity features are virtual additions, not hardware.
- Looper section: N/A for SD-1 (not a looper).

## 11 Verification log
Retry 2026-10-21. Reached (WebSearch snippets only; all WebFetch/archive.org blocked): hobby-hour.com, barbarach.com, electric-safari.com, forum.seymourduncan.com, alamomusic.com, modulargrid.net, zedem.ca, musicmatter.co.uk, fairdealmusic.co.uk. Not reached: Boss manual PDF, electrosmash, electricdruid, scribd, freestompboxes, wikipedia. Status key: DOCUMENTED (retailer/Boss spec copy), SOURCED-SECONDARY (forum/analysis snippet, not primary), ESTIMATED, OMIT.
| # | Item | Old -> New | Status | Source |
|---|---|---|---|---|
| 1 | Drive pot | 500k B [E] -> 1 MΩ linear | SOURCED-SECONDARY | forum.seymourduncan.com/threads/is-the-ts7-a-good-pedal (snippet), diystompboxes thread; fits item 3 |
| 2 | Level pot | 100k A [E] -> 10 kΩ (taper still E audio) | SOURCED-SECONDARY | same forum snippet; low confidence |
| 3 | Gain range | 21.5-41 dB [E] -> 17-47 dB | SOURCED-SECONDARY | barbarach.com/tube-screamer-like-pedals (snippet) |
| 4 | R_fix | 51k [E] -> 27k | ESTIMATED | derived: 1+R/4.7k=7.1 (17 dB) |
| 5 | R_g | 4.7k | ESTIMATED | TS-class topology, consistent with fit |
| 6 | C_g / lowcut default | 100nF / 500 Hz [E] -> 47 nF / 720 Hz | SOURCED-SECONDARY | electric-safari.com mod post (stock C3 47nF -> 100nF) |
| 7 | C_f | 51-100 pF -> 100 pF | ESTIMATED | TS-class; unverified |
| 8 | Clipper | 3 Si diodes asymmetric | DOCUMENTED (hobby-hour list: 1S2473 D3-D8; 3 diodes, 2 one way 1 other) | hobby-hour.com |
| 9 | Diode SPICE | n/a (1S2473 unpublished) -> 1N4148 proxy | ESTIMATED | datasheet class |
| 10 | Op-amp | uPC4558C | DOCUMENTED | hobby-hour.com |
| 11 | Tone-stage corners/shelf dB | unchanged | ESTIMATED | only: near-TS tone stage, stock C6 10nF (mod text, ambiguous) |
| 12 | Z_in 1 MΩ, nominal -20 dBu, Z_out 1 kΩ, load >=10k | unchanged | DOCUMENTED | alamomusic.com, modulargrid.net |
| 13 | Current | 4 vs 10 mA -> 10 mA (4/6/10 conflict recorded) | DOCUMENTED (conflict) | modulargrid, alamomusic, musicmatter, fairdealmusic |
| 14 | Dimensions 73x129x59 mm | unchanged | DOCUMENTED | zedem.ca, fairdealmusic |
| 15 | Weight | 410 g -> 360-400 g (410 unsourced) | DOCUMENTED | zedem.ca, musicmatter.co.uk |
| 16 | 1981 introduction | unchanged | DOCUMENTED (carried from earlier, wikipedia not re-reached) | - |
| 17 | `sd1-vintage` variant | modelled -> removed | OMIT | no source of difference |
| 18 | SD-1W CUSTOM mode, `sd1_mode`, CC 18 | reserved -> removed | OMIT | undocumented; plugin does nothing |
| 19 | Waza 15 mA | unchanged | DOCUMENTED | alamomusic.com |
| 20 | MIDI/expression/velocity/aftertouch | virtual | OMIT as hardware; kept as flagged virtual PF_Ext extras (no hardware chart exists) | modulargrid/retail: no MIDI |
| 21 | Messy rows fixed | '??' in Shockley/led rows, drive taper row, quality enum, exp_min/_max merged row, 'sd1-4a' pseudo-variant | cleaned | this file |
| 22 | Noise floor -85 dBu, buffer 15 Hz/40 kHz poles, 7 Hz input HPF, rail +-4.2 V | unchanged | ESTIMATED | no source |
Counts: upgraded 7 (items 1,2,3,6,8,10 + weight 15), kept-estimate ~8 groups, omitted 3 (17,18,20).
