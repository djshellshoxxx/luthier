# boss-dd3t — BOSS DD-3T Digital Delay (inspired-by) — DEV SPEC

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `boss-dd3t`, **paramPrefix `boss_dd3t_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/boss_dd3t.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/boss_dd3t_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_boss_dd3tTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `boss_dd3t_active`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims (none) -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `boss_dd3t_bits`, `boss_dd3t_conv_sim`, `boss_dd3t_mono_out_dup`, `boss_dd3t_stereo_split`, `boss_dd3t_variant`.  Momentary/Trigger (gesture, not lane-mapped): `boss_dd3t_loop_hold`, `boss_dd3t_tap`.  PF_Ext (virtual extension, state + MIDI-learn only): `boss_dd3t_tap_func`, `boss_dd3t_sync`, `boss_dd3t_sync_div`, `boss_dd3t_at_fb`, `boss_dd3t_tempo_bpm`.
> - Host lanes `ref_s{nn}_p01..` in this order: `boss_dd3t_elevel`, `boss_dd3t_feedback`, `boss_dd3t_time`, `boss_dd3t_mode`, `boss_dd3t_trails`, `boss_dd3t_time_law`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Tags: [D]=DOCUMENTED (manual/spec sheet via search snippets) [M]=MEASURED (none yet) [E]=ESTIMATED (verify vs hardware).
Research note: WebFetch DNS failed for boss.info, adorama, effectsdatabase, stinkfoot, thermionic; values from WebSearch snippets + domain knowledge. Every [E] needs bench confirm before any "accurate" claim. No hardware capture exists.

## Sources
- Owner's manual PDF (snippets only): https://www.adorama.com/col/productManuals/BSDD3T.pdf  [S/M/L ranges, short loop 200-800 ms, E.LEVEL, TEMPO jack]
- Boss product page: https://www.boss.info/ca/products/dd-3t/
- Effects Database: https://www.effectsdatabase.com/model/boss/compact/dd3t
- Retail spec (1 MΩ in, -20 dBu nom in, 1 kΩ out, 10 kΩ load, 45 mA, 420 g): https://www.alamomusic.com/products/boss-dd-3t-digital-delay-pedal
- DD-3 versions/12-bit analysis: https://stinkfoot.se/archives/516 ; https://thermionic-studios.com/wiki/index.php?title=DD-3
- Service manual thread (DD-3(A)/DD-3T schematics exist): https://music-electronics-forum.com/forum/effectification/guitar-effects/16304-boss-dd-3-b-schematics
- Prior notes: spec/reference-pedalboard/models/boss-dd3t.md ; MIDI plumbing: specs/MIDI_EXPRESSION.md ; universal wet/dry is board-level (specs/BOARD_CORE.md), do not duplicate.

## 1 Identity / variants
- id `boss_dd3t`; display "DD-3T Digital Delay" (inspired by BOSS DD-3T). Category delay. Mono in / mono out [D]. DD-3T = DD-3 circuit + tap tempo + TEMPO jack [D, "sound circuitry and controls identical to DD-3"].
- Power: 9V battery or PSA adaptor, 45 mA [D]. Input Z 1 MΩ, nominal in -20 dBu [D]; out Z 1 kΩ, load >=10 kΩ [D]. Weight 420 g w/ battery [D]. Dims 73 W x 129 D x 59 H mm [D].
- Variant enum `boss_dd3t_variant` {0 modern_dd3t}. Only one; reserved {1 dd3_classic_12bit} (E, off by default) for lo-fi 12-bit flavour.
- Bypass: electronic switching, BUFFERED bypass [D-ish, Boss standard]; delay trails are NOT spilled on bypass in the DD-3 family (repeats cut when switched off) [E]. Plugin param `boss_dd3t_trails` default 0 (cut) [E].

## 2 Controls
| ctl | range | taper | default | real behaviour |
|---|---|---|---|---|
| E.LEVEL | 0-10 (norm 0-1) | audio/log-ish B-style [E]; full CW = delay level equals direct level (unity) [D] | 0.50 | wet level mixed with dry. 0 = no repeats audible |
| FEEDBACK | 0-10 (0-1) | linear [E] | 0.40 | regen. 0 = single repeat; max = near self-oscillation, but bounded (loop gain max 0.97 [E]) |
| TIME | 0-10 (0-1) | linear in delay time within MODE range [E] | 0.50 | delay time within selected range; short loop: loop length 200-800 ms [D] |
| MODE | 6-pos rotary [D-ish] | stepped | pos L | S / M / L / tap-1/4 / tap-dotted-1/8 / tap-1/4T. See below |
| pedal switch | momentary, latching state | - | on | bypass toggle; in TAP mode acts as tap; hold ~2 s toggles Bypass<->Tap function [D from reseller text, verify in manual] |
| TEMPO jack | 1/4" TRS for FS-5U/FS-6/FS-7 | - | - | external tap; optional footswitch [D] |
| LED | red | - | - | ON = effect on. In tap mode LED blinks at the tapped tempo [E] |
| IN | 1/4" TS | 1 MΩ | | |
| OUT | 1/4" TS | 1 kΩ | | |
| DC | 9V centre-neg | | | |
MODE ranges [D]: S = 12.5-50 ms, M = 50-200 ms, L = 200-800 ms. Positions: `0 S, 1 M, 2 L, 3 SHORT_LOOP, 4..6 tap subdivisions`. Real knob grouping: three delay ranges + SHORT LOOP (HOLD) + tap subdivisions (quarter-note triplet, dotted eighth, quarter) [D]. Param `boss_dd3t_mode` enum {S,M,L,LOOP,TAP_QT,TAP_D8,TAP_Q}. Tap positions allow up to 800 ms [D]. Exact knob detent geometry [E].
TIME mapping: D_ms = Dmin + (Dmax-Dmin)*time_norm, linear in ms [E] (analog clock-rate sweep is 1/t, but spec says min = 1/4 max so ratio 4:1; clock-rate linear in pot => delay hyperbolic [E]; plugin offers `boss_dd3t_time_law` {0 linear_ms,1 hyperbolic} default 1 hyperbolic: D = Dmax/(1+3*(1-t))... i.e. D = Dmin*Dmax/(Dmax-(Dmax-Dmin)*t); verify).
SHORT LOOP [D]: hold pedal switch: audio immediately before the press is captured and looped while held; TIME sets loop length 200-800 ms; changing TIME during playback changes pitch (loop read-rate change) [D]. Releasing returns to normal delay.

## 3 Signal path / circuit & DSP model
Stages:
1. Input buffer (JFET/opamp), HPF ~ 20 Hz [E], gain 1.
2. DRY path: buffered, unfiltered, always passes while effect on (and in bypass).
3. Wet path: input mix (in + feedback) -> pre-emphasis? None in DD-3 digital (analog compander absent) [E] -> anti-alias LPF (4th-order-ish) -> A/D -> RAM delay -> D/A -> reconstruction LPF -> feedback tap + E.LEVEL.
4. Converter: original DD-3 = 12-bit class [E from analysis], DD-3T same circuit. Model: 12-bit with companding-free quantize + noise shaping off; `boss_dd3t_bits` 12..24 default 12 [E]. Sample rate follows delay (variable sample clock, data memory fixed ~ 64k words [E]): fs_conv = N/Dtotal, N=2^15..2^16 words [E]. Consequence: bandwidth falls with longer delay: S range ~ 12 kHz, M ~ 6-8 kHz?, L ~ 3-4 kHz [E; DD-3 L range audibly dark]. Use LPF fc: S 12 kHz, M 7 kHz, L 3.5 kHz at max time, interpolated by actual D: fc = clamp(0.35*fs_conv, 2.5k, 14k).
5. Feedback path: wet signal -> HPF 100 Hz? no; feedback LPF = same recon filter, gain g = 0.97*fb_norm^1.2 [E]; soft limit tanh(x) at ±1.0 (0 dBFS=1.0 V ref... see below) so runaway is bounded.
6. Mix: out = dry + elevel_gain*wet, with elevel_gain = elevel_norm^1.6 (unity at 1.0) [D unity / E curve].
7. Output buffer, 1 kΩ.
- Clipping topology: input stage soft clip, symmetric tanh with headroom: 0 dBFS maps to 1.0 = +6 dBu? Use ref: -20 dBu nominal = -18 dBFS [D nominal, E mapping]; clip at +14 dBu peak ~ 0 dBFS-ish; feedback loop soft clip as above. Model `y = tanh(x*1.0)`.
- Delay: min 12.5 ms, max 800 ms [D]. No modulation (DD-3 has none), but TIME sweep produces pitch glide (tape-like resample) — keep as-is.
- Latency: reported plugin latency 0; dry untouched.
- Bypass transition: relay-less electronic switching; crossfade dry-through/effect 5 ms equal power (E). Repeats cut on bypass (trails=0) with 10 ms wet fade-out; trails=1 lets feedback run out with fb continuing but input muted.
- Mode-change / tap change: delay-time change glides over 80 ms smoothing (hardware clock slews; audible pitch dive) [E].

## 4 Algorithm for plugin
Block: in(mono sum) -> HPF20 -> split dry | wetIn=(x + fbState) -> tanh-softclip(2x OS when fs<=48k) -> preLPF -> quantize(bits) -> circ buffer write -> read frac(D) -> postLPF -> wet -> fbState=g*lpf_fb(wet) ; out=dry+gE*wet.
Per sample (fs host):
- t_s = smooth(target D_ms, tau=80 ms) in ms domain (one-pole on delay-samples; cross-fade not used; pitch artefact intentional).
- Dsamp = t_s*fs/1000. Fractional read: Hermite 4-pt (Lagrange3 acceptable). Buffer length pow2 >= ceil(0.8*fs)+16 (<=2^17 @96k, grow in prepare()).
- bitcrush quantize only when bits<24: q = round(x*2^(bits-1))/2^(bits-1), TPDF dither 1 LSB; default 12 bit. Applied at stored sample rate: emulate converter fs_conv by sample-and-hold decimation? Default OFF (`boss_dd3t_conv_sim` 0): use LPF only; ON: ZOH at fs_conv with ratio r=fs_conv/fs, linear interp (CPU 2x).
- Filters: 2x biquad Butterworth (4th order) preLPF/postLPF; coefs refreshed when fc changes >1% (compute off per-sample: update every 32 samples, no alloc, std::sin/cos cheap). 1-pole HPF 20 Hz.
- Feedback: fbState delayed by 1 sample extra not needed; loop includes full delay. Denormal: ScopedNoDenormals + flush |s|<1e-20 -> 0 in all states; add ±1e-25 dither? no, use flush.
- Smoothing: E.LEVEL, FEEDBACK 20 ms one-pole; bypass 5 ms; time 80 ms; mode step -> time retarget.
- Short loop FSM: IDLE -> (switch down in LOOP mode) CAPTURE_LATCH: copy last L=lenFromTime samples from ring at press (read from ring head, no alloc; loop store is a separate preallocated 0.8 s buffer) -> PLAYING (loop read ptr, rate r = L_ref/L(t); TIME change alters rate => pitch) -> (switch up) RELEASE: 10 ms xfade back to normal delay line (loop buffer discarded). Input dry continues passing during loop (hardware input is mixed? dry passes [E]).
- Tap FSM (see sec 6). CPU budget: <0.5% core @48k/512 (conv_sim off), <1.0% on. RT: all buffers in prepare(), params via std::atomic<float>*, no locks, no allocs, no logging.
- Aliasing: tanh stage 2x OS (31-tap halfband) when fs<=48k; else none.

## 5 I/O & routing
- Mono TS in/out. Plugin: stereo bus -> (L+R)*0.5 -> pedal -> L=R out (`boss_dd3t_mono_out_dup` 1). Option `boss_dd3t_stereo_split` (virtual, off): ping-pong not on hardware; if on, L tap D, R tap D*0.75 [E, extension].
- TEMPO jack: external footswitch input (momentary, tip=tap, TRS for FS-6 polarity) mapped to board tap source (MIDI note/CC/key).
- No FX loop, no dir out. Board-level tricks supported: (a) series chain order (delay after drive/mod, before reverb), (b) parallel via board A/B split with wet-only (board dry/wet 100%), (c) effects-loop feedback: board send->return routes DD-3T out back to its input; board must hard-limit loop gain <1 and tanh-clip; combined with internal feedback caps loop at +6 dB runaway — oscillation allowed. (d) Two DD-3T series with `time` set to non-integer ratio for pseudo-multitap; (e) stereo split into two DD-3T with different modes.
- Feedback tricks tied to pedal: set FEEDBACK max + sweep TIME for oscillation / pitch dive; SHORT LOOP + TIME for pitch-shifting loop.

## 6 MIDI / sync / expression (virtual; real unit has none [D], label "Virtual MIDI (not on hardware)")
- Tap tempo: pedal switch (when in tap function), TEMPO jack, MIDI note, CC `ccTap` (>=64 rising edge). Tempo = 60000/avg(last 3 gaps) ms; gap range 12.5..800 ms -> D_tap = tapInterval*sub, sub: Q=1.0, D8 (dotted eighth)=0.75, QT (quarter-note triplet) = 2/3 [D]. Valid interval after subdivision must be <= 800 ms (clamp, flash LED); reset history if gap >2 s [E]. Min 2 taps. Tap in M/L/S modes ignored.
- Hold 2 s on switch toggles function BYPASS<->TAP [D from reseller].
- MIDI clock sync (`boss_dd3t_sync` off/on, virtual): tempo from 24ppqn clock or host; D = beat_ms*sub; sub choices by MODE tap positions (Q, D8, QT) plus extras `sync_div` {1/4,1/8,1/8D,1/4T,1/16}; if D>800 ms divide by 2 until <=800. Latch change at beat boundary with 80 ms glide.
- Default CC map (ESTIMATED, remappable): CC14 time, CC15 feedback, CC16 e_level, CC17 mode (0-127 -> 7 steps), CC85 bypass (>=64 on), CC86 tap, CC87 short-loop hold (>=64 = held). Program Change n -> user preset n.
- Expression target (virtual): default FEEDBACK (heel=min, toe=max), alt TIME/E.LEVEL; 20 ms smoothing, min/max settable. (Hardware has no expression jack; closest analog is the TEMPO footswitch.)
- Gesture mapping: footswitch velocity -> bypass xfade time 3 ms (vel127) to 40 ms (vel1) [design]. Keyboard aftertouch (chan/poly) -> FEEDBACK add 0..+0.4 when `boss_dd3t_at_fb` on (swell regen like riding the knob). In SHORT LOOP: note-on = hold start, note-off = release; velocity -> none. Velocity-sensitive tap: tap tempo accepts note-on events; velocity ignored.
- MIDI Start/Stop/Continue: Start resets tap phase only (no effect on audio).

## 7 Visuals
- Enclosure: Boss compact, die-cast aluminium; DD-3T = pale/white-cream body? (DD-3 and DD-3T white/cream-ish silver-white ~#E9E7DD, [E visual]); 73 x 129 mm face, scale 1 mm = 4 px.
- Layout (mm, origin top-left of face): four knobs in 2x2 grid top: E.LEVEL (18, 20), FEEDBACK (55, 20), TIME (18, 44), MODE (55, 44); knob dia 17 mm, black skirted cap with white pointer; scale ticks 0-10 (E.LEVEL/FEEDBACK/TIME) printed 3 mm outside ring; MODE shows printed positions S, M, L, HOLD(SHORT LOOP), tap symbols (crotchet-triplet, dotted-quaver, crotchet) around ring. Red LED centre (36.5, 62) dia 5 mm with 7 mm bezel. Pedal switch pad full lower area 55 x 42 mm, y 80-122 (rounded 6 mm, two grip lines). Jacks drawn on side silhouettes: IN left, OUT right, TEMPO jack top-left side, DC top-right side.
- Text: "DD-3T" 9 mm caps at (36.5, 72), "DIGITAL DELAY" below 3.5 mm; knob labels "E.LEVEL" "FEEDBACK" "TIME" "MODE" 3 mm caps. "BOSS" wordmark NOT used; "inspired by" appears only in the info panel.
- Vector art: original SVG, radial-gradient knob with 11 knurl ticks, 4 slot-head hinge screws bottom, battery door outline. No photo textures.
- LED glow 8 ms attack; in tap mode LED blinks at tempo (50% duty, 40 ms flash).

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | auto |
|---|---|---|---|---|---|---|---|---|
| boss_dd3t_elevel | E.Level | 0 | 1 | 0.50 | norm | pow 1.6 gain | 20 | y |
| boss_dd3t_feedback | Feedback | 0 | 1 | 0.40 | norm | gain 0.97*x^1.2 | 20 | y |
| boss_dd3t_time | Time | 0 | 1 | 0.50 | norm | per time_law | 80 | y |
| boss_dd3t_mode | Mode | 0 | 6 | 2 | enum | step | - | y |
| boss_dd3t_active | Active | 0 | 1 | 1 | bool | step | 5 | y |
| boss_dd3t_loop_hold | Short loop hold | 0 | 1 | 0 | bool | step | 10 | y |
| boss_dd3t_tap | Tap (trigger) | 0 | 1 | 0 | trig | step | - | y |
| boss_dd3t_tap_func | Switch=tap | 0 | 1 | 0 | bool | step | - | n |
| boss_dd3t_trails | Trails | 0 | 1 | 0 | bool | step | - | n |
| boss_dd3t_time_law | Time law | 0 | 1 | 1 | enum | step | - | n |
| boss_dd3t_bits | Bit depth | 12 | 24 | 12 | bits | step | - | n |
| boss_dd3t_conv_sim | Converter sim | 0 | 1 | 0 | bool | step | - | n |
| boss_dd3t_sync | MIDI sync | 0 | 1 | 0 | bool | step | - | y |
| boss_dd3t_sync_div | Sync div | 0 | 4 | 0 | enum | step | - | y |
| boss_dd3t_at_fb | AT->feedback | 0 | 1 | 0 | bool | step | - | n |
| boss_dd3t_mono_out_dup | Mono dup | 0 | 1 | 1 | bool | step | - | n |
| boss_dd3t_stereo_split | Stereo split | 0 | 1 | 0 | bool | step | - | n |
| boss_dd3t_variant | Variant | 0 | 1 | 0 | enum | step | - | n |
| boss_dd3t_tempo_bpm | Tap tempo (read-only) | 30 | 300 | 120 | bpm | lin | - | n |
(boss_dd3t_active canonical bypass: 1 = on.) Board wet/dry applies at board level.

## 9 Tests / acceptance (provisional thresholds until hardware capture)
- Impulse @48k, FB 0, mode S, time min: wet echo at 12.5 ms ±0.3; time max S 50 ms; M 50-200; L 200-800 ms ±0.5% (L max 800.0 ms ±4).
- Feedback decay: impulse, FB 0.4, L/400 ms: successive peaks ratio 0.97*0.4^1.2*bandwidth ≈ 0.32-0.40 ±0.03 (E); FB 1.0 stable (no |y|>2.0 within 60 s), tanh-bounded.
- Tap: 4 taps at 500 ms in TAP_Q -> D=500 ±2 ms; TAP_D8 -> 375; TAP_QT -> 333.3; 2.1 s gap resets.
- Short loop: pressing at t0 captures previous 400 ms (TIME gives 400); loop repeats bit-exact until release; TIME 0->1 shifts pitch by factor 4 over range (+24 st max, 800/200) ±1 st.
- FRF (E.LEVEL 1, FB 0, wet-only via sub of dry): -3 dB corner S ≈ 12 kHz ±3 dB, M ≈ 7 kHz, L ≈ 3.5 kHz ±1 kHz; HPF 20 Hz -3 dB ±5 Hz.
- THD @1 kHz -18 dBFS (nominal) wet: <0.5% (bits 24); 12-bit: noise floor -70 ±5 dBFS, THD+N < 1% [E]. At -3 dBFS feedback loop 3rd-harmonic 1-5%.
- Dry null: boss_dd3t_active=0 -> out == in within -90 dB after 10 ms. E.LEVEL=0 -> output == dry within -80 dB.
- Silence in -> silence out |out|<1e-6, DC <1e-4. Block sizes 1/4/16/64/512 identical within 1e-6. Sample rates 44.1-192k pass. 30 min randomized soak no NaN/Inf, no alloc (instrumented). State save/restore bit-exact. CPU <0.5% @48k/512.
- Bypass click < -60 dBFS jump; delay-time sweep no clicks (|d/dt| bounded).

## 10 Known inaccuracies
- All filter freqs, converter bits, time law, taper values ESTIMATED; no hardware capture, schematic not read directly (fetch failed). Real 12-bit companding/noise character and variable-clock aliasing only approximated (conv_sim).
- Exact MODE positions/detent count and tap-hold behaviour from retail text; verify against manual.
- Trails behaviour on bypass unverified.
- MIDI, expression, sync, AT, stereo split, bit depth, trails are virtual extensions beyond hardware.
- DD-3T cream/white colour is estimate; original DD-3 was white/silver; do not claim photo match.
- Looper section: N/A (SHORT LOOP is a hold of <=800 ms, no overdub/undo; do not conflate with phrase loopers).
