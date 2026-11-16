# Recorder concurrency continuation — 2026-11-16

Parent: PR #102 at `7f673fa6ba0a450e0d210a2bcbfceabd7d09a5f4`.
The owner resumed Windows/macOS release work. The November 11 paused status is
historical; this continuation does not qualify or publish a release.

## Defect and fix

The message thread reset the recorder clock and MIDI FIFO while audio capture
was writing them. Export also copied a live audio ring without excluding writes.
The pre-existing looper storage gate now also protects the session recorder:

- Reset, preparation, and export snapshots close callback admission and wait for
  already-admitted capture to finish. This waiting is on the message thread.
- Audio callbacks never acquire a mutex or wait. They skip capture while storage
  is exclusive. The processor captures audio and MIDI through one admission, so
  reset cannot split a host block's audio and MIDI clocks.
- MIDI drain/reset ordering retains the existing consumer mutex. UI metadata and
  the clock read by MIDI import use atomics.
- Export snapshots audio, MIDI, and sample rate before releasing the gate; WAV
  and MIDI writing then operate on that snapshot with capture admission reopened.
- Invalid nonfinite preparation inputs are rejected before arithmetic/conversion.

This is a single-audio-producer API. Message-thread export/control methods remain
message-thread operations. Snapshot copying can skip capture blocks while it
holds exclusive access, as resizing previously could; this is not a guarantee
of uninterrupted recording during editing or large take export. It does not
change audio rendered to the host.

## Evidence

A focused Linux target compiles the real production sources and JUCE 8.0.10,
without GUI/audio-device backends or synchronization mocks. See
`scripts/recorder_checks/README.md` for reproducible commands.

- Before the production edit, GCC 14 ThreadSanitizer reported a data race between
  `SessionRecorder::captureMidi` reading `samplesSeen` and `reset` writing it;
  exit 66. The unmodified parent recorder sources were compiled for this run.
- After the edit, five tests / 103 checks pass under ThreadSanitizer, exit 0.
  Coverage includes concurrent resize/reset and capture, stereo snapshot integrity
  while capture continues, reset MIDI timestamps read from the exported file,
  host rate transitions, and the gate's deterministic admission/exclusion test.
- The same five tests / 103 checks pass under ASan+UBSan, exit 0. Local leak
  detection was disabled because this traced environment cannot run LeakSanitizer;
  ASan memory-error detection and UBSan remained enabled.
- Release-asset Python regressions: 13 passed. Workflow YAML parses successfully.
- Quick-check includes a dedicated real-JUCE ThreadSanitizer job and the native
  `SessionRecorderConcurrency` suite in both Free and Pro smoke selections.
- Native product and platform qualification remains pending.

## Earlier native evidence and outstanding release gates

PR #101 run 43843489246: Windows Free has 1/2911 failed tests, the known
`RefPedalCe2.bbdOnOriginalThdGate`; Windows Pro has the same sole failure.
Windows plugin validators pass. macOS Free/Pro built and their unit tests were
still running at inspection. PR #102's rack lifetime fix has separate native
checks in progress. Its macOS jobs were cancelled after 15 minutes with no
runner assigned (`runner_id: 0`) and no steps; this is not a product test result.
Do not count #101 as validation of later source changes.

Remaining work includes chorus model qualification, rhythm MIDI continuity,
native macOS/crash investigation, other recorded audit findings, integrated
platform tests, and packaging. No current-source release artifact is claimed.
