# ehx-nano-360: EHX Nano Looper 360 (inspired-by) dev spec

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `ehx-nano-360`, **paramPrefix `ehx_nano_360_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/ehx_nano_360.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/ehx_nano_360_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_ehx_nano_360Tests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): (none).
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims (none) -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `ehx_nano_360_char_on`, `ehx_nano_360_conv_lat_ms`, `ehx_nano_360_mem_mode`, `ehx_nano_360_store24`, `ehx_nano_360_ext_stereo`.  Momentary/Trigger (gesture, not lane-mapped): `ehx_nano_360_footswitch`.  PF_Ext (virtual extension, state + MIDI-learn only): `ehx_nano_360_sync_mode`, `ehx_nano_360_quant`, `ehx_nano_360_midi_note`, `ehx_nano_360_midi_at_level`, `ehx_nano_360_vel_pressure_on`, `ehx_nano_360_expr_target`, `ehx_nano_360_bounce_src`.
> - Host lanes `ref_s{nn}_p01..` in this order: `ehx_nano_360_level`, `ehx_nano_360_loop`, `ehx_nano_360_seam_xfade_on`, `ehx_nano_360_switch_mode`, `ehx_nano_360_tap_on_press`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Ver 1.0 | 2026-10-21 | model_id `ehx-nano-360`, param prefix `ehx_nano_360_` | Sibling: ../BOARD_CORE.md, ../MIDI_EXPRESSION.md, ../../models/ehx-nano-360.md, `Source/ReferencePedals/PhraseLooper.h` (prototype, to be superseded).
Tags: [DOC]=manufacturer docs/retail listings, [MEAS]=bench measurement (none available), [EST]=design choice/estimate.
Name use: descriptive "inspired by Electro-Harmonix Nano Looper 360". No manual text/photos copied. Visuals = original vector art.

Research status (re-run 2026-10-22): WebFetch is DNS-blocked for every host tried (ehx.com, bhphotovideo, fullcompass, manualmachine, synthmanuals, looperpedalreviews; web.archive.org refused), so only WebSearch snippets of the manual copies were reachable; upgrades in sec 11 come from those. Earlier status: WebFetch of ehx.com / effectsdatabase blocked (ENOTFOUND). Facts below from WebSearch snippets of the official manual (ehx.com/wp-content/uploads/2021/01/nano-looper-360-manual.pdf) + retailer listings (gearank, boullard.ch, Adorama, Andertons, manualslib). No schematic/teardown/measurement found. Items not in snippets = [EST], listed in sec 10.

## 1 Identity / variants
- Real unit: compact (nano) mono phrase looper, 11 independent loop memories, 360 s (6 min) total, 24-bit uncompressed, 44.1 kHz [DOC]. Memories persist until erased [DOC] (non-volatile, survives power-off; "remain in memory until you erase them").
- Variants: Nano Looper 360 (this), Pico 360 Looper (smaller sibling, announced; same engine, not modelled), 360 Nano Looper = same product. No MIDI, no stereo, no expression jack [DOC: only 1 in + 1 out listed].
- Retail listing conflict: one says "mono", one says "stereo recording time" [DOC conflict]. Resolve: jacks mono (re-confirmed: audio described mono, 24-bit A/D/A, 44.1 kHz, 1 in / 1 out) (manual "1/4 mono input and output jacks") -> model mono core.
- Plugin deviation (declared): plugin runs host rate; memory stored as float32 at host rate; capacity expressed in SECONDS (360 s) not samples. Optional stereo-in/out in plugin (sec 5) = extension, flagged `ext`.

## 2 Controls
| # | Control | Real behaviour | Range / taper | Default |
|---|---|---|---|---|
| 1 | LEVEL knob | Loop playback output volume only. Dry signal stays analog unity from in to out, never affected [DOC] | 0..1 pot. Taper [EST] audio: formula in sec 8 (x=0 mute, 0.5 -> -15.2 dB, 0.75 -> -3.1 dB, 1 -> 0 dB unity) | 0.75 |
| 2 | LOOP knob | 11-position selector, numbered 1-11, selects memory [DOC]. Rotary (likely continuous-rotation detented switch; [EST] detented 11 positions) | int 1..11 | 1 |
| 3 | Footswitch | One momentary switch, all functions by gesture (sec 3.2) | momentary | n/a |
| 4 | REC LED | on while recording/overdubbing | - | - |
| 5 | PLAY LED | on while playing | - | - |
| 6 | MEM LED | off = selected loop empty; on = loop has audio; flashes fast while LOOP knob scrolls [DOC] | - | - |
| 7 | IN jack | 1/4" mono, Zin 2.2 MOhm [DOC] | - | - |
| 8 | OUT jack | 1/4" mono, Zout 470 Ohm [DOC] | - | - |
| 9 | DC jack | 9.6 VDC 200 mA supplied; draws 90 mA [DOC ehx.com listing]; center-negative [DOC earlier run, not re-confirmed]. BATTERY CONFLICT RESOLVED: the manual contains a 'Changing the Battery' section (bottom plate, screws; LEDs fail to light on a dead battery) so a battery is supported [DOC]; earlier 'does not take batteries' claim is withdrawn. Battery/power-loss behaviour: OMIT (plugin has no power model), except: loop being recorded at power loss is not saved [DOC] | - | - |
LED colours not in sources [EST]: REC red, PLAY green, MEM amber (user-selectable skin; no behaviour impact).
No other knobs, switches, jacks. Do not add physical controls to face. Plugin-only extras live in an Advanced overlay (sec 5/6), not on the face.

## 3 Signal path and behaviour model
### 3.1 Path (stage by stage)
```
IN -> input buffer (Zin 2.2M) -+--------------------------------+--> sum -> OUT buf (Zout 470R)
                               |  [dry: analog unity, always]   ^
                               +--> ADC 24b/44.1k -> loop RAM --+-> LEVEL -> DAC
