## tab-export.md

TuneToScore: convert a written Tune into PerformanceScore and export ASCII tab / MusicXML / GP / combined print page. Codex-owned lane (origin/codex/luthier-tab-export, luthier-notation-export). This checkout has the Notation exporters (`Source/Notation`) and the TUNE export dialog, but no `TuneToScore`, print page or one-click Tab button.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TX-0 (§0) | Ground rules | `Source/Notation/PerformanceScore.h` | - | - | OWNED |
| TX-1.1 (§1.1) | TuneToScore inputs | - | - | - | OWNED |
| TX-1.2 (§1.2) | Per-section conversion (voicings, strum expansion, melody, layers) | - | - | - | OWNED |
| TX-1.3 (§1.3) | What it does not resolve | - | - | - | OWNED |
| TX-1.4 (§1.4) | Relationship to PerformanceCapture | `Source/Capture/PerformanceCapture.cpp` | - | `Capture::recordsVoicedNotesNotMidi` | PARTIAL |
| TX-2 (§2) | 'If feasible' resolution | - | - | - | OWNED |
| TX-3 (§3) | Combined print page (PDF/PNG) | - | - | - | OWNED |
| TX-4.1 (§4.1) | TUNE tab Tab button, source picker | - | `TuneExportDialog.h`; no Tab button / source picker | `TuneIntegration::theExportDialogWritesEachDestinationFromOneScreen` | PARTIAL |
| TX-4.2 (§4.2) | Notation export dialog source picker | `Source/Notation/NotationExport.cpp` | `NotationPanel.h`, `Overlays.h exportTabButton` | `Notation::asciiTabColumnsAlign`, `Notation::musicXmlRoundTrips`, `Notation::guitarProRoundTripsStringsAndFrets` | PARTIAL |
| TX-4.3 (§4.3) | Empty and error states | `NotationExport.cpp` | - | `Notation::anEmptyScoreIsRefusedWithAReason` | PARTIAL |
| TX-5 (§5) | Interactions with other specs | `TuneIntegration::notationAndProjectExportKeepSectionsChordsAndTheBundle` | - | `TuneIntegration::notationAndProjectExportKeepSectionsChordsAndTheBundle` | PARTIAL |
| TX-T1 (§6 TABX-01..06) | Determinism, voicing parity, strum, technique, sections, layers | - | - | - | OWNED |
| TX-T2 (§6 TABX-07) | Format parity via existing exporters | `Source/Notation/NotationExport.cpp` | - | `Notation::asciiTabRoundTrips`, `Notation::musicXmlRoundTrips` | PARTIAL |
| TX-T3 (§6 TABX-08..12) | Print page, source picker, one-click, worker thread, empty fallback | - | - | - | OWNED |
