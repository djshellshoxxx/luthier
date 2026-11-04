# GENERIC_PEDALS: 10 non-branded pedals (dev-ready)

Ver 1.0 | 2026-10-21 | Status SPEC. Depends on BOARD_CORE.md sec 11 (`IPedal`, `ParamDesc`, `PedalDesc.paramPrefix`). Namespace `luthier::reference::generic`, dir `Source/ReferencePedals/Generic/`. Reconciliation: IMPLEMENTATION_PLAN sec 1 wins on conflict.
Pedals: gen-boost, gen-overdrive, gen-fuzz, gen-compressor, gen-chorus, gen-delay, gen-reverb, gen-tremolo (required 8) + gen-wah, gen-phaser (optional, included). Generic = "family-typical behaviour", NOT a model of any product. Names/art original; no trademarks. All DSP is textbook, public-domain technique; values below are design choices tagged [EST] unless [DOC]. Tag meaning: [DOC]=standard published formula/reference; [EST]=chosen, tune by ear/measurement; [MEAS]=to measure in QA.

## 0. Common rules
### 0.1 IDs and registry
modelId `generic.<name>` (e.g. `generic.overdrive`). Param ids `gen_<name>_<ctrl>` snake_case, e.g. `gen_overdrive_drive`. `PedalDesc.paramPrefix` is the `gen_<short>_` shown in each table (boost, overdrive, fuzz, comp, chorus, delay, reverb, trem, wah, phaser); modelId stays `generic.<name>`. Host lane id = `ref_s{nn}_p{kk}` assigned by `PedalRegistry::laneMap` (BOARD_CORE 9.1): table rows in order, skipping footswitch `_on` (PF_Engage, aliases slot `enabled`), `_quality` (PF_Config), and Trigger/Momentary rows (`_tap`, `_hold`, `_freeze`). Params append-only per revision; never renumber. `revision` = 1.
### 0.2 Levels / conventions
- Internal signal: double, 0 dBFS = 1.0 (BOARD_CORE `kRefDbu`). Pedal nominal input = -18 dBFS RMS guitar-ish, headroom to +6 dBFS.
- Unity: all pedals at default are within +-1.5 dB of unity loudness at -18 dBFS 1 kHz sine, except where stated. [MEAS]
- dB-law levels: `Level/Volume` 0..1 -> gain dB = -inf at 0 (below -60 dB fade-to-zero), 0 dB point defined per pedal below.
- Taper codes: LIN, LOGA (audio, `v=n^2.0` shape: `value = min + (max-min)*n^k`, k=2), LOGB, REV; exp-freq `f = fmin*(fmax/fmin)^n`; dB-lin `dB = dBmin + n*(dBmax-dBmin)`. Implemented through `ParamDesc.taper/skew`; `fromNormalised`: `n^(1/skew)` when skew!=1 (matches legacy `PedalParam`), so to get value=min+(max-min)*n^2 set skew=0.5.
- Smoothing: 20 ms linear unless noted; discrete params instant + 5 ms xfade.
- Bypass: footswitch `gen_<name>_on` (PF_Engage: NOT stored in the pedal, aliases slot `enabled`/`ref_s{nn}_enabled`; default 1 for a loaded pedal). Bypass mode per pedal below. LED = model `enabled`.
- Oversampling: nonlinear pedals (boost-clip, overdrive, fuzz) use `Oversampler.h` 4x default (Quality param `gen_<name>_quality`: 0 Eco 1x, 1 Normal 2x, 2 High 4x; default 1). Latency reported = oversampler FIR group delay (linear-phase halfband, [EST] 2x: 16 samples@1x ; 4x: 32). No other pedal reports latency.
- Denormal/NaN: each process() ends with isfinite clamp +-16 (like prototype `safe()`).
- No generic pedal is stateless (`isStateless()` false).
### 0.3 Standard MIDI mapping (per-pedal LOCAL profile; reconciled with MIDI_EXPRESSION sec 9)
Each pedal section ends with "MIDI native: CC n label ..." = that pedal's LOCAL CC profile (`ParamDesc.midiCcDefault`). A local profile is active ONLY for a slot that has an explicit `midi_chan` (1..16) or currently holds MIDI focus (`ref_board_selected_pedal`). There is NO global "CC 80+k" or "CC 14 = footswitch" scheme (draft rows removed): native CCs of different pedals intentionally overlap and never conflict because only the addressed pedal listens. Two pedals on the same explicit channel with overlapping CCs: the one earlier in chain order wins, the UI lists the clash. Reserved CCs of MIDI_EXPRESSION 4.3 (0, 32, 120-127; 1, 11, 64 only as documented there) must not appear in a local profile except CC11 (wah pedal) and CC64 (momentary footswitch) per that section.
| MIDI | Action |
|---|---|
| Footswitch (native CC >=64 / note) | `FootEvent` press/release to the slot; latch/momentary per MIDI_EXPRESSION 5.2 |
| Note-on velocity | `FootEvent.speed` = vel/127 (BOARD_CORE 9.6): xfade ms = lerp(12,2,speed) |
| Channel/Poly aftertouch | `FootEvent.force` 0..1; if the pedal has an aftertouch target: modulates that param by `at_amount` (-1..1, default 0.5) via board binding (not a pedal param) |
| Pitch bend | not used by default |
| Clock 0xF8 / transport | consumed by `ClockSync`; time-based pedals (delay, tremolo, chorus, phaser) read `TimeInfo` when `*_sync` != off |
Native CC ids are ESTIMATED own allocation in unassigned/general ranges; all 0..127 -> norm linear; Toggle >=64; Trigger rising edge >=64.
### 0.4 Visuals (common)
Original vector art, 2D, DPI-independent (`juce::Path` + gradients; no bitmaps in v1). Pedal body "stompbox" 66 x 112 (w x h in MILLIMETRES, ~2.6 x 4.4 in; UI renders at 1.6 px/mm, UI_BOARD_STYLES 4.1) for Standard footprint, "Wide" 90x112 for delay/reverb/wah (wah: 54 x 150 treadle, separate). Elements: body rounded rect (radius 6), top-edge bevel gradient, 4 corner screws (decor), jack stubs left/right (IN/OUT, DC on top), knob = cylinder with pointer line + knurled ring (30 deg..330 deg sweep, 300 deg travel, detent mark at centre for centre-detent), labels sans-serif caps 9 px min (scales), footswitch = metal dome ~ 24 px with LED (red 5 mm) above. Colour: each pedal one distinct enamel colour + 1 accent. Knob hit-target >= 28 px; double-click resets default; shift-drag fine (0.1x); mouse wheel step 1%; arrow keys 1%/10% with Shift; Right-click: MIDI learn, expression assign. Tooltip: value text with unit. Accessible names: "<pedal> <label>, <value text>". Switches (toggles) = 3-position or 2-position mini toggles with labelled positions. LED shows model enabled; a second small amber LED shows "tail active" for delay/reverb. Pedal state for mounting (BOARD_CORE 14) draws screws/velcro per board style around the pedal base and does not change the pedal face.
Sizes in table below use `Face` = Standard unless noted.

