# Pedalboard continuation handoff

Updated 2026-10-24 00:55 UTC. Implementation is ongoing; no release/tag created.

- Repository: `/workspace/luthier`, branch `codex/luthier-pedalboard-completion`.
- Local checkpoint before this handoff: `3e5b6ca6` (query HEAD for later commits).
- Last verified pushed PR80 checkpoint: `e5d350e5994c96e02277f9c8a00606073df47213`.
- Draft PR80: https://github.com/djshellshoxxx/luthier/pull/80, base master.
- PR79 ancestry preserved: `0b319c0b9b480d17605674374818711de94f7cb7`.
- Master at intake: `0f62c3d67bdba055b41e66c7ab8ac9ab4b728662`.
- No uncommitted JSON codec was found at intake; persistence was reconstructed.

All seven slots are active: coordinator plus engine, dsp, timefx, state,
host_midi and ui. Worktrees are `/workspace/luthier-workers/{engine,dsp,timefx,
state,host-midi,ui-next}`. Workers own scoped commits; coordinator integrates
serially. Do not use other repos, environments or a duplicate continuation timer.

Read `docs/coverage/PEDALBOARD-ACCEPTANCE.md`, lane coverage documents and
`spec/reference-pedalboard/specs/IMPLEMENTATION_DECISIONS.md`. All original
requirements remain. Do not relax numerical or quality limits to claim success.
Candidate DSP commits `daaa5fb8`, `40d3503d`, `99234e0d`, `4696bd91` are WIP,
not accepted product implementations. Shared TanhADAA2 `b1aa654b` is independently
reviewed and integrated as an experimental helper, without pedal acceptance.

## Actual checks

- Complete coherent Linux Pro executable at source `05bbdb8e`: **206 selected
  tests, 5,649,492 checks, all passed, 40.1s**. Includes real processor automation
  without a message pump and 1,000 writes/second. Log: `native-pedalboard-checkpoint2.log`.
- Previous native checkpoint: three failures (legacy sparse move, ownership
  headers, leaked test factory). Fixed without weakening checks; passed above.
- Separate legacy `releasedStringIsDampedQuickly` still fails: P-Style Bass
  note42, 34.7dB against unchanged 35dB, 1/25 checks. NoBody fixture isolation
  helped the prior resonator failure but did not close this gate.
- Requested 30-second grind passed. A separate immutable core snapshot
  `347f0ce558b72fb5f3a37f3e5b15c552f7c8cd95` completed the full 1,800-second
  concurrent soak: **180,000 edits, 6,913,402 blocks, 3,049,960,869 samples,
  callback_new=0, invalid=0; 3 tests/6,111 checks/0 failures**. Driver exit0,
  1810.19s including collection. Peak sampled RSS26,628KiB; kernel HWM29,212KiB.
  `grind-30m{.log,-memory.jsonl,-result.json}` preserve evidence. This predates
  newer integrations and counts C++new, not Cmalloc or wrapper locks.
- Seeded 1,000 malformed live-state loads: 7,004 checks passed in a coherent
  focused harness at `72232279` plus the test, now committed as `075d943b`.
- Worker ASan/UBSan/TSan/UI/stress results are scoped precisely in lane ledgers.
  No final full suite, all-platform validator or hardware/DAW acceptance claim.

## Integrated since the last executable; rerun coherently

AB/send-return/feedback with guards, three optional aux sends, bounded raw/output
MIDI, E55 runtime-extra capture, E57 routing snapshot/input definitions, physical
UI controls/APVTS attachments/drag rollback/stress, and bounded paged looper
storage/compaction are integrated. Named processors remain registration-gated.
The next full application executable has not yet validated these integrations.

Major work still active: 12 named drive/dynamics/Muff processors; paged transport
and four named loopers, memories/rhythm; persistent asset service E59; extended
50-field host/MIDI mapping and canonical tempo delivery; numerical/CPU closure;
UI visual/idle/accessibility completion. UI screenshots revealed clipping and
mojibake; fix in progress. Native Xvfb peer idle probe failed BadAtom, not passed.
E58 Phase90 correction is a reviewed mathematical proposal awaiting measured
adoption; the alternative 2x FIR added15samples and failed CPU, so was rejected.

## Build and CI

`source /workspace/luthier-toolchain/env.sh` enables workspace-local CMake/Ninja,
dev dependencies and Xvfb. No system/root/security changes were made. JUCE8.0.10
SHA `3af3ce009f6a02f6fa651008fffb5b41743a9fab`, CLAP extension
`55525c9858d4b25687be7759a5e0f70eccef218e` are pinned.

No full builder is currently running. Freeze C++/shared dependencies before:
`cmake --build build --target LuthierTests LuthierRender Luthier_VST3 Luthier_CLAP Luthier_Standalone -j3`.
Use only one full builder. Local identity is pinned `pedalboard-local-qa` to
avoid recompiling on documentation commits; final release provenance needs SHA.
The JUCE standalone MIDI reserve patch is now applied and verified by CMake:
constructor65536 bytes, exact pre/post hashes, unknown sources refused. Four
isolated tests pass for both original and patched trees. No broad compiledefine.
Linux pluginval1.0.4 and clap-validator0.3.2 are installed/runnable; product
validation is pending.

Current CI for pushed e5d350e5: PR verification40045916131, CodeQL40045916125.
The previous40042179060 was cancelled by the checkpoint push. Let long native
CI jobs finish; use a separate backup branch for later milestones instead of
continually cancelling PR80 runs. Results never cover later local commits.
GitHub CLI token is invalid; supported connector and managed git transport work.

## Persistence and release

Verified Library archives (incremental bundles require the first full bundle):

- `/luthier-intake-and-dsp.bundle`, full history through e733841e,
  SHA256 `39b8d85e0a5552e03df65abfd81ee027ac21d28c4b8862650f7822db0e3cb93a`.
- `/luthier-checkpoint-0299fe14.tar.gz`, SHA256
  `904e4d04369889365ad5e695662be2aa404562289018d0c82324b76657f6a9f8`.
- `/luthier-checkpoint-e5d350e5.tar.gz`, accepted and WIP worker refs plus evidence,
  SHA256 `4fe422a16d141ce1a303e4470f2e3c711d0f53e72306c85c84fd9a8dccaa90e6`,
  Library ID `libfile_676f4ed72bd48191bf9fee2162ef8ee2`.

Verify checksums and `git bundle verify` before restoration. Exclude credentials,
keys, build/dependency caches and user assets from source/release archives.
Existing releases v0.1.0-alpha1, v0.1.0-macos-beta1 and draftv1.0.0-beta1 are
immutable. Choose a new version under project policy. Incomplete acceptance
requires an accurately labeled draft/prerelease; never claim feature complete.
Signing secrets and manual hardware/DAWs are not available here.

The environment briefly reported unavailable and immediately returned; checks
found work/processes intact. No action was denied and no new environment started.
