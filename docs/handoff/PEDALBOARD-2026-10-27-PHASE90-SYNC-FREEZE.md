# Phase90 sync/transport freeze — 2026-10-27

Code/test commit **`6e9129474255e097ddbe5f6eabd8fd7c8462cba9`**, on
`codex/luthier-pedalboard-completion`, starting from
`3a93c16f51bed439a1914bd1c3e899175a62272f`. Same `/workspace/luthier` executor;
one coordinator, zero workers. No duplicate review, full-suite rebuild, Claude
request, push, CI or release was used. The existing three untracked build/cache
paths were preserved and excluded from commits/evidence.

## Defect and smallest correction

The original Phase90 specification §3 requires phase frozen while bypassed.
Its §6 retains host/MIDI sync and Start/Continue resync. The previous render
entry points already stopped free-running advancement while disabled, but
`setTime` still wrote host PPQ phase and `onTransport` still reset phase for
Start/Continue while disabled. Existing seven model tests plus the later four
single-envelope regressions did not cover those writers.

Two new real-model tests reproduced actual failures before production edits:

| New test | Checks | Failed before fix | Exit |
|---|---:|---:|---:|
| `syncTimeKeepsBypassedPhaseAndRetainsLatestTempo` | 540 | 120 | 1 |
| `transportResyncOnlyRunsWhileEngagedAndStompResetSurvives` | 560 | 40 | 1 |

The exact compiled RED source and build provenance are saved. An explicit
`<limits>` include was added afterward; the preserved RED snapshot is verified
against the original compiler-recorded hash rather than mislabeled as the
later source. Assertions and acceptance thresholds were not relaxed.

The production change adds **`enabled &&` to the two phase-write conditions**.
`time=t` still executes while bypassed, retaining latest tempo/clock context.
Engaged phase-lock/resync, free mode, Stop/Locate handling, explicit reset-on-
stomp and lifecycle reset behavior are preserved. No transport event is queued
for later replay. After re-engagement, the next valid playing host update
resumes ordinary PPQ lock. DSP rendering, the tested single bypass envelope,
interfaces, state format, other pedals and registrations are unchanged.

## Focused qualification

The same installed-tool harness from the previous checkpoint was reused with
new output directories; every project translation unit in the slice was freshly
compiled. Only matching unchanged JUCE module objects were reused. No obsolete
project object was linked across the earlier interface change.

| Configuration | Final result | Test time |
|---|---|---:|
| Linux Pro Release | 13 tests / 54,620 checks, exit0 | 4.036s |
| Linux Free Release | 13 tests / 54,620 checks, exit0 | 4.469s |
| Linux Pro ASan/LSan/UBSan | 13 tests / 54,620 checks, exit0 | 9.741s |

All recorded source hashes still match the code/test commit above. Strict
environment: `ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
No diagnostics through shutdown; stderr is only `JUCE v8.0.10`.

New coverage uses both actual render entry points, five sample rates from
44.1–192kHz and all six clock divisions. It checks frozen phase under changing,
negative and nonfinite PPQ; latest bypassed tempo is used immediately after
re-engagement; valid engaged host PPQ locks; stopped/invalid host updates do not
write phase. Start/Continue/Stop/Locate are checked in free and sync modes.
Explicit stomp reset and repeated engaged notifications retain their original
semantics. Relevant timing/event/render calls allocate nothing in these tests.
The earlier 60 composition cases, 27 reversal/trim/duration cases, legacy
linear/buffered/tail paths, free-running freeze and original model tests also pass.

Commands and complete evidence are under
`/workspace/luthier-evidence/phase90-sync-freeze-20261027/`:
`python3 <evidence>/build.py pro` (also `free`, `asan`), then
`<evidence>/<configuration>/tests RefPedalPhase90`, with the strict environment
above for ASan. RED runs selected each new test by its exact name. Logs, exits,
full compiler/link flags, source/module hashes and binary identities are retained.
Harness attribution to the retained in-project engine runner is preserved;
upstream authorship/licenses and the prior adapted fixture notice are unchanged.

## Remaining acceptance and recovery

This closes the identified model-level sync/transport phase-write defect;
it does not qualify full BoardEngine/host/clock integration or every MIDI rule.
No pedal is registered or claimed complete. **CPU<0.4% remains unchanged and
open/RED**, retaining the previous measured corrected range0.491946320–
0.567572280% and baseline0.496025180–0.552433890% under their original SHAs.
Those measurements were not rerun or transferred to this new source as passes.
The original24-bit click criterion is still unresolved/unapproved; original
failure data and the equal-power correction remain preserved. Thirty-minute
model soak, remaining MIDI/fidelity/hardware acceptance, coherent full-project
rebuild, full-suite/plugin validators and current Windows/macOS Intel+ARM native
qualification remain outstanding. No native artifact or release was created.

The six selected models remain Phase90, CE-2, DD-3T, CE-2W, Dyna Comp and RC-5;
the other twelve and all retained candidate work remain intact. Other ready
Phase90 work includes objective remaining clock/expression acceptance coverage
and CPU diagnosis that preserves tonal/safety limits. No new action is approved
by this note. Current no-push/no-CI/no-release/no-new-software boundaries and
recorded denied Actions/UI/process operations remain binding.

The same-identity private Library checkpoint adds this evidence and a verified
portable Git bundle, preserving the previous checkpoint as a nested archive.
Its manifest/receipt records final documentation HEAD, SHA256 and Library
version. Bundle recovery requires `f1efeded706b9dc57da7e941ca856b17eaad5183`;
verify before importing into a separate branch, preserving local work. Binary
builds, caches, credentials and private intake archives are excluded. The local
harness paths require this executor's configured toolchain; a different machine
must build matching dependencies rather than reuse nonexistent/stale objects.