---
## 1. Clean Boost  `generic.boost`  (family: Boost)  Face: Compact 54x90, colour flat grey-blue, accent white
Purpose: transparent level lift with optional treble emphasis and mild soft-clip at extremes.
| # | id | label | range | default | taper | unit | notes |
|---|---|---|---|---|---|---|---|
|1| gen_boost_gain | GAIN | 0..+30 dB | +12 | dB-lin | dB | pre-clip gain |
|2| gen_boost_tone | TONE | -6..+6 dB high-shelf @ 3 kHz | 0 | dB-lin | dB | centre detent = flat |
|3| gen_boost_level | LEVEL | -24..+6 dB | -6 | dB-lin | dB | post gain trim |
|4| gen_boost_mode | MODE | {Clean, Warm, Fat} | Clean | enum | - | Clean: pure gain; Warm: +3 dB shelf @ 100 Hz, tanh knee; Fat: 3rd-order low-shelf +6 dB @ 120 Hz + soft clip |
|5| gen_boost_quality | QUALITY | {Eco,Normal,High} | Normal | enum | - | oversampling |
|6| gen_boost_on | (footswitch) | 0/1 | 1 | toggle | - | |
DSP [DOC textbook]:
1. HPF 1-pole @ 20 Hz (DC block, bilinear). 2. Gain `g = 10^(gain_dB/20)` smoothed (log domain). 3. Tone: high-shelf biquad (RBJ cookbook), f0=3 kHz, S=1, gain `tone_dB`. 4. Mode shaping: Clean -> `y = x` hard-limited by soft clip `y = x / (1 + |x|^4)^(1/4)` (knee at ~1.0 so clean until ~0.8), Warm -> `y = tanh(1.0*x)` normalised `/tanh(1)` blend 0.5 with clean (`0.5*x + 0.5*tanh(x)`), Fat -> low shelf + `tanh(1.4*x)`. 5. LPF 1-pole @ 18 kHz (alias guard, only when quality=Eco). 6. Level gain. Oversampled around steps 3-5.
Bypass: True. Latency by oversampler. Stereo: Stereo (per channel same params, no cross-talk). CPU [EST] 0.3%.
MIDI native: CC 20 gain, 21 tone, 22 level, 23 mode, 24 quality (rare), 25 on. Aftertouch default target: gain (+0.5 -> up to +15 dB).
Visual: 3 knobs triangular (GAIN top centre, TONE/LEVEL bottom), mode as 3-way mini toggle at top; LED right.
Tests: gain 0 dB, tone 0, level 0 dB, mode Clean -> unity within 0.1 dB for -30 dBFS sine; THD of Clean at -20 dBFS input with +12 dB gain < 0.5% [MEAS]; frequency response 20 Hz-20 kHz flat +-0.3 dB at tone 0; no aliasing above -60 dBFS at 5 kHz sine +30 dB gain, High quality.

## 2. Overdrive  `generic.overdrive`  (family: Overdrive)  Face: Standard, colour green enamel, accent cream
Purpose: mid-focused soft-clipping drive with tone and level (3-knob classic layout) plus 3-way clipping switch.
| # | id | label | range | default | taper | unit | notes |
|---|---|---|---|---|---|---|---|
|1| gen_overdrive_drive | DRIVE | pre-gain 0..+40 dB | +16 | LIN-in-dB (n^1) | dB | |
|2| gen_overdrive_tone | TONE | LP cutoff 500 Hz..9 kHz | 3 kHz | exp-freq | Hz | post-clip 1-pole |
|3| gen_overdrive_level | LEVEL | -60..+12 dB (n=0 mutes) | 0 dB at n=0.69 | skew 2 | dB | see formula below |
|4| gen_overdrive_clip | CLIP | {Soft(sym), Asym, Hard(diode)} | Soft | enum | - | |
|5| gen_overdrive_bass | BASS CUT | {Off, 300 Hz, 700 Hz} | 300 Hz | enum | - | pre-clip HPF (TS-like mid hump) |
|6| gen_overdrive_quality | QUALITY | {Eco,Normal,High} | Normal | enum | | |
|7| gen_overdrive_on | footswitch | | 1 | | | |
Level taper: `dB = -60 + 72*n^0.5` for n>0, n==0 -> mute; 0 dB at n=0.69 (default n=0.69 -> `-60+72*0.8307=-0.2 dB`). [EST] Use `skew=2` in ParamDesc with min -60 max +12 (n^(1/2)). Bass cut default = 300 Hz.
DSP [DOC: Pakarinen & Yeh 2009 survey of guitar distortion modelling; Yeh "Digital Implementation of Musical Distortion Circuits by Analysis and Simulation" 2009, standard memoryless waveshaper refs]:
1. Input HPF per `bass`: 1-pole @ {20 Hz, 300, 700}. Implemented as split: `x_clip_in = x - LP(x, f)` + `0.3*LP(x,f)` (keeps some low end, feedback-clipper style, from prototype `Board.h` ts808 treatment).
2. Gain stage `g = 10^(drive_dB/20)`, smoothed.
3. Clipping (memoryless, applied 4x oversampled): Soft: `y = tanh(x)`; Asym: `y = tanh(x + 0.15) - tanh(0.15)` normalised plus DC block; Hard (diode pair): `y = sign(x)*(1 - exp(-|x|))` (approximation of Shockley diode clipper), scaled by 1/0.9. Post: `y *= 1/ (1+0.1*g^0.5)` compensation (keeps loudness within 6 dB over drive range, [EST]).
4. Tone: 1-pole LP at tone freq. 5. Level. 6. Output DC block 5 Hz.
Clipping-point alias control via 4x oversampling (High) else 2x.
Bypass: True. Channel: Stereo. CPU 1.0% [EST] at 4x.
MIDI native: CC 26 drive, 27 tone, 28 level, 29 clip, 30 bass cut, 31 on. Aftertouch default target: drive (+0.5 = up to +20 dB over range).
Visual: three knobs in an arc (DRIVE left, TONE centre top, LEVEL right), two mini toggles (CLIP, BASS) on lower strip; green body.
Tests: sine -20 dBFS: THD rising monotonic with drive; low drive (0 dB) near-linear (THD<1%); level calibration: default settings -18 dBFS 1 kHz in -> out within +-3 dB; harmonic content soft: 3rd > 2nd by >10 dB (Soft), Asym even harmonics present (2nd > -40 dBc at drive max); tone cutoff -3 dB at set freq +-10% at drive 0; alias test at 4x High.

