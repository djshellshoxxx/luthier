# boss-rc1 — BOSS RC-1 Loop Station (dev spec)

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `boss-rc1`, **paramPrefix `boss_rc1_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/boss_rc1.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/boss_rc1_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_boss_rc1Tests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): (none).
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims (none) -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `boss_rc1_lofi`.  Momentary/Trigger (gesture, not lane-mapped): `boss_rc1_pedal`, `boss_rc1_ext_fs`.  PF_Ext (virtual extension, state + MIDI-learn only): `boss_rc1_sync_start`.
> - Host lanes `ref_s{nn}_p01..` in this order: `boss_rc1_level`, `boss_rc1_max_min`, `boss_rc1_keep_loop`, `boss_rc1_hold_ms`, `boss_rc1_dtap_ms`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Tags: MEASURED = bench-verified; DOCUMENTED = vendor/retailer published; ESTIMATED = inferred/from memory of manual, VERIFY vs official manual before ship.
Research note (2nd pass 2026-10-21): all page fetches DNS-blocked (boss.info, static.roland.com, manuals.plus, manualslib, manualzz, bonedo, archive.org); only WebSearch result text obtained, which quotes the official Owner's Manual (https://static.roland.com/assets/media/pdf/RC-1_eng03_W.pdf and RC-1_M_eng01_W.pdf, search-snippet access only, not read in full) plus retailer spec sheets. Items upgraded from that are tagged DOCUMENTED [M]. Remaining ESTIMATED items have no published value. See section 11. Legal: behaviour/controls only; "inspired by BOSS RC-1"; original vector art; no Boss logos/photos.

Sources:
- S1 https://www.boss.info/global/products/rc-1/ (product page; specs per snippets)
- S2 https://www.klundertmusic.nl/en/boss-rc-1-398 (retailer spec table: I/O, controls, levels)
- S3 https://www.musik-produktiv.com/products/boss-rc-1/ (16 bit/44.1 kHz resolution, 440 g)
- S4 https://mmrmagazine.com/boss-rc-1-loop-station/ and https://www.woodbrass.com/en-gb/loopers-boss-rc-1-p186049.html (24-LED ring, single-pedal workflow, double-tap stop)
- S5 https://www.effectsdatabase.com/model/boss/compact/rc1
- M https://static.roland.com/assets/media/pdf/RC-1_eng03_W.pdf (official Owner's Manual; also RC-1_M_eng01_W.pdf, newer edition) - reached via search-result text only
- B https://www.bonedo.de/?p=531690 (German review: undo/redo/clear gestures, FS-6 pairing; search-text only)

## 1 Identity/variants
- id `boss_rc1`; category looper; 2013 Boss compact; successor RC-3/RC-5 (separate specs). One phrase, no stored banks.
- Variants: none. Plugin variant flag `rc1_ext_fs` (external FS-5U/FS-6/FS-7 on STOP/UNDO jack) = UI jack option, not a model.
- Physical [DOCUMENTED S3]: 73 W x 129 D x 59 H mm, 440 g. Power 9V battery (6LR61) or PSA adaptor, 95 mA, ~3 h alkaline.
- Max record ~12 min stereo, min ~0.25 s (pressing earlier keeps recording until ~0.25 s) [DOCUMENTED M]; at max, rec/overdub is interrupted and unit goes to loop playback [DOCUMENTED M]. Conversion 16-bit/44.1 kHz [DOCUMENTED S3 only; low confidence].

## 2 Controls
| Control | Range/states | Taper | Default | Behaviour |
|---|---|---|---|---|
| LEVEL knob | 0..max (loop playback volume only) | audio, ~log; ESTIMATED 0.5 pos = unity? NO: assume pos 0.75 (~3 o'clock-ish) = 0 dB, max +6 dB headroom [E] | 0.75 | Scales loop playback (incl. overdub layer on playback). Dry input passes through at unity, unaffected [ESTIMATED]. LEVEL adjusts loop playback volume only [DOCUMENTED M]. |
| Pedal switch (single footswitch, large) | momentary | n/a | - | State machine below. |
| STOP/UNDO jack (TRS 1/4") | ext. footswitch | n/a | none | Ext FS single press = stop [DOCUMENTED M]; with a dual FS-6, left = stop, hold 2 s = clear; right = undo/redo [DOCUMENTED B]. Polarity auto-detect: ESTIMATED (not in sources). |
| INPUT A (mono) / INPUT B | 1/4" TS | - | - | Mono source on A only = dual-mono. A+B = stereo. |
| OUTPUT A (mono) / OUTPUT B | 1/4" TS | - | - | A only = mono sum? [E] Plug in A only -> L+R mixed to A; both plugged -> stereo. |
| DC IN | 9V | - | - | n/a plugin |
| Loop indicator | 24-segment multicolor LED ring | - | off | Ring shows position of loop head (rotating dot/fill, 24 steps per loop length). Colors [DOCUMENTED M]: red = recording, red+green (renders orange/amber) = overdubbing, green = playback; red rapid blink = erasing, green rapid blink = undo/redo. Off when stopped/empty [ESTIMATED]. Ring is a 24-segment rotating position display [DOCUMENTED S4]. LED colour mix as orange for overdub is ESTIMATED rendering of 'red and green'. |
| Rec-mode / other | none: RC-1 has no menus, no rhythm, no quantize, no reverse/half-speed/FX (those are RC-3/RC-5/RC-30). | | | |

## Gesture state machine (pedal switch) [hold 2 s and double-press stop DOCUMENTED M; see notes]
States: EMPTY, REC, PLAY, OVERDUB, STOPPED (loop retained), CLEARED=EMPTY.
- EMPTY --tap--> REC (ring red; writes buffer from t=0; length unbounded to 12 min; hitting limit auto -> PLAY).
- REC --tap--> PLAY (sets loop length = elapsed samples; no quantise; seamless wrap).
- PLAY --tap--> OVERDUB (ring orange; input summed into loop at head).
- OVERDUB --tap--> PLAY.
- Cycle order REC -> PLAY -> OVERDUB -> PLAY ... [DOCUMENTED S4 cycle order].
- PLAY or OVERDUB --double-press (2 presses within 1 s [DOCUMENTED M])--> STOPPED (loop kept; ring off). [DOCUMENTED S4: double-click to stop]. Note first tap triggers mode toggle (PLAY<->OVERDUB), second tap stops; on stop discard last toggle effect? Real: stop occurs and overdub state closed. Emulate: first tap applies normally, second tap within window cancels toggle and stops.
- STOPPED --tap--> PLAY from start (position 0).
- PLAY/OVERDUB --hold >= 2 s [DOCUMENTED M]--> UNDO (cancels last layer; green LED rapid blink); hold >= 2 s again in PLAY = REDO (restores) [DOCUMENTED M]. Undo depth = exactly one layer: ESTIMATED (manual says 'last layer'; user reports agree, depth not stated).
- STOPPED --hold >= 2 s [DOCUMENTED M]--> CLEAR (red LED rapid blink, return EMPTY). Do not cut power during blink/rotate or data may be lost [DOCUMENTED M].
- 'Stop-enabled' mode (double-press stops AND erases phrase; hold 2 s cancels): editions of the manual differ (eng03 vs M_eng01) and entry procedure not retrieved -> OMIT (plugin has no such mode).
- REC with immediate double-tap -> stop & discard? [E]; plugin: ignore (treat as REC->PLAY then stop).
- Phrase is SAVED across power-off on hardware [DOCUMENTED M: 'The recorded phrase is saved even if you turn off the power'] (earlier erase assumption was wrong; models/boss-rc1.md note was right). Plugin `boss_rc1_keep_loop` default 1 = authentic.

## 3 Signal path & DSP model
- Analog input buffer (1 MΩ in [DOCUMENTED S2], nominal -20 dBu in/out) -> A/D 16-bit/44.1 kHz [S3] -> DSP -> D/A -> output (1 kΩ out, load >= 10 kΩ [S2]).
- Dry path: always passes through (buffered, not true bypass); loop sum = dry + LEVEL * loop at output mixer. Overdub writes input (not loop) summed with existing loop: new = old + in (no feedback attenuation; real unit no decay [E]).
- Latency: unmeasured; ESTIMATED 1-2 ms conversion; plugin dry path 0 added latency, loop alignment compensated.
- No bypass-transition: dry never muted. Loop start/stop: short fade [E] ~ 2-5 ms click suppress at wrap and stop.
- Converter modelling: optional `rc1_lofi` (default on): quantise to 16 bit TPDF dither; band-limit 20 Hz..~18 kHz (ESTIMATED anti-alias 4th-order at ~19 kHz, hpf 20 Hz) — ESTIMATED, no measurements. Rate stays host rate; SRC from 44.1 only when `rc1_lofi` resample on (default off).

## 4 Plugin algorithm
Block diagram: in(L,R) -> [mono-detect A-only] -> dryOut ; in -> rec gate -> LoopBuf(stereo float, preallocated 12 min x sr x 2 x 4 B = 12*60*48000*8 = 276 MB at 48k; allocate lazily per max `rc1_max_min` default 12 -> make UI warn; option 5 min default 5? NO: keep 12 spec, allocate on prepare off-thread, 96k halves minutes? cap by bytes: 128 MB default => ~5.5 min stereo float32 @48k — decision DEV: store int16 (matches HW) => 12 min@48k = 138 MB; use int16 + 1 undo layer copy = 276 MB worst).
- Storage: `std::vector<int16_t>` preallocated in prepareToPlay (message thread). Undo layer = second buffer of overdub delta, same size, allocated on first overdub off audio thread (pre-alloc at prepare).
- Per-sample:
  - REC: buf[w++] = q16(in); w==cap -> PLAY, len=cap.
  - PLAY: out = dry + g*buf[r]; r=(r+1)%len.
  - OVERDUB: delta[r]=in16; buf[r]=clip16(buf[r]+in16); out = dry + g*buf[r].
  - g = LEVEL taper(level) smoothed 20 ms one-pole.
  - wrap fade: last/first 64 samples crossfade only for REC->PLAY boundary (len closure), 2 ms ramp on stop/start.
- Undo: buf -= delta (saturating); redo: += . Single layer.
- Oversampling: none (linear mix). Aliasing: none; lofi quantiser uses dither.
- Denormals: ScopedNoDenormals + int16 storage; filter states flushed.
- CPU: <0.3% core @48k/128 (copy+add); no alloc/locks audio thread; gestures via lock-free SPSC queue from UI (enum events w/ timestamp in samples) — gesture timing (double-tap, hold) decoded on audio thread by sample counters, not UI timers.
- Tempo/quantize: none on RC-1.

## 5 I/O & routing
- Mono: input A only -> dual-mono internal; output: mono pair duplicated. Stereo: A=L,B=R both ways.
- Plugin host: sidechain/aux not needed; bus layout stereo in/out. Mono host bus: treat as A only.
- Tricks to expose (board-level, not in pedal): effects-loop feedback — place RC-1 before/after other pedals; put effects *after* RC-1 for wet loop processing, before RC-1 to bake effects into loop (board serial order is authoritative). Parallel: board A/B split mode -> RC-1 on branch. Stereo split: RC-1 inputs A/B fed from two pedal outputs (needs board stereo split node, ref ARCHITECTURE.md). Feedback: Output B into Input B via board send/return = infinite-regen (clamp: soft limit + 1 sample delay; mute on |x|>4).
- Ext FS jack: board `ext_stop_undo` binding (see §6).

## 6 MIDI/sync/expression
Real RC-1: NO MIDI, NO expression jack, no tempo sync [DOCUMENTED S2: only audio + STOP/UNDO + DC jacks]. Plugin emulation (comparable function, clearly labelled extension "not on hardware"):
- Footswitch gesture: any MIDI note-on (assignable, default note 60 / learn) or CC#64-style sustain (CC 64 >=64 = down, <64 = up) drives the pedal switch with real press timing (so hold and double-tap work).
- Ext STOP/UNDO FS: CC 65 (>=64 down) -> tap=stop, hold=undo/redo; or note 62.
- Velocity/aftertouch/CC7->LEVEL maps: OMIT (no hardware basis; plugin does nothing). Switch is binary, velocity ignored. Slider/knob MIDI learn via board MIDI map (MIDI_EXPRESSION.md); params all automatable.
- DAW sync: none native. Extension (default off): `rc1_sync_start` quantises REC start/stop and tap to host transport bar if host playing (documented as extension). Not part of RC-1 parity.
- Program change: none (ignore).
- Host transport stop must not clear loop.

## 7 Visuals (original vector art)
- Enclosure: compact stomp, 73x129 mm, matte black/dark grey powder coat body is typical; use slightly textured dark grey; face = top plate. [ESTIMATED colours; no photo copying]
- Top layout (mm, origin top-left of 73x129 face, y downwards): LEVEL knob centre (36.5, 22) dia 17, white pointer, scale ticks min..max over 270°; 24-LED ring centre (36.5, 52) outer dia 30, each LED 3x3 mm radial dots; large pedal switch plate (36.5, 95) 60x44 rounded rect; footswitch label "STOP/UNDO" not on face (jack on top edge). Text labels: "LEVEL", "RC-1" generic styled "LOOP STATION" by descriptive wording only ("Loop Station inspired"). No Boss logo; use "REF-LOOP" wordmark.
- Side jacks: left edge INPUT A, INPUT B; right edge OUTPUT A, OUTPUT B; top edge DC IN (left), STOP/UNDO (right); dims 9 mm dia TS jack bodies at y=30 and 48.
- LED ring: red/orange/green fill, 24 segment, glow radius 1.5 mm, update 30 fps from audio-thread atomic position.
- Foot press: plate dips 1.5 mm, 60 ms ease.
- Drawer integration: pedal width 73 mm base units; face keyed `boss-rc1` in ReferencePedalDrawer.

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | autom |
|---|---|---|---|---|---|---|---|---|
| boss_rc1_level | Level | 0 | 1 | 0.75 | norm | audio: gain=0 at 0, 1.0 at 0.75, 2.0 (+6 dB) at 1; below 0.75 ^2 taper | 20 | yes |
| boss_rc1_pedal | Pedal switch | 0 | 1 | 0 | bool momentary | n/a | 0 | yes (gesture) |
| boss_rc1_ext_fs | Ext STOP/UNDO | 0 | 1 | 0 | bool momentary | n/a | 0 | yes |
| boss_rc1_lofi | 16-bit lofi | 0 | 1 | 1 | bool | n/a | 0 | yes |
| boss_rc1_max_min | Max loop minutes | 1 | 12 | 12 | min | linear | n/a | no |
| boss_rc1_sync_start | Host-bar quantise (ext) | 0 | 1 | 0 | bool | n/a | 0 | yes |
| boss_rc1_keep_loop | Keep loop on recall (authentic: HW retains phrase) | 0 | 1 | 1 | bool | n/a | 0 | no |
| boss_rc1_hold_ms | Hold time | 800 | 3000 | 2000 | ms | linear | n/a | no (tunable); 2000 DOCUMENTED M |
| boss_rc1_dtap_ms | Double-press window | 150 | 1000 | 1000 | ms | linear | n/a | no (tunable); 1000 DOCUMENTED M |
Non-param runtime state (not automated): mode enum {EMPTY,REC,PLAY,OVERDUB,STOPPED}, loop length, position.

## 9 Tests/acceptance
1. Record 10 s sine, tap -> PLAY: loop length == recorded samples exactly (+/-0), wrap sample-continuous (|step| < 1 LSB16 excl. input discontinuity).
2. Cycle REC->PLAY->OVERDUB->PLAY x5: layer sum equals sum of inputs within 16-bit clip tolerance.
3. Undo removes exactly last overdub (null-test residual < -90 dBFS); redo restores bit-exact.
4. Double-tap in PLAY: STOPPED within 1 block; no residual click (> -60 dBFS step).
5. STOPPED hold >= hold_ms: buffer zeroed, mode EMPTY.
6. Max-time: auto PLAY at cap, no overrun, no alloc (verify via allocation hook).
7. Dry path: gain 0 dB +/-0.01, THD+N < -90 dB with lofi off; with lofi on noise floor ~ -96 dBFS +/-3 (16-bit).
8. LEVEL sweep: 0, 0.25, 0.5, 0.75, 1 -> gain 0, -12, -6?? (^2 taper): expected 0.0, 0.11, 0.44, 1.0, 2.0 linear gain +/-0.5 dB. (taper ESTIMATED)
9. Block sizes 1/4/16/64/512; sample rates 44.1/48/96: same loop length in seconds +/-1 sample.
10. 30-min random gesture soak: no crash, no NaN, no >-1 dBFS spike beyond clip.
11. Mono-in (A only) yields identical L/R.
12. Thread-safety: TSan clean; gesture queue lossless at 1000 events/s.
Frequency response checkpoints (lofi on) [ESTIMATED]: 20 Hz -3 dB, 1 kHz 0, 15 kHz -1 dB, 18 kHz -6 dB. No hardware reference captured.

## 10 Known inaccuracies
- Resolved 2nd pass: hold 2 s, double-press 1 s, LED colours, power-off retention, 12 min / 0.25 s limits. Still ESTIMATED: LEVEL taper/gain range, undo depth, wrap/stop fades, latency.
- 16-bit/44.1 kHz from single retailer (S3); one review calls RC-1 16-bit vs RC-5 32-bit (corroborates 16-bit, not rate); converter filter/noise unmeasured, no spec sheet value for freq response/dynamic range (checkpoints in sec 9 ESTIMATED).
- Latency, dither, clipping behaviour on overdub saturation not measured.
- MIDI/expression/sync are plugin extensions, no hardware analogue.
- Mono-out summing behaviour with A only unverified.
- Bypass: dry always buffered; pedal "off" in plugin = loop mode STOPPED, dry through.

## 11 Verification log
Sources reached (search text only; all direct fetches DNS-blocked): Roland/BOSS Owner's Manual PDFs (M), manuals.plus / manualslib mirrors (via search quotes), reverb/alamo/sweetwater spec listings, bonedo.de review (B), kirstein.de reviews.
| Item | Old -> New | Status | Source |
|---|---|---|---|
| Max loop time | 12 min stereo -> same + min 0.25 s, interrupt-to-playback at cap | DOCUMENTED | M |
| Input/output impedance, levels | 1 MOhm/1 kOhm/-20 dBu/>=10 kOhm -> confirmed | DOCUMENTED | spec listings (Reverb/Alamo), S2 |
| Power 9V, 95 mA, ~3 h | confirmed | DOCUMENTED | manuals.plus/manualslib spec text |
| Double-tap stop window | ~400 ms [E] -> 1 s (param default 1000, range 150-1000) | DOCUMENTED | M |
| Hold for undo/redo/clear | ~2 s [E] -> 2 s | DOCUMENTED | M, B |
| Undo scope | PLAY/OVERDUB undo; redo in PLAY | DOCUMENTED | M |
| Undo depth = 1 layer | [E] kept | ESTIMATED (manual says last layer) | M, user reports |
| LED colours | red/green/orange [E] -> red rec, green play, red+green overdub; red rapid blink = erase; green rapid blink = undo/redo | DOCUMENTED (orange rendering ESTIMATED) | M |
| Power-off | erases -> phrase retained; keep_loop default 0 -> 1 | DOCUMENTED | M |
| LEVEL = loop playback volume only | confirmed | DOCUMENTED | M |
| LEVEL taper / +6 dB headroom / 0.75 default | kept | ESTIMATED (no published taper; derive: typical Boss 1-knob volume, unity near 3 o'clock) | - |
| 16-bit / 44.1 kHz | kept | ESTIMATED-leaning (single retailer + 16-bit review corroboration) | S3 |
| Ext FS: stop single press; FS-6 L=stop/clear, R=undo/redo | [E] -> documented | DOCUMENTED | M, B |
| Ext FS polarity auto | kept | ESTIMATED | - |
| Stop-enabled mode | absent -> noted, editions conflict | OMIT | M variants |
| MIDI vel/aftertouch/CC7 -> level | extension -> removed | OMIT | no hardware MIDI |
| MIDI note/CC64/CC65 footswitch triggers, host-bar sync | kept, labelled plugin extension | ESTIMATED (design choice, not hardware) | - |
| Latency 1-2 ms, fades 2-5 ms, freq response checkpoints, mono-out summing | kept | ESTIMATED (no source) | - |
| Dimensions 73x129x59, 440 g | kept | DOCUMENTED | S3 |
