# Pedalboard continuation handoff

Latest checkpoint: [fresh2026-10-25 build6 and ownership reconciliation](handoff/PEDALBOARD-2026-10-25-BUILD6.md).
Tested production source280e182a freshly builds all five Linux Pro targets;
actual CLAP18/0/3unsupported skips and VST3 strictness5 pass. Combined unit/stress
qualification and cross-platform/full acceptance remain open. Older entries
below preserve their original source-specific evidence and are not current
absence reports. The final Claude preservation ref1f646a65 is verified; sole
integration ownership is reconciled, and disjoint testing continues.

Updated 2026-10-25 10:40 UTC. Work continues in the same environment; no new
release or tag has been created. Only djshellshoxxx/luthier is in scope.

Seven actual slots are active: coordinator plus the existing engine, dsp,
timefx, state, host_midi and ui workers, each freshly acknowledged. Worktrees
are /workspace/luthier-workers/{engine,dsp,timefx,state,host-midi,ui-next}; the
coordinator's isolated rhythm worktree is /workspace/luthier-workers/rhythm.
No additional environment or duplicate continuation timer was created.

## 2026-10-24 16:00 UTC critical correction checkpoint

Latest prior frozen source3cb71450c09f3e14be7f4d84d72d0ceeb612feb3 built all native
Linux Pro targets (Standalone/VST3/CLAP/tests/renderer). Its selected UI suite
segfaulted after a valid generic Add was wrongly rejected: Generic A estimates
were milli-percent while E65 interpreted every raw unit as one percent. New
661a6ed7/0f720246 plus scoped Generic A metadata05b4f758 normalize the explicit
scales, preserve the30% default cap and retain original DSP behavior. Named DSP
candidates remain isolated/unregistered. Additive0cab2698 regression covers every
actually registered model, mixed scales and overflow. UI guard7c99c111 preserves
failed prerequisites and their error instead of dereferencing an empty order.

Actual coherent3cb CLAP:21run/15passed/3failed/3skipped. All three remaining
state failures concern legacy Active Electronics, not pedal slot values. Exact
upstream wrapper discarded same-value incoming host events before the processor
could observe intent. Pinned reproducible patchf4777cb1 and configure hook05b4f758
retain notifications with changed-only setValue and outgoing feedback suppression.
Five isolated patch tests pass; an isolated actual wrapper rebuild gives18passed,
0failed,3skipped. This is not yet a coherent new production product qualification.
Independent arithmetic review found no CPU fix blocker. Scanner findings were
only the two public pinned wrapper SHA256 values, not credentials.

CI40106049619 at3cb compiled all six Windows/Linux/macOS universal edition cells.
Both focused Linux preflights40106049578 passed. Broad unit jobs fail with the
admission/UI crash; Linux Pro and Windows Pro validators report the legacy state
failure. Free validators pass. Products/logs were retained as CI diagnostic
artifacts; no release qualification or publication is claimed. macOS Pro validator
was still running at this checkpoint. No tag/release created; no master merge.

Next: freeze corrected exact HEAD, rebuild all native targets coherently, run
focused+full tests, actual CLAP and VST3 validation and standalone UI; repeat30s
and longer soak on the corrected source. Push this tested-status checkpoint now,
then use native CI on the corrected commit. Continue E69 memory and other original
acceptance gates independently. Seven slots have ongoing assignments (coordinator
plus engine,dsp,timefx,state,host_midi,ui); no other project's workers were taken.

## 2026-10-25 first focused execution checkpoint

Latest tested source before this documentation commit is
64292744f770fd6789afc4edca3a63cc0ed1363f (fixture commits b69759f3 and64292744).
Only three direct-host MIDI test fixtures changed; production remains identical
to b5bec374. Query git rev-parse HEAD for the documentation checkpoint SHA.
PR80 was freshly verified open/draft at b5bec374; master remains0f62c3d6.
A new Claude branch peaceful-carson-vzl4e4 at6d9048b5 was found and inspected.
Its b3ec7a41 capacity-only file slices were reused with authorship and provenance;
no whole divergent history or blanket ref_* visibility exclusion was merged.