## 3. Fuzz  `generic.fuzz`  (family: Fuzz)  Face: Standard, colour red/black with silver, accent
Purpose: two-transistor-style germanium/silicon-ish fuzz with FUZZ, TONE, VOLUME plus bias and gate.
| # | id | label | range | default | taper | unit | notes |
|---|---|---|---|---|---|---|---|
|1| gen_fuzz_fuzz | FUZZ | +6..+60 dB | +44 dB (n=0.7) | dB-lin | dB | |
|2| gen_fuzz_tone | TONE | tilt EQ: pivot 1 kHz, -12..+12 dB high/low tilt | 0 (centre detent) | LIN | dB | |
|3| gen_fuzz_volume | VOLUME | -60..+12 dB (skew 2) | -6 dB | LOGA | dB | |
|4| gen_fuzz_bias | BIAS | 0..1 asymmetry/starve | 0.5 | LIN | - | 0 starved (gated, spitty) -> 1 (open, smooth) |
|5| gen_fuzz_type | TYPE | {Silicon, Germanium, Octave-ish} | Silicon | enum | - | selects waveshaper |
|6| gen_fuzz_gate | GATE | 0..1 | 0 | LIN | - | thresh -90..-40 dBFS, off at 0 |
|7| gen_fuzz_quality | QUALITY | {Eco,Normal,High} | Normal | | | |
|8| gen_fuzz_on | | | 1 | | | |
DSP:
1. Input: HPF 1-pole @ 60 Hz; Germanium: additional input LPF 1-pole @ 9 kHz and HPF 90 Hz.
2. Pre-gain `g=10^(fuzz_dB/20)`; two cascaded stages: stage1 `y1 = clip1(g1*x)`, stage2 `y2 = clip2(g2*y1)`, g1*g2=g with g1=sqrt(g) (split evenly in dB).
3. Waveshapers [DOC: Yeh, Pakarinen]: Silicon: asymmetric hard-ish `f_pos = tanh(1.3x)`, `f_neg = 1.1*tanh(0.9x)`; Germanium: `f(x)=tanh(1.8*(x+b))-tanh(1.8*b)`, b = 0.2*(1-bias) (soft, sagging) + sag: envelope follower (attack 5 ms, release 120 ms) reduces gain by up to 20% `g_eff = g*(1-0.2*env)`; Octave-ish: add `|x|` full-wave rectified component: `y = 0.6*f(x) + 0.4*(|f(x)|-mean)` high-passed 80 Hz (sounds like rectifier octave-up; documented as approximation).
4. Bias: `b_sh = (0.5 - bias)*0.6` DC shift added before stage2 (gating when starved: below threshold output collapses: `y2 *= smoothstep(0, 0.1, |y1| + 0.1*bias)`), DC blocker 10 Hz after.
5. Tone: tilt EQ: `y_t = LP(y,1kHz)*10^(-t/20) + (y - LP(y,1kHz))*10^(t/20)` t in dB.
6. Gate (noise): downward expander: level detector peak (att 1 ms, rel 80 ms), thresh from GATE, ratio 4:1... gate gain floor -60 dB, hold 30 ms; implemented before volume.
7. Volume. Output LPF 1-pole 14 kHz Eco.
Bypass: True. Channel: Stereo. CPU 1.2%.
MIDI native: CC 32 fuzz, 33 tone, 34 volume, 35 bias, 36 type, 37 gate, 38 on. Aftertouch default: bias (+0.5) (starve fuzz by pressure - fun expressive).
Visual: three big knobs in a row (FUZZ, TONE, VOLUME) plus small BIAS trimmer knob and TYPE rotary 3-position; stripe graphic.
Tests: no NaN with input +16; max output <= +12 dB; bias sweeps monotonic RMS; gate: -70 dBFS noise silenced when gate on, 0 dBFS burst passes with attack <2 ms; germanium shows envelope sag [MEAS] 10-25% reduction over 300 ms at sustained -10 dBFS; THD > 20% at fuzz max; alias at 4x high < -50 dBc.

