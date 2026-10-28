# Six-pedal continuation — 2026-10-28

Session: Claude cloud session, branch `claude/quirky-darwin-eoesyi` (draft PR #83,
base `claude/luthier-pedalboard-integration` = PR #82). Master untouched.
This adds to [the coordinator map](SIX-PEDAL-COORDINATOR-MAP-2026-10-27.md); it
does not replace the acceptance ledger or any original gate.

## Reconciliation (2026-10-28 01:45 UTC)

| Item | Finding |
|---|---|
| PR #82 | draft, head `25cd2135218a2974244988404fb209cf684a1f0d` on `claude/luthier-pedalboard-integration` (not the `-checkpoint-` branch) |
| Checkpoint | `claude/luthier-pedalboard-checkpoint-1e7aaf1c` = `1e7aaf1c939d…`, ancestor of `25cd2135` (integration = checkpoint + 1 CI/docs commit). Not re-imported. |
| Merge base with master | `0f62c3d67bdba055b41e66c7ab8ac9ab4b728662` (master unchanged; integration 324 ahead / 0 behind) |
| Newer remote work | none: no remote branch newer than `25cd2135`; newest codex branches are 2026-10-26 (`codex/luthier-checkpoint-20261026-qa2` f1efeded, an ancestor). |
| Worker lanes | `claude/luthier-pb-{phase90,dd3t,chorus,dyna,rc5,looper-core}` named in the map were **never pushed**. The previous container is gone, so the returned Phase90/CE-2 proposals are not recoverable from any ref. Lanes continue in this session (single writer); the "CE2 BBD-mode calibration proposal (Reserved: Claude broker)" is superseded by slice 2 below. |
| Codex ownership | no Codex branch/PR activity after 2026-10-26; PR #80 (`codex/luthier-pedalboard-completion`) is an ancestor. No competing integrator started; results here are prepared for the canonical PR #82 owner. |
| Dirty files at start | none (fresh clone). |

## Slice 1 — Windows MSVC lookup (commit `97d3b013`)

Run 40603478203 (synthetic merge `0054fdcf`) failed Windows Free+Pro with C2653/C2065
at `ReferencePedals_mxr_phase90_scriptTests.cpp:172`: MSVC does not apply a function's
`using namespace` inside a local class's default member initializer. Fix: fully
qualified `::luthier::reference::ChannelMode::Stereo` / `BypassMode::True`. Test-only.
Native verification: `build.yml` dispatched (windows, pro+free, LTO off, strictness 10)
on exact commit `97d3b013`: run 40617409778. Original failing logs remain in run 40603478203.

## Slice 2 — CE-2 BBD-on THD gate (RED → GREEN on Linux)

Root cause (measured): at −12 dBFS the CE-2 input shaper is linear (onset 0.5) and the
~150 kHz BBD clock is far above host rate, so the clocked ZOH adds no harmonic distortion.
The model had no BBD transfer nonlinearity at all; baseline BBD-on THD 0.0003–0.0097 %
(reproduced exactly by the standalone probe) vs spec §9 0.5–3 %.

Change: `common::BbdCore::setTransferNonlinearity(even, odd)` — clocked mode only,
`x += even*(x²−⟨x²⟩₅Hz) − odd*x³`; default 0 leaves output bit-identical. Only CE-2 sets it:
`even=.02, odd=.03` (ESTIMATED; no hardware capture). The odd term keeps spec §9's −3 dBFS
clause (2–6 %, 3rd harmonic dominant) true with BBD on as well.

| Rate | BBD-on −12 dBFS | BBD-on −3 dBFS (h2 / h3) | BBD-off (unchanged) |
|---|---|---|---|
| 44.1k | 1.0825 % | 4.752 % (2.891 / 3.656) | −12: 0.0000 %, −3: 2.683 % |
| 48k | 1.0465 % | 4.589 % (2.796 / 3.527) | −3: 2.583 % |
| 88.2k | 0.9350 % | 4.117 % (2.502 / 3.168) | −3: 2.268 % |
| 96k | 0.9234 % | 4.077 % (2.479 / 3.136) | −3: 2.250 % |
| 192k | 0.8982 % | 3.973 % (2.412 / 3.057) | −3: 2.179 % |

New test `RefPedalCe2.bbdOnHeadroomGateKeepsThirdHarmonicDominant` encodes the −3 dBFS
clause with BBD on. Thresholds unchanged. Bit-exact A/B (sha256 of full outputs, 4 rates ×
3 blocks × 4 settings): CE-2 BBD-off and CE-2W all modes incl. clocked staircase identical
(`4d60f37b…`).

Still open for CE-2: raw on/off jump (RED 12/60 cells, max 0.0014990 > 0.001, unchanged;
not touched), CPU <0.4 % / <1.0 % (bbd) not yet measured as a gate.

## Slice 3 — exact-output CPU reductions (gates still RED)

- Phase90: stage-major interleave of the two channels' serial allpass/sqrt/divide chains
  and `[stage][channel]` state layout. Full output bit-identical (sha256 `b14c2aee…` over
  4 rates × 3 blocks × 6 settings × both render entries, incl. bypass toggles); probe
  FR/THD lines identical. Ablation: coefficient updates ≈ 0.10–0.15 %, the 4-stage
  sqrt/divide latency chain ≈ 0.32 %. Original probe `phase90-e58-actual.cpp`, GCC 13.3
  `-O3`, this 2.1 GHz Xeon (noisy): base 0.465–0.88 %, candidate 0.419–0.78 %
  (quiet trials ~0.43/0.49 % at stereo phase 0/180). **<0.4 % remains RED.**
- Dyna Comp: exact-value caches (release coefficient per channel, noise gain) and hoisted
  constants. Full output bit-identical (sha256 `7361d5ca…` over 5 rates × 4 blocks ×
  3 policies × 6 settings). Probe (48k/128, 10 s): base 1.06–1.91 %, candidate
  0.93–1.78 %. Spec: ≤0.5 % of one **3 GHz** core (≈0.71 % here if linear). **RED.**
  Ablation: 2× oversampling doubles the OTA-law cost; removing OS alone gives ≈0.47–0.55 %.

Owner decisions needed to close these CPU gates (not taken here): Phase90 spec names no
reference CPU — confirm the reference machine, or approve non-bit-exact math (e.g. a
reciprocal-sqrt form); Dyna — approve a cheaper 2× law evaluation that changes low-order
bits, or qualify on the named 3 GHz class.

## CE-2W finding (no change)

Standalone probe (depth 0, 1 kHz): STANDARD −10 dBFS 0.078–0.083 % (passes <1 %).
CE-1 chorus 0 dBFS: output A 0.42 %, output B 1.05–1.13 %; CE-1 vibrato 0.81 %.
Spec "CE-1: 1..3 % at 0 dBFS EST" does not name the output or depth; A and B differ ~3×
due to dry/wet geometry. Sweeping the CE-1 colour scale (4.5 → 2.75) and moving it pre-split
cannot put A, B and vibrato all in 1–3 %. Needs an owner/spec decision on which output the
CE-1 THD criterion applies to. DSP unchanged.

## Six-model status at this checkpoint

| Model | DSP | Registered | Open original gates |
|---|---|---|---|
| Phase90 | integrated | no | CPU <0.4 % RED; 24-bit click criterion unresolved |
| CE-2 | integrated | no | BBD THD now GREEN; raw jump RED; CPU unmeasured as gate |
| DD-3T | integrated | no | CPU <0.5 % gate open |
| CE-2W | integrated | no | CE-1 THD criterion ambiguous (above); CPU |
| Dyna Comp | integrated | no | CPU RED; THD/attack original failures (90 cells) retained |
| RC-5 | wrapper v1 (slice 4) | no | memory switching, quantize/auto-length, MIDI PC/CC map, half-speed, live reverse toggle, sanitizer/soak/native |

Registration stays gated on original thresholds (ledger policy); 0/6 registered.

## Slice 4 — RC-5 model wrapper v1 (`Models/boss_rc5.{h,cpp}`)

Was an empty skeleton on every branch. Now a real `IPedal` composed from the existing
tested components (`PagedLooperCore`, `Rc5PressCycle`, `Rc5Rhythm`, `LooperStateCodec`):

- Descriptor: 23 params in spec §8 order; the 13 host lanes match the reconciled header
  exactly; pedal1..3 Momentary; lofi/ram_cap PF_Config; sync/quantize/exp/clock_out PF_Ext.
  Unregistered (`registerRc5()` unchanged).
- Looper: one stereo paged track (5400 s, undo depth 1, ≥2 s read-ahead, RAM budget =
  `ram_cap_mb`) on the board's asset store. No store → looper unavailable, dry+rhythm
  still run, `noAssetStore` fault bit, gestures ignored.
- Main pedal (onFoot / pedal1): REC→PLAY→OVERDUB→PLAY; double press ≤ dtap_ms stops (and
  cancels a provisional pass); hold ≥ hold_ms in PLAY/OVERDUB = undo, again = redo; hold in
  STOP = clear. A double-press STOP never arms the clear. CTL1 = play/stop, CTL2 = undo/redo
  (plugin defaults, ESTIMATED per spec).
- Overdub across passes: a paged layer cannot be rewritten after its first pass (page
  already flushed → `readUnderrun|writeOverrun`, auto-close; found by test). The wrapper
  closes each pass at the wrap and opens the next at that sample, so undo removes the last
  pass (spec §2.2 "single level (last pass)"). Each closed pass retires the previous one and
  the worker folds it into the base asynchronously; at real-time pace this keeps up
  (test), faster than real time it can exhaust streams (writeOverrun, overdub stops, audio
  kept). While a hold from PLAY is pending, the provisional pass is cancelled at the wrap
  instead, so a hold never consumes the undo level; released before hold_ms → overdub resumes.
- Loop level 0..200 → gain 0..2 (20 ms smoothing, 32-frame steps), dry mute 20 ms ramp,
  rhythm pattern/variation/kit/level/on, tempo from host clock in SYNC AUTO else the param
  (clamped 40..300), lofi 20 Hz/20 kHz one-poles, EXP to loop/rhythm level or tempo.
- User data: `reset()` stops but keeps the loop; re-`prepare()` at the same rate restores
  it; at a new rate (no resampler yet) the completed loop is kept and re-saved unchanged.
  `saveExtra` overlays only `extras.looper` (codec schema 2); an incomplete capture re-emits
  the last saved loop instead of dropping it. `loadExtra` before or after prepare restores
  Stopped; decode failures keep the previous state.

Tests (`ReferencePedals_boss_rc5Tests.cpp`, 11 new): descriptor/lanes, exact record length +
float32-exact playback at 44.1/48/96 k, loop level ±0.1 dB with dry unchanged, double press,
hold undo/redo/clear incl. the one-sample-short boundary, real-time multi-pass overdub and
last-pass undo, no-store fallback, sync tempo, state round trip in both load orders,
reset/re-prepare preservation, zero callback allocations. `RefPedalBossRc5`: 13 tests /
25,635 checks, stable ×3; with RC-5/looper suites 120 tests / 109,340 checks pass.

Not done (RC-5 stays unregistered): 99-memory switching (`Rc5MemoryBank` exists, no audio
handoff), MIDI PC/CC80-87 (`wantsRawMidi` false), quantize/auto loop length, tap tempo
button, half-speed (param present, no effect), live reverse toggle (core accepts direction
only when stopped; applied at next stop), MIDI clock out, sanitizer/30-min soak, 90-min spill
test, native host validation.
