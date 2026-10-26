# DD-3T prepared-memory checkpoint — 2026-10-26

Repository `djshellshoxxx/luthier`, branch `codex/luthier-pedalboard-completion`,
same `/workspace/luthier` executor. First code checkpoint
`9a9a248172876029ec8df63b5e6abd41d1d871e2`, parent
`0bef72a64b365eab375fde27f3044b8e819c5b1a`. The reviewed test correction adds
direct storage observation and fractional-rate coverage without changing DSP.
Final source hashes, commit/bundle identity and terminal qualification are in
the private evidence checkpoint. No source push, CI or release is authorized
in this continuation. No Claude API, new software, environment replacement,
credentials/security changes or denied-operation retry was used.

## Delivered behavior

`BossDd3t::prepareMemory` estimates a fresh runtime without constructing it,
allocating or touching assets. `getPreparedMemoryBytes` reports `sizeof(model)`
plus actual capacities of all six prepared audio buffers. This removes the
model's unknown-memory report; it does **not** register DD-3T for shipping.

| Storage | Count | Requested doubles per channel |
| --- | --- | --- |
| FractionalDelay | 2 | `floor(.81*rate)+32` (16 at call site,16 inside delay) |
| History | 2 | `floor(.81*rate)+16` |
| Short Loop | 2 | `floor(.8*rate)+8` |

The estimate uses ceilings to conservatively cover rounding; object size includes
inline filter/oversampler state and vector objects. The immutable257×16-double
interpolation kernel is shared, separately inventoried by the existing memory
contract, and is not a new per-instance exclusion. No model-owned heap prepare
scratch is required. Actual reporting uses capacities, not requested sizes or
the current rate, so storage retained on a lower-rate reprepare remains charged.

The estimator rejects nonfinite/out-of-range rates, nonpositive block sizes and
policies outside0/1/2. Rate acceptance remains the pool's8,000–1,000,000Hz range;
the requested44.1–192kHz matrix is included. Null parameter storage is valid
because configuration does not affect these buffer capacities. No delay range,
precision, sample-rate target, CPU threshold or memory cap was reduced.

At192kHz, six requested arrays alone occupy7,435,136bytes/model. Eight occupy
59,481,088bytes; nine occupy66,916,224bytes before object/runtime/dry overhead.
The existing **64,000,000-byte** pool cap is unchanged. Tests confirm that the
ninth candidate is refused before factory construction, retained reservations
remain intact, sharing a runtime does not double-charge it, and the final pin
releases its reservation. Production registration remains empty in
`Models/RegisterModDelay.cpp`; registry insertion in the test is scoped/restored.

## RED/GREEN and independent review

Before implementation, real-DD-3T reporting failed both focused cases:
**2tests,7failed/18checks,exit1** at all five required rates and after reprepare.
Log `actual-report-red.log`, SHA256
`fcf2442d9ac9e7732264af551fd1b851f6fd19a6a66d9c63609958b2c621bbd4`.
The separately added estimator tests then failed compilation because
`BossDd3t::prepareMemory` did not exist; that expected compile RED is preserved.
No production implementation preceded those failing tests.

Initial Pro and Free focused runs each passed21tests/4,978checks, including
the7existing DD-3T behavior cases,7pool cases,2registry cases and5new cases.
They are superseded by the reviewed test revision below, not reused as its pass.

Independent read-only review found one portability error in the new test:
`vector::assign` does not universally promise unchanged capacity. A narrow
friend observer, with no runtime fields/layout change, now reads the actual six
capacities independently of the reporting method. Tests compare the report
against observed owned storage, preserving reset invariance without prescribing
a library allocation policy. Fractional rates44,100.1 and192,000.125Hz strengthen
rounding coverage. The reviewer confirmed that finding resolved with no required
finding remaining. Cross-platform execution remains unperformed.

Reviewed final Pro and Free focused runs each pass **21tests/5,020checks**,
1.3s/1.0s respectively, exit0. On this Linux library, independent observation
records7,440,200 owned bytes both before and after lower-rate reprepare; the test
does not require that allocation policy on other libraries. Final log SHA256:
Pro `d0968bbb546ecc2101eb4f4e7a8740953703be752db8c252de7790be457653cf`;
Free `2688dcfd0c3dc1883aa75957008a0f15b54c992c4da0f82a4b9740d105f8fbb4`.
Strict sanitizer status remains pending at this interim checkpoint.
The current one existing ASan/UBSan build expanded to830objects because adding
the test caused CMake's Git-derived build string to change from `f1efeded` to
`0bef72a6`. This is local compilation, not a full-suite run or release build.
It is not cancelled or duplicated; the final incremental dependency check must
finish before executing the tests on the reviewed source. No reduced sanitizer
options are approved to save build time.

## Preservation and next commands

Evidence directory `/workspace/luthier-evidence/dd3t-memory-20261026/` contains
RED/initial GREEN logs, before/final sources, build logs, hashes and statuses.
The private same-identity checkpoint preserves the prior Phase90/fixture/full-suite
archive too. No executables, build caches, private input research packs or secrets
belong in that deliverable. Attribution/copyright/license notices are retained.

Installed toolchain commands:

```sh
source /workspace/luthier-toolchain/env.sh
ninja -C build -j2 LuthierTests
ninja -C build-free-qualification-20261026 -j2 LuthierTests
# Use the existing sanitizer process first; do not start a duplicate build.
# After it terminates, reconcile incremental dependencies on frozen source.
```

Each focused runner uses its original build location and adjacent Resources,
fresh HOME, DISPLAY=:99 and `LUTHIER_PERF` unset:
`LuthierTests RefPedalDd3t RefPedalMemory`.
Strict sanitizer qualification must keep leak detection and allocator-mismatch
checks enabled, `LSAN_OPTIONS` unset and ASan/UBSan halt-on-error. No full-suite
rerun, additional UI stress, manual device/DAW acceptance or current native
Windows/macOS qualification is claimed by this slice.

DD-3T remains unregistered with original CPU/tonal/model gates open. Existing
production artifacts and full-suite results belong to their recorded earlier
source; they are not transferred to this metadata change. The Pro/Free tiny-IR
fixture fix77ade0bc and its original failure evidence are preserved.

The Phase90 handoff now corrects the categorical law-conflict claim using the
independent review: linear transition position can drive equal-power audio.
The double-fade defect remains open, and the raw/ideal-gain-step measurements
remain intact. **No proposed24-bit residual interpretation is approved**, no
shared bypass is edited, and no named pedal is labeled complete. Keep the
approved six includingRC-5 and all twelve deferred models/specs/tests.
