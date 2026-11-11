# DD-3T held-loop spillover scheduling — 2026-10-27

Starting source `47234e04853183e8cfbe6dc49e7d3c42d0f5c3c8`, same executor,
branch `codex/luthier-pedalboard-completion`. One integrator and one completed
OpenAI read-only reviewer. No Claude/external model calls.

Code/test commit `0cdfef09ac5904bd8771822aed45958aef815de5`.

## Reproduced failure and narrow correction

BOARD_CORE5.2 allows sleeping only after the tail is silent. E60/E68 preserve
Buffered tails and their actual wet residual. DD-3T emits selected held-loop
samples, but its energy envelope measures the underlying delay only. With
feedback zero that line drains while the captured loop remains audible. The
real node then stops rendering at the unchanged−90dBFS threshold.

The new actual model/node tests reproduce this on the original production:
3tests/415checks/40failures, exit1. Continuous loops fail at all five rates and
blocks1/64/2048; sparse800ms loops fail at all five rates. Worst observed
sample disagreement is0.134615838868648. Hot/trimmed dry and residual composition
already passes. Initial two-test evidence has15 additional allocation assertions
contaminated by the failure-report allocations; the expanded RED fixture defers
reporting until after its allocation measurement, and that noise disappears.
Both raw results are preserved, not erased.

`getTailEnergyDb()` now conservatively returns0dB while a prepared held buffer
may emit (`loopLength && (holding || holdMix>0) && wetGain>0`). After release
or trails-off cut, the existing meter governs again. Merely metering the current
held sample could sleep during a sparse loop's quiet gap. Silent held buffers
therefore also retain processing until release/cut; CPU qualification remains
open. This is a scheduling bound, not a calibrated loudness meter.

No audio equation, bypass law, timing, pool cap, object layout, parameter/state
format or registration changes. Existing provisional bypass-law/fidelity
criteria are not resolved by these node-composition checks.

## Verification

Linux Pro Release:22tests/3,942checks, exit0,6.234s.
Linux Free Release:22tests/3,942checks, exit0,6.560s.
Linux Pro ASan/LSan/UBSan:22tests/3,942checks, exit0,52.928s; no findings
through shutdown (stderr contains only JUCE v8.0.10). The focused closure
contains existing14model and five memory/pool tests plus three new node tests.
The latter cover continuous held spillover/release15cells, sparse held loops
and trails-off cut5cells, and real residual/hot dry/trim composition60cells.
A separate always-rendered model provides output independent of the node's
sleep decision; it is not an independent circuit-fidelity oracle.

Build: `python3 /workspace/luthier-evidence/dd3t-node-tails-20261027/build.py pro`
(also `free`, `asan`). Run `<evidence>/<edition>/tests RefPedalDd3t`.
Strict sanitizer environment:
`ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
All project TUs in the focused closure are fresh; only matching unchanged JUCE
module objects are reused. Sanitizers also compile real routing implementations.
Commands, source/module/binary hashes, exits, raw RED/GREEN logs and review are
retained in `/workspace/luthier-evidence/dd3t-node-tails-20261027/`.
Fixture/harness adaptations retain upstream ownership and provenance comments.

## Remaining work and recovery

DD-3T remains unregistered and experimental. Its other original fidelity,
performance, bypass-law, long-soak, host and hardware requirements remain open.
Phase90 CPU<0.4% remains RED and its24-bit criterion unresolved/unapproved.
No new full-product/native/plugin qualification follows from this focused slice.
The selected six and deferred twelve models remain preserved.

Next bounded prerequisite identified in current source: CE-2 and CE-2W still
inherit unknown prepared-memory reporting, preventing budget-safe admission.
Their two/three BBD delay allocations need constructor-free bounds and actual
retained-capacity reports before any registration. DSP behavior must stay intact.

The guarded same-identity Library checkpoint preserves this source, the complete
predecessor chain and a verified portable Git bundle requiring
`f1efeded706b9dc57da7e941ca856b17eaad5183`. Verify manifest/checksums, then import
into an isolated branch preserving all local work. No binaries/caches/vendor
copies/private intake archive/secrets are packaged. Three pre-existing untracked
build/cache paths remain excluded and preserved. No push/CI/merge/release,
new dependency/environment or denied-action retry occurred.