The original JM39 failed unchanged with63 unmatched captured note-ons. Diagnostic
A/B: unregistered125generated/0returned/125drops; stable registered125/125/0,
with the same42checks passing. Independent JM43 original Jam4/tune0/drops28
became exact vectors[33,29,36,31]/[33,29,36,31],zero drops,unchanged5checks pass.
After scoped fixture reuse, this exact command passed32tests/1392checks/exit0:

    xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests JamPlugin MidiOutPanel ModelGapsUi

This turn rebuilt only the changed test TUs and linked LuthierTests. The saved
production build command was launched2026-10-24 15:59:32UTC and completed
16:25:23UTC,exit0:

    cmake --build build --target LuthierTests LuthierRender Luthier_VST3 Luthier_CLAP Luthier_Standalone -j3

Fresh2026-10-25 validation of those unchanged b5 products: CLAP21run/18pass/
0fail/3unsupported-preset-discovery skips,exit0; VST3 pluginval strictness5,
seed0x51a7,all30 rate/buffer combinations through192kHz and2048samples,GUI and
automation included,SUCCESS/exit0. These are Linux results,not release acceptance.
Native link inputs were independently copied before test replacement:
/workspace/luthier-evidence/native-b5-link-snapshot-20261025/manifest.json,
813inputs/107271888bytes plus matching resources/executable. No product rebuild
was repeated merely to resume. Details: coverage/PEDALBOARD-DIRECT-HOST-MIDI-20261025.md.

The private Library request resolved version30 but the unchanged supported
materialization helper failed with 'download failed'; no archive bytes/hash or
private packets were recovered/read. No URL guessing or repeat transfer was
attempted. Primary-source repository/executable diagnostics continue. Exact Mac
post-prepare JSON mismatch is not reproduced on Linux; original Mac saved pair
and a crash backtrace remain missing. Failure-only diagnostics are being added
without equality filters. Original full CI40109481251 remains failed; quick-check
passed,CodeQL cancelled. Nothing has been tagged or published.

All seven slots were verified running at this checkpoint: coordinator integration;
DSP cabinet/normalization; engine retirement safety; host format/RT QA; state
exact-JSON diagnostics; timefx GUI attribution/CQ03 reconciliation; UI native
stress/reentrant gestures. Existing workers/worktrees only; no other repositories,
new environments,usage bypasses or implementation timers were used.

Next batch is review-gated: exact GUI p08 event attribution; original-vs-required
CQ03 CPU-cap semantics; unchanged legacy bus indices with three appended buses;
real UI drag gesture reentrancy; retirement allocation-failure safety; cabinet
stable asset keys; exact codec/state slices and full prepared-memory closure.
Preserve all red logs. Original PStyle output35dB gate still fails (~34.7dB),
whole-processor no-lock acceptance is open,Mac diagnostics/platform full suites
and hardware/DAW/manual acceptance are incomplete. Windows standalone,macOS both
architectures,VST3,CLAP thenLinux remain the requested publication order once
applicable gates qualify; signing/notarization must be reported truthfully.

## Source and preservation

Main work branch: codex/luthier-pedalboard-completion. Query git rev-parse HEAD
for the exact handoff commit. The integrated source just before this handoff is
e7ca9cc9 (including PR77 merge a63c6100 and portable codec91678b4c).
This is followed by a documentation-only freeze checkpoint. PR80 remains draft;
its remote head must be rechecked after the next qualified CI push.
Read-only current GitHub intake confirms no newer Claude pedalboard branch than
PR79 at0b319c0b; that ancestry is already preserved, not a newer merge target.
The old-master release run40096845972 was cancelled and is not a pedalboard build.

PR77's reviewed 24-commit branch910cc2a9 is locally merged, including Free
normalization isolation, deterministic host-write serial transactions, room
reflection geometry, classic practice-looper reprepare and package validation.
CombinationTests.cpp and PerfBudgetTests.cpp retain our existing thresholds;
the merge did not take PR77's relaxed Resonator/startup limits. PR77 itself was
not merged into master; its event77-only automatic old beta release remains inert.

