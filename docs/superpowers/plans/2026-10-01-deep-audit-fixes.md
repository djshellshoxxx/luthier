# Deep Audit Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the deterministic regressions and RT-safety defects proven by the deep audit without touching trunk or Claude's in-progress integration-fix work, and improve TAB-reader tuning fidelity using real-world tab conventions.

**Architecture:** Work from `wip/five-merge` on an isolated Codex branch. Each defect gets its own failing regression test, minimal production fix, targeted verification, then broader suite/CI verification before merge. TAB-reader tuning remains part of the existing notation import/playback path: the imported score is the single source of truth for string tuning and capo.

**Tech Stack:** JUCE C++17, existing Luthier test framework, GitHub Actions.

**Spec:** Existing `spec/` documents plus the current integration/audit findings and real-world ASCII-tab conventions surveyed during this pass.

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
- TAB tuning/capo instructions may appear before or after the tablature, in prose, or in string labels. Explicit instructions outrank inference; ambiguous prose never silently retunes.

---

### Task 1: Motif false-positive regression

**Files:**
- Modify: `Source/Tests/MotifDetectorTests.cpp`
- Modify: `Source/Model/Playing/MotifDetector.h`

**Interfaces:**
- Consumes: `MotifDetector::noteOn`, `MotifDetector::consumeTrigger`
- Produces: A matcher that still accepts the intended phrase/slips but rejects common near-matches.

- [x] Add a failing test for `{60,62,64,67,69,71}` and transpositions.
- [x] Confirm the current matcher accepts the false trigger.
- [x] Implement the smallest matcher change that rejects that family without breaking existing exact/extra-note/dropped-note cases.
- [ ] Re-run the complete project test suite when CI/local build access is available.

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

### Task 4: TAB-reader tuning and capo fidelity

**Files:**
- Modify: `Source/Notation/AsciiTabReader.cpp/.h` as needed
- Modify: `Source/UI/PracticePanel.cpp` if playback is not already consuming score tuning/capo
- Modify/add existing notation/TAB-reader tests

**Real-world forms to cover:**
- Prefix: `Tuning: Drop D`, `TUNING: D A D G D F#`, `Eb standard`, `half step down`, `whole step down`.
- Suffix/post-tab instructions and corrections.
- Explicit lists such as `Eb Ab Db Gb Bb Eb`, `DADGAD`, `E2 A2 D3 G3 B3 E4`.
- Per-string labels that imply tuning (`D|---`, etc.).
- Combined tuning/capo prose (`Drop D, capo 2nd fret`).
- `tab/frets relative to capo` versus `actual frets`/`relative to nut`.
- Multiple-guitar prose must not silently retune a single track when ambiguous.

**Interfaces:**
- Consumes: complete ASCII tab text.
- Produces: `PerformanceScore::ScoreTrack::tuning`, `numStrings`, `capoFret`, diagnostics, and playback using those values.

- [ ] Scan the entire document for explicit tuning/capo instructions, not only leading headers.
- [ ] Define precedence: explicit unambiguous instruction > consistent string-label inference > standard fallback.
- [ ] Preserve a later explicit correction only when it clearly overrides an earlier instruction; otherwise report ambiguity rather than guessing between alternatives/multiple guitars.
- [ ] Add tests for pre-tab, post-tab, named tunings, note-list tunings, half/whole-step-down phrasing, capo + tuning, relative-to-capo, actual-fret wording, and ambiguous multi-guitar text.
- [ ] Before TAB playback, apply the score track's tuning/string-count/capo through the existing engine tuning path.
- [ ] Show detected tuning/capo in status/diagnostics.
- [ ] Run focused notation/TAB tests and complete suite when runner access is available.
