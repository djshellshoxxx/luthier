# Phase90 single-envelope checkpoint — 2026-10-27

Subsequent model-level sync/transport freeze correction:
[source and evidence](PEDALBOARD-2026-10-27-PHASE90-SYNC-FREEZE.md).
The earlier measurements and remaining gates recorded here keep their original
source identity; the later note supersedes the particular phase-write gap.

Code/test commit: **`78b1d84ffcae43de6eae5ef869c9831c087a2aba`**, branch
`codex/luthier-pedalboard-completion`, based on
`c49bffc82ad30e1e98d57565e69120c5da65df0f` in the same `/workspace/luthier` executor.
One coordinator edited; one OpenAI worker reviewed read-only and finished.
No Claude/API credit, push, CI, merge, release, new software or environment was used.

## Completed slice

The actual Phase90/node composition previously applied two linear audio fades,
producing `dry + a*a*(active-dry)`. The new regression reproduced this before
the implementation changed: **1 test, 300 checks, 120 failures**, exit1,
covering 5 sample rates × 6 host block sizes × enable/disable.
Both direct and node audio-law checks failed in every case; linear-control,
finite-output and settled-dry checks passed. Original diagnostic files and
the exact RED test source are preserved unchanged in the evidence copy.

The original clauses are compatible: Board Core specifies a control position
linear in time, and Phase90 specifies equal-power audio gains over5ms.
`PedalDesc::bypassFade` is appended with a Linear default. Only Phase90 opts
into EqualPower. For a non-tail opted-in descriptor, the node calls the additive
`IPedal::processForBoard` hook and applies the only audible envelope. The default
hook forwards to ordinary processing. Phase90 shares one real DSP renderer;
direct clients retain one equal-power fade, while board clients receive the
active processed signal. Engagement is never forced, and filter/LFO arithmetic
is retained. Exact endpoints avoid residual gain or unnecessary trigonometry.

Other models retain their previous linear expression and processing dispatch.
DSP/buffered-tail residual rendering remains unchanged even if a descriptor
requests EqualPower. The change is an internal source-interface addition:
**all project consumers must be coherently rebuilt**. It changes no serialized
state/parameter identities, registrations, pool cap or original acceptance gate.
See the unnumbered entry in `spec/reference-pedalboard/specs/IMPLEMENTATION_DECISIONS.md`;
historical E69 still denotes prepared-memory work.

## Executed checks

Final focused runner results on the exact source tree committed above:

| Configuration | Result | Test time |
|---|---|---|
| Linux Pro Release | 11 tests / 53,520 checks, exit0 | 4.396s |
| Linux Free Release | 11 tests / 53,520 checks, exit0 | 4.328s |
| Linux Pro ASan/LSan/UBSan | 11 tests / 53,520 checks, exit0 | 10.748s |

The runner freshly compiled every project translation unit used in this slice.
Only unchanged JUCE module objects were reused from the matching edition/debug/
sanitizer configuration. The sanitizer's retained routing-table/typeinfo
dependencies were also freshly compiled from real source, with no project stubs.
Initial helper failures (Release/Debug mismatch, missing dependency closure,
then compdb dependency-output paths) are preserved separately; they are not
test passes. The final helper reads the actual configured compilation database.