```
- Dry: always passes, analog, unity [DOC]. Hence "bypass" is not a state: looper is effectively always in circuit; stopped = no loop signal. Bypass type: buffered/always-on analog dry path [DOC-derived]; no click possible on dry. Plugin: dry gain exactly 1.0 (bit-exact when loop silent; test T1).
- Converter: 24-bit @ 44.1 kHz [DOC]. Model by resampling to 44.1k conceptually optional (sec 4.5); default OFF: store at host rate. Converter filter response, noise floor, THD: unknown [MEAS needed]. Placeholder: ADC/DAC lowpass 20 kHz 4th-order ~ flat to 18k [EST], noise floor -98 dBFS A-wtd [EST] (toggle `ehx_nano_360_char_on`, default off).
- Latency: record/playback aligned to loop; assume zero added latency to dry. Loop-to-dry alignment: playback sample n aligned to recorded sample n (no converter delay modelled; real unit ADC+DAC ~1-2 ms [EST] ignored; optional `ehx_nano_360_conv_lat_ms` 0..3 default 0).
- Storage: 360 s total shared across 11 memories [DOC]. Allocation: dynamic, not fixed 32.7 s each [EST; manual says "11 independent loops, 6 min total"]. Rule: sum(len_i) <= 360 s. Once the 360 s is used up, NEW recording is blocked until a loop is erased [DOC]. Behaviour when budget runs out MID-take is not documented: plugin auto-transitions to PLAY at exhaustion [EST]. Recording start with 0 s remaining: ignored, LEDs unchanged [EST]. Single loop max = 360 s minus others.
- Loop overdub: unlimited layers, length fixed by first take [DOC "unlimited overdub"]. Overdub sums new input onto existing material (no feedback decay; 100% sustain) [EST; EHX describes layers, 1.0 feedback].

### 3.2 Footswitch gesture decoder (state machine). Undo is only accepted in PLAYING (not RECORDING/OVERDUBBING/STOPPED) [DOC]; erase only when STOPPED and MEM lit [DOC]
Gestures [DOC]: tap1 = record start/stop/overdub toggle; double-tap = stop; hold 1 s DURING PLAYBACK = undo/redo, PLAY LED blinks twice [DOC manualslib/ehx PDF via search]; undo is NOT available during record, overdub or stop [DOC]; stop = two quick presses, both REC and PLAY LEDs go dark on the 2nd press and playback stops immediately [DOC]; hold 2 s while stopped = erase, MEM LED blinks six times then stays off, only enabled when MEM LED is lit [DOC].
Timing constants [EST unless noted]:
```
kDebounceMs      = 15   // switch bounce reject
kDoubleTapMs     = 300  // 2nd press within window of 1st press-down -> double tap  ("two quick presses")
kHoldUndoMs      = 1000 // [DOC "about a second"]
kHoldEraseMs     = 2000 // [DOC "two seconds"]
kEraseWindowMs   = 3000 // after erase threshold reached feedback LED flash 
```
Decision: on press-down start timer. Actions are issued on press-DOWN for tap (immediate record/play response, like real unit) EXCEPT the double-tap resolution: first tap acts immediately (e.g. starts overdub), second tap within kDoubleTapMs cancels that action (undo it) and issues STOP. Real unit exhibits the same brief overdub-then-stop glitch; to model, rewind overdub write of the last kDoubleTapMs (see 3.3 `abortOverdubTail`). [EST]
Hold: if still down at kHoldUndoMs and state in {playing, overdubbing->just ended, i.e. has undo layer} -> UNDO/REDO toggle; hold ignored on release. If state==stopped and still down at kHoldEraseMs -> ERASE selected loop. If state==stopped and held 1..2 s: nothing (no undo while stopped [EST]). Tap issued by the initial press-down in hold case is retracted (state restored) when hold threshold passes [EST], so a hold during PLAY does not leave a stray overdub: on press-down while playing -> enter overdub tentatively; at 1 s -> revert overdub (discard layer written so far) then apply undo of previous layer. Simplification (acceptable): decode on release for gestures when hold>kHoldUndoMs; tap-on-press only for first press; config `ehx_nano_360_tap_on_press` (bool, default true).

State machine (per selected memory m; global current gesture state):
```
EMPTY(m)        --tap--> RECORDING
RECORDING       --tap--> PLAYING (len=head; wrap to 0)      ; also on budget exhaustion
RECORDING       --dtap--> STOPPED? NO: dtap during rec = stop & keep take, len=head [EST]
PLAYING         --tap--> OVERDUBBING
OVERDUBBING     --tap--> PLAYING   (commit layer -> undoStack push)
PLAYING|OVERDUB --dtap--> STOPPED  (head=0)
PLAYING         --hold1s--> UNDO (if undoDepth>0) toggles: UNDO<->REDO, stays PLAYING [DOC]
STOPPED         --tap--> PLAYING from head 0
STOPPED         --hold2s--> ERASED -> EMPTY(m)
any             --LOOP knob change--> see 3.4
```
Undo model [DOC: hold 1 s in playback removes the last overdub; hold again restores it; single-level]: one undo layer (last overdub). "Undo" subtracts the last layer; "redo" restores it. Starting new overdub discards redo layer. Depth 1 [EST; docs say "remove the last take", "hold again restores it"]. Implementation: layer buffer = last overdub delta stored separately (`layerBuf`), main buffer = base+older layers, playback = main + (undone ? 0 : layer) *or* the delta is added into main and subtracted on undo. Choose separate `layerBuf` (RAM +1x of loop length, not counted against 360 s) so undo is exact and fade-free; playback sum on the fly. Commit layer into main on next overdub start or loop change.

### 3.3 Overdub and loop boundaries
- Playback head wraps at len. Record write head = playback head. Overdub is sample-aligned.
- Rec start: no pre-roll; count starts at press instant (block-split, sample accurate). Rec end: quantised to press instant; loop seam crossfade `kSeamXfadeMs = 2.5` ms cos^2 over last samples blended with first samples [EST] (real unit seam: unknown, probably hard cut; `ehx_nano_360_seam_xfade_on` default true to avoid clicks; set false for hard cut).
- `abortOverdubTail`: on dtap resolution, zero layerBuf over [headAtFirstTap, now] wrapped.
- Overdub input gain: unity. Output tap: loop playback * LEVEL; during overdub, play heard = base+layer-in-progress? Real: input monitored via dry; loop plays previous content (not new layer until next pass). Model: during overdub, playback reads main (and previous layers) only; new layer plays from next pass [EST].

### 3.4 Loop selection while playing
Knob change while PLAYING: finishes current loop pass? Unknown [not in snippets]. Constraint from existing notes: "Preserve memory switching safety while playing". Model [EST]: switch takes effect at next loop wrap of the OLD loop (quantised to boundary), pending target shown by MEM LED flashing fast; if old loop STOPPED/EMPTY switch immediate. New loop state: if it has audio -> starts PLAYING from head 0 if old was playing (continue-play behaviour) else STOPPED [EST; config `ehx_nano_360_switch_mode`: 0=stop on switch, 1=boundary play (default)]. If old loop was RECORDING/OVERDUBBING: knob is ignored until tap ends (lockout) [EST] OR forces commit (default: lockout; MEM LED flashes 3x). Crossfade 8 ms at switch to avoid clicks. No audio lost: switching never writes/erases memory.
Knob scroll flashing: MEM LED flashes (fast 8 Hz) for 250 ms after each position change [DOC "flashes quickly during transitions"]; then steady (on if loop has data, off if empty) [DOC].

### 3.5 LED logic
```
REC  on  : RECORDING, OVERDUBBING            (else off)
PLAY on  : PLAYING, OVERDUBBING              (else off) ; blink 2Hz when STOPPED & loop has data? [EST no; off]
MEM  on  : loop[m].len>0 ; off: empty ; 8Hz flash 250ms after knob move
Erase confirm: MEM LED blinks 6 times then stays off [DOC]; blink rate [EST] 6 Hz
Undo confirm: PLAY LED blinks twice [DOC]; redo confirm blink pattern not documented -> same double blink [EST]
```

## 4 Algorithm for plugin
### 4.1 Block diagram
```
in(L,R) -> monoSum? (mono mode: x=(L+R)/2 [if stereo host], else L) -> inGain(unity)
 gesture queue (lock-free SPSC, sample-stamped) -> StateMachine (audio thread)
 loopRAM[11] (preallocated pool, 360 s*maxSR floats) -> read(head) -> sum layerBuf -> *levelGain(smoothed) -> out
 out = dry + loopPlay   (dry untouched)
