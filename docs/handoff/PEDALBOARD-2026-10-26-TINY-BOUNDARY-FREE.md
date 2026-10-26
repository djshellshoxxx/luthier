# Tiny-buffer boundaries and Linux Free continuation — 2026-10-26

## Source and scope

Same repository/executor; branch `codex/luthier-pedalboard-completion`.
Starting HEAD `f1efeded706b9dc57da7e941ca856b17eaad5183`. Only tests and
handoffs have changed; product source and original acceptance limits are intact.
The tested extension and handoffs are committed locally as
`d5b64ab650665d94825eaaf460f0024558825f38`; no push was attempted. The
incremental bundle `d5b64ab6-over-f1efeded.bundle` verified successfully
(SHA-256 `0766645982df50b4fa61b648c3b169ee916f7ff06951cc0dd0a0ab62d752d800`),
and requires the original f1efeded source. The private Library checkpoint
`libfile_6170df4828e88191be1b452634243d3c` preserves this completed sanitizer
milestone plus an explicitly partial Free build log; it is not a completed
Free qualification report.
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

The one fresh build completed with exit 0 (original session `72841`):

```sh
source /workspace/luthier-toolchain/env.sh
ninja -C /workspace/luthier/build-free-qualification-20261026 -j4 \
  LuthierTests LuthierRender Luthier_Standalone Luthier_VST3 Luthier_CLAP
```

All five requested targets built across 1,650 steps. Build log SHA-256:
`eeb2ab745fe0bb90e96b17c78e967863a6c1ffc17d4c734dfb7ae945b066503e`.
All product executables/modules were identified as genuine Linux x86-64 ELF.
The renderer is included so CLI tests can execute their real subprocess route
rather than skip due to a missing tool. Full Free unit acceptance is still
pending; the listing confirms 2,790 registered tests.

| Product | SHA-256 |
| --- | --- |
| Standalone | `ad37d8c61da6f2dec2fabac84511e035c0b00b2d4e399e2b8aa0268f7f0d9121` |
| VST3 module | `34b0c885bab04d67ffde5721bd6555ccb924844111579b7ae7104a0917fca45b` |
| CLAP module | `af8bc40f1ead729ab56136827f63f0fd34bead8e3e8e6f16f7bed96ea3283e36` |
| Renderer | `9ff21228c5c6810717d17d6b4eecc92caf08c8f6b79504d44460bf556b9f5576` |
| Test runner | `1ec4c2717071c65fe475ab09d02f0a1d3cf23b9349770ebc18801e98a8afc675` |

The installed repository validation route passed (exit 0): VST3 pluginval
strictness 5 and CLAP 21 tests / 18 passed / 0 failed / 3 optional preset-factory
skips / 0 warnings. Log SHA-256
`224602c36efea6dea84a9c8d930108ee762dd479f4a7b7f08c5ba6c251d22d5e`.
Because pluginval's defaults covered only 15 pairs, a supplemental strictness-5
run explicitly tested 44.1/48/88.2/96/192 kHz x buffers 1/16/37/64/481/2048.
Both audio and automation logs contain all 30 pairs; the run exited 0 with
SUCCESS. Log SHA-256:
`371d760328ca27a4ec9bdfecb0408023839cf116c43365c75d35dab28584a91e`.
No GUI test was excluded. The external Steinberg validator was not installed
or run; pluginval explicitly reports that optional check unavailable.

The Standalone remained open for 10 seconds under Xorg :99; timeout exit 124
was expected. Its ALSA log says `/dev/snd/seq` is unavailable. This is launch
survival only, not audio-device/DAW/listening acceptance. Smoke log SHA-256:
`7f4899a2173c4c01d1325de76cd0bcfaa9311ef64dbfd90e0dd8ca45fe48ac12`.
The build directory is untracked and must be excluded from every Git add.
The GCC 14 optimized build emitted a retained `-Wstringop-overflow` diagnostic
at `CymbalPiece::render`'s `std::fill` length calculation; no runtime failure
has established an actual overflowing path. Do not silently drop that warning.

## Next actions and exclusions

1. The strict sanitizer selection is complete and green as recorded above.
2. The secret-checked local test/documentation checkpoint and incremental
   bundle are saved. No push is authorized in this bounded continuation.
3. Build and native Free validators are green as recorded above. Run the one
   clean-home Free full suite; preserve its generated robustness report and
   inspect its finding count independently of process exit. Do not rerun the
   already completed Pro full suite or start a duplicate Free suite.
4. Update this handoff with exact source/binary/log identities and terminal
   results before any next long tranche. Preserve any generated report before
   restoring the preceding tracked report; do not erase failure evidence.

One coordinator, zero workers. No pushes, CI, new downloads/software,
environment replacement, paid infrastructure, credentials/security/network
changes, merges/deployments/releases, Library input retry, or separately
denied additional UI callback stress is part of this continuation.
