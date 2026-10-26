# Luthier resumed implementation checkpoint — 2026-10-26

Sole integration owner: this existing Codex task; only djshellshoxxx/luthier.
Current source before this documentation checkpoint: **8d6600f8d55abf153821fbd1a78ce83b4cf68118**.
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

Corrected **Mac-only run40292011494** is actually building at source8d6600f8:
https://github.com/djshellshoxxx/luthier/actions/runs/40292011494
Mac job121469040555; Windows job121469042266 is deliberately skipped. No old
Windows result was reattributed to this newer source. Do not push handoff-only
changes to codex/luthier-macos-diagnostics and cancel its run. Original native
branch remains frozen6d9. Re-query the run for actual current results.

## Current bounded work and integration decisions

One coordinator plus one focused review worker were actually active at this
checkpoint. The inexpensive native-command worker completed its bounded slice.
No old seven-worker count is asserted as current, and no other project's owner
was used. Existing worker branches/patches remain preserved.

The saved eight-file engine retirement-progress adapter7cc140070429516462b6f8883e345f1905e338ba9252cadd1d321fb814151047
applies cleanly to current Source. Independent review supports the completed-
reader protocol, six aliases, MAX exhaustion and original eight-owner bound.
It identified one callback-lock closure issue: compiler PedalPool::acquire
can call saveExtra while holding the new nonrecursive control guard, contrary
to the unconditional unlocked-callback claim. Engine reentry could deadlock.
Existing production saveExtra implementations do not demonstrate that reentry;
no observed product deadlock is claimed. Adapter remains **unmerged** pending
a test-first narrow correction and coherent affected-target rebuild/host tests.
Keep original RED, TSan race, sanitizer/progress evidence and lifecycle limits.

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

Next: inspect the Mac-only run without cancelling it; retain exact failed step/
logs/artifacts if red. Finish the scoped retirement callback-lock review/fix,
then coherently rebuild affected dependencies and run actual engine/history/
memory/host/state/automation tests, requested30s and longer final-source stress
and actual plugin validators. Do not reuse old-object evidence across layouts.
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
