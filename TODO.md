# TODO

Working list for the autonomous run (`PROMPT.md`). Order follows
`INDEX.md` "Build order for the engine work", then `CLAUDE_CODE_BRIEF.md`
steps 6-12. `GAPS.md` has the detail behind each row; `DECISIONS.md` has
every judgement call made along the way.

Build: `cmake --build build --config Release --target LuthierTests -- -v:m -p:CL_MPCount=1`,
one target at a time, foreground. Tests: `build/LuthierTests_artefacts/Release/LuthierTests.exe [filter...]`.

## In progress

- [ ] **1. Advanced ranges UI** (`advanced-ranges.md` 5-7): Options RANGES
      page, warning-colour arc and `*` readout, header padlock, right-click
      unlock/restrict items, clamp notification (gui-integration 15),
      first-unlock explainer (onboarding.md).

## Remaining

- [ ] 2. `GuitarCircuit` replacing `CableSim` (`volume-knob-interaction.md`),
      and the Adv Col 2 CIRCUIT panel replacing CABLE.
- [ ] 3. `NoiseEngine` pool, then `PickModel`, `SqueakModel`, buzz sensing
      (`pick-noise.md`, `string-squeak.md`, `fret-buzz.md`); CHARACTER PICK /
      STRING NOISE groups; noise-event strip; Aux 8 noise bus row.
- [ ] 4. `SetupGeometry` (`fret-buzz.md`).
- [ ] 5. `SlideEngine` (`slide-guitar.md`), Slide mode.
- [ ] 6. `PartLibrary` and `mapSpec` (`guitar-workshop.md`, `part-acoustics.md`),
      `.luthierguitar` / `.luthierpart` files, partial capo.
- [ ] 7. `WorkshopPanel` and the WORKSHOP tab (`workshop-ui.md`).
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
- [ ] 15. Polish pass, performance pass, onboarding pass (brief steps 7-9).
- [ ] 16. Installer pass (brief step 10). The platform matrix cannot be run
      from this Windows-only machine; see DECISIONS.md when reached.
- [ ] 17. Bug bash and final check (brief steps 11-12).
- [ ] 18. Plugin targets (`Luthier_VST3`, `Luthier_Standalone`) build clean.

## Done

- [x] `PhysicalRange`, `RangeRegistry`, `RangeState` (commit 2ef230b).
- [x] `RangeState` wired into preset save and load, ranges block applied
      before parameter values; harness fixed; round-trip test added.
