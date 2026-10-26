# Luthier resumed implementation checkpoint — 2026-10-26

Sole integration owner: this existing Codex task; only djshellshoxxx/luthier.
Current local branch starts at **e18694042367087ac1f4a8397afdbd05c45464d8**
(`codex/luthier-pedalboard-completion`). The integrated retirement adapter and
callback-lock correction below are committed as
**77a0e8767718b3511ec7e445b08c59add5ff0cb7**. Linux products and tests below
were built/run from the identical source tree before commit; only coverage and
handoff prose followed those executions. The commit is the checkpoint candidate;
verify its remote checkpoint ref before treating it as published.
Prior published checkpoint22d6b18d is preserved. No reset, force push, master
merge, release/tag, deployment, additional environment or timer was used.

## Completed in this continuation

- Harmless workspace access succeeded; original tracked CQ03 edits and untracked
  evidence/caches were preserved. Fresh remote readback confirms checkpoint22d6,
  original native6d9 and stopped Claude1f646a65. PR81 f36037f1 is a donor, not a
  replacement baseline; its eight generated pycache binaries were not imported.
- Owner-approved CQ03 exact-ID correction is committed13042441956ebb8fb017c8d855795078f5452439
  and verified on remote codex/luthier-checkpoint-20261026-resume. Only exact
  ref_board_cpu_cap is admitted. All original preset/quality predicates remain.
  The original actual8b RED is1/1177 with one failure. Test-only build exit0;
  fresh corrected run **3 tests/1217 checks/exit0**, including70 active factory
  presets,12 policy and28 actual processor checks. BinarySHA256
  4761d086625a79947d1898ab88fee917b8652951e2ea1729363475a720add2a0.
  Production products were unchanged; this does not claim a fresh product build.
  Evidence: ../coverage/CQ03-APPROVED-EXACT-ID.md.
- Native verification command failure traced to Apple lipo's parser: tokens
  following -verify_arch are all architectures, so the executable must precede
  the option. Corrected diagnostic product/validator calls and existing beta
  packaging call. Strict executable fixtures reproduced RED then passed; all
  **19 helper/release-guard tests pass** and bash syntax passes. No thresholds,
  validators, missing-architecture checks or test predicates were removed.
  Evidence: ../coverage/native-lipo-evidence/PROVENANCE.md.

## Actual native artifact matrix

Original run: https://github.com/djshellshoxxx/luthier/actions/runs/40198638051
Frozen source6d9b5943217d6e0872f65aea6514ebf40fc789e1.

| Product | Actual result | Artifact | Qualification |
|---|---|---|---|
| Windows x64 Pro standalone/VST3/CLAP | Build, focused GUI/host tests and required validators passed; VST3 strictness5 SUCCESS; CLAP21 run/17 pass/0 fail/4 skips/0 warnings |12602394197,145418213bytes|UNQUALIFIED DIAGNOSTIC; not a full-suite/release/hardware claim|
| macOS Pro universal standalone/VST3/CLAP | All native compilation/linking passed; five malformed lipo invocations failed; focus/validators did not execute because verified architecture was unavailable |12602221877,183216035bytes|UNQUALIFIED failed diagnostic; neither slice presence nor runtime acceptance inferred|
| Linux Pro standalone/VST3/CLAP | FreshOct25 source5d2 build8b compiled all five targets; actual VST3 strictness5 all30rate/buffer pairs and CLAP18/0/3unsupported pass; engine161/219579 plus300s grind1/8 and search31/8952 pass |saved local matching products and evidence|Linux-only evidence; product Source matches6d9; priorOct24 products are distinct|

Windows artifact SHA25678f5f38746020c0b89915328e901a42a0f06030d96ed6ec74d7e0fe5c8c7ef31.
Mac artifact SHA2569ba0dd2cbbb49b92a1bc24226a0bea5b62d1a6ed2462f14fbc5dedeb1be3ec07.
Artifacts are retained in Actions, expire2026-11-01; they are not Releases.
Source/checksums/licenses/install references and unsigned/ad-hoc signing truth
are included by the diagnostic bundler. Full release signing/notarization is open.

Corrected **Mac-only run40292011494** was last independently observed building
at **08:45 UTC** at source8d6600f8:
https://github.com/djshellshoxxx/luthier/actions/runs/40292011494
Mac job121469040555; Windows job121469042266 is deliberately skipped. No old
Windows result was reattributed to this newer source. The exact read command was
`gh run view 40292011494 --json status,conclusion,headSha,headBranch,jobs --jq '{status,conclusion,headSha,headBranch,jobs:[.jobs[]|{name,status,conclusion,steps:[.steps[]|{name,status,conclusion}]}]}'`.
It returned `failed to get run: Get "https://api.github.com/repos/djshellshoxxx/luthier/actions/runs/40292011494?exclude_pull_requests=true": Forbidden`.
No rate-limit header or authentication reason was provided. Current
run state is therefore unknown, not still-building or passed. Do not retry or
reroute that denied status read, or push to codex/luthier-macos-diagnostics while
it may be active. Original native branch remains frozen6d9. The Mac source also
predates the integrated retirement adapter, so it cannot qualify that newer
code.

