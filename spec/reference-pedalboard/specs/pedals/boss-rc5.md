# boss-rc5 — "Loop Station RC-5 inspired" stereo looper + rhythm (dev spec)

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `boss-rc5`, **paramPrefix `boss_rc5_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/boss_rc5.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/boss_rc5_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_boss_rc5Tests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): (none).
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims (none) -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `boss_rc5_lofi`, `boss_rc5_ram_cap_mb`.  Momentary/Trigger (gesture, not lane-mapped): `boss_rc5_pedal1`, `boss_rc5_pedal2`, `boss_rc5_pedal3`.  PF_Ext (virtual extension, state + MIDI-learn only): `boss_rc5_sync_clock`, `boss_rc5_quantize`, `boss_rc5_exp_target`, `boss_rc5_exp`.
> - Host lanes `ref_s{nn}_p01..` in this order: `boss_rc5_loop_level`, `boss_rc5_rhythm_level`, `boss_rc5_rhythm_on`, `boss_rc5_tempo`, `boss_rc5_pattern`, `boss_rc5_variation`, `boss_rc5_kit`, `boss_rc5_memory`, `boss_rc5_reverse`, `boss_rc5_hold_ms`, `boss_rc5_dtap_ms`, `boss_rc5_dry_mute`, `boss_rc5_half_speed`.
> - Implementation erratum (2026-10-30, prototype): append `boss_rc5_tap` as a non-lane Trigger for the existing RHYTHM TEMPO button. It does not alter the three pedal gestures or the fixed lane order. Tap/host automation echo and per-memory persistence remain open acceptance work; this event alone does not qualify the model.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Tags: DOCUMENTED = vendor/retailer/manual snippet seen; REPORTED = forum/user; ESTIMATED = inferred, VERIFY vs official manual before claiming fidelity; EXT = plugin extension, no hardware analogue.
Research limits (pass 2, 2026-10-21): boss.info, static.roland.com, files.kraftmusic.com, manualslib, support.roland.com, help.morningstar.io all unreachable (WebFetch ENOTFOUND; curl no connection through proxy; archive.org also unreachable). Only WebSearch snippets of those pages obtained; values below tagged DOCUMENTED come from such snippets of the official manual/Boss page. See section 11. Every ESTIMATED item MUST be re-checked against official RC-5 Reference Manual (https://static.roland.com/assets/media/pdf/RC-5_reference_eng01_W.pdf) and Parameter/MIDI docs before ship. No schematic/ElectroSmash exists for RC-5 (digital; DSP not public) -> no circuit model; behavioural model only.
Legal: behaviour/controls only; name = "inspired by BOSS RC-5" descriptive; no Boss logo/photo/manual text; original vector art; wordmark "REF-LOOP 5".

Sources:
- S1 https://www.boss.info/global/products/rc-5/ (product page; not fetched)
- S2 https://static.roland.com/assets/media/pdf/RC-5_reference_eng01_W.pdf (Reference Manual; via search snippets only)
- S3 https://www.manualslib.com/manual/2045241/Boss-Loop-Station-Rc-5.html (p13/14 snippets: undo/redo/clear holds, pedal function list, reverse, tempo 40.0-300.0, LOOP LEVEL 0-200, SYNC CLOCK AUTO/INTERNAL)
- S4 retailer specs: fullcompass.com/prod/595289-boss-rc-5-advanced-compact-loop-station ; buzzmusic.co ; alamomusic.com (99 memories, 1.5 h/track, 13 h total, 44.1 kHz, 32-bit AD/DA, 57 patterns x2 variations, 7 kits, mini TRS MIDI, BMIDI-5-35, USB WAV import/backup, 9V/PSA)
- S5 https://www.guitarworld.com/reviews/boss-rc-5-loop-station-review ; https://looperpedalreviews.com/boss-rc-5-loop-station-review/ (same size as RC-3; 32-bit vs RC-3 16-bit; screen; extra pedals via ext FS)
- S6 MIDI forum reports: https://forum.morningstar.io/t/boss-rc-5-does-not-receive-midi-in/548 ; https://morningstarengineering.atlassian.net/wiki/spaces/MMS/pages/831782913/Boss ; https://unity.neuraldsp.com/t/midi-commands-to-control-boss-rc-5/14255 (CC#80-87 user-assignable RX map; send 127 then 0 for momentary; PC switches memory; BMIDI-5-35 into TRS MIDI IN)
- S8 https://www.boss.info/us/products/rc-5/ (spec snippet via search: 56 mm H, 450 g, accessories FS-5U/FS-6/FS-7/FV-500H/FV-500L/EV-30/EV-5) ; retailer mirrors kraftmusic.com, wollongongmusic.com.au (73 x 129 x 56 mm)
- S9 manual MIDI/Control snippets (static.roland.com RC-5_reference_eng01_W.pdf, manualslib p18 via search): SETUP > CONTROL > CC#80..87 FUNC; RX CTL CH 1-16; OMNI; TX CH incl. 'RX CTL'; PEDAL FUNC for pedal + CTL1/CTL2; PC 1-99 = memory, 100-128 ignored; Bank Select ignored
- S7 existing repo notes: spec/reference-pedalboard/models/boss-rc5.md, specs/pedals/boss-rc1.md (shares gesture grammar)
Conflict flagged (RESOLVED, see 11): one review (musicgearspecialist) claims "16 rhythms" and "no built-in FX"; manual/retailers say 57x2 and 7 kits. Use 57x2/7 [DOCUMENTED S4]. RC-5 has NO half-speed/reverse-pitch FX in snippet evidence; REVERSE is per-track setting [DOCUMENTED S3]; half-speed ABSENT from sources -> ship as EXT `boss_rc5_half_speed` default off, labelled "not on hardware".

## 1 Identity/variants
- id `boss_rc5`; category looper+rhythm; compact stomp (same footprint as RC-3 minus few mm height [S5]); successor family RC-1/RC-3/RC-5/RC-30/RC-500/505.
- Exactly one stereo track [DOCUMENTED]; 99 memories (phrases), each storing loop audio + rhythm settings (pattern, kit, tempo, level) [S4].
- Record limits [DOCUMENTED S4]: ~1.5 h per track, ~13 h total across all memories. Converters 32-bit AD/DA [S4], fs 44.1 kHz [S4].
- Power: 9V battery or PSA adaptor; plugin ignores. Dimensions 73 W x 129 D x 56 H mm, 450 g with battery [DOCUMENTED S8].
- Variants: none. No plugin option flags (the former `ext_fs_count` is dropped; CTL1/CTL2 are always shown in the UI).

## 2 Controls
| Control | Range/states | Taper | Default | Real behaviour |
|---|---|---|---|---|
| MEMORY/LOOP LEVEL (push-encoder knob) | endless rotary + push | detented, step | - | Context-sensitive: normal screen = select memory 1..99 (rotate), push = confirm/enter; Loop Level screen = track level 0..200 (100 = unity, +6 dB max ESTIMATED) [DOCUMENTED S3 range 0-200]; tempo screen = tempo; SETUP menu = select item, push to confirm [DOCUMENTED]. |
| SETUP button | momentary | - | - | Opens global settings menu (see 2.3). |
| MEMORY button | momentary | - | - | Opens memory menu (per-memory params: reverse, play mode, rhythm assign, name/clear/copy ESTIMATED). |
| RHYTHM ON/OFF button | toggle | - | OFF | Starts/stops rhythm; rhythm independent of loop level [S7]. Backlit when on. |
| RHYTHM TEMPO button | tap | - | - | Tap repeatedly = tap tempo [DOCUMENTED S3]; press = tempo screen; turn encoder to set. |
| Main pedal (large) | momentary | - | - | REC/PLAY/OVERDUB cycle; double-press stop; hold undo/redo/clear (see 2.2) [DOCUMENTED S3]. |
| CTL1 / CTL2 (external footswitches on CTL jack, FS-5U/FS-6/FS-7) | momentary | - | - | RESOLVED: hardware has ONE built-in stomp pedal; CTL1/CTL2 are optional external switches; PEDAL FUNC assigns functions to pedal, CTL1, CTL2 [DOCUMENTED S8,S9; reviews S5 'one footswitch']. Function list (TRK PLY/STP, TRK CLEAR, TRK UND/RED, TRK REVERSE ...) [DOCUMENTED S3]. Plugin: `boss_rc5_pedal1`=built-in pedal, `pedal2`=CTL1 (default STOP assign ESTIMATED), `pedal3`=CTL2 (default UNDO/REDO assign ESTIMATED). Defaults of CTL1/CTL2 unknown (OMIT as a claim of factory default; plugin default is a plugin choice). |
| Display | backlit display, resolution not published (ESTIMATED; plugin draws segment/bar) | - | - | Shows memory #, time/progress bar, tempo, state [S5]. |
| 4 backlit buttons | - | - | - | [DOCUMENTED Thomann: encoder + four backlit buttons + backlit display]. |
| Jacks | see 5 | | | |

### 2.1 Pedal states
EMPTY, REC, PLAY, OVERDUB, STOP(loaded), plus UNDONE flag. Cycle: EMPTY -tap-> REC -tap-> PLAY -tap-> OVERDUB -tap-> PLAY [DOCUMENTED S3 "switches between recording, playback, overdubbing"].

### 2.2 Gesture grammar (main pedal) [DOCUMENTED S3 unless noted]
- Tap: advance state per cycle. Press timing = pedal-down edge (RC-5 triggers on press; ESTIMATED).
- Double press (2nd press <= `dtap_ms` default 400 ESTIMATED) in PLAY/OVERDUB: STOP [DOCUMENTED "pressing twice stops playback"]. First press toggle is cancelled/closed; loop stops at second press.
- Hold >= 2.0 s during PLAY/OVERDUB: UNDO (cancel recording or last overdub); hold again >= 2.0 s: REDO [DOCUMENTED]. Undo depth: single level (last pass) ESTIMATED.
- Hold >= 2.0 s while STOP: CLEAR phrase [DOCUMENTED].
- STOP --tap--> PLAY from loop start.
- REC with Auto Loop Length/rhythm: if rhythm ON and quantize ON, REC end snaps to bar/beat (see 4.5) ESTIMATED.
- Memory switch while playing: Loop Sync behaviour (setup) — switch at loop end vs immediate ESTIMATED.

### 2.3 SETUP menu (global) [names DOCUMENTED only where tagged; all others ESTIMATED]
| item | values | default |
|---|---|---|
| SYNC CLOCK | AUTO / INTERNAL | AUTO [DOCUMENTED S2/S3] |
| CONTROL > RX CTL CH | 1..16 [DOCUMENTED S9] | 1 ESTIMATED (default not found) |
| CONTROL > OMNI | on/off; on = all channels, off = RX CTL CH only [DOCUMENTED S9] | ESTIMATED on |
| TX CH | 1..16 or 'RX CTL' (follows RX CTL CH) [DOCUMENTED S9] | RX CTL ESTIMATED |
| MIDI TX START/STOP,CLOCK | on/off | off ESTIMATED |
| CONTROL > CC#80..CC#87 FUNC | each = function from list (REVERSE, ONE SHOT, REC ACTION, DUB MODE, AUTO REC, START, STOP, FADE TIME, REVERB, PATTERN ... partial) [DOCUMENTED S9] | factory map NOT FOUND -> OMIT (plugin ships all eight unassigned) |
| PEDAL FUNC (pedal, CTL1, CTL2) | list in 2.2 [DOCUMENTED S9] | pedal = TRK REC/PLY [DOCUMENTED S9]; CTL1/CTL2 ESTIMATED stop / undo |
| EXP PEDAL TARGET | LOOP LEVEL / RHYTHM LEVEL / TEMPO ESTIMATED | LOOP LEVEL |
| CTL FUNC | same list | TRK PLY/STP |
| INPUT/OUTPUT LEVEL, MONO/STEREO | | stereo |
| LCD brightness, auto-off | | n/a plugin |
| STOP MODE (immediate / finish loop) | | immediate ESTIMATED |
| LOOP QUANTIZE | on/off | off ESTIMATED |
| REC ACTION (REC->PLAY / REC->OVERDUB) | | REC->PLAY ESTIMATED |

### 2.4 MEMORY params (per memory) [REVERSE DOCUMENTED S3; rest ESTIMATED]
REVERSE off/on; PLAY MODE loop/one-shot; LOOP LEVEL 0..200 (def 100); RHYTHM pattern 1..57, variation A/B, KIT 1..7, rhythm level 0..100, tempo 40.0..300.0 [DOCUMENTED S3], count-in/pre-count; name. Memory-switch via MIDI PC 1-99 only [DOCUMENTED S9].

## 3 Signal path & DSP model
- In (Lr) -> input buffer/ADC (32-bit, 44.1 kHz [S4]) -> record bus. Dry path: ADC -> DAC buffered (digital through, NOT true bypass) ESTIMATED; latency ~1-3 ms RT ESTIMATED, unmeasured.
- Output = dry + g_loop * loop + g_rhy * rhythm. LOOP LEVEL scales loop playback only, not dry [S7 DOCUMENTED]; rhythm independent.
- No analog stage modelled. No clipping topology; internal float, overdub sum new = old + in, hard-limit at +/-4.0 float (EXT soft limit), no decay (ESTIMATED).
- Reverse: read pointer decrements; record-time reverse not supported (playback only) [DOCUMENTED setting is playback].
- No filters/delays/mod in RC-5 beyond converters; converter band-limit ESTIMATED 20 Hz HPF (1st order DC block) + AA ~20 kHz at 44.1 (pure host SR bypass option `lofi=0`).
- Loop boundaries: crossfade 2 ms (96 samples @48k) at REC->PLAY close, at wrap (none if seamless) and at stop/start/undo ESTIMATED.
- Bypass transition: n/a. Dry never muted; "off" = STOP.

## 4 Algorithm for plugin
### 4.1 Block diagram
```
host in -> inTrim -> [mono detect] -> dryBus -> out sum
                    -> recTap -> LoopEngine(track buf, undo layer) -> loopGain -> out sum
