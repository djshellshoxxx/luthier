# BOARD_CORE: pedalboard engine spec (dev-ready)

Ver 1.1 | 2026-10-21 | Status SPEC (cross-spec reconciled; normative errata in IMPLEMENTATION_PLAN.md sec 1 win on conflict). Supersedes prototype `Source/ReferencePedals/Board.h` (8 fixed slots, packedOrder 4-bit, 3 knobs, 3 hardcoded models). Namespace `luthier::reference` kept. Sibling specs: GENERIC_PEDALS.md, ../ARCHITECTURE.md (compat rules still binding), ../models/*.md.
Value tags: [DOC]=from existing code/spec, [EST]=estimate, tune after profiling, [MEAS]=to be measured. No external hardware values here; all numbers in this file are design choices = [EST] unless tagged.

## 0. Ground facts (from repo, [DOC])
- Luthier = instrument synth. Buses (`PluginProcessor.cpp:45`): Output stereo (main), optional Sidechain stereo input (default disabled), many aux stereo outputs (strings, noise, jam drums/bass). Board input is the engine's internal signal, not host audio, unless reamp-from-sidechain enabled (sec 6.8).
- Prototype taps: `LuthierEngine.cpp:3461` (before preEffects), `:3529` (after postEffects). Engine owns `reference::Board referencePedals` (`LuthierEngine.h:897`). Audio buffers are `double*` L/R.
- Existing `EffectsChain` pre/post, `PedalType`, factory presets: UNTOUCHED. Board default OFF.
- Host state: JSON blob with `formatVersion`, unknown root keys preserved (`HostStateFormat.cpp isKnownRootKey`). New root key `referencePedals` must be added to `isKnownRootKey`.

## 1. Goals / non-goals
G1 N-slot ordered chain; audio order == visual order, always (single source of truth, sec 3).
G2 Reorder at runtime (drag, keyboard, MIDI, automation) lock-free, click-free.
G3 Universal board wet/dry (equal-power), latency-compensated; per-pedal mix independent.
G4 Routing modes: series, parallel, A/B split, send/return loop, feedback loop (safe), pre/post amp, stereo, expression/aux jacks.
G5 State save/restore, versioned; legacy sessions bit-identical with board off.
G6 Zero alloc/lock on audio thread; CPU budget enforced.
Non-goals: modelling impedance interaction with pickups (ARCHITECTURE.md), host-audio multichannel (> stereo), pedal-model DSP (see model/GENERIC specs).

## 2. Constants
```
kMaxSlots        = 16      // storage capacity; UI default shows 8, scroll to 16
kMaxBranches     = 4       // parallel/split lanes
kMaxLoops        = 2       // send/return loops
kMaxXfadeMs      = 12
kReorderDipMs    = 6       // per half (out + in)
kMaxBlock        = host maxBlock (prepare), internal sub-block 64 samples for control smoothing
kMaxParams       = 32 (descriptor), kMaxLaneParams = 16 (host lanes)  -- see sec 11
kMaxLatencySamp  = 8192    // board total, @48k = 170 ms; reject adds exceeding
kMaxInstanceId   = 255 (uint8)  // storage index 0..15, instanceId is UUID (state only)
```

## 3. Data model: single source of truth
```
struct SlotId   { uint8_t idx; }                 // storage index 0..kMaxSlots-1, stable for life of instance
struct PedalInstance {                           // owned by BoardModel (message thread); DSP object owned by Pool
  uuid        instanceId;                        // persisted, stable across reorder
  std::string modelId;                           // "generic.overdrive", "boss-ds1", ...
  uint16_t    modelRevision;
  bool        enabled;                           // footswitch (pedal-level)
  Placement   placement;                         // preAmp | loop (sec 6.5)
  float       params[kMaxParams];                // normalised 0..1, raw store, see sec 9
}
struct Topology {                                 // immutable, built by Compiler on message thread
  uint32_t  version;                              // monotonically increasing
  Mode      mode;                                 // Series | Parallel | AB | Loop (sec 6)
  uint8_t   n;                                    // active slots
  uint8_t   order[kMaxSlots];                     // slot idx in signal order == left-to-right visual order
  Lane      lanes[kMaxBranches];                  // for non-series: list of order[] indexes per lane
  Loop      loops[kMaxLoops];
  FeedbackCfg fb;
  int       latencyTotal; int laneDelay[kMaxBranches];
  uint16_t  dryDelay;                             // wet/dry latency comp
}
```
Rule O1: `order[]` is the ONLY ordering authority. UI card strip is generated from the same `Topology` (or the BoardModel it is compiled from); no parallel UI-side list. Reorder = BoardModel mutation -> `Compiler::compile()` -> publish. UI reads model back after publish for redraw (never optimistic reorder without model change).
Rule O2: state (delay lines, filters, LFO phase, envelopes) lives in the pedal object keyed by SlotId, NOT in the chain position. Moving a pedal keeps its tail/phase.
Rule O3: pedal objects preallocated in `PedalPool` (kMaxSlots), model switch = construct new model on message thread into spare object, swap by pointer in a new Topology (sec 4). RT never constructs.

## 4. Lock-free publish, retire, crossfade
### 4.1 Publication (RCU-lite, triple pointer)
```
std::atomic<const Topology*> live;               // audio reads once per block (acquire)
std::atomic<uint32_t>        audioEpoch;         // audio writes version it adopted (release) at block start
RetireList retired;                              // message thread only; free when audioEpoch >= retiredVersion+1 AND (no audio running OR 2 blocks elapsed)
```
- Message thread: build new Topology (heap OK), `live.store(new, release)`, push old to `retired`. A 30 Hz juce::Timer (or next UI event) runs `reclaim()`: delete entries whose version < audioEpoch. If audio stopped (host suspended, `audioEpoch` frozen) reclaim after `releaseResources()`/`prepareToPlay` barrier.
- Audio thread: at block start `t = live.load(acquire)`; if `t->version != cur->version` -> start transition (4.2). Never frees. Never reads message-thread structures.
- Coalescing: if multiple publishes between blocks, only newest adopted; intermediates retired. Rapid drag emits <= 1 publish per 8 ms (UI debounce).
- DSP pool objects referenced by Topology are never destroyed while any non-retired Topology references them; model-swap puts old DSP object into `retiredDsp` with the same version rule.
### 4.2 Click-free order change: dip-through-dry (default, zero extra CPU)
Reorder (or any structural change: add, remove, mode change, model swap) is a structural transition:
```
state T_IDLE -> (new topology seen) T_FADE_OUT (kReorderDipMs, cos^2 ramp of wet-path gain g: 1->0)
  at g==0 (sample-accurate: split block at ramp end) swap cur=new; reset only NEW/changed pedals' states
-> T_FADE_IN (kReorderDipMs, g: 0->1) -> T_IDLE
```
- Gain g applies to the board WET signal before the wet/dry mixer, so audible result = crossfade wet->dry->wet(new order); dry path (latency-comp) continuous. If board mix==0 (fully dry) transition is skipped visually but still swaps at block start.
- Pedals keep processing during FADE_OUT with old order (tails continue) and from swap on with new order. Inputs seen by a pedal change discontinuously at swap, but g==0 hides it.
- Block-size independent: transition counters in samples, block split into <=2 segments (`seg1 old topology`, `seg2 new`); process() is called twice with sub-blocks. Pedals MUST be block-size independent (test T-BLK).
- Non-structural changes (enable toggle, params) are NOT transitions: per-pedal bypass xfade 5 ms (sec 5.2); params smoothed (sec 9.3).
- Alternative (rejected, documented): dual-render xfade of old vs new order needs 2x CPU and duplicate state per pedal (stateful, so impossible without cloning). Allowed later as `xfadeMode=Dual` only for pedals flagged `isStateless()`.
- Drag UX: while dragging a card, order is NOT published until drop (or hover-swap commit, 120 ms dwell). Each commit = one transition (<=12 ms). Spec: rapid successive commits queue; transition restarts from current g (no jump: ramp continues from current g value toward 0 then new topology).
### 4.3 Order determinism test: processing `[A,B]` != `[B,A]` for nonlinear pairs; golden: ODthenDelay vs DelaythenOD null-test difference > -30 dBFS RMS.

## 5. Per-slot signal path
```
in -> [InputTrim] -> pedal.process (if enabled or tail) -> [OutputTrim] -> out
bypass: crossfade between processed and latency-matched dry (kBypassXfadeMs=5)
```
### 5.1 Pedal bypass modes (per-model flag `bypassMode`)
- `TrueBypass`: dry passes untouched (but latency-matched delay pad when pedal latency>0, so toggling never shifts timing). Modelled click-free xfade 5 ms (hardware clicks optional: `bypassClick` model option, default off).
- `Buffered`: dry passes through pedal's input/output buffer stages (adds model-specific HF/gain colouration; for pedals with `bufferedBypass=true`).
- `DSPBypass`: effect inputs muted, tails run out (delay/reverb "trails"). Param `trails` (bool, default true for time-based pedals): when off, tail is cut with 20 ms fade.
### 5.2 Pedal enable ramp: `mix_enable` smoothed linearly 5 ms; when ramp==0 AND tail energy < -90 dBFS (tracked per pedal `tailSilent`) skip `process()` to save CPU (tail meter via pedal `float getTailEnergyDb()`; default returns 0 dB -> never skips).
### 5.3 Per-pedal mix (each pedal's own Mix/Blend/Level knob) lives INSIDE the pedal and is independent of board mix.
### 5.4 Trims: `InputTrim` -12..+12 dB default 0 (slot-level, id `ref_s{nn}_in_db`), `OutputTrim` same. Applied before/after pedal, outside of model, to calibrate level (ARCHITECTURE.md: -20 dBu != fixed dBFS). Reference level: 0 dBFS = +4 dBu nominal = [EST]; document `kRefDbu`.

## 6. Routing
All modes compile to a `Topology`; the audio engine executes a fixed set of node kinds (no dynamic graph on RT).
Node kinds: `Pedal(slot)`, `Split(k)`, `Sum(k)`, `LaneGain`, `Delay(n)`, `SendTap`, `ReturnInject`, `FbSend`, `FbReturn`, `Limiter`, `StereoWidth` (`FbSend` added by errata E25: the feedback tap after pedal s needs a node of its own). Normative definitions: `Source/ReferencePedals/Topology.h`.
### 6.1 Series (default)
`in -> order[0] -> ... -> order[n-1] -> out`. Visual left-to-right == audio order. Signal arrow on the UI.
### 6.2 Parallel (2-4 lanes)
`in -> Split -> lane[k] (own ordered pedals) -> LaneGain_k (0..+6, -inf..) -> Sum -> out`. Lane latencies equalised with `Delay(laneDelay[k] = maxLaneLat - lat_k)`. Sum gain = 1/sqrt(active lanes) default [EST] (`parallel_norm` choice: sqrt | unity | none). Lane polarity invert option per lane (`laneK_phase`, bool) for phase tricks. UI: lanes drawn as stacked rows; reorder only within a lane; cross-lane drag moves pedal and publishes.
### 6.3 A/B split (ABY box)
`in -> Split2 -> laneA, laneB`; select `ab_select`: A | B | A+B | A-then-sticky toggle. Switch via 10 ms equal-power xfade (not hard-switch). Outputs: `ab_out_mode`: Sum (to main) | Dual: `Dual` = laneA -> main output, laneB -> aux output bus "Pedal B" (new aux stereo bus appended AFTER existing ones; existing bus indices unchanged; default disabled). Ground-lift hum is not modelled; only per-lane `laneK_phase` invert is offered.
### 6.4 Send/Return loops (up to 2)
Node `SendTap(at order position p)` and `ReturnInject(at q>p)`: pedals between p..q run on the "loop lane" while main path is split by `loop_mix` (0 = loop muted, 1 = loop fully replaces). Models a looper/amp 4-cable method or pedal FX loop: `loop_kind`: Serial (100% through) | Parallel (blend, `loop_blend`) | Kill-dry (wet only). Loop has own `loopK_enabled` (footswitch), trims, and send/return level (-24..+12 dB). Latency equalised like lanes.
### 6.5 Placement relative to amp (pre/post)
Prototype used `Placement::{beforeExistingPre, afterExistingPost}`. New enum, values preserved numerically:
```
enum class Placement : int { beforeExistingPre = 0 /*UI tag PRE*/, afterExistingPost = 1 /*UI tag LOOP, between amp and cabinet*/, postCab = 2 /*UI tag POST, after cabinet, before room; NEW tap, see below*/ };  // 3 values only. Old draft value preAmpInput(3) REMOVED (alias of 0, never serialised). Numeric values 0/1 preserved from prototype.
```
Each slot has a placement; engine runs the board in up to 2 (3 with postCab) segments: a segment executes the subset of `order[]` whose placement matches, preserving relative order from the single `order[]`. So the UI displays ONE strip but pedals carry a coloured "PRE | LOOP | POST" tag; chain order within a segment == strip order; across segments the amp lies between, which is the physically correct behaviour. UI must draw an "AMP" marker between PRE group and LOOP group in the strip and prevent drops that would mix them without changing placement (drop across marker changes `placement` field; confirm with undo).
Segment latency compensation: each segment has its own wet/dry (sec 7); universal board mix applies per segment in lock-step (same `ref_board_mix`).
Tap sites (VERIFIED against `Source/LuthierEngine.cpp` @ this commit): PRE = `LuthierEngine.cpp:~3461` immediately before `preEffects.processStereo` (dual-mono double scratch `pedalScratchL/R`); LOOP = `:~3529` immediately after `postEffects.processStereo`, before EQ-match and `cabinet.processBlock` (so LOOP is pre-cabinet, amp-FX-loop semantics); POST = NEW call site between `cabinet.processBlock(workBuffer)` and `room.processBlock(workBuffer)` (`:~3550`). `workBuffer` there is `juce::AudioBuffer<float>` stereo: POST site copies float->double scratch (`pedalScratchL/R`, already sized to maxBlock), runs `Board::processStereo(...,Placement::postCab)`, copies back; zero cost when no POST pedal exists (early-out before copy). O1 RESOLVED: postCab tap is feasible at that site; the three call sites are the ONLY engine edits (WP9).
### 6.6 Feedback loop
Hardware analog: a delay/reverb output returned to an upstream pedal input (e.g. out of delay back into fuzz).
```
FbSend(tap after pedal s) --> FbGain(g_fb) --> HP(20Hz) --> LP(fb_lp, def 12kHz) --> SoftLimit --> Delay(1 block min = kMinFbDelay) --> FbReturn(sum at input of pedal r, r<=s)
```
- Unit delay: feedback return is read from previous sub-block (min delay = 64 samples sub-block; documented, +1.3 ms @48k). No zero-delay loops.
- Safety (mandatory, in this order): (1) loop gain g_fb 0..1.05 range; UI max "1.05 = runaway-capable" requires `fb_runaway_ok` toggle else clamp 0.98; (2) tanh-style soft limiter at +6 dBFS ceiling `x/(1+|x|/ceil)` or `ceil*tanh(x/ceil)`, ceil=2.0 (linear); (3) DC blocker; (4) NaN/Inf guard -> zero state + latch `fb_fault` flag + UI warning; (5) energy watchdog: if loop RMS > +12 dBFS for > 200 ms, ramp g_fb to 0 over 50 ms and set `fb_tripped` (user re-arms); (6) output `BoardLimiter` always present at end of board (sec 6.7). (7) Kill: a "kill feedback" param (momentary) 5 ms mute of FbGain.
- Multiple feedback loops: max 1 default (kMaxFb=1); r<=s only (acyclic otherwise). Compiler rejects s<r.
- Modulation: expression jack may target g_fb (self-oscillation knob, sec 6.9).
### 6.7 Board safety
`BoardLimiter` at board output: soft-knee peak limiter, ceiling 0 dBFS default (range -6..+6 dB), lookahead 0 (zero-latency; tanh soft clip above ceiling*0.9 before hard clamp +-16 like prototype `safe()`), always `isfinite` sanitize each segment output. Disable = `ref_board_limiter_on`=false, still sanitize NaN/Inf and clamp +-16.
### 6.8 Mono / stereo / I-O
- Processing format: double L/R planar (matches engine). Pedal API `process(double* L, double* R, int n)`. Pedals declare `ChannelMode`: `Mono` (hardware-mono pedal; Config param `stereo_mode` {dual_mono (default, matches branded specs), mono_sum}: dual_mono = two independent DSP states L/R sharing params; mono_sum = (L+R)*0.5 processed once and written to both. CPU rule: if upstream `outputsDualMono()` (L==R guaranteed) the board processes ONE channel and copies, which is bit-exact for deterministic dual_mono because both states see identical input), `Stereo` (L/R independent, shared params), `TrueStereo` (cross-coupled, e.g. stereo delay/reverb), `MonoInStereoOut` (mono in, decorrelated stereo out, e.g. chorus).
- Mono-to-stereo chain rule: the chain is stereo end-to-end; a mono pedal after a stereo pedal sums L/R as (L+R)/2 (no extra gain; documented). Avoid summing when input L==R (engine internal signal usually dual-mono): detect `L==R` per block via `std::equal`? Cheaper: flag `isDualMono` propagated by pedals (`outputsDualMono()`), mono pedal skips summing and processes one channel, copy.
- Board I/O jacks (virtual): `IN L/R` (internal tap), `OUT L/R`, `SEND/RETURN` (6.4), `AUX IN` = optional Sidechain bus (stereo; if enabled by host) as "reamp input": `reamp_enable` mixes sidechain into board input at `reamp_gain` (-inf..+12 dB), or replaces engine signal (`reamp_replace`). Default off; zero effect when sidechain bus disabled. Latency of sidechain not compensated vs engine (documented).
- Board OUT: main stereo (default), optional `Pedal B` aux bus (6.3 Dual), optional `Wet only` aux bus and `Dry only` aux bus for external processing (appended AFTER existing aux buses, default disabled, `isBusesLayoutSupported` unchanged for old layouts).
### 6.9 Expression / aux jacks
- `exp1`, `exp2` virtual jacks: normalised 0..1 float sources. Any pedal param flagged `expressionTarget` may be assigned to exp1/exp2 with range `[lo,hi]` (0..1 normalised each) and curve (lin | log | s-curve) via `slot{n}_{param}_exp` assignment struct {src:0/1/2, lo, hi}. Sources: MIDI CC (learn), MIDI aftertouch (channel/poly), velocity (note-on, latched), UI slider, host-automatable param `exp1`,`exp2`, "heel/toe" switch (toe-switch = threshold 0.97 emits press).
- Aux footswitch jacks `aux_sw1`, `aux_sw2` (momentary or latching, `aux_mode`): functions: tap-tempo, next preset, bank select, enable-group, kill-feedback, loop-engage. Assigned via `AuxFn` enum. Gesture decode in UI-side module as in ARCHITECTURE (sample/timestamps).
- Expression processing: audio thread reads atomics (relaxed) once per sub-block, smoothed 10 ms.

## 7. Universal wet/dry
Params: `ref_board_mix` 0..1 (default 1.0 = 100% wet), `ref_board_mix_law` {0 EqualPower (default), 1 Linear, 2 -4.5 dB}. Mix always active while board enabled.
```
theta = ref_board_mix * pi/2
EqualPower:  out = dryD * cos(theta) + wet * sin(theta)
Linear:      out = dryD * (1-m) + wet * m
Third law (`ref_board_mix_law`=2): -4.5 dB pan law, out = dryD*sqrt((1-m)*cos(theta)) + wet*sqrt(m*sin(theta)) [EST]; reserved, optional in v1.
```
- Equal-power note: for fully correlated dry/wet (all pedals bypassed) level at m=0.5 is +3.01 dB. Accepted, documented, and law switchable. Test asserts both.
- Per-sample smoothing of cos/sin coefficients: 20 ms one-pole on `m`, coefficients recomputed per 64-sample sub-block via table `cosTab[1025]` linear interp (no std::sin on RT per sample). Stage runs per segment (sec 6.5).
- Dry path: `dryD = delay(dry, L_wet)`; L_wet = total latency of the wet path (sum of pedal latencies along the chain, max over lanes after lane equalisation). Integer delay line, ring buffer preallocated `kMaxLatencySamp+maxBlock`. When L_wet changes (add/remove/quality), delay changes on structural transition (sec 4.2), crossfade of read taps 10 ms inside dip. Fractional (oversampler half-sample) rounding: pedal latencies reported integer; sub-sample residual ignored (documented).
- `ref_board_mix == 1`: dry read skipped (exact wet output; no dry multiplication; avoids cos(pi/2)=6e-17 leakage). `ref_board_mix == 0`: wet path still processed (tails alive) but only if ref_board_mix_tail=true (default true) else skip processing for CPU.
- Per-pedal Mix (inside pedal) is independent: board mix operates on whole-board wet vs board dry.
- Board bypass (`ref_board_enabled`=false): ramp board mix to 0 over 20 ms using the wet/dry itself; after 20 ms + tails (default `ref_board_trails`=true: run wet path with input muted until `tailSilent`) skip entirely. When board disabled from startup (state: off) -> bit-identical legacy path: `Board::processStereo` returns immediately before touching buffers; Test T-LEGACY.
- Reported latency: see 8.

## 8. Latency
- Each pedal reports `getLatencySamples()` (oversampler FIR, lookahead). Value fixed per (model, quality mode, sampleRate), independent of bypass/enable (bypass path delay-matched, 5.1).
- Board latency `L_board = sum over placed (non-empty) pedals in the longest lane + 0`. Series: sum all. Changes only on structural change.
- Plugin PDC: engine already internal; the board lies on the audio path of a synth with MIDI input => latency is relative to MIDI note timing. Policy: `setLatencySamples(base + L_board)` on message thread after structural commit only if `report_latency` param true (default TRUE for correct offline bounce; host sees one-time change). Debounced 250 ms. Never change latency during playback via sample loop. When false, board latency is uncompensated (timing offset ~ms) - documented.
- Test T-LAT: impulse through board with N latency pedals bypassed => output impulse at index L_board +- 0, mix 0 and mix 1 align (null at mix 0.5 for all-bypass: out == in*sqrt2 at delay L; check peak positions equal).
- Oversampling inside pedals: use `Source/DSP/Common/Oversampler.h`; minimum-phase vs linear-phase documented per pedal; default linear-phase 2x/4x, latency counted.

## 9. Parameters, automation, MIDI
### 9.1 IDs (snake_case, append-only; never reuse/renumber)
Board: `ref_board_enabled`, `ref_board_mix`, `ref_board_mix_law`, `ref_board_report_latency`, `ref_board_limiter_on`, `ref_board_trails`, `ref_board_mode`, `ref_board_exp1`, `ref_board_exp2`, `ref_board_fb_gain`, `ref_board_fb_lp`, `ref_board_fb_src`, `ref_board_fb_dst`, `ref_board_fb_runaway_ok`, `ref_board_loop1_*`, `ref_board_ab_select`, `ref_board_reamp_*`.
Slot params are fixed host-visible automation lanes (APVTS, appended after all existing params; created once at construction): per slot nn=01..16: `ref_s{nn}_enabled` (bool, == BoardModel `enabled`, the pedal footswitch), `ref_s{nn}_p{kk}` kk=01..16 (float 0..1; kMaxLaneParams=16), `ref_s{nn}_in_db`, `ref_s{nn}_out_db`, `ref_s{nn}_pos` (int 0..15, see 9.4). Count = 16*(1+16+2+1) = 320 params. Lane assignment (PedalRegistry builds `LaneMap` per model): walk `PedalDesc.params` in order; SKIP params with `ParamFlag::Engage` (they alias `ref_s{nn}_enabled`), `Config` (structural/quality/variant, state-only), `Ext` (virtual-extension, state+MIDI only) and `Trigger`/`Momentary` kinds; assign the rest to p01.. until p16; leftover automatable params beyond 16 are state-only and still MIDI-learnable. `Hidden` params are never shown. Host names re-labelled dynamically via `AudioProcessor::updateHostDisplay` + `getParameterName` returning "Slot3: Drive" (DAW lane label), IDs stable. Never created/destroyed on RT. Old projects: absent params use default.
### 9.2 Mapping slot param -> model param
Slot `p{kk}` maps through `PedalRegistry::laneMap(modelId)` (sec 9.1 rule: skips Engage/Config/Ext/Trigger/Momentary), NOT by raw descriptor index. Unused p -> hidden, no-op, host name "-". Lane order is append-only per model revision; a revision may only append params (never reorder), so saved automation stays valid.
### 9.3 Smoothing / RT application
Params stored as `std::atomic<float>` in `SlotParamBlock` (fixed array). Audio thread per sub-block (64): load relaxed, push into pedal via `setParam(idx, norm)`; pedal smooths via `SmoothedValue` linear/exp (default 20 ms; discrete = immediate with 5 ms xfade where audible). UI writes through APVTS (gesture begin/end for automation recording).
### 9.4 Order automation
`ref_s{n}_pos` = position of slot n. Setting is a request: message thread collects pending position map each timer tick (30 Hz), resolves conflicts (stable sort by (pos, lastTouchTime)), emits ONE reorder commit. Never read as authoritative: BoardModel->order is; params are mirrors written back (`setValueNotifyingHost` guarded against feedback with `isUpdatingFromModel` flag).
### 9.5 MIDI hooks
Board-level: `ref_board_midi_chan` (omni|1..16), MIDI learn (existing `Source/Support/MidiLearn.*` is the legacy engine-parameter learn and stays untouched; the board uses its own `Midi/MidiLearn` + `MidiMap` of MIDI_EXPRESSION.md) can target any param above. PC (program change) -> board snapshot recall (sec 10.4). Realtime messages 0xF8/0xFA/0xFB/0xFC/0xF2 are consumed by `ClockSync` (MIDI_EXPRESSION sec 3) which publishes `TimeInfo` to pedals via `IPedal::setTime` and START/STOP/CONTINUE/LOCATE via `IPedal::onTransport`; per-tick `onMidiClockTick` REMOVED (pedals derive phase from `TimeInfo.ppq`).
`ClockSync` (Source/ReferencePedals/Midi/ClockSync.*, owned by MIDI_EXPRESSION; the name `BoardClock` is retired): tempo-source rule is MIDI_EXPRESSION 3.1 (`ref_board_tempo_source`: INTERNAL | HOST | MIDI_CLOCK | AUTO; AUTO = HOST playing > MIDI_CLOCK (>=2 ticks in 500 ms) > INTERNAL, 2-beat/1 s hysteresis, 50 ms glide). It reuses the existing `Source/Support/HostClockGuard.h` / engine `hostClock.read(getPlayHead())` for host position instead of calling AudioPlayHead a second time. Per block it fills the single canonical struct `TimeInfo {double bpm; double ppq; bool playing; bool clockLocked; bool valid; int64_t sample;}` (defined in `IPedal.h`). Lock detection 2 beats of stable intervals (jitter < 1%); loss-of-clock after 500 ms -> fall back per 3.1 and set `clockLost`.
Per-pedal MIDI: ONE path. `MidiRouter` (MIDI_EXPRESSION sec 2) resolves bindings to `ParamWrite {uint8 slot; uint16 paramIdx; float norm; uint16 smoothMs; int offset}` and delivers them through `IPedal::setParam(idx,norm)` at sample offset (blocks split at event offsets, max 32 split points/block, min split 16 samples, rest at block end). Discrete gestures arrive as `FootEvent` (footswitch) or as `ParamKind::Trigger` writes (norm 0->1 rising edge = tap/stop/undo/clear/next-memory). `IPedal::onMidi(const MidiEvent&)` is called ONLY for pedals whose `PedalDesc.wantsRawMidi` is true (RC-5 PC/clock-master, DD-3T none) with the canonical `MidiEvent {uint8 status,d1,d2; int offset}`. `setControlSnapshot` (ARCHITECTURE.md draft) is REMOVED.
### 9.6 Velocity / aftertouch emulation of foot press (engine-level)
`FootController` per slot: maps note-on velocity -> `pressSpeed` (0..1) and channel/poly aftertouch -> `pressForce` (0..1, continuous). Modes: Latch (note-on toggles), Momentary (hold = on while key down, release = off; optional minimum hold), Gate-expression (aftertouch drives a param such as wah position or bypass depth). Pedal receives `FootEvent{type: press|release, speed, force, offsetSamples}`; model may use speed (e.g. footswitch debounce click level, bypass xfade length 2..12 ms: fast press = short xfade) and force (soft-touch: partial engage = effect mix). Defaults: bypass xfade ms = lerp(12, 2, speed). `softPressMix` off by default.

## 10. State save / restore
### 10.1 Format (inside existing host blob)
Root key `referencePedals` (add to `isKnownRootKey`):
```json
{"schema":1,"enabled":false,"drawerOpen":false,"window":"drawer|popout","mode":"series",
 "boardStyle":"plywood",
 "mix":1.0,"mixLaw":0,"reportLatency":true,"limiterOn":true,"trails":true,
 "order":["uuid-a","uuid-b"],
 "instances":[{"id":"uuid-a","slot":0,"model":"generic.overdrive","rev":1,"enabled":true,"placement":0,
   "lane":0,"inDb":0.0,"outDb":0.0,"params":{"drive":0.5,"tone":0.5,"level":0.5},"x":0.0,"mounting":"velcro","extras":{}}],
 "lanes":[],"loops":[],"feedback":{"on":false,"src":"uuid","dst":"uuid","gain":0.5,"lpHz":12000},
 "exp":{"1":{"cc":11},"2":{}},"midi":{"chan":0,"map":[]},"unknown":{}}