Qualified scoped integrations include engine e559/527/1d69/4a0/c5c/b484/4e68/3f88,
host77194cdf+1f517727, state59305029/8efb49bc/0d1db1a7/99695aa6,
timefx b0ee30d8/b83293af/f6c2e5e2/b3c3559f, UI79f/5db/1c48/2bee/9a5e,
and rhythm c810/66ad/89959. Local dependency-only imports are intentionally
skipped; no whole divergent worker history was merged. DSP candidates stay
isolated/unregistered where original tonal, alias or CPU gates fail.

## Exact fresh evidence

Frozen02e4e1fc native Linux Pro build completed. All product targets linked;
selected tests:290 tests/6,839,271 checks, one failed initialization assertion,
59.7seconds. The router fixture inherited internal120 then expected immediate
host133 despite the intended50ms source glide. Re-prepare before that independent
initialization subtest preserves all original assertion values and glide behavior.
Combined new source needs coherent rerun; do not call the old executable current.

Fresh exact02 VST3 pluginval strictness5: SUCCESS/exit0 with sample rates
44100/48000/88200/96000/192000, blocks1/16/37/64/481/2048, seed0x51a7.
CLAP:21 run,15 passed,3 failed,3 skipped. Failures reset empty/unused slot and
structurally rejected global APVTS values during save/restore. Host worker owns
an actual-processor red/green regression and correction; no exclusions approved.
A coherent02 Source snapshot for native test overlays is retained at
/workspace/luthier-evidence/native-snapshot-02e4e1fc. Do not combine its object
files with newer headers whose member sizes changed.

Earlier206 selected tests/5,649,492 checks passed at05bbdb8e. The full1800second
wall-clock concurrent core soak at347f0ce5 passed180000edits/3049960869samples,
C++new0/nonfinite0; sampledRSS26628KiB, kernelHWM29212KiB. This predates new source
and does not certify Cmalloc or host wrapper locks. Malformed-state1000-load
focused run passed7004checks. Original Phase90 oracle failures remain visible.

Current source trademark scan:0 hits. Release asset preparation:13 Python tests
passed. Exact JUCE standalone MIDI capacity patch:4 tests passed using
python3 scripts/tests/test_patch_juce_midi_capacity.py ThirdParty/JUCE.
An initial unittest-discover invocation was unsuitable for that positional-path
harness and failed before fixture setup; do not treat that as patch behavior.

## Latest qualified slices and current build boundary

Actual processor regression164aaefa is integrated as1bb22354:16host tests,
2511checks pass against the coherent02 object/header overlay, including all373
host intents, invalid structural requests, unused lanes and real feedback DSP
kill-replay safety. E65 feed816e1320 forwards actual callback CPU measurements;
its integrated runtime test is pending the new executable.

E68 interface, engine and actual Delay/Reverb/DD3T overrides are integrated.
The original hot-trim polarity failure was reproduced (+5 becoming-10.5475).
Explicit one-pass wet residuals remove analytic dry subtraction. Engine native
83tests174808checks and frozen ASan/UBSan/leak83tests172155checks pass, including
actual registered hot-dry0/+12dB trims, Cut/Spillover and zero allocations. Shared
JUCE objects are not instrumented in that sanitizer run. Model-owner sanitizer
checks6134208 and separate1966080-frame no-allocation probe also pass.

E66 Hall correction is integrated with unchanged canonical5-rate white±3dB,
mixed-sine±1.5dB, raw NED200ms, RT60 and freeze reference gates. Independent
lossless-output/phase review is retained. Expanded-seed mixed output reaches
+1.643dB: full normalization acceptance remains OPEN. Original geometry/sign-grid
failures remain tracked in docs/coverage/probes. Named model registration is
still gated. Delay coefficient-cache e7ca9cc9 is bit-identical over3967500 frames
and passes its mode CPU fixtures; source behavior was not approximated.

