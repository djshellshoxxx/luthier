# MIDI & Expression spec — Reference Pedalboard

Version 0.1 | 2026-10-21 | Status: SPEC (not implemented)
Scope: clock/transport sync, tap tempo, CC/PC maps, MIDI learn, keyboard-as-footswitch gestures, expression, curves, conflicts, per-pedal default maps, lock-free pipeline, persistence, tests.
Tags: DOCUMENTED = manufacturer manual/public behaviour. ESTIMATED = chosen by us, no hardware measurement. MEASURED = none yet (no hardware captured).
Namespace: `Source/ReferencePedals/Midi/` (new). Never touch legacy `EffectsChain`, `PedalRack`, `PedalType`. Board off => MIDI layer inert, audio bit-identical.
Params: all IDs snake_case; pedal params use `PedalDesc.paramPrefix` (BOARD_CORE sec 11; e.g. `boss_dd3t_time`); board-level ids are `ref_board_*` (this doc previously wrote `board_*`; canonicalised), slot-level `ref_s{nn}_*`. Namespace `luthier::reference::midi`. Reconciliation: IMPLEMENTATION_PLAN sec 1 wins on conflict. See pedal specs in `specs/pedals/` for ID lists.

## 0. Legal / honesty
- Real units' MIDI behaviour is emulated functionally ("inspired by"). Do not copy manual tables verbatim; CC numbers marked ESTIMATED are our own allocation. No claim of byte-exact hardware MIDI implementation.
- Pedals with no MIDI in hardware (DD-3T, Ditto, Nano Looper 360) get a clearly labelled **Virtual MIDI (not on hardware)** map, off by default for hardware-faithful presets.

## 1. Module layout (modular)
| File | Role |
|---|---|
| `MidiTypes.h` | `MidiEv` POD (24 B, router-internal): `uint32 sampleOffset; uint8 status,d1,d2; uint8 src; uint16 pad; double ppq`. Converted to the canonical pedal-facing `MidiEvent {status,d1,d2,offset}` (BOARD_CORE sec 11) only for `wantsRawMidi` pedals |
| `MidiRouter.h/.cpp` | audio-thread entry: block MIDI -> per-block event list, filter by channel, dispatch |
| `ClockSync.h/.cpp` | 24ppqn tracker, host transport, tempo PLL, tap tempo |
| `MidiMap.h/.cpp` | binding table (immutable snapshot), resolve, conflict rules |
| `BoardMidiLearn.h/.cpp` | learn FSM (message thread) + RT capture hook. Named `Board*` to avoid confusion with legacy `Source/Support/MidiLearn.*` (`luthier::MidiLearnManager`), which stays untouched and independent |
| `Curves.h` | curve LUT + eval |
| `GestureEngine.h/.cpp` | velocity/aftertouch -> footswitch press synthesis |
| `ExprEngine.h/.cpp` | expression sources (CC/AT/PB/keyboard/slider), smoothing |
| `MidiState.h/.cpp` | (de)serialise, migration |
| `Source/Tests/ReferencePedalsMidiTests.cpp` | all tests sec 14 |
Each <= 500 LOC, header-only where hot. No JUCE types on RT path except `juce::MidiBuffer` iteration at router entry.

## 2. Lock-free pipeline (MIDI -> param)
```
[Host MIDI in / Standalone device / on-screen kbd / UI sliders]
   | (device thread or message thread)            | (plugin host: processBlock midi buffer)
   v                                              v
 SPSC ring<MidiEv,1024> "uiInRing"        MidiRouter::process(midiBuffer, blockStart, n)
   +----------------> merged by timestamp <---------+
                              v
        MidiMap snapshot (atomic<const MapSnap*>) -> resolve (O(1) table[chan][type][num])
                              v
        Source stage: Curve -> range -> invert -> smoothing target  (ExprEngine)
        Gesture stage: GestureEngine (velocity/AT) -> FootswitchEvent{pedalIdx, kind, tNs, hard}
        Clock stage: ClockSync -> tempo/transport/ppq
                              v
        ParamWrite{slot,paramIdx,norm,smoothMs,offset} -> `IPedal::setParam(idx,norm)` (+ FootEvent / Trigger writes) at sampleOffset
                              v
        pedal.process(L,R,n) (BOARD_CORE IPedal) is called
```
Rules:
- RT-safe: no alloc, locks, logging, file/UI calls. All containers fixed-capacity (`std::array`/preallocated in `prepare()`).
- Rings: `uiInRing` SPSC 1024 events (producer = device/UI thread, consumer = audio). Overflow: a full ring drops the new item (wait-free SPSC cannot drop the oldest, errata E25), increment `std::atomic<uint32> dropped`; UI shows "MIDI overload". Clock bytes (0xF8) never dropped (reserve 256 slot priority sub-ring).
- Audio->UI feedback (learn captures, LED/knob echo, meters): SPSC ring<UiEv,512> audio->message; UI polls by 60 Hz timer. No `callAsync` from audio.
- Map edits: message thread builds new immutable `MapSnap` (flat arrays), publishes via `atomic<const MapSnap*>` release; audio acquires once per block; old snap retired to `RetireQueue` freed on message thread after 2 blocks (epoch counter). Never mutate live snap.
- Sample-accurate: events carry `sampleOffset` in [0,n). Router sorts stably by offset (insertion sort, n<=256/ block typical; cap 512 events/block, extras dropped+counted). Board splits processing at each distinct offset where a *discrete* event (footswitch, switch, mode, transport, tap) lands; continuous values (CC knobs) are applied via per-sample smoother ramps starting at offset (no split). Minimum split length 16 samples (coalesce events within 16 samples to the next boundary, preserving order; ESTIMATED to bound CPU/overhead).
- UI-origin events (on-screen kbd, sliders) have no host offset: stamped offset 0 of next block.
- MIDI output (clock out, PC out, learn echo): `juce::MidiBuffer` appended at same offsets in `processBlock` (plugin must declare `producesMidi`); only if host supports; otherwise disabled with UI note.
- Determinism: same input event stream + same block partition-independent result within 1 LSB: sub-block split must not change audio vs unsplit when only continuous values change (test T-12).

