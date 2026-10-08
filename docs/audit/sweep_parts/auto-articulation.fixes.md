All rows are OWNED by the auto-articulation FEAT session; no Phase-2 fixes. Pre-existing code the owner must integrate with:

- NOTE `Source/Model/Playing/MidiInterpreter.cpp:409` — Mono path calls `voicer->voiceSingleNote(midiNote, velocity, lastMonoString)` (to become `planSingle`); `MidiInterpreter.h:225 StringSlot::releaseDueAt` (fall reuse), `:235 flushChordGroup`, `:241 emitVoicedNote`.
- NOTE `Source/Model/Playing/TechniqueEngine.h:73 decide` already does its own legato/slide inference (to be gated by `setLegatoInferenceEnabled`); `:92 slideDurationFor`; `:89 getDampingStateFor`.
- NOTE `Source/Model/Playing/RubricVoicer` `setPreferredPosition`/`voiceSingleNote` (tested in `ModelTests.cpp:671`, `RubricVoicerTests.cpp:362,510,588`).
- NOTE `Source/Rhythm/StrumGesture.h:124 StrumRequest`, `:149 StrumGesture` — fill before `plan`.
- NOTE `Source/Model/Playing/PlayingEvents.h:48 NoteOnEvent` — new fields append after `bassTechnique`.
- NOTE `Source/LuthierEngine.h:439 updatePerBlockModulation`, `:482 ScheduledEvent` (add `dampingLift` kind); `Source/Parameters.h:430 ParameterBridge`.
- NOTE Parameter order: the current last parameter is `aux1_pre_circuit` (`Parameters.h:363`, the spec's `aux_1_pre_circuit` spelling is wrong); REALISM/TECHNIQUES branches also append parameters, so append `aa_*` after whatever is last at merge.
- NOTE `Source/Notation/PerformanceScore.h:36 numTypes` — append `pickStrokeUp/Down` before it; `Source/Capture/PerformanceCapture` receives `e.technique` today.
- NOTE `Source/UI/EasyPanel.h:118 playingModeSelector` (Easy mode column) and `Source/UI/RhythmPanel.h:133 RhythmPanel` (PLAYING group host).
