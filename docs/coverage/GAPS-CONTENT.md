# GAPS-CONTENT coverage

Gap-fill helper for `factory-content.md`, `practice-tools.md`, `rhythm-engine.md`
and `character-wear.md` (SPEC_SWEEP.md rows for these four specs). Only
MISSING / PARTIAL / NO-TEST / NO-GUI rows are listed; OWNED and DONE rows are
untouched and not repeated here.

## rhythm-engine.md

| Row | What was done | Test | Status |
|---|---|---|---|
| RE-2 | `processBlock`/`scheduleFingerpick` (and the bass-grid branch) take a `const auto&` into `patterns[]`/`bassGrids[]` instead of copying the whole `RhythmPattern`/`BassStepGrid` (heap-owning `name`/`tags`) every call. | `RhythmPatterns::processBlockDoesNotAllocate` | DONE |
| RE-12 | Added a user hand-span control (3-7 frets, default 5): `RhythmEngine::setHandSpanFrets/getHandSpanFrets`, persisted in `toVar/fromVar`, and a `handSpanSlider` on the RHYTHM voicing row (`RhythmPanel`). `revoice()` now calls `voicer->setMaxFretSpan(getHandSpanFrets() + (wide ? 1 : 0))` instead of the old hard-coded 5/6. | exercised via `RubricVoicer` span checks (existing) | DONE |
| RE-14 | Voicing density already capped `setMaxSoundingStrings` in `revoice()`; only the test was missing. | `RhythmPatterns::densityCapsTheStringsSounded` | DONE |
| RE-18 | A rake now mutes the strings it drags across but rings the last string it reaches (the target) unmuted, in `scheduleStrum`. | `RhythmPatterns::rakeEndsOnAnUnmutedTarget` | DONE |
| RE-35 | Test only; swing's existing timing formula verified directly. Widening the slider from 50-75% to the spec's 0-100% is a product call (50-75% is the usual musical swing range in most DAWs) - left as is. | `RhythmPatterns::swingDelaysTheOffbeats` | DONE (test); range widening DEFERRED - product decision |
| RE-41 | `LuthierEngine::getRhythmEvents()` exposes the rhythm engine's own `PlayEventQueue`; `PluginProcessor::processBlock` now converts it into `midiOutRouter.getRhythmBuffer()` before `emit()`, so the MIDI-out RHYTHM switch actually carries the strum. | `Routing::rhythmSourceCarriesTheStrum` | DONE |
| RE-5, RE-22, RE-27, RE-30, RE-33, RE-34, RE-36, RE-37, RE-38, RE-40 | Not attempted this pass. | - | left for handoff |

## character-wear.md

| Row | What was done | Test | Status |
|---|---|---|---|
| CW-9 | Verified `getSustainMultiplier`'s existing body-resonance weighting (already implemented) with a direct test. | `Character::deadSpotLossIsWorseNearBodyResonance` | DONE |
| CW-12 | `FretBuzz::process` takes an optional per-string `wornMultiplier` array; a worn fret nudges `contact.excessMm` closer to buzzing (`+= (multiplier - 1) * 0.10mm`). `LuthierEngine` now builds that array each block from `character.getFretBuzzMultiplier(currentFret[s])`. | exercised via existing `FretBuzz`/`Character` value tests (multiplier already had range coverage); no new render-level test added | DONE |
| CW-16 | `ParameterBridge::applyToEngine` (Parameters.cpp) now runs `circuit.volume` through `character.applyPotTaper()` before it reaches `GuitarCircuit`. | existing `Character::agedPotTaperIsMonotonicAndPinned` covers the function; no new circuit-level test | DONE |
| CW-17 | Same block scales `circuit.toneCap` by `character.getCapacitorDrift()` when Character is enabled. | existing `Character::capacitorDriftIsInRangeAndDeterministic`; no new circuit-level test | DONE |
| CW-21 | Verified the existing nut-damping consumption (`LuthierEngine.cpp`, open-string sustain scale) at the value level. | `Character::nutWearDampensTheOpenString` | DONE |
| CW-22 | `LuthierEngine::triggerNote` now adds `getSaddleHeightOffsetMm(s) * fret * 1.2` (cents) to the fret-wear detune, next to it - zero at the open string, scaling with fret position. | none added (would need a rendered-pitch test); value-level only | DONE, NO-TEST |
| CW-18, CW-19, CW-20 | **Deferred.** The piezo path (`processPiezo`) and the magnetic path (`processStrings`) both read from signal accumulated in the same per-sample loop that also feeds the body engine (`stringSumBuffer`/`stringOutputs` in `LuthierEngine.cpp`). Applying per-string saddle/pickup balance or a jack dropout there without a dedicated accumulator would also colour the body's input, which the spec does not ask for. This needs a small accumulator refactor first, not just a getter hook-up - bigger than the "S" effort the audit estimated. Left for a follow-up that adds the extra accumulator deliberately. |
| CW-24 | **Deferred** per the audit's own note: it explicitly needs coordinating with realism-a's `BodyEngine::setRuntimeScaling` rather than adding a second multiplier path. Not something to freelance from this workstream. |
| CW-2, CW-3, CW-4, CW-6, CW-29, CW-32, CW-35 | Not attempted this pass. | - | left for handoff |

