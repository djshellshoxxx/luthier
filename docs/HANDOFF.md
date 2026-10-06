# Luthier — project handoff (for any agent without conversation context)

Assume you inherit NOTHING from any chat. Everything you need is in the repo.

## What it is
Physically-modelled guitar VST3 / CLAP / Standalone. JUCE 8.0.10, C++17.
Integration branch: `claude/luthier-consolidate` (base for all work from 2026-10-06; see
"Consolidation" below). `codex/luthier-beta` and `claude/luthier-cloud-session-5lzlix` are superseded.

## Build / test (Linux)
- Setup: `scripts/setup_linux.sh` (installs deps, clones JUCE into ThirdParty/JUCE,
  configures build/ with Ninja+clang). First build ~15 min.
- Build tests: `ninja -C build -j$(nproc) LuthierTests`
- Run tests: `xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests [name/tag filters]`
  (no filter = all; the Combo suite is long — filter while iterating).
- Build plugins: `ninja -C build Luthier_VST3 Luthier_Standalone` (and CLAP target).
- Line endings: repo mixes CRLF/LF — run `scripts/fixeol.sh` before every commit.

## Consolidation (2026-10-06)
Every branch with unmerged work was merged into `claude/luthier-consolidate`:
`codex/luthier-licensing-core` (editions phase 1 + Pro licensing core, on top of
`codex/luthier-beta`), `codex/luthier-beta-looper-concurrency`, `codex/luthier-continue`,
`codex/deep-audit-fixes-final2` (ASCII tab pipeline, CLI converter, banjo easter egg, perf,
binary size, TAB reader), `claude/ecstatic-hawking-prpppy` (MIDI->tune import, string roll,
Reset & Stop, error recovery, BoundedMidi), `claude/luthier-spec-sweep` (GP7/8 reader, MIDI
clock transport, SysEx in, mod-source editors, themes/i18n resources, sweep tests),
`claude/luthier-w2-crossplatform`, `codex/audit-host-clock-validation`,
`codex/luthier-audit-spec-coverage` and `master` (licence headers, CDL plugin standard).
Every other remote branch was already an ancestor of one of these.
Where two branches implemented the same fix differently (looper storage gate, RT-safety body
bank and MIDI slices, host state format, fretboard accessibility), the trunk's design was
kept and the duplicate dropped; see the merge commits for each decision.
Owner decisions recorded: 3 seats per licence (licensing Q-L2); Free guitars and amps drawn at
random (editions Q-3, `Source/Edition.h`). Deferred by the owner: support URL/email, licence server.

## Architecture entry points
spec/INDEX.md (map of all specs) · spec/engine.md (DSP rules: double precision,
no audio-thread allocation, NaN guards, reset(), sample-rate independence) ·
spec/gui-integration.md (single source of truth for UI location) ·
spec/ui-wiring.md (attachment pattern) · spec/part-acoustics.md (instrument/part
model) · spec/DECISIONS.md.

## Non-negotiable code rules
- Parameters are APPEND-ONLY inside `// ==== BEGIN <WS> params ====` /
  `// ==== END <WS> params ====` markers at the end of the layout; mirror IDs +
  bridge; update the literal sum in Source/Tests/IntegrationTests.cpp by
  appending ` + <n>  // <WS>`. Never reorder/remove existing params (host
  automation is indexed by position).
- New code in NEW files where possible; small marked edits to hub files
  (LuthierEngine.*, PluginProcessor.*, Parameters.*, panels).
- Every automatable param needs a visible control (Combo test
  everyAutomatableParameterHasAVisibleControl).
- Do not regress tests; build + run relevant suites before pushing.

## Current state (2026-09-26)
- Integration is green on Linux; FIX-CROSS feedback fix merged (B-18/B-19 fixed).
- Expansion in progress: product doc, feature specs (tiers 1-4 + tab export),
  new-instrument research, accuracy audit, effects audit. See COORDINATOR_PLAN.md.
- Windows + macOS builds are PAUSED until the project is complete (Linux only).
- GitHub Actions CI is currently blocked by the account's Actions spending limit
  (jobs fail in ~5s with no logs); owner must raise it. Not a code fault.

## Known open items
- Beta report docs/audit/BETA_TEST_REPORT.md (B-xx). Feedback (B-18/B-19) fixed.
- Requirements recorded in COORDINATOR_PLAN.md: single-coil hum as an Options
  switch; finger-squeak audibility/discoverability; expression ease-of-use;
  new instruments (Chapman Stick, guitarrón, chitarra sarda, Zon, tenor,
  acoustic bass, extended-range bass).

## Priorities (order)
1. Finish helper workstreams + gap fills. 2. Full beta test + accuracy audit,
no open findings. 3. CLI easter egg (LAST feature). 4. Tag v1.0-full.
5. Fork FREE + PRO editions (Pro: strongest anti-RE + novel DSP-fingerprint
gating). 6. Cross-platform (Win/Mac) LAST.

## Collaboration
Claude coordinator (Anthropic cloud routine) owns architecture, task selection,
merges, and the Linux test gate. External agents (Codex) implement on
`codex/luthier-<workstream>` branches and open DRAFT PRs; the coordinator is the
ONLY merger. Full rules: docs/helpers/EXTERNAL_AGENT_BRIEF.md.
