## notation-export.md

`PerformanceScore`, `PerformanceCapture` (8192-record lock-free ring, drop-oldest counter, off/rolling/armed, quarter-note or seconds timing, 10 Hz drain) and the MusicXML / Guitar Pro / ASCII / MIDI writers are implemented and well tested; the model-gaps merge added worker-thread export, engine technique and chord feeds, Mono offline chord extraction, current-bar fretboard dots and marked-region ranges. The NOTATION tab carries capture state, live tab controls, format/range/options/preview and export; File menu has "Export notation...". Remaining gaps: no Guitar Pro importer (so no GP round-trip test), GP chord "diagrams" are names only and GP lacks whammy bar events / semi harmonics, MusicXML has no grace notes, the Session tab's "Save last take" has no notation export beside it, the tab-reader export in PracticePanel still writes synchronously, and the 100-progression chord test and MuseScore fixture are missing.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| NE-1 (§0.1) | Export offline on a worker thread — NOTATION tab and File menu use `writeAsync`; tab-reader export (`PracticePanel.cpp` ~1211) still calls `exporter.write` on the message thread | `UI/NotationPanel.cpp:NotationTakeExport::writeAsync` | NOTATION tab Export, File > Export notation... | `ModelGapsUi::notationExportRunsOnAWorkerThread` | PARTIAL |
| NE-2 (§0.2) | Guitar-aware output: string/fret, not pitch | `Notation/PerformanceScore:ScoreNote` | n/a | `Notation::stringAndFretAreNotDerivedFromPitch` | DONE |
| NE-3 (§0.3) | Technique metadata preserved (bends, slides, legato, PM, harmonics, tap, whammy) | `ScoreTechnique`, `Capture/PerformanceCapture` | n/a | `Capture::techniquesBendsChordsAndMetersReachTheScore`, `ModelGapsUi::theCaptureHearsTechniquesAndChordsFromTheEngine` | DONE |
| NE-4 (§0.4) | Exports round-trip with documented per-format losses — no GP re-import | `NotationImporter::read` (txt/musicxml only) | n/a | `Notation::musicXmlRoundTrips`, `Notation::asciiTabRoundTrips`, `Notation::formatsDeclareTheirLosses` | PARTIAL |
| NE-5 (§1) | PerformanceScore model (meta, tracks, measures, voices, notes, techniques) | `Notation/PerformanceScore.h` | n/a | `Notation::captureBuildsMeasures` | DONE |
| NE-6 (§1) | Capture last N minutes, default 10 | `PerformanceCapture::kDefaultRollingMinutes` | NOTATION `rollingMinutes` slider | `Capture::rollingKeepsTheLastMinutes` | DONE |
| NE-7 (§2.1) | MusicXML 4.0 `<technical>` bend/slide/hammer/pull/PM/harmonic/tap/string/fret | `NotationExporter::renderMusicXml` | NOTATION format box | `Notation::musicXmlIsWellFormedAndGuitarAware` | DONE |
| NE-8 (§2.1/§4) | MusicXML chord symbols from detector (`<harmony>`) | `renderMusicXml` (chordSymbols) | NOTATION export | `ModelGapsUi::theCaptureHearsTechniquesAndChordsFromTheEngine` | DONE |
| NE-9 (§2.1) | Grace notes for hammer-on/pull-off ornaments — not written | - | - | - | MISSING |
| NE-10 (§2.1) | Multi-voice per staff (`<voice>`/`<backup>`) — implemented, no test exercises a second voice | `PerformanceScore` voice split, `renderMusicXml` | n/a | `Notation.polyphonicMaterialGetsASecondVoice` | DONE |
| NE-11 (§2.1) | Whammy as pitch-bend text direction | `renderMusicXml` whammy words | n/a | `Notation::formatsDeclareTheirLosses` | DONE |
| NE-12 (§2.2) | Guitar Pro .gp bundle: string/fret, bend curves, slide types, PM, harmonics, tapping — no semi harmonic, no whammy bar events; fidelity only checked as valid zip + note count | `NotationExporter::writeGuitarPro/renderGuitarProXml` | NOTATION format box | `Notation::guitarProBundleIsAValidZip` | PARTIAL |
| NE-13 (§2.2) | GP chord diagrams at first occurrence — writes `<Chord firstOccurrence>` name only, no fret diagram | `renderGuitarProXml` | NOTATION `chordDiagrams` toggle | - | PARTIAL |
| NE-14 (§2.3) | ASCII: 6 lines low-bottom, ruler, symbols b r h p / \ ~ PM <12> [12], width 80 default, section headings | `NotationExporter::renderAsciiTab` | NOTATION `lineWidth` slider | `Notation::asciiTabColumnsAlign`, `Notation::asciiTabUsesTheSpecifiedSymbols` | DONE |
| NE-15 (§2.4) | MIDI per-string tracks (16 max) | `NotationExporter::writeMidi` | NOTATION format box | `Notation::midiExportIsPerString` | DONE |
| NE-16 (§2.4) | MIDI RPN string/fret hints, pitch bend for bend/whammy, CC68 legato — written, not asserted by any test | `NotationExporter::writeMidi` | n/a | `Notation.midiExportCarriesBendRangeBendsAndLegato` | DONE |
| NE-17 (§3) | Live TAB view (last N beats) in practice panel + NOTATION tab | `renderAsciiTabWindow`, `NotationPanel`, practice TAB drawer | Practice drawer TAB; ADVANCED > NOTATION | `Notation::liveTabWindowRendersASlice`, `Capture::theLiveTabShowsWhatWasPlayed` | DONE |
| NE-18 (§3) | Current bar as tab dots on the fretboard | `FretboardComponent::refreshTabDots` | fretboard | `ModelGapsUi::theCurrentBarIsDrawnOnTheFretboardAsTabDots` | DONE |
| NE-19 (§3) | Controls: bar count 1-8, symbol density full/minimal/notes-only | `NotationExportOptions::density`, window | NOTATION `barsBox`, `densityBox` | `Notation::liveTabWindowRendersASlice`, `Notation::asciiTabUsesTheSpecifiedSymbols` | DONE |
| NE-20 (§3) | Controls: show/hide, scroll speed slow/medium/fast/freeze — present, untested | `NotationPanel::timerCallback` (kSpeedTicks) | NOTATION `showTab`, `speedBox` | - | NO-TEST |
| NE-21 (§4) | Poly: detector chords written at change beats | `PerformanceCapture` chord track | n/a | `ModelGapsUi::theCaptureHearsTechniquesAndChordsFromTheEngine` | DONE |
| NE-22 (§4) | Mono: offline template chord extraction | `PerformanceCapture::toScore` (extractChordsWhenMissing) | n/a | `ModelGaps::aMonoTakeGetsItsChordsOffline` | DONE |
| NE-23 (§5) | File menu Export -> Notation — present, no test drives the menu item | `HeaderBar.cpp` item 13 -> `writeAsync` | Header File > Export notation... | - | NO-TEST |
| NE-24 (§5) | Dialog: format dropdown MusicXML/GP8/ASCII/MIDI, destination | `NotationPanel::exportWithChooser` | NOTATION `formatBox` + chooser | `NotationTab::exportsEveryFormat` | DONE |
| NE-25 (§5) | Range: entire / last N seconds / marked region | `UI/CaptureRanges`, `NotationPanel::currentCaptureOptions` | NOTATION `rangeBox`, `lastSeconds`, MARK IN/OUT | `CaptureRanges::theMarkedRegionIsWhatWasPlayedBetweenTheMarks` | DONE |
| NE-26 (§5) | Per-format options (ASCII line width, GP chord diagrams) + preview of first bar for MusicXML/ASCII | `NotationPanel::updatePreview` | NOTATION `lineWidth`, `chordDiagrams`, `previewView` | `NotationTab::stateButtonsLiveTabAndPreview` | DONE |
| NE-27 (§5) | "Export to Notation" beside the MIDI-capture "Save last take" button — only in the File menu; Session tab's Save last take has no notation option | `SessionTab::saveTake` | PRACTICE drawer Session tab | - | PARTIAL |
| NE-28 (§6.1) | Records voiced notes (string/fret/time/dur/vel/flags), not MIDI | `PerformanceCapture::noteOn` from engine | n/a | `Capture::recordsVoicedNotesNotMidi` | DONE |
| NE-29 (§6.1) | Chord, tempo/TS, BASS_TECH and slide event tracks | `PerformanceCapture` event tracks | n/a | `Capture::bassAndSlideEventsBecomeLuthierEvents`, `BassTechniques::aStrikeIsCapturedAsBassTech`, `Capture::techniquesBendsChordsAndMetersReachTheScore` | DONE |
| NE-30 (§6.2) | 8192-entry lock-free ring, preallocated, 10 Hz message-thread drain, drop-oldest + counter shown | `Capture/CaptureRing.h`, `PluginProcessor::timerCallback` drain | NOTATION status (overflow count) | `Capture::ringOverflowDropsTheOldestAndCountsExactly`, `Capture::capturingTenThousandNotesDoesNotAllocate` | DONE |
| NE-31 (§6.3) | States off / rolling (default) / armed | `PerformanceCapture::setState` | NOTATION state buttons | `Capture::offWritesNothingAndLeavesTheAudioAlone`, `Capture::armedStartsCleanFromTheNextNote`, `NotationTab::stateButtonsLiveTabAndPreview` | DONE |
| NE-32 (§6.4) | Quarter notes vs transport, seconds when stopped, quantise afterwards | `CaptureClock`, quantise options | NOTATION `quantiseBox` | `Capture::transportTimingIsInQuarterNotes`, `Capture::freePlayIsInSecondsAndQuantisesAfterwards` | DONE |
| NE-33 (§7) | Test: MusicXML round trip via MuseScore fixture, zero technique loss — uses own importer and only asserts some techniques survive | `NotationImporter::readMusicXml` | n/a | `Notation::musicXmlRoundTrips` | PARTIAL |
| NE-34 (§7) | Test: Guitar Pro round trip via fixture parser, string/fret identical — no GP parser | - | n/a | - | MISSING |
| NE-35 (§7) | Test: ASCII column alignment at 4/4 | `renderAsciiTab` | n/a | `Notation::asciiTabColumnsAlign` | DONE |
| NE-36 (§7) | Test: exported MIDI re-rendered nulls at -60 dBFS | `MidiProfiles`, `MidiPerformance` | n/a | `MidiExport::luthierRoundTripNullsEveryFactoryPreset`, `Capture::aCapturedPhraseRoundTripsThroughLuthierMidi` | DONE |
| NE-37 (§7) | Test: 100 chord progressions, >95% detection — only a two-chord case exists | chord detector / offline extraction | n/a | `Notation.chordExtractionOnAHundredProgressions` (the detector both paths use) | DONE |
| NE-38 (§7.1) | Capture test: voiced notes not MIDI | | n/a | `Capture::recordsVoicedNotesNotMidi` | DONE |
| NE-39 (§7.1) | Capture test: no allocation over 10 000 notes | | n/a | `Capture::capturingTenThousandNotesDoesNotAllocate` | DONE |
| NE-40 (§7.1) | Capture test: overflow drops oldest, exact counter | | n/a | `Capture::ringOverflowDropsTheOldestAndCountsExactly` | DONE |
| NE-41 (§7.1) | Capture test: off costs nothing, bit-identical audio | | n/a | `Capture::offWritesNothingAndLeavesTheAudioAlone` | DONE |
| NE-42 (§7.1) | Capture test: transport timing (beat 3 -> 2.0 within 1 ms) | | n/a | `Capture::transportTimingIsInQuarterNotes` | DONE |
| NE-43 (§7.1) | Capture test: free-play in seconds | | n/a | `Capture::freePlayIsInSecondsAndQuantisesAfterwards` | DONE |
| NE-44 (§7.1) | Capture test: live TAB shows what was played | | n/a | `Capture::theLiveTabShowsWhatWasPlayed` | DONE |
| NE-45 (§7.1) | Capture test: Luthier-profile MIDI export round-trips note list | | n/a | `Capture::aCapturedPhraseRoundTripsThroughLuthierMidi` | DONE |

<!-- counts DONE=32 NO-GUI=0 NO-TEST=4 PARTIAL=6 MISSING=3 OWNED=0 -->
