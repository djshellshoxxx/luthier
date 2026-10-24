# Pedalboard completion acceptance ledger

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
| R1 functionality | All WP1-WP23 scope implemented, no registered placeholders | pending |
| R2 legacy/full tests | Prior suites plus reference suites green, both editions, 3 OS | baseline failed |
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