## 4. Compressor  `generic.compressor`  (family: Dynamics)  Face: Standard, colour pale blue, accent orange
Purpose: feed-forward stompbox compressor: SUSTAIN (thresh/ratio linked), ATTACK, LEVEL, BLEND; plus fully-manual mode.
| # | id | label | range | default | taper | unit |
|---|---|---|---|---|---|---|
|1| gen_comp_sustain | SUSTAIN | 0..1 -> threshold -10..-45 dB & ratio 2:1..12:1 linked | 0.5 | LIN | - |
|2| gen_comp_attack | ATTACK | 0.5..60 ms | 10 ms | exp | ms |
|3| gen_comp_release | RELEASE | 40..1200 ms | 350 | exp | ms |
|4| gen_comp_level | LEVEL | -12..+24 dB makeup | auto (+6) | dB-lin | dB |
|5| gen_comp_blend | BLEND | 0..1 dry/wet (parallel) | 1.0 | LIN | - |
|6| gen_comp_mode | MODE | {Sustain(linked), Manual} | Sustain | enum | - |
|7| gen_comp_thresh | THRESH (manual) | -60..0 dB | -24 | dB-lin | dB |
|8| gen_comp_ratio | RATIO (manual) | 1.5..20 (n^2 skew) | 4 | LOGA | :1 |
|9| gen_comp_knee | KNEE | 0..12 dB | 6 | LIN | dB |
|10| gen_comp_on | | | 1 | | |
DSP [DOC: Giannoulis, Massberg, Reiss 2012 JAES "Digital Dynamic Range Compressor Design"]:
- Detector: peak/RMS blend: `env = max(|L|,|R|)` stereo-linked (one gain for both channels; param `link` fixed 100%). Level in dB: `x_dB = 20log10(env + 1e-9)`.
- Static curve with soft knee W: `y_dB = x_dB` if 2(x_dB-T) < -W; `x_dB + (1/R -1)(x_dB - T + W/2)^2/(2W)` if |2(x_dB-T)| <= W; `T + (x_dB-T)/R` otherwise. Gain computer `g_dB = y_dB - x_dB`.
- Smoothing in dB domain (branching log-domain peak detector): `g_s[n] = a*g_s[n-1] + (1-a)*g_dB` with a = attack coeff when g_dB < g_s (gain reducing) else release coeff; `a = exp(-1/(tau*sr))`, tau = attack or release seconds. Hold none.
- Sustain mapping: `T = -10 - 35*s` dB, `R = 2 + 10*s`; makeup auto = `-T*(1-1/R)*0.5` dB (applied when mode Sustain and level at default; otherwise level knob). Explicit: level knob adds to auto makeup.
- Soft-clip safety after makeup: `y = x` up to 0.95, tanh above (output safety only).
- Blend: `out = dry*(1-b) + comp*b` with dry latency 0 (no lookahead, so no comp).
- No oversampling (non-clipping), but gain smoothing at sample rate; sidechain HPF 1-pole @ 80 Hz (avoid pumping from bass) fixed.
- GR meter: atomic float to UI (LED ladder of 5 amber LEDs).
Bypass: True. CPU 0.3%. Stereo linked.
MIDI native: CC 40 sustain, 41 attack, 42 release, 43 level, 44 blend, 45 mode, 46 thresh, 47 ratio, 48 knee, 49 on. Sidechain external (sidechain bus when enabled): `gen_comp_sc` {Internal, Sidechain bus} (param 11, default Internal); no latency.
Visual: 4 knobs (SUSTAIN, ATTACK, LEVEL, BLEND) in 2x2; small RELEASE trimmer, GR LED ladder across top.
Tests: static curve points within 0.2 dB (T=-24,R=4,knee 0: -12 dB in -> -21 dB out etc.); attack/release time constants measured to 63% of step within 10%; makeup auto brings -30 dBFS input RMS up consistent; blend 0 = dry exactly; stereo link: L-only loud signal reduces R by same GR; no zipper; pass-through at ratio 1.5 low-level.

## 5. Chorus  `generic.chorus`  (family: Modulation)  Face: Standard, colour teal, accent silver
Purpose: analog-style (BBD-ish) mono-in/stereo-out chorus with rate/depth/tone, plus vibrato and dimension modes.
| # | id | label | range | default | taper | unit |
|---|---|---|---|---|---|---|
|1| gen_chorus_rate | RATE | 0.1..8 Hz | 0.8 | exp | Hz |
|2| gen_chorus_depth | DEPTH | 0..1 -> mod depth 0..±4.5 ms (at base delay) | 0.5 | LIN | - |
|3| gen_chorus_mix | MIX | 0..1 (equal-power) | 0.5 | LIN | - |
|4| gen_chorus_tone | TONE | wet LP 1.5..12 kHz | 6 kHz | exp | Hz |
|5| gen_chorus_mode | MODE | {Chorus, Vibrato(100% wet), Dimension (2 voices fixed)} | Chorus | enum | - |
|6| gen_chorus_wave | WAVE | {Sine, Triangle} | Sine | enum | - |
|7| gen_chorus_spread | SPREAD | 0..1 stereo width | 0.8 | LIN | - |
|8| gen_chorus_sync | SYNC | {Off, 1/4, 1/8, 1/8., 1/16, 1/2, 1 bar} | Off | enum | - |
|9| gen_chorus_on | | | 1 | | |
DSP [DOC: Dattorro 1997 "Effect design part 2"]:
- Delay line per voice: modulated fractional delay, base `d0 = 7 ms` (chorus), 4 ms (dimension); LFO `d(t) = d0 + depth_ms * lfo(t)`, `depth_ms = depth*4.5`. Interpolation: 4-point Lagrange (3rd order) [or cubic Hermite]. Buffer 20 ms at SR + guard.
- Voices: Chorus: 2 voices (L: LFO phase 0, R: phase 0.25 cycle*spread...). Right voice LFO phase offset `phi_R = 0.5*spread` cycle (spread 1 = anti-phase), left phase 0. Dimension: 2 fixed voices, rate 0.4 Hz, depth 0.35, wet inverted on one side (phase inversion on R).
- Vibrato: dry removed (mix forced 1), depth range ×0.6, base d0 = 5 ms.
- Wet path: input HPF 1-pole 100 Hz (only wet), tone LP (2-pole Butterworth biquad @ tone, bilinear w/ prewarp). Anti-BBD flavour: soft saturation `tanh(0.7 x)/0.7*0.9` on wet (very mild) and noise floor -90 dBFS [EST] off by default (`gen_chorus_noise` omitted).
- Mix equal-power: `out = dry*cos(m*pi/2) + wet*sin(m*pi/2)`; stereo: mono input is sum (L+R)/2 unless dual-mono (BOARD_CORE 6.8).
- LFO: phase accumulator double, `inc = rate/sr`; sine via wrap-safe `std::sin(2pi*phase)` per 8 samples (control-rate interpolation) or table 2048+lerp. Triangle: `1-4|phase-0.5|`... smooth-rounded.
- Sync: tempo from `TimeInfo` (BOARD_CORE 9.5): `rate = bpm/60 * (division)`; phase locked to ppq: `phase = frac(ppq*div + phase0)` hard lock when playing, free-run otherwise; switching sync on/off preserves phase and glides rate 100 ms.
Bypass: True (tail <= 20 ms, ignored); xfade 5 ms.
Channel mode: MonoInStereoOut. CPU 0.8%.
MIDI native: CC 50 rate, 51 depth, 52 mix, 53 tone, 54 mode, 55 wave, 56 spread, 57 sync, 58 on; MIDI clock sync via sync param; Tap not provided.
Visual: 3 top knobs RATE/DEPTH/MIX, TONE small; mode 3-way toggle; LED tempo-blink at current rate (visual LFO phase).
Tests: LFO frequency exact to 0.1% (zero-cross count); depth: peak delay deviation = depth*4.5 ms +-2%; mix=0 -> dry exactly; vibrato pitch deviation matches `ΔF/F = 2π f_lfo * depth_ms/1000` +-3%; stereo correlation at spread 1 <= 0.5 for noise; sync lock: LFO phase aligned to ppq grid within 1 sample/50 after 2 beats; no clicks on mode switch (5 ms xfade); fractional delay passband flat +-0.5 dB to 14 kHz at 48k.

