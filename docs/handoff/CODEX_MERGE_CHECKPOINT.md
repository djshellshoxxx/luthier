# Codex mega-merge checkpoint (2026-09-29) — REPLAY RECIPE for Claude

Codex built `codex/luthier-beta` LOCALLY but COULD NOT PUSH (no GitHub creds in
its shell). The branch is NOT on origin. Use this as the exact recipe to REPLAY
the merge on origin after the Claude weekly reset (all source branches ARE on
origin). Do not wait for codex/luthier-beta to appear.

## Recipe (verified to compile by Codex)
- Base: codex/luthier-beta from origin/claude/luthier-cloud-session-5lzlix (@ e9492ef).
- Merge common integrated base 3d8266a ONCE (resolve ci-cadence.yml in favour of
  integration's cadence/Windows policy). Then merge as deltas:
  origin/claude/luthier-w2-crossplatform (its one unique CMake commit),
  origin/codex/{guard-last-tune-section, gui-slap-rebound-gap, host-compatibility-doc,
  missing-next-row, one-audit-regression, verify-factory-tunings},
  and origin/claude/luthier-w2-recount (unique audit recount — NOT a dup).
- Compiled green at both gates (403/403, then 53/53). Param roster test 1/1
  (2253 checks). Literal count so far: 450+3+15+29+29+2+34+1 = 563.
- origin/claude/luthier-fable-feedback is ALREADY an ancestor via the base;
  B-18/B-19 named tests [pass] (2 tests, 5 checks).
- NOTE: LuthierTests exe returned exit 1 despite "ALL PASSED" — investigate the
  runner's exit status before claiming green by process status.

## STOPPED at: origin/claude/luthier-integrate-2 (15 conflicts, aborted clean)
Conflicts to resolve (keep append-only param blocks; keep integration's SPEC-SWEEP
additions; after merge add `+4 // FEAT-ASSIST` and `+25 // FEAT-MIC` to the count):
PlayingEvents.h, Parameters.cpp/.h, PluginEditor.cpp, PluginProcessor.cpp,
PresetManager.cpp, AccessibilityTests.cpp, IntegrationTests.cpp,
JamProcessorTests.cpp, AnimationPolicy.cpp, EasyPanel.cpp, FretboardComponent.cpp,
GuitarBodyComponent.cpp, HeaderBar.cpp, OptionsPages.cpp.
Run scripts/fixeol.sh before the merge commit; rebuild before further branches.

## Remaining ahead branches after integrate-2
accuracy-audit, audit, feat-browser, feat2-notation, fix-cross, gaps-content,
gaps-gui, gaps-host, gaps-sound, qa-robustness, qa-rtsafety, techniques.
feat-assist/feat-mic/feat-riffs/feat-search are ANCESTORS of integrate-2 — merge
integrate-2 once, then recount and merge only remaining deltas. feat-browser is
NOT in that ancestry. Verify ancestry (git merge-base / --contains) before
treating any codex/* as outstanding; several were already in the base.

## Then
Run full non-Combo suite; fix real failures (no skips). Push the branch (Claude
HAS creds), open draft PR to integration, then proceed to beta critical path.
