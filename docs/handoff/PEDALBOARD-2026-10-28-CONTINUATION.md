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

## Slice 2 — CE-2 BBD-on THD gate: candidate applied, then REVERTED (gate stays RED)

Root cause (measured): at −12 dBFS the CE-2 input shaper is linear (onset 0.5) and the
~150 kHz BBD clock is far above host rate, so the clocked ZOH adds no harmonics; the model
has no BBD transfer nonlinearity. BBD-on THD 0.0003–0.0097 % vs spec §9 0.5–3 % [E].

Commit `4dd4adc2` added an opt-in clocked-mode term in `BbdCore`
(`x += even*(x²−⟨x²⟩) − odd*x³`, CE-2 even=.02/odd=.03), measured 0.90–1.08 % at −12 dBFS
and 3.97–4.75 % (3rd dominant) at −3 dBFS. It was **reverted in the next commit** after
reading `PEDALBOARD-2026-10-27-REVERSE-DSP-GATES.md`: an independent DSP review had already
ruled that fitting a cubic/tanh to meet the floor is unsupported without "aggregate
conditioned transfer/harmonic data or a defensible explicit estimated behavioral
rationale". The only rationale found does not support it:

- MN3007 THD is quoted as 0.5 % typ / 2.5 % max at Vi = 0.78 Vrms (forum-quoted datasheet
  figures: diystompboxes.com MN3007/MN3207 thread; the Panasonic PDF was not read).
- With spec §3's 0 dBFS = 1.0 V pk, −12 dBFS ≈ 0.18 Vrms; second-order BBD distortion that
  scales with level gives ≈ 0.1 % at the BBD, before dilution by the 1:1 dry path.
- The fitted coefficients therefore needed ≈ 9× the datasheet typical.

So the spec's [E] 0.5–3 % at −12 dBFS appears inconsistent with datasheet-scale evidence
unless the CE-2 drives the BBD well above unity (unsourced). **Owner decision needed:**
revise the [E] threshold, supply level/transfer data, or approve an explicit estimated
rationale. Source is back to `97d3b013` for BbdCore/CE-2 (bit-identical); the gate test is
the original known RED.

Kept: `RefPedalCe2.bbdOnHeadroomGateKeepsThirdHarmonicDominant` encodes spec §9's −3 dBFS
clause (2–6 %, 3rd dominant) with BBD on; it passes on the unmodified model
(2.02–2.42 %; 192 kHz margin is small, 2.015 %).

Still open for CE-2: raw on/off jump (RED 12/60, max 0.0014990 > 0.001). Analysis: the
metric is the max sample-to-sample step inside the toggle window; the spec's own 5 ms
equal-power slope (π/2 over 0.005·fs samples) times |wet−dry| (~0.14 for the fixture's
20/31 Hz inputs) is ≈ 0.0010–0.0015 at 44.1/48 kHz, halving at higher rates — i.e. the
criterion conflicts with the specified 5 ms law rather than indicating a click. Owner
ruling needed on the jump definition; the fade law was not changed.

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
| CE-2 | integrated | no | BBD-on THD RED (owner: spec vs datasheet); raw jump RED (owner: definition); CPU ~0.66 % contended |
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

## Dyna Comp spec §9 probe (current model, standalone, 1-sample blocks)

| Gate | Spec | Measured (44.1/48/96 k) | Status |
|---|---|---|---|
| #6 THD 1 kHz −20 dBFS, SENS 0.5 | 0.1–1 % | 0.183–0.184 % | pass |
| #6 THD at max SENS | "up to 3–5 %" [E] | 0.017 % | RED |
| #4 attack, 63 % of GR, −30→−10 dBFS, FAST | 5 ms ±50 % | 1.22–1.29 ms | RED |
| #4 release, 63 %, −10→−30 dBFS | ~1 s ±50 % | 1.247 s | pass |

SLOW equals FAST on the default M102 variant by design (the attack switch is Mini-only).
The model uses the documented 5 ms as the detector RC; in this feedback topology the
measured GR attack is ~4× faster. A detector sweep gives 2.33/2.88/3.40/3.94 ms at
15/20/25/30 ms, so meeting §9 #4 needs ≈ 40 ms (8× the documented figure, whose spec text
says "measurement method unknown"). Not changed.