## 6. Delay  `generic.delay`  (family: Time)  Face: Wide 90x112, colour deep blue, accent chrome
Purpose: versatile delay: time 20 ms..2 s, feedback, mix, with Digital / Analog / Tape voicing, ping-pong, tap tempo and MIDI-clock sync.
| # | id | label | range | default | taper | unit |
|---|---|---|---|---|---|---|
|1| gen_delay_time | TIME | 20..2000 ms | 380 | exp | ms |
|2| gen_delay_feedback | FEEDBACK | 0..1.1 (runaway >1.0 only if board `ref_board_fb_runaway_ok`; else clamp 0.98) | 0.35 | LIN | - |
|3| gen_delay_mix | MIX | 0..1 (equal-power) | 0.35 | LIN | - |
|4| gen_delay_tone | TONE | repeat LP 1.2..12 kHz | 5 kHz (Analog default 3 kHz) | exp | Hz |
|5| gen_delay_mode | MODE | {Digital, Analog(BBD-ish), Tape} | Digital | enum | - |
|6| gen_delay_mod | MOD | 0..1 -> wow/flutter or BBD chorus depth | 0.15 | LIN | - |
|7| gen_delay_sub | SUBDIV | {1/4, 1/8, 1/8., 1/8T, 1/16, 1/2, 1 (quarter-based tempo)} | 1/4 | enum | - |
|8| gen_delay_sync | SYNC | {Off(free ms), Host/Clock, Tap} | Off | enum | - |
|9| gen_delay_pingpong | PING-PONG | {Off, On, Wide} | Off | enum | - |
|10| gen_delay_spread | STEREO | 0..1 (L/R time offset ratio 1:1 .. 1:0.75) | 0 | LIN | - |
|11| gen_delay_trails | TRAILS | {Cut, Spillover} | Spillover | enum | - |
|12| gen_delay_hold | HOLD | momentary (infinite feedback, input muted while held) | 0 | momentary | - |
|13| gen_delay_on | | | 1 | | |
|14| gen_delay_tap | TAP | Trigger | 0 | trigger | - |
DSP [DOC: Zölzer DAFX 2nd ed. ch.2; Dattorro; Parker & Esqueda for BBD/tape trends]:
- Buffer: stereo circular, 2.2 s max at current SR (+ interpolation guard + modulation headroom 50 ms), power-of-two mask, preallocated in prepare().
- Time: smoothed with slew limiter (one-pole 60 ms for glides) to give tape-like pitch-bend on time change (Analog/Tape) or crossfaded dual-tap (Digital: two read taps, xfade over 40 ms on change > 5%, no pitch glide). Interp: Hermite 4-point (Digital: 4-pt Lagrange; Tape: Hermite).
- Feedback path (per repeat): `fbIn = clipSafe( HP_20Hz( LP_tone( sat( delayed ))) ) * fb`; `sat`: Digital none; Analog: `tanh(1.2x)/1.2` + companding-ish: HP @ 150 Hz in the loop and LP tone steeper (2-pole), repeats progressively darker; Tape: `tanh(0.9x)` + wow/flutter + HF loss (LP 1-pole 6 kHz in loop) + 1-pole HP 80 Hz.
- Mod: Digital: 0; Analog: sine LFO 0.4 Hz ±(mod*0.8 ms); Tape: wow LFO 0.6 Hz ±(mod*1.2 ms) + flutter 7 Hz ±(mod*0.15 ms) + slow random drift (sample/hold LFO lowpass 0.3 Hz, deterministic seed from instance id, ±mod*0.5 ms).
- Ping-pong: input to L tap only; L delay feeds R delay and vice versa through feedback crossing (`fbL = delayedR*fb`, `fbR = delayedL*fb`); `Wide` adds 15% longer R time + 2 ms Haas. Spread: R time = L*(1 - 0.25*spread) (not ping-pong).
- Sync: Host/Clock: `time_ms = 60000/bpm * div_beats` where div_beats from SUBDIV {1,0.5,0.75,0.3333,0.25,2,4}; clamp to 2000 ms (if exceeds, halve until fits). Tap: Trigger param `gen_delay_tap` (row 14, rising edge from UI, native CC 85, or MIDI note) (2 taps min, accept 60..2000 ms, timeout 2.5 s). MIDI clock quantised via `TimeInfo.bpm` from ClockSync (`clockLocked` required else keep last value).
- HOLD: input gain -> 0 over 5 ms, feedback -> 1.0 (limiter protected by 6.6 soft limiter ceiling), exits on release with 50 ms ramp.
- Trails: Spillover: bypass fades only the dry/wet input, tail continues until `tailSilent` (energy < -90 dBFS); Cut: output muted 20 ms.
- Mix equal-power; wet only taps (dry path continuous).
- tailEnergyDb: running RMS of buffer output smoothed.
Bypass: DSP (trails). Channel: TrueStereo (ping-pong needs L/R coupling), mono-in OK. CPU 1.0% (Tape 1.4%).
MIDI native: CC 59 time, 60 feedback, 61 mix, 62 tone, 63 mode, 85 tap (momentary), 65 mod, 66 subdiv, 67 sync, 68 pingpong, 69 hold, 70 on; MIDI clock sync: when sync = Host/Clock use `TimeInfo.bpm`.
Visual: wide face; 4 knobs row (TIME, FEEDBACK, MIX, TONE), MODE rotary 3-position selector, small MOD trimmer, SUBDIV rotary detent knob with printed note symbols; TAP footswitch (second small switch) + main on; 7-segment-style LED shows BPM when synced; amber LED = repeats active.
Tests: delay time exactness: impulse peak at time*sr +-0.5 sample (smoothing off); feedback decay 0.5 -> repeats ratio 0.5 each (Digital, tone fully open) +-0.2 dB; ping-pong alternates channel; no zipper/pitch artifacts when changing time in Digital (xfade); Analog repeats HF rolloff increasing per repeat; runaway clamp <=0.98 unless allowed; limiter bounds at 1.1; tail continues 5 s after bypass in Spillover then ends < -90 dBFS; sync accuracy: 120 BPM 1/8 = 250 ms (+-1 sample after lock); tap tempo error < 1 ms for steady taps; reset clears buffer; state save restores tap time via `saveExtra`.