## Current bounded work and integration decisions

The actual roster at this checkpoint is one coordinator and zero active workers;
the focused retirement reviewer and native-command worker have completed their
bounded slices. No other project's owner was used. Existing worker
branches/patches remain preserved.

The saved eight-file engine retirement-progress adapter7cc140070429516462b6f8883e345f1905e338ba9252cadd1d321fb814151047
was applied locally at the existing latest Source and independently reviewed.
The review approved the completed-reader protocol, six aliases, MAX exhaustion
and original eight-owner bound. Its callback-lock issue is now corrected with
a synchronous exact-call seam: active runtime is pinned, control lock is paused
only around the existing config/snapshot saveExtra calls, and is reacquired on
normal return or exception before the pool continues. No callback runs on audio.

Test-first evidence: the focused snapshot-restore reentry regression timed out
(exit124) with the seam omitted, then the restored seam passed. Final local tests:
ControlRace5/1042 and RefPedalRetirement12/11725 pass. The broader native RefPedal
selection passed411/12,246,497 checks in219.1s, including the10,000-operation
real processor/editor UI lifecycle stress. Test `saveExtraExceptionReacquires...`
verifies a throwing callback leaves the guard usable for a successful retry. The
exact full five-target Linux build and actual plugin validators are recorded
below. Existing lifecycle limits remain: one
non-nested audio reader, one serialized message-thread publisher, and prepare/
release/destruction with stopped audio plus quiescent model mutations. Full native
TSan requalification of the integrated callback seam remains open.

Other source is deliberately unmerged: host backend release-retry9167c620 has
focused original-object evidence but incomplete full-native closure; preview
transport f29404c7 changes processor layout and is ABI-unqualified; corrected
Kaiser looper a0733cf3 and Phase90 e8138ee2 retain named-model/full CPU/runtime
gates and remain unregistered. Preserve these histories; do not replace the
current tree with any divergent worker or Claude branch. Required models and
original numerical/CPU gates have not been dropped. PR80 remains draft at old
b5bec374; master remains0f62c3d6. No outstanding whole-branch merge is qualified.

## Blockers, exclusions and next commands

The current Library finish-plan archive resolved libfile_64d8843fc36881918fd49bf6c3cf8e3a
version0/199831bytes, but the current supported consumer-local transfer helper
exited1: 'library file transfer failed: download failed'. No readable bytes,
checksum verification or plan contents were recovered. This is a generic
transfer failure, not a proven access denial; no retry or guessed URL was used.
Source-pinned owner instructions and repository handoffs continue to govern
independent work. Do not publish the private multi-project archive.

The earlier **additional** CommandPalette lifecycle UI stress task failed before
editing with an automatic cybersecurity flag. It remains blocked, not pending
an ordinary user click. Do not retry/rephrase/reroute it. Completed search/UI
coverage and independent audio/native work remain valid. No security-setting
change or bypass is authorized. CQ03's separate approval is resolved/applied.

Next: preserve and verify the newly committed source checkpoint, then continue
the bounded remaining full-suite/runtime/CPU/native qualification. The latest
Mac-run status read returned403 Forbidden; do not retry it. Do not reuse
old-object evidence across layouts or reattribute an artifact built from source6d
or8d to the integrated source below.
Full unit/all models/CPU/whole-processor no-lock still have open gates:20 existing
processor try-locks remain. Manual audio-device/DAW, second Mac runtime, both
editions, strictness10/LTO release and installer/signing/notarization acceptance
are unexecuted. No release can be called feature-complete on these results.

Use /workspace/luthier-toolchain/env.sh for local commands. CQ03 command:
`xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests CQ03`.
Helper command: `python3 -m unittest scripts.test_native_diagnostics scripts.tests.test_release_workflow_guards`.
Read the next checkpoint commit and verify remote full SHA before integrating.
Keep concise and detailed handoffs at each milestone; bundle/checksum only
source/evidence, excluding objects/caches/credentials/private intake packs.


## 2026-10-26 integrated Linux build and QA

Source commit **77a0e8767718b3511ec7e445b08c59add5ff0cb7**. Exact command
`cmake --build build --target LuthierTests LuthierRender
Luthier_Standalone Luthier_VST3 Luthier_CLAP -j3` exited **0** on the integrated
retirement adapter and callback-lock correction. The 16-step Ninja build freshly
compiled the shared sources and linked test, renderer, standalone, VST3 and CLAP
products. Evidence and logs are local at
`/workspace/luthier-evidence/engine-retirement-source8/`.

