# Pedalboard completion acceptance ledger

2026-10-26 owner-approved initial release scope: **six named models — Phase90,
CE-2, DD-3T, CE-2W, Dyna Comp and RC-5**. RC-5 replaces provisional Distortion+
to retain advanced dedicated looping. The other twelve stay on the original
eighteen-model roadmap with all source/specs/tests/failures preserved. This
changes initial delivery quantity, not any selected model's original numerical,
CPU, safety or shared-board gate. No selected named model is registered yet.
See [the retained scope comparison](../handoff/PEDALBOARD-SIX-MODEL-SCENARIO-2026-10-26.md).

Latest bounded local QA: the corrected-HOME Linux Pro full suite passed
2,806 tests / 14,935,613 checks at exact source `f1efeded706b9dc57da7e941ca856b17eaad5183`.
The subsequent test-only tiny-buffer extension passed strict ASan/LSan/UBSan
over 21 rate/size cells: 2 tests / 11,029 checks. At the same production code,
Linux Free freshly built all five local targets and passed VST3 strictness 5
(all 30 requested rate/buffer pairs for audio/automation) and CLAP validation
(18 passed, 3 unsupported preset-factory skips, 0 failures). The full Free
unit suite completed **failed:2/2790 tests,21/14,943,980 checks,5375.2s,exit1**.
Both failures are tiny-convolution body-IR reload assertions. The separate
generated robustness report has0findings and the original report was restored;
that does not clear failed assertions. Standalone smoke does not qualify hardware.
See the [QA checkpoint](../handoff/PEDALBOARD-2026-10-26-QA-CHECKPOINT.md)
and [boundary/Free handoff](../handoff/PEDALBOARD-2026-10-26-TINY-BOUNDARY-FREE.md)
for exact source/binary/log identities. Native Windows/macOS, remaining model
quality, TSan and manual acceptance gates are not cleared by these passes.

Current2026-10-26 evidence at source77a0e8767718b3511ec7e445b08c59add5ff0cb7:
[resume checkpoint](../handoff/PEDALBOARD-2026-10-26-RESUME.md).
The integrated reader-retirement/callback-lock correction builds all five Linux
Pro targets; the RefPedal suite passes411/12,246,497 checks and the explicit
30-second concurrent edit/process grind passes with zero callback allocations
or non-finite output. Linux VST3 strictness5 and CLAP validators pass. The fresh
standalone launches under Xvfb, but no audio device/DAW acceptance was performed.
The corrected native Mac run was last observed building at08:32 UTC; a later
status read was denied with403 Forbidden, so its current result is unknown.
Existing Windows/Mac artifacts are from older source and do not qualify this
integration. Full models/CPU/RT/platform/manual gates remain open; no release
acceptance is claimed. Historical intake rows and earlier counts below are not
current implementation absence reports.

Latest local execution evidence and exact binary checksums are in the current
[resume checkpoint](../handoff/PEDALBOARD-2026-10-26-RESUME.md); legacy Linux
build6 evidence below remains source-pinned to its earlier commit and is not
substituted for the fresh integrated build. Historical intake columns and prior
failures retain their original provenance.

Intake: 2026-10-23 UTC. Coordinator branch `codex/luthier-pedalboard-completion`.
Source PR79 HEAD: `0b319c0b9b480d17605674374818711de94f7cb7`.
Master: `0f62c3d67bdba055b41e66c7ab8ac9ab4b728662`.
The clean saved checkout had no uncommitted JSON codec; state work is an explicit
reconstruction, not a recovery. No AGENTS.md exists in this repository or its
workspace parent. Existing handoff rules apply except where superseded by the
user's explicit authorization to coordinate, integrate and release all platforms.

Normative scope: `spec/reference-pedalboard/specs/IMPLEMENTATION_PLAN.md` v1.1,
its errata, BOARD_CORE, GENERIC_PEDALS, MIDI_EXPRESSION, UI_BOARD_STYLES and the
individual pedal specifications. This ledger does not narrow those requirements.

## Current implementation, 2026-10-24 continuation

The Intake column below records historical conditions; it is not a current
absence report. The integrated checkpoint has a real engine and routing,
validated state/snapshots/assets, three host processing placements, 373 appended
board/slot parameters, MIDI mapping, a board window and procedural UI, and ten
registered generic processors plus explicitly provisional legacy models.
Named drive/dynamics/Muff candidates exist on the DSP work branch but remain
unregistered where original numeric/CPU gates fail. Four named modulation/delay
processors exist with reviewed Phase90 correction and retained original oracle;
their registration remains gated. Four named looper wrappers are still incomplete,
although paged transport, undo/redo, asset storage and original rhythm helpers
exist. These distinctions do not waive any original feature requirement.