## 7. Reverb  `generic.reverb`  (family: Time)  Face: Wide 90x112, colour cream/wood-grain accent, accent gold
Purpose: Room / Hall / Plate / Spring-ish reverb with decay, damping, pre-delay, mix, mod.
| # | id | label | range | default | taper | unit |
|---|---|---|---|---|---|---|
|1| gen_reverb_decay | DECAY | RT60 0.2..12 s | 2.2 | exp | s |
|2| gen_reverb_mix | MIX | 0..1 (equal-power) | 0.3 | LIN | - |
|3| gen_reverb_predelay | PRE-DELAY | 0..250 ms | 15 | LIN | ms |
|4| gen_reverb_tone | TONE | tilt -1 dark .. +1 bright (damping HF cut 2..16 kHz + LF shelf) | 0 | LIN | - |
|5| gen_reverb_size | SIZE | 0.3..1.7 delay-length scale | 1.0 | LIN | - |
|6| gen_reverb_type | TYPE | {Room, Hall, Plate, Spring, Shimmer-free "Ambient"} | Hall | enum | - |
|7| gen_reverb_mod | MOD | 0..1 | 0.25 | LIN | - |
|8| gen_reverb_lowcut | LOW CUT | 20..800 Hz (wet) | 120 | exp | Hz |
|9| gen_reverb_trails | TRAILS | {Cut, Spillover} | Spillover | enum | - |
|10| gen_reverb_freeze | FREEZE | momentary | 0 | momentary | - |
|11| gen_reverb_on | | | 1 | | |
DSP [DOC: Dattorro 1997 "Effect design part 1: reverberator and other filters" (plate topology); Jot & Chaigne 1991 FDN; Schroeder 1962; Välimäki et al. 2012 "Fifty years of artificial reverberation"]:
- Types: Room/Hall/Ambient = 8-line Feedback Delay Network (FDN), Hadamard (Householder) 8x8 feedback matrix (fast, energy-preserving; `A = I - (2/N) 1 1^T`), mutually-prime delay lengths (base 29..97 ms scaled by SIZE; Room 12..45 ms base, Hall 30..110, Ambient 40..140); input diffusion: 4 series allpass (g=0.6, delays 5.3, 7.1, 10.6, 14.7 ms scaled). Per-line decay gain `g_i = 10^(-3 d_i / RT60)` (d_i seconds), HF damping per line: 1-pole LP with coefficient so RT60(HF) = RT60 * (0.15 + 0.85*bright) where `bright = (tone+1)/2`; LF damping: 1-pole HP shelf: RT60(LF) = RT60 * 1.0.. fixed.
- Plate: Dattorro tank topology (figure-8 with modulated allpass, decay diffusion 0.7, damping 1-pole, ~2 stereo taps) using published structure; delay-line lengths scaled from 29761 Hz sample set to SR (rounded) [DOC Dattorro paper values 142,107,379,277 input diffusion samples @29761].
- Spring: 3 parallel dispersive chains: cascade of 24 first-order allpasses (a=0.62..0.7 ±) producing chirp dispersion + 2-line feedback delays 33 and 41 ms + 4 kHz LP + nonlinear "boing" saturation tanh on input; approximates a spring [EST], not an exact model.
- Modulation: slow random-walk LFO 0.1..0.9 Hz on 2 late-delay lines (±(0.5..4 ms*mod)); decorrelates tail.
- Pre-delay: separate delay line 250 ms max. Low cut: HPF on wet input 2-pole. Output: stereo from different FDN taps (L: lines 0,2,4,6 with signs; R: 1,3,5,7) → decorrelated; mono input OK.
- Freeze: feedback gain → 1.0 (`g_i`=1, damping off), input muted 20 ms ramp; release resumes decay over 100 ms.
- Trails/Mix same as delay; equal-power mix; wet RMS normalised so that Mix=1 stays within +-3 dB of dry at default.
- Denormals: add noise floor dither -150 dBFS alternate sign in FDN loop OR flush-to-zero per block; no feedback gain >= 1 outside freeze (clamped 0.9995).
- Memory: Hall max 12 s tail needs per-line delay up to size 1.7*140 ms => ~0.24 s * 8 lines *8 B * SR(192k)= 3 MB OK.
Bypass: DSP (trails). Channel: MonoInStereoOut (stereo input summed to mono for tank input unless TrueStereo "Plate/Hall stereo in": use mid/side input sum (L+R)/2 only). CPU 2.5% (Hall) / 3% (Plate).
MIDI native: CC 71 decay, 72 mix, 73 predelay, 74 tone, 75 size, 76 type, 77 mod, 78 lowcut, 79 trails, 102 freeze, 103 on (native CCs scoped to the focused/assigned slot).
Visual: cream enamel, wood-grain veneer strip across top (vector noise pattern generated procedurally, original), 3 big knobs (DECAY, MIX, TONE), PRE-DELAY and SIZE small knobs, TYPE 5-position rotary with icons (room, hall, plate, spring coil, cloud); FREEZE tiny footswitch.
Tests: RT60 measured (Schroeder backward integration) within 10% of DECAY for Hall 1..8 s; energy-stable (output decays monotonic, no growth) for all types and parameter sweeps for 60 s random; frequency response of tail: HF decays faster at tone<0; mono compat: L+R sum not cancelling (> -6 dB vs L); modal density/echo density grows (normalized echo density > 0.9 by 200 ms for Hall); freeze holds level ±1 dB for 10 s; no clicks on type change (50 ms xfade / dip); CPU within budget; no denormal CPU spike (10x slowdown guard) at silence input.

