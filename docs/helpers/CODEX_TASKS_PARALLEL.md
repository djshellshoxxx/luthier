# Codex task board — PARALLEL SET (for the coordinator's 3 subagents)

Run these ALONGSIDE the phase-1 lanes the coordinator is already doing
(tab-export, amp/cab-IR, tuner, ui-scaling). These three are chosen to be
low-collision: mostly NEW files, minimal edits to shared hub/parameter files.

Repo: https://github.com/djshellshoxxx/luthier
Base branch for ALL tasks: `claude/luthier-cloud-session-5lzlix`
Rules/build/test: docs/HANDOFF.md + docs/helpers/EXTERNAL_AGENT_BRIEF.md. Push
codex/* branches, open DRAFT PRs, NEVER merge, never touch claude/* branches or
spec/ files, no Windows/macOS work, run scripts/fixeol.sh before commits, verify
on Linux before each PR. The Claude coordinator is the only merger.

If a task needs to add a parameter, use its OWN marker (below), append-only, and
serialize the moment it edits Parameters.cpp/.h, ParameterBridge, and the sum in
Source/Tests/IntegrationTests.cpp with the other param-adding lanes.

## Task X — branch codex/luthier-notation-export — marker FEAT2-NOTATION
Read spec/notation-export.md and spec/midi-export.md. Implement STAFF notation
export (distinct from the ASCII/GP tab lane): render the exported performance to
standard notation output (MusicXML is ideal; else a clean staff text/quantised
format). NEW files (Source/Export/NotationExport.*). Wire into File -> Export.
Likely NO new parameters.
ACCEPT: test — a fixed PerformanceScore fixture renders to deterministic
notation output with correct pitches/durations for a known phrase.
Coverage: docs/coverage/FEAT2-NOTATION.md.

## Task Y — branch codex/luthier-meters — marker FEAT2-METERS
Add input and output level meters + a clip indicator to the plugin header/footer
(peak + short RMS, with a peak-hold and a clip LED that latches until clicked).
Read-only displays fed from the existing audio path via an atomic level snapshot
(no audio-thread allocation/locking). NEW UI files; tiny marked hook to publish
levels. Prefer NO new automatable parameters (a show/hide pref is fine).
ACCEPT: test — a known-amplitude buffer produces the expected meter reading; a
>0 dBFS sample latches the clip indicator.
Coverage: docs/coverage/FEAT2-METERS.md.

## Task Z — branch codex/luthier-doubler — marker FEAT2-DOUBLER
FIRST confirm build state: search Source/ and spec.md for an existing "doubler".
If it exists and works, STOP and note in the PR (no duplicate). Else implement
automatic double-tracking: a second panned copy with small timing + pitch
variation for a wide guitar sound; controls for width, delay-spread,
pitch-spread, mix; save/restore. NEW files (Source/DSP/Doubler/*).
ACCEPT: test — produces a decorrelated second voice (measurable stereo-width
increase) and is bit-neutral at mix=0.
Coverage: docs/coverage/FEAT2-DOUBLER.md.

DEFERRED (do NOT take concurrently — parameter/automation-core heavy, do later
serialized): midi-learn, randomize+A/B (see docs/helpers/CODEX_TASKS_PHASE2.md).
