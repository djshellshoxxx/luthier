# Selective retained Dyna Comp prototype integration — 2026-10-27

Source commit `28b4c63e6785cdd101042f44f99207deab215b89`, starting at
`ba64000556f0e9a51fb81f25698eb1f5181f7cb7`, same executor/single integrator,
one completed OpenAI read-only reviewer. No Claude/external model API.

## Preserved work integrated

Only `Models/mxr_dynacomp.{h,cpp}` and
`Tests/ReferencePedals_mxr_dynacompTests.cpp` were copied, byte-identically,
from retained DSP tip `40ed1621b52a6b9110e15d87747924cf59b96e3d`.
Current helper APIs already support the required scalar paths. No common DSP,
drive, adapter or circuit-base changes were imported. No branch history was
merged, reset, rewritten or deleted; production registration stays blank.

Original implementation `40d3503d`, memory work `378c8913`, release-cache/test
change `b85b53b2` and CPU-unit declaration `23d5242c` remain traceable through
that ref. Original OWNER notices and upstream authorship are preserved; the
integration commit names their provenance. Private DONOR.json records the three
exact Git blobs. This is reuse of existing work, not a new DSP implementation.

The completion branch previously had empty skeletons. Importing the tests first
reproduced compile exit1 because MxrDynaComp was missing. This is missing-module
integration evidence, not a newly discovered audio defect. Original compiler
output, source snapshots and build context remain preserved.

## Fresh verification

The imported four tests pass81,937checks within the combined chorus/Dyna cohort:

| Configuration | Combined result | Time |
|---|---|---:|
| Linux Pro Release |18tests /83,344checks, exit0|0.835s|
| Linux Free Release |18tests /83,344checks, exit0|1.217s|
| Linux Pro ASan/LSan/UBSan |18tests /83,344checks, exit0|4.592s|

Dyna coverage includes silence/sample partition identity, bounded detector,
2x/4x/off latency selection, allocation-free lifecycle and reused-versus-fresh
rate/quality release coefficients. Actual dry-buffer capacity remains included.
The chorus memory/mode tests remain green. All project TUs are freshly compiled
against current headers; only matching unchanged JUCE module objects reused.
Strict environment:
`ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
No sanitizer findings through shutdown; stderr only JUCE v8.0.10.

Evidence: `/workspace/luthier-evidence/dynacomp-integration-20261027/`.
Build `python3 <evidence>/build.py pro` (also `free`, `asan`); run
`python3 <evidence>/run.py <edition>`. The missing-module build's original
compile error/source/context are retained; the runner at that point exited
before its final full-command provenance write, so no nonexistent complete
RED command manifest is claimed. Final full commands/source/module/binary
hashes and logs are retained. The guarded Library copy includes the predecessor
chain, original failure evidence and a verified portable Git bundle requiring
`f1efeded706b9dc57da7e941ca856b17eaad5183`.

## Open acceptance and next slice

Five of18named models now have real integrated DSP prototypes (Phase90, CE-2,
DD-3T, CE-2W, Dyna Comp). This supersedes the earlier four-model code inventory,
not its original dated evidence. Zero named models are registered/accepted.
RC-5 remains required in the selected six; the other twelve remain preserved.

Original Dyna numerical/CPU failures remain open. Retained corrected natural-
control THD, recovery and release-cache/CPU evidence from40ed1621 is preserved
with blob/SHA256 provenance and explicitly marked historical. Those bytes are
not fresh current-tree measurements. Old1.189473% exploratory CPU median exceeds
the unchanged0.5% numerical target and was contention affected; this import does
not turn it into a pass. Full fidelity/alias, board bypass/PDC, all five-rate
safety/soak, host/UI, native-library and hardware qualification remain open.
The estimator currently ignores policy validation and restricts rates to≤192k;
its exact fresh-capacity assertion is library-dependent. Next bounded slice:
reject unsupported preparation policies, then selected-model boundary/robustness
coverage, preserving all original predicates and production registration.

Phase90 CPU<0.4% remains RED and24-bit criterion unresolved/unapproved. No new
product/full-suite/native/plugin or release qualification follows from these
unit tests. No push/CI/merge/release/deployment, new dependency/environment,
credential/security change or denied Actions/UI/process retry occurred.
