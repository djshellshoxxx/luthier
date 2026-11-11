# Pedalboard native build6 checkpoint

2026-10-25 UTC. This is unfinished development evidence, not a release.
Tested production source: `280e182a8f033457f3d84384b9917a024b1d1ed9`.
Query `git rev-parse HEAD` for the later documentation checkpoint.

## Fresh execution

The following command ran afresh in this resumed environment, starting
11:35:09 UTC and finishing 11:39:09 UTC, exit 0:

```sh
source /workspace/luthier-toolchain/env.sh
cmake --build build --target LuthierTests LuthierRender Luthier_Standalone Luthier_VST3 Luthier_CLAP -j3
```

All five Linux Pro targets linked. The source was frozen throughout this build
and subsequent qualification. The saved October24 b5 products and 813 matching
test link inputs were separately preserved before this build; those older
binaries are not evidence of fresh compilation or of the changed source.

Fresh actual CLAP validation: 21 run, 18 passed, 0 failed, 3 unsupported preset
discovery skips, exit 0. Fresh VST3 pluginval strictness5, seed0x51a7: SUCCESS,
exit0, five sample rates 44100/48000/88200/96000/192000 by six buffer sizes
1/16/37/64/481/2048, with GUI and automation enabled and no exclusions.

Binary SHA256:

| Product | SHA256 |
|---|---|
| CLAP | `31a4e970ea4a1979607e78fb2b2f7fd184c818770f2993546f052a0f0b30218a` |
| VST3 module | `229afff0cef807304dc25ff2ac5dd51161f4514518e61b1a79295cec9485ca52` |

Exact commands, logs, before/after hashes and timestamps are retained under
`/workspace/luthier-evidence/20261025-host-validators-280e182a-build6`.
The build log and source manifest are `native-integrated-build6.log` and
`native-build6-source.json` in the same evidence root. Unit/state/routing/UI
and stress qualification is still running; do not infer full-suite acceptance.

Fresh focused engine/API/routing/CPU-admission/retirement/MIDI/RT-allocation
selection:119 tests/197413 checks,exit0. Fresh direct-host MIDI/bus/E46 selection:
35 tests/743 checks,exit0. Both used test binary SHA256
`143e9c59daa6a8c1f1dbd988306f9853bbb60fdaacfb10a540643a1dc407eb67`.
These do not qualify the later eight-plan retirement bound or E69 memory work.

Factory-dependent QA exposed a setup error rather than missing recovered files:
constructing a processor does not invoke the lazy bank refresh. With an empty
writable executable-adjacent Factory directory, unchanged ON03 forces ordinary
refresh/generation and passes12checks. It generated70 active presets plus four
migration backups; all1253 original resource files remained byte-identical.
This controlled test bank matches the separately verified b5 bank exactly.
No production product resource directory was changed during the validators.

## Integrated scoped work

Since the verified public MIDI checkpoint9773fca8, this source includes the
append-only bus-layout regressions; lossless JSON overlay/canonical helper and
failure-only host-state diagnostics; compact rejection display and reentrant
drag fixes; actual10000-operation UI lifecycle test; Generic B/C CPU admission
metadata; deterministic cabinet asset resolution; retirement reserve-before-
publication and paired nothrow test allocators; bounded MIDI map retirement
preallocation; and safe macOS failure-evidence collection scripts.

Each source slice has lane evidence; the new coherent build and validators
establish only their stated combined-source checks. The macOS collector still
needs its workflow invocation and a real macOS failing run. Runtime extra-state
hardening, the eight-plan retirement bound, complete prepared-memory accounting,
GUI attribution/lifetime test corrections and packaging/release provenance work
remain separate reviewed integration candidates.

## Other author preservation

Fresh remote readback verified Claude's final preservation branch
`claude/peaceful-carson-vzl4e4` at
`1f646a65e7d4db5f4f06592a119abf7234463b3f`, one documentation/log commit atop
907dedb9. Its final handoff reports a clean tree, no unpushed work, and stopped
Luthier edits/timer. No whole divergent branch was merged. Its authoritative
handoff is `docs/handoff/INTEGRATION-HANDOFF-2026-10-25.md` on that ref.

All six skipped commits are preserved by current counterparts, checked against
root280e and Claude907 source, not inferred from commit counts:

| Skipped originals | Current counterpart / evidence |
|---|---|
| 11f30424, 8ecf6a5a | Same MIDI enable patch; included in b7757366/228b6d13 with the newer revision guard. Fresh frozen-b5 run: seven existing regressions,50 checks pass. |
| 6fcd9b90 | Same full send/return patch as root eb86da56 and Claude e0e2709b. |
| 5a716ec7 | Same send/return production patch; its additional sparse-slot regression exists through bc33a4b6. |
| c65a4a69 | Same full feedback patch as root72232279 and Claude b4e9ab31; nine associated regression bodies match. |
| 6d9ce297 | Same full runtime-capture patch as root c42ab4eb and Claude e240d107; capture hook and regression body match. |

Historical routing/capture passes remain historical; matching patches alone do
not establish current runtime acceptance. New combined-source suites are running.
Claude's blanket ref_* GUI/search and CPU-ID exemptions were rejected. Its
unbuilt Windows/macOS hypotheses and unqualified model registration were not
adopted. Four circuit helpers match already preserved DSP-lane code; CPU/alias/
tonal model gates remain open. Failed historical Linux ZIPs exist, but are not
release candidates. Nothing was deleted, force-pushed or overwritten.

## Remaining gates and next commands

Seven actual agents are active at this checkpoint: coordinator, engine, DSP,
host/MIDI, state, timefx and UI, in the existing seven slots. No other project,
new environment or timer is used. Source ownership is reconciled; no worker is
waiting solely for the parent. The pending CQ03 exact-required-ID specification
decision remains unapplied; the original assertion is unchanged.

The original Pro P-Style Bass35dB output-decay assertion still fails near34.7dB;
no threshold was lowered. Whole-processor RT probing found20 try-lock attempts
per callback despite zero observed allocator/blocking-lock calls in that limited
matrix. These are open gates, not accepted exceptions. Prepared-memory closure,
named DSP/looper acceptance, full native suites, Windows/macOS both architectures,
signing/notarization, real hardware/DAW/listening and manual acceptance remain
incomplete. No tag, master merge or new release has been made.

The historical macOS exact JSON pair and crash backtrace remain unavailable.
Failure-only new evidence must be labelled new, not recovered. Private Library
version30 resolved, but the unchanged supported helper exited1 with
`library file transfer failed: download failed`; it exposed no underlying HTTP
reason. No readable archive bytes/hash or private packets arrived. This is a
failed byte transfer, not a proven access denial, absent capability or guessed
consumer path. No retry/reroute was attempted.

Next: finish disjoint fresh280e suites/stress; preserve failures; integrate only
reviewed scoped corrections serially; save/push each tested-status milestone;
then run the smallest justified native Windows/macOS diagnostics. Publish new
versioned artifacts with checksums/source/licenses only after applicable gates,
clearly disclosing unsigned/unnotarized and manual gaps. Do not dispatch a full
known-bad matrix or claim compilation alone as feature completion.