Freshly verified source at `02e4e1fcf48ff8be29b5a3dc841cc883a4698653`:

- Linux Pro Standalone, renderer, VST3, CLAP and test executables built. The
  selected native run passed 289 of 290 tests / 6,839,270 of 6,839,271 checks.
  The failure is an independent router initialization fixture inheriting a
  previous internal tempo; the live 50 ms source-change glide is intentional.
- VST3 pluginval strictness 5 passed at all five rates through 192 kHz and
  blocks 1/16/37/64/481/2048. CLAP failed three state-load checks, resetting
  unused-slot and rejected structural APVTS values; this is being fixed without
  validator exclusions. Built binaries alone do not qualify release gates.
- PR77's normalization, looper reprepare, room geometry and packaging fixes are
  merged locally into the candidate, retaining both the existing release-decay
  and startup test thresholds. PR77 was not merged into master, so its
  old-source automatic beta2 publication was not triggered.
- The scoped portable JSON fix and qualified engine/MIDI/UI/state/helper commits
  are integrated serially. Combined-candidate compilation/tests are next; lane
  checks do not establish combined acceptance. See the continuation handoff.

## 2026-10-25 focused continuation evidence

Source64292744 contains only three scoped direct-host MIDI fixture corrections
reused from Claude b3ec7a41 after original RED and registered-buffer A/B execution.
Combined32tests/1392checks pass; assertions, licensing branches and production
processing are unchanged. The original JM39 return-seam loss is measured,not
inferred from the passing same-name Rhythm route. See
[the focused ledger](PEDALBOARD-DIRECT-HOST-MIDI-20261025.md).

The saved b5 Linux production build finished2026-10-24(exit0). Fresh2026-10-25
actual CLAP18pass/0fail/3unsupported skips and VST3 strictness5(all30requested
rate/buffer combinations) pass. Test-only rebuilding is distinct from that saved
production compilation. Full cross-platform CI/unit acceptance remains failed;
no release/tag/publication has occurred. Historical intake descriptions below
remain historical,not statements that these implemented modules are absent.

## Finite implementation gates

| Gate | Owner branch suffix | Required implementation and verification | Intake |
|---|---|---|---|
| WP1 | coordinator | Frozen interfaces, registry, SPSC, headless boundary, header self-containment | partial; ownership regression |
| WP2 | engine | Compile/publish/retire immutable topology; series, trims, mix, bypass, latency, zero callback allocation/locks | model commands only; engine skeleton |
| WP3 | engine | Parallel/AB/loops/feedback/reamp; cycles, equalization, watchdog and fault behavior | skeleton |
| WP4 | state | Bounded JSON, unknown/forward schema preservation, atomic restores, 8 snapshots, assets | skeleton; prior codec absent |
| WP5 | host-midi | Clock/router/maps/curves, offsets, flood/drop counters, RCU | skeleton |
| WP6 | host-midi | Foot gestures, learn, expression, reserved CCs, stable UUID targets | skeleton |
| WP7 | dsp | Double DSP helpers, denormals, oversampling latency, finite guards | skeleton |
| WP8 | dsp | Generic boost/overdrive/fuzz/compressor; A-PED + specific tests | skeleton |
| WP9 | timefx | Generic chorus/phaser/tremolo/wah; sync/expression + A-PED | skeleton |
| WP10 | timefx | Generic delay/reverb; RT60/freeze/tails/tap + A-PED | skeleton |
| WP11 | dsp | DS-1/TS808/SD-1 specification DSP and evidence labels | skeleton |
| WP12 | dsp | Distortion+/RAT/Dyna Comp specification DSP and evidence labels | skeleton |
| WP13 | dsp | Four Muff variants, gate/mids/expression and alias/CPU tests | skeleton |
| WP14 | timefx | CE-2/CE-2W/Phase90/DD-3T; BBD, sync, mode transitions | skeleton |
| WP15 | state | LooperCore/Ditto/Nano360/RC-1; gestures, exact undo, bounded storage | experimental prototype only |
| WP16 | state | RC-5, 99 memories, rhythm patterns, storage worker, clock master | skeleton |
| WP17 | ui | Single board view, dock/popout, keyboard reorder, geometry, no obscuring controls | prototype overlay |
| WP18 | ui | 7 styles and 7 mounts, LOD/cache/contrast/DPI tests | skeleton |
| WP19 | ui | Descriptor controls, footswitches, generic faces, accessibility | skeleton |
| WP20 | ui | Original drive/Muff/dynamics faces and evidence chips | skeleton |
| WP21 | ui | Original modulation/delay/looper faces and progress displays | skeleton |
| WP22 | ui | Cables/browser/MIDI panel/learn; errors and keyboard navigation | skeleton |
| WP23 | host-midi | PRE/LOOP/POST hooks, append-only automation, state, lifecycle, PDC, legacy null | prototype taps only |
| WP24 | coordinator | Integration sweeps, sanitizers, RT audit, soak, CPU, validators, native builds, packages | not accepted |

