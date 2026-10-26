# Pedalboard QA and native-build checkpoint — 2026-10-26

## Current source and ownership

- Repository: `djshellshoxxx/luthier`.
- Integration branch: `codex/luthier-pedalboard-completion`.
- Source baseline before the current tested changes: `51df65e9258c96dfdd3ec5047cd15e475b82f5b1`.
- Tested PB-02/tiny-block implementation commit: `63178fcbebcbb4bfd5ca97012ef168baa2d5324e`.
- Corrected host-state QA fixture and refreshed fuzz report commit:
  `34ac45e9ee8a2298042aa44a8cfd83fad794ac50`. Its test source was built and
  the focused state test plus full selected fuzz filter passed. No product DSP
  source changed in this follow-up.
- Results below are pinned to `f1efeded706b9dc57da7e941ca856b17eaad5183`.
  A later test-only boundary extension is recorded separately in
  [the boundary/Free continuation](PEDALBOARD-2026-10-26-TINY-BOUNDARY-FREE.md);
  its results are not substituted for this full-suite source identity.
- One active coordinator; no implementation workers are currently active. A
  narrow read-only review completed. Other Luthier worker branches/worktrees
  remain intact and were not merged or modified in this checkpoint.
- The present executor grants `/workspace` as a writable root. Work stayed in
  `/workspace/luthier`.

## Completed tests and artifacts

The earlier full suite is terminal and failed; it must not be confused with the
focused green runs below. Its log is
`/workspace/luthier-evidence/engine-retirement-source8/full-suite-writable-home.log`
(SHA-256 `22ea411849136dba0ce7b38771116df5ef66093f92b0ec02e6bae5e41ecf1f02`).
It ran 2,805 tests and 14,933,248 checks over 5,711.4 seconds (95.2 minutes),
with one failed test: `PresetPreview.PB02_shippedClipsMatchFreshRenders`, where
Wah Funk Rhythm decoded at −33.18 LUFS versus −32.66 LUFS, a 0.52 LU difference
against the unchanged 0.5 LU limit. This earlier failure remains part of the
record. Within that same run,
`seededRandomConfigurations` passed 2,000 configurations/checks and
`structuralChangesWhileAudioRuns` passed both checks. The log does not record an
exact source commit SHA. It predates the current PB-02/tiny-buffer candidate.

The corrected-HOME full-suite rerun completed on exact source
`f1efeded706b9dc57da7e941ca856b17eaad5183`, with `LUTHIER_PERF` unset. Command:
`DISPLAY=:99 build/LuthierTests_artefacts/Release/LuthierTests` against the
existing Xorg dummy display (`-nolisten tcp`). It exited 0 after 5,870.9
seconds (97 minutes 51 seconds): **2,806 tests and 14,935,613 checks, all
passed**. Log:
`/workspace/luthier-evidence/engine-retirement-source8/full-suite-clean-home-f1ef-final.log`
(SHA-256 `d5a42e592ca30740824c789e083d7a1da6fdb687bc7de8a2a1f9374080dbaeac`);
the adjacent `.exit` file contains `0`. PB02 passed with the unchanged 0.5 LU
limit: max spectrum RMS 0.787941 dB, max loudness delta 0.458321 LU, 2,931,405
Ogg bytes, q0.6 retained only for Wah Funk Rhythm, and three cap fallbacks.
The 2,000-configuration test, `structuralChangesWhileAudioRuns`, and
`AS26_persistenceAndScope` passed. The explicit 1–4-sample heap-corruption
regression `Fuzz.convolutionPreparesSafelyForTinyHostBlocks` also passed in
this full-suite run: 2,102 checks in 15.0 seconds. This is runtime evidence;
the separate ASan/UBSan run below provides instrumentation evidence.

