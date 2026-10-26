# Pedalboard bounded qualification review — 2026-10-26

## Source and result

Repository `djshellshoxxx/luthier`, branch
`codex/luthier-pedalboard-completion`, unchanged HEAD
`f1efeded706b9dc57da7e941ca856b17eaad5183`. No production or test source was
changed, rebuilt, or merged during this review. One coordinator, zero workers.

The existing GCC 14.2 Debug Pro ASan/UBSan binary has SHA-256
`198d528b5ad544f9346bf946cd5adb63222c5ad0062e565408b54cba0b2c793d`.
The stricter invocation was:

```sh
timeout --signal=TERM --kill-after=5s 600s env -u LSAN_OPTIONS -u LUTHIER_PERF \
  DISPLAY=:99 HOME=<fresh-temp-home> \
  ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:abort_on_error=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  /workspace/luthier/build-asan/LuthierTests_artefacts/Debug/LuthierTests \
  convolutionPreparesSafelyForTinyHostBlocks
```

**Passed: 1 test, 2,102 checks, 157.1 seconds, exit 0.** There was no
ASan/LeakSanitizer/UBSan diagnostic, including at process shutdown. No timeout,
new suppression, new software, duplicate full suite, or extra UI stress was
used. Log `asan-tiny-strict-f1ef.log` SHA-256:
`a4f72a279dd7a52ddc91e928719f07f9f29f80a7db881b5a9ca39fe125073a16`.

The original focused run remains unchanged in `asan-tiny-run.log`: leak and
allocator-mismatch detection disabled, 1 test / 2,102 checks / 152.4 seconds /
exit 0, log SHA-256
`553a9c28ccf77ef73caebe6c59c2001cbf1bc02fe973a2507ee1b5283d5a37d0`.
The earlier full-suite PB02 failure and subsequent source-pinned full-suite
pass are also retained, with their original 0.5 LU predicate.

## Why the suppressions were used

The prior invocation copied the recipe in
`docs/specs/TODO_SPECS_AND_BUILD_PLAN.md`, S13. That recipe sets both options
to zero. Historical Oct22 focused-run evidence records that LeakSanitizer
could not inspect `/proc` in its then-current execution environment. It does
not establish the same limitation now. The existing nightly workflow instead
sets `detect_leaks=1:abort_on_error=1:halt_on_error=1`.

The test runner replaces global new/delete with a matching malloc/free family
in `Source/Tests/CircuitTests.cpp`. Commit
`21688f2874ad5f1969432f69289f913e1b29e5ce` added matching nothrow operators on
Oct25. No current source or saved failure establishes that
`alloc_dealloc_mismatch=0` is still required. The strict result demonstrates
that neither suppression was necessary for this regression; the first run's
reduced diagnostic coverage was avoidable on this current workload.

This is not proof of the original allocator-overwrite cause, a complete
tiny-buffer/sample-rate matrix, a full-suite sanitizer pass, or TSan coverage.
Valgrind is absent. GCC's libtsan is installed; no TSan build/run was started.

## Library evidence identity and transfer limitation

The existing saved report is confirmed by the Library read API:

| Field | Confirmed value |
| --- | --- |
| Library file ID | `libfile_7d80351232d081918e326bac31b36d42` |
| Current file ID | `file_00000000081c820eb3c88169fb31c156` |
| Filename | `PEDALBOARD-2026-10-26-QA-CHECKPOINT.md` |
| Version | `1` |
| Stored size | 13,046 bytes |
| Upload-source SHA-256 | `73d261bc9657f9d6085ee38e1c6b0c9bf30872ee152d6927de52c8e95f1edcf8` |

That version contains the full-suite and original suppressed ASan summaries,
commands, provenance and hashes, **not the raw logs or binaries**. The full
Library text read is identical to the local upload source after restoring one
terminal LF omitted by the text API. This verifies text content but is not an
independent raw-file download checksum. The original local upload source is
preserved as `library-report-v1-upload-source.md` before the new local edits.
The Library report version above was not replaced in this review.

