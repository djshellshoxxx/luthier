# Reference pedalboard editing increment (2026-10-22)

These rows cover the message-thread model only; they do not certify the audio
engine, host integration, pedal models, UI, or release readiness. Current work
continues on PR #79 (`claude/affectionate-planck-adfno4`).

| ID | Requirement | Source | Implementation | Verification | Status |
| --- | --- | --- | --- | --- | --- |
| REF-EDIT-DUP | Duplicate preserves controls, placement, lane and enabled state; new identity | BOARD_CORE.md §3, §10.6 | BoardModel.cpp Duplicate | RefPedalBoardCore duplicate regression | locally verified |
| REF-EDIT-TRIM | Finite input/output trims in -12..+12 dB; rejection is atomic | BOARD_CORE.md §5.4; errata E4 | BoardModel.cpp SetTrim | RefPedalBoardCore trimAndMountingValidateBeforeMutation | locally verified |
| REF-EDIT-MOUNT | Valid mount selection; reject invalid enum without mutation | UI_BOARD_STYLES.md; errata E16 | BoardModel.cpp SetMounting | RefPedalBoardCore trimAndMountingValidateBeforeMutation | locally verified |
| REF-EDIT-UNDO | 100-step undo/redo, nested grouped transactions, empty/rejected transaction preservation | BOARD_CORE.md §10.6; BoardModel.h | BoardModel.cpp history | RefPedalBoardCore transaction and history tests | locally verified |
| REF-EDIT-GESTURE | Consecutive writes coalesce by gesture, command kind, instance and parameter | BoardCommand.h; BOARD_CORE.md §10.6 | BoardModel.cpp Gesture | RefPedalBoardCore gesture tests | locally verified |
| REF-EDIT-SLOT | Stable storage slots across move/duplicate/remove/undo; validate and fill restored slots | BOARD_CORE.md §3, §10.1 | BoardModel.cpp firstFreeSlot, replaceAll | RefPedalBoardCore automation-slot tests | locally verified |
| REF-EDIT-RT | Compile/publish immutable audio topology after accepted edits | BOARD_CORE.md §4 | BoardEngine/TopologyCompiler skeletons | none | pending |

Full native platform CI is required before merge/release. Local verification
uses the repository's real RefPedalBoardCoreTests and BoardModel with JUCE
8.0.10 core, linked in a focused headless runner. This does not run the complete
LuthierTests suite or plugin validators.

Release blockers observed in this increment:

- `BoardEngine` and `TopologyCompiler` are empty skeletons; facade still runs
  the three-model prototype. Editing the new model does not yet publish audio.
- `BoardState` and `BoardSnapshots` are empty skeletons; host persistence and
  automation integration require implementation and round-trip tests.
- Generic/model registration packages and most model DSP files are skeletons;
  their existence is not evidence that the pedal library is implemented.
- `BoardRoot`, `BoardWindow` content, canvas, materials and MIDI panels require
  implementation; most face files are skeletons.
- Full unit tests, sanitizer/race tests, 30-minute soak, CPU measurements,
  host/UI compatibility, plugin validation and native packaging remain gates.
- Do not merge the whole feature or publish a pedalboard release based only
  on the focused board-model test results in this section.
