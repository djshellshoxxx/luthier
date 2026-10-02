# Deep Audit Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the deterministic regressions and RT-safety defects proven by the deep audit without touching trunk or Claude's in-progress integration-fix work.

**Architecture:** Work from `wip/five-merge` on an isolated Codex branch. Each defect gets its own failing regression test, minimal production fix, targeted verification, then broader suite/CI verification before merge.

**Tech Stack:** JUCE C++17, existing Luthier test framework, GitHub Actions.

**Spec:** Existing `spec/` documents plus the current integration/audit findings.

## Global Constraints

- Do not modify trunk directly.
- Parameter IDs remain append-only.
- Do not alter cabinet/perf-gaps code while Claude's integration worker owns that lane.
- Preserve audio-thread allocation-free/lock-free behavior.
- One root cause and one independently reviewable fix per commit.

## Review Focus

- Ordinary musical phrases must not trigger the hidden banjo mode.
- Message-thread looper operations must not mutate buffers still owned by an in-flight audio callback.
- Looper metadata read across threads must be synchronized.
- Fixes must not add allocations or locks to the audio callback.
- Existing positive motif tolerance cases must remain valid unless the spec is deliberately tightened.

---

### Task 1: Motif false-positive regression

**Files:**
- Modify: `Source/Tests/MotifDetectorTests.cpp`
- Modify: `Source/Model/Playing/MotifDetector.h`

**Interfaces:**
- Consumes: `MotifDetector::noteOn`, `MotifDetector::consumeTrigger`
- Produces: A matcher that still accepts the intended phrase/slips but rejects common near-matches.

- [ ] Add a failing test for `{60,62,64,67,69,71}` and transpositions.
- [ ] Run the focused MotifDetector tests and confirm the new test fails for the expected false trigger.
- [ ] Implement the smallest matcher change that rejects that family without breaking existing exact/extra-note/dropped-note cases.
- [ ] Re-run the focused tests and then the complete test suite.
- [ ] Commit the regression and fix.

### Task 2: Looper scalar cross-thread race

**Files:**
- Modify: `Source/Practice/Looper.h`
- Modify: `Source/Practice/Looper.cpp`
- Test: relevant looper/practice test file(s)

**Interfaces:**
- Consumes: existing `LoopLayer` read/write API
- Produces: synchronized publication of recorded length/content state.

- [ ] Add a test that exercises audio-thread recording while message-thread-visible content/length is queried.
- [ ] Convert only the cross-thread scalar state required for correctness to atomics or coherent snapshot publication.
- [ ] Verify no audio-thread lock/allocation is introduced.
- [ ] Run practice/looper tests and full suite.

### Task 3: Looper destructive-operation ownership

**Files:**
- Modify: `Source/Practice/Looper.h`
- Modify: `Source/Practice/Looper.cpp`
- Test: relevant looper/practice test file(s)

**Interfaces:**
- Consumes: block-boundary looper processing
- Produces: clear/restore/export operations that cannot touch storage still in use by an in-flight callback.

- [ ] Add deterministic concurrency/state-handoff regression tests around clear/restore/export.
- [ ] Introduce a block-boundary handoff/generation mechanism following the repository's existing command/snapshot patterns.
- [ ] Keep buffer allocation/destruction off the audio thread.
- [ ] Run RT-safety and looper tests, then full suite.