## Owner decisions needed (blocking original gates; nothing here was relaxed)

1. **CPU reference machine.** Phase90 (<0.4 %), CE-2 (<0.4 %/<1 %), DD-3T (<0.5 %), Dyna
   (≤0.5 % "on 3 GHz") all measure RED on this loaded 2.1 GHz Xeon (contended indicative:
   Phase90 0.42–0.78 %, CE-2 ~0.66 %, CE-2W ~1.5 %, DD-3T ~0.9 %, Dyna 0.93–1.78 %).
   Name the qualifying CPU class, or approve non-bit-exact optimizations.
2. **CE-2 BBD-on THD** (0.5–3 % [E] vs datasheet-scale ~0.1 %): revise, supply data, or
   approve an explicit estimated rationale.
3. **CE-2 raw jump** definition vs the specified 5 ms equal-power law.
4. **CE-2W CE-1 THD 1–3 %**: which output (A, B, vibrato) and depth.
5. **Dyna attack**: is "5 ms" the detector RC (current) or the §9 #4 63 % GR time?
   **Dyna max-SENS THD 3–5 %** [E]: requires a topology change (sensitivity as pre-OTA gain).
6. **Phase90 24-bit click criterion** (unresolved since 2026-10-26).
7. **RC-5 memory switching** needs a message-thread hook for an off-audio `restore`
   (frozen IPedal/board interface, WP1 errata) before 99-memory audio selection can work.

## Sanitizer evidence (source `0b5af0c3`, Linux x64, clang 18 `-fsanitize=address,undefined`, Release)

`ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:abort_on_error=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`;
`LuthierTests RefPedalPhase90 RefPedalCe2 RefPedalCe2w RefPedalDd3t RefPedalDyna RefPedalBossRc5 RefPedalRc5 RefPedalChorusMemory RefPedalBbdCore RefPedalPagedLooper`:
**129 tests / 199,110 checks, 123.3 s; only failure = known RED `RefPedalCe2.bbdOnOriginalThdGate`
(5 checks); no ASan/UBSan/LSan report.** Environment note: sanitizer runtimes needed
`libclang-rt-18-dev` (link failed without it; not a code issue). TSan not run.

## Slice 5 — paged looper REC→PLAY origin underrun (real defect, fixed)

Found by a focused TSan harness (RC-5 wrapper + paged core + AssetStore + juce_core built
with `-fsanitize=thread`, real-time pace, concurrent message-thread `saveExtra`/capture):
**TSan reported nothing** (4,408 concurrent saves), but the run ended with `readUnderrun`.
Reproduced without TSan and at core level: at real-time pace the worker finishes page 0's
write and `LooperStorage::freeWrite()` reuses that slot for page 1, so after REC→PLAY the
origin page is only on disk → 256–513 frames (5–11 ms) of silence at the start of the first
pass + sticky `readUnderrun`, for any loop longer than one page. Unpaced tests never hit it
because the worker lagged.

Fix: while recording (not sparse layers), the slot holding page 0 is a last-resort choice,
so the origin stays resident. New `RefPedalPagedLooper.realtimeRecordThenImmediatePlay…`
(RC-5 geometry, 0.3/1/4.3 s): RED without the fix (silence + underrun), GREEN with it.
Wrapper probe: 0.3/1/3/10 s loops now start at frame 1 with faults 0.
Full `RefPedal`: 527 tests / 12,400,451 checks; only failure = known CE-2 BBD THD RED.
Whole-target TSan cannot link (the harness overrides global operator new; conflicts with the
TSan runtime) — focused harness used instead.

## Native Windows evidence — build.yml run 40617409778, exact commit `97d3b013` (MSVC 14.51, VS 18, LTO off, pluginval strictness 10)