## 8. Tremolo  `generic.tremolo`  (family: Modulation)  Face: Standard, colour black/sparkle gold, accent
Purpose: amplitude tremolo with several waveforms, tempo sync, stereo auto-pan and optional "harmonic" (split-band) mode.
| # | id | label | range | default | taper | unit |
|---|---|---|---|---|---|---|
|1| gen_trem_rate | RATE | 0.2..16 Hz | 4.5 | exp | Hz |
|2| gen_trem_depth | DEPTH | 0..1 | 0.6 | LIN | - |
|3| gen_trem_wave | WAVE | {Sine, Triangle, Square(soft), Opto-ish, Harmonic} | Sine | enum | - |
|4| gen_trem_shape | SHAPE | 0..1 (pulse width / soft edge / duty) | 0.5 | LIN | - |
|5| gen_trem_stereo | STEREO | {Mono, Pan (auto-pan), Dual-phase} | Mono | enum | - |
|6| gen_trem_phase | PHASE | 0..180 deg R vs L | 90 (used when Dual/Pan fine tuning) | LIN | deg |
|7| gen_trem_sync | SYNC | {Off, 1/4, 1/4T, 1/8, 1/8., 1/8T, 1/16, 1/2, 1 bar, 2 bars} | Off | enum | - |
|8| gen_trem_level | LEVEL | -12..+6 dB | 0 | dB-lin | dB |
|9| gen_trem_tap | TAP | Trigger (sets rate) | 0 | trigger | - |
|10| gen_trem_on | | | 1 | | |
DSP [DOC: Zölzer DAFX; Opto behaviour: Lp/attack asymmetry per published LA-2A style T4 opto, simplified]:
- Gain `g(t) = 1 - depth * u(t)` where u in [0,1]: u = (1 - lfo)/2 (lfo in [-1,1], max gain at lfo=+1). So depth 1 reaches silence at valley; gain unity at peak.
- Waves: Sine `lfo = sin(2πφ)`; Triangle `lfo = 1-4|φ-0.5|`... (phase-corrected to start at +1); Square soft: `lfo = tanh(k*sin(2π(φ+ ... )))/tanh(k)` with k = 1 + 40*shape (PWM: threshold `sin > cos(π*duty)`), duty = shape; Opto-ish: `u` passed through asymmetric one-pole (attack 8 ms rise, release 40 ms fall) and expo curve `u^1.5` (lamp-like); Harmonic: split input at 700 Hz (2nd-order LR crossover), low band gets `g(t)`, high band gets `g(φ+0.5)` (inverted), summed → vocal/phase-shifty harmonic tremolo, depth scales both.
- Stereo: Mono: same g both; Pan: L gain `1 - depth*u`, R gain `1 - depth*(1-u)` with equal-power law `cos/sin` (depth scales pan width); Dual-phase: R LFO phase = L + `phase`.
- Level: output gain. Gain smoothed 1 ms to avoid zipper on param changes (not on LFO).
- Phase continuity: LFO phase accumulator is never reset; rate changes preserve phase; sync mode locks to `ppq` (`φ = frac(ppq * div)`) when `playing`, else free-run with phase continuity glide.
- Tap: 2-tap min, avg of last 3 intervals, rate range clamp. TAP → sets `rate` param via model message (UI shows).
- LFO precision: double phase; sine by table 4096 + linear interp or std::sin per 4 samples; error < -100 dB.
Bypass: DSP (no tail), xfade 5 ms. Channel: Stereo. CPU 0.2%.
MIDI native: CC 104 rate, 105 depth, 106 wave, 107 shape, 108 stereo, 109 phase, 110 sync, 111 level, 112 tap, 113 on; MIDI clock sync via sync; aftertouch default → depth (+0.5).
Visual: black body with sparkle-gold vector speckle pattern (procedural), 3 knobs RATE/DEPTH/SHAPE, WAVE as 5-position rotary with tiny waveform icons, STEREO 3-way toggle, tap switch; LED pulsing with the LFO (brightness = g).
Tests: RMS gain vs depth correct: at depth d valley gain = 1-d exactly; sine rate error <0.1%; no DC; Pan mode L²+R² constant within 0.1 dB; Harmonic: bands inverted (low/high gain complementary); sync: phase locked to ppq within 0.5 ms; param sweeps no clicks (max sample step bounded); mix with silence outputs silence.

