## REVIEW.md

REVIEW.md lists ten gaps and five ambiguities in the original four specs, plus a milestone order. On this checkout every gap has a spec, an implementation, a UI home and tests, and every ambiguity is resolved in code. Notation now captures techniques and chords from the engine. The one real shortfall is gap 9: localisation offers 15 locales but ships only the English catalog. Gap 3's drag-to-assign modulation convenience has landed (`DragToModulate::*`).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| RV-1 (Gap 1) | Rhythm / pattern engine and practice metronome / looper / backing track | `Rhythm/*`, `Practice/*` | col 4 RHYTHM, practice drawer | `RhythmPatterns::*`, `GenreKits::everyKitSoundsWhenApplied`, `PracticeMetronome::*`, `PracticeLooper::*` | DONE |
| RV-2 (Gap 2) | Live layer: snapshot banks, morph, program-change routing, tap tempo, monitor mix, kill switch, expression calibration | `Live/*` | LIVE tab, live strip, Options EXPRESSION | `LiveSnapshots::*`, `LiveTapTempo::*`, `LiveMonitor::*`, `LiveKillSwitch::*`, `LiveExpression::*` | DONE |
| RV-3 (Gap 3) | Modulation matrix: sources route to any parameter, step sequencers, macro curves | `Modulation/ModMatrix`, `ModSources` | col 4 MOD, right-click > Modulate | `Modulation::*`, `Editor::rightClickOffersModulationAndBuildsTheRoute` | DONE |
| RV-4 (Gap 4) | Multi-out (dry / wet / DI / per-string), sidechain, MIDI out, re-amp | `Routing/*`, processor buses | col 4 ROUTING | `Routing::*`, `PluginBuses::perStringLayoutPutsEachStringOnItsOwnBus` | DONE |
| RV-5 (Gap 5) | User IR loading, folder convention, cab / EQ match, capture workflow | `ToneMatch/*` | col 4 TONE MATCH | `ToneMatch::*`, `ReviewRegression::aCaptureRecordsTheMainOutput` | DONE |
| RV-6 (Gap 6) | Notation output: format, schema, live TAB, capture of what was played incl. techniques | `Notation/*`, `Capture/*` | col 4 NOTATION | `Notation::*`, `NotationTab::*`, `ModelGapsUi::theCaptureHearsTechniquesAndChordsFromTheEngine` | DONE |
| RV-7 (Gap 7) | Controllers: GK, TriplePlay, Jamstik, MPE; per-source latency, calibration, hex-channel routing | `Controllers/*` | col 4 CONTROLLERS | `Controllers::perChannelRoutingSendsEachChannelToItsString`, `Controllers::latencyWizardIsStableAndReportsItsScatter` | DONE |
| RV-8 (Gap 8) | Character / aging beyond one selector: dead spots, fret wear, tuner drift, capacitor aging | `Character/*` | col 4 CHARACTER | `Character::*` | DONE |
| RV-9 (Gap 9) | Accessibility and localisation: screen reader, colourblind palettes, keyboard-only, i18n catalog, RTL — the catalog is English only; no translated catalogs, so RTL never engages | `Accessibility/*`, `Localisation::loadCatalog` | Options > ACCESSIBILITY / LOCALIZATION | `Accessibility::*`, `Localisation::*` | PARTIAL |
| RV-10 (Gap 10) | Updates, telemetry, crash reporting, delivery to support, auto-update | `Updates/*` | Options UPDATES / PRIVACY, banners | `Telemetry::*`, `Editor::theWindowRaisesSectionFifteensTriggersAndIsQuietWhenItShould` | DONE |
| RV-11 (Amb) | Chord auto-fingering rubric (hand span, playability, barre / open weights, capo) | `Model/Playing/RubricVoicer` | n/a | `RubricVoicer::weightsAreTheSpecsWeights`, `RubricVoicer::theCapoIsWhereTheNeckStarts` | DONE |
| RV-12 (Amb) | Feedback: pick one model — a physical loop | `DSP/Feedback/FeedbackLoop` | col 3 FEEDBACK | `Feedback::eachStringHearsItsOwnNote`, `Feedback::theVolumeKnobLowersTheLoopByTheCircuitsAttenuation` | DONE |
| RV-13 (Amb) | Freeze vs E-Bow: both, as two features | `FreezeOverlay`, `EBowDriver` | col 3 SUSTAIN | `Sustain::*`, `EBow::*` | DONE |
| RV-14 (Amb) | Doubler timing / pitch / pan defaults | Doubler pedal (`PedalsMod`) | post-FX rack | `Doubler::*`, `ModelGapsUi::theDoublerDefaultsAreTheClassicAdt` | DONE |
| RV-15 (Amb) | Preset morph specified (smooth A→B transition) | `Presets/PresetMorph` | preset browser morph row | `PresetMorph::*` | DONE |
| RV-16 (Order) | Milestone order routing-io → updates-telemetry followed | M31-M41 in `spec/PROGRESS.md` | n/a | n/a | DONE |

<!-- counts DONE=15 NO-GUI=0 NO-TEST=0 PARTIAL=1 MISSING=0 OWNED=0 -->
