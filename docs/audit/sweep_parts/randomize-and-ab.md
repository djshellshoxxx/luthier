## randomize-and-ab.md

Randomize (category/amount/distribution/locks) and A/B compare slots. Deferred feature, no owner. Basic randomise (locks, respects-stock) and A/B slots with copy exist in `LuthierAudioProcessor`; category scope, Amount, triangular distribution, popover and per-field diff flip are missing.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| RA-0 (§0) | Ground rules, no audio-thread work | `PluginProcessor.cpp:LuthierAudioProcessor::randomiseParameters` | - | `Presets::randomiseNeverProducesSomethingBroken` | PARTIAL |
| RA-1.1 (§1.1) | Randomize categories (Tone, etc.) | `PresetManager::randomise` (all params, no categories) | none (no category popover) | - | MISSING |
| RA-1.2 (§1.2) | Amount control | - | - | - | MISSING |
| RA-1.3 (§1.3) | Musical distribution (triangular around current, within live range) | `PresetManager::randomise` | - | `Presets::randomiseRespectsLocks` | PARTIAL |
| RA-1.4 (§1.4) | Locks | `LuthierAudioProcessor::setParameterLocked/isParameterLocked` | `Widgets.cpp` menu 'Lock (exclude from randomise)' | `Presets::randomiseRespectsLocks` | DONE |
| RA-1.5 (§1.5) | Never touches structure (guitar, tuning, ranges, mappings, rack, uiState) | `PresetManager::randomise`, `randomiseRespectsStock` (`RangesUi::kRandomiseInStockKey`) | Options toggle `randomiseToggle` | `Presets::randomiseNeverProducesSomethingBroken` | PARTIAL |
| RA-2.1 (§2.1) | A/B slot contents | `LuthierAudioProcessor::storeToSlot/recallSlot` | HeaderBar `compareA/compareB` | `Presets::abSlotsCompareAndCopyTheCurrentOneAcross` | DONE |
| RA-2.2 (§2.2) | Active slot | `isSlotBActive/setSlotBActive` | HeaderBar A/B buttons highlight | `Editor::abButtonsHighlightTheActiveSlot` | DONE |
| RA-2.3 (§2.3) | Flipping: diff-only, click-free | `recallSlot` (whole-state recall) | HeaderBar, `abCompare` shortcut | `Editor::undoRedoAndABKeysReachTheProcessor` | PARTIAL |
| RA-2.4 (§2.4) | Copy A>B and Reset both | `LuthierAudioProcessor::copyAtoB` | HeaderBar `copyAB` | `Presets::abSlotsCompareAndCopyTheCurrentOneAcross` | PARTIAL |
| RA-2.5 (§2.5) | What A/B does not do; transient, not saved | processor slots not serialized | - | `Editor::abCompareIsTransientAndNotSaved`, `StateModel::aPresetLoadClearsABCompareWithABanner` | DONE |
| RA-3 (§3) | UI: randomise popover, A/B strip | `HeaderBar.cpp` randomise button; `EasyPanel` randomiseButton | HeaderBar, EasyPanel; no popover | `Editor::randomiseAndResetKeys`, `Onboarding::theRandomiseTooltipShowsOnTheFirstHoverOnly` | PARTIAL |
| RA-4 (§4) | Undo: one multi-target 'randomize' entry; flips not in history | `randomiseParameters` -> `pushUndoState("Randomise")` | - | `UndoSweep` none specific | PARTIAL |
| RA-5 (§5) | Interactions with other specs | - | - | - | PARTIAL |
| RA-T1 (§6 RAND-01..06) | Category scope, distribution, stock range, structure, undo, popover state | - | - | `Presets::randomiseNeverProducesSomethingBroken`, `Presets::randomiseRespectsLocks` | PARTIAL |
| RA-T2 (§6 AB-01..07) | Load resets, diff-only swap, click-free flip, copy/reset, save-active-only, undo independence, session boundary | - | - | `Presets::abSlotsCompareAndCopyTheCurrentOneAcross`, `StateModel::aPresetLoadClearsABCompareWithABanner`, `Editor::abCompareIsTransientAndNotSaved` | PARTIAL |