On the same source `f1efeded706b9dc57da7e941ca856b17eaad5183`, the existing
GCC 14.2/CMake/Ninja toolchain was configured for a Debug Pro `LuthierTests`
target with `LUTHIER_SANITIZE=address,undefined`, then built with
`ninja -C /workspace/luthier/build-asan -j2 LuthierTests` (exit 0). The single
focused invocation
`DISPLAY=:99 HOME=<fresh-temp-home> ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:alloc_dealloc_mismatch=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 build-asan/LuthierTests_artefacts/Debug/LuthierTests convolutionPreparesSafelyForTinyHostBlocks`
passed: **1 test, 2,102 checks, 152.4 seconds, exit 0**, with no ASan/UBSan
diagnostic. The test disabled leak detection and allocator mismatch detection;
this is a focused memory-boundary check, not a full-suite sanitizer run. Build
log SHA-256 `1e475ad966475b144041d7817d3d14e07fd3bef56e0cf4d9efd8c7b043ef8ac4`,
run log SHA-256 `553a9c28ccf77ef73caebe6c59c2001cbf1bc02fe973a2507ee1b5283d5a37d0`,
and test binary SHA-256
`198d528b5ad544f9346bf946cd5adb63222c5ad0062e565408b54cba0b2c793d` are
preserved under `/workspace/luthier-evidence/engine-retirement-source8/` or
`/workspace/luthier/build-asan/`. The test did not rewrite the tracked
`docs/review/QA_ROBUSTNESS.md`; the wrapper verified its original and final
SHA-256 both equal
`e8993815226dd95e871671ac0effdb7d2e482d2abe1cc80efb2a02d4441d78d6`.
Two setup attempts before this successful configure failed before compiling
tests: `cmake` was absent from the default `PATH`, then JUCE's `juceaide` build
could not find the toolchain's X11/Freetype headers. Both were resolved by
using the already installed CMake and sourcing
`/workspace/luthier-toolchain/env.sh`; the successful configure/build logs are
preserved separately.