Each lane maintains a separate coverage document with exact commits, commands,
tests and remaining requirements. Integration is serial; workers commit only
their isolated worktrees. The coordinator owns hub build/QA/release changes.

## Baseline evidence and reconciliation

PR verification run 39748967619 failed at PR79 HEAD. The build's merge SHA in
artifact names is `5f1fa5c6715feffc2f625e1d73dcda19b2db3371`. All six Free/Pro
Windows/macOS/Linux cells compiled. Every unit-test cell failed; do not quote
focused BoardModel passes as a full green suite.

* Linux Pro job 119726160793: 2/2420 tests failed, 2/2662947 checks, 4635.6 s.
  `RefPedalApi.skeletonFilesHaveOwners`: BoardModel.cpp lacks first-line owner.
  `Combo.releasedStringIsDampedQuickly`: Resonator note42 only 24.4 dB down at
  output versus 35 dB threshold after note-off. Investigate body-vs-string
  fixture and existing branches before changing behavior or thresholds.
* macOS Pro job 119726160762: 1/2420 tests failed, ownership header above.
  VST3 pluginval strictness5 reports SUCCESS. The combined validator step failed:
  AU pluginval received `Bus error: 10` at 13:14:28 UTC. CLAP's summary was
  21 run, 18 passed, 0 failed, 3 skipped. This is a combined-step failure, not
  evidence that VST3 or CLAP individually failed.
* CodeQL run39748967626 was cancelled; no successful analysis claim.
* PR79 has no discussion comments. Open PR78 is README/wiki only. Remote
  histories are retained; no branch deletion, history rewrite or force push.

## Release gates (all required for feature-complete publication)

| Gate | Required evidence | Status |
|---|---|---|
| R1 functionality | Shared WP1-WP23 contracts plus the six selected named models at original gates; twelve other named models explicitly deferred, no registered placeholders | pending; full eighteen-model roadmap retained |
| R2 legacy/full tests | Prior suites plus reference suites green, both editions, 3 OS | Linux Pro full suite passes at f1efeded; Free full suite fails2/2790 tests(21 checks); current native-OS qualification remains open |
| R3 numerics | 44.1/48/88.2/96/192kHz; buffers1/16/37/64/481/2048 plus extremes; deterministic/nonfinite/denormal checks | pending |
| R4 RT/concurrency | Callback allocation/lock hooks, publication/retirement stress, ASan/UBSan/TSan | pending |
| R5 state/topology | Seeded1000 malformed states, migrations/unknown preservation, repeated loads, cycles | pending |
| R6 stress/performance | Requested30-second grind plus full30-minute seeded soak, memory/CPU logs, 10k UI operations | pending |
| R7 native formats | Standalone smoke, VST3 pluginval>=5, CLAP validator on built source per platform | pending |
| R8 manual acceptance | Actual hardware/DAW matrix, focus/automation/audio-device checks, listening | unavailable here; explicitly separate |
| R9 packaging | New immutable version, genuine native artifacts, checksums, provenance/install/license/known issues, secret scan | pending |

Existing releases at intake: published prereleases `v0.1.0-alpha1` (all three
OS), `v0.1.0-macos-beta1`; draft `v1.0.0-beta1`. Never replace these assets or
reuse tags. Choose a new version only after defining the accepted release scope.
Unmet feature gates require a clearly labeled draft/prerelease; no feature
complete assertion. Unsigned/unnotarized status must be visible.

## Continuation

Seven slots were actually running at dispatch: coordinator + six workers.
Worktrees: `/workspace/luthier-workers/{engine,dsp,timefx,state,host-midi,ui}`.
No extra environment or duplicate continuation timer was created.
Local GitHub CLI token failed authentication; repository connector reads and
public git fetch succeeded. Git push dry-run succeeded; real write outcome must
be checked independently. Build dependencies are being staged under the
workspace because this account cannot install system packages.
