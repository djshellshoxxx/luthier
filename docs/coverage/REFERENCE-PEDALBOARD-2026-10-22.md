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
| REF-EDIT-SETTINGS | Scalar board controls validate exact booleans/enums/ranges and retain undo/redo | BoardCommand.h SetBoardParam; BOARD_CORE.md §6 | BoardModel.cpp setBoardScalar | scalarBoardControlsRoundTripAndUndo; malformedScalarControlsDoNotMutateOrConsumeHistory | locally verified |
| REF-RESTORE-VALIDATION | Invalid restored settings/controls leave current board and history untouched | BOARD_CORE.md §10.5 | BoardModel.cpp validBoardSettings/validPedalControls, replaceAll | invalidRestoredSettingsLeaveBoardAndHistoryIntact; invalidRestoredPedalControlsDoNotReplaceExistingBoard | locally verified |
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


## Additional verification

The expanded focused suite passes 19 tests / 416 checks. New scalar-command
and restore-validation tests were run against the preceding implementation
and failed before the corresponding changes. Full native CI for the previous
editing increment reached unit tests on Windows/Linux and macOS Free; macOS
Pro was still compiling when checked. These intermediate job steps are not
passing-suite or release evidence.

The earlier 15-test model/runner build passed AddressSanitizer and UBSan with
`ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer could not inspect `/proc` in this
execution environment, so no leak-free claim is made. JUCE core was linked
from an uninstrumented object; this focused check is not the full sanitizer,
race, audio-thread, UI, or soak release gate.

The expanded 19-test / 416-check run also passed with BoardModel compiled
under AddressSanitizer and UBSan (leak detection disabled for the environment
limitation above). In that expanded run, the test runner and JUCE core were
uninstrumented. This verifies the changed model paths only.