```
Per-sample (audio thread):
```
x = in[n]
p = (state in {PLAY,OVERDUB,pendingStopFade}) ? main[m][h] + (undone?0:layer[h]) : 0
if state==RECORDING: main[m][h]=x; h++; len=h; if h>=cap -> PLAYING,h=0
if state==OVERDUBBING: layer[h] += x           // write layer; cap clip +-8.0
if playing/overdubbing: h++; if h>=len: h=0
y = x + g * p        // g = levelGain smoothed
```
- No feedback: no loop decay. Output clip: soft-limit y at +-16 (hard clamp, as prototype) NOT modelling analog headroom. Converter clip at 0 dBFS option `ehx_nano_360_char_on`: hard clip loop signal at +-1.0 before storage [EST].
- Oversampling: none (linear mix, no nonlinear stage). Aliasing: none. Rate conversion if `ehx_nano_360_char_on` 44.1k emulation: polyphase sinc 64-tap, 2 ms latency; default off.
- Smoothing: LEVEL one-pole 20 ms (sample-rate-independent coeff = 1-exp(-1/(0.02*sr))). State transitions: 2 ms linear fade on loop playback start/stop (no clicks), 8 ms on loop switch. Dry never faded.
- Denormals: loop data float32; flush |v|<1e-20 to 0 on write; `juce::ScopedNoDenormals` on callback. Level smoother snaps when |err|<1e-7.
- Memory: pool allocated in prepare(): capacityFrames = ceil(360*sr) floats + layer buffer same size (max single loop) = 2*360*48000*4B = 138 MB @48k mono. Reduce: layer buffer lazily sized in prepare to min(360s, userMaxLoop). Param `ehx_nano_360_mem_mode`: full (360 s) | lite (60 s) default full for desktop; 192 kHz -> cap 360 s*192k*4*2 = 553 MB: reject; limit to 96k-equiv by decimating storage (store at min(sr,96k) with linear-interp resample, flag). Allocation only on message/prepare thread, via `std::vector::assign` (never audio thread). Per-loop slice allocation within pool by bump allocator at record start from a pre-sized free list (no malloc).
- CPU budget: <0.1% of one core @48k/512 for idle/play (one read+add+mul/sample); record/overdub +1 write. Target `process` < 0.5 us/sample worst case ... [EST]. No locks; gesture queue SPSC 64 entries.
### 4.2 Persistence (off audio thread)
- Loop audio saved to sidecar binary `<plugin-state-dir>/ehx_nano_360_loops.bin`: header magic `EH36`, ver 1, sampleRate, per-loop {len, crc32}, then float32 PCM (optionally 24-bit PCM to mirror 24-bit [EST] `ehx_nano_360_store24`). Saved by background `juce::Thread` on loop commit (end of rec/overdub, erase) and on state save; host state JSON stores only `{loopLens[11], selected, filePath, sr, crc}` (not audio). Load async on prepare; loops unavailable until loaded (MEM LED off, taps ignored with LED flash). If sr differs: resample on load off-thread (source rate migration test). Missing/corrupt file -> loop EMPTY, no crash.
- Atomic file write: temp + rename. Max 360 s*44.1k*4 B = 63.5 MB float32 mono.
- Undo layer not persisted [EST] (cleared on load).

## 5 I/O and routing
- Real: 1 mono in, 1 mono out, no loop/aux jacks [DOC]. Plugin face: IN, OUT, DC (cosmetic).
- Board integration (BOARD_CORE modes): treated as pedal with `latency=0`; dry path internal (y = x + loop). Position matters: looper after drive/mod records processed sound (usual); before = records clean. Series/parallel/A-B all supported by board; looper in parallel lane: lane output contains dry+loop (board dry-comp handles).
- Wet/dry (board universal): looper `ehx_nano_360_mix_internal` not present (real dry is always unity). Board wet/dry applies on top.
- Feedback-loop tricks (user): (a) place looper in board FX-LOOP: send/return around reverb/delay so loop re-records tails (resampling: record OUT of chain including looper's own earlier pass) — board feedback loop (BOARD_CORE sec 6, safe limiter) lets output of later pedals return to looper input: result = loop + effect re-recording ("re-amp"/resample trick); looper itself has no feedback param so layering decay is only via downstream effect. Safety: board limiter, loopback gain <=0.98 default. (b) Chain two looper instances (different banks) -> 22 memories / sync copies by manual bounce. (c) Stereo split: extension `ehx_nano_360_ext_stereo` (bool, default false): L and R independent mono instances sharing the gesture state machine (two buffers, each 360 s/2? No: each 360 s, memory 2x). Flag `ext`, off by default. (d) "Resample" bounce: record loop 1 while playing loop 2 through downstream effects (switch lockout allows record only on selected loop; MIDI/Advanced `ehx_nano_360_bounce_src` (select 0 none, 1 other-loop) [EST, ext]).
- No expression, aux, or MIDI jacks on real unit. Plugin exposes control surface (sec 6) as host-automation/MIDI extension only (labelled `ext`).

## 6 MIDI / sync / expression emulation
Fidelity statement: real unit has NO MIDI, NO tempo sync, NO expression input [DOC]. Everything here is `ext` (convenience), default mapping off except footswitch note learn. Comparable-fidelity rule: footswitch gestures must still decode through the same state machine from physical timing; MIDI trigger just synthesizes press-down/up events with real timing.
- Footswitch via MIDI note (default learn off; mapping `ehx_nano_360_midi_note` int -1..127, -1 off; default 60 when enabled): note-on = press-down, note-off = press-up. Duration of hold = real hold (gestures hold 1 s/2 s work). Velocity ignored [EST] except optional: `ehx_nano_360_vel_pressure_on`: note-on velocity <32 = "soft tap" (rec toggle only), >=100 = adds nothing. Default off.
- Aftertouch (channel/poly): not mapped by default; optional target LEVEL (`ehx_nano_360_midi_at_level` bool default false): LEVEL_eff = LEVEL * (0.5+0.5*at/127)... [EST ext].
- CC map (ext, channel omni default): CC7 -> LEVEL (0..127 -> 0..1 via same taper), CC20 -> LOOP (0..10 -> 1..11, value/ (128/11)), CC21 value>=64 edge -> tap (rising), CC22 -> double-tap (rising), CC23 -> undo/redo (rising, equivalent to 1 s hold), CC24 -> erase (rising; only effective when STOPPED, else ignored), CC25 -> stop (rising). Program Change 0..10 -> LOOP 1..11. Mapping table user-editable via MIDI-learn (MIDI_EXPRESSION.md). Event applied at sample offset of the MIDI event.
- Tempo/DAW sync (ext, `ehx_nano_360_sync_mode`: 0 off default, 1 quantise start, 2 quantise length): real unit has none. Mode 1: tap-to-record start deferred to next beat/bar (`ehx_nano_360_quant` select: off, 1/4, 1 bar, 2 bar, 4 bar; default off) using host PPQ / MIDI clock (24 ppqn) via board clock; mode 2: end-of-record rounds to nearest bar multiple (pad/trim zero). Latency of quantised start up to one grid unit; LED REC blinks 4 Hz while armed. Clock loss: falls back to unsynced.
- Expression pedal: none on real. Plugin ext: target selectable (`ehx_nano_360_expr_target`: off, level default); expression 0..1 -> LEVEL absolute.
- Mappable controllers: sliders/knobs map to LEVEL and LOOP via standard board MIDI-learn; LOOP maps as 11 steps with hysteresis 0.3 step.

## 7 Visuals (original vector art; no EHX trade dress copy)
- Enclosure: nano-size, aluminium die-cast look [EST], colour original palette: matte steel-blue `#3b5a73` with off-white silkscreen `#ecece4` (not matching EHX colours exactly). Size real: 2.75x4.5x2.1 in = 70x115x54 mm [DOC ehx.com / retailer listing]; use w=70, h=115 mm face.
- Layout (face, origin top-left, mm) [EST layout]:
  - Jacks at top edge: IN at x=14 (left), OUT at x=56 (right), DC at x=35 center, y=0 (edge icons).
  - LEVEL knob: centre (21, 26), dia 16 mm, 270 deg sweep, pointer line, tick labels 0, 5, 10 (or 0-10 arc).
  - LOOP knob: centre (49, 26), dia 16 mm, 11 detent ticks numbered 1..11 around 300 deg; a printed "1..11" ring.
  - LEDs: REC (14, 47), PLAY (35, 47), MEM (56, 47), dia 3.5 mm, with tiny labels REC, PLAY, MEM below at y=52.
  - Footswitch: centre (35, 84), dia 22 mm chrome-look, 3 concentric rings.
  - Text: "NANO LOOPER 360" (generic: "LOOPER 360") at (35, 63), "LEVEL","LOOP" above knobs, "inspired-by" note in credits only.
