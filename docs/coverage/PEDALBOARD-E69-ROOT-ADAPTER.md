# E69 root adapter — focused qualification passed; product integration pending

Exact root baseline:879be6be2b3a46cabf804683ff7b7ba04735bb14, including qualified a654 bounded retirement.

This private candidate enables audited constructor-safe memory admission for all13 currently registered models. Limit remains exactly64,000,000bytes. Runtime objects, dry buffers, owned prepared model capacities, retained distinct runtimes and peak preparation scratch are charged. Routing scratch, shared immutable tables and separately budgeted looper media are separately inventoried; allocator metadata, control blocks and state/plan JSON/string metadata are not an RSS claim. Named model candidates remain unregistered.

Dependencies adapted:9adef3c8 API;29da6726 budget;0168fc98 prototype/engine fixtures;2f96a496 FractionalDelay capacity;e29f01e3 GenericB/C-only reporters;DSPccb3774f five-file root GenericA patch;hostf0647b8d audited fixtures;9f3e2538 enforcement;fbbc0647 exact extras/rate recovery;dfc23db9 retained service guard. Root CPU amounts/scales remain unchanged. No whole candidate GenericA implementation is imported.

New combined regression exposed C++17 owning-string allocation in memoryRequirement lookup for long IDs (162 focused failures;216 independent-oracle failures). The API now scans static descriptor IDs under its existing off-audio registry guard, copies the estimator then calls it outside the guard. No callback registry access is introduced. Negative missing/overflow/inaccurate estimator fixtures remain negative.

Current actual evidence: focused native133tests/207692checks passed; independent allocation-payload oracle13models×8rates×3policies×3parameter settings=936fresh+936lower-rate-reprepare cases passed, exact reported-vs-measured capacities, bounded prepare peak, zero estimator/report allocations. Original red logs retained in external qualification evidence. Source-only oracle is probes/e69-root-memory-capacity.cpp.

Fresh ASan/UBSan/leak focused133tests/215010checks passed with stderr0. The independent allocation oracle also passed936fresh+936reprepare cases under ASan/UBSan/leak with stderr0. All private production/test TUs were instrumented; linked shared JUCE objects were prebuilt and uninstrumented. Different focused check totals reflect concurrent schedules. Actual combined native host/processor E69 and all-product gates remain pending rootbuild7; the focused133-case harness is not that product build. A saved build6 dependency closure identified417 TUs requiring recompilation for changed virtual APIs and class layouts, retaining405 immutable unaffected inputs; coordinator chose coherent root CMake rebuild instead of duplicate private host rebuild.

Invariants: reserve before factory/prepare; checked overflow and unknown refusal; reservation lives until last retained runtime dies; failed normal edit preserves old working audio/model/history; stopped rate failure publishes inactive exact-dry board with latency0 and error, preserving stored model/history/assets for correction/reprepare. Qualified a654 SC/six-pointer retirement and reserve-before-publication are preserved. Post-publication BoardModel history allocation atomicity remains open; no universal bad_alloc rollback claim. Audited truthful estimators are required; actual report mismatches are rejected after preparation.

Independent timefx core/Generic review and DSP GenericA review found no new source issue under documented single-reader/single-writer/stopped-lifecycle and logical-payload assumptions. Reviews are distinct from independent test execution.

## Exact private qualification identities

Native focused binary SHA-256:9d5a09ea3137b1841a3f6635e385eb79196ab4bd8e2a347f24c35576b70304b9.
ASan/UBSan focused binary:a56c1ea4d2197ccd526efbf513dfd797764ca2532356557624850ec29630f839.
Native allocation oracle:525045c2e68794438b0856232f7e1cb57e1d1ab17c47f6b86b5bdb7212a7a7e4.
ASan/UBSan oracle:cb296ec504582037f8769e2a55186b5bd380575d713eedbfa1b6f08b2d015fec.
Sanitizers used detect_leaks=1:halt_on_error=1 and UBSan halt_on_error=1:print_stacktrace=1; no suppressions.

Source-only adapter applies cleanly with git apply --check --whitespace=error-all to coordinator root after879. No main source files or mutable native objects were changed by this lane. Private proof requires coherent rebuilding of all changed-API dependents, not a few-object mixed-vtable overlay.
