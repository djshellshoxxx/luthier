# CE-2 / CE-2W prepared-memory admission — 2026-10-27

Starting source `eaea31bae100f5bfcef024f53b8f728b47e76fea`, same executor and
single integrator, with one OpenAI read-only reviewer. No Claude/external model
API, source push or CI. The selected six/deferred twelve remain preserved.

Code/test commit `d54d5719e12b9b47ca743c5e4c30a4490c1918c8`.

## Demonstrated gap and implementation

Both real models inherited unknown prepared-memory reporting and had no
constructor-free bounds. The original one-test run produces20checks/10failures,
exit1, across both models and five44.1–192k rates. This is a budget-safe
admission prerequisite, not a demonstrated audio-fidelity defect.

CE-2 owns two12ms BBD delay arrays; CE-2W owns three40ms histories, even when
only one mode is audible. `BbdCore` now exposes heap capacity through its existing
FractionalDelay. Both models report `sizeof(model)` plus actual vector capacities,
including those retained by lower-rate reprepare. Filters/oversamplers are inline;
the interpolation table is shared immutable storage, not per-instance allocation.

Fresh bounds validate rate8000–1000000, maxBlock>0 and policy0–2 before size
conversion. Each buffer needs `ceil(rate*delayMs*.001)+24` doubles: BBD adds8,
FractionalDelay adds16. Temporary owned prepare storage is zero. No factory or
model construction is used to estimate, and the getter/estimator allocate nothing.

Vector capacity growth is standard-library dependent. Fresh requested lengths
are verified against actual capacities on this Linux library, not asserted as a
universal allocator guarantee. The existing pool rejects an actual report that
exceeds its reservation. Native library checks remain required. The original
64,000,000-byte cap, accounting rejection and shared-pin refunds remain unchanged.

## Actual checks

Four new memory tests plus eight existing CE-2/CE-2W model tests pass:

| Configuration | Result | Test time |
|---|---|---:|
| Linux Pro Release |12tests /1,312checks, exit0|0.537s|
| Linux Free Release |12tests /1,312checks, exit0|0.493s|
| Linux Pro ASan/LSan/UBSan |12tests /1,312checks, exit0|2.968s|

The expanded checks were added with the new API; only the original unknown-report
case is claimed as executed RED. Fresh coverage spans nine boundary/fractional
rates, all three policies and default/max controls (108model/preparation cells),
actual capacity/reset/reprepare, invalid input, allocation-free reporting/bounds,
and real pool construction/reuse/rejection/refunds. At1MHz, Pro/Free admit296CE-2
or64CE-2W runtimes under64MB and reject the next before factory construction.
This is retained-runtime accounting across generations, not an overfull board.
Actual retained totals differ in the sanitizer configuration and are logged
separately, not transferred between builds. At192k, model storage reports58,464
and208,280bytes respectively and retains those capacities after48k reprepare on
this library. Tests observe capacity instead of requiring retention everywhere.

Existing impulse, bypass, partition, CE-2 distortion limits, CE-2W mode transition
and stale-mode-history assertions remain unchanged and pass. No processing,
parameter/state/UI, bypass, policy behavior or registration changed.

Evidence: `/workspace/luthier-evidence/chorus-memory-20261027/`. Build
`python3 <evidence>/build.py pro` (also `free`, `asan`), run
`<evidence>/<edition>/tests RefPedal`. All project TUs freshly compile; only
matching unchanged JUCE module objects are reused. Strict environment:
`ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
No findings through shutdown; sanitizer stderr is only JUCE v8.0.10.
Commands, source/module/binary hashes, original source/RED logs, final results
and scoped review are preserved, with fixture/harness attribution retained.

## Remaining and recovery

Both models remain unregistered prototypes. This closes memory-admission work
only. Original fidelity, CPU, long-soak, UI/host and native/hardware gates remain.
Phase90 CPU<0.4% is still RED; its24-bit criterion remains unresolved/unapproved.
No fresh full-product/native/plugin qualification is implied. A next bounded
slice is CE-2W rapid mode-change continuity within its existing10ms transition;
inspect the actual current blend before claiming a defect.

The same-identity private Library checkpoint preserves the full predecessor
chain and a verified Git bundle requiring
`f1efeded706b9dc57da7e941ca856b17eaad5183`. Verify manifest/checksums and import
into an isolated branch while preserving local edits. No binaries/caches,
private intake archive, vendor tree or credentials are packaged. The three
pre-existing untracked build/cache paths remain preserved/excluded. No push,
CI, merge, release, deployment, new dependency/environment, credential/security
change or retry of denied Actions/UI/process operations occurred.
