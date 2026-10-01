No NO-GUI/NO-TEST/PARTIAL/MISSING rows; everything not DONE is OWNED by the mic-placement FEAT session. Pre-existing code the owner must integrate with:

- NOTE Legacy params `Source/Parameters.h:226-235` (`mic_type`, `mic_position`, `mic_distance`, `_2` variants, `mic_blend`, `mic_width`, `mic_phase_align`) and their Advanced Col 3 CAB controls `Source/UI/AdvancedPanel.cpp:813-826` — these become the Quick combos; keep them attached so `GuiReach.*` stays green.
- NOTE `Source/Parameters.h:507 applyStructural`, `:510 readStructuralValues` (currently treat position/distance as structural -> IR reload), `:484 writtenSinceGuitarType` + `:560 lastWrite` stamps (for the ±250 ms legacy-write rule).
- NOTE `Source/DSP/Amp/CabinetEngine.cpp:23 kSpeakers`, `:45 kMics` (anonymous namespace, to move to `CabinetVoices.h`); `CabinetEngine.h:78 getIrLoadCount` (MP-13 hook already exists); `IrLibrary::findCabIr`.
- NOTE `Source/DSP/Common/DspCommon.h:161 Biquad` (not modulation-safe), `:307 ExpSmoother`, `:394 RtRandom`; `Source/DSP/Body/BodyEngine.h:96 getAirResonanceHz`.
- NOTE Parameter append order: last param today is `aux1_pre_circuit`; realism/techniques branches and the other FEAT specs (auto-articulation `aa_*`, jam `jam_*`) also append — coordinate final order and the index-stability tests (MP-01, AA-34, JM-36).
- NOTE Room: `RoomEngine` (materials/sizes) needs `setCloseMicDistance`; realism-a (environment) may also touch room — check `origin/claude/luthier-realism-a` before editing.
