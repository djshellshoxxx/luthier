# boss-ds1 : Distortion pedal ("inspired by BOSS DS-1") - dev-ready spec

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `boss-ds1`, **paramPrefix `boss_ds1_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/boss_ds1.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/boss_ds1_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_boss_ds1Tests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `boss_ds1_on`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims `boss_ds1_input_trim_db`, `boss_ds1_output_trim_db` -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `boss_ds1_hq`, `boss_ds1_stereo_mode`, `boss_ds1_revision`, `boss_ds1_noise`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): (none).
> - Host lanes `ref_s{nn}_p01..` in this order: `boss_ds1_dist`, `boss_ds1_tone`, `boss_ds1_level`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Version 0.1 | 2026-10-21 | model_id `boss-ds1` | param prefix `boss_ds1_` | status SPEC
Evidence tags: DOCUMENTED (manual/schematic analysis), MEASURED (none yet), ESTIMATED (derived / from memory, must be calibrated).
Research note (rev 2026-10-21b): direct page fetch still blocked in this sandbox (DNS failure for electrosmash.com, rarebuzzer.com, thermionic-studios.com; archive.org denied by tool). Only WebSearch result summaries were reachable. Values tagged "DOCUMENTED (secondary)" come from those search summaries of ElectroSmash/manual spec tables, not from the primary page. See section 11 for the item-by-item log. Every remaining ESTIMATED value needs verification against the schematic before release.

Sources
- S1 ElectroSmash analysis: https://www.electrosmash.com/boss-ds1-analysis (stages: input buffer, transistor booster, op-amp gain, tone/level, output buffer; C5-R10 HPF 23 Hz; booster ~35 dB per secondary summary)
- S2 BOSS product page: https://www.boss.info/global/products/ds-1/ ; owner's manual PDF mirror: https://www.strumentimusicali.net/manuali/BOSS_DS1_ENG.pdf
- S3 MUMT618 DS-1 model (DIST network Rt=100k*D, Rb=(1-D)*100k+4.7k, Cz=1uF, Cc=250pF): https://napulen.github.io/reports/mcgill/mumt618/
- S4 DIY mod write-up (tone LPF 234 Hz, HPF 3290 Hz): Premier Guitar "Boss DS-1 Mods" https://www.premierguitar.com/diy/pedal-projects/boss-ds-1-mods
- S5 MusicRadar FX files (op-amp stage ~25 dB max, tone dip ~500 Hz): https://www.musicradar.com/how-to/the-fx-files-boss-ds-1
- S6 schematic/PCB: https://www.hobby-hour.com/electronics/s/boss-ds1-distortion.php ; Tonepad DS1 https://stompboxschematics.com/circuits/boss-ds1-by-tonepad/
- S7 Wikipedia history: https://en.wikipedia.org/wiki/Boss_DS-1

## 1 Identity / variants
- Real unit: BOSS DS-1 Distortion, launched 1978, mono, analog, 9 V DC (PSA adaptor) or 9 V battery (006P). No MIDI, no expression jack, no sync. DOCUMENTED.
- Variants: (a) 1978-1993 TA7136AP preamp IC version (Japan, "Made in Japan" silver-label era): different clipping/tone feel; (b) 1994+ discrete-transistor + op-amp "modern" revision (Taiwan since ~2000): default model; (c) 40th-anniversary and other special editions: OMIT (unverified, not modelled, no product copy); (d) DS-1X (DSP version): OMIT. Op-amp history per Wikipedia/ElectroSmash search summaries: original 1978 used Toshiba TA7136AP (a preamp IC, not an op-amp); later units Rohm op-amp, then Mitsubishi M5223AL from ~2000; transistors Q1/Q2 2SC2240 (Thermionic Studios table, later revision). Variant selector `boss_ds1_revision` enum {0=modern (default), 1=ta7136_1978}. Rev 1 is OMIT (no credible topology data): v1 ships rev 0 only, selector hidden and ignored.
- Display name must read "Distortion (DS-1 style)"; no trademark logos; original vector art only.

## 2 Controls
| Control | Range / positions | Taper | Default | Real behaviour |
|---|---|---|---|---|
| DIST (knob) | 0..10 (UI 0..1) | audio 'A' 100k, DOCUMENTED-ish. use x^2.0 UI->pot position (ESTIMATED) | 0.5 | sets op-amp non-inverting gain 1 (0) .. ~23 (10), also lowers HPF corner as pot rises (see 3) |
| TONE (knob) | 0..10 | linear 'B' 20k (S4/S6) | 0.5 | CCW: LPF branch (234 Hz) dominates = dark; CW: HPF branch (3290 Hz) dominates = bright; centre = max mid scoop (~ -10..-15 dB at ~500 Hz-1 kHz) |
| LEVEL (knob) | 0..10 | audio 'A' 100k | 0.5 | output attenuator after tone; unity at roughly 2 o'clock (see 3.6) |
| Footswitch | momentary latch | - | OFF (bypass) | toggles effect; electronic switching (FET/relay-free), bypass is BUFFERED |
| Red LED | - | - | off | lit when effect ON; current-limited, fixed brightness |
| Battery/DC | - | - | - | DC jack 9 V centre-negative; no UI beyond tooltip |
| INPUT jack | 1/4" mono | - | - | input Z 470k (DOCUMENTED (secondary): BOSS spec table as reproduced at scan.co.uk; some retailers list 1 MOhm, unresolved, 470k used); nominal input -20 dBu (DOCUMENTED secondary); inserting plug powers on battery |
| OUTPUT jack | 1/4" mono | - | - | output Z 1k, recommended load >= 10k, nominal output -20 dBu (DOCUMENTED (secondary): BOSS spec table); current draw 10 mA (DOCUMENTED secondary; some retailers say 20 mA) |
No other knobs/switches. Do NOT add hidden trim knobs; `boss_ds1_input_trim_db` / `boss_ds1_output_trim_db` live in board-level pedal info overlay (Advanced), not on face.

## 3 Signal path and circuit model
Order (modern rev). Rnn labels follow S1/S6, treat as ESTIMATED:
1. Input buffer: emitter follower, Zin 470k, gain ~0 dB, coupling HPF ~ 1.5 Hz (negligible; model as 1-pole HPF 2 Hz for DC safety).
2. Transistor booster (2 HPF + common-emitter, Q2 2SC2240): input HPF corners ~33 Hz and ~3.3 Hz (DOCUMENTED (secondary): ElectroSmash figures quoted on DIYstompboxes; a poster disputes them vs the plotted response, so verify); gain ~56x = 35 dB (DOCUMENTED (secondary): ElectroSmash); asymmetric clipping gives even harmonics (DOCUMENTED secondary). Previous estimate 72 Hz / 15 Hz superseded. Mid-band gain +35 dB CLAMPED by supply rails (rail +-4.5 V centred on 4.5 V bias). Model as: HPF 33 Hz -> gain 56x (35 dB) into soft rail clip `y = 4.5*tanh(x/4.5)` -> HPF 15 Hz. (Note: 35 dB figure is for small signal; at guitar levels (0.1-0.3 Vpk) this stage already adds soft asymmetry; add bias asymmetry term k=0.08: `y = 4.5*tanh((x+k*x*x)/4.5)`.)
3. Op-amp gain stage (non-inverting): Zf = Rf || Cc, Zg = Rg + 1/(sCz).
   - D = DIST taper position 0..1. Rf = 100k*D (+ 100 ohm floor). Rg = 4.7k + 100k*(1-D). Cc = 250 pF across Rf. Cz = 1 uF (S3; schematic may be 0.47 uF; ESTIMATED).
   - H(s) = 1 + Zf/Zg. DC/mid gain G = 1 + 100k*D/(4.7k+100k*(1-D)): D=0 -> 1.0 (0 dB); D=0.5 -> 1.9 (5.6 dB); D=1 -> 22.3 (27 dB). S5 quotes ~25 dB max: consistent (ESTIMATED).
   - Corner low: fc_hp = 1/(2*pi*Rg*Cz): 34 Hz at D=1 up to ~1.5 Hz at D=0. Corner high: fc_lp = 1/(2*pi*Rf*Cc) = 6.4 kHz at D=1 (rises as D falls; D<0.05 -> clamp to 20 kHz).
   - Additional HPF 23 Hz (C5-R10, S1) before the op-amp input.
   - Op-amp rails 0..9 V centred at 4.5 V: output limited +-4.0 V (ESTIMATED, rail-to-rail not true); slew limit ~0.5 V/us (model as part of clip; neglect at 4x OS).
4. Clipping: antiparallel 1N4148 pair to bias reference after a series resistor (Rd ~ 2.2k, ESTIMATED) with output node load. Model `i = 2*Is*sinh(v/(n*Vt))` per diode pair; Is = 2.52 nA, n = 1.752, Vt = 25.85 mV (1N4148 typical, ESTIMATED from datasheet-fit). Solve implicit node: `(vin - v)/Rd = 2*Is*sinh(v/(n*Vt)) + v/Rload`, Rload = tone network input (~ 15k). Newton 3 iterations, init from previous sample (warm start); fallback to closed-form Lambert-W approximation (Parker/D'Angelo) when |v|>0.3.
5. Tone network (passive, Big-Muff-style crossfade, S4): two branches from clipper node: LPF = R 6.8k + C 0.1 uF to ground (fc = 1/(2*pi*6.8k*0.1uF) = 234 Hz, DOCUMENTED S4), HPF = C 0.022 uF + R 2.2k to ground (fc = 3288 Hz, DOCUMENTED S4); 20k linear TONE pot wiper picks mix between the two taps. Model as: `y = (1-T)*LP1(x, 234 Hz) + T*HP1(x, 3290 Hz)` with pot loading correction (ElectroSmash via search summary: overall pedal loss ~ -12 dB with notch near 500 Hz, DOCUMENTED secondary; centre insertion loss of -17 dB here is ESTIMATED; wet chain restores via LEVEL stage / makeup fixed +14 dB). Exact nodal solution (3.5 ms-order precompute) preferred: 2x2 state-space, trapezoidal discretisation, coefficients recomputed on TONE change at control rate.
6. Level: 100k audio pot into output buffer stage (Zout 1k). Unity: at LEVEL=0.5 and DIST=0.5, TONE=0.5, a 1 kHz sine at -20 dBFS DI yields ~ -12 dBFS out (ESTIMATED; must calibrate in Test T6). Level range -inf..+~14 dB: `g_level = 10^((-60 + 74*L^0.6)/20)`... simpler: `g = pot_audio(L) * 5.0` where pot_audio(L)=L^2 (A taper, 10% at mid) ESTIMATED.
7. Output buffer: emitter follower, Zout 1k, HPF 5 Hz.
Calibration: analog volts = DI_sample * 0.5 V per FS (guitar peak ~ 0.25 V at -6 dBFS; ESTIMATED; set via `boss_ds1_input_trim_db`). Rails limit applied in volts then normalised back.
- Latency: 0 samples algorithmic (OS filter latency compensated, see 4).
- Bypass: BUFFERED (input buffer + output buffer stay in circuit, tiny coloration: HPF 2 Hz, LPF > 30 kHz = transparent). Bypass transition: electronic switch, crossfade 5 ms equal-power (real unit is click-free-ish with audible small thump; do not model thump, optional `boss_ds1_switch_thump` false). Footswitch hold-time: none.
- Noise: model hiss option (pre-gain noise 3.5 uV rms-equivalent ESTIMATED), default OFF for tests, ON in product at -92 dBFS wideband when DIST>0.5.

## 4 Plugin algorithm
Block diagram (per channel, mono core; stereo handled by processing L and R independently with linked params, or sum-to-mono per board setting):
```
x -> inHPF2Hz -> HPF33 -> *56 -> softclip(tanh,asym) -> HPF3.3 -> HPF23 -> opamp(H(s;D)) -> railclip(+-4V)
  -> diodeNode(Newton) -> tonePair(LP234, HP3290, mix T) -> *g_level -> outHPF5Hz -> y