- Knob art: round cap 16 mm, knurl lines 24, shadow offset 1 mm.
- Animation: LED glow 6 mm radial alpha; MEM flash 8 Hz; knob drag snaps to 11 positions with 120 ms ease.
- Accessibility: LED state also text in tooltip (EMPTY, REC, PLAY, OVERDUB, STOP).

## 8 Parameter table
| id | name | min | max | default | unit | skew | smooth ms | automatable |
|---|---|---|---|---|---|---|---|---|
| ehx_nano_360_level | Level | 0 | 1 | 0.75 | norm | audio taper, see formula below | 20 | yes |
| ehx_nano_360_loop | Loop | 1 | 11 | 1 | int | linear step | 0 (event) | yes (stepped) |
| ehx_nano_360_footswitch | Footswitch | 0 | 1 | 0 | bool momentary | - | 0 | yes (maps press/release) |
| ehx_nano_360_char_on | Converter character | 0 | 1 | 0 | bool | - | 0 | yes |
| ehx_nano_360_conv_lat_ms | Converter latency | 0 | 3 | 0 | ms | linear | 0 | no |
| ehx_nano_360_seam_xfade_on | Seam crossfade | 0 | 1 | 1 | bool | - | 0 | no |
| ehx_nano_360_switch_mode | Switch mode | 0 | 1 | 1 | enum | - | 0 | no |
| ehx_nano_360_tap_on_press | Tap on press | 0 | 1 | 1 | bool | - | 0 | no |
| ehx_nano_360_mem_mode | Memory mode | 0 | 1 | 0 | enum 0=360s,1=60s | - | 0 | no |
| ehx_nano_360_store24 | Store 24-bit | 0 | 1 | 0 | bool | - | 0 | no |
| ehx_nano_360_sync_mode | Sync mode | 0 | 2 | 0 | enum | - | 0 | yes |
| ehx_nano_360_quant | Quantise grid | 0 | 4 | 0 | enum | - | 0 | yes |
| ehx_nano_360_midi_note | MIDI footswitch note | -1 | 127 | -1 | int | - | 0 | no |
| ehx_nano_360_midi_at_level | Aftertouch->Level | 0 | 1 | 0 | bool | - | 0 | no |
| ehx_nano_360_vel_pressure_on | Velocity soft tap | 0 | 1 | 0 | bool | - | 0 | no |
| ehx_nano_360_expr_target | Expression target | 0 | 1 | 0 | enum | - | 0 | no |
| ehx_nano_360_ext_stereo | Stereo extension | 0 | 1 | 0 | bool | - | 0 | no |
| ehx_nano_360_bounce_src | Bounce source | 0 | 1 | 0 | enum | - | 0 | no |
Level taper (exact, replaces the garbled skew cell): `gain_dB(x) = x<=0.001 ? -inf : -40*(1-x)^1.4`, giving x=0.5 -> -15.2 dB, x=0.75 -> -3.1 dB, x=1 -> 0 dB [EST]. Linear gain = 10^(dB/20). Hard mute below x=0.001.
Read-only (not params; state/visual): loop states, lengths, undo flag via atomic snapshot.