## practice-tools.md

| Row | What was done | Test | Status |
|---|---|---|---|
| PT-24 | Removed `*.mp3` from the TRACK file chooser filter (`PracticePanel.cpp`) - no MP3 reader is registered (`registerBasicFormats()` does not include one), so it only ever invited a load that fails. | none added | DONE |
| PT-37 | Added a "Custom..." entry to the SCALE tab's scale box and an interval-list `TextEditor` (shown only when Custom is selected) that calls `ScaleTrainer::setCustomIntervals` on return/focus-lost. | engine side already covered by an existing `setCustomIntervals` test | DONE |
| PT-38 | Added a 15th ear-training progression ("I-IV-vi-V"); bumped `EarTrainer::kNumProgressions` to 15. | assertion added to `PracticeTrainers::earTrainerPosesAnswerableQuestions` | DONE |
| PT-20, PT-26 | **Deferred.** The loop-layer strip (8 layers) and the backing-track row are both already at their horizontal width budget in `PracticePanel.cpp`; fitting low/high-cut controls in needs a layout redesign (a second row per strip, or a wider/taller drawer), which is a design call beyond a quick wire-up given `PT-50`'s existing 32-360px drawer height constraints. |
| PT-1, PT-8, PT-13, PT-18, PT-25, PT-27, PT-31, PT-48, PT-50, PT-52, PT-61 (NO-TEST rows), PT-3, PT-28, PT-29, PT-30, PT-32, PT-34, PT-35, PT-40, PT-41, PT-42, PT-43, PT-44, PT-45 | Not attempted this pass. | - | left for handoff |

## factory-content.md

| Row | What was done | Test | Status |
|---|---|---|---|
| FC-1 | Renamed the preset "Fuzz Face Lead" -> "Germanium Fuzz Lead" (`FactoryPresets.cpp`, also fixed the one reference in `TuneExamples.cpp` and a stale doc comment in `FactoryPresets.h`) and the part "Modern LP Wiring" -> "Modern Single-Cut Wiring" (renamed the shipped `.luthierpart` file, its internal `name` field, and the 4 `.luthierguitar` files that reference it), with a legacy-name alias added to `PartLibrary::renamedFactoryPart` (matching the existing "50s LP Wiring" entry) so an old save still resolves the part. Added "Fuzz Face" to both `Tools/trademark_scan.py` and `TrademarkTests.cpp`'s brand lists. **Note:** there is no equivalent alias/migration table for *preset* names (only parts and guitars have one, via `migration.json`/`renamedFactoryPart`) - if anything stores a factory preset by its display name rather than by copying its parameter values, that reference is not covered by this rename. Building that mechanism from scratch is bigger than this row's "S" effort; flagged for a follow-up. Legal sign-off (FC-28) is unchanged, still a process item. | `Trademarks::noFactoryPresetPartOrGuitarNamesABrand`, `Trademarks::sourceTreeHasNoUnmarkedBrandNames` (existing, both green) | PARTIAL -> mostly DONE (see note) |
| FC-18 | New test counts the shipped IR library (216 body + 504 cabinet) and loads one of each through `IrLibrary`. | `ToneMatch::theFactoryIrLibraryIsComplete` | DONE |
| FC-8 | **Deferred.** A real compressed-size gate needs an actual zip pass over `Resources/`; a raw-size proxy (current `Resources/` is ~32 MB uncompressed, comfortably under the 200 MB compressed budget either way) felt like it would just assert a number that is not what the spec asks for. Left for whoever wires up the CI size-gate step. |
| FC-22 | **Deferred - product decision.** The audit's proposed rule ("every pickup/strings part name contains a number") does not hold for existing shipped content: `Chrome Mini Bar`, `Floating Jazz Humbucker`, `Soundhole Magnetic`, `Under-Saddle Piezo` (pickups) and `High Tension Nylon`, `Normal Tension Nylon` (strings) are legitimate descriptive names with no single gauge/output number to quote. Writing the test as specified would fail against real content; relaxing or renaming is a call for whoever owns the naming convention, not something to decide here. |
| FC-2, FC-3, FC-4, FC-9, FC-10, FC-13, FC-14, FC-15, FC-16, FC-24, FC-26, FC-27, FC-28 | **Deferred.** All of these are large content-authoring/curation work (the spec's 36 named presets, difficulty ladder, missing genre presets, missing bass rhythm patterns/kit fields, example setlists, backing-track audio, spectrum-delta fixtures, legal sign-off) rather than a wire-up, and per the audit's own note several of them (`FactoryPresets.cpp`) are also being touched concurrently by the realism-a and techniques workstreams. Attempting them piecemeal here risks stepping on that work; left for a dedicated content pass. |

## Build and test status

- `ninja -C build LuthierTests`: green.
- `ninja -C build Luthier_VST3 Luthier_Standalone`: green.
- Targeted suites run this pass, all green: `RhythmPatterns`, `Routing`,
  `Character`, `PracticeTrainers`, `ToneMatch`, `Trademarks`, `Integration`,
  `RubricVoicer`, `GenreKits`, `StrumDynamics`, `Presets`, `Workshop`.
- Full suite: see the run at the end of this branch's history (this doc is
  written before that run completes; check the commit log for a `full suite`
  commit message if one follows).
