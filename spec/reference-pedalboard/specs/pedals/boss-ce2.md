# boss-ce2 — BOSS CE-2 Chorus (inspired-by) — DEV SPEC

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `boss-ce2`, **paramPrefix `boss_ce2_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/boss_ce2.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/boss_ce2_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_boss_ce2Tests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `boss_ce2_bypass`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims (none) -> `ref_s{nn}_in_db/out_db`; footswitch-mode params `boss_ce2_momentary` -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `boss_ce2_variant`, `boss_ce2_noise`, `boss_ce2_mono_out_dup`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): `boss_ce2_sync`, `boss_ce2_sync_div`, `boss_ce2_sync_reset`, `boss_ce2_tap_enable`, `boss_ce2_at_depth`, `boss_ce2_lfo_phase_offset`.
> - Host lanes `ref_s{nn}_p01..` in this order: `boss_ce2_rate`, `boss_ce2_depth`, `boss_ce2_bbd_mode`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Tags: [D]=DOCUMENTED [M]=MEASURED [E]=ESTIMATED (must be verified vs hardware).
Research note (v1.2 re-verification 2026-10-21): direct fetch of ALL primary sites (electrosmash, hobby-hour, effectsdatabase, anasounds, kimsmil, web.archive.org, Roland PDF hosts) failed again (DNS/proxy 403). Values below come from WebSearch result text only (search-engine summaries of those pages); each is tagged with the page it was attributed to. Tag [S]=sourced from search-result text of the named page (not page-read by us). Every [E] needs bench confirm before any "accurate" claim.

## Sources
- ElectroSmash CE-2 analysis: https://www.electrosmash.com/boss-ce-2-analysis  (MN3007 + MN3101, uPC4558C I/O, TL022 LFO)
- Hobby Hour schematic: https://www.hobby-hour.com/electronics/s/boss-ce2-chorus-schematic.php
- Effects Database: https://www.effectsdatabase.com/model/boss/compact/ce2
- Benjiao CE2 LFO: https://benjiaomodular.com/post/2022-05-22-ce2-lfo/
- Boss CE-2W (successor, NOT original look): https://www.boss.info/global/products/ce-2w/
- Roland CE-2 technical specifications: https://support.roland.com/hc/en-us/articles/201927549-CE-2-Technical-Specifications  (reached via search text)
- Roland CE-2B technical specifications: https://support.roland.com/hc/en-us/articles/201945599-CE-2B-Technical-Specifications
- DIYstompboxes 'CE-2 clock range' (multimeter clock measurements): https://www.diystompboxes.com/smfforum/index.php?topic=120428.0
- ElectroSmash MN3007 BBD page: https://www.electrosmash.com/mn3007-bucket-brigade-devices
- PedalPCB Cepheoid CE-2 service notes thread (service-note copy cited at godiksennet.com): https://forum.pedalpcb.com/threads/cepheoid-ce-2-chorus-service-notes-adjustments.7171/latest
- Not reached (blocked): Boss/Roland CE-2 manual PDF, service-note schematic itself, MIDI chart (none exists: CE-2 has no MIDI).
- Prior notes: spec/reference-pedalboard/models/boss-ce2.md

## 1 Identity / variants
- id `boss_ce2`; display "CE-2 Chorus" (inspired by BOSS CE-2). Category chorus. Mono in / mono out. Production 1979-1992 [S: Roland support CE-2 specs]. MN3007 + MN3101 [D: Hobby Hour via search text].
- Variants: the earlier black-label/silver-screw/Taiwan era split could NOT be sourced; no variant-specific component values found. CE-2B (1987-1995, 10 mA, same 470k in, same 70x55x125 mm, 400 g) is a separate Roland-listed model [S: Roland CE-2B specs], OUT OF SCOPE with CE-2W (reissue: 1M in, 25 mA, 450 g, vibrato/mode; NOT this pedal).
- Plugin variant param `boss_ce2_variant` enum {0 black_label,1 silver_screw}: ESTIMATED, era split unverified; differences = small LPF cutoff offset only. Era years and Taiwan/MN3207 variant claims removed (OMIT).
- Power: 9V battery or AC adaptor, 9 mA @9V [S: Roland CE-2 specs]. Input Z 470k [S: Roland + Hobby Hour]. Rec. load >=10k [S: Roland]. Max input 0 dBm @100 Hz, -10 dBm @1 kHz [S: Roland]. Output Z: unsourced for original (the 1 kohm figure belongs to CE-2W); ESTIMATED ~1k from op-amp output buffer. S/N spec: not found; ESTIMATED real ~ -75..-80 dB.
- Bypass: electronic switching, buffered bypass (dry via buffer) [D].

## 2 Controls
| ctl | range | taper | default | behaviour |
|---|---|---|---|---|
| RATE | 0-10 (norm 0-1) | log (E) mapped to LFO 0.3 (E) .. 3.56 Hz (max, calculated from component values by ElectroSmash [S]) | 0.40 (~0.8 Hz) | LFO speed |
| DEPTH | 0-10 (0-1) | linear (E); stock pot reported 100k linear [S: DIYstompboxes, single forum post]; pot ends tied to ~4.5 V bias [S: music-electronics-forum] | 0.50 | LFO amplitude into clock; 0 = static ~3.4 ms comb (still chorus-ish tone, NOT clean) |
| footswitch | momentary | - | on | toggles effect; red LED lit when on |
| LED | red | - | - | ON = effect active |
| IN jack | 1/4" mono TS | | | 470k |
| OUT jack | 1/4" mono TS | | | low Z ~1k [E, unsourced for original] |
| DC jack | 9V, centre-neg | | | no battery in plugin |
- No Level, no Tone, no mode switch, no stereo, no expression jack, no MIDI [S: Roland spec page lists none; CE-2 has two knobs only].
- Gesture mapping (plugin): footswitch = click/velocity (section 6).

## 3 Signal path / circuit model
Stages (stage-by-stage):
1. Input buffer (uPC4558 half, JFET-free): HPF ~ 15 Hz [E], gain 1.
2. Split: DRY path and WET path summed at output (fixed mix ~ 1:1 [E]; true wet level unity-ish).
3. Pre-emphasis / compander-less input filter: BBD input LPF (anti-alias) 4th order-ish, fc typical 6-7 kHz [E, unsourced]. Note: a search summary of ElectroSmash quotes pre/de-emphasis corners ~410 Hz/2.34 kHz and an anti-alias RC of '14.6 Hz' (330k/0.033u); the latter is implausible, the former unverified -> NOT modelled (OMIT); fixed LPFs are ESTIMATED.
4. MN3007 BBD: 1024 stages, 2 phases => delay D = N/(2*fclk) = 512/fclk [D: ElectroSmash formula]. Chip clock range 10-100 kHz (5.12-51.2 ms) per datasheet-derived text [S: ElectroSmash MN3007]; CE-2 runs it faster than that (below).
5. Clock: MN3101 driver, modulated by LFO. MEASURED (multimeter on MN3007 clock pins, one unit, DIYstompboxes 'CE-2 clock range'): depth 0 -> ~148-152 kHz = ~3.4 ms; depth full -> ~113-233 kHz = ~2.2 to ~4.5 ms. Another unit under repair: ~75-115 kHz at full depth (6.7-4.5 ms). Posters attribute spread to clock-cap tolerance (47 pF +/-20%). Model: centre 3.4 ms, depth1 sweep 2.2..4.5 ms (asymmetric: -1.2/+1.1 ms) [M, single unit; tolerance +/-40% across units]. Replaces earlier 5.5 ms centre / 3.0-8.0 ms sweep (ESTIMATED, now superseded).
6. Output reconstruction LPF (2nd-4th order) fc ~ 5 kHz [E]; BBD wet bandwidth ~ 80 Hz-5.5 kHz.
7. Output sum + buffer; dry unfiltered; overall BBD noise floor ~ -70 dBFS-ish clock noise + 'tick' of clock leakage (variant knob `boss_ce2_noise`, off by default).
8. LFO: TL022 triangle [D: ElectroSmash], max rate ~3.56 Hz [S: calculated by ElectroSmash from component values]; followed by RC low-pass with 72 Hz corner [S: ElectroSmash summary] (replaces 'corner ~3x rate' estimate).
- Clipping: BBD input soft-clips (~ +-0.9 V peak at 9V single-supply bias 4.5V; headroom ~ 3 V pk) — model tanh(x/Vc) with Vc = 1.0 for input > -6 dBFS-to-clip ref (0 dBFS = 1.0 V pk) [E].
- Latency: dry 0; wet delay itself. Plugin reports 0 latency (modulated delay is the effect).
- Bypass transition: buffered; crossfade effect-in/out 5 ms equal-power (hardware has small pop-free switch; ~ 3 ms) [E].

## 4 Plugin algorithm
Block: in -> inHPF(15Hz 1p) -> [dry]; -> preLPF -> softclip -> fracDelay(D(t)) -> postLPF -> wetgain -> sum(dry+wet) -> outHPF? none -> out.
Per sample (fs = host rate, internal fs_i = fs, 2x OS only for clip stage when fs<=48k):
- phase += rateHz/fs; tri = 4*|phase-0.5|-1 (range -1..1); lfo = lp1(tri, k=1-exp(-2pi*72/fs)) (fixed 72 Hz corner).
- rateHz = 0.3 * pow(3.56/0.3, rate_norm)  (log; max 3.56 Hz sourced, min 0.3 Hz and log taper ESTIMATED).
- D_ms = 3.4 + depth*lfo*(lfo>=0 ? 1.1 : 1.2)  (ms; MEASURED single unit, see sec 3). D_samp = D_ms*fs/1000.
- Fractional read: 4-point Hermite (or Lagrange3) from ring buffer length ceil(0.012*fs)+8, pow2 sized; no BBD stepping in default mode.
- BBD mode (`boss_ce2_bbd_mode` {0 off,1 on}): sample-and-hold at fclk=512/D, 1.5 fclk-ish; implement via aliased zero-order-hold reading + fclk-tracking LPF fc = 0.42*(fclk/2)*..., cap fc at 6.5 kHz. Costs ~2x; default OFF (cheap Hermite + fixed LPFs).
- preLPF/postLPF: 2x biquad Butterworth fc_pre=6.5kHz, fc_post=5.5kHz (variant silver_screw: 7.5/6.0) [E, unsourced]. Coefs recomputed only on sample-rate change (not per block alloc).
- Wet gain 1.0 (-0.5 dB) [E]; mix dry:wet = 1:1 fixed. Board universal wet/dry (board-level) applies on whole pedal output — do not duplicate inside pedal.
- Smoothing: rate, depth, bypass crossfade: one-pole 20 ms (rate/depth), 5 ms bypass. Rate smoothing in log domain.
- Denormals: juce::ScopedNoDenormals at processBlock + add 1e-18 DC-free flush in filter states (state = |s|<1e-20?0:s).
- CPU budget: <0.4% of one core @48k/512 (Hermite path); <1.0% with bbd_mode. No alloc/lock; all buffers prepared in prepare(); params read via atomic std::atomic<float>*.
- Aliasing: soft-clip stage 2x OS (halfband 31-tap) at fs<=48k; otherwise none.

## 5 I/O & routing
- Mono TS in/out. In plugin: stereo bus -> sum to mono (L+R)*0.5 into pedal; output duplicated L=R (mono pedal semantic) — board option `mono_out_dup` keeps true mono.
- No extra jacks. Tricks supported by board (not pedal): put in effects loop with parallel dry split; series order matters (before reverb/delay; after drive/comp typical); stereo-split via board A/B-split feeding two ce2 with different rate (user must use two instances; each instance independent LFO; optional `boss_ce2_lfo_phase_offset` 0-360 deg hidden advanced param for twin-instance phase-lock).
- Feedback loop tricks: board effects-loop send/return can feed ce2 output back to input (board must hard-limit gain<1 and soft-clip); no internal feedback in hardware (CE-2 has none; do NOT add regen).

## 6 MIDI / sync / expression (emulation; real unit has none [D])
Real hardware: none. Plugin extras (flagged "virtual extension" in UI, off by default; follow board MIDI_EXPRESSION.md for plumbing):
- ALL items in this section: hardware has no MIDI/sync/tap/AT [S]; no MIDI chart exists, so no CC chart can be DOCUMENTED -> hardware MIDI = OMIT. The items below are plugin design choices (not hardware claims), off by default; the CC numbers are arbitrary pedal-LOCAL design assignments.
- MIDI clock sync (`boss_ce2_sync` off/on): rate_hz = bpm/60 * 4/div; div ids {1/1,1/2,1/4,1/8,1/4T,1/8T,1/16}; default 1/4; knob RATE selects div when sync on (stepped). Phase reset on MIDI Start or beat 1 when `boss_ce2_sync_reset`.
- Tap tempo: footswitch double-tap (<500 ms gap, 2 taps) when `boss_ce2_tap_enable`; bpm = 60000/avg(last 3 gaps), clamp 30-300, timeout 2 s.
- CC map (default, user-remappable, 7-bit, ch omni): CC14 rate, CC15 depth, CC85 bypass (>=64 on), CC16 sync div.
- Program Change: PC n loads user preset n (0-127) stored in board bank.
- Expression pedal target default: depth (alt: rate); heel=0, toe=1, 20 ms smoothing, map min/max settable.
- Footswitch gesture: velocity of note-on mapped to bypass crossfade time: vel 127 -> 3 ms, vel 1 -> 40 ms (linear) [design]. Aftertouch (channel/poly) -> depth add 0..+0.5 when `boss_ce2_at_depth`; pressing-hard momentary mode: handled by FootController mode (param `boss_ce2_momentary` removed per reconciled header).
- Slider/knob MIDI-learn: any param, 14-bit NRPN optional.

## 7 Visuals
- Enclosure: Boss compact, die-cast aluminium; CE-2 = light blue (~#4FA5D8 black label era; silver-screw ~#6FB3DE) [E colour, visual estimate]. Hardware 70 W x 125 D x 55 H mm, 400 g [S: Roland CE-2 specs] (replaces 73x129x59 [E]); UI footprint 70 x 125 mm at 1.6 px/mm (112 x 200 px) per reconciled header (replaces 4 px/mm).
- Layout (mm, origin top-left of face, face 70 x 125; positions are design choices scaled from the earlier 73x129 layout): two knobs top row: RATE centre (17, 21), DEPTH centre (53, 21), knob dia 17 mm cap (black skirted, white pointer line). Red LED centre (35, 50) dia 5 mm with 7 mm chrome bezel. Stomp pad (rubber-top, plastic) rect 55 x 42 centred (35, 96) with lower face. Jacks on sides (IN left, OUT right, DC top-right) drawn as side-rail notches only.
- Text: "CE-2" large white at (35, 68) 9 mm cap-height; "CHORUS" below; "RATE" "DEPTH" above knobs, 3 mm caps; "BOSS-style" tagline NOT used (trademark); use "inspired by" in info panel only.
- Vector-art notes: original SVG paths only; knob = radial gradient + 11 knurl ticks; switch pad = rounded rect 6 mm radius with 2 grip lines; bottom hinge screws (4 slot-head). No photographed textures.
- Footswitch clickable zone full lower 55x42 mm; LED glow animated 8 ms.

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | auto |
|---|---|---|---|---|---|---|---|---|
| boss_ce2_rate | Rate | 0 | 1 | 0.40 | norm | log→0.3(E)..3.56 Hz | 20 | y |
| boss_ce2_depth | Depth | 0 | 1 | 0.50 | norm | lin | 20 | y |
| boss_ce2_bypass | Engage (PF_Engage; 1 = effect on) | 0 | 1 | 1 | bool | step | 5 | y |
| boss_ce2_variant | Variant | 0 | 1 | 0 | enum | step | - | n |
| boss_ce2_bbd_mode | BBD sim | 0 | 1 | 0 | bool | step | - | n |
| boss_ce2_noise | BBD noise | 0 | 1 | 0 | norm | lin | 50 | y |
| boss_ce2_sync | Sync | 0 | 1 | 0 | bool | step | - | y |
| boss_ce2_sync_div | Sync div | 0 | 6 | 2 | enum | step | - | y |
| boss_ce2_sync_reset | Reset on start | 0 | 1 | 1 | bool | step | - | n |
| boss_ce2_tap_enable | Tap enable | 0 | 1 | 0 | bool | step | - | n |
| boss_ce2_at_depth | AT→depth | 0 | 1 | 0 | bool | step | - | n |
| boss_ce2_lfo_phase_offset | LFO phase | 0 | 360 | 0 | deg | lin | 0 | n |
| boss_ce2_mono_out_dup | Mono dup | 0 | 1 | 1 | bool | step | - | n |
(`boss_ce2_bypass` is the single canonical engage param, 1 = effect on, per reconciled header; the earlier duplicate `boss_ce2_active` and the `boss_ce2_momentary` row are removed.)

## 9 Tests / acceptance
- Impulse @48k: dry spike at t=0 plus wet spike at 3.4 ms +-0.3 ms (depth 0) [M]. Wet delay sweep with depth 1: min 2.2 +-0.2, max 4.5 +-0.2 ms [M, single unit].
- LFO period: rate 0 -> 3.3 s +-5% [E]; rate 1 -> 281 ms +-5% (3.56 Hz) [S].
- FRF depth 0: comb notches at f=(2k+1)/(2D): first notch 1/(2*3.4ms)=147 Hz; with wet LPF the nulls above 5.5 kHz fade. Check 100 Hz-5 kHz within +-1.5 dB of model; wet gain at 10 kHz < -30 dB.
- THD @1 kHz, -12 dBFS: <0.3% (bbd off), 0.5-3% (bbd on) [E]; @ -3 dBFS clip: 2-6% 3rd harmonic dominant.
- Sidebands: 1 kHz sine, depth 1, rate 5 Hz -> FM sidebands at +-5 Hz multiples, peak phase dev ≈ 2*pi*1kHz*1.1ms ≈ 6.9 rad (rate 5 Hz exceeds max 3.56 Hz: use 3 Hz) verify by FFT.
- Silence in -> silence out (|out|<1e-6 with noise 0); no DC (<1e-4); bypass null test: boss_ce2_active=0 output == input within -90 dB after 10 ms crossfade; on/off click < -60 dBFS jump.
- Block sizes 1/4/16/64/512 yield identical output within 1e-6; sample rates 44.1-192k; CPU budget in sec 4; 30 min randomized soak no NaN/Inf; state save/restore bit-exact.
- Provisional thresholds until hardware capture.

## 10 Known inaccuracies
- Delay centre/sweep and clock range are MEASURED on one unit (multimeter, forum) with ~+/-40% unit-to-unit spread; LFO max rate is calculated (ElectroSmash), LFO min rate, pot tapers, filter cutoffs, mix level, clip level, HPF are ESTIMATED; variant era split unverified.
- BBD companding absent in CE-2 (none) — modelled with none; noise & clock tick approximations.
- Pot tapers not verified (depth pot reported 100k linear, one forum post). No input impedance interaction with guitar pickups modelled (add optional `boss_ce2_load_z` later).
- No hardware capture exists; no "accurate/identical" claims. CE-2W image is not the original CE-2.
- MIDI/sync/tap/AT features are virtual extensions beyond real hardware.
- Looper section: N/A (CE-2 is not a looper).

## 11 Verification log
Access: all primary pages (ElectroSmash, Hobby Hour, effectsdatabase, anasounds, archive.org, Roland PDFs) unreachable by fetch (DNS/proxy 403); only WebSearch result text read. "S" = stated in search-result text attributed to that page, not page-read.

| item | old -> new | status | source |
|---|---|---|---|
| Delay centre | 5.5 ms -> 3.4 ms | MEASURED (1 unit, multimeter) | https://www.diystompboxes.com/smfforum/index.php?topic=120428.0 |
| Delay sweep depth 1 | 3.0-8.0 ms -> 2.2-4.5 ms | MEASURED (1 unit) | same |
| Clock range | 20-170 kHz -> ~148-152 kHz @depth 0, 113-233 kHz @full | MEASURED (1 unit) | same |
| Delay formula | 512/fclk -> unchanged | DOCUMENTED | https://www.electrosmash.com/mn3007-bucket-brigade-devices |
| LFO max rate | ~13 Hz -> 3.56 Hz | DOCUMENTED (calculated by ElectroSmash, S) | https://www.electrosmash.com/boss-ce-2-analysis |
| LFO min rate | 0.3 Hz -> unchanged | ESTIMATED (no value found; ~0.3 Hz guess) | derivation: typical chorus LFO floor |
| LFO shape | triangle -> unchanged | DOCUMENTED | ElectroSmash CE-2 analysis |
| LFO smoothing | corner 3x rate -> fixed 72 Hz RC | DOCUMENTED (S) | ElectroSmash CE-2 analysis |
| Rate/depth tapers | log/linear -> unchanged | ESTIMATED (depth pot 100k lin from one forum post) | https://www.diystompboxes.com/smfforum/index.php?msg=222358 |
| Input Z | 470k -> 470k | DOCUMENTED | https://support.roland.com/hc/en-us/articles/201927549-CE-2-Technical-Specifications |
| Rec. load | >=10k -> same | DOCUMENTED | Roland CE-2 specs |
| Current | ~9 mA -> 9 mA | DOCUMENTED | Roland CE-2 specs |
| Max input | (none) -> 0 dBm@100 Hz, -10 dBm@1 kHz | DOCUMENTED (added) | Roland CE-2 specs |
| Dimensions | 73x129x59 mm -> 70x125x55 mm, 400 g | DOCUMENTED | Roland CE-2 specs |
| UI scale | 4 px/mm -> 1.6 px/mm (inconsistent with header) | fixed | reconciled header |
| Production years | 1979-1989 -> 1979-1992 | DOCUMENTED (S) | Roland CE-2 specs |
| Variants black-label/silver-screw/Taiwan MN3207 [D]/[E] | era claims -> unverified; enum kept, differences ESTIMATED | ESTIMATED; Taiwan/MN3207 era claim OMIT | none found |
| S/N >=90 dB [D] | -> removed claim; real -75..-80 dB | ESTIMATED | derivation: BBD chorus typical |
| Output Z ~1k | kept | ESTIMATED (1k figure is CE-2W) | derivation: op-amp buffer |
| Pre/de-emphasis 410 Hz/2.34 kHz; AA RC '14.6 Hz' | not modelled | OMIT (unverified/implausible in search summary) | ElectroSmash summary |
| Pre/post LPF 6.5/5.5 kHz | kept | ESTIMATED | derivation: MN3007 wet band-limit typical |
| Mix 1:1, wet gain, HPF 15 Hz, clip Vc | kept | ESTIMATED | topology |
| Bypass crossfade 5 ms | kept | ESTIMATED | design |
| Hardware MIDI/CC chart | none exists | OMIT (plugin does nothing; virtual extensions are design choices, off) | Roland specs |
| Sync/tap/AT/CC14/15/85/16 | tagged design, not hardware | OMIT as hardware claims | - |
| boss_ce2_bypass vs boss_ce2_active duplicate | -> single boss_ce2_bypass (1 = on) | fixed | reconciled header |
| boss_ce2_momentary row | removed | fixed | reconciled header |
| Tests (impulse, notch, sidebands, LFO periods) | recomputed from new values | derived | - |

Counts: upgraded 12 (delay centre, sweep, clock range, LFO max, LFO smoothing, input Z, load, current, dimensions, years, max input, delay formula), kept-estimate 8, omitted 4.
