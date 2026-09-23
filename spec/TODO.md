# TODO

Working list for the autonomous run (`PROMPT.md`). Order follows
`INDEX.md` "Build order for the engine work", then `CLAUDE_CODE_BRIEF.md`
steps 6-12. `GAPS.md` has the detail behind each row; `DECISIONS.md` has
every judgement call made along the way.

Build: `cmake --build build --config Release --target LuthierTests -- -v:m -p:CL_MPCount=1`,
one target at a time, foreground. Tests: `build/LuthierTests_artefacts/Release/LuthierTests.exe [filter...]`.

## In progress

- [ ] **G. Realistic guitar illustration** - IN PROGRESS. Built and green
      (432 tests): `GuitarRenderer` draws every factory guitar from its parts
      (body outline data, finishes with grain/burst/aging/sparkle, lighting
      per visual-polish 1, bridges, tailpieces, pickups, pickguards, neck,
      fretboard, inlays, frets, nut, headstock, tuners, strings by material),
      hit regions with section 16 descriptions, overlays (played notes, slide
      bar, hover); `GuitarBodyComponent` now uses it (Easy and Advanced).
      Renders: `%TEMP%/luthier-guitar-renders/`. Remaining: headstock shapes
      refined (assistant, `HeadstockOutlines.h`), body refinements
      (assistant), sharktooth inlays too large, family switching (12), zoom /
      pan (1), preset-browser thumbnails on a worker thread with a 200-entry
      cache (15), per-string material override (10), capo drawing, reduced
      motion crossfade rules (16), 60 ms note-dot timing test (19).
- [ ] **C. `docs/spec-coverage.md`** (CLAUDE.md workflow) - IN PROGRESS, being
      built by an assistant agent: one row per actionable requirement across
      every `spec/*.md`, with location, verification and status. Keep it
      current after each step once it lands.
- [ ] 6e. Part swaps that keep the string count move to an off-thread build
      and block-boundary swap with step 7 (DECISIONS C-09). Notes arriving
      while parked are now queued (done, `WorkshopSwap` test).
- [ ] 5b. `slide-technique-controls.md`: position source (modwheel / bend /
      MPE Y / expression / CC / drag), absolute/relative, slant & pressure
      sources, contact string mask, speed limit, auto-vibrato on hold,
      scripted SlideGesture, presets; Techniques tab Slide sub-tab.
- [ ] 3f. A way to fire pick scrape (midi-export `pick_scrape` class / Easy rake
      gesture); the engine API `triggerPickScrape` exists.

## Remaining

- [ ] **G. Realistic guitar illustration (user request, 2026-09-23; high
      priority).** The current `GuitarBodyComponent` drawing does not read as a
      real guitar. Rebuild it to `spec/guitar-illustration.md`: per-model body
      outlines for every family (4: electric, acoustic, classical, bass,
      resonator) drawn from real proportions, the layered z-order (5), necks
      and headstocks with tuners (6), bridges and tailpieces (7), pickups with
      covers and pole pieces (8), pickguards (9), strings by material and gauge
      (10), finishes - solid, burst, transparent grain, natural, metallic,
      sparkle, relic (11) and hardware colour. Static geometry cached, live
      overlays per frame (2); family switching (12); keep the existing hit
      regions working (13). Drive it from the current `WorkshopGuitar` so the
      picture is the parts guitar that is playing. Check it by rendering to
      PNG in a test and looking at the result for each factory guitar.
      Started: `Source/UI/Guitar/GuitarRenderer.h` (API only: GuitarScene in
      saddle-origin mm, GuitarOverlay, build/fitTransform/paint/hitTest/
      render). The .cpp is not written. Plan: body outlines as per-style
      point lists in neck-pocket mm, Catmull-Rom smoothed; pocket X from the
      neck-joint fret; headstock layout inferred from neck joint/family/
      string count; part `illustration` hints override inference (spec 18).