```
- `params` keyed by parameter-id string (NOT index), normalised 0..1. Missing -> model default. Unknown -> preserved in `extras`.
- `order` authoritative; `instances[].slot` informs APVTS mirror index (host automation lane assignment). On conflict order wins.
- `unknown` retains unrecognised root-level keys; every nested object preserves unknown fields (round-trip test).
- Unknown model id: instance loaded as `MissingPedal` (passes audio through bit-exact, bypassed, keeps state/params, UI "missing model"), never substitute.
- Size cap: 256 KB; loops/audio never inline (looper audio -> content-addressed external asset per ARCHITECTURE.md).
### 10.2 Versioning / migration
`schema` int. Current = 1 (the prototype never persisted any board state, so there is nothing to migrate; the MIDI map block `midi` ships INSIDE schema 1 - MIDI_EXPRESSION sec 11 "schema bump to 2" is VOID). Reader accepts schema <= current; `schema` > current: keep whole block verbatim in `unknown`, board forced disabled (`enabled=false` in memory), notice raised once (re-use state-newer-format mechanism), written back unchanged until user edits board. Migrations are pure functions `migrate_N_to_N+1(json)`; chain runs on message thread, original backed up by existing `state-backup-*` mechanism when schema < current. Fixtures per schema in `Source/Tests/Fixtures/ReferencePedals/ref_board_v{N}.json` (capital F: existing dir). Root key `referencePedals` must be added to `isKnownRootKey` in `Source/HostStateFormat.cpp`; `kCurrentStateFormatVersion` (PluginProcessor.h, =1) is NOT bumped.
### 10.3 Legacy identity
Absent `referencePedals` key => board constructed disabled; `Board::processStereo` early-out BEFORE any buffer touch => bitwise-identical output to pre-feature builds (Test T-LEGACY: render 10 s corpus with factory presets, hash compare to golden). Also board enabled but zero non-empty instances => early-out (zero cost) and mix bypassed. Latency reporting must not change (L_board=0 => setLatencySamples not called with a new value).
### 10.4 Board snapshots / presets
Board preset file = same JSON object (`referencePedals`) + `name`. A/B slot (`slotBActive` existing) not duplicated; board snapshots (8) stored under `referencePedals.snapshots[]`. Program change n -> snapshot n; applied as structural transition (4.2) with parameter smoothing.
### 10.5 Concurrency of save/load
`getStateInformation` (message or host thread, may be called while audio runs): snapshot BoardModel under `BoardModel::mutex` (message-thread mutex, never taken on audio thread); audio never reads BoardModel. `setStateInformation`: parse -> build complete BoardModel off-thread -> `compile` -> publish; atomically replace; partial parse failure => keep previous model and report.
### 10.6 Undo/redo
BoardModel mutations are `BoardCommand` objects: Add, Remove, Duplicate, Move(instanceId,newIndex), SetModel, SetParam(gesture-coalesced), SetEnabled, SetPlacement, SetMode, SetLane, SetLoop, SetFeedback, SetStyle, SetMounting, SetMix/SetMixLaw, SetBoardEnabled, SetMidiBinding (add/remove/edit), SetSlotChannel, SetScene, ApplySnapshot. Stack 100, coalesces drags. Undo = inverse command -> republish (transition). Not in audio thread.

## 11. Pedal library API (IPedal)  [FROZEN INTERFACE I-1, see IMPLEMENTATION_PLAN sec 2]

**Proposed WIP erratum (2026-10-31; not yet accepted):** A freshly prepared
pedal may report `extraOverridesInitialParam(index)` on the message thread
after its initial params, `prepare`, and `loadExtra`. The topology compiler
uses this only for a newly created unpublished runtime to avoid replaying that
same initial parameter over the loaded extra-state value. The default is false.
Subsequent explicit model and host writes, including writes before the first
audio block, must still be applied. Reused or already published runtimes are
never mutated on the message thread. This does not provide a pedal-to-host
parameter echo; that separate feature and full tests remain open.
Located `Source/ReferencePedals/IPedal.h`, `PedalDesc.h` (juce_core/juce_audio_basics only; NO juce_gui). Distinct from legacy `Pedal`/`PedalType` (not modified), but descriptors reuse `PedalParam` field semantics (min,max,default,skew,discrete) so tapers match. Namespaces: core `luthier::reference`; generic pedals `luthier::reference::generic`; branded models `luthier::reference::models` (the `ReferencePedals::Models` spelling in per-model specs is VOID).
```cpp
namespace luthier::reference {
constexpr int kMaxParams = 32;      // descriptor capacity per pedal (state, MIDI-learn reach)
constexpr int kMaxLaneParams = 16;  // host-automation lanes ref_s{nn}_p01..p16 (sec 9.1)
enum class ChannelMode : uint8_t { Mono, Stereo, TrueStereo, MonoInStereoOut };
enum class BypassMode  : uint8_t { True, Buffered, DSP };
enum class ParamKind   : uint8_t { Continuous, Discrete, Toggle, Momentary, Trigger, Tempo, Enum };
enum class Taper       : uint8_t { Linear, LogA, LogB, ReverseLog, Skew, Detent, Custom };
enum ParamFlag : uint16_t { PF_None=0, PF_Engage=1, PF_Config=2, PF_Ext=4, PF_Hidden=8,
                            PF_Automatable=16, PF_ExprTarget=32, PF_TempoSync=64, PF_OwnSmoothing=128 };

struct ParamDesc {
  const char* id;           // snake_case, MUST start with PedalDesc.paramPrefix
  const char* label;        // front-panel text (original wording; physical label names)
  const char* unit;
  ParamKind kind; Taper taper;
  float min, max, def, skew;
  int   steps;              // discrete count (Enum/Discrete), else 0
  const char* const* choices;   // Enum labels, steps entries
  uint16_t flags;           // ParamFlag bitset
  float smoothMs;           // default 20
  int16_t midiCcDefault;    // pedal-LOCAL profile CC (-1 none), see MIDI_EXPRESSION 9
};
struct PedalDesc {
  const char* modelId;      // "generic.overdrive", "boss-ds1"  (hyphens allowed; immutable, serialised)
  const char* paramPrefix;  // == modelId with '.'/'-' -> '_' EXCEPT generic pedals (gen_<short>_); trailing '_' included. Unique registry-wide.
  uint16_t revision;
  const char* displayName;  const char* family;  const char* inspiredBy; // inspiredBy nullptr for generic
  ChannelMode ch; BypassMode bypass;
  int numParams; const ParamDesc* params;       // static storage
  int maxLatencySamples; bool hasTails; bool oversampled;
  bool wantsRawMidi;        // true only if onMidi() is needed (sec 9.5)
  uint32_t cpuUnits;        // [MEAS] vs performance-budget.md units
};
struct TimeInfo   { double bpm = 120; double ppq = 0; bool playing = false; bool clockLocked = false; bool valid = false; int64_t sample = 0; };
struct MidiEvent  { uint8_t status, d1, d2; int offset; };          // canonical raw event (router-internal MidiEv converts)
struct FootEvent  { bool press; float speed; float force; int offset; };  // speed/force 0..1
enum class TransportEvent : uint8_t { Start, Stop, Continue, Locate };
struct PedalServices { class IAssetStore* assets = nullptr; uint32_t instanceSeed = 0; int maxBlock = 0; double sampleRate = 48000; };

class IPedal {
public:
  virtual ~IPedal() = default;
  virtual const PedalDesc& desc() const noexcept = 0;
  // message thread, may allocate. Called on prepareToPlay and sampleRate/blocksize change.
  virtual void prepare (double sampleRate, int maxBlock, int oversamplePolicy) = 0;
  virtual void setServices (const PedalServices&) {}            // message thread, before prepare()
  virtual void reset() noexcept = 0;                              // clears state, RT-safe
  // audio thread. In place. n <= maxBlock. noexcept, no alloc/lock/log/IO.
  virtual void process (double* L, double* R, int n) noexcept = 0;
  virtual void setParam (int idx, float norm) noexcept = 0;       // norm 0..1, applies taper, smooths; Trigger kind: rising edge fires
  virtual float getParam (int idx) const noexcept = 0;
  virtual void  setEnabled (bool on) noexcept {}                  // pedal-internal engage (trails state machine); BoardModel.enabled is truth
  virtual int   getLatencySamples() const noexcept { return 0; }
  virtual float getTailEnergyDb() const noexcept { return 0.f; }  // <= -90 => board may skip process()
  virtual void  setTime (const TimeInfo&) noexcept {}             // once per block, before events
  virtual void  onTransport (TransportEvent, int offset) noexcept {}
  virtual void  onMidi (const MidiEvent&) noexcept {}             // only if desc().wantsRawMidi
  virtual int   drainMidiOut (MidiEvent* out, int cap) noexcept { return 0; }  // RC-5 clock master; audio thread
  virtual void  onFoot (const FootEvent&) noexcept {}
  virtual void  setExpression (int jack, float v) noexcept {}
  virtual bool  isStateless() const noexcept { return false; }
  virtual bool  outputsDualMono() const noexcept { return false; }
  virtual void  saveExtra (juce::var& out) const {}               // message thread; non-param state (e.g. tap time, loop asset refs)
  virtual void  loadExtra (const juce::var& in) {}
};
using PedalFactory = std::unique_ptr<IPedal>(*)();
class PedalRegistry { public:
  static void registerAll();                                   // explicit list, idempotent (sec 11 bullets)
  static bool add (const PedalDesc&, PedalFactory);            // false = invalid descriptor or duplicate id/prefix (E25)
  static std::unique_ptr<IPedal> make (const char* modelId);   // message thread only
  static const PedalDesc* find (const char* modelId);
  static juce::Span<const PedalDesc* const> all();   // C++17: juce::Span, not std::span (E25)              // order: generic (GENERIC_PEDALS order), then models (catalog order)
  static const LaneMap& laneMap (const char* modelId);         // sec 9.1 lane assignment
};
}
```
- Engage polarity: any param flagged `PF_Engage` (specs call them `*_on`, `*_bypass`, `*_active`, `*_fs`, `*_footswitch`) is NOT stored in the pedal: registry maps it to the slot `enabled` (value 1 = effect engaged; a spec that names it `_bypass` means "footswitch", default engaged). It never consumes a lane. Secondary footswitches (Muff `mids_on`, `bass_boost`, `tone_in`, RC `pedal` selectors) are ordinary Toggle params.
- Slot-level trims (`ref_s{nn}_in_db`/`out_db`) replace per-pedal `*_input_trim_db` / `*_output_trim_db` / `*_in_trim` (remove them from pedal ParamDesc lists; DS-1, TS808, Deluxe Muff, SD-1, RAT, Green Russian, D+ specs listed them: treat as slot trims). Calibration constant "0 dBFS = ref_vpk V" is a model-internal constant, not a param, unless the spec marks it Config.
- `PF_Config` (variant/revision/quality/hq/stereo_mode/noise/os_factor/bits): structural, applied via `prepare()`/structural transition, never lane-mapped, saved in `params`.
- Base helper `PedalBase<N>` (Source/ReferencePedals/PedalBase.h): owns `float norm[N]`, `SmoothedValue` array, calls `onParamChanged(idx, value)`; provides `Dc/Soft guards`; implements `setParam` clamp+isfinite, Trigger edge detection, taper conversions (`fromNormalised`: `n^(1/skew)` when skew!=1).
- MIDI hooks: `ParamDesc.midiCcDefault` is the pedal-LOCAL CC profile (MIDI_EXPRESSION sec 9): active only for the slot whose `midi_chan` is set or that has MIDI focus; user remap through board MidiLearn. CC to param: value/127 -> norm via param taper; Toggle >=64; Trigger rising edge.
- Footswitch-state authority: BoardModel `enabled` is truth; pedal.setEnabled mirrors; UI LED reflects model + audio `tailActive` read-back atomic.
- Dispatch order per block: (1) apply param snapshot, (2) `setTime`, (3) transport/foot/param events (sorted, split sub-blocks), (4) process lanes.

## 12. Thread model
| Thread | Allowed | Forbidden |
|---|---|---|
| Message/UI | BoardModel edit, compile, publish, alloc, pool create, reclaim, file IO, state | holding audio-shared locks |
| Audio | read atomics/live ptr, process, write meters (`std::atomic<float>`), push `SpscRing` events to UI | alloc, free, mutex, log, file, juce::String, std::function, exceptions, `std::sin` heavy per-sample w/o table |
| Worker (optional) | looper disk IO, IR load, big table gen | touching RT structures |
- UI->audio: params via atomics; structural via Topology pointer; momentary events via NEW `Source/ReferencePedals/Rt/SpscRing.h` (wait-free SPSC, fixed power-of-two capacity, a full ring drops the NEW item and counts it, E25). NOTE: existing `Source/Support/CommandQueue.h` is NOT usable for audio->UI (its `push` takes a SpinLock) and is not used by the board; it stays message->audio only for legacy EngineCommand.
- Audio->UI: meters, tail state, clock lock, fb_tripped via atomics/SPSC; UI polls 30 Hz.
- Memory order: publish release/acquire; params relaxed; version epoch release.
- Denormals: `ScopedNoDenormals` in engine process; plus explicit flush in feedback paths (`if (fabs(s)<1e-30) s=0` per block).

## 13. CPU / alloc budget
- Allocation: 0 bytes on audio thread (verify with allocator hook in test; existing `CodexQA_RtSafety.cpp` pattern).
- CPU [EST] at 48k stereo double, 16-sample..2048 blocks: board overhead (mix, routing, trims, limiter) <= 0.5% of one core; each generic pedal <= 1.0% (reverb 2.5%, oversampled drive 2x/4x 1.5%); whole 8-pedal board <= 8% total. [MEAS] to replace in `spec/performance-budget.md` units.
- Enforcement: each pedal exposes `cpuUnits`; BoardModel sums; adding pedal that exceeds `ref_board_cpu_cap` (default 30% of measured audio budget via `CpuLoadMonitor.h`) is blocked with message ("Board CPU budget reached") rather than silently degrading. Runtime: `CpuLoadMonitor` > 85% for 2 s -> auto-switch pedals to `quality=Eco` (2x->1x oversampling) via structural transition, notify user; revert not automatic.
- Preallocation: PedalPool 16 objects each with max buffers sized at prepare (sum <= 64 MB; delay up to 2 s*SR*2ch*8B=1.5 MB@48k per delay, reverb 1 MB; looper separate).
- Block scratch buffers: kMaxLanes*2*maxBlock doubles preallocated.

## 14. UI contract (engine-facing)
- Strip shows `Topology.order` left->right; arrows between pedals; current segment/placement tags; AMP marker.
- Reorder inputs: drag handle (mouse/touch), Alt+Left/Right (keyboard), context menu Move Left/Right/First/Last, MIDI/automation (`ref_s{nn}_pos`). All route to `BoardCommand::Move(instanceId, newIndex)`.
- Drawer collapse/expand and pop-out window: separate view component bound to the same BoardModel; `window` state saved. Closing the window or collapsing never changes audio. Footswitch/knobs reachable at 8..16 slots via horizontal scroll with auto-scroll-to-focused.
- LED/footswitch state read from BoardModel + audio read-back (tail active). Board mix knob, board bypass, mode selector fixed in toolbar (not scrolling).
- Board style = `plywood|velcro|clamps|routed|rail|ceramic|loose` (7, UI_BOARD_STYLES sec 5; "metal framework" = `rail`, "metal clamps" = `clamps`); per-instance mounting = `screws|velcro|clamps|routed|rail|ceramic|none`. Presentation only: `boardStyle`, `mounting` saved but zero audio effect (exception: `loose` may enable cosmetic jitter animation only).

## 15. Test plan (acceptance = all pass, deterministic with noise off)
| ID | Test | Pass |
|---|---|---|
| T-LEGACY | board absent/off, 10 s corpus, factory presets, 44.1/48/96k | output hash identical to pre-feature golden (bitwise) |
| T-LEGACY2 | classic rack on + board bypassed (mix 0 / enabled=false) | null vs board absent < -200 dB (exact) |
| T-ORDER | chain [A,B] vs [B,A] with OD+delay | differ > -30 dB; equals manual cascade of reference pedals processed in that order (null < -120 dB) |
| T-REORD-ID | move instance X; params/tail state follow X | state preserved (delay tail continues), identical params |
| T-XFADE | reorder every 50 ms for 30 s on sine 1 kHz -0.1 dBFS | max sample step |dy| <= 1.5x max natural step; no click > -60 dBFS in HP-filtered output |
| T-BLK | block sizes 1,16,64,512,2048, non-pow2 (37,481) | outputs equal within 1e-9 abs across all (reorder commit at same sample) |
| T-MIX | mix 0 / 0.5 / 1; all-bypass | m=0: out == dryD exactly; m=1: out == wet exactly; m=.5 EqualPower correlated +3.01 dB +-0.01; uncorrelated noise power-preserving +-0.1 dB |
| T-LAT | pedals with latency 0/37/128 bypassed & enabled | impulse position == L_board for any enable state; mix path aligned |
| T-LAT2 | reorder changes no total latency | L_board const |
| T-PAR | parallel lanes with different latency | lanes aligned (cross-correlation peak at 0 lag +-0) |
| T-AB | A/B switch | no click, equal-power through 10 ms |
| T-LOOP | send/return serial/parallel/kill | nulls match analytical sums |
| T-FB | feedback g=0.98, 1.05+runaway; step input; NaN injected | bounded |y|<=ceil*1.01; watchdog trips; NaN -> recovers silent, flag set |
| T-STATE | save/restore randomized boards (fuzz 1000 seeds, 0..16 pedals) | JSON equal; audio equal after restore (bitwise for deterministic models) |
| T-MIGR | fixtures v1..current; schema+1; unknown keys; missing model | migrate correct; unknown preserved byte-wise on rewrite; MissingPedal passes audio exactly |
| T-AUTO | automate `ref_s*_p*` and `_pos` from host sim 1k events/s | no crash, no zipper (max param-induced step < threshold), order consistent |
| T-RT | allocator hook + lock detector over 10 min with random edits at 100 Hz | 0 allocs/locks on audio thread; TSan clean |
| T-RETIRE | publish storm 1000/s, ASan | no UAF, retired lists bounded <= 8 |
| T-CPU | 8 and 16 pedals, all models, 48/96k | within sec 13 budget; cap enforcement triggers |
| T-MIDI | MIDI clock 24ppqn at 60..200 BPM with jitter, host bpm | lock within 2 beats; tempo error < 0.5% ; loss fallback |
| T-NAN | Inf/NaN/denormal/+-1e6 input, params fuzz | output finite, within +-16 |
| T-SOAK | 30 min random footswitch/reorder/param/model swap, sample rate change | no xrun in offline sim, no leak, memory flat |
| T-UI | keyboard reorder, screen-reader names, drawer vs pop-out parity | model==view after each action |
Existing test files `ReferencePedalBoardTests.cpp`, `ReferencePhraseLooperTests.cpp` updated to new API; keep old assertions (order CAS semantics now via compile).

## 16. Implementation plan (files, modular)  (authoritative file ownership: IMPLEMENTATION_PLAN.md sec 4)
```
Source/ReferencePedals/                    (engine side: juce_core/juce_audio_basics/juce_dsp only; also compiled into LuthierRender)
  IPedal.h  PedalDesc.h  PedalBase.h  PedalRegistry.{h,cpp}
  BoardModel.{h,cpp}  BoardCommand.h       // msg thread truth, commands, undo
  TopologyCompiler.{h,cpp}
  BoardEngine.{h,cpp}                      // RT executor: transitions, lanes, mix, fb
  WetDryMixer.h  DelayLine.h  BoardLimiter.h  FootController.h
  BoardState.{h,cpp}                       // JSON <-> model, migrations
  Board.{h,cpp}                            // FACADE owned by LuthierEngine (replaces prototype Board.h; legacy API shims)
  Rt/SpscRing.h  Rt/RetireList.h
  Services/AssetStore.{h,cpp}              // looper content-addressed external assets
  Midi/  (MIDI_EXPRESSION sec 1 files; ClockSync replaces BoardClock)
  Common/ (shared DSP: BbdCore, LooperCore, GestureDecoder, FractionalDelay, Biquad, OnePole, TptSvf, Lfo, EnvelopeFollower, SoftClip, NewtonClipper, ...)
  Generic/*.h                              // GENERIC_PEDALS.md
  Models/<snake_id>.{h,cpp}                // branded models, one pair per pedal (snake_id = modelId with '-' -> '_')
Source/UI/ReferencePedals/                 (UI side; excluded from engine/renderer targets by the existing Source/UI/ CMake filter)
  see UI_BOARD_STYLES sec 17; pedal faces: Faces/<snake_id>_face.cpp (procedural juce::Path; NO .svg/bitmap assets)
```
Phases: P1 IPedal+PedalRegistry+BoardEngine(series)+mix+state+T-LEGACY/ORDER/MIX/XFADE; P2 latency+lanes+loops+AB; P3 feedback+safety; P4 params/automation/MIDI/ClockSync; P5 generic pedals; P6 UI drawer+pop-out+styles; P7 stress/QA/audit.
Prototype `Board` API compatibility: `setModel/setKnob/move/getIdAtPosition/setPlacement/setSlotEnabled` remain as shims (3 knobs -> p01..p03) until tests migrate; ts808/ds1/phase90 prototypes become `Prototype` models registered under same ids with their existing math.

## 17. Open items
O1 Is a postCab tap present in engine graph (verify Cabinet/Room order in `LuthierEngine.cpp`).
O2 Sidechain reamp: confirm bus enable flow; default off.
O3 Latency reporting policy vs host: decide default `report_latency` after DAW testing (Reaper, Logic, Ableton, Bitwig).
O4 Equal-power default confirmed by product owner (+3 dB coherent bump).
