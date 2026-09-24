All rows are OWNED by the riff-library FEAT session; no Phase-2 fixes. Pre-existing code the owner must integrate with:

- NOTE `Source/Export/MidiPerformance.h:161 fromScore`, `MidiPerformance.cpp:25 kTechniques` (tech wire tokens, static_assert against `ScoreTechnique::numTypes`); `Source/Export/MidiProfiles.h:137 exportToMemory`; drag pattern `Source/UI/MidiOutPanel.cpp:144 performExternalDragDropOfFiles`.
- NOTE `Source/Export/MidiImportTargets.h:51 importPerformance` (Send to Looper path).
- NOTE `Source/UI/PracticePanel.h:207 TabReaderTab` — needs the new `openScore(const PerformanceScore&, title)`.
- NOTE `Source/Tune/TunePlayer.*` — SpinLock/ScopedTryLock hand-over + `collectGarbage` pattern to copy; `Source/Tune/TuneSession`/`TuneMidi` (Add to Tune, `MelodyNote::extra` coordination); tune-help workstream is also editing Tune.
- NOTE `Source/Model/Playing/PlayingEvents.h:48 NoteOnEvent` — auto-articulation spec adds `palmMuteAmount`; this spec adds `palmMuteDepth` + `explicitArticulation`. Same concept, two names: owners must agree on one field.
- NOTE No `edition::Feature`/`Limits` exist on this checkout (editions.md is also OWNED/deferred); §11 depends on it landing first.
- NOTE Advanced workspace tab order is set in `Source/UI/AdvancedPanel.cpp` (`getWorkspaceTabName`/`setWorkspaceTabNamed`); RIFFS goes between TUNE and LIVE. `R` must be checked against `AccessibilitySettings::buildDefaultShortcuts` (`Source/Accessibility/Accessibility.cpp:458`) and any bindings added on other owner branches.
- NOTE `Tools/RenderCli.cpp` (for `--export-riffs`), `Tools/trademark_scan.py`, `Tools/generate_factory_parts.py` (style reference); `IrLibrary::getResourcesFolder` (`Source/Support/IrLibrary.h`).
