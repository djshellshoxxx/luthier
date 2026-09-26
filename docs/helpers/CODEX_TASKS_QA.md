# Codex task board — QA / REVIEW fleet (second coordinator)

Collision-free by design: these lanes ONLY read existing code and write (a) report
files under docs/review/ and (b) NEW test files. They must NOT edit engine, UI,
parameter, or spec files, and must NOT "fix" anything — findings are reported for
Claude/Codex-#1 to fix later (serialized). This lets it run alongside the feature
fleet safely.

Repo: https://github.com/djshellshoxxx/luthier
Base branch: `claude/luthier-cloud-session-5lzlix`
Rules/build/test: docs/HANDOFF.md + docs/helpers/EXTERNAL_AGENT_BRIEF.md. Branch
prefix `codex/luthier-qa-*`. Push branches, open DRAFT PRs, NEVER merge, never
touch claude/* branches. No Windows/macOS work. scripts/fixeol.sh before commits.
Verify NEW tests build+run on Linux before the PR.

HARD LIMIT: do NOT modify any file under Source/ EXCEPT to add NEW test files
(Source/Tests/CodexQA_*.cpp). Do NOT edit spec/. Do NOT add parameters. Do NOT
change engine/UI/hub files. If a fix is needed, WRITE IT UP, don't apply it.

## Task Q1 — branch codex/luthier-qa-rtsafety — real-time-safety & code review
Read the audio-thread code (Source/DSP/**, LuthierEngine.*, PluginProcessor
processBlock, and anything they call). Produce docs/review/CODEX_RTSAFETY.md:
every place that could allocate, lock, do file IO, throw, or otherwise violate
real-time safety on the audio thread; plus NaN/denormal risks, uninitialised
state across reset(), and sample-rate assumptions. Cite file:line, severity,
and a proposed fix (described, NOT applied). Where feasible, add NEW tests
(Source/Tests/CodexQA_RtSafety.cpp) that assert no audio-thread allocation using
the repo's existing allocation-counter pattern.

## Task Q2 — branch codex/luthier-qa-coverage — test-coverage gap analysis
Map which engine/DSP/technique features have unit tests and which do not (compare
Source/ against Source/Tests/). Produce docs/review/CODEX_COVERAGE.md ranking the
biggest untested/under-tested areas. Then ADD NEW tests (Source/Tests/CodexQA_*.cpp)
for the top gaps you can cover WITHOUT changing production code — deterministic,
Linux-green. Do not weaken thresholds; if a test reveals a real bug, report it in
the doc, mark the test as an expected-fail note, and move on (do not fix code).

## Task Q3 — branch codex/luthier-qa-robustness — fuzz + preset/state QA
Add a NEW fuzz/robustness harness (Source/Tests/CodexQA_Robustness.cpp): drive the
engine with randomised parameter combinations, random MIDI, extreme buffer sizes
and sample rates, and every FACTORY PRESET; assert no crash, no NaN/inf, no
runaway level, and that state save->load round-trips. Produce
docs/review/CODEX_ROBUSTNESS.md listing any preset or combo that misbehaves
(file:line / preset name, symptom, seed to reproduce). Do not fix — report.

When done, per task: branch, report file, new tests added, Linux verification
output, and a prioritised findings list. The Claude coordinator reviews the
reports and schedules fixes (serialized) so they don't collide with the feature
fleet.
