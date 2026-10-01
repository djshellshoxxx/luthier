# Codex task board — PHASE 2 (run only AFTER phase-1 lanes are done/merged)

Repo: https://github.com/djshellshoxxx/luthier
Base branch for ALL tasks: `claude/luthier-cloud-session-5lzlix`
Rules/build/test: see docs/HANDOFF.md and docs/helpers/EXTERNAL_AGENT_BRIEF.md.
Same discipline as phase 1: push codex/* branches, open DRAFT PRs, NEVER merge,
never touch claude/* branches or spec/ files, no Windows/macOS work, run
scripts/fixeol.sh before commits, verify on Linux before each PR. The Claude
coordinator is the only merger and serializes parameter-file merges.

These lanes touch the parameter/automation core, so COORDINATE tightly: use the
distinct markers below, append-only, and serialize edits to Parameters.cpp/.h,
ParameterBridge, and the sum in Source/Tests/IntegrationTests.cpp.

## Task A — branch codex/luthier-midi-learn — marker FEAT2-MIDILEARN
MIDI Learn / CC (and MPE where relevant) mapping on any automatable parameter:
right-click a control -> Learn -> next incoming CC binds; mapping shown, clearable,
and saved/restored with plugin state. NEW files for the mapping manager; small
marked hooks in the parameter/attachment layer.
ACCEPT: test — a learned CC drives its target parameter; mapping survives
state save/load. Coverage: docs/coverage/FEAT2-MIDILEARN.md.

## Task B — branch codex/luthier-randomize-ab — marker FEAT2-RND
Header controls: (1) constrained musical Randomize (nudges parameters within
musically sensible ranges, never into silence/instability); (2) A/B compare
(hold two full states, swap, copy A->B). NEW files; small header wiring.
ACCEPT: test — Randomize keeps every parameter within its valid range and leaves
audio non-silent/stable; A/B swap restores the exact stored state.
Coverage: docs/coverage/FEAT2-RND.md.

## Task C — branch codex/luthier-doubler — marker FEAT2-DOUBLER
FIRST confirm build state: search Source/ and spec.md for an existing "doubler".
If it already exists and works, STOP and note that in the PR (no duplicate). If
not: automatic double-tracking — a second panned copy with small timing + pitch
variation for a wide guitar sound; width, delay-spread, pitch-spread, and mix
controls; save/restore. NEW files (Source/DSP/Doubler/*).
ACCEPT: test — doubler produces a decorrelated second voice (measurable stereo
width increase) and is bit-neutral when mix=0. Coverage: docs/coverage/FEAT2-DOUBLER.md.

NOTE: new-instrument lanes (tenor, acoustic bass, extended-range bass) will be
added to a later board once their specs land from INSTRUMENT-RESEARCH.
