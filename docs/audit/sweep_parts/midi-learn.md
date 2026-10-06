## midi-learn.md

Unified MIDI Learn: sources (CC/PC/pressure/notes/MPE), two entry points, mapping list, global vs preset, ranges, serialization. Deferred feature, no owner; a substantial base exists (`MidiLearnManager`, arm overlay, global mappings) so most rows are PARTIAL. Missing: MPE dimensions, burst resolution, mappings popover with Clear all / Save-all-as-global.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| ML-0 (§0) | Ground rules (no audio-thread alloc, one control one mapping) | `Source/Support/MidiLearn.h:MidiLearnManager` | - | `MidiLearn::learningDoesNotAllocateOrLockOnTheAudioThread` | PARTIAL |
| ML-1 (§1) | Sources: CC, PC, aftertouch, pressure, notes, MPE dims | `MidiLearnManager::sourceKeyFor`, `describeSource` | - | `MidiLearn::learnsPcAftertouchAndPressure`, `MidiLearn::notesAreLearnedOnlyWhenAllowed` | PARTIAL |
| ML-2.1 (§2.1) | Header arm then click | `MidiLearnManager::setArmed/claimArmedLearn/expireIfIdle` | Header arm button, `MidiLearnArmLayer` (Overlays.h), `LuthierAudioProcessorEditor::setMidiLearnArmed` | `MidiLearn::armingIsSeparateFromLearningUntilAControlClaimsIt`, `Editor::ctrlLArmsMidiLearn`, `MidiLearn::armedButtonAndTargetPulse` | DONE |
| ML-2.2 (§2.2) | Direct per-control right-click learn | `MidiLearnManager::startLearning` | `Widgets.cpp` context menu item 5 'MIDI Learn' | `MidiLearn::mapsAndUnmapsCleanly` | DONE |
| ML-2.3 (§2.3) | Catching the source: burst resolve, 20 s timeout, status text | `MidiLearnManager::handleAsyncUpdate/expireIfIdle` | status banner in editor | `MidiLearn::armingTimesOutAfterThirtySeconds`, `MidiLearn::everyCcLearnsWithinOneBlock` | PARTIAL |
| ML-3 (§3) | Mapping list popover, Clear all, Save-all-as-global | `MidiLearnManager::clearAllMappings/getNumMappings` | none found (no mappings popover) | - | NO-GUI |
| ML-4 (§4) | Global vs preset mappings, preset wins on conflict | `MidiLearnManager::setMappingGlobal/isMappingGlobal/getGlobalMappingsFile/mergeGlobalMappings` | Widgets.cpp context menu global toggle | `MidiLearn::aGlobalMappingSurvivesAPresetLoad` | PARTIAL |
| ML-5 (§5) | Ranges, invert, modulation interaction with advanced ranges | `MidiLearnManager::setMappingRange` | none (no range editor) | - | NO-GUI |
| ML-6 (§6) | Serialization (midi_mappings, mpe_dimension), undo, a11y | `MidiLearnManager::toVar/fromVar`, `PresetBlocks.cpp midiMappings` | - | `LiveInput::assignmentsRoundTripThroughTheirConfigFile` (other file) | PARTIAL |
| ML-7 (§7) | Failure modes | `MidiLearnManager::cancelLearning` | - | `MidiLearn::disarmingCancelsAnInFlightLearn`, `Stress::midiLearnArmDisarmHundredTimes` | PARTIAL |
| ML-T1 (§8 LEARN-01..05) | Entry-point parity, all params learnable, CC catch, burst, timeout | see ML-2 | - | `MidiLearn::everyCcLearnsWithinOneBlock`, `MidiLearn::armingTimesOutAfterThirtySeconds` | PARTIAL |
| ML-T2 (§8 LEARN-06..08) | MPE aggregation, note-class toggle, one-control-one-mapping | - | - | `MidiLearn::notesAreLearnedOnlyWhenAllowed` | PARTIAL |
| ML-T3 (§8 LEARN-09..14) | Global/preset resolution, serialization, range, undo, popover, a11y | - | - | `MidiLearn::aGlobalMappingSurvivesAPresetLoad` | PARTIAL |
