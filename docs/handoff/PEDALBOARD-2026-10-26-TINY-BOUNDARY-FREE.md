# Tiny-buffer boundaries and Linux Free continuation — 2026-10-26

## Source and scope

Same repository/executor; branch `codex/luthier-pedalboard-completion`.
Starting HEAD `f1efeded706b9dc57da7e941ca856b17eaad5183`. Only tests and
handoffs have changed; product source and original acceptance limits are intact.
Normal workspace, patch and build actions succeeded in this directly
user-authorized continuation. No alternate route around a rejection was used.

`Source/Tests/QA_Robustness.cpp` SHA-256 after the test extension:
`6221828e6b695d5a07e5485c7e7e97b22a02795fda5465afdf31fb74e333c2f4`.
The original file is preserved in the evidence directory as
`QA_Robustness.f1ef.cpp` (SHA-256
`1a6bbaba92fce8f0289655e963e2b1db331a42e9ff1c66af0ef5f3d230a6a5d5`).
The test-only patch has SHA-256
`bdceccfbea29ed8bf6fe0c273f9dfb01368d3061b5ada20f0c025c6b851f112d`.

The original ASan runner was copied before rebuilding, retaining SHA-256
`198d528b5ad544f9346bf946cd5adb63222c5ad0062e565408b54cba0b2c793d`.
The incremental ASan rebuild passed (exit 0); the extended runner is
`efa1f4baf48d905d9461434cadcb5fddf252c93aca5ebfa6f746f5a11f7cd75f`.
All files/logs here are under
`/workspace/luthier-evidence/tiny-boundary-free-20261026/`.

## Test extension

The original regression still renders **20,000 blocks per preset** at 8 kHz
and host sizes 1/2/3/4, using the same load/install/reload/finite/nonzero-output,
destruction and allocation-churn assertions. Those checks are factored into a
shared test helper; no predicate or original long-run count was reduced.

The added `Fuzz.convolutionTinyHostRateAndCapacityBoundaries` covers the 17
additional cells of `{8,44.1,192 kHz} x {1,2,3,4,5,7,8 samples}`, excluding the
four original long-run cells. Each added cell renders 2,000 blocks for each of
the same two presets. Each cell is logged before construction so an abort can
be attributed to a sample rate/buffer size.

Both tests completed together with `detect_leaks=1`,
`alloc_dealloc_mismatch=1`, no `LSAN_OPTIONS`, and halt/abort on ASan/UBSan
errors, under a 900-second process timeout (not reached). **2 tests / 11,029
checks / 407.5 seconds / exit 0**. The original long test kept its 2,102
checks (175.9 seconds); the additional matrix passed 8,927 checks (231.6
seconds). No sanitizer diagnostic occurred during execution or shutdown.
The log `asan-boundary-strict.log` has SHA-256
`61a270294fafdc81c60ebdb00b46843a6e64063f849a3177b515d2ed7e737349`.
This is selected-test qualification of the extension, not a new full-suite
pass or proof of the original byte-level overwrite cause.

## Linux Free

Fresh configure passed, using only the existing toolchain and local third-party
sources. Directory `/workspace/luthier/build-free-qualification-20261026`:
Release, FREE, LTO off, tests and renderer enabled, build string
`f1ef-boundary-free-qa`. No software was downloaded or installed. The explicit
build string plus source/test hashes above identify this test-only tree even
if a subsequent documentation/test commit advances HEAD.

The one fresh build is running (session `72841`):

```sh
source /workspace/luthier-toolchain/env.sh
ninja -C /workspace/luthier/build-free-qualification-20261026 -j4 \
  LuthierTests LuthierRender Luthier_Standalone Luthier_VST3 Luthier_CLAP
```

There are 1,650 build steps. Logs: `linux-free-configure.log/.exit` and
`linux-free-build.log/.exit`. No finished Free product, unit-test, validator or
audio-device acceptance is claimed yet. The renderer is included so CLI tests
can execute their real subprocess route rather than skip due to a missing tool.
The build directory is untracked and must be excluded from every Git add.
The GCC 14 optimized build emitted a retained `-Wstringop-overflow` diagnostic
at `CymbalPiece::render`'s `std::fill` length calculation; no runtime failure
has established an actual overflowing path. Do not silently drop that warning.

## Next actions and exclusions

1. The strict sanitizer selection is complete and green as recorded above.
2. Preserve a secret-checked local test/documentation checkpoint once stable,
   using the actual nested repository Git directory. No push is authorized in
   this bounded continuation.
3. Finish the one Free build, then qualify its Free-edition unit tests and real
   VST3/CLAP products using installed tools. Keep failures, unsupported cases,
   standalone launch smoke and actual device/DAW acceptance distinct.
4. Update this handoff with exact source/binary/log identities and terminal
   results before any next long tranche. Preserve any generated report before
   restoring the preceding tracked report; do not erase failure evidence.

One coordinator, zero workers. No pushes, CI, new downloads/software,
environment replacement, paid infrastructure, credentials/security/network
changes, merges/deployments/releases, Library input retry, or separately
denied additional UI callback stress is part of this continuation.