Strict invocation: `ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
No sanitizer diagnostics through shutdown; stderr contains only JUCE's version banner.

The 11 tests comprise the original seven Phase90 tests plus:

- 60 real-model/direct/node fade cases at44.1–192kHz, blocks1/4/16/64/512/2048,
  independently comparing against one equal-power audio envelope and a linear
  control ramp, with exact settled dry and finite output checks.
- 27 real-node reversal/trim cases across3rates,3blocks and2.5/5/10ms board
  durations. These protect one envelope owner, continuation from the current
  position and zero allocations in processing. Non5ms durations are diagnostic
  ownership cases, not a change to the production5ms requirement.
- Default linear dispatch, buffered bypass with/without tails, and DSP residual
  rendering under input/output trims; no call into the opt-in hook on these paths.
- Free-running LFO freeze/resume through both Phase90 render entry points.
  This deliberately does not claim sync/transport freeze acceptance.

Commands, full flags, project/module hashes, logs, binary hashes and runner source:
`/workspace/luthier-evidence/phase90-single-envelope-20261027/`.
Build command: `python3 <evidence>/build.py pro` (also `free`, `asan`).
Run: `<evidence>/<configuration>/tests RefPedalPhase90` with the strict environment
above for ASan. The helper adapts the earlier in-project engine runner; the test
fixture attributes the retained actual-model/node audit it adapts. Existing
authorship, notices and licenses remain intact.

## Original CPU gate remains RED here

The unchanged `docs/coverage/probes/phase90-e58-actual.cpp` was freshly built and
run on both corrected source and an exact archived `c49bffc8` baseline with the
same compiler/Release flags and matching JUCE dependencies.

| Source | Ten measured stereo CPU trials | Original limit |
|---|---|---|
| Corrected | 0.491946320–0.567572280% | <0.4% |
| Unchanged baseline | 0.496025180–0.552433890% | <0.4% |

Both fail the existing limit on this executor; overlapping ranges do not prove
either a platform-independent pass or exact performance equivalence. Historical
0.332–0.390% observations remain historical machine-specific evidence.
The probe itself exits0 because it prints measurements; **its exit is not a CPU
acceptance pass**. Maximum FRF error was7.15567625e-10dB; maximum measured THD
was0.028330842%. Response/THD records and output checksums match the baseline.
No threshold was relaxed and no speculative DSP optimization was made.

## Remaining gates and next slice

Phase90 is **unregistered and not accepted as complete**. Its separate original
“peak delta <1LSB24b for sine” criterion is unchanged and unresolved; no residual
or slope-subtracted estimator is approved. Earlier raw/ideal fade-step failures
are preserved, not reclassified as passes. Sync `setTime`/transport can still
write phase while disabled; that pre-existing source-supported gap needs its own
test-backed correction. CPU<0.4%, the required30-minute model soak, remaining
model-specific MIDI/fidelity gates and real reference hardware also remain open.

These are focused node/model tests, not a new full BoardEngine/host/GUI/full-suite
or plugin-format qualification. Prior full-suite/validator/native results retain
their old source identity. The changing shared interface requires coherent
rebuilds before any product-level current-source claim. The build-stamp source
scope fix remains preserved; its one-time command migration is still pending.
All metadata cache/environment overrides were previously cleared; configure
normally for the current Git-derived identity before any future product build.

The approved initial six remain Phase90, CE-2, DD-3T, CE-2W, Dyna Comp and RC-5.
No selected named model is registered; the remaining twelve and their roadmap
are preserved. DD-3T's tested memory-accounting fix and Pro/Free IR fixture fix
are retained. A next bounded Phase90 slice can reproduce disabled sync/transport
phase mutation without depending on the unresolved24-bit estimator; CPU work
must preserve response, quality and safety thresholds. No new operation is
implicitly authorized by this handoff.

## Recovery and restrictions

The same-identity private Library checkpoint retains prior evidence and adds
this milestone's logs, source snapshot and a verified Git bundle above
`f1efeded706b9dc57da7e941ca856b17eaad5183`. Its final manifest/receipt records
the documentation HEAD, exact SHA256 and Library version; no build binaries,
caches, credentials, private intake archives or vendor trees are uploaded.
Use `git bundle verify`, then import into an isolated branch from the required
base. Inspect local edits before checkout; never reset/delete provenance.
The evidence helper expects this executor's configured local toolchain; another
machine must build its own matching JUCE/project objects, not reuse binary paths.

Current restrictions remain: no pushes/CI/merge/release/deployment, software
installation, paid infrastructure, credential/security changes or replacement
environment; no Actions40292011494 denied-read retry, separately denied extra
UI callback stress, or denied process-control/read retry. These tests are the
authorized existing local DSP/node qualification, not that denied UI task.
Retain untracked Free build and Python caches, but do not stage/package them.
