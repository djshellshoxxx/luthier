# DD-3T beat latch and glide correction — 2026-10-27

Code/test commit **`df1dcb1de1bf200f34bf3ba8177993f8544f610d`**, on
`codex/luthier-pedalboard-completion`, starting from
`5d2c66586f1253db394e62515dbb2e065bf22634`. Same executor, one coordinator;
one OpenAI read-only reviewer completed. No Claude or external model API used.

## Reproduced defect and unchanged requirement

DD-3T §6 requires sync changes latched at beat boundaries; §4 specifies the
80ms one-pole time constant. `IPedal::setTime` is once per block. The old model
noticed a beat only when its callback-start PPQ changed integer part, so an
interior crossing could postpone retargeting until a subsequent callback.

Initial observations on unchanged production:

- Explicit delivered-boundary glide:1test/900checks, exit0. The original
 80ms equation was correct and remains unchanged.
- Blocked versus one-sample host timeline:1test/60checks,17 timing failures,
 exit1.17of20rate/buffer/tempo cells differed. At44.1kHz/2048/75BPM the beat
 occurs at sample8820, but the next callback starts10240: about32.20ms late.
 Maximum observed delay-state disagreement was99.404961ms, not99ms of
 boundary lateness. Finite-output/allocation checks passed.
- Extended original-source regressions, including fractional multi-beat
 callbacks:4tests/1,100checks,27failures, exit1.

Review found an additional interim-implementation error: capturing the division
at `setTime` ignored a discrete event delivered afterward. A five-rate test
reproduced5failures/20checks on that interim source. Its source/results remain
separate from the original-production failures. The final code reads the latest
division at the predicted latch; it does not change the current latched target early.

## Narrow correction

`setTime` anchors an audio-owned fractional samples-to-next-beat countdown from
valid playing time. The model checks a due boundary before the current sample's
existing smoothing update, carries the fractional remainder, and records handled
beats so the following callback cannot relatch a new request prematurely.
There is at most one predicted event per sample, no catch-up loop or unchecked
double-to-integer counter conversion. Prediction requires a representable next
beat and finite period of at least one frame; authoritative delivered updates
retain their previous behavior when prediction is unavailable.

Only arithmetic roundoff next to integer sample positions is snapped, using a
floating-error bound capped at1e-7of a frame. This is not a replacement audio
or timing acceptance threshold. Fractional crossings still occur at the first
sample at/after the boundary, and fractional periods do not accumulate rounding
drift. Stopped/invalid snapshots and reset/reprepare disarm prediction; a derived
nonfinite period is rejected before delay-time arithmetic. Explicit delivered
changed-beat/backward-locate handling and the original subdivision/halving rules
remain. No shared clock/router, parameter/state format or registration changed.

The new private state adds32bytes on this build. Existing `sizeof(model)`-based
fresh estimation/actual reports include it automatically. The64,000,000byte
prepared pool cap is unchanged; existing tests still admit eight192k models,
reject the ninth before construction and verify shared-pin refunds/capacity.

## Final focused verification

| Configuration | Result | Test time |
|---|---|---:|
| Linux Pro Release | 19 tests / 3,527 checks, exit0 | 4.093s |
| Linux Free Release | 19 tests / 3,527 checks, exit0 | 4.356s |
| Linux Pro ASan/LSan/UBSan | 19 tests / 3,527 checks, exit0 | 36.993s |

These comprise14DD-3T model tests and five existing DD-3T memory/pool tests.
Five new timing tests add1,150checks: five rates44.1–192kHz, blocks1/64/2048,
all five divisions, rising/falling tempos, analytic80ms glide, callback crossings,
post-snapshot and post-crossing division events, fractional multi-beat callbacks,
stopped/invalid time, reset/reprepare, and authoritative backward locate.
New event/render paths show no allocation-counter change and finite output.
The existing impulse, tap, hold, partition,60second-feedback and UI-meter tests
remain intact. No numerical/CPU/safety threshold was relaxed.

Strict options: `ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
No findings through shutdown; stderr contains only `JUCE v8.0.10`.
Every project TU in the focused closure was freshly compiled, including pool,
registry, state and asset code. Only matching unchanged JUCE module objects
were reused. Recorded final source hashes match the committed tree.

Evidence: `/workspace/luthier-evidence/dd3t-sync-glide-20261027/`.
Build `python3 <evidence>/build.py pro` (also `free`, `asan`); run
`<evidence>/<configuration>/tests RefPedalDd3t`, with the strict environment
above. Initial/extended/interim source snapshots, stdout/stderr and exits are
retained; final full commands/source/module/binary hashes are recorded.
Intermediate build-provenance files were overwritten by later focused builds,
not separately recovered; their retained source/result evidence is distinguished
from final command/binary provenance. Harness adaptation and original source
ownership notices remain. Review was read-only and did not execute tests.

## Remaining gates and recovery

This closes the reproduced model-level callback-size timing defect, not full
host/MIDI clock integration or named-pedal acceptance. DD-3T stays unregistered.
Its other original fidelity/performance/soak/bypass requirements and current
product/full-suite/plugin/native/hardware qualification remain outstanding.
Phase90 CPU<0.4% remains RED, its24-bit criterion unresolved/unapproved, and
the rejected CPU candidate/evidence remain preserved without new probes.
All six selected models and retained twelve remain intact. A next bounded
slice is actual DD-3T/node bypass and residual-tail composition under slot trims,
using existing E60/E68 requirements before diagnosing any defect.

The guarded same-identity private Library checkpoint contains this evidence,
a verified portable Git bundle and the complete prior checkpoint chain. Its
manifest/receipt records final documentation HEAD, SHA256 and Library version.
Recovery requires `f1efeded706b9dc57da7e941ca856b17eaad5183`; verify/import into
an isolated branch while preserving local work. No build binaries/caches,
credentials, private intake archive or vendor tree is packaged. Local harness
paths require matching installed dependencies, not reuse of stale objects.

No push, CI, merge, release, deployment, new dependency/environment or
credential/security change occurred. Denied Actions/UI/process operations were
not retried. Three pre-existing untracked build/cache paths remain preserved.