The supported materialization helper was freshly loaded from the Library
skill with its companion. Preparation succeeded for the exact report version
and the frozen finish-plan archive. Both helper transfers exited 1 with the
exact message `library file transfer failed: download failed`. No HTTP status
or underlying reason was exposed; this is not a confirmed 403 or auth denial.
No alternate transfer route or repeated retry was attempted. No signed URLs,
headers or credentials are included in this evidence.

The frozen plan archive remains unmaterialized:
`libfile_64d8843fc36881918fd49bf6c3cf8e3a`, version 0,
`three-project-finish-plans-20261026.zip`, expected SHA-256
`31eb1876637a7eb942cc8b3b9ede6363febcbb6b97b33e51c81f7c4c93a184b3`.
Its exact private contents were not recovered or reviewed. The local review
used the source-pinned owner instructions and existing repository handoffs;
old intake skeleton labels were not treated as current absence evidence.

## Corrections to earlier gate reports

- The current explicit read-only Git path is `/workspace/.git`. The actual
  repository Git directory is `/workspace/luthier/.git`, owned by this user
  on a writable mount. The earlier blanket claim that Git commits were
  filesystem-blocked was incorrect. No write probe, permission change, Git
  metadata mutation, commit or push was attempted in this review.
- CI push events have branch/tag filters. `ci-cadence.yml` lists specific
  integration branches, `codeql.yml` watches master/beta, native diagnostics
  watches its two diagnostic branches, and release watches `v*` tags. It is
  incorrect to say an arbitrary documentation-checkpoint push necessarily
  starts these workflows. No remote ref was contacted in this continuation.
- The saved CLAP console warning for `com.luthieraudio.luthierx1` matches its
  `create-id-with-trailing-garbage` negative test, reported PASSED. The wrapper
  prints this message and returns null for nonmatching IDs. Its configured
  descriptor is `com.luthieraudio.luthier`; the final validator report is
  21 tests / 18 passed / 0 failed / 3 optional-factory skips / 0 warnings.
  This is expected invalid-ID rejection output, not a demonstrated descriptor
  mismatch. No code or validator predicate was changed to classify it.
- The historical Actions 403 remains unresolved. Its recorded workflow-run
  GET needs repository read/Actions-read access; it is not evidence of a
  Checks-annotations failure or a categorical fine-grained-token limitation.
  The denied run was not re-read or inspected by another route.

## Remaining work: ready versus gated

| Work | Current classification / next bounded action |
| --- | --- |
| Tiny blocks at additional sample rates | Local coverage work, unattempted here. The current regression is 8 kHz; the historical S13 matrix also names 44.1/192 kHz and sizes 5/7/8. Preserve original predicates when extending it. |
| Linux Free exact-source build/test | Locally feasible with the installed toolchain, unattempted in this review; not an owner-only access gate. |
| Targeted retirement TSan | GCC libtsan exists; a separate instrumented target and existing-test selection would be needed. Runtime compatibility is untested, not a proven permission block. |
| Required named-model acceptance | Remaining implementation/quality work on preserved branches; do not replace failed numeric/CPU gates with registration or infer missing code from old intake labels. |
| Windows and both Mac architectures | Genuine binaries require native runners; current continuation explicitly prohibits CI. This policy hold is independent of the old denied Actions status read. |
| Hardware/DAW/listening | Requires actual devices/hosts and manual acceptance unavailable in this Linux runner. |
| Signing/notarization | No identity creation or new credentials authorized; disclose unsigned/unnotarized output where accepted rather than treating signing as a universal release prerequisite. |
| Git checkpoint | No current evidence that this nested repository's Git directory is read-only. Remains unattempted in this bounded review, not an owner-only filesystem gate. |
| Frozen private finish plan | Supported transfer failed generically; exact archive review is blocked until that supported transfer works. Independent source-pinned work can continue. |

No build matrix, GitHub retry, CI, merge, deployment, release, paid
infrastructure, credential/security/permission change, or separately denied
UI callback stress was performed.
