# Shared test harness

**IDs:** shared by PB-00 through PB-12 and the PB-M/PB-E/PB-P/PB-U suites; no ID of its own. **ED:** 8.0 (the roadmap "Tests" bucket). **Status:** missing.

**Summary.** The offline renderer, allocation counter, and comparison helpers used by all PB tests.

## User-facing behaviour
None.

## Engine and data model
- Offline renderer: runs a `BoardProgram` (or a v1 rack) over fixed input at 44.1, 48 and 96 kHz, in the test target only.
- Allocation counter: global `operator new` hook, enabled per test, to assert zero allocations in `BoardProgram::process` (PB-05).
- Comparison helpers: bit-exact compare, dB-RMS difference, peak/width measurement (PB-08), 5 kHz level (PB-09), latency reading.
- Factory preset loader for PB-M01 (every preset, both paths).
- Board fixture builder: constructs `BoardModel` values for PB-13 notices and PB-E01 gating.
- Test-only; no code in the plugin binary.

## Parameters and data
None.

## State and migration
None.

## Edition
Test variants exist for Free and Pro (PB-E01).

## Performance budget
Not applicable (test target only).

## Tests
This item is the harness. Its own check: a known identity (bypassed pedal of type None) renders bit-identical to dry input, so the harness itself is validated.

## Effort and dependencies
ED 8.0. Needed by 16 first (PB-04/05/06), then all others. Suites live in `Source/Tests/` and are wired per `docs/HANDOFF.md`; the literal count in `IntegrationTests.cpp` is updated in file 03.
