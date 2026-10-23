# Pedalboard continuation handoff

Updated2026-10-23 UTC. This is ongoing implementation, not a completed feature or release.
Source checkpoint before this handoff: `fa909efd5bef43d5c80e9a4157a6c4e0db2af4b9`. Query `git rev-parse HEAD` for the handoff commit and later updates.
Repository `/workspace/luthier`, branch `codex/luthier-pedalboard-completion`.
Draft PR80: https://github.com/djshellshoxxx/luthier/pull/80 (base master; preserves PR79 ancestry).
PR79 source remains0b319c0b9b480d17605674374818711de94f7cb7; master remains0f62c3d67bdba055b41e66c7ab8ac9ab4b728662.

## Active ownership

All7 available slots actually running: coordinator plus engine,dsp,timefx,state,host_midi,ui.
Isolated worktrees `/workspace/luthier-workers/{engine,dsp,timefx,state,host-midi,ui}`.
Workers commit scoped changes; coordinator cherry-picks only scoped commits and integrates serially.
State now owns transferred LooperCore/looper models (WP15/16); timefx continues WP14.
No other repo was modified, no new environment launched and no duplicate timer created.

## Verified evidence

- Coordinator registered sweep+30-second concurrent grind:3tests4619checks0failures.
  Seed0x51a7;3000edits,125904blocks,55544648samples,new-allocationcount0,invalidoutput0.
  This predates some later integrations and is not a full suite or sanitizer claim.
- Worker core checkpoint38tests56155checks; state16cases5254checks plus prior scopedASan;
  MIDI13cases64checks with scopedASan/UBSan and TSan; generic/time effects focused suites pass.
  Per-lane coverage documents distinguish exact instrumentation and untested gates.
- Current native Linux Pro build started through1626 compile/link steps, log `/workspace/luthier-evidence/build.log`.
  Full plugin/test/renderer targets requested; files changed during this initial build.
  It MUST be rerun coherently after integration before executing or trusting binaries.
- CI40042179060 and CodeQL40042179099 are running for pushed checkpoint6f025613e97eab0d9556f0d22236e288fa8401f2.
  Six native OS/edition cells are active. Their eventual results do not cover later local commits.
- Previous CI39748967619: allsix compiled; ownershipheader failure allcells, LinuxProresonator release fixture failure.
  Ownership fixed, prior fixture isolation candidate8470a6e8 cherry-picked asf29cd5d1 (same decay threshold).
  New full-suite result pending. MacPro VST3 passed, CLAP18passed/3skipped; AU crashed during96kautomation.

## Remaining work

Read `docs/coverage/PEDALBOARD-ACCEPTANCE.md` and all `PEDALBOARD-*.md`. All24packages remain in scope.
Engine serial/parallel active; advanced routing, MIDI event application/echo, RT host overrides in progress.
Host currently has323 appended lanes and lossless raw board state; timer-only control bridge is being replaced byE44 audio snapshots.
Named pedal library remains incomplete; do not register placeholders or label complete.
UI is model-backed dock/popout with descriptor controls; bespoke faces, accessibility/stress and detailed layouts pending.

DSP contradictions are explicit engineering decisions, NOT relaxed acceptance limits:
E37 reference-only linear-phase oversampler; legacy oversampler unchanged. Current candidate stillfails alias/CPU gates and is NOT integrated.
E38 boost HP2Hz and OD reference loudness calibration preserve original frequency/level thresholds.
E40 higher-order fractional delay to meet original14kHz±.5dB threshold; mandated cubic fails.
Parent requires independent review plus original vs revised criteria and rationale visible before spec corrections merge.
Peers DSP/timefx review these before integration. AdditiveE32-E44 interfaces recorded in per-lane ledgers; consolidate errata serially.

No30-minute soak/full sanitizer/full regression/native validator/DAW/hardware acceptance has passed yet.
No release assets or tags were created. Existing immutable releases remain unchanged.

## Build environment

`source /workspace/luthier-toolchain/env.sh` exposes workspace-local CMake/Ninja, native headers/pkg-config paths and Xvfb.
System package install lacksroot; internal mirror403. Public Debian packages were staged locally, no system security changes.
JUCE8.0.10 at `/workspace/luthier/ThirdParty/JUCE` (SHA3af3ce009f6a02f6fa651008fffb5b41743a9fab), shared symlinks in workers.
Pinned CLAP extension55525c9858d4b25687be7759a5e0f70eccef218e.
Native g++ plus shared focused JUCE objects `/workspace/luthier-toolchain/objects`.
For coherent full build: `cmake --build build --target LuthierTests LuthierRender Luthier_VST3 Luthier_CLAP Luthier_Standalone -j3`.
Do not launch competing full builds. Parent manages continuation monitor on this same task.

## Persistence and access

GitCLI tokeninvalid, but supported repository connector reads/writes and gitfetch/push work.
Last verified remote branch6f025613e97eab0d9556f0d22236e288fa8401f2; later commits local until next checked push.
Verified complete-history early bundle `/workspace/luthier-evidence/luthier-intake-and-dsp.bundle` saved in ChatGPTLibrary.
SHA25639b8d85e0a5552e03df65abfd81ee027ac21d28c4b8862650f7822db0e3cb93a; contains earlye733841e only.
A later incremental bundle can be applied atop that full base; verify bundle and checksums before attaching.
Secret scanner reports1969findings in29trackedfiles, chiefly existing calibration hashes. New changed-file findings are known parameter IDs/testUUIDs; full release archive inspection remains required.
