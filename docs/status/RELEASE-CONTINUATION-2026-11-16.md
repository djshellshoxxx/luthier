# Windows/macOS release continuation — 2026-11-16

Resumed at the owner's request from `integration/luthier-completion`,
`dbd360890f750c66c2a53949ea9a97dda9d7d675` (latest fetched integration head).
The November 11 pause is historical; the release gates in PROJECT_STATUS.md
remain unresolved unless explicitly superseded below.

## Changes in this continuation

- Host preparation updates the session recorder's sample rate. Same-rate
  restarts retain audio/MIDI; rate changes discard the old take and resize an
  allocated ring to the previous duration (subject to the existing memory cap).
  A recorder never allocated by the user stays unallocated.
- Three native regressions cover the processor wiring, retained/discarded takes,
  unused recorder, and actual WAV rate, length and sample contents. They replace
  the unbuilt pitch-estimation proposal in #97 with deterministic sample checks.
  The separate concurrent reset defect from #97 is not fixed by this change.
- PR verification now includes PRs targeting `integration/luthier-completion`.
  This enables the existing Windows/macOS/Linux Pro/Free test and validator matrix
  before integration, without merging unfinished work to master.
- Retired `pro-beta2-release.yml`: it no longer listens for PR closures and its
  preflight is unconditionally disabled. An indirect merge of #77 must not
  authorize an obsolete release. This takes effect when the workflow change
  reaches the relevant base branch; do not merge the older integration head.

## Verification and limitations

- Python script suite: 56 tests, 55 passed, one failed because gpg-agent could not
  start in this environment (`test_each_manifest_signature_and_relative_public_key`).
- Release asset suite separately: 13/13 passed.
- Whitespace checked with `core.whitespace=cr-at-eol`; original mixed file line
  endings preserved after running the required fixeol script.
- No local native C++ build: CMake/JUCE and system development headers are absent;
  this environment has neither sudo nor the supported package installer helper.
- New C++ regressions are uncompiled locally. Native CI is required before merge.
- No release, tag or installer is claimed. Existing DSP, rack-pedal lifetime,
  MIDI continuity, macOS crash, Windows GUI, security and real-host gates remain.

## Next release gates

1. Inspect native CI for this PR, particularly `SessionRecorderRate`.
2. Finish and validate #98's pedal lifetime fix; its parked tests are not a fix.
3. Validate #90, investigate #99, and finish recorder reset concurrency separately.
4. Resolve the chorus gate using supported model evidence, and triage the native
   GUI/crash failures without weakening assertions.
5. Only after integration passes: strictness-10/LTO builds and native installers,
   Windows standalone/VST3 and universal macOS standalone/VST3/AU validation,
   followed by real-host checks and the signing/notarization steps available.