Snapshot helper89c5a7ed now uses validated undoable commands, preserving cloned
bank metadata, captured live extras and prior undo history. This is separate
from MIDI's128 control-only scenes; PC must never accidentally recall full
snapshots, reorder pedals or restore loop audio. That bridge and40ms scene ramp
remain pending. E67 natural Discrete mapping07d9c296 is integrated with UI choice
fix7d4f187f; no earlier registered model used Discrete.

UI BPM metadata, actual GuiReach board/settings/inspector flow, meaningful
position selectors and keyboard gesture recording are integrated. Live-drag final-only recording and the host position-echo mask are integrated
from a8268054 and8f30e9d9. Focused UI40tests673922checks plus the strengthened
37-check drag lifecycle test pass; actual host18tests2533checks pass. Integrated
whole-editor reachability and the full bridge are still pending. No hidden/fake
unused-lane widgets.

The actual old02 native decay gate was freshly reproduced:1of25 fails at34.7dB
versus35. Root's separate experimental zero-coupling/player-gesture overlays each
showed17failures and do not yet agree with an unchanged relink control; they are
UNQUALIFIED diagnostics and no production fixture change was adopted. Engine
owns frozen-object/header consistency diagnosis, then current-candidate retest.

Source is being frozen for one coherent native build plus the next six-cell CI.
No later C++ changes may enter main until the build/test checkpoint completes;
workers continue isolated and queue commits. Current local build identity stays
pedalboard-local-qa; final CI/release provenance must use the exact source SHA.
README now truthfully labels Luthier as a software artwork while retaining
functional descriptions and pedalboard development/acceptance status.

## Critical next steps

1. Freeze the final bounded drag/echo slice if ready, record exact source SHA,
   and run the coherent combined native build; do not reuse the old02 executable.
2. Compile once from a frozen combined source, run reference/host/normalization/
   practice/mic/unit/UI regressions, real standalone smoke and VST3/CLAP validators.
   Keep original full suite and legacy decay requirement unchanged.
3. Secret-check and push source plus handoff; verify exact remote head. Trigger
   genuine Windows/macOS/Linux CI from the qualified candidate, both editions.
   Native platform products and validators must pass before release qualification.
4. Continue named model registration gates, looper wrappers/memories, scenes,
   CPU governance and all outstanding original specs in parallel. A preview must
   state its limited accepted scope; never label the feature complete prematurely.
5. Prepare a new immutable version, source SHA, checksums, install/known-issue/
   license notes, and unsigned/unnotarized labels. Preserve existing releases.

The whole-rig P-Style Bass note42 decay was34.7dB versus35 before the PR77 merge.
No threshold was relaxed. Its fixture still includes minimum sympathetic
coupling0.05 (the engine has a test-only zero API); investigate after current
normalization fixes, retaining original evidence. Hardware/listening/DAW operator
acceptance remains distinct and unavailable here.

## Portable milestones

The full base bundle through e733841e is in Library:
SHA25639b8d85e0a5552e03df65abfd81ee027ac21d28c4b8862650f7822db0e3cb93a.
The latest prior incremental archive luthier-checkpoint-20261024-0130.tar.gz
SHA256e009392fbe893bd04a40c4ad0506f106d4ad4f42c74702777ff896b4d638987a
is saved in Library as libfile_14c7879cd4508191b8b60be124df8c56.
Verify checksums and git bundle verify before importing. Exclude keys,
credentials, dependency/build caches and user assets from all archives.

Known full-feature memory gate:64MiB prepared-pool cap is not yet certified.
Sixteen stereo2s double delays at192k need98,304,000 raw bytes before overhead;
current Wide reservation is2.4s. Do not lower precision/range or silently raise
the cap. A resource admission limit/accounting policy is still required.

Freeze boundary update: source includes host8f30e9d9 and UIa8268054; query HEAD
of this handoff for the exact combined build commit. All later C++ changes queue
in worktrees until the coherent native run completes. Original02 overlay legacy
diagnostics had two environmental inconsistencies: a contaminated shared header
snapshot and missing Resources beside relocated executables. Engine is repairing
both unchanged controls; no code/fixture changes were adopted from those runs.