## 9 Tests / acceptance
T1 Dry transparency: loop empty -> out == in bit-exact (null -infinity dB), all block sizes 1/4/16/64/512.
T2 Record/play exactness: record 5 s impulse train, play back: out == in delayed by len multiples, null < -140 dBFS (float32 exact), after wrap.
T3 Budget: record in loops until sum=360 s: auto-stop, no overrun, no alloc (alloc-trap hook).
T4 Overdub: layer sum null vs reference sum < -120 dBFS; length unchanged; seam crossfade click metric < -60 dBFS.
T5 Undo/redo: after overdub hold 1000 +- 20 ms -> loop equals pre-overdub (null < -140 dB); hold again -> restored; new overdub discards redo.
T6 Gesture timing: tap<300ms double-tap resolves STOP; 15 ms bounce ignored; 1 s / 2 s thresholds +-5% jitter at block sizes 1..512 and sr 44.1/48/96k.
T7 Erase: only when STOPPED, hold 2 s; loops 2-11 untouched (isolation test: 11 loops hash check).
T8 Loop switch: 10k random switches with playing/rec states: no audio click > -60 dBFS, no memory change, no crash; recording lockout holds.
T9 Level: taper checkpoints -15.2 / -3.1 / 0 dB +-0.1 dB, 20 ms smoothing no zipper (spectral).
T10 Persistence: save/restore, hash identical; sample-rate migration 44.1 -> 48 -> 96 length error < 1 sample/min; corrupt file safe.
T11 MIDI: CC/PC/note mapping sample-accurate (+-1 sample) ; note hold gesture works.
T12 Sync: quantised start within +-1 sample of grid; clock loss fallback.
T13 30-min randomized soak (gestures, switches, saves), ASan/UBSan/TSan clean, no xrun; CPU <0.1%.
T14 Denormal: silence after long loop: no CPU spike >2x.
Frequency response: char_off flat 0 dB +-0.001 (digital); char_on target 20 Hz-18 kHz +-0.5 dB [EST, until MEAS]. THD: char_off < -140 dB, char_on < -85 dB [EST]. No claim of hardware match: fidelity label "behavioural (documented)".