### 2.1 Source taxonomy (`MidiSrc`)
`CC(0-127, 14-bit pair MSB cc n / LSB cc n+32 optional)`, `NRPN` (optional, ESTIMATED), `Note(on/off, key)`, `NoteVel`, `PolyAT(key)`, `ChanAT`, `PitchBend` (14-bit), `PC`, `ClockStart/Stop/Continue`, `UiSlider` (internal), `Keyboard.Velocity`.
Channel: 1-16 or OMNI per binding; global "MIDI channel" default OMNI. Receive filter per source: `ref_board_midi_in_enable`, `ref_board_midi_chan` (0=omni,1-16).
14-bit CC: if binding flag `hi_res`, value = (msb<<7|lsb)/16383; LSB arrival without MSB ignored; MSB alone applies 7-bit (lsb=last known or 0). ESTIMATED.

## 3. MIDI clock & transport sync
### 3.1 Tempo master selection (`ref_board_tempo_source`, enum)
0 INTERNAL (tap/knob), 1 HOST (AudioPlayHead), 2 MIDI_CLOCK (0xF8 in), 3 AUTO (default). AUTO priority: HOST playing > MIDI_CLOCK (if >=2 ticks in last 500 ms) > INTERNAL. Hysteresis: switch source only after 2 beats of stable alternative or 1 s (ESTIMATED) to avoid flapping; switch changes tempo via 50 ms glide (`tempo_glide_ms`, default 50).
Global board tempo `ref_board_tempo_bpm` (20.0-300.0, default 120.0). Each time-based pedal has `<id>_sync` (bool, default per sec 9) and `<id>_division` (enum sec 3.5). Pedals read tempo from `ClockSync::snapshot()` (struct: `double bpm; double ppqPos; bool playing; bool valid; uint32 tickCountSinceStart`).

### 3.2 Host transport (DOCUMENTED API: JUCE `AudioPlayHead::getPosition()` -> `PositionInfo`: `getBpm`, `getPpqPosition`, `getIsPlaying`, `getTimeSignature`, `getIsLooping`, `getLoopPoints`)
- Query once at block start (RT-safe, no alloc). Missing fields => host invalid => fall back to next source.
- ppq at sample i = `ppqStart + i * bpm/(60*sr)` (linear within block; if tempo changes mid-block, host provides only block-start; use that, ESTIMATED).
- Transport events synthesised: isPlaying false->true => `TRANSPORT_START` (or `CONTINUE` if ppq != 0 and ppq != loop start; START if ppq==0); true->false => `STOP`; ppq jump |delta - expected| > 0.05 beat => `LOCATE` (relocate; looper/delay phase re-align).
- Time signature: used for looper bar quantize (beats per bar = numerator * 4/denominator); default 4/4.

### 3.3 MIDI clock in (24 ppqn, DOCUMENTED MIDI 1.0 spec: 0xF8 timing, 0xFA start, 0xFB continue, 0xFC stop, 0xF2 SPP)
State machine `ClockFSM`: `STOPPED` -> (0xFA) -> `ARMED_START` -> (first 0xF8) -> `RUNNING` (tick=0) ; `RUNNING` -> (0xFC) -> `STOPPED_HOLD` ; `STOPPED_HOLD` -> (0xFB) -> `RUNNING` (tick continues) ; (0xFA) from any -> tick=0 RUNNING after next 0xF8. 0xF2 SPP (14-bit, units of 6 clocks = 1/16 note): sets tick = spp*6 while STOPPED; ignored while RUNNING (ESTIMATED).
Timeout: no 0xF8 for 500 ms (or > 4 expected intervals at current tempo, min 250 ms) => `clockLost`, state -> STOPPED_HOLD, fall back per 3.1; tempo freezes at last value.
Tick timestamps: sample-accurate `tSample = blockStartSample + sampleOffset` as int64 (monotonic running counter, never reset by transport).
### 3.4 Tempo estimation + jitter smoothing
Goal: stable BPM despite USB/host jitter (typ +-1-3 ms).
1. Interval `d_k = t_k - t_{k-1}` (samples). Reject if d outside [sr*60/(300*24)*0.5, sr*60/(20*24)*2] (outlier gate).
2. Median-of-5 on d -> `dm`.
3. PLL: predicted `tp_k = tp_{k-1} + P`; period estimate `P` update `P += Kp*(dm-P)` Kp=0.05 (ESTIMATED), phase error `e = t_k - tp_k`, `tp_k += Ki*e` Ki=0.25 (ESTIMATED). Lock when |e| < 0.5 ms for 24 consecutive ticks; unlocked Kp=0.3 Ki=0.5 for fast acquire (first 24 ticks).
4. `bpm = 60*sr/(24*P)`. Display rounded 0.1 BPM. Quantise-to-integer snap: if |bpm - round(bpm)| < 0.05 and locked, snap (helps DAW round tempos).
5. Delay time modulation by bpm changes: delay pedals use `time_glide` (see pedal spec) — recommended 1 pole slew 30 ms to avoid zipper/pitch artefact; for BBD-model pedals tempo change slews naturally (pitch shifts allowed, authentic).
6. Derived 24ppqn phase: `tickPhase` 0..1 inside tick for sub-tick interpolation of LFO phase reset.
Latency: PLL adds 0 samples to audio; lock time <= 1 beat (24 ticks).

### 3.5 Division / subdivision
`<id>_division` enum (index, multiplier of quarter-note beats `q`):
0 `1/1`(4.0), 1 `1/2`(2.0), 2 `1/2T`(4/3), 3 `1/2D`(3.0), 4 `1/4`(1.0), 5 `1/4T`(2/3), 6 `1/4D`(1.5), 7 `1/8`(0.5), 8 `1/8T`(1/3), 9 `1/8D`(0.75), 10 `1/16`(0.25), 11 `1/16T`(1/6), 12 `1/16D`(0.375), 13 `1/32`(0.125), 14 `2/1`(8.0), 15 `Dotted-8th+8th` (DD-3T style "dotted eighth" = 0.75 duplicate alias of 9, kept for UI label).
Time(ms) = 60000/bpm * mult. Frequency(Hz, LFO) = bpm/60 / mult.
Per pedal `division_allowed` mask limits legal values (sec 9). Out-of-range resulting delay time: clamp to pedal min/max, and if clamped by max, auto halve (up to 3 times) and flag UI "halved" (ESTIMATED, matches how many synced delays behave).
Phase alignment for LFO pedals (tremolo/phaser/chorus/flanger): on `TRANSPORT_START`/`LOCATE` with `<id>_sync_phase_lock` (default true when synced) set phase = frac(ppq/mult) * 2π (+ `<id>_lfo_phase_offset`). Otherwise free-run, rate follows tempo only. Phase slewing: if |err| > 5% cycle jump with 20 ms crossfade of LFO output, else gentle 100 ms correction.
Looper quantize: `<id>_quantize` enum {off, beat, bar(default for rc5 synced), 1/2 bar, 2 bars, 4 bars}; record start/stop gestures are deferred to next grid line (pending state `ARMED` shown by blinking LED), sample-exact via ppq. `quantize_late_window_ms` (default 80, ESTIMATED): gesture arriving within window AFTER grid line snaps backward (loop start = grid line, audio captured earlier is retroactively included via pre-roll ring 200 ms) else waits for next.
Loop length lock: on synced stop, loop length = integer multiple of `bar` (round to nearest); error absorbed by time-stretch-free trim/pad of <=1/32 beat.