```
- Oversampling: 4x polyphase IIR (half-band cascade, 8th order elliptic, >100 dB stop) for DIST > 0.2; 2x for DIST <= 0.2 (switch with 10 ms crossfade). Enable `boss_ds1_hq` (Off=2x/On=4x, default On). Offline render forces 8x. Linear stages (HPF/LPF/tone) run at base rate except where noted (op-amp H(s) at OS rate because Cc pole near 6 kHz).
- Op-amp H(s): bilinear transform with prewarp at fc_lp; coefficients recomputed when D changes (smoothed, 10 ms; update per 16 samples).
- Aliasing: THD+alias test (T5) requires alias components < -60 dBc for 1 kHz and 5 kHz sines at DIST=1; add 1-pole 20 kHz LPF post-clip at OS rate (mirrors circuit LPF) before decimation.
- Param smoothing: see table; one-pole exponential, coefficient per sample `a = 1-exp(-1/(tau*fs))` , tau=ms/1000 ; DIST/TONE smoothed then used to derive filter coefs every 16 samples; LEVEL smoothed per sample (`gain` multiplier ramp).
- Denormals: `juce::ScopedNoDenormals` at processBlock, plus `+1e-18` DC offset injected before IIR feedback paths (HPFs, tone filters), alternatively flush `|s|<1e-15`.
- Real-time safety: no heap alloc / locks / system calls in process(). All buffers (OS, filter states) prepared in prepare(); param reads via std::atomic<float> from APVTS; UI->audio only through atomics. Bypass flag atomic<bool> read once per block. Newton iteration bounded (max 4), no exceptions, NaN guard: if !isfinite(y) reset states and output 0.
- CPU budget: <= 0.35 % of a 3 GHz core per instance at 48 kHz, 4x OS, stereo (~ 120 flop/sample at OS rate x 4). Budget gate: 10 instances < 4 % in CI benchmark. Board supports 16 pedals.
- Block sizes 1..8192 supported; sample rates 44.1-192 kHz (at >= 96 kHz default OS = 2x).

## 5 I/O and routing
- 1 in / 1 out mono. Stereo host buses: process L and R separately with identical params (independent filter states); option `boss_ds1_stereo_mode` {0 dual-mono (default), 1 sum-to-mono then dup}.
- No extra jacks, no send/return, no control jack. Real-unit tricks available through board routing (BOARD_CORE.md): placement in series anywhere; parallel via board splitter (dry+DS-1 mix handled by board dry/wet); in a board FX-loop send/return configuration feedback loops are permitted by board (feedback path must contain the board's loop safety limiter and 1-block delay); place before amp gain for "booster into distortion" stack or after amp (post-gain "DS-1 in the loop"): input levels differ (+/-12 dB), input trim handles this.
- Input impedance interaction: when preceded by a non-buffered pedal (e.g. fuzz) model Zin 470k via board 'impedance matching' flag, otherwise ignore.
- Universal wet/dry: board-level mix (not a DS-1 control). Do not display on face.

## 6 MIDI / sync / expression
Real unit has no MIDI/expression/sync; emulate via Luthier generic mapping layer (see specs/MIDI_EXPRESSION.md):
- OMIT: no MIDI implementation chart exists for the real unit (no MIDI). Everything below is Luthier-local emulation design, not hardware fact; CC numbers are arbitrary pedal-local defaults.
- Footswitch gestures: MIDI note-on velocity from keyboard -> `boss_ds1_on` toggle on note-on, momentary mode: on while note held (like holding switch; latch/momentary selectable in mapping). Velocity maps to bypass crossfade time 2 ms (vel 127) .. 25 ms (vel 1) (soft vs. hard stomp feel). Aftertouch (channel pressure/poly) can map to DIST (default off): range D +0..+0.5 added, smoothed 30 ms.
- CC map defaults (all learnable, ch omni): CC 16 DIST, CC 17 TONE, CC 18 LEVEL, CC 19 on/off (>=64 on). 7-bit scaled to 0..1; 14-bit via CC+32 optional. Program Change: selects board preset (board-level), not pedal-level.
- Expression pedal emulation: any CC/slider/knob can drive DIST, TONE or LEVEL with min/max range + curve (linear/log). Default expression target: LEVEL.
- Tap tempo / MIDI clock: N/A (no time-based effect). Ignore clock.
- Sliders/knobs: mapped params are automatable (host automation + MIDI learn).

## 7 Visuals (original vector art; no logos)
Enclosure: compact die-cast stomp box, 73 W x 129 D x 59 H mm (face 73 x 129 mm, portrait). Colour: orange/amber body (#E8751A approx; ESTIMATED), black knob caps with white pointer line, white silkscreen text. Layout (face origin top-left, mm):
- Knobs (diameter 15 mm each, bottom-up triangular arrangement): LEVEL centre (14.5, 22), TONE (36.5, 22)... use row: LEVEL (13, 20), TONE (36.5, 20), DIST (60, 20) left-to-right; labels 5 mm above in caps "LEVEL" "TONE" "DIST".
- Pointer sweep: 270 deg, min at 7:30, max at 4:30. 11 tick marks 0..10.
- Red LED: diameter 4 mm at (36.5, 42); glow when ON.
- Top-centre name plate: "DISTORTION" word (generic) 8 mm tall at y=55; sub-label "DS-1 STYLE" small.
- Footswitch: large hinged plate (60 x 40 mm) at y=75..115, spans width; stomp animation 3 mm press, 80 ms ease-out.
- Jacks drawn on side faces in board view: INPUT left, OUTPUT right, DC 9 V rear.
- Vector notes: rounded rect corner 4 mm, subtle bevel gradient, screws ×2 on baseplate not shown. Board mounting: fits all mounting styles (screws at both sides, velcro, clamps, routed pocket, loose) via UI_BOARD_STYLES.md footprint 73 x 129 mm.

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | automatable |
|---|---|---|---|---|---|---|---|---|
| boss_ds1_dist | Dist | 0 | 1 | 0.5 | norm | pot A100k (x^2) | 20 | yes |
| boss_ds1_tone | Tone | 0 | 1 | 0.5 | norm | linear | 20 | yes |
| boss_ds1_level | Level | 0 | 1 | 0.5 | norm | pot A (x^2) | 15 | yes |
| boss_ds1_on | Footswitch on | 0 | 1 | 0 | bool | step | 5 (xfade) | yes |
| boss_ds1_hq | HQ oversample | 0 | 1 | 1 | bool | step | - | no |
| boss_ds1_stereo_mode | Stereo mode | 0 | 1 | 0 | enum | step | - | no |
(trim params removed from this pedal; slot layer `ref_s{nn}_in_db/out_db`, see reconciliation header)
| boss_ds1_revision | Revision | 0 | 1 | 0 | enum | step | - | no |
| boss_ds1_noise | Noise on | 0 | 1 | 1 | bool | step | - | no |

## 9 Tests / acceptance (provisional until hardware captures)
- T1 impulse + silence: no DC (|mean| < 1e-4 FS), output exactly 0 on digital silence with noise off.
- T2 FRF small-signal (-40 dBFS sine): TONE=0: -3 dB corner within +-15 % of 234 Hz low-pass; TONE=1: HPF corner 3290 Hz +-15 %; TONE=0.5: dip depth >= 8 dB below 1 kHz-tonal maximum, dip centre 400 Hz-1.2 kHz.
- T3 gain law: small-signal gain at 1 kHz (op-amp stage only) D=0: 0 dB +-0.5; D=0.5: 5.6 dB +-1; D=1: 27 dB +-2.
- T4 clipping: DIST=1, 1 kHz, -20 dBFS input -> output peak clipped at diode ~ +-0.55 V node (post-tone, scaled) ; THD (2nd..10th) at DIST=1: 25-40 % ; at DIST=0.2: 1-6 %; even-order (H2) share < 25 % of H3 (symmetric diodes) except booster asym tiny H2.
- T5 aliasing: 5 kHz sine, DIST=1, 48 kHz, 4x: aliasing < -60 dBc; 2x < -48 dBc.
- T6 level: unity calibration within +-1.5 dB of table (3.6).
- T7 bypass/null: bypassed output vs input null < -80 dB (buffer HPF 2 Hz ignored above 100 Hz); on/off crossfade no click: max |dy| < 0.05 FS.
- T8 cross-sample-rate: FRF at 44.1/48/96/192 within 0.5 dB < 10 kHz.
- T9 block sizes 1/4/16/64/512 bit-exact results (deterministic).
- T10 CPU: <0.35 % core per stereo instance 4x OS; soak 30 min randomised parameter/MIDI actions, no NaN, no xruns.
- T11 state save/restore: all params round-trip, hq/stereo/revision persisted.
- Reference comparison: null-test vs recorded DS-1 (same revision) at 5 knob positions x 3 gain levels; pass target (provisional): spectral envelope within 3 dB 100 Hz-8 kHz, THD within 20 % relative.

## 10 Known inaccuracies
- All component values in 3.2-3.7 are ESTIMATED/derived from excerpts; booster stage gain/corner frequencies, Rd, Cz (1 uF vs 0.47 uF), op-amp rails and slew, level taper are unverified.
- 1978 TA7136 version not modelled in v1 (different topology).
- No part tolerance, temperature, battery sag, power-supply ripple, transistor hFE variance modelled.
- Footswitch click/thump and enclosure microphonics not modelled.
- Noise floor figure is a plausible estimate, not measured.
- Tapers (A100k, B20k) from parts lists in hobby sources; real units vary by pot batch.
- Variant (c)/(d) in section 1 are OMIT; the earlier garbled 40th-anniversary text was removed.
- No claims of "authentic" or "identical"; label as "inspired by".

## 11 Verification log
Reachable: WebSearch result summaries only (ElectroSmash, Wikipedia, DIYstompboxes thread, Thermionic Studios table, retailer/BOSS spec tables via scan.co.uk and alamomusic). All direct fetches failed (DNS/denied). No schematic, service manual or primary manual PDF read.
| # | Item | Old -> New | Status | Source |
|---|---|---|---|---|
| 1 | Input Z | 470k ESTIMATED -> 470k (1 MOhm in some retailer listings) | DOCUMENTED (secondary) | https://www.scan.co.uk/products/boss-ds-1-distortion-effect-stompbox-classic-tones-for-all-types-of-music-unique-tone-control |
| 2 | Output Z / load | 1k ESTIMATED -> 1k, load >=10k | DOCUMENTED (secondary) | same + alamomusic.com listing |
| 3 | Nominal in/out level, current | none -> -20 dBu / -20 dBu, 10 mA (20 mA conflicting) | DOCUMENTED (secondary) | scan.co.uk; alamomusic.com |
| 4 | Booster gain | 56x/35 dB estimate -> 56x | DOCUMENTED (secondary) | https://www.electrosmash.com/boss-ds1-analysis (search summary) |
| 5 | Booster HPF corners | 72 Hz / 15 Hz -> 33 Hz / 3.3 Hz (disputed by a forum poster) | DOCUMENTED (secondary, disputed) | https://www.diystompboxes.com/smfforum/index.php?topic=126980.0 citing ElectroSmash |
| 6 | Overall loss / notch | -17 dB centre, ~500 Hz -> ~ -12 dB overall, notch ~500 Hz; centre insertion loss figure stays | DOCUMENTED (secondary) / ESTIMATED | ElectroSmash search summary |
| 7 | Op-amp part history, Q1/Q2 | unstated -> TA7136AP (1978), Rohm then M5223AL (~2000), 2SC2240 | DOCUMENTED (secondary) | https://en.wikipedia.org/wiki/Boss_DS-1 ; https://thermionic-studios.com/wiki/index.php?title=DS-1 |
| 8 | Variant (c) garbled text, (d) DS-1X | unverified -> removed | OMIT | none |
| 9 | Revision 1 (TA7136 1978) | PHASE 2 -> not modelled, selector hidden | OMIT | no topology data |
| 10 | MIDI/CC chart | implied -> none exists on hardware; CC 16-19 are local design | OMIT (hardware) / design choice | n/a |
| 11 | `boss_ds1_footswitch` vs `boss_ds1_on` | duplicate name -> `boss_ds1_on` | fixed | reconciliation header |
| 12 | Trim params in table | duplicated slot params -> removed | fixed | reconciliation header |
| 13 | DIST/LEVEL tapers, Rd 2.2k, Cz, rails, diode Is/n, tone R/C values (234/3290 Hz), 23 Hz HPF, noise floor, calibration 0.5 V/FS, colour | unchanged | ESTIMATED (kept; derivations in sections 2-3; S3/S4 are DIY/model write-ups, not Boss schematics) | n/a |