## 9. Wah (optional)  `generic.wah`  (family: Filter)  Face: Treadle 54x150, colour chrome/black rubber pad
| # | id | label | range | default | taper | unit |
|---|---|---|---|---|---|---|
|1| gen_wah_pedal | PEDAL (treadle position) | 0..1 | 0.5 | LIN | - | expression target, exp1 default |
|2| gen_wah_range_lo | FREQ LOW | 250..600 Hz | 350 | exp | Hz |
|3| gen_wah_range_hi | FREQ HIGH | 1.2..3.2 kHz | 2200 | exp | Hz |
|4| gen_wah_q | Q | 2..12 | 6 | exp | - |
|5| gen_wah_mode | MODE | {Treadle, Auto(env), LFO} | Treadle | enum | - |
|6| gen_wah_sens | SENS | 0..1 (Auto) | 0.5 | LIN | - |
|7| gen_wah_rate | RATE | 0.2..10 Hz (LFO) | 1.5 | exp | Hz |
|8| gen_wah_mix | MIX | 0..1 | 1 | LIN | - |
|9| gen_wah_toe | TOE SWITCH | {Off, On} auto-engage when pedal>0.97 (footswitch in toe) | Off | enum | - |
|10| gen_wah_on | | | 1 | | |
DSP: Chamberlin/ZDF state-variable filter bandpass (Cytomic/Zavalishin TPT SVF, stable to Nyquist/6, per-sample coefficient from smoothed f): `f = flo * (fhi/flo)^pos` (exp taper), pos from pedal (smoothing 8 ms) / envelope follower (att 2 ms, rel 80 ms, `pos = clamp(env*sens*8)`) / LFO sine. Q fixed per setting; output = BP * (1 + (Q^-1 compensation)) normalised so peak gain ≈ +12 dB at resonance [EST]; Optional mix with dry (dry parallel yields notch-like). Pre-filter soft-saturation `tanh(0.5x)` before SVF (Normal quality 2x oversampling optional). Treadle drag: mouse vertical drag, wheel, MIDI CC 11 (expression, default exp1), aftertouch → pedal. Toe switch: when pedal>=0.97 → onFoot(press) toggles bypass (hysteresis 0.9). Bypass True, Stereo, CPU 0.5%.
MIDI native: CC 11 pedal, 114 q, 115 mode, 116 sens, 117 rate, 118 mix, 119 on.
Visual: foot treadle with vector rubber ridges, angled 20 deg rotation tied to position, small knobs on side plate (Q, RANGE), LED on toe.
Tests: centre frequency vs position monotonic and matches `flo*(fhi/flo)^pos` ±3%; Q measured (-3 dB bandwidth) ±15%; envelope mode follows level; stable at fast sweeps (no NaN); toe switch hysteresis.

## 10. Phaser (optional)  `generic.phaser`  (family: Modulation)  Face: Standard, colour orange, accent black
| # | id | label | range | default | taper | unit |
|---|---|---|---|---|---|---|
|1| gen_phaser_rate | RATE | 0.05..10 Hz | 0.5 | exp | Hz |
|2| gen_phaser_depth | DEPTH | 0..1 sweep range | 0.8 | LIN | - |
|3| gen_phaser_feedback | FEEDBACK | -0.9..0.9 (centre = 0, detent) | 0.3 | LIN | - |
|4| gen_phaser_stages | STAGES | {2, 4, 6, 8, 12} | 4 | enum | - |
|5| gen_phaser_mix | MIX | 0..1 | 0.5 | LIN | - |
|6| gen_phaser_center | CENTER | 200..2000 Hz (log centre of sweep) | 700 | exp | Hz |
|7| gen_phaser_sync | SYNC | like chorus | Off | enum | - |
|8| gen_phaser_on | | | 1 | | |
DSP [DOC: Smith "Physical Audio Signal Processing" allpass phasers; Välimäki]: N first-order allpass stages `A(z) = (a + z^-1)/(1 + a z^-1)`, `a = (tan(πf/sr)-1)/(tan(πf/sr)+1)`, f modulated exponentially `f = center * 2^(depth*2.5*lfo)` ([center/5.6, center*5.6]), sine LFO, stages share a (updated every 8 samples); feedback from last stage to input with `fb` signed, sat `tanh(0.9x)` in loop for stability; mix equal-sum (`dry + wet*mix` with notches when mix=0.5; mix up to 1 = all-pass chain). Stereo: right channel LFO +90deg (MonoInStereoOut) optional via same LFO phase; Stages even count → notch pairs. Bypass True with 5 ms xfade; CPU 0.6%. MIDI native (avoids CC 120-127 channel-mode messages): CC 14 rate, 15 depth, 16 feedback, 17 stages, 18 mix, 19 center, 3 sync, 9 on. Visual: orange body, 4 knobs (RATE, DEPTH, FEEDBACK, MIX), STAGES 5-position selector; small LED breathing with LFO.
Tests: notch frequencies equal N/2 count; notch frequency tracks LFO ±3%; stability for fb 0.9 at 12 stages (peak < +12 dB); LFO/sync tests as chorus.

---
## 11. Cross-pedal acceptance
- Registry: all 10 register under `generic.*` ids via `PedalRegistry::registerAll()`; `PedalRegistry::all()` order = this document order. Test T-REG: ids unique, snake_case, every `ParamDesc.id` starts with `paramPrefix`, ids unique across registry, descriptor param count <= kMaxParams (32) and lane count <= kMaxLaneParams (16).
- Every param: round trip norm->value->norm ±1e-6; discrete steps exact; `getParameterText` strings include unit.
- Determinism: same input/params -> bit-identical output on two runs (no random without seeded generator).
- Block independence: 1..2048, non-pow2, split at random points: |diff| < 1e-9 (T-BLK, BOARD_CORE 15).
- Bypass: enable toggle crossfades in 5 ms (±0.5 ms) and produces no step > natural step * 1.5; dry latency-matched when oversampled.
- RT: no alloc/lock in process/setParam/onMidi/onFoot (allocator hook).
- CPU: each pedal within stated [EST] share; [MEAS] to replace.
- Chain-order proofs: OD->Delay vs Delay->OD distinguishable; Fuzz->Comp->Delay gives expected spectra; Chorus before vs after OD differs.
- Visual: golden-image snapshot (original art) at 100/150/200% scale; accessibility names present; knob min/max keyboard reachable.
- Legal: art original, names generic, no brand marks; fuzz/overdrive/etc. descriptions are family-level, no product clone claims.

## 12. Implementation order
1. PedalBase + PedalRegistry + `gen-boost` (simplest) - proves API/tests.
2. `gen-tremolo`, `gen-compressor` (no oversampling).
3. `gen-overdrive`, `gen-fuzz` (oversampler, aliasing tests).
4. `gen-chorus`, `gen-phaser`, `gen-wah` (modulation/filters, sync).
5. `gen-delay` (sync/tap/feedback safety with BOARD_CORE 6.6).
6. `gen-reverb` (CPU heavy; FDN, Dattorro plate).
Parallelisable per pedal after step 1; shared helpers in `Generic/Common/` (LFO.h, Biquad.h, TptSvf.h, OnePole.h, FractionalDelay.h, EnvelopeFollower.h, SoftClip.h, TempoSync.h, Smoothers.h) — each unit-tested.