### 3.6 Start/stop/continue handling per consumer
| Event | Delay | LFO (trem/phaser/chorus) | Looper |
|---|---|---|---|
| START | no change (tempo only); optional `<id>_clear_on_start` false | phase reset (if phase_lock) | if `<id>_follow_transport`: PLAY from loop start (or begin record if `ARMED`) |
| STOP | feedback tail continues | free-run at last rate | if follow: stop (STOPPED), tail 10 ms fade |
| CONTINUE | none | phase from ppq | resume from ppq-derived loop position `((ppq - ppqLoopOrigin) mod loopBeats)` |
| LOCATE | none | re-align phase | re-align read pointer with 10 ms crossfade |
| clockLost | freeze tempo | freeze | stop follow (remain in current state), UI badge |

### 3.7 Tap tempo
Gesture source: footswitch tap (UI click, MIDI note, CC value>=64 rising edge, keyboard gesture). Board-level `ref_board_tap` and per pedal `<id>_tap` (only if pedal has tap).
Algorithm (DOCUMENTED behavior: tap tempo on DD-3T averages of last taps; exact algorithm ESTIMATED):
- Keep last 4 tap timestamps (sample counter). New tap if gap in (60000/300=200 ms ... 3000 ms); gap > `tap_timeout_ms` (default 2000) => restart chain (this tap = tap#1, no tempo change).
- Interval = mean of last up to 3 intervals; apply with weights 1,2,3 (newest highest). Result clamped to pedal min/max time.
- After >= 2 taps tempo applies immediately (first interval) and refines on taps 3,4.
- Velocity-aware accuracy: see 5.4 (`tap_jitter_comp`).
- Tap in synced mode sets `ref_board_tempo_bpm` only if source INTERNAL or AUTO-with-no-external; otherwise ignored, UI toast "Tempo source external".
- DD-3T style: tap sets delay time directly; with division chosen, tap interval is interpreted as a quarter note and time = interval * mult (DOCUMENTED: DD-3T has tap subdivision switch behavior, ESTIMATED mapping).
- Tap-hold >= 700 ms (non-looper pedals, `tap_hold_ms`) = no op (reserved for per-pedal function, e.g. DD-3T none).
- Tap LED blink = beat at tempo, 50 ms on.

## 4. CC / PC mapping model
### 4.1 Binding record (`Binding`, fixed 32 B in `MapSnap`)
Normative layout: `Source/ReferencePedals/Midi/MidiTypes.h` (errata E26). `target` is one global index (`slot*kMaxParams+paramIdx`, board targets from `kTargetBoardBase`), mode and curve id share one byte, `curveParam` is a float. The field list below is the logical content.
`uint16 id; uint8 chan(0=omni); uint8 srcType; uint8 srcNum; uint8 flags(hi_res, relative, invert, latch, soft_takeover, enabled); uint16 targetParamIdx; float inMin,inMax(0..1 normalised source), outMin,outMax(0..1 normalised param); uint8 curveId; float curveParam; uint16 smoothMs; uint8 mode(absolute|relative|toggle|momentary|trigger|step); uint8 priority`.
Targets: any param descriptor from the pedal's `ParamDesc` list (knob, switch, enum, button, footswitch virtual param `<id>_fs_engage`, `<id>_fs_tap`, etc.), plus board params (`ref_board_mix`, `ref_board_tempo_bpm`, `ref_board_enabled`, `ref_s{nn}_enabled`, `ref_board_scene_next`...). Normalised 0..1 domain: bindings write normalised; pedal applies its own taper (taper stays in pedal spec). Never bypass taper.
Mode semantics:
- absolute: target = curve(map(src)).
- relative (two's-complement/binary-offset/signed-bit per `flags`, ESTIMATED): target += delta*`rel_step` (default 1/128 of range).
- toggle: rising edge (src>=64 / note-on) flips target 0<->1 (switch/enum cycles).
- momentary: target=1 while held, 0 on release (note-off or value<64).
- trigger: one-shot event (tap, clear, undo).
- step: for enums: value quantised to N steps with src range split equally; `n_steps` from descriptor.
Soft takeover (`soft_takeover`, default ON for absolute CC to knobs, OFF for switches): ignore incoming until src value crosses/near within 3% (of current param) in normalised space; UI knob shows ghost-marker. Reset on preset change.

### 4.2 Program Change
- Board-level PC => scene/preset: `ref_board_pc_mode` {off, scene (default), pedal_memory}. Scenes = saved set of {engaged pedals, knob values, order not changed}; 128 scenes `ref_board_scene_0..127`. PC N loads scene N (if exists else ignore). Bank select CC0/CC32 (MSB/LSB) => bank*128+PC (up to 16383). Scene recall = no click: knob targets ramp over `scene_ramp_ms` (default 40), switches apply at block boundary, delay time glides, looper state NOT changed unless scene stores `looper_cmd`.
- Per-pedal PC (pedal_memory mode, for pedals with memories: RC-5 99 mem, Nano 360 11 mem): PC value -> memory slot number (RC-5: PC 0..98 -> memory 1..99, DOCUMENTED RC-5 PC selects memory); channel can be per pedal `<id>_midi_chan`. Memory switch while loop playing: apply at loop end (quantized) unless `<id>_mem_switch_immediate`.
- Output PC: `ref_board_pc_out_enable` default off; on scene change, send PC on `ref_board_midi_out_chan`.

### 4.3 Default global CC reservations (apply to all bindings unless overridden; ESTIMATED allocation; follow MIDI 1.0 standard meanings)
CC1 mod wheel (free source), CC7 volume -> `ref_board_output_level`? NO: reserved: CC7 -> `ref_board_mix` OFF by default (do not hijack); CC64 sustain -> default source for "momentary fs" of the SELECTED pedal (see 6.5); CC11 expression -> selected pedal main expression target; CC120 all sound off -> panic (kills delay feedback buffers, looper stops playing), CC121 reset all controllers -> reset expression sources to defaults, CC123 all notes off -> release held gestures. CC0/32 bank. These cannot be bound by user to other params (guard list, UI shows lock icon) except 1, 11 and 64.
Free CC pool for auto-allocation: 14-31, 70-79, 80-83, 85-90 (undefined/general controllers per MIDI 1.0, DOCUMENTED).

## 5. Keyboard note/velocity/aftertouch -> footswitch gestures
Purpose: a velocity/aftertouch keyboard plays the pedal's footswitch like a foot.
### 5.1 Binding
Note key (or key range = 'pad bank', one key per pedal in chain order) bound to a pedal footswitch. Default `ref_board_kbd_fs_base_note` = 36 (C2), pedal i (chain position 0-based) -> note 36+i, up to 16 pedals (36..51). Channel default omni. Alternates: note per pedal assigned via learn.
### 5.2 Press model (`FsPress`)
Input: note-on velocity v (1..127), note-off (release), poly AT (key) or channel AT a (0..127) while held.
- `hardness h = curve(v/127)` default curve Linear; `pressSpeed s` = hardness mapped to simulated switch travel time `travel_ms = lerp(60 ms, 8 ms, h)` (soft press 60 ms, hard stomp 8 ms; ESTIMATED).
- Effects of velocity:
  1. **Engage ramp (click/crossfade):** bypass<->effect crossfade time `engage_ms = lerp(25, 2, h)` for pedals with buffered/DSP bypass (min 2 ms to avoid click, hardware true-bypass pop modelled only if `<id>_switch_pop` enabled: pop amplitude `popAmp = h^2 * popMax`, popMax per pedal default 0 = off).
  2. **Mechanical bounce (optional, `ref_board_fs_bounce` default OFF):** hard press (h>0.9) may produce double-trigger within 3 ms on cheap-switch models (generic "bad switch" option) — off by default; if on, debounce logic of the pedal must reject (tests T-18).
  3. **Latching vs momentary:** by pedal footswitch type (sec 9) — latching (DS-1, TS808, Muff, RAT, Phase 90, CE-2, DynaComp, D+, SD-1, DD-3T) toggles on note-on; momentary (selected pedals with momentary option, e.g. DD-3T hold? no; use `<id>_fs_mode`) active while key down. Per pedal `fs_mode` {latch, momentary, auto}: auto = latch if press shorter than `auto_hold_ms` 400 ms, else momentary (press-and-hold = temporary, released on note-off) — ESTIMATED; common "latching on tap, momentary on hold" pedal behaviour.
  4. **Tap tempo accuracy:** tap time stamped at *contact point* not note-on: `tTap = tNoteOn + travel_ms*0.5*(1-h)` (soft press registers later, models real stomp physiology, `tap_jitter_comp`, default on). With keyboard timing, real tap error is the human's; tests compare to stated formula.
  5. **Looper tap vs hold-to-erase:** gesture decode from note-on/off durations (sample timestamps): see 5.3. Velocity modifiers: soft press (h<0.25) registers as *tap* only if release < `tap_max_ms`; hard press (h>=0.75) shortens double-tap window by 20% (fast stomps) — `double_tap_ms = lerp(350, 280, h_avg)`; (ESTIMATED).
- Aftertouch usage (selectable `fs_at_mode`): 
  - `off` (default).
  - `depth`: while key held, AT modulates a bound continuous param (typically wet/mix or expression) as "rock the pedal"; value = curve(AT/127).
  - `pressure_hold`: AT > `at_hold_threshold` (default 90) is interpreted as a *hold* (same as pressing long) even before `hold_ms`; lets looper "hold to erase" be done by pressing hard. ESTIMATED.
  - `rocker` (for treadle/expression models): AT maps to treadle position.
- Channel AT vs poly: if keyboard sends channel AT only, it applies to all held keys (use max). If poly AT, per key. Config `ref_board_at_source` {poly, channel, both(default max)}.
- Release velocity (note-off vel): if non-zero used to scale `engage_ms` of momentary release: `lerp(25,2,vOff)`; default 64 treated as unknown => 12 ms.

### 5.3 Looper gesture decoder (shared, sample-timestamped; implements separately from PhraseLooper per `PHRASE_LOOPER_FOUNDATION.md`)
Inputs: events {down(tSample, h), up(tSample)}. Thresholds in ms (convert at prepare):
| Param | Default | Notes |
|---|---|---|
| `tap_max_ms` | 350 | press shorter = tap |
| `double_tap_ms` | 350 (280 hard) | gap between tap#1 up and tap#2 down |
| `hold_ms` | 1000 (Ditto: 2000 hold-to-undo/redo? see below) | press longer = hold |
| `erase_hold_ms` | 2000 | hold while stopped = clear |
Decode: `TAP`, `DOUBLE_TAP`, `HOLD` events emitted: TAP emitted at up if no second down within `double_tap_ms` (so TAP latency = double_tap_ms except when pedal has no double-tap action in current state: emit immediately, "fast tap" optimisation, T-17); `DOUBLE_TAP` at 2nd down; `HOLD` at `hold_ms` after down while still down (not at release). Action mapping per pedal is in sec 9 (DOCUMENTED functions; ESTIMATED timings):
- Ditto (DOCUMENTED function set): tap = record->play->overdub cycle, double-tap = stop, hold (>~3 s) = clear? Use `erase_hold_ms` 2000, undo/redo: hold while playing (>=1 s) toggles undo/redo (DOCUMENTED: Ditto undo by holding while playing, redo by holding again). Double tap stop.
- Nano 360 (DOCUMENTED style): tap = rec/play/overdub; double-tap = stop; hold when stopped = erase; hold while playing = undo/redo. 
- RC-5: dedicated hardware button set; footswitch gestures = REC/PLAY/OD, double tap stop, hold 2 s = clear (ESTIMATED), plus 3 pedal-like buttons incl. undo/redo, rhythm start/stop.
UI footswitch click (mouse): pointer-down/up timestamps drive same decoder (velocity from click = hardness 0.5; modifier Shift = soft h=0.2, Ctrl = hard h=0.9).

### 5.4 Accuracy test for tap tempo
Synthetic: taps at exact 500 ms (120 BPM) with +-5 ms Gaussian jitter, h varying => estimated BPM within +-1.5 BPM after 4 taps, and exactly 120.0 with zero jitter and equal h (T-08).

## 6. Mapping/learn
### 6.1 Learn FSM (message thread owns; audio only forwards)
States: `IDLE` -> (user clicks MIDI-learn on any control or right-click "Learn") -> `ARMED(target)` -> first accepted incoming message (note-on / CC / PB / AT / PC) -> `CAPTURED(src)` -> `CONFIRM` (shows src name; user may adjust curve/range) -> commit => `IDLE`. Cancel on ESC, 15 s timeout, or clicking elsewhere. `LEARN_ALL` mode: sequentially arm every control in a pedal face (next on commit).
Capture rules: ignore clock/active-sensing/sysex; CC: wait for >=2 distinct values within 400 ms to avoid capturing a 1-off stray (for knobs) except for switch targets (accept first message); for CC jitter use "most active CC wins over 500 ms window if multiple". Note: capture note number; if target is continuous param and message is note => offer mode `NoteVel`/`PolyAT`. Pitch bend/aftertouch captured only if movement span > 12 (7-bit) within 400 ms. Resolution of ambiguous types: prefer CC > PB > ChanAT > PolyAT > Note.
RT hook: when `learnArmed` (atomic<bool>), router pushes first 8 candidate events into `learnRing` (SPSC) for message thread to process; normal routing of those events is suppressed while armed.
Learned binding defaults: `soft_takeover` per target type, curve linear, range full, smooth 10 ms for knobs.
### 6.2 Targets for learn
Any visible control: knob, slider, toggle, mode switch, footswitch, tap, board wet/dry, tempo, pedal bypass, scene next/prev, pedal order move (`ref_board_move_selected_left/right`), drawer open/close (UI only: `ref_board_ui_toggle` via message thread, not audio).
### 6.3 Sliders / knobs "mapped in the same way"
On-screen sliders, host automation lanes and external controllers all enter via the same `Binding` table; UI sliders (UiSlider source) can be bound to other targets (macro): `macro_1..8` (0..1) as bind sources/targets, each macro fans out to up to 8 bindings with independent range/curve (ESTIMATED).
### 6.4 Learn UX details
Right-click control -> "MIDI Learn", "Clear mapping", "Mapping...". Mapping dialog shows: source (type/ch/num), mode, min/max (draggable), curve editor (sec 7), invert, smoothing ms, soft takeover, hi_res. Activity LED on bound controls (flash 80 ms on event). Learn state not shown in host-undo.
### 6.5 Selected-pedal focus mapping
`ref_board_selected_pedal` (index, UI-driven). "Focus" bindings (`target = @sel.knob[k]`, k=0..7) route the generic 8-knob controller bank (CC 70-77 default ESTIMATED) to the selected pedal's first 8 params in descriptor order; switching selection snaps soft-takeover. CC11 -> `@sel.expression`; CC64 -> `@sel.fs`.

## 7. Curves, range, smoothing
### 7.1 Curve editor
Curve IDs: 0 linear, 1 log (audio-taper-like), 2 exp, 3 s-curve, 4 inverse (=invert), 5 step (n steps), 6 custom 5-point (points x,y in [0,1], monotone cubic Hermite / PCHIP; x strictly increasing, enforced min dx 0.01).
Formulas (x in [0,1], p = `curveParam` default per curve):
- linear: y=x
- log: y = log10(1+ (10^p -1)*x)/p , p default 2 (p in 0.5..4) -> gives fast rise
- exp: y = (10^(p*x) - 1)/(10^p - 1), p default 2
- s-curve: y = x^2*(3-2x) blended by k in 0..1: y = lerp(x, smoothstep(x), k), k default 1
- step: y = floor(x*N)/(N-1) clamped
All evaluated via 256-entry LUT per binding (built on message thread, stored in `MapSnap`; audio uses linear interpolation). Monotonic enforced (test T-20 for 0 <= x1<x2 => y1<=y2 unless user-set non-monotone custom with explicit "allow non-monotone" flag, default false).
UI: 180x120 graph, draggable points, shows live input/output cursor, numeric entry. Presets in dropdown: Linear, Log, Exp, S, "Pedal heel-toe" (exp p=1.5), "Velocity soft" (log p=2), "Velocity hard" (exp p=2).
### 7.2 Range & mapping chain
`x_in` normalised source -> clip to [inMin,inMax], rescale to 0..1 -> optional invert (1-x) -> curve -> map to [outMin,outMax] (outMin>outMax allowed = inverts) -> target normalised value. Defaults: in 0..1, out 0..1. For switches/enums: thresholds, hysteresis 5% (on at >=0.5+0.025, off at <0.5-0.025).
### 7.3 Smoothing
Per binding `smoothMs` 0..500 (default 10 knobs, 0 switches, 30 for expression, 5 for gesture-AT). One-pole in normalised domain, coefficient `a=exp(-1/(smoothMs*0.001*sr))`, per-sample at target; Equal initial state to avoid jump on first event. Smoothing can add on top of the pedal's own parameter smoothing (pedal spec): effective = sum; UI warns if total > 200 ms. For time params (delay time) smoothing is replaced by pedal's own glide (avoid double-smoothing): descriptors flagged `own_smoothing` skip binding smoother. Denormal flush (`1e-15` threshold).
Rate limiting: control values applied at most per sample; CC bursts (e.g. 1 kHz) fine due to smoothing; UI echo capped 60 Hz.

## 8. Expression pedal emulation & conflict resolution
### 8.1 Expression engine
Virtual EXP input per pedal that supports one (hardware: RC-5 EXP/ CTRL, EHX loopers none; DD-3T EXT CONTROL jack takes footswitch/exp? treat as DOCUMENTED-lite) plus board-level `ref_board_exp1`, `ref_board_exp2`.
Source options per EXP: CC (default CC11 for exp1, CC4 foot controller for exp2, DOCUMENTED GM assignments), PitchBend, ChanAT, PolyAT, mod wheel CC1, keyboard velocity-last, UI slider, macro N. Heel/toe calibration: `exp_heel` (default 0.0), `exp_toe` (1.0) normalised calibrate-by-learn (UI "Rock to calibrate" captures min/max over 5 s). Heel-down switch: `exp_heel_down_switch` threshold 0.03 for 150 ms -> trigger bound (e.g. toggle bypass, "toe down switch" emulating treadle switch, ESTIMATED).
Output of exp engine = normalised 0..1 after curve+range+smoothing, delivered to `target_list` up to 4 targets (each own range/curve) e.g. wah-like: exp -> filter freq, reverb mix.
### 8.2 Pitch bend / aftertouch as expression
Pitch bend: 14-bit, centre 8192 -> normalised: bipolar mode (-1..1) or unipolar (0..1 using positive half only or full range). Default unipolar-full. Return-to-center spring behaviour: smoothing applies; "latch" option holds last value after bend release if `pb_latch`.
Channel AT: 0..127. Poly AT: per key; if multiple keys held default combine = max.
### 8.3 Conflict resolution
1. Same source bound to multiple targets: allowed (fan-out), executed in binding id order; max 8 per source, UI shows count badge.
2. Multiple sources to one target (e.g. CC and aftertouch both to wet/dry): priority by `binding.priority` (higher wins on *latest event*), tie -> last-writer-wins within a block by sample offset; plus `source_hold_ms` 300 (ESTIMATED): after a higher-priority source moves, lower ones are ignored until 300 ms of its inactivity. Sum mode (`combine=add`) available only for expression targets flagged `additive` (e.g. LFO depth), clamp 0..1.
3. UI knob drag vs MIDI: UI drag has priority (`ui_hold` while pointer down + 500 ms after); MIDI soft-takeover reactivates after.
4. Host automation vs MIDI: host automation write wins when host `isPlaying` AND parameter has automation recorded (we cannot know) -> rule: last writer wins by timestamp, soft takeover applies to MIDI after automation moves.
5. Duplicate learn: learning a source already bound -> dialog options {Replace, Add (fan-out), Cancel}. Message-thread only.
6. Reserved guard list (sec 4.3) cannot be overridden.
7. Channel conflicts between per-pedal channel and global omni: per-pedal channel (non-zero) wins for its pedal bindings; global omni bindings still match any channel except those claimed by per-pedal channels (precedence: exact channel > omni).
8. Same note bound as footswitch and as a note-trigger param: footswitch gesture binding wins (priority 200 reserved for gesture bindings).
9. Instance duplication: duplicated pedal does NOT inherit bindings (avoid double-binding); UI offers "copy mapping".
10. Reorder keeps bindings (they address by `instanceId`, not index). Index-based defaults (e.g., kbd base note + i) follow chain order recomputed on reorder; user may pin ("lock note") per pedal.

## 9. Per-pedal default maps (RECONCILED: LOCAL profiles, no global direct map)
Conventions: channel OMNI; CC values 0..127; no per-pedal PC unless stated. Three layers, in precedence order:
1. **Explicit bindings** (user/learn, sec 4).
2. **Focus mode** (default routing, sec 6.5): the generic 8-knob bank CC70-77 drives the first 8 non-Engage/non-Config params of the SELECTED pedal; CC11 -> `@sel.expression`; CC64 -> `@sel.fs`.
3. **LOCAL profile** (`ParamDesc.midiCcDefault`, listed in each pedal spec section "MIDI"/"CC map"): active only for a slot with an explicit `midi_chan` (1..16, per-slot state `midi.slotChan[instanceId]`) or that has MIDI focus AND whose "Use pedal CC profile" toggle is on (default off). Because only the addressed slot listens, profiles of different pedals may reuse the same CC numbers (every pedal spec does: CC14-19 etc.). Two slots on one explicit channel with overlapping CC: earlier chain position wins, clash list shown in the Board MIDI panel. The old global "Direct map" (`ref_board_direct_map_enable`, former 9.2 per-model CC base table with CC102-119 etc.) is REMOVED; the param id is retired (never reused).
**Pedal spec wins on CC ids; this doc wins on semantics.** Reserved CCs (4.3) overrule profiles except where 4.3 allows (1, 11, 64).
### 9.1 Generic auto-map (any pedal without a profile)
Knobs k=0..N-1 in descriptor order (Engage/Config/Ext skipped) -> `CC (20 + k)` for k < 12 (20-31, free pool) ; footswitch -> CC 85 (>=64 toggle, momentary option); only under the same local-profile activation rule. Default Note: footswitch note per sec 5.1 (`ref_board_kbd_fs_base_note` 36 + chain position); looper specs' "default note 60" means the LOCAL-profile note used when the slot has an explicit channel.
### 9.2 Footswitch mode ownership
`fs_mode {latch, momentary, auto}` (sec 5.2) is a property of the slot's FootController (`midi.fs[instanceId].mode`), NOT a pedal param. Pedal-spec params named `*_stomp_mode`, `*_footsw_mode`, `*_fs_mode`, `*_momentary` (MXR D+, SD-1, CE-2, Dyna Comp, Nano 360, ...) are REMOVED from the ParamDesc lists; those specs' behaviour text applies to the FootController mode.
### 9.3 BOSS RC-5 / RC-1 (DOCUMENTED: RC-5 has MIDI IN/OUT, receives MIDI Clock/Start/Stop and PC for memory select and can send MIDI clock/sync; exact CC set ESTIMATED)
Hardware features emulated:
- **Sync**: `boss_rc5_sync_clock` {INTERNAL, AUTO} (RC-5 spec s2.3, DOCUMENTED; AUTO = follow the board tempo source resolved by `ClockSync`, i.e. MIDI clock slave or host) + EXT `boss_rc5_clock_out` (master: generates 0xF8 at board tempo, sends 0xFA/0xFC on rhythm/loop start/stop via `IPedal::drainMidiOut`; only when host supports MIDI out, else disabled with UI note).
- **Rhythm tempo** from synced tempo when slave; rhythm start/stop follows transport when `follow_transport`.
- **PC -> memory** 0..98 -> memory 1..99 (RC-5 PC memory select DOCUMENTED; RC-1 same as virtual extension, ESTIMATED).
- **Footswitch-equivalents** (RC-5 has 1 main pedal switch REC/PLAY/OD and 2nd stop/ tap... gestures per model spec) map via notes/CC:
| Function | CC (RC-5 spec s6 wins) | Note | Mode |
|---|---|---|---|
| main_fs (rec/play/od) = Pedal1 | 80 | n_base | gesture (down >=64 / up <64, 127->0 timing preserved) |
| stop | 81 | n_base+1 | trigger |
| undo/redo | 82 | n_base+2 | trigger |
| clear | 83 | n_base+3 | trigger |
| rhythm start/stop | 84 | n_base+4 | toggle |
| reverse | 85 | - | toggle |
| memory next / prev | 86 / 87 | - | trigger |
| loop level | 7 | - | absolute (0..127 -> `boss_rc5_loop_level`) |
| EXP / target | 11 | - | absolute (`boss_rc5_exp`) |
| tempo (internal) | learn only | - | absolute 40-300 BPM linear |
| PC | - | - | memory select 0..98 -> 1..99 |
RC-1 (rc1 spec s6): CC64 = pedal switch down/up, CC65 = ext stop/undo, CC7 = level. CC80-87 are user-assignable on the real RC-5 (DOCUMENTED); the plugin exposes them as the RX map `boss_rc5_rx_map` (Ext). (`n_base` = local-profile note base, default 60, configurable.) Collisions are moot: profiles are local (sec 9 intro).
- Start/stop emulation: MIDI START (0xFA) when RC-5 slave: if `quantize=bar` waits for tick 0 of next bar. STOP: stops playback and rhythm. Continue: resume.
- Tempo sync note: loop length derived from tempo (bars) when "tempo sync" on (`boss_rc5_quantize`).
### 9.4 TC Ditto, EHX Nano Looper 360, BOSS DD-3T (no MIDI on hardware -> Virtual MIDI; CC ids = pedal spec section 6)
- **tc-ditto**: CC80 tap, CC81 double (stop), CC82 double+hold (erase), CC83 undo/redo, CC7 (or 85) loop level; `tc_ditto_midi_enable` default off (flag `virtual_ext`); `tc_ditto_quantize` / `tc_ditto_follow_transport` default off.
- **ehx-nano-360**: CC7 level, CC20 loop select (0..127 -> 11 steps, hysteresis 0.3 step), CC21 tap, CC22 double-tap, CC23 undo/redo, CC24 erase (only STOPPED), CC25 stop; PC 0..10 -> loop 1..11; memory switching while playing applies at loop end unless immediate (DOCUMENTED safety note in model file).
- **boss-dd3t**: CC14 time, CC15 feedback, CC16 e_level, CC17 mode (7 steps), CC85 on/off, CC86 tap, CC87 short-loop hold (>=64 held); tap subdivisions Q / D8 / QT (DOCUMENTED) plus sync_div extras; `boss_dd3t_sync` default off; when on, time = beat_ms*division clamped 12.5..800 ms (halving rule 3.5).
### 9.5 Time-based non-looper pedals (virtual sync only; hardware has none)
- boss-ce2/ce2w, mxr-phase90 (speed): `<id>_sync` default off; division allowed {1/1..1/16 D/T}; LFO phase lock on START. Rate range stays per pedal spec; synced rate clamped into [min,max] of knob rate with octave fold (rate*2 or /2 until in-range, ESTIMATED).
- Tremolo/delay generic pedals: sync default off but division list full.
### 9.6 Non-time pedals
Dist/OD/fuzz/comp have no clock behaviour; MIDI only for params + footswitch + exp (wired to e.g. gain/level when user maps).
### 9.7 Footswitch types summary (DOCUMENTED for the real units, our emulation)
Latching: DS-1, TS808, SD-1, D+, RAT, Muffs, Phase 90, CE-2/2W, Dyna Comp, DD-3T bypass. Looper units: gesture-based (momentary classification). Ditto/Nano 360/RC-5/RC-1: gesture decoder 5.3. Pedal spec may override `fs_type`.

## 10. Board-level params
`ref_board_midi_in_enable` bool def 1; `ref_board_midi_chan` 0-16 def 0; `ref_board_tempo_source` enum def 3; `ref_board_tempo_bpm` 20..300 def 120; `ref_board_midi_clock_out_enable` bool def 0; `ref_board_midi_out_chan` 1-16 def 1; `ref_board_pc_mode`; (`ref_board_direct_map_enable` RETIRED, sec 9); `ref_board_kbd_fs_base_note` 0..127 def 36; `ref_board_at_source`; `ref_board_mix` (CC unmapped def; target for universal wet/dry); `ref_board_fs_bounce` def 0; `ref_board_learn_active` (UI, non-persisted); `ref_board_selected_pedal` (UI); `ref_board_midi_panic` trigger.
Host-automatable APVTS: `ref_board_tempo_bpm`, `ref_board_mix`, `ref_board_enabled`, and the slot lanes `ref_s{nn}_*` of BOARD_CORE 9.1 (append-only). MIDI maps themselves are state, not host params.

## 11. State persistence
Add `midi` block in `referencePedals` state (stays in schema 1: nothing shipped before; BOARD_CORE 10.2. A missing `midi` block loads as empty):
```
midi:{schema:1, tempoSource, bpm, channel, bindings:[{id,instanceId|"board",paramId,chan,src:{type,num},mode,flags,in:[a,b],out:[a,b],curve:{id,p,pts?},smoothMs,priority}], scenes:[{n, pedals:[{instanceId,enabled,controls:{...}}]}], kbd:{baseNote,pins:{instanceId:note},atMode,atSource}, macros:[...], expr:[...] }
```
- Bindings address by `instanceId` + paramId (string), resolved to indices when building `MapSnap` on message thread; unresolved binding kept (greyed "missing target"), never dropped.
- Unknown fields preserved (round-trip tests). Unknown source types preserved.
- Learn state, soft-takeover cursors, clock FSM, tap history NOT persisted. Tempo bpm persisted.
- MIDI maps also savable as standalone `.rpmap` (JSON) files, shareable per pedal ("map template") with import conflict dialog (sec 8.3.5). Atomic write (temp+rename).
- Limits: <= 1024 bindings, <= 128 scenes. Exceeding: refuse add, UI message.
- Host `getStateInformation` must not block audio: serialise from message thread copy.

## 12. Real-time safety notes (checklist)
No `std::vector::push_back` on RT; no `std::function`; no `new`; no `std::string` in `MidiEv`; MapSnap lookup via flat table `uint16 first[3][16][128]` (type,chan,num) -> index list in `uint16 idx[]`; max 8 fan-out. Float math deterministic (no fast-math differences between platforms for LUT build); denormal guard; sample counters int64. Clock tick handler O(1). Learn ring bounded. Retire queue freed on message thread. Static-assert POD sizes. TSAN-clean required.

## 13. UI hooks (for UI spec author)
Per control: MIDI badge, LED activity, right-click menu; Board MIDI panel (inside drawer settings): tempo source selector + BPM + tap + clock lock LED, MIDI in/out device (standalone), channel, per-slot channel + "Use pedal CC profile" toggle (sec 9), mapping list table (sortable, filter by pedal), panic button. Keyboard-footswitch panel: shows note assigned per pedal + velocity meter + AT meter. Keyboard-only access to every item; screen-reader names "MIDI learn <pedal> <control>".

## 14. Tests / acceptance
Unit (deterministic, no real device):
- T-01 Clock: feed 0xF8 at 120 BPM (period 1 s/48 -> 20833.33 samples @48k... ie 24ppqn=48 ticks/s) -> bpm = 120.0 +-0.1 after <=24 ticks.
- T-02 Clock jitter: +-3 ms uniform jitter on ticks -> bpm std dev <= 0.2 BPM after lock; lock acquired <= 1 beat.
- T-03 Clock loss: stop sending -> `clockLost` within 500 ms +-1 block; tempo frozen; no NaN.
- T-04 Start/Stop/Continue FSM table-driven (all transitions incl. SPP, double START, CONTINUE before START).
- T-05 Host transport: mock PlayHead with bpm changes, loops, relocates -> START/STOP/LOCATE synthesised exactly once; ppq phase matches within 1 sample.
- T-06 Division math: all 16 divisions x tempos {20,60,120,300} -> ms/Hz exact (double), clamp+halve rules.
- T-07 LFO phase lock: START at ppq 0 and 4.0 -> LFO phase = 0 (±1e-6 cycles) at that sample.
- T-08 Tap tempo: sec 5.4; timeout; clamp; velocity compensation formula; subdivision.
- T-09 Looper quantize: gesture at grid-0.02 s -> waits to line; at grid+0.05 s with 80 ms window -> snaps back using pre-roll (loop contains exact same bar length); sample-exact start index.
- T-10 Gesture decoder: table of (down,up) sequences -> events (tap, double, hold@1000, erase@2000); boundary values +-1 sample; velocity-adjusted window; "no double-tap action in state => immediate tap".
- T-11 Learn FSM: arm/capture/cancel/timeouts/ambiguous (CC burst vs note), suppression of forwarded events, duplicate dialog logic (pure model test).
- T-12 Sample-accuracy: CC step at offset 100 in 512 block => param ramp begins at sample 100 (±0); split vs unsplit render difference <= 1 LSB (continuous); footswitch note at offset 37 => crossfade centre at 37+engage/2 ±1.
- T-13 Curves: LUT vs analytic max error <= 1e-4; monotonic; endpoints exact 0 and 1; PCHIP monotone.
- T-14 Range/invert/smoothing: step response reaches 63.2% at smoothMs ±5%; no overshoot; no zipper (max per-sample delta bound).
- T-15 Soft takeover: pickup crossing, direction cases, reset on preset change.
- T-16 Conflict: priority/hold/combine; UI vs MIDI vs automation orderings; duplicate; reorder keeps bindings.
- T-17 Latch/momentary/auto fs modes; note-off before engage complete; stuck notes released by CC123 and by transport/board disable.
- T-18 Debounce with bounce option: two note-ons 3 ms apart = 1 toggle.
- T-19 State: round-trip JSON, migration v1->v2, missing model/instance keeps bindings, unknown fields preserved, atomic file write failure leaves old file.
- T-20 Reserved CC guard; PC bank math (bank*128+pc); scene recall ramps, no clicks (max sample delta <= 0.01 FS beyond input change on silence + DC test).
- T-21 14-bit CC: MSB/LSB ordering permutations.
- T-22 PB/AT: PB 0/8192/16383 -> normalised bipolar/unipolar; poly AT max combine; channel AT broadcast.
Stress/RT:
- S-01 MIDI flood: 100k events/s random CC/AT/PB for 60 s plus 24ppqn at 300 BPM -> no xruns (CPU budget < 3% of one core @ 48k/64 block), dropped counter reported not crashing.
- S-02 Alloc detector (malloc hook / `-fsanitize=...`) zero allocations in `process` under all above tests.
- S-03 TSAN: concurrent UI map edits (publish snapshots at 1 kHz) while audio runs: no races, no use-after-free (epoch retire test).
- S-04 Block sizes 1,4,16,64,512,2048 + non-pow2 (e.g. 37, 333): same clock/transport results.
- S-05 Sample rates 44.1/48/88.2/96/192k identical musical timing (ms-accurate).
- S-06 30 min randomized gesture/CC/clock/transport fuzz (seeded); invariants: finite audio, looper never reads out-of-bounds, state machine legal, no stuck notes.
- S-07 Host reload during active learn / during scene recall / during clock lock: no crash, state sane.
Manual QA: DAW sync (Reaper/Ableton/Logic): tempo changes, loop region, relocate; MIDI keyboard w/ aftertouch (poly + channel); standalone device hot-plug; MIDI-learn every control type; "board off" path bit-identical (T-00 null test).

## 15. Implementation plan (order)
1. `MidiTypes`, `Curves`, `MapSnap` + tests T-13/T-14/T-19 (pure).
2. `ClockSync` (FSM, PLL, host, tap) + T-01..T-08.
3. `MidiRouter` + rings + sub-block splitting + T-12, S-01..S-04.
4. `MidiMap` + resolve + conflict rules + T-15/16/20-22.
5. `MidiLearn` + UI hooks + T-11.
6. `GestureEngine` + looper decoder + T-09/10/17/18.
7. `ExprEngine`.
8. Per-pedal default maps (`MidiDefaults.cpp`, data tables; tests assert no table collision in direct-map and every pedal param has a map or generic fallback).
9. State persistence + migration.
10. Soak S-05..S-07, performance budget entry in `spec/performance-budget.md`.

## 16. Open items / risks
- Exact RC-5 MIDI implementation (CC list, clock out behaviour) not verified; need manual MIDI chart (ESTIMATED here). DD-3T/Ditto/Nano 360 have no hardware MIDI (virtual extension only).
- Host MIDI out availability varies (VST3 yes via event list, AU/others limited); clock out may be unavailable.
- Standalone MIDI device selection uses JUCE `MidiInput` callback thread -> into `uiInRing`.
- Aftertouch/velocity-to-engage-time constants are perceptual guesses; tune with user testing.
- Sub-block splitting overhead vs. pedals with heavy oversampling: need `prepare` flag `supportsSplit`; those pedals get events quantised to block start with 1-block latency (documented).
