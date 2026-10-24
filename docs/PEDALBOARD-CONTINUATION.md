# Pedalboard continuation handoff

Updated 2026-10-24 UTC. Implementation and QA are ongoing; no release/tag has been created.
Coordinator source checkpoint: `05bbdb8e233964a37a874455c28291c5b0f15ba2`. Query `git rev-parse HEAD` for this handoff and later commits.
Repository `/workspace/luthier`, branch `codex/luthier-pedalboard-completion`.
Draft PR80: https://github.com/djshellshoxxx/luthier/pull/80 (base master; retains PR79 ancestry).
PR79: `0b319c0b9b480d17605674374818711de94f7cb7`; master: `0f62c3d67bdba055b41e66c7ab8ac9ab4b728662`.

All seven slots were verified actually running: coordinator plus six isolated workers.
No other repository, cloud environment or duplicate continuation timer was used.
Workers own scoped commits; coordinator integrates serially. Current worker checkpoints:

```json
{
  "engine": "950dbf93bfa2fc37ac3d2188eaefae3328097a1e",
  "dsp": "dcef63b33f4ee639aeccbf891c0b9f19c3407152",
  "timefx": "30f154e7696f60005a020329baa179d6b6499856",
  "state": "fb5abc01adcdca2d9071aa4e0b6ded622e19e771",
  "host-midi": "99dd72b48d3b8294b40233184f68d2c248908863",
  "ui-next": "f379e78b4fd0a15cd8bd569593f39630b1bec24e"
}
```

Read `docs/coverage/PEDALBOARD-ACCEPTANCE.md`, lane coverage files and
`spec/reference-pedalboard/specs/IMPLEMENTATION_DECISIONS.md` before continuing.
The clean intake had no uncommitted codec; persistence was reconstructed explicitly.
Full original requirements remain; failing numerical/CPU gates are never relaxed to claim acceptance.
DSP candidate commits `daaa5fb8` / `40d3503d` are preserved WIP, NOT accepted integrations.

Current integrated implementation includes series/parallel and sidechain/reamp, E44 host audio snapshots,
E47 telemetry/E48 intent/E50 model API, channel-local/focus MIDI maps, generic effects,
real modulation/delay candidates (registration gated), state/snapshots, bounded looper compaction,
bounded disk pager (not yet model-integrated), and physical UI face profiles (not full UI acceptance).
Engine950dbf93 (send/return, E52 MIDI, E53 cancel), host3cf84e36 (curves), DSPdcef63b3
(short FIR optimization) are queued beyond this frozen source snapshot; verify dependencies and reviews.

Actual complete Linux Pro executable at bfc01448 ran151tests/5,033,311checks:3failedtests/39checks,
39.6s. Failures: sparse legacy slot move, ownership first lines, leaked test.event registration in sweep.
Fixes e375401e/da61b859 are now integrated without changing thresholds. Second coherent native build
is running from `05bbdb8e233964a37a874455c28291c5b0f15ba2`: session36684, `/workspace/luthier-evidence/native-integrated-build2.log`.
Do NOT edit C++/shared JUCE sources until this build ends; documentation edits are safe.
Then run `LuthierTests RefPedal ReferencePedals ReferencePhraseLoop` under Xvfb, preserve failures.
Separate releasedStringIsDampedQuickly baseline stillFAILS P-StyleBass note42:34.7dB vs35dB unchanged
(1/25checks), despite prior legitimate NoBody fixture isolation. Coordinator must diagnose; no slack added.

30-minute concurrent core soak is running (session79995, PID23607):
`grind-30m.log`, `grind-30m-memory.jsonl`, eventually `grind-30m-result.json` in evidence directory.
It uses a separate immutable core snapshot before current integration; do not claim final-feature coverage.
Earlier requested30s grind passed3tests/4619checks:3000edits,125904blocks,55,544,648samples,
callback_new0/invalid0. New hook counts C++new only, not C malloc.
Worker scoped ASan/UBSan/TSan results and exact instrumentation limits are in lane ledgers.
No full final-suite/sanitizer/native-validator/hardware/DAW/manual acceptance is yet established.

CI40042179060 and CodeQL40042179099 are running for pushed6f025613e97eab0d9556f0d22236e288fa8401f2.
Six platform cells were still compiling at last check. Results do not cover later local commits.
GitHub CLI token invalid; supported GitHub connector reads/PR edits and managed gitfetch/push work.
Last verified remote branch is6f025613; source after that remains local until a tested checkpoint push.
Use CI for genuine Windows/macOS. Existing releasesv0.1.0-alpha1,v0.1.0-macos-beta1,draftv1.0.0-beta1
are immutable. New incomplete acceptance can only be accurately labeled draft/prerelease.

`source /workspace/luthier-toolchain/env.sh` enables local CMake/Ninja/dev dependencies/Xvfb.
System installation lacksroot; public Debian packages are staged locally. JUCE8.0.10 pinned
3af3ce009f6a02f6fa651008fffb5b41743a9fab; CLAP extension55525c9858d4b25687be7759a5e0f70eccef218e.
Build identity is pinned `LUTHIER_BUILD_STRING=pedalboard-local-qa` to avoid whole rebuilds on docs commits;
final release provenance must use exact source SHA. One full native builder only; no source mixing.

Verified complete-history early bundle saved in Library: `/luthier-intake-and-dsp.bundle`,
SHA25639b8d85e0a5552e03df65abfd81ee027ac21d28c4b8862650f7822db0e3cb93a, contains e733841e.
Incremental0299fe14 package also saved in Library, SHA256904e4d04369889365ad5e695662be2aa404562289018d0c82324b76657f6a9f8.
Newer checkpoint bundles require that complete-history base; verify checksums and `git bundle verify`.
Release archive secret scanning remains mandatory; prior tracked scan flags chiefly existing calibration hashes,
known parameter IDs/test UUIDs. Never copy credentials, private keys, dependency caches or build caches to archives.
