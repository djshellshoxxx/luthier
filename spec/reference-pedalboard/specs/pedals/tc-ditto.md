# tc-ditto -- TC Electronic Ditto Looper (original mono) -- dev spec

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `tc-ditto`, **paramPrefix `tc_ditto_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/tc_ditto.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/tc_ditto_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_tc_dittoTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): (none).
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims (none) -> `ref_s{nn}_in_db/out_db`; footswitch-mode params (none) -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params `tc_ditto_midi_enable` -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `tc_ditto_conv_latency_samples`.  Momentary/Trigger (gesture, not lane-mapped): `tc_ditto_fs`.  PF_Ext (virtual extension, state + MIDI-learn only): `tc_ditto_undo_depth`, `tc_ditto_stop_at_end`, `tc_ditto_quantize`, `tc_ditto_follow_transport`, `tc_ditto_send_return_mode`.
> - Host lanes `ref_s{nn}_p01..` in this order: `tc_ditto_loop_level`, `tc_ditto_dry_level`, `tc_ditto_undo_hold_ms`, `tc_ditto_rec_to_overdub`, `tc_ditto_seam_xfade_ms`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

Version 0.1 | 2026-10-21 | Status: SPEC (not implemented) | model_id `tc-ditto` | param prefix `tc_ditto_`
Tags: DOCUMENTED = retailer/manufacturer listing or manual TOC seen in search; ESTIMATED = our choice / recall, unverified; MEASURED = none (no unit captured).
Research limits: egress proxy blocked manual PDF hosts (r2.gear4music.com, manualslib.tech, effectsdatabase.com -> 403/ENOTFOUND). Only search snippets verified. Every gesture timing below not in snippets = ESTIMATED; re-verify against manual before "faithful" claim.
Sources:
- S1 https://www.thomann.de/be/tc_electronic_ditto.htm (5 min, unlimited overdub, undo/redo, 24-bit, true bypass, analog dry-through)
- S2 https://reverb.com/mt/p/tc-electronic-ditto-looper (single LOOP LEVEL knob; double-click stop; double-click+hold erase; hold = undo/redo)
- S3 https://www.bonedo.de/artikel/tc-electronic-ditto-test (24-bit uncompressed; hold ~1.5 s for undo/redo per DE retailer note)
- S4 https://r2.gear4music.com/media/6/63009/download_63009_1.pdf (manual; TOC only: sections Undo, Redo, Loop level; text not retrieved)
- S5 https://loopersdelight.com/LDarchive/201303/msg00116.html (user report: double-press fast + long press in stop mode)
- S6 https://www.tcelectronic.com/en/products/0709-AFB (product page, existing notes)
- No schematic/teardown found. Converter part, sample rate, analog filter corners = UNKNOWN.

## 1. Identity / variants
- Original Ditto Looper, mono in/out, 1 knob, 1 footswitch, 1 LED. Released 2013 (ESTIMATED).
- NOT modelled: Ditto+ / Ditto 2 (dual footswitch? 2 knobs/ stop & mode), Ditto X2 (stereo, 2 footswitches, reverse/half-speed, 5 min + USB export), Ditto X4, Ditto Jam. Those get separate model_ids (`tc-ditto-x2` etc.).
- Hardware (ESTIMATED unless noted): 9 V DC centre-negative, ~100 mA (X2 listing says >=100 mA; original assumed same); no battery; true bypass + analog dry-through while looper active (DOCUMENTED S1); 24-bit uncompressed (DOCUMENTED S1,S3); 5 min loop (DOCUMENTED); unlimited overdubs (DOCUMENTED); undo/redo (DOCUMENTED).
- Plugin: kind `Looper`, mono in (stereo host bus: sum L+R to mono for record, output mono to both channels), dry path never enters the loop DSP (see 3).

## 2. Controls
| Ctl | Name | Range / states | Taper | Default | Real behaviour |
|---|---|---|---|---|---|
| Knob | LOOP LEVEL | 0..1 (UI), mapped to -inf..+6 dB | dB-linear ESTIMATED (see 8) | 0.909 (unity) ESTIMATED | scales loop PLAYBACK only; dry unaffected; does not change recorded data (DOCUMENTED S2) |
| Footswitch | LOOP | momentary SPST, gestures below | n/a | n/a | single control for all functions |
| LED | STATUS | off / red / green / orange blink etc. | n/a | off | see state table; colours ESTIMATED |
| Jack | INPUT | 1/4" TS | | | mono |
| Jack | OUTPUT | 1/4" TS | | | mono |
| Jack | DC 9V | 2.1 mm | | | centre neg |
Only user-facing hardware controls: LOOP LEVEL + footswitch. Plugin extras (NOT hardware, in "Advanced" overlay only, default off): `tc_ditto_dry_level`, `tc_ditto_tempo` quantize, MIDI. Never show as knobs on the pedal face.

### 2.1 Footswitch gesture decoder (mono pedal, one switch)
Raw events: `down(t)`, `up(t)`. Constants (all ESTIMATED except where S noted):
- `TAP_MAX_MS` = 300 (press shorter = tap)
- `DOUBLE_GAP_MS` = 350 (second down within this after first up = double)
- `HOLD_MS` = 1000 for erase hold (after double), `UNDO_HOLD_MS` = 1500 for undo/redo (S3), configurable 800..2500.
- Debounce 10 ms.
Gesture classes:
- TAP: single down/up < TAP_MAX_MS, no second press within DOUBLE_GAP_MS. Decision delayed by DOUBLE_GAP_MS **only in states where double-click is meaningful** (PLAYING, OVERDUBBING, REC_FIRST? see below). In IDLE/EMPTY, TAP acts on down edge (zero latency record start). In REC_FIRST, TAP on down edge ends recording (loop length edge must be accurate; double is not meaningful there). In PLAY/OVERDUB, TAP acts on down edge too for overdub toggle (latency-critical) and a following second tap within DOUBLE_GAP_MS reinterprets: the pending toggle is rolled back (see 2.2 rollback) and STOP runs. Rationale: real unit toggles instantly (ESTIMATED) -- choose rollback to avoid 350 ms sluggishness.
- DOUBLE: down,up,down(within gap): -> STOP (S2).
- DOUBLE_HOLD: down,up,down held >= HOLD_MS -> ERASE (clear loop, S2).
- HOLD (single): down held >= UNDO_HOLD_MS -> UNDO (if last action produced undoable layer) else REDO (S2,S3). Fires on threshold crossing while still held; release afterwards = no-op.
- Hold thresholds checked by audio-thread timer on sample count; gesture decoder runs in processBlock (sample-accurate), not UI timer.

### 2.2 Rollback of speculative toggle
On TAP-down in PLAYING: switch to OVERDUBBING immediately (write position uses punch-in crossfade). If second down arrives <= DOUBLE_GAP_MS after first up: abort overdub layer (discard layer captured since punch-in; <= 350 ms of audio) and go STOP. Implementation: overdub layer written to scratch layer buffer, so discard = drop pointer. No data in base loop is touched until layer commit.

## 3. Signal path & model
Real unit is a digital looper with analog dry-through (DOCUMENTED). Stage list:
1. Input buffer (ESTIMATED: Zin ~1 MOhm, gain 1). 
2. Dry path: analog, always passes to output (true bypass when pedal "off" = no loop playing? see 3.2). Plugin: dry = input * `tc_ditto_dry_level` (default 1.0 = 0 dB), zero latency, NO processing (no filter, no clip). A tiny flat unity buffer; optional ESTIMATED 1st-order HPF at 5 Hz (DC block) default OFF.
3. Record path: input -> anti-alias LPF -> ADC (24-bit) -> sample memory. Plugin: store float32 mono at host sample rate (no SRC). Optional "converter colour" (default OFF): 1-pole LPF at 0.45*fs? ESTIMATED not measured. Converter rate UNKNOWN; assume 44.1/48 kHz class with band limit ~20 kHz (ESTIMATED).
4. Playback path: memory -> DAC -> recon LPF -> LOOP LEVEL gain -> sum with dry.
5. Overdub: input summed into loop at write pointer: `mem[w] = mem[w] + in` (feedback = 1.0, DOCUMENTED-ish "unlimited overdubs": no decay). Real hardware headroom/clipping while stacking UNKNOWN: plugin applies soft limit only if |mem| > 1.0 (see 4.4) -- flagged inaccuracy.
6. Output sum: `out = dry + loop*level`.

### 3.1 Latency
- Dry: 0 samples (analog) DOCUMENTED S1 (spec "None").
- Loop wet: playback aligned to recorded phase: loop plays exactly one length L after record start; recording latency of converters (~1-2 ms ESTIMATED) NOT modelled by default; option `tc_ditto_conv_latency_samples` (0..256, default 0, Advanced).
- Plugin reports latency 0 to host.

### 3.2 Bypass semantics
- Hardware: footswitch always controls loop; "bypass" = loop stopped (true bypass relays? S1 says true bypass). Stopped loop audio is muted; dry passes. Plugin: board bypass of the pedal -> dry only, loop state retained (not erased) but paused in time (see 5.3 for continue). 
- Transition: loop mute/unmute crossfade = 5 ms raised-cosine (ESTIMATED, avoids click). Real stop in hardware: loop plays to end? UNKNOWN; plugin: STOP is immediate (5 ms fade), not end-of-loop. Option `tc_ditto_stop_at_end` (bool, default false) for end-of-loop stop.
- Relay click not modelled.

### 3.3 State machine (single footswitch)
States: EMPTY, REC_FIRST, PLAYING, OVERDUBBING, STOPPED, ERASED(transient->EMPTY).
LED (ESTIMATED colours): EMPTY off/dim; REC_FIRST red solid; OVERDUBBING red solid (or orange) ; PLAYING green solid; STOPPED off; undo-confirm flash 2x; erase flash 3x.
| From | Gesture | To | Action |
|---|---|---|---|
| EMPTY | TAP | REC_FIRST | w=0, len=0, start recording first layer |
| REC_FIRST | TAP | PLAYING* | len=w (cap MAXLEN), loop wraps to 0; *or OVERDUBBING if `tc_ditto_rec_to_overdub`=true (default false: Ditto goes straight to play after first rec? ESTIMATED; hardware manual TOC suggests record->play; verify). |
| REC_FIRST | reaching MAXLEN | PLAYING | auto-close, len=MAXLEN |
| PLAYING | TAP | OVERDUBBING | layer start at pointer, crossfade in 3 ms |
| OVERDUBBING | TAP | PLAYING | commit layer to mem (see 4.3), push undo stack |
| PLAYING/OVERDUBBING | DOUBLE | STOPPED | commit layer if any, fade out |
| STOPPED | TAP | PLAYING | restart from position 0 (loop start) -- ESTIMATED; hardware restarts from beginning |
| any non-EMPTY | DOUBLE_HOLD | EMPTY | clear mem, clear undo/redo, len=0 |
| PLAYING/STOPPED/OVERDUBBING | HOLD | UNDO or REDO | undo last committed layer; if already undone -> REDO; two-slot toggle (see 4.5) |
| STOPPED | DOUBLE (second stop) | STOPPED | no-op |
Undo model: depth 1 (undo last overdub layer, redo restores it) -- DOCUMENTED: "undo = delete last take, redo = restore deleted take" (S4 TOC). Plugin default `tc_ditto_undo_depth`=1 (matches hardware, ESTIMATED), option up to 32 (Advanced, extra RAM).
Undo of first layer (base loop) = erases whole loop (state EMPTY with redo available) ESTIMATED.

### 3.4 Storage
- MAXLEN_SEC = 300 (5 min, DOCUMENTED). At fs: MAXLEN = 300*fs samples; mono float32: 44.1k=52.9 MB, 48k=57.6 MB, 96k=115.2 MB; plus undo layer(s). Layer buffer = full-length copy only for touched region (4.3). RAM budget per instance: 2 x MAXLEN x 4 B (base + undo) + scratch.
- Allocation: preallocate in `prepare()` (message thread) via std::vector sized for current fs; resize on sample-rate change only in prepare. Lazy-commit pages: allocate base buffer once; no alloc on audio thread ever.
- Persistence: loop audio NOT saved in plugin state by default (size); option `save_loop_in_state` writes 24-bit PCM FLAC-like raw to file chunk asynchronously (message thread), ESTIMATED.

## 4. Algorithm
### 4.1 Block diagram
```
in(L,R) --mono sum--> m
m ---------------------------------> dry*gDry ---+
m --> recordGate --> layerWriter --> LoopMem      |
LoopMem --> readPtr --> xfade/fades --> *gLevel --+--> sum --> out(L=R)
```
### 4.2 Per-sample (loop `for n in block`)
```
m = 0.5*(inL+inR)             // or inL if mono bus
lvl = levelSmoother.next()    // see 4.6
fade = stateFade.next()       // 0..1 loop mute fade
p = rd                        // read idx in [0,len)
y = mem[p]                    // + undo layer? no: mem is committed-only
if (state==OVERDUBBING) {
   g = odFade.next()          // 0..1 over 3 ms
   layer[p] += m * g          // write to scratch layer at p
   y += layer[p]              // monitor layer sum
}
if (state==REC_FIRST) { mem[w++] = m; y = 0; if (w>=MAXLEN) close(); }
out = m*dry + y*lvl*fade
rd = (rd+1 == len) ? 0 : rd+1  // wrap; zero-gap; no seam xfade after first close
```
First close: loop seam. Real unit hard-wrap (ESTIMATED). Plugin: apply 64-sample (1.3 ms @48k) crossfade of last 64 recorded samples into samples [0..63]? No: keep exact hard wrap by default; option `tc_ditto_seam_xfade_ms` (0..10, default 0).
### 4.3 Layer commit
At OVERDUBBING->PLAYING: `mem[i] += layer[i]` for touched range [pStart..pEnd] (wrapped), then store `undoLayer` = touched range copy of layer (not mem) so undo = `mem -= layer` (exact, float add/sub error ~1e-7; for exactness store pre-mem copy instead: `undoBackup[i]=mem[i]` before add; undo = swap(mem, backup) for range). Use backup-swap: bit-exact undo/redo. Touched range tracked as one or two intervals; max = len.
Commit executed incrementally on audio thread: range copy of up to 14.4 M samples would spike CPU -> do commit **per-sample into mem during overdub** instead: write `backup[p] = mem[p]` once on first touch of p (bitset first-touch flags, 1 bit/sample = 1.8 MB) and `mem[p] += m*g`. Then no commit cost. Undo = swap over touched range executed over multiple blocks by a background-thread-free "incremental swap" budget: 8192 samples/block; until finished, state UNDO_PENDING (playback keeps reading mem via a read-through mask: read from backup if flag set when undo in progress). Simplest RT-safe: swap done on **message-thread worker** with atomic double buffering: mem is two full buffers A/B? Memory x2 already budgeted (undo). So: undo = flip `activeBuf` index atomically at the loop wrap (zero-click: flip only at p==0 or via 5 ms crossfade). Layer data: overdub writes into `bufNext` = copy of `bufActive` made incrementally? Decision (dev-ready): **ping-pong buffers**: at overdub start, background thread (preallocated worker, lock-free command queue) copies active->next lazily; audio reads active; during overdub the audio thread writes `next[p] = active[p] + m*g` for p it passes; for p ahead of the pointer not yet copied, worker copies in the background faster than realtime (copy 14 M floats ~ 10 ms). Audio thread guard: if `copiedUpTo` flag for p false, do `next[p]=active[p]+...` itself (idempotent). On commit: wait copy complete (worker flag, else audio finishes remaining copy in 8192-sample slices; STOP-the-world avoided by keeping read from active until `next` complete) then atomic swap index at next wrap or xfade 5 ms. `prev` index retained for undo = flip back (same mechanism). Redo = flip forward. Third buffer not needed for depth 1. Depth N>1: ring of N+1 buffers (Advanced, N<=8 to cap RAM).
### 4.4 Overdub gain / clipping
Sum is unbounded float. Output stage `outLimit`: if |y*lvl| > 1.0 apply tanh-like softclip `y = x/(1+|x|*0.?)`? Keep: no clipping inside mem (float); only master `softClip(x) = x - x^3/3` for |x|<=1.5 else +-1 (ESTIMATED), default OFF (hardware headroom unknown; marked inaccuracy). Aliasing: none (memory playback, no nonlinearity unless softClip on -> 2x oversample not needed at default OFF; if ON, apply 2x polyphase half-band 31 taps).
### 4.5 Undo/redo stack
`struct Slot {uint8 bufIdx; uint32 len;}` ring of depth+1. `undoPtr`, `redoAvail` flags. New overdub commit clears redo (standard). Hold gesture: if `redoAvail && lastActionWasUndo` -> REDO else UNDO. Actions: UNDO then HOLD again = REDO (toggle), matches single-gesture S2 "hold = undo/redo".
### 4.6 Smoothing / denormals / CPU
- `tc_ditto_loop_level` smoothing: 1-pole, tau 10 ms ("smoothing ms" col). Stop/start fade 5 ms raised-cosine; overdub in/out 3 ms.
- Denormals: set `juce::ScopedNoDenormals` in processBlock; memory is audio floats added at most; flush |x|<1e-30 to 0 on write when state==OVERDUBBING.
- Oversampling: none. Resampling: none.
- CPU budget: <0.3% one core @48k/512 for play+overdub; per sample ~10 flops. Memory-bound; sequential access. Worker copy thread: nice priority low, never blocks audio.
- RT-safety: no alloc/locks/logging on audio thread; commands (undo, erase, buffer swap done) via `std::atomic` + SPSC<Cmd,64>; erase = set `len=0` and zero buffers on worker (muted until done; `eraseBusy` shows LED flash).
- Sample-rate change: stop, free, re-alloc in prepare; loop audio converted (lossy resample) via worker if state non-empty, else discarded -> warn.

## 5. I/O & routing
- Hardware: 1 in, 1 out, DC. No aux/expression/MIDI/USB on original.
- Plugin: board slot with mono sum in; output mono to L=R unless board stereo mode: if upstream stereo, dry stays stereo (pass-through L/R separate), loop added equally to both (`out = dry_stereo + loop`). 
- Position in chain: typical looper is LAST (captures full chain) or first (clean DI then re-amp) -- user picks via board order (BOARD_CORE). Because dry-through is not recorded in the loop output stage, **feedback tricks**: 
  - Loop in effects loop of another pedal/amp: place after drive/amp -> records wet sound.
  - Re-amping: looper first, output of loop goes to chain after it -> loop plays through downstream effects, changing the tone live (note: dry also feeds downstream).
  - Resample feedback: board option `tc_ditto_send_return_mode`: route board "return bus tap" into looper input (feedback loop) -- gain-limited to 0.98 loop gain + DC block, clipped at 1.0 to avoid runaway (ESTIMATED). Default off.
  - Series/parallel: parallel = looper on a send bus with dry/wet bus; series = default.
  - Stereo split: not on hardware; Ditto mono duplicates to L=R.
- Hot-swap order change while recording: record input tap follows board order; changing order mid-record does not touch recorded audio.

## 6. MIDI / sync / expression (Virtual -- not on hardware; off by default for faithful presets)
Hardware has none (S1). Provide comparable functions to later Ditto X2 / RC-1 class, labelled "Virtual MIDI (not on hardware)". CC numbers ESTIMATED (see MIDI_EXPRESSION.md for global map; these are defaults):
| Source | Target | Notes |
|---|---|---|
| CC 80 (>=64 on, edge) | footswitch TAP | `tc_ditto_midi_fs_tap` |
| CC 81 | footswitch DOUBLE (stop) | edge |
| CC 82 | footswitch DOUBLE_HOLD (erase) | edge |
| CC 83 | UNDO/REDO | edge |
| CC 7 (or 85) | `tc_ditto_loop_level` | 0..127 -> 0..1 via taper in 8 |
| PC n | none (looper has no presets); optional: PC0 = stop, PC1 = play | |
| MIDI clock (0xF8) | Quantize | see below |
| Note-on vel | gesture synthesiser | velocity >= `hard_thr` (100) = DOUBLE (stop) else TAP; aftertouch hold > 0.7 for 1.5 s = HOLD (undo/redo) |
| Expression pedal / slider / aftertouch | loop level | via MIDI_EXPRESSION.md curves |
Tempo sync (virtual): `tc_ditto_quantize` enum {off, beat, bar}, default off. When on: REC_FIRST start and end snapped to nearest beat/bar of board clock (host ppq or MIDI clock; AUTO priority per MIDI_EXPRESSION sec 3.1); end snapped forward so len = k*bar (k>=1); START waits max 1 bar showing LED blink. Host transport start/stop optional follow: `tc_ditto_follow_transport` (bool, default off): host play -> PLAYING from 0; host stop -> STOPPED. Loop length in beats displayed. Without sync the unit is free-running (hardware parity).
Latency of MIDI-triggered events: applied at event sampleOffset, sub-block split per MIDI_EXPRESSION sec 2.

## 7. Visuals (original vector art; no copied photos)
Enclosure: compact stompbox, ESTIMATED 48 x 93 x 52 mm (W x D... hardware ~ 1.9 x 3.7 in?). Colour: matte black body, white/silver printed legends, flat top; draw as rounded rect r=3 mm. UI scale: 1 mm = 4 px base.
Layout (face, origin top-left, 48 x 93 mm):
- Footswitch: round metal cap 16 mm dia centre (24, 68) mm.
- LOOP LEVEL knob: 14 mm dia, black knurled w/ white line pointer, centre (24, 30) mm; travel -135 deg..+135 deg (7 o'clock to 5 o'clock), label "LOOP LEVEL" above (centre 24,14).
- Status LED: 3 mm, centre (24, 50) mm, above footswitch; lens colour per state (3.3).
- Jacks (side/top, drawn as notches): INPUT right side, OUTPUT left side at y = 40 mm; DC jack top-centre (back) not clickable.
- Logo text: descriptive only "Looper"; avoid TC trademark logo; label name field "Ditto-style Looper (inspired by)" in drawer.
Vector notes: footswitch with concentric rings, LED glow radial gradient additive; knob indicator rotates with `tc_ditto_loop_level`; screws none.
Board mounting visuals handled by UI_BOARD_STYLES.md (footprint 48 x 93 mm; weight ESTIMATED 130 g).

## 8. Parameters
| id | name | min | max | default | unit | skew | smooth ms | automatable |
|---|---|---|---|---|---|---|---|---|
| tc_ditto_loop_level | Loop Level | 0 | 1 | 0.909 | norm | dB-linear, see below | 10 | yes |
| tc_ditto_dry_level | Dry Level (adv) | 0 | 1 | 1.0 | lin | linear | 10 | yes |
| tc_ditto_fs | Footswitch (momentary) | 0 | 1 | 0 | bool | n/a | 0 | yes (gesture decoded) |
| tc_ditto_undo_hold_ms | Undo Hold (adv) | 800 | 2500 | 1500 | ms | lin | 0 | no |
| tc_ditto_undo_depth | Undo Depth (adv) | 1 | 8 | 1 | int | n/a | 0 | no |
| tc_ditto_rec_to_overdub | Rec->Overdub (adv) | 0 | 1 | 0 | bool | n/a | 0 | no |
| tc_ditto_stop_at_end | Stop At End (adv) | 0 | 1 | 0 | bool | n/a | 0 | no |
| tc_ditto_seam_xfade_ms | Seam Xfade (adv) | 0 | 10 | 0 | ms | lin | 0 | no |
| tc_ditto_quantize | Quantize (virtual) | 0 | 2 | 0 | enum | n/a | 0 | yes |
| tc_ditto_follow_transport | Follow Transport (virtual) | 0 | 1 | 0 | bool | n/a | 0 | no |
| tc_ditto_conv_latency_samples | Converter Latency (adv) | 0 | 256 | 0 | smp | n/a | 0 | no |
| tc_ditto_midi_enable | Virtual MIDI | 0 | 1 | 0 | bool | n/a | 0 | no |
| tc_ditto_send_return_mode | Feedback Tap (adv) | 0 | 1 | 0 | bool | n/a | 0 | no |
Loop level mapping (ESTIMATED; hardware taper unknown): x<0.01 -> 0 (mute); else g_dB = -60 + 66*x; x=0.909 = 0 dB (default, unity), x=1.0 = +6 dB.
State (not params, not automatable): loop mem, len, state, undo ring -> not serialised unless `save_loop_in_state`.

## 9. Tests / acceptance
T1 Capacity: record 300 s -> auto-close PLAYING, len == 300*fs exactly; 300.0 s + 1 smp not allowed. Pass: exact.
T2 Zero-gap wrap: record sine of integer periods, play 10 loops; seam discontinuity |delta| <= sine slope. Pass: null vs ideal repeat <= -120 dBFS (float).
T3 Gesture precedence: synthetic down/up traces: tap, double (gap 200 ms), double-hold (1000 ms), hold 1500 ms, tap+tap at 351 ms (two taps not a double). Expected states per 3.3; decision latency for tap <= 1 block.
T4 Indefinite overdub: 1000 overdub cycles of 1 s loop; mem sum equals analytic sum within 1e-5 relative; no NaN, no growth beyond sum.
T5 Undo/redo bit-exact: overdub, undo -> mem equals pre-overdub bytes; redo -> equals post-overdub; 100 random undo/redo sequences.
T6 Dry null: pedal in EMPTY/STOPPED: out == in bit-exact at dry_level=1.
T7 Level: loop_level sweep: gain at x=0.909 = 0 dB +-0.01; x=0.01 monotonic; no zipper (max step per sample < 0.001 of full scale on 1 kHz tone).
T8 Frequency response: dry path flat +-0.01 dB 20 Hz..20 kHz; loop path flat (no converter colour) +-0.01 dB. THD+N loop path < -140 dBFS (float). With converter-colour ON: -3 dB at 0.45*fs ESTIMATED.
T9 Block sizes 1,4,16,64,512 and fs 44.1/48/88.2/96 k: bit-identical loop output.
T10 RT safety: ASAN/TSAN, no malloc under audio callback (malloc hook), 30 min randomized gesture soak (seeded) -- no crash, no xrun CPU >0.5%.
T11 Quantize: with tempo 120 BPM, quantize=bar -> len == 2.0 s * k exact in samples within +-1.
T12 Save/restore: params round-trip; state EMPTY after restore (unless save_loop_in_state: samples identical).
T13 Host transport follow: start/stop -> states per table.
T14 Hardware comparison: NONE possible until a physical unit is measured; thresholds provisional. Do not claim "identical".

## 10. Known inaccuracies
- Footswitch timings (TAP/DOUBLE/HOLD) ESTIMATED; only qualitative gestures documented (S2,S3).
- Gesture names for stop/erase from retailer text; may differ per firmware.
- No info on converter part, sample rate, anti-alias filter, analog clipping / overdub headroom, LED colours/patterns, dimensions, input/output impedance, loop level taper, STOP end-of-loop behaviour, restart position, first-record end action (record->play vs record->overdub).
- Level taper and unity point invented.
- Click/relay/pop behaviour of true bypass not modelled.
- Plugin loop is float32 (hardware 24-bit); bit-exactness differs.
- Virtual MIDI/quantize are additions not on hardware.
- Next evidence: official manual PDF text (S4) once egress allowed; unit measurements.
