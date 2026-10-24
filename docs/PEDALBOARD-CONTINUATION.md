# Pedalboard continuation handoff

Updated 2026-10-24 14:30 UTC. Work continues in the same environment; no new
release or tag has been created. Only djshellshoxxx/luthier is in scope.

Seven actual slots are active: coordinator plus the existing engine, dsp,
timefx, state, host_midi and ui workers, each freshly acknowledged. Worktrees
are /workspace/luthier-workers/{engine,dsp,timefx,state,host-midi,ui-next}; the
coordinator's isolated rhythm worktree is /workspace/luthier-workers/rhythm.
No additional environment or duplicate continuation timer was created.

## Source and preservation

Main work branch: codex/luthier-pedalboard-completion. Query git rev-parse HEAD
for the exact handoff commit. The integrated source just before this handoff is
4611f47f (including PR77 merge a63c6100 and portable codec91678b4c).
PR80 remains draft, remotely e5d350e5 until the next qualified CI checkpoint.
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

## Critical next steps

1. Integrate host's actual CLAP restore correction and UI's real exhaustive
   reachability/BPM metadata fixes; inspect dependency closure and tests.
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