## 10 Known inaccuracies / unknowns
(Superseded where sec 11 says so.)
- Converter filters, noise, THD, true latency: unmeasured [MEAS]. 
- LED colours; exact knob detent type; footswitch timing constants (double-tap window, debounce) [EST]; manual only says "two quick presses", "about a second", "two seconds".
- Behaviour of loop switch while playing; record-time exhaustion; behaviour of dtap during rec; undo depth (assumed 1): [EST].
- Whether loop storage is dynamic or 32.7 s fixed per slot (assumed dynamic).
- Power draw 90 mA @ 9V [DOC]; 9.6 V adapter; battery conflict ignored.
- No MIDI/sync/expression on real hardware; all of sec 6 is extension.
- Plugin memory at host rate, float32 not 24-bit/44.1k; stereo ext not hardware.
- Unreachable sources this run: ehx.com, effectsdatabase (network). Re-verify against manual PDF https://www.ehx.com/wp-content/uploads/2021/01/nano-looper-360-manual.pdf, https://www.manualslib.com/manual/891780/Electro-Harmonix-Nano-Looper-360.html, https://www.fullcompass.com/common/files/22677-ElectroHarmonixNanoLooper360UserManual.pdf when network permits.

## 11 Verification log
Tools: WebSearch only (WebFetch blocked on all hosts, archive.org refused). No schematic, teardown, ElectroSmash page, MEAS data, or MIDI chart exists/found; hardware has no MIDI. "Via search" = text returned in search snippets of the cited page.
Sources: M1 https://www.ehx.com/wp-content/uploads/2021/01/nano-looper-360-manual.pdf ; M2 https://www.manualslib.com/manual/891780/Electro-Harmonix-Nano-Looper-360.html?page=3 ; M3 https://manualmachine.com/electroharmonix/nanolooper360/6496488-instructions ; M4 https://www.fullcompass.com/common/files/22677-ElectroHarmonixNanoLooper360UserManual.pdf ; L1 https://www.ehx.com/products/nano-looper-360 ; L2 https://www.andertons.co.uk/guitar-dept/guitar-pedals/looper-pedals/electro-harmonix-360-nano-looper-pedal

