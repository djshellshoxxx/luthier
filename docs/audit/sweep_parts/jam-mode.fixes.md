All rows are OWNED by the jam-mode FEAT session; no Phase-2 fixes. Pre-existing code the owner must integrate with:

- NOTE `Source/Rhythm/ChordDetector.h:87 ChordDetector` (`kConfidenceFloor = 0.6` at :95; `ChordSymbol::isSlash` in `ChordDetector.cpp:206`; templates' `intervalMask` at `ChordDetector.cpp:156`) — reuse as-is for `JamChordFollower`.
- NOTE `Source/Rhythm/RhythmEngine.h:109 isBassFamily`, `:202 isDriving`, `:207 getCurrentChord`.
- NOTE `Source/Practice/Metronome.h:85`, `Source/Practice/Looper.h:154` (needs new `renderPlaybackMidi`), `Source/Practice/BackingTrack.h:43 BackingTrackPlayer`; `Source/Live/TapTempo.*` and `LuthierAudioProcessor::getEffectiveTempo` (used by `UI/LiveStrip.cpp:394`).
- NOTE `Source/Live/LiveControls.h:31 KillSwitch` — needs `applyBlockRamp`.
- NOTE `Source/PluginProcessor.cpp:33 buildBusesProperties` — order is Main, Sidechain in, Aux 1-7 (`kNumAuxBuses`), per-string buses, then Aux 8 (`kNoiseAux`) last; Jam Aux 9/10 must append after Aux 8, and `isBusesLayoutSupported` + `Source/Routing/RoutingMatrix` (`MidiOutConfig` at `RoutingMatrix.h:43`) must learn them.
- NOTE `Source/Tune/TuneMidi.h:124 TuneTimeline` / `TuneMidi.cpp:236 build` — where `JamChordMap` is emitted; tune-help workstream edits Tune too.
- NOTE `Source/PhysicalRange.h:29 RangeFamily` (add `jam` before `numFamilies`); `Source/DSP/Common/DspCommon.h:394 RtRandom`; `GenreKitLibrary` (`Source/Rhythm/GenreKit.h`); `preset_morph_position` exclusion pattern in `Source/Presets/PresetManager.cpp` for transient params.
- NOTE No `ModalResonatorBank` exists; realism-a (body-coupling) may add modal banks — check before writing a second one.
