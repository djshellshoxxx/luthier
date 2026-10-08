# External agent collaboration brief (ChatGPT / Codex)

The Luthier project is built by a Claude coordinator + a fleet of Claude
helper sessions. An external agent (ChatGPT / Codex, via its GitHub
connection) can collaborate. The repo is the ONLY shared channel — no
model-to-model chat. Read files here instead of re-explaining; keep messages
and PR text short.

## Hard rules (same as Claude helpers)
1. READ FIRST: spec/CLAUDE_CODE_BRIEF.md, spec/INDEX.md, spec/gui-integration.md,
   spec/ui-wiring.md, spec/DECISIONS.md, docs/COORDINATOR_PLAN.md (the plan).
2. BRANCHES: external agent uses the prefix `codex/luthier-<workstream>`.
   Never push to `claude/*` branches, never to the integration branch
   `claude/luthier-cloud-session-5lzlix`, never to master/main.
3. ONE MERGER ONLY: the Claude coordinator merges everything into the
   integration branch after a Linux build + test. External agent hands off by
   pushing its `codex/*` branch and opening a DRAFT PR to the integration
   branch (or leaving the branch for the coordinator to merge). Do NOT merge
   into integration yourself.
4. NO OVERLAP: work only your assigned workstream's files. New code in NEW
   files where possible. Shared hub files (LuthierEngine.*, PluginProcessor.*,
   Parameters.*, panels) get small, marked edits only.
5. PARAMETERS: append-only, inside `// ==== BEGIN <WS> params ====` /
   `// ==== END <WS> params ====` markers at the end of the layout; mirror the
   IDs and bridge; update the literal sum in Source/Tests/IntegrationTests.cpp
   by appending ` + <n>` with a comment. Never reorder/remove existing params.
6. LINE ENDINGS: run scripts/fixeol.sh before each commit (repo mixes CRLF/LF).
7. TESTS: build + run relevant suites before handing off; do not regress.
8. TERSE output; code/specs stay clear and complete (docs/helpers/TERSE_MODE.md).
9. PAUSED: no Windows/macOS build/test/packaging work. Linux Standalone +
   VST3 + CLAP only. Cross-platform is the final step.
10. Coverage: write docs/coverage/<WORKSTREAM>.md (table + decisions), like the
    Claude helpers. Do NOT edit spec/TODO.md, DECISIONS.md, PROGRESS.md.

## Ownership split (to avoid double work and save Claude credits)
- EXTERNAL AGENT (ChatGPT) — well-specified, plumbing-heavy lanes:
  feature implementations once their spec lands (ui-scaling, tuner+tuning-ref,
  midi-learn, randomize+ab, tab-export, amp-cab-ir), and the LIGHT new
  instruments (tenor guitar, acoustic bass, extended-range bass).
- CLAUDE — hard DSP (Chapman Stick, guitarrón, chitarra sarda, Zon,
  tone-match), all merges + Linux test gate, the accuracy/effects audits,
  beta testing, and the final editions/licensing/hardening.

## Handoff protocol
- Status lives in docs/COORDINATOR_PLAN.md + per-workstream coverage files.
  Both sides read these; do not relay context through a human.
- When a `codex/*` branch is green and ready, the coordinator picks it up on
  its next check-in (every 4h), tests on Linux, merges. Keep PR/commit text
  short and factual.
