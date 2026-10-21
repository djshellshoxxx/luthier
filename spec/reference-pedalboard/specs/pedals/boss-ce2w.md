# boss-ce2w — BOSS CE-2W Waza Craft Chorus (inspired-by) — DEV SPEC

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `boss-ce2w`, **paramPrefix `boss_ce2w_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/boss_ce2w.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/boss_ce2w_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_boss_ce2wTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `boss_ce2w_bypass`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims (none) -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `boss_ce2w_noise`, `boss_ce2w_hq`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): `boss_ce2w_sync`, `boss_ce2w_sync_div`.
> - Host lanes `ref_s{nn}_p01..` in this order: `boss_ce2w_rate`, `boss_ce2w_depth`, `boss_ce2w_mode`, `boss_ce2w_out_mode`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Tags: DOC = documented by retailer/manufacturer listing; MEAS = measured by third party (forum/analysis); EST = estimated, must be verified on hardware.
Prefix for params: `boss_ce2w_`. Model id: `boss-ce2w`. Code: `Source/ReferencePedals/Models/boss_ce2w.h/.cpp`, test `Source/Tests/ReferencePedals_boss_ce2wTests.cpp`.
Research caveat (updated 2026-10-21, pass 2): boss.info, electrosmash.com, manuals.plus, hobby-hour.com all still DNS ENOTFOUND; web.archive.org blocked by the fetch tool. Reached only via WebSearch summaries and one WebFetch of github.com/LuhOli42/OpenGuitarMultiFx PR #115. Values tagged DOC-2ND are second-hand (search-tool summary of a third-party page quoting the manual / ElectroSmash), not read from the primary. See section 11 verification log. Nothing here is a hardware-verified claim. Name usage: "inspired by BOSS CE-2W"; no manual text or photos copied.

## 1 Identity / variants
- BOSS CE-2W Waza Craft Chorus, all-analog BBD, made in Japan, released 2016 for BOSS 40th anniversary (DOC: scan.co.uk, oxbowaudio listings).
- 3 modes: STANDARD (= CE-2 chorus, now with stereo out), CE-1 CHORUS, CE-1 VIBRATO (DOC). CE-1 modes reproduce stereo chorus/vibrato of Roland/BOSS CE-1 with variable depth (original had fixed) (DOC).
- Not the same as CE-2 (`models/boss-ce2.md`, MN3101/MN3007 mono, 1979-1982) nor CE-1 (Roland, 1976, MN3002/MN3101 era, tall box). Reuse the BBD core, different voicing per mode.
- Variants to NOT model: CE-2 original (mono), CE-5 (EQ), CE-1 rackbox.
- Sources: https://www.boss.info/global/products/ce-2w/ (manual, not fetched), https://www.electrosmash.com/boss-ce-2-analysis (CE-2 circuit, not fetched; snippet only), https://diystompboxes.com/smfforum/index.php?msg=242342 (clock 1/X law), https://groupdiy.com/threads/bbd-chorus-debugging.73779/ (LFO levels), https://electro-music.com/event2008/forum/post-227741.html (clock 71.7 kHz in one build), https://www.americanmusical.com/p/boss-ce-2w-chorus-waza-craft-effects-pedal (specs).

## 2 Controls
| Control | Type | Range | Taper | Default | Real behaviour |
|---|---|---|---|---|---|
| RATE | knob | 0..10 (UI 0..1) | log-ish (B/A mix) EST; map to LFO Hz below | 0.35 | LFO speed. STANDARD: ~0.3..10 Hz EST. CE-1 modes: same pot, CE-1 voicing. |
| DEPTH | knob | 0..10 | linear EST | 0.5 | Mod depth. STANDARD: CE-2W Depth. CE-1 CHORUS/VIB: variable depth (new vs CE-1) (DOC). |
| MODE | single 3-position mini slide switch between the knobs (DOC-2ND: Guitar World, Premier Guitar reviews; STANDARD marked "S") | STANDARD / CE-1 CHORUS / CE-1 VIBRATO | - | STANDARD | One control, not two (resolves earlier "2 controls" guess). Plugin: 3-state enum `boss_ce2w_mode`: 0 STD, 1 CE1_CHORUS, 2 CE1_VIBRATO. RATE and DEPTH knobs both exist and are shared by all modes (DOC-2ND Guitar World: "same Rate and Depth controls" as CE-2). |
| Pedal switch | footswitch, latching | on/off | - | on | Buffered bypass (DOC). |
| CHECK LED | red LED | - | - | - | On when effect active; dims/off when battery low (battery check) (DOC). |
Jacks: INPUT (1 Mohm DOC), OUTPUT A (MONO) (output Z 1 kohm, recommended load >=10 kohm; DOC-2ND manuals.plus), OUTPUT B ("direct-only" second jack giving stereo chorus/vibrato; DOC-2ND Guitar World; when only A used it is the summed mono), DC IN 9V centre-negative, 25 mA (retailer listing, unverified this pass). Battery 9V (6LR61/6F22), ~32 h alkaline (retailer listing, unverified). Nominal in/out level: -20 dBu (DOC-2ND manuals.plus manual reproduction; some retailers print +20 dBu, treated as sign typo). Use -20 dBu nominal; calibration 0 dBFS = +14 dBu stays ESTIMATED (common convention, no source).
No MIDI, no expression jack, no tap tempo on real unit (DOC: none listed). Physical: 73 W x 129 D x 59 H mm, 450 g incl battery (DOC, one listing).

## 3 Signal path and DSP model (EST except where tagged)
STANDARD (CE-2 topology, per ElectroSmash/forums):
1. Input buffer: JFET/transistor, Zin 1 Mohm, HPF ~ 10 Hz.
2. Split: dry path and wet path. Wet path: pre-emphasis (shelf ~+6 dB above ~1 kHz EST), 2nd-order Sallen-Key LPFs at 6.59 kHz and 6.93 kHz (DOC-2ND: ElectroSmash CE-2 values 6.6/6.9 kHz as quoted in OpenGuitarMultiFx PR #115; used for both anti-alias and reconstruction stages, CE-2 not CE-2W), BBD (original CE-2 = MN3007, 1024 stages, with MN3101 clock; CE-2W chips unknown, MN3207/MN3102 pair is the modern equivalent EST), de-emphasis (inverse of pre-emphasis; note CE-2 has no true compander, only emphasis EST).
3. Clock: MN3101 driven by LFO; clock period linear in control voltage => delay time linear in LFO voltage (1/X law, MEAS forum). Delay range STANDARD: 3..12 ms centre ~6.5 ms at depth max EST (BBD 1024 stages with clock 71.7 kHz => 2*N/fclk = 14.3 ms; MEAS single build, so min clock ~ 85 kHz..max ~ 300 kHz). Use: d(t) = d0 + dd*depth*lfo(t), d0=6.5 ms, dd=±4 ms EST.
4. LFO: triangle, integrator-type; range 1.2..4.5 V at integrator, shaped to ~0.7..3.3 V at clock chip (MEAS groupdiy, earlier pass, not re-verified). Original CE-2 max rate 3.56 Hz (DOC-2ND: ElectroSmash figure quoted in PR #115). CE-2W rate range not found: ESTIMATED 0.3..4 Hz (CE-2 max with small headroom; the earlier 10 Hz max is withdrawn as unsupported).
5. Mix: wet + dry summed 1:1 (STANDARD mono OUT A). STANDARD stereo: OUT A = dry + wet, OUT B = dry - wet (opposite phase wet) EST (CE-2W "expansive sound field").
CE-1 CHORUS: CE-1 voicing: dry + wet, but with chorus/vibrato from CE-1 mix; outputs A/B stereo with opposite-polarity wet (CE-1 "stereo chorus" known to be L=dry+wet, R=dry-wet, DOC description "stereo"). Add CE-1 preamp colour: gentle even-order tilt (+2..3 dB at 3 kHz, soft clip at ~+6 dBu EST); noise floor slightly higher.
CE-1 VIBRATO: dry muted; 100% wet; LFO controls delay only; vibrato rate/depth from same knobs; pitch deviation max ±~25 cents EST.
Bypass: buffered (DOC), true analog buffer; bypass transition: crossfade 5 ms (avoid click; real unit has relay-less electronic switch, click tiny) — EST. Latency: wet path group delay = d(t); dry path 0 samples; bypass 0.
Clipping: input and BBD stages soft-clip; model as tanh(x/Vclip)*Vclip with Vclip=4.5 V (rail = Vref 4.5 V, DOC/MEAS). BBD noise: add shaped noise floor -78 dBFS wideband (EST) with HF-weighting, `boss_ce2w_noise` internal switch default on.

## 4 Plugin algorithm
Block diagram: `in -> InBuf(HPF 10Hz) -> split -> [dry] / [PreEmph -> AA-LPF -> FracDelay(d(t)) -> Recon-LPF -> DeEmph -> Clip] -> Mixer(mode) -> OutA/OutB`.
Per-sample (fs = host, internal rate fo = 2*fs if fs<=48k else fs):
- lfo phase p += rate_hz/fo; tri = 1 - 4*|p - 0.5|... map to [-1,1].
- delay_ms = d0 + dd*depthS*tri (STANDARD); CE-1 chorus d0=7.0, dd=±3.5 (EST); CE-1 vib d0=4.5, dd=±2.5.
- fractional read: 4-point Lagrange (hermite) from circular buffer of 40 ms @fo, preallocated in prepare().
- BBD staircase emulation (optional HQ flag): sample-and-hold at fclk = N/(2*delay_s) using zero-order hold then recon LPF; default off (cheap), on in Hi-Q mode. Aliasing: AA-LPF as 4th-order Butterworth biquad cascade at 7 kHz; oversample 2x (polyphase FIR 31-tap, ~0.1 dB ripple) for the clipper. Denormals: juce::ScopedNoDenormals + add 1e-18 DC-free alternating bias in feedbackless filters. Smoothing: depth, rate, mix via one-pole 20 ms (ramp per block, `SmoothedValue`). LFO phase continuous across rate change (phase accumulator).
- CPU budget: <1.0% of 1 core @48k stereo, buffer 128 (target), hard cap 2%. No alloc/locks/exceptions on audio thread; params read as `std::atomic<float>`; mode change crossfade 10 ms with two voices running (mode change not click).
- Mono-in stereo-out: if host input stereo, sum to mono pre-wet (hardware has 1 input).

## 5 I/O and routing
- Real: 1 in, OUT A (mono), OUT B. Plugin: mono-sum in; OUT A/B -> board L/R.
- `boss_ce2w_out_mode` enum: A_ONLY (mono sum A to both), STEREO (A=L, B=R).
- Board tricks to expose (via Board routing graph): (a) series: any pedal before/after; (b) parallel: split input to 2 CE-2W instances; (c) effects-loop feedback: board-level feedback send/return allowing OUT B fed back to a pre-pedal with feedback gain 0..0.7 and limiter (hardware has no internal feedback path; this is a board-level trick, not a pedal feature); (d) stereo split: OUT A to amp A, OUT B to amp B / dual chain; (e) polarity-flipped stereo to mono sum cancels dry (vibrato-like) — model naturally by the opposite polarity wet. Feedback safety: soft-clip + DC blocker + 6 dB/oct limiter above gain 0.9.

## 6 MIDI / sync / expression emulation
Real unit: none. Plugin extras (clearly marked virtual, not on hardware):
- MIDI clock sync: optional `boss_ce2w_sync` (off default). Rate = clock BPM mapped: rate_hz = bpm/60 * (4 / division), divisions 1/4,1/8,1/4T... max clamp 10 Hz.
- Tap tempo: footswitch long-press 0.5 s sets tempo from two taps (virtual extra).
- CC map (virtual): CC14 rate, CC15 depth, CC16 mode (0-42/43-85/86-127), CC17 bypass(>=64 on), CC18 out_mode. PC: none; PC0-2 select mode.
- Expression: assignable target (rate or depth) via board MIDI_EXPRESSION.md global mapper, 0..1 over knob range.
- Gestures: velocity -> depth scale for 150 ms bursts (note-on with Ch pressure) optional; aftertouch -> depth with 30 ms smoothing; footswitch velocity -> bypass crossfade time (5..40 ms). All opt-in, default off.

## 7 Visuals
- Enclosure: BOSS compact (73 x 129 mm top face), light-blue/teal body (CE-2W is a Waza Craft in the light-blue family; EST, verify) with white WAZA CRAFT lettering, metallic silver knobs, black foot pedal switch spanning lower ~45%. Original vector art only.
- Layout (top view, origin top-left of 73x129): RATE knob centre (17.5, 18) mm dia 12; DEPTH knob (55.5, 18) dia 12; MODE slider/switch (36.5, 40) 20x8 mm; CHECK LED (36.5, 52) dia 3 red; pedal switch (36.5, 86) 53x62 mm area with rubber texture lines. Jacks on top edge: INPUT (left side), OUTPUT A/B (right side), DC IN (top). Labels: "CE-2W", "CHORUS", "RATE", "DEPTH", "STANDARD / CE-1 / CHORUS-VIBRATO", "CHECK", "OUTPUT A (MONO)", "OUTPUT B", "INPUT". Knob pointer: white line, travel 270 deg (-135..+135).
- Footprint for Board: w 73 mm, d 129 mm, h 59 mm (DOC). Weight 450 g.

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | automatable |
|---|---|---|---|---|---|---|---|---|
| boss_ce2w_rate | Rate | 0 | 1 | 0.35 | norm (0.3..4 Hz, EST; CE-2 max 3.56 Hz DOC-2ND) | log (k=3) | 20 | yes |
| boss_ce2w_depth | Depth | 0 | 1 | 0.5 | norm | linear | 20 | yes |
| boss_ce2w_mode | Mode | 0 | 2 | 0 | enum STD/CE1_CHO/CE1_VIB | n/a | crossfade 10 | yes |
| boss_ce2w_bypass | Engage | 0 | 1 | 1 | bool (1=effect engaged, per RECONCILED header; supersedes "1=bypassed") | n/a | 5 | yes |
| boss_ce2w_out_mode | Output | 0 | 1 | 1 | enum A_ONLY/STEREO | n/a | 10 | no |
| boss_ce2w_sync | MIDI sync (virtual) | 0 | 1 | 0 | bool | n/a | 0 | yes |
| boss_ce2w_sync_div | Sync division | 0 | 5 | 1 | enum | n/a | 0 | yes |
| boss_ce2w_noise | BBD noise | 0 | 1 | 1 | bool | n/a | 0 | no |
| boss_ce2w_hq | BBD staircase HQ | 0 | 1 | 0 | bool | n/a | 0 | no |
rate_hz = 0.3 * (4/0.3)^rate EST (rate=0.5 -> ~1.1 Hz). Bypass values stored in normalised form.
Section 9 LFO test: replace "0.3/~1.7/10 Hz" with 0.3/~1.1/4 Hz +-10%.
Section 6 CC map, tap tempo, velocity/aftertouch gestures: these are plugin-only virtual extras; real unit has no MIDI (OMIT as hardware features; kept only as optional non-authentic extras). The "PC0-2 select mode" and CC chart are invented plugin conventions, not a documented chart.

## 9 Tests / acceptance
All thresholds PROVISIONAL until hardware capture (EST).
- Silence in -> silence out, DC < 1 mV-eqv after 10 s, no NaN/Inf for 30 min soak with randomised params and block sizes 1/4/16/64/512, at 44.1/48/96 kHz.
- Impulse: STANDARD wet impulse arrives at d0 +-0.5 ms at depth 0; vibrato mode: no dry peak (dry attenuation > 60 dB).
- FRF checkpoints (STANDARD, depth 0, wet only): -3 dB at 6.5 kHz +-1.5 kHz; -24 dB at 15 kHz min; comb notches near 1/(2*d0)=77 Hz spacing of 154 Hz EST. Mix FRF at 5 rate positions and 4 levels (-30,-20,-10,0 dBFS).
- THD at 1 kHz, -10 dBFS: <1.0% STANDARD; CE-1: 1..3% at 0 dBFS EST. Two-tone IMD < -50 dB at -12 dBFS.
- LFO: rate_hz at boss_ce2w_rate=0/0.5/1 equals 0.3/~1.7/10 Hz +-10%; triangle symmetry > 98%.
- Stereo: CE-1 modes OUT A/B wet polarity opposite; sum(A+B) = 2*dry (wet cancels) within 0.5 dB; vibrato mode dry < -60 dB.
- Bypass/null: bypassed output equals input delay-compensated null < -90 dBFS (buffered bypass flat +-0.1 dB). On/off click peak < -40 dBFS.
- Mode change: no discontinuity > -30 dBFS spike; rate change smooth: LFO phase continuity (no step).
- CPU: <1% core. UI: all knobs reachable, state save/restore round-trip exact, MIDI sync locks within 1 cycle, undo-safe.
- Determinism: noise off => bit-identical across runs.

## 10 Known inaccuracies / TODO
- T0 (partly closed pass 2): control set confirmed second-hand (RATE, DEPTH, one 3-way mode slider, A/B outs). Official manual PDF and ElectroSmash still unread directly; CE-1 depth behaviour and CE-2W-specific delay/filter values remain open.
- Delay times, filter corners, clip levels, noise floor, LFO range all ESTIMATED from CE-2 forum data; CE-2W is a reissue with its own parts (MN3207/MN3102 clone chips uncertain).
- Nominal level (+20 vs -20 dBu) unresolved.
- BBD staircase/clock feedthrough only in HQ mode; no compander artefacts.
- MIDI/tap/expression are virtual extras not on hardware; real unit has none.
- Body colour not verified; verify with licensed photo or leave generic blue.
- No "authentic/identical" claims allowed until measured vs same-revision unit.

## 11 Verification log
Pass 2, 2026-10-21. Reached: WebSearch summaries; github.com/LuhOli42/OpenGuitarMultiFx/pull/115 (WebFetch). Blocked (DNS/policy): boss.info, electrosmash.com, manuals.plus, hobby-hour.com, web.archive.org. DOC-2ND = second-hand summary, primary not read.

| Item | Old -> New | Status | Source |
|---|---|---|---|
| Nominal level | +20 vs -20 dBu unresolved -> -20 dBu in/out | DOCUMENTED (2nd-hand) | https://manuals.plus/boss/ce-2w-waza-craft-chorus-pedal-manual (via search summary); fivestarguitars.com listing |
| Output impedance | 1 kohm -> 1 kohm, load >=10 kohm | DOCUMENTED (2nd-hand) | manuals.plus (same) |
| Input impedance | 1 Mohm -> unchanged | DOCUMENTED (2nd-hand) | manuals.plus / retailers |
| MODE control | "2 controls?" -> one 3-pos mini slider (STD "S"/CE-1 chorus/CE-1 vibrato) | DOCUMENTED (2nd-hand) | https://guitarworld.com/gear/review-boss-waza-craft-ce-2w-chorus-pedal ; https://www.premierguitar.com/gear/boss-ce-2w-waza-craft-chorus-review |
| RATE knob existence | conflicting -> exists, with DEPTH, same as CE-2 | DOCUMENTED (2nd-hand) | Guitar World |
| OUTPUT B | stereo pair -> second "direct-only" jack, stereo chorus/vibrato | DOCUMENTED (2nd-hand) | Guitar World |
| Filter corners | 6..8 kHz AA, 5..6 kHz recon -> 6.59 / 6.93 kHz Sallen-Key (CE-2) | DOCUMENTED (2nd-hand, CE-2 not CE-2W) | https://github.com/LuhOli42/OpenGuitarMultiFx/pull/115 quoting ElectroSmash |
| BBD / clock (CE-2) | MN3007 1024 EST -> MN3007 + MN3101 original | DOCUMENTED (2nd-hand) | hobby-hour CE-2 schematic via search; diystompboxes thread |
| BBD / clock (CE-2W) | MN3207/MN3102 uncertain -> unchanged | ESTIMATED | MN3207<->MN3102 pairing per aionfx.com Azure docs; CE-2W board unseen |
| LFO max rate | 10 Hz -> 4 Hz (CE-2 3.56 Hz + headroom) | ESTIMATED (derived from DOCUMENTED CE-2 3.56 Hz) | PR #115 quoting ElectroSmash |
| LFO min rate | 0.3 Hz -> unchanged | ESTIMATED | CE-2 clone forum data |
| Rate taper/mapping | 0.3*(10/0.3)^r -> 0.3*(4/0.3)^r | ESTIMATED | derived |
| Delay centre/depth (d0 6.5 ms, dd 4 ms; CE-1 7.0/3.5, vib 4.5/2.5) | unchanged | ESTIMATED | derivation in sec 3; no primary found (Fractal forum hints 3-4 ms CE-1 base, anecdotal) |
| Pre-emphasis shelf, clip level, noise floor, 0 dBFS cal | unchanged | ESTIMATED | topology / convention |
| Engage param | bypass 1=bypassed -> boss_ce2w_bypass 1=engaged, default 1 | fix (header rule) | RECONCILED v1.1 block |
| Dimensions 73x129x59 mm, 450 g, 25 mA, ~32 h battery | unchanged | retailer listing, NOT re-verified this pass (search found only shipping box 6x4x3 in) | zzounds.com |
| Body colour | light-blue EST -> unchanged | ESTIMATED / verify | none |
| MIDI/CC/PC/tap tempo/aftertouch/velocity gestures | virtual extras -> real unit has none | OMIT as hardware features; plugin-only conventions, not documented | no MIDI chart exists for this pedal |
| Manual PDF, service schematic, ElectroSmash CE-2W, MIDI chart | not read | UNREACHED | blocked hosts above |