- [ ] **V. Visual appeal (user request, 2026-09-23; after G).** Done: the
      Luthier guitar-shop theme (visual-polish 6) - rosewood/walnut/Tolex
      palette, maple-and-cream Light palette, black bell knobs with cream
      pointers, mini toggles, brass fader caps, walnut panels with corner
      screws, engraved brass section plates, brass headstock brand mark,
      Lato + Bebas Neue shipped in `Resources/Fonts` (OFL); palettes now
      actually reach the UI (they did not before) and switch live via
      `Palette::remap`; `Theme` tests. Remaining: header plugin name in the
      display face, Options -> Appearance accent choices + follow-the-guitar
      (5), amp and pedal faces (2) with model knob caps (3), tube glow / VU
      meter / room light (4), live overlays polish on the guitar (G 14),
      preset-browser thumbnails (G 15), screenshots of every panel in all
      three palettes reviewed by eye.

- [ ] 2e. **Easy Mode layout per gui-integration.md 3**: the build has the
      older three-band layout. Missing: the 280 px RIG STRIP (guitar circuit
      compact card with `CircuitResponseView` miniature, compact pre/post racks
      with popover slots, amp card, cab card, room card), the PLAYING strip
      (mode, humanize, character macro, whammy display), the TONE strip (input,
      output, wet/dry, width), rhythm strip dice + chord/next-strum readout.
- [ ] 2f. **Aux 8 noise bus** (pick-noise.md 1.3; routing-io.md lists only 7
      aux). Must be appended *after* the 12 per-string buses so existing bus
      indices (sessions using layouts C/D) do not shift; update RoutingMatrix /
      PluginProcessor bus arithmetic, routing-io.md table, latency (128-sample
      allowance). Engine already fills `LuthierEngine::getNoiseBusData()`.
- [ ] 2c. Feedback per `ambiguity-resolutions.md` 1 (mic-to-speaker loop,
      feedback_amount/distance/angle/focus/octave_bias params, post-circuit
      path, Adv Col 3 SUSTAIN feedback row). Currently a heuristic.
- [ ] 2d. Audit the rest of `ambiguity-resolutions.md` against the build
      (brief step 4).
- [ ] 7. `WorkshopPanel` and the WORKSHOP tab (`workshop-ui.md`), with its
      tab-header padlock.
- [ ] 8. `StrumGesture` (RHYTHM STRUM group), then `BassTechniques` (SLAP
      group, bass step grid).
- [ ] 9. `PerformanceCapture`, then the NOTATION tab.
- [ ] 10. MIDI export profiles (`midi-export.md`), then the MIDI OUT tab.
- [ ] 11. `PracticeRoutines` and the PRACTICE tab.
- [ ] 12. Tune Builder (`tune-builder.md`) and the TUNE tab.
- [ ] 13. HELP tab (column 4).
- [ ] 13b. **Phase 5b technique specs (added 2026-09-23)**, in INDEX order:
      `string-scraping.md` (ScrapeEngine), `string-slap-technique.md`,
      `muting-rhythm.md`, `two-hand-tapping.md`, `microtonal-bends.md`,
      `technique-cascade.md`, `gui-techniques-updates.md` (Techniques tab,
      Playing strip pills, fretboard overlays), `engine-technique-layer.md`.
- [ ] 13c. **BLOCKED: phase 2b specs not on disk** - `string-aging.md`,
      `environment.md`, `body-coupling.md`, `harmonic-realism.md`,
      `string-interaction.md`, `fingerstyle-attack.md`, `noise-floor.md`,
      `sustain-and-decay.md`, `tuning-stability.md`. Listed in INDEX and the
      brief (2026-09-23) but the files do not exist. Asked the user.
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

- [x] 6. Workshop parts: part model + library, 148 factory parts and 27
      guitars, mapSpec, processor loader, preset `guitar.reference` /
      `override`, Save As Guitar / Save As Part, Ctrl+G and Ctrl+Shift+E,
      click-free swap, and the partial capo (TuningEngine string mask, capo in
      the preset guitar block). 415 tests green.

- [x] 5. Slide Mode: SlideEngine, header toggle and `S` shortcut, CHARACTER
      SLIDE group with the low-action warning, fretboard bar overlay.

- [x] 4. Setup geometry, sensed fret buzz, sitar mode, SETUP group with the
      live heatmap and setup styles. (Character fret wear moving buzz,
      fret-buzz.md 8, waits on a per-fret wear height from CharacterEngine.)

- [x] 3. NoiseEngine pool; pick click/chirp/scrape; finger squeak; CHARACTER
      tab STRING NOISE and PICK groups with style presets, noise-event strip
      and the tab padlock. (Aux 8 is 2f; scrape trigger is 3f.)

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