| # | Item | Old -> New | Status | Source |
|---|---|---|---|---|
| 1 | Undo confirm LED | REC double-flash -> PLAY LED blinks twice | DOCUMENTED | M1/M3 via search |
| 2 | Undo availability | stopped/playing/overdub ambiguous -> playback only; not in record/overdub/stop | DOCUMENTED | M1 via search |
| 3 | Erase confirm | all 3 LEDs flash 3x -> MEM LED blinks 6x then off | DOCUMENTED | M3 |
| 4 | Erase precondition | stopped -> stopped AND MEM lit (loop has data), 2 s hold | DOCUMENTED | M3 |
| 5 | Stop gesture | double-tap -> two quick presses; REC+PLAY LEDs both off on 2nd press, immediate stop | DOCUMENTED | M3 |
| 6 | Memory exhausted | auto-stop assumed -> new recording blocked until a loop erased | DOCUMENTED (blocking); mid-take auto-stop kept | M1 via search; ESTIMATED part: mid-take behaviour |
| 7 | Battery | "no battery" -> battery supported (manual has Changing the Battery section) | DOCUMENTED | M4 via search |
| 8 | Power-loss during record | unspecified -> in-progress loop not saved; other loops persist | DOCUMENTED | M1 via search |
| 9 | Factory restore (new) | absent -> unplug, LOOP=11, replug holding footswitch, 3 LEDs blink, ~4 s, erases ALL loops, lands on loop 11. Plugin: `reset` action in state UI, not a param | DOCUMENTED | M1 via search |
| 10 | Dimensions | 70x114x53 -> 70x115x54 mm | DOCUMENTED | L1/L2 |
| 11 | Power draw 90 mA, 9.6 VDC 200 mA | unchanged | DOCUMENTED | L1 |
| 12 | 24-bit/44.1k, 360 s/11 loops, mono | unchanged | DOCUMENTED | L1/L2 |
| 13 | Zin 2.2 MOhm, Zout 470 Ohm | unchanged | DOCUMENTED (earlier-run snippet; not re-confirmed this run) | M1 earlier run |
| 14 | MEM LED flashes fast while scrolling loops; steady on/off by content | unchanged | DOCUMENTED | M1 |
| 15 | Debounce 15 ms, double-tap window 300 ms | unchanged | ESTIMATED (manual: "two quick presses"; typical looper values) | derivation: common footswitch practice |
| 16 | Hold thresholds 1 s / 2 s | unchanged | DOCUMENTED nominal (manual 1 s, 2 s); +-5% tolerance ESTIMATED | M1/M3 |
| 17 | Loop switch while playing (boundary quantised, lockout while rec) | unchanged | ESTIMATED; manual excerpts silent | none found |
| 18 | LED colours (REC red, PLAY green, MEM amber) | unchanged | ESTIMATED | none found |
| 19 | Undo depth 1, overdub 100% sustain, separate layer | unchanged | ESTIMATED (consistent with doc) | M1 |
| 20 | Converter filter 20 kHz, noise -98 dBFS, THD, latency 1-2 ms | unchanged, char_on default off | ESTIMATED (no measurements found) | typical 24-bit codec datasheets |
| 21 | LEVEL taper -40*(1-x)^1.4 dB | unchanged | ESTIMATED (audio taper, no pot value found) | derivation sec 8 |
| 22 | Seam crossfade 2.5 ms | unchanged | ESTIMATED (hardware seam unknown; plugin default on) | design choice |
| 23 | Dynamic vs fixed 32.7 s slots | dynamic -> dynamic ("360 s shared across 11 loops") | DOCUMENTED shared pool | M1 via search |
| 24 | Pico 360 variant | "same engine, not modelled" | OMIT (unverified; no data) | - |
| 25 | Retail "stereo recording time" listing conflict | resolved mono; stereo listing treated as retailer typo | DOCUMENTED (mono jacks) | L2 |
| 26 | MIDI note/CC/PC/AT/expression/tempo sync (sec 6) | extensions | OMIT as hardware features (hardware has none); kept only as flagged `ext` plugin conveniences | L1 |
| 27 | Battery-life LED indication / dead-battery behaviour | n/a | OMIT | M4 |
| 28 | Variants conflicts: "Nano Looper 360" = "360 Nano Looper" = "Nano 360" | consolidated | DOCUMENTED naming | L2 |
| 29 | Compat: rows with garbled skew cell | already fixed by exact taper formula; param table checked: no duplicate ids | fixed | - |