A stronger review reran the same binary and regression with both earlier
suppressed checks enabled, no `LSAN_OPTIONS` suppressions, and a fresh HOME:
`ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:abort_on_error=1:halt_on_error=1`
plus `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The command was bounded
by `timeout --signal=TERM --kill-after=5s 600s`. It passed **1 test / 2,102
checks / 157.1 seconds / exit 0**, including normal process exit with no
ASan/LeakSanitizer/UBSan diagnostic. Log: `asan-tiny-strict-f1ef.log`, SHA-256
`a4f72a279dd7a52ddc91e928719f07f9f29f80a7db881b5a9ca39fe125073a16`.
The original suppressed command/result above remains historical evidence.
The suppressions were copied from the old S13 plan, not established as a
current harness requirement: older focused evidence records a `/proc` LSan
limitation, while `nightly.yml` explicitly enables leak detection. The
nothrow allocation-family correction `21688f2874ad5f1969432f69289f913e1b29e5ce`
also predates this tested source. Both suppressions were unnecessary for the
current focused workload. See `PEDALBOARD-2026-10-26-QUALIFICATION-REVIEW.md`.

On the current tested source tree derived from the baseline above:

| Check | Result | Evidence |
| --- | --- | --- |
| Fuzz filter including tiny-block and host-state regressions | 13 tests, 1,144,712 checks, 178.1 s, pass | `fuzz-host-notify-final.log`, SHA-256 `31d4255beb228d150141695365b448cef5214cf5f30deb143816660bdd447dfe` |
| Tiny host blocks 1, 2, 3, 4 at 8 kHz, two loaded presets, 20,000 blocks/preset/size | Included above; 2,102 checks, 17.0 s in the final filter, pass | same fuzz log; pre-fix failure details in `BUFFER-SIZE-1-4-HEAP-CORRUPTION-2026-10-26.md` |
| Preset preview suite including PB-01/02/03 | 23 tests, 1,671 checks, 162.6 s, pass | `preset-preview-final.log`, SHA-256 `4a200d9be0b94477e3417969d2961a11e6b96ba1f53093e10e607c8c96395a67` |
| Reference-pedal race + 30-second concurrent soak | 6 tests, 1,054 checks, pass; 3,000 edits, 123,798 blocks, 54,615,551 samples, callback allocations=0, locks=0, invalid=0 | `refpedal-soak-final.log`, SHA-256 `371c9907270a47fdc26d540682691bb320d4694d0573aaeec71cf1a013cf9048` |
| PB-27 performance | 1 test, 9 checks, 61.0 s, pass; median factory render 859.7 ms, slowest 1,145.9 ms | `pb27-perf-final.log`, SHA-256 `fee2c81a1cd0a4aeb4ce8b8cbb1ed350ae1e597e11e8c77194eafca09a73d37b` |
| Fresh Linux Pro Standalone, VST3, CLAP build | Exit 0 | `linux-formats-final-build.log`, SHA-256 `9015558eadf6bbc5221a5944b17a68924ac1ff6100158c10ecd2e991b96bb1f2` |
| Linux VST3 pluginval strictness 5 and CLAP validator | Exit 0; VST3 passed; CLAP 21 tests: 18 passed, 0 failed, 3 skipped | `linux-formats-validation.log`, SHA-256 `807c8c803d4c834e53fbf64921a2424f1fd6698860244a8f3d1fdec9f6936e4f` |
| CLI render of shipped previews | Exit 0; 70 Ogg files, 2,931,405 bytes total, 214,323 bytes under 3 MiB cap | `render-previews-final.log`, SHA-256 `c5a1fa11f18a636642c828e2a047bd572322415aedd091d53d3470e7d6d0c599` |
| Standalone launch smoke under Xvfb | Stayed open for 10 s; expected timeout status 124; not audio-device acceptance | `linux-standalone-smoke.log`, SHA-256 `5ca930287de272c63c96482c554e9fd62c83e309185cac44b8552f3368800c40` |

Linux product hashes:

- Pro Standalone: `3d8efa395d4e119d1460ec7c2a855d259ea74aae298e1ba86b4dbb352bac3502`
- Pro VST3 ELF: `d42f2e534024e004c042241db500de5103bdd0d3aa81c753933a94cd160ae2da`
- Pro CLAP ELF: `ecd337691797b30f4bc9a2cdd403ca956762e0c0e54ef8a46a093d82dc29195f`

These are local Linux diagnostics, not qualified release artifacts. The
standalone smoke logged that ALSA `/dev/snd/seq` is unavailable in this runner.
CLAP validator skipped three preset-discovery tests because that optional
factory is not implemented. The console message for an ID ending in `x1`
corresponds to the validator's `create-id-with-trailing-garbage` negative test,
which passed. The wrapper logs a mismatched ID and returns null, as required
by that test. The final validator summary is 0 warnings; this message is not
evidence of a configured plugin-ID failure. No validator exclusion was added.

## Source changes and unresolved risks

- PB-02 retains q0.5 as the default. If q0.5 decoded loudness misses the
  existing 0.5 LU threshold, q0.6 is retained only if it meets the same
  threshold and the 64 KiB per-clip cap. Current PB-02 passed with 0.787941 dB
  max spectrum RMS, 0.458321 LU max loudness delta, 2,931,405 total bytes, q0.6
  retained for Wah Funk Rhythm, and three cap fallbacks. No acceptance limit was
  relaxed. Release builds must regenerate preview files and bump the plugin
  version so old sound-hash caches are invalidated.
- Tiny-buffer candidate prepares cabinet/body/IR-variant convolution capacity
  at a minimum of 8 while preserving host callback sizes 1–4. The fixed
  regression passed as above, with both impulse responses installed and
  reloaded. The original heap corruption was reproducible in a prior run, but
  the exact allocator overwrite/root cause is unproven. The targeted
  ASan/UBSan test passed with leak/mismatch detection enabled, but the full suite was not instrumented and Valgrind
  remains unavailable; retain the original finding as an unproven root cause.
- The prior 16 randomized host-state re-save mismatches were reproduced and traced to the
  QA helper calling `AudioProcessorParameter::setValue()` directly: this
  changes `getValue()` but bypasses APVTS's listener adapter, leaving raw
  parameter atomics stale. The test mixed randomized values in the saved
  parameter block with default engine/extra-state values. The helper now uses
  `setValueNotifyingHost()`, which follows the host automation path; the focused
  16-seed round-trip test passed (1 test, 1 check, 9.7 s) with 0 findings. This
  is a test-fixture correction, not a production state-code change. The complete
  13-test fuzz selection then passed (1,144,712 checks, 178.1 s) with 0 report
  findings. The seed-0 pre-fix JSON pair is preserved locally under
  `/workspace/luthier-evidence/engine-retirement-source8/` as
  `host-state-seed0-before-invalid-fixture.json` (SHA-256
  `4b175653c42f0904c4c43ac56380ce5b3dba9ab6cf8ecf42a2f2203758c3cbb0`) and
  `host-state-seed0-after-invalid-fixture.json` (SHA-256
  `9e441b76ade20cd2b0d2c2653864ab98622c419fa5e6901932e0bc74ddce870c`). The
  corrected-HOME full suite now passes on exact source
  `f1efeded706b9dc57da7e941ca856b17eaad5183`; see the result and log hash above.
- A separate additional UI callback stress operation was automatically
  security-blocked and remains unexecuted. Do not retry or repackage it. The
  existing UI lifecycle tests in the full run are separate evidence.
- No actual Windows or macOS build/validator has passed on this current source.
  A Mac-only diagnostic Actions run was observed building earlier, but reading
  run `40292011494` was denied with HTTP 403 Forbidden (no reason/rate headers
  returned); its final result is unknown and must not be represented as a
  pass. The recorded request was `GET /repos/{owner}/{repo}/actions/runs/{run_id}`
  with `exclude_pull_requests=true`; GitHub documents repository read access
  and `Actions: read` for fine-grained tokens for this endpoint. The separate
  check-run annotations endpoint requires `Checks: read`; it was not the
  recorded request. The token type, its effective permissions, and actual cause
  of the 403 remain unknown. See [workflow-run access requirements](https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run)
  and [check-run annotation access requirements](https://docs.github.com/en/rest/checks/runs#list-check-run-annotations).
  No Windows candidate for this current source was verified.
- No Free build, full-suite sanitizer run, Valgrind run, actual hardware/DAW
  audio acceptance, or complete current-source cross-platform test matrix has
  passed. The targeted tiny-buffer ASan/UBSan result above is narrower evidence.
  Linux build and validator success is not a release gate.

## Next steps

1. Preserve the handoff patch and private evidence archive. Correction to the
   prior permission report: the read-only path is `/workspace/.git`; this
   repository uses `/workspace/luthier/.git` on a writable mount. No Git write
   probe or commit was attempted in the bounded review. Push workflows have
   branch/tag filters; a checkpoint push does not automatically trigger every
   listed workflow. No GitHub access, push, CI, merge, or release was attempted
   here. The last verified remote checkpoint is `f1efeded`; its live ref was
   not re-read during this continuation.
2. If additional local qualification is authorized, consider a targeted
   Valgrind run or broader sanitizer selection. The tiny-buffer regression is
   now green under ASan/UBSan; keep its scope and runtime distinct from full
   sanitizer coverage.
3. Run supported Windows standalone and Pro/Free plugin targets, both macOS
   architectures, and their real validators on current source. Preserve any
   failures. The separate denied Actions read is not permission to retry or
   route around access.
4. Only after platform and full QA gates pass, produce release installers and
   VST3/CLAP packages with provenance, checksums, licenses, installation notes,
   and clear signing/notarization status. No release was created by this
   checkpoint.

## Recovery

Start in `/workspace/luthier`, verify the branch and clean/dirty state with
`git status --short` and `git rev-parse HEAD`, then read this handoff and the
small-buffer handoff linked above. Test logs are local under
`/workspace/luthier-evidence/engine-retirement-source8/`; verify their hashes
before relying on them. Exclude Python cache directories from all Git adds.
