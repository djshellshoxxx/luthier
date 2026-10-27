# DD-3T tap boundary/history coverage — 2026-10-27

Test commit **`c6b67ed44330e07f0f7c0a143ddd61aebebaf759`**, branch
`codex/luthier-pedalboard-completion`, starting at
`3c2a2b63c12086392ced09c3e4eaf3d53e199985`. Same executor, one coordinator,
zero workers. **Coverage-only: no production defect was found or fix made.**
The entire `Source/ReferencePedals` tree remains unchanged. The previous DD-3T
memory-accounting work, Phase90 fixes and rejected CPU experiment are preserved.

## Existing contract and new checks

DD-3T §6 accepts133–800ms tap intervals, averages the last three valid gaps,
and resets averaging history after a2second gap. §9 explicitly tests a2.1second
gap. The normative overall12.5–800ms delay range and existing QT/D8/Q
333.3/375/500ms mapping remain unchanged; the old conflicting50ms QT clamp
is not reinstated or hidden. No original assertion/tolerance was removed.

The new tests use real `BossDd3t` instances and real parameter-trigger and
balanced foot-press/release routes; no mock tap engine or internal-counter access:

- `tapIntervalBoundariesUseRepresentableSampleTimes`:120cells across five
  rates44.1/48/88.2/96/192kHz, blocks1/64/2048, two routes and four edges.
  Accept the first integer-sample interval at/above133ms and last at/below800ms;
  reject one sample outside each boundary. Published state/BPM, saved/restored
  tap intent and finite output are checked. Settled delay uses the original
  ±2ms tap tolerance; published sample-timer state is checked to1e-9ms.
- `tapAverageRejectsInvalidGapsAndResetsAfterTwoPointOneSeconds`:30cells over
  the same rates/blocks/routes.300/500/700/400ms taps establish averaging and
  oldest-gap eviction. Invalid short/long gaps preserve the published average.
  After2.1seconds, a new600ms pair must publish600ms without old history.
  Exact2second inclusivity and the audible hold-last-tempo behavior during a
  timeout are not invented as new acceptance criteria.
- The allocation counter remains unchanged during measured event/render paths;
  persistence/var construction occurs outside that measurement. Tap-function
  foot gestures must not request an unintended board engagement toggle.

Both tests **passed first on existing production**:1,080 and870checks,
respectively, exit0. There is no RED defect or claimed correction in this slice.
Raw initial results are preserved rather than labeling a passing oracle a bug.

## Focused verification

All seven existing DD-3T model tests plus the two new tests:

| Configuration | Result | Test time |
|---|---|---:|
| Linux Pro Release | 9 tests / 2,142 checks, exit0 | 3.230s |
| Linux Free Release | 9 tests / 2,142 checks, exit0 | 3.121s |
| Linux Pro ASan/LSan/UBSan | 9 tests / 2,142 checks, exit0 | 28.961s |

Every project TU in the slice was freshly compiled; only matching unchanged
JUCE module objects were reused. The runner adapts the previously retained
engine harness; source provenance/notices remain. No stale project objects,
replacement production classes or new dependencies were used.

Strict environment: `ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
No diagnostics through shutdown; stderr contains only `JUCE v8.0.10`.
Recorded source hashes match the committed test tree and unchanged model.

Evidence: `/workspace/luthier-evidence/dd3t-tap-boundaries-20261027/`.
Build: `python3 <evidence>/build.py pro` (also `free`, `asan`). Run:
`<evidence>/<configuration>/tests RefPedalDd3t`, with the strict environment
above for ASan. Initial runs selected each exact new test name. Full commands,
source/module/binary hashes, stdout/stderr, exits and timing records are retained.

## Remaining gates and next slice

This is model-level tap coverage, not full host/MIDI/UI/board routing, fidelity,
performance,30minute soak or native/plugin acceptance. DD-3T remains unregistered;
its original thresholds and the64,000,000byte prepared-memory cap remain intact.
Phase90 CPU<0.4% remains RED, its24-bit criterion remains unresolved/unapproved,
and the rejected exact-output CPU candidate stays out of production. No repeated
Phase90 benchmark or unchanged full suite was run. All six selected pedals and
the retained twelve remain on the existing roadmap.

Next bounded DD-3T acceptance slice: objective host-sync beat-boundary latching
and80ms glide. Existing model tests cover slow-clock halving, but do not directly
assert a mid-beat tempo/division change remains latched until the next beat and
then follows the required transition. Test the actual `setTime`/process path
before diagnosing any defect; this note does not assert one or approve scope
expansion. Product rebuild/validators and Windows/macOS Intel+ARM qualification
remain separate outstanding work; no release artifact was created.

## Preservation and restrictions

The owner explicitly clarified private Library checkpoint operations are
authorized. The guarded same-identity checkpoint now incorporates this evidence
and the previously local CPU-investigation archive, preserving all predecessors.
Its manifest/receipt records final documentation HEAD, Library version and
SHA256. A verified source bundle requires
`f1efeded706b9dc57da7e941ca856b17eaad5183`; verify/import on an isolated branch,
preserving local work. No binaries, caches, credentials, private intake archive
or vendor tree is packaged. The local runner expects existing matching build
dependencies; other machines must rebuild their own.

No Claude/paid model/live-chain request, push, CI, merge, release, installation,
new environment or credential/security change occurred. Denied Actions/UI/
process operations were not retried. Three existing untracked build/cache paths
were preserved and excluded from commits/checkpoints. No owner input is pending
for this completed coverage slice.