RhythmEngine(clock) -> kit voice mixer -> rhyGain -> out sum
ClockMaster (internal bpm | host/midi clock) -> quantizer, rhythm, loop sync
GestureDecoder(sample-accurate) <- lock-free SPSC events (UI, MIDI, pedal jacks)
FileWorker thread: disk spill, WAV import/export, memory load/save
```
### 4.2 Storage (real-time safe; supports 1.5 h/track, 13 h total)
- RAM ring of active memory only: pre-reserve RAM bounded by `boss_rc5_ram_cap_mb` (default 256 MB = ~11 min stereo float32 @48k; there is no separate minutes param). Longer than cap: spill pages (64 K-frame blocks) to disk via FileWorker; audio thread reads only pre-fetched pages (double-buffer, lookahead >= 2 s); on miss: output silence for loop + flag UNDERRUN, never block. Record past RAM: write pages via lock-free ring to worker; fail disk -> auto PLAY, set error LED, never crash.
- 99 memories = files `<appdata>/luthier/rc5/mem_NN.wav` (+ .json metadata). Total cap 13 h ~ 13*3600*48000*8 B = 18 GB float32 — store 24-bit/16-bit-packed on disk (EXT option) default float32 w/ warning; enforce `total_cap_h` 13 and `track_cap_h` 1.5 [DOCUMENTED]. Undo layer: second file/page set (delta), same size worst case; pre-allocate lazily off-thread when REC starts.
- Sample rate change: resample stored loops on load (worker), loop length in seconds preserved +/-1 sample.
### 4.3 Per-sample
- REC: buf[w++]=in; if w==cap -> PLAY(len=cap). PLAY: loopOut=buf[r]*g; r=(r+1)%len (REVERSE: r=(r-1+len)%len). OVERDUB: delta[r]=in; buf[r]+=in.
- g_loop = lvl/100 with taper g=(lvl/100)^1 up to 100, 100..200 maps linear to 1..2 (+6 dB) ESTIMATED; smooth 20 ms one-pole. g_rhy = rLevel/100 ^2, smooth 20 ms.
- Undo: buf -= delta; redo: buf += delta. Dry: unity.
### 4.4 Rhythm engine
- 57 patterns x A/B, 7 kits [DOCUMENTED]: original synthesized/sample-free kits (no Roland samples): kick/snare/hat/clap/rim/tom/crash via short synth voices (sine sweep kick 150->50 Hz 120 ms; noise+BPF snare 180 Hz+4 kHz; hat HPF 7 kHz 40/200 ms). Pattern data = 16-step grids per bar, tables in `rc5_patterns.inc`. Genres list ESTIMATED (rock, pop, funk, blues, shuffle, metronome, ...) — to author originally; count must be 57.
- Tempo 40.0..300.0 BPM, 0.1 step [DOCUMENTED S3]. Tap tempo: average of last 4 intervals; timeout 2 s; ignore intervals outside 200..1500 ms (plugin choice); bpm = 60000/mean.
- Time signature 4/4 default; patterns define own length (2-4 bars ESTIMATED).
### 4.5 Quantize / auto loop
- If LOOP QUANTIZE on and rhythm running: REC start waits for next beat (or count-in), REC stop rounds to nearest bar; tolerance: if within 1/16 note (plugin choice) snap, else next bar. Auto Loop Length (ESTIMATED): when rhythm on and REC started, set loop = N bars, auto PLAY at end.
- Loop-derived tempo (rhythm OFF at first REC) option: after REC stop, set tempo = 60*bars*beats/seconds nearest in [40,300] ESTIMATED EXT.
### 4.6 Safety
- ScopedNoDenormals; flush filter states; NaN guard on input (zero + count); no alloc/locks/IO on audio thread; UI->audio via fixed-size SPSC queue of `GestureEvent{type,sampleOffset,value}` (cap 1024). Gesture timing (dtap, hold) via sample counters on audio thread.
- CPU budget: loop copy+add + rhythm 8 voices < 0.8% core @48k/128; disk worker separate thread, priority below audio.

## 5 I/O & routing
- Inputs: INPUT L/MONO, INPUT R (1/4" TS) [ESTIMATED]; L only = dual-mono. Outputs OUTPUT L/MONO, R: mono plug -> L+R sum [ESTIMATED].
- Plugin extras: EXP jack and CTL jack (CTL1/CTL2 footswitches [DOCUMENTED S9]; compatible EXP pedals FV-500H/L, EV-30, EV-5 [DOCUMENTED S8]; TRS wiring ESTIMATED), MIDI IN/OUT mini TRS (BMIDI-5-35 adapter) [DOCUMENTED S4], USB (MIDI + WAV storage) [DOCUMENTED].
- Board tricks (board node owns routing, see ARCHITECTURE.md/BOARD_CORE.md): (a) effects-loop feedback: board send/return from RC-5 out to its own in through effects chain, soft limiter + one block delay, mute if |x|>4 for 3 blocks; (b) series: effects before RC-5 are recorded into loop, after RC-5 process wet loop and dry; (c) parallel: RC-5 on branch with dry mix; (d) stereo split: L/R from two chains into RC-5 inputs; (e) re-amp: loop out -> amp pedals while dry bypasses (output-only mode `boss_rc5_dry_mute` EXT).
- Rhythm output on both channels (center); (Dropped: `rhythm_sep` has no ParamDesc; not implemented.)

## 6 MIDI/sync/expression emulation
Hardware: MIDI IN/OUT via mini TRS, USB MIDI; receives MIDI clock and syncs when SYNC CLOCK=AUTO (falls back to internal) [DOCUMENTED S2]; CC#80-87 user-assignable RX map [DOCUMENTED S9]; PC 1-99 selects memory, 100-128 ignored, Bank Select (CC0/CC32) ignored [DOCUMENTED S9]; momentary needs CC value 127 then 0 (manual gives no value semantics) [REPORTED S6].
- Clock sync: host transport (plugin) and MIDI clock (24 ppqn) both feed ClockMaster. AUTO: if clock seen in last 500 ms follow; else internal. Tempo smoothing: PLL average of last 24 ticks; clamp 40..300; ignore jitter < 0.5 BPM. Start (0xFA)/Continue/Stop (0xFC) start/stop rhythm (and loop if setup `START_LINK` ESTIMATED). Loop playback is NOT time-stretched (hardware does not [ESTIMATED]); only rhythm follows clock.
- MIDI TX (optional): send clock 24 ppqn + start/stop when INTERNAL and enabled.
- CC map: factory default of CC80-87 NOT FOUND -> OMIT; plugin ships CC80..87 unassigned, each user-assignable from the documented function list (previous invented defaults CC80..87 and CC7/CC11 REMOVED; CC7/CC11 not documented for RC-5 -> OMIT, EXP is reachable via host lane/MIDI-learn only). Program change -> memory 1..99, PC 100..128 ignored, Bank Select ignored [DOCUMENTED S9]; PC wire numbering (0- vs 1-based) ESTIMATED: treat PC value 0..98 as memory 1..99.
- Press-timing fidelity: pedal down/up from CC 127/0 or note on/off (assign note, default 60) preserves hold and double-tap windows exactly like physical.
- Velocity/aftertouch mappings: OMIT (no hardware analogue, no ParamDesc; removed with `rc5_vel_to_overdub_gain`).
- EXP: PF_Ext lane `boss_rc5_exp` 0..1 -> selected target (LOOP LEVEL 0..200, RHYTHM LEVEL, TEMPO 40..300; target list ESTIMATED); no CC11/aftertouch fixed mapping; `exp_min/exp_max` dropped (OMIT). Board MIDI-learn slider/knob map per MIDI_EXPRESSION.md for all params.

## 7 Visuals (original vector art)
- Enclosure: body 73 x 129 mm footprint [DOCUMENTED S8]; colour red/black ESTIMATED; plugin face widened to 110 mm to show built-in pedal plus CTL1/CTL2 external-switch icons.
- Layout (mm, origin top-left of 110 x 129 face, y down): display 40x16 window centre (55,14), dark glass with segment/progress bar; MEMORY/LOOP LEVEL knob centre (55,34) dia 15 push-encoder, no ticks (endless), pointer dot; 4 buttons row y=52 at x=21,43,67,89 (each 14x8 mm, backlit; labels SETUP, MEMORY, RHYTHM, TEMPO) ; pedals: main centre (55,96) 58x34 rounded plate; CTL1 (22,112) 28x20; CTL2 (88,112) 28x20 (drawn as external footswitch icons).
- Edges: top: MIDI IN, MIDI OUT (3.5 mm), USB, EXP/CTL, DC; sides L: IN L/MONO, IN R; R: OUT L/MONO, OUT R; jack body dia 9 mm at y=30, 48.
- Labels (original text): "MEMORY / LOOP LEVEL", "SETUP", "MEMORY", "RHYTHM", "TEMPO", "REF-LOOP 5". LED colour: red REC, green PLAY, orange OVERDUB, off STOP; rhythm button amber backlight.
- Loop progress bar 120 px, 30 fps from atomic position; press dip 1.5 mm 60 ms ease; hold progress ring around pedal during 2 s hold. Drawer face keyed `boss_rc5` (note: existing model id `boss-rc5`; map hyphen<->underscore).

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | autom |
|---|---|---|---|---|---|---|---|---|
| boss_rc5_loop_level | Loop Level | 0 | 200 | 100 | % | linear | 20 | yes |
| boss_rc5_rhythm_level | Rhythm Level | 0 | 100 | 80 | % | square | 20 | yes |
| boss_rc5_rhythm_on | Rhythm On | 0 | 1 | 0 | bool | n/a | 0 | yes |
| boss_rc5_tempo | Tempo | 40.0 | 300.0 | 120.0 | bpm | linear | 50 | yes |
| boss_rc5_pattern | Pattern | 1 | 57 | 1 | int | n/a | 0 | yes |
| boss_rc5_variation | Variation A/B | 0 | 1 | 0 | bool | n/a | 0 | yes |
| boss_rc5_kit | Kit | 1 | 7 | 1 | int | n/a | 0 | yes |
| boss_rc5_memory | Memory | 1 | 99 | 1 | int | n/a | 0 | yes |
| boss_rc5_reverse | Reverse | 0 | 1 | 0 | bool | n/a | 5 | yes |
| boss_rc5_pedal1 | Pedal 1 | 0 | 1 | 0 | momentary | n/a | 0 | yes |
| boss_rc5_pedal2 | CTL1 footswitch | 0 | 1 | 0 | momentary | n/a | 0 | yes |
| boss_rc5_pedal3 | CTL2 footswitch | 0 | 1 | 0 | momentary | n/a | 0 | yes |
| boss_rc5_sync_clock | Sync Clock | 0 | 1 | 0 | enum AUTO/INTERNAL | n/a | 0 | yes |
| boss_rc5_quantize | Loop Quantize | 0 | 1 | 0 | bool | n/a | 0 | yes |
| boss_rc5_exp_target | EXP target | 0 | 2 | 0 | enum | n/a | 0 | no |
| boss_rc5_exp | EXP | 0 | 1 | 0 | norm | linear | 10 | yes |
| boss_rc5_hold_ms | Hold time | 800 | 3000 | 2000 | ms | linear | 0 | no |
| boss_rc5_dtap_ms | Double-tap window | 150 | 600 | 400 | ms | linear | 0 | no |
| boss_rc5_lofi | Converter model | 0 | 1 | 0 | bool | n/a | 0 | yes |
| boss_rc5_dry_mute | Dry mute (EXT) | 0 | 1 | 0 | bool | n/a | 20 | yes |
| boss_rc5_half_speed | Half speed (EXT) | 0 | 1 | 0 | bool | n/a | 20 | yes |
| boss_rc5_ram_cap_mb | RAM cap | 64 | 2048 | 256 | MB | linear | n/a | no |
| boss_rc5_clock_out | MIDI clock master out (EXT; not in host lane list, state only) | 0 | 1 | 0 | bool | n/a | 0 | no |
| boss_rc5_rx_map | CC#80-87 FUNC map (state blob via saveExtra) | - | - | all unassigned | blob | n/a | 0 | no |
Non-param state: mode enum, loop length, position, undo flag, memory metadata (not host-automated).

## 9 Tests/acceptance
1. Record 10 s sine, tap -> PLAY: length exact; wrap sample-continuous (|step| < 1e-4 float).
2. Cycle REC/PLAY/OVERDUB x5: layer sum equals sum of inputs, null residual < -120 dBFS (float).
3. Undo then redo bit-exact; undo residual null < -120 dBFS.
4. Double-press in PLAY -> STOP within 1 block; no step > -60 dBFS.
5. STOP hold >= hold_ms -> buffer empty; hold < hold_ms - 1 sample -> no clear.
6. Reverse: output == time-reversed loop bit-exact; toggling mid-play click < -60 dBFS (5 ms fade).
7. LOOP LEVEL sweep 0,50,100,150,200 -> linear gain 0,0.5,1,1.5,2 +/-0.1 dB; dry unchanged +/-0.01 dB.
8. Tempo: tap at 120 BPM x8 -> 120.0 +/-0.5; MIDI clock 24 ppqn at 90 BPM -> tempo 90.0 +/-0.3 within 2 beats; no clock 500 ms -> INTERNAL fallback.
9. PC 0..98 -> memory 1..99 (sample-exact before next block); PC 99+ ignored.
10. CC 127/0 timing reproduces hold/double-tap identically to UI gestures.
11. Rhythm: 57 patterns x 2 variations render, no DC (< -80 dBFS), tempo jitter < 1 sample/bar; 7 kits.
12. Storage: 90 min record at 48k with spill: no xrun (audio-thread time < 50% budget), no alloc (allocation hook), disk full -> clean auto-PLAY.
13. 99 memory save/restore round trip bit-exact; sample-rate change 44.1/48/96 length +/-1 sample.
14. Block sizes 1/4/16/64/512; TSan clean; 30-min random gesture soak: no NaN/crash.
15. Frequency response with lofi=1 (ESTIMATED, no hardware capture): 20 Hz -3 dB, 1 kHz 0, 20 kHz -3 dB. THD+N dry < -100 dB (lofi off).

## 10 Known inaccuracies
- Official manual PDF still unreachable (pass 2); verified only via search snippets. Still ESTIMATED: SETUP defaults (RX CTL CH, OMNI, TX CH), CTL1/CTL2 defaults, EXP target list, hold/dtap windows other than hold 2 s, display, level taper above 100, undo depth. OMIT: factory CC80-87 map, CC7/CC11, velocity/aftertouch maps.
- Rhythm pattern/kit sounds are original, not Boss samples; genre list unverified.
- Half-speed not found for RC-5 (EXT only). No input/track FX evidence for RC-5 (RC-505/500 have; do NOT port).
- Pedal count RESOLVED: 1 built-in pedal + CTL1/CTL2 external.
- Converter response/latency/overdub saturation unmeasured; storage 13 h at float32 exceeds practical disk, disk format differs from hardware.
- Loop is not time-stretched on tempo change (assumed).
- Existing models/boss-rc5.md claim of 13 h "real time" retained as storage cap only.

## 11 Verification log
Pass 2 (2026-10-21). Primary PDFs/pages not fetchable (static.roland.com, files.kraftmusic.com, manualslib, support.roland.com, help.morningstar.io, boss.info: ENOTFOUND/no connection; archive.org unreachable). "DOCUMENTED" below = official Boss/Roland text seen in WebSearch result snippets. Sources reached (search-level): boss.info/us/products/rc-5, kraftmusic.com, wollongongmusic.com.au, manualslib manual p18 snippet, static.roland.com PDF snippet, Roland support/Morningstar snippets, guitarworld, looperpedalreviews, musicgearspecialist, sweetwater, equipboard. No ElectroSmash/schematic exists (digital).

| # | item | old -> new | status | source |
|---|---|---|---|---|
| 1 | dimensions | ~73x129x59 mm est -> 73x129x56 mm, 450 g | DOCUMENTED | boss.info/us/products/rc-5 (S8) |
| 2 | pedal count | 1 vs 3 conflict -> 1 built-in pedal + CTL1/CTL2 external | DOCUMENTED | S8, S9, looperpedalreviews |
| 3 | PEDAL FUNC scope | main/stop/undo -> pedal, CTL1, CTL2 assignable; pedal default TRK REC/PLY | DOCUMENTED | S9 |
| 4 | CTL1/CTL2 default functions | stop/undo est -> unchanged plugin default | ESTIMATED | not found |
| 5 | RX CTL CH | 1..16 est -> 1..16 range confirmed; default 1 | range DOCUMENTED, default ESTIMATED | S9 |
| 6 | OMNI | missing -> on/off setting added; default on | setting DOCUMENTED, default ESTIMATED | S9 |
| 7 | TX CH | 1..16 -> 1..16 or 'RX CTL' | DOCUMENTED, default ESTIMATED | S9 |
| 8 | CC#80-87 mapping | invented defaults (CC80 pedal...CC87 mem-) -> all unassigned, user-assignable from function list | default map OMIT; assignability DOCUMENTED | S9 |
| 9 | CC7 loop level / CC11 EXP | present -> removed | OMIT | no source |
| 10 | Program change | 0..98 -> mem 1..99 est -> PC 1-99 = memory, 100-128 ignored; 0/1-based wire numbering kept as 0..98 | range DOCUMENTED, numbering ESTIMATED | S9 |
| 11 | Bank Select | ignored est -> ignored | DOCUMENTED | S9 |
| 12 | CC momentary value | 127 then 0 | REPORTED (manual silent) | S6 |
| 13 | SETUP path | none -> SETUP > CONTROL > CC#80 FUNC | DOCUMENTED | S9 |
| 14 | EXP pedal models / FS accessories | none -> FV-500H/L, EV-30, EV-5; FS-5U, FS-6, FS-7 | DOCUMENTED | S8 |
| 15 | clear gesture (hold in STOP) | DOCUMENTED S3 -> kept, but not reconfirmed in pass 2 snippet | DOCUMENTED (S3 only) | S3 |
| 16 | undo/redo hold 2 s | -> confirmed ("two seconds or longer") | DOCUMENTED | S9 |
| 17 | rhythm 57x2 / 7 kits | conflict w/ 16 rhythms -> 57x2, 7 kits | DOCUMENTED | guitarworld (S5) |
| 18 | 99 mem, 1.5 h/13 h, 32-bit | -> unchanged | DOCUMENTED | S4/S8 |
| 19 | display | 128x? colour -> backlit display, resolution unknown | ESTIMATED | Thomann via S4 |
| 20 | loop-level 100..200 = +6 dB | unchanged | ESTIMATED (range 0-200 DOCUMENTED) | derivation: 200% = 2x |
| 21 | converter band-limit 20 Hz/20 kHz, latency 1-3 ms, 2 ms crossfade, +/-4.0 limit | unchanged | ESTIMATED | topology/typical ADC filters |
| 22 | hold 2.0 s / dtap 400 ms window | hold DOCUMENTED, dtap ESTIMATED | mixed | S9 / plugin choice |
| 23 | half-speed | EXT off | OMIT as hardware claim (kept as labelled EXT) | no source |
| 24 | velocity->overdub gain, aftertouch map | present -> removed | OMIT | EXT without ParamDesc |
| 25 | exp_min/exp_max, rhythm_sep, ext_fs_count, ram_minutes | orphan non-param names -> removed/merged to ram_cap_mb | inconsistency fixed | internal |
| 26 | pedal2/pedal3 labels | 'Pedal 2 (stop)'/'Pedal 3 (undo)' -> CTL1/CTL2 (ids unchanged) | fixed | internal |
| 27 | tap-tempo line / quantize tolerance | messy -> cleaned, marked plugin choice | fixed | internal |
| 28 | SYNC CLOCK AUTO/INTERNAL, tempo 40.0-300.0 | unchanged | DOCUMENTED | S2/S3 |
