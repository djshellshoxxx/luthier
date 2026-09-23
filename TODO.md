# TODO

Working list for the autonomous run (`PROMPT.md`). Order follows
`INDEX.md` "Build order for the engine work", then `CLAUDE_CODE_BRIEF.md`
steps 6-12. `GAPS.md` has the detail behind each row; `DECISIONS.md` has
every judgement call made along the way.

Build: `cmake --build build --config Release --target LuthierTests -- -v:m -p:CL_MPCount=1`,
one target at a time, foreground. Tests: `build/LuthierTests_artefacts/Release/LuthierTests.exe [filter...]`.

## In progress

- [ ] **3. `NoiseEngine` pool** — next.

## Remaining

- [ ] 2c. Feedback per `ambiguity-resolutions.md` 1 (mic-to-speaker loop,
      feedback_amount/distance/angle/focus/octave_bias params, post-circuit
      path, Adv Col 3 SUSTAIN feedback row). Currently a heuristic.
- [ ] 2d. Audit the rest of `ambiguity-resolutions.md` against the build
      (brief step 4).
- [ ] 3. `NoiseEngine` pool, then `PickModel`, `SqueakModel`, buzz sensing
      (`pick-noise.md`, `string-squeak.md`, `fret-buzz.md`); CHARACTER PICK /
      STRING NOISE groups; noise-event strip; Aux 8 noise bus row. CHARACTER
      tab-header padlock (gui-integration 21) via `RangesUi::drawPadlock`.
- [ ] 4. `SetupGeometry` (`fret-buzz.md`).
- [ ] 5. `SlideEngine` (`slide-guitar.md`), Slide mode.
- [ ] 6. `PartLibrary` and `mapSpec` (`guitar-workshop.md`, `part-acoustics.md`),
      `.luthierguitar` / `.luthierpart` files, partial capo.
- [ ] 7. `WorkshopPanel` and the WORKSHOP tab (`workshop-ui.md`), with its
      tab-header padlock.
- [ ] 8. `StrumGesture` (RHYTHM STRUM group), then `BassTechniques` (SLAP
      group, bass step grid).
- [ ] 9. `PerformanceCapture`, then the NOTATION tab.
- [ ] 10. MIDI export profiles (`midi-export.md`), then the MIDI OUT tab.
- [ ] 11. `PracticeRoutines` and the PRACTICE tab.
- [ ] 12. Tune Builder (`tune-builder.md`) and the TUNE tab.
- [ ] 13. HELP tab (column 4).
- [ ] 14. Audit `ui-wiring.md`, `onboarding.md`, `performance-budget.md`,
      `qa-polish.md`, `installer.md`, and gui-integration 20-22 against the
      build (GAPS.md "Not audited yet"); fix what they find.
- [ ] 14b. `action-and-undo.md`: audit entry classes, grouping and state
      boundaries against the snapshot undo stack (fixed off-by-one in c0b05eb).
- [ ] 14c. Onboarding: "Restore first-run experience" in Options ->
      Diagnostics must also clear `ranges_first_unlock_explained`.
- [ ] 15. Polish pass, performance pass, onboarding pass (brief steps 7-9).
- [ ] 16. Installer pass (brief step 10). The platform matrix cannot be run
      from this Windows-only machine; see DECISIONS.md when reached.
- [ ] 17. Bug bash and final check (brief steps 11-12).
- [ ] 18. Plugin targets (`Luthier_VST3`, `Luthier_Standalone`) build clean.

## Done

- [x] 2. `GuitarCircuit` replacing `CableSim`; 11 circuit params in the
      `circuit` family; CIRCUIT panel with live response view and standard-value
      dropdowns; coil resonance moved out of `PickupEngine`.
- [x] 2b. Advanced amp ranges audible (AmpEngine accepts the advanced span).

- [x] 1. Advanced ranges UI: Options RANGES page, warning arc and `*`,
      header padlock, right-click unlock/restrict, locked-edge notice,
      first-unlock explainer, randomise-in-stock, control resync on range swap.

- [x] `PhysicalRange`, `RangeRegistry`, `RangeState` (commit 2ef230b).
- [x] `RangeState` wired into preset save and load, ranges block applied
      before parameter values; harness fixed; round-trip test added.