- `xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests RefPedal`
  passed **411 tests / 12,246,497 checks / 219.1s**. It includes
  `RefPedalUiLifecycle.tenThousandNativeOperationsAndEditorDestruction`:
  10,000 seeded real processor/editor operations, 21 editor destructions, 546
  native closes, and accepted drag/drop, gestures, resize, docking and popouts.
  The same selection includes the 30-second finite/allocation-free soak below.
  This is the existing authorized regression suite, not the separately blocked
  additional UI callback stress request.
- A separately recorded exact 30-second run,
  `LUTHIER_SOAK_SECONDS=30 xvfb-run -a
  build/LuthierTests_artefacts/Release/LuthierTests RefPedalSoak`, exited **0**:
  seed `0x51a7`, 3,000 edits/296 rejections, 103,116 audio blocks, 45,491,342
  samples, zero callback allocations and zero invalid samples. Evidence:
  `30s-soak-final.log` / `.exit`.
- Final focused regressions: `RefPedalControlRace` **5/1,042** and
  `RefPedalRetirement` **12/11,725**, both exit0. Approved CQ03 remains
  **3/1,217**, 70 factory presets, unchanged global-quality exclusions.
- Linux VST3 pluginval strictness5 exited0 over rates 44.1/48/88.2/96/192kHz
  and blocks 1/16/37/64/481/2048. Linux CLAP validation exited0:
  **21 run / 18 pass / 0 fail / 3 unsupported skips / 0 warnings**.
- Linux standalone was launched under Xvfb and stayed alive until the deliberate
  10-second timeout (exit124). The environment logged absent ALSA sequencer and
  unwritable font-cache warnings. This establishes a process/display smoke only,
  not audio-device, listening or DAW acceptance.

Fresh product SHA256: standalone
`7f23bcd937b1ab9ef7d4e2ebd82ff818d855a11a5826cbae4da556651ba28619`, VST3
module `0751d4b763e77bf29d1b610290ec597335a066dc5490e6ddadc63bbd72348161`, CLAP
`9ce33ddd24dabaa41214f8b049a676b4c8f58502478d721fbb7834a89959168a`. These are
Linux local diagnostic products from the integrated source only. A successful
Linux build/validator does not qualify Windows/macOS or a release. Windows and
Mac artifacts from source6d are stale for this changed engine; do not reuse or
publish them as current-source products.

This checkpoint's portable incremental Git bundle contains commit
`c4af8fbd81ee45379f0089a02310a0b14689dd1f` and requires base
`e18694042367087ac1f4a8397afdbd05c45464d8`. File:
`/workspace/luthier-evidence/luthier-pedalboard-77a0e876.bundle`, 18,806 bytes,
SHA256 `8c40bd647cfdf830c511b0002bf0c633003bf5a40a564dab3492e38b6589f185`.
Library file `libfile_cd3a6c8a4bc48191a7d5c2af59fdba7e`, version0, was created
and the local version xattr was set and verified. It contains only source and
checkpoint commits, not build caches or the private plan archive. The remote
checkpoint ref `codex/luthier-checkpoint-20261026-resume` read back at
`c4af8fbd81ee45379f0089a02310a0b14689dd1f` after a fast-forward push; the older
remote `codex/luthier-pedalboard-completion` at `b5bec374` was left untouched.

## Full-suite probe and environment correction

The unfiltered test invocation
`source /workspace/luthier-toolchain/env.sh && env -u LUTHIER_SOAK -u LUTHIER_SOAK_SECONDS xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests`
was stopped after 15m18s (wrapper exit143) once its default user-documents paths were found outside the
allowed writable roots (`/workspace` and `/tmp`). Its original log remains
`/workspace/luthier-evidence/engine-retirement-source8/full-suite-after-integrated-build.log`.
It recorded the expected nonwritable preference/preset symptoms: AS26
`persistenceAndScope` (6 checks), AutoArticulation `offAmountZeroAndNoRulesAreTheSameAsNothing`
(preset loads), `everyFactoryPresetPlaysEveryPhrase` (no presets), and
`snapshotsAndPresetMorph` (need two presets). An accidental probe
`build/LuthierTests_artefacts/Release/LuthierTests --list-tests | head -40`
showed that `--list-tests` is unsupported and started another unfiltered
runner; PID6719 was stopped after6m02s, while the primary run was kept long
enough to preserve its initial red evidence before it too was stopped. This
overlap is an additional contamination. Do not treat the partial run as a
source-suite result.

To verify the environmental diagnosis, the three localized regressions were
rerun serially with a dedicated writable home:
`HOME=/tmp/luthier-test-home`, `XDG_CONFIG_HOME=$HOME/.config`,
`XDG_CACHE_HOME=$HOME/.cache`, `XDG_DATA_HOME=$HOME/.local/share`, and
`XDG_DOCUMENTS_DIR="$HOME/Documents"`. AS26 passed6/6; AutoArticulation
passed21/21; `snapshotsAndPresetMorph` passed48/48. The run wrote93 factory
preset files into that isolated home. Full unfiltered suite acceptance remains open; if
resuming it, use the isolated home above and do not run another test process
concurrently against the same per-user state.