| Step | Free | Pro |
|---|---|---|
| Configure and build (Windows) | pass | pass |
| Early release regression tests (Windows Pro) | — | pass |
| Unit tests (Windows) | **FAIL** | **FAIL** |
| pluginval + clap-validator (Windows) | pass (CLAP 21 run / 17 pass / 0 fail / 4 skip) | pass |
| Stage / upload plugins | pass | pass |

Unit-test failure (both editions, identical): every suite passes up to `RefPedalHistoryFailure`,
whose first test fail-fasts with `Tests failed (-1073740791)` = 0xC0000409 (terminate/abort).
Same suite passes on Linux clang 18 (Release and ASan/UBSan, 7 tests / 2,104 checks). The base
branch's Windows run never reached tests (C2653), so this is the first MSVC execution. Diagnosis:
an injected `bad_alloc` escapes a `noexcept` boundary only under the MSVC STL; static audit of
the apply/publication `noexcept` functions found no MSVC-only allocation. Diagnostic terminate
handler pushed in `4159a634`; Windows Pro re-dispatched on that commit.

Uploaded artifacts (unqualified diagnostic binaries — unit tests red, not release candidates):
`Luthier-FREE-windows-97d3b013…` sha256 `a31fab050f4c6d8a7a7cd10a94b4299213547c1909c93f9bfeba94d04e4f34d9`;
`Luthier-PAID-windows-97d3b013…` sha256 `6c51367658be3ff9e2e1b04c43dc48d1cb8ca740610e80ae8788692210320053`
(artifact zip digests from the Actions API). Log download from the blob host is blocked by this
environment's network policy; job logs were read through the API.

## Full Linux Pro suite (binary at `66d3e65a`, clang 18 Release, xvfb)

`LuthierTests` (all suites): **2,921 tests / 15,095,657 checks, 108 min; 1 failure**:
`Trademarks.sourceTreeHasNoUnmarkedBrandNames` — `mxr_dynacomp.cpp` `inspiredBy "MXR Dyna Comp"`.
Pre-existing on the base (`25cd2135`, from the Dyna import); the other four named models already
use `inspiredBy = nullptr`. Fixed by aligning Dyna (and the new RC-5 wrapper) to `nullptr`; the
test is unchanged and now passes. Owner question (optional): whether nominative "Inspired by …;
independent and unaffiliated" provenance text should be permitted for named models — the
scanner currently forbids it. Note: that run's binary contained the later-reverted CE-2
candidate, so `bbdOnOriginalThdGate` passed in it; at the current source it is the known RED.
The suite rewrites `docs/review/QA_ROBUSTNESS.md` while running; a mid-run truncated copy was
restored, not committed.

## Slice 6 — Windows unit-test fail-fast root-caused and fixed

Diagnostic runs (Windows Pro): `4159a634` → `HISTORY-FAULT terminate: op=0 ordinal=13`;
`350733ef` → per-ordinal allocation sizes. Apply, MSVC: `224 32 32 224 32 32 8960 32 32 27 224 32 27
**16 16 64** 27 …` vs Linux `… 224 21 27 31 27 31 …`: three MSVC-only allocations exactly at the
fault ordinal, where `Board::Impl::settingsChanged` builds its MIDI-map change key with
`juce::String(settings.tempoBpm)`. JUCE's `NumberToStringConverters::doubleToString` is declared
`noexcept` but constructs a `std::ostream`, which allocates under the MSVC STL (not libstdc++);
JUCE's JSON writer formats doubles through the same path. An injected `bad_alloc` there hits a
`noexcept` frame → `std::terminate` → 0xC0000409 on Windows only.

Fix (production, `Board.cpp`): the change-detection key is built by a small `appendKey(var)` that
encodes doubles by exact bit pattern and never calls JUCE double formatting or `JSON::toString`.
Same change semantics (stricter for doubles). Test unchanged. Linux: history-failure 7 tests
pass (Apply now 22 sites); full `RefPedal` 527 tests / 12,395,541 checks, only the known CE-2 RED;
ASan/UBSan board/MIDI/history/extra-state 108 tests / 59,872 checks clean. Diagnostics kept
(terminate report + size log; silent/one line per op). Windows Free+Pro re-dispatched on the
fix commit.
