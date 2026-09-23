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
      (assistant), family-switch UI in the Workshop drawer (mechanics done:
      `PartLibrary::switchFamily`, `switchGuitarFamily`; amp defaults per
      family, 12.3, not yet), zoom /
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

- [ ] 2h. Easy rig strip polish: the amp card's knobs are cramped at 1200x720;
      give the amp card more height or two knob sizes.
- [ ] 2c. Feedback per `ambiguity-resolutions.md` 1 (mic-to-speaker loop,
      feedback_amount/distance/angle/focus/octave_bias params, post-circuit
      path, Adv Col 3 SUSTAIN feedback row). Currently a heuristic.
- [ ] 2d. Audit the rest of `ambiguity-resolutions.md` against the build
      (brief step 4).
- [ ] **7. Workshop bench** - IN PROGRESS. Done and green (457 tests):
      7a model (`WorkshopBench`, `SpectrumDelta`, live pickup moves); 7b
      `WorkshopPanel` - header with name/modified/Save As Guitar/A-H slots,
      `BenchIllustration` (hover names, click selects, pickup drags with
      snap and limits, saddle drags, scroll for height, Ctrl-scroll zoom,
      pan, ruler with pickup rail, Tab/arrow keyboard parity), inspector,
      13-category parts drawer (fit on click, Alt-hover audition, user
      section empty state, slide needs Slide Mode), setup strip, spectrum
      pane (+-12 dB / auto-zoom, comb notches, summary sentence); 7c WORKSHOP
      is column 4's first tab and takes over columns 3+4; the header's
      Workshop button opens it (Advanced) or the Easy overlay.
      Also done: the Guitar (family) category with the once-per-session
      confirm, and double-click field editing in the inspector.
      Remaining: per-string string overrides (3.3), nut
      slot drag on the nut, pick/slide/capo overlays and drags on the bench
      (4), WORKSHOP tab padlock when part fields gain
      ranges, an editor-level test of the wrench in Easy mode, spectrum
      summary announced to screen readers, slide material in the engine
      (5b). Known issue: the first note after a body/cab IR load renders
      slightly differently (~0.02 peak at the onset); the spectrum fixture
      primes around it.
- [ ] 8. `StrumGesture` (RHYTHM STRUM group), then `BassTechniques` (SLAP
      group, bass step grid).
- [ ] 9. `PerformanceCapture`, then the NOTATION tab.
- [ ] 10. MIDI export profiles (`midi-export.md`), then the MIDI OUT tab.
      Model done and green (`Source/Export`, `MidiExportTests`: per-string
      export, Luthier / generic profiles, live MIDI out, round-trip null of
      every factory preset at <= -60 dBFS after the reset-determinism fixes,
      DECISIONS). MIDI OUT tab done (`MidiOutPanel`, `MidiOutPanelTests`):
      profile editor = Options -> MIDI defaults (8) with .midprofile save /
      load (7), export of the retrospective capture (entire / last N s) with
      preview (4.1), drag-out with Alt for Generic (4.2), live sources shared
      with ROUTING (6) incl. EVENTS (noise triggers as Luthier SysEx on their
      sample) and WORKSHOP (part fits); the header's "Save last MIDI take"
      uses the defaults. Remaining: TUNE source (needs tune playback, 12),
      CHARACTER seed / environment events, import UI (5: File -> Import,
      drop a .mid, target choice), marked-region / current-section ranges,
      drag from the session recorder's own Save button (practice drawer).
      Residual: a brand-new engine's
      first 7-string render after a 6-string one differs from the next by
      ~1e-4 peak (about -80 dB) from ~27 ms in; under the bar, not yet found.
- [ ] 11. `PracticeRoutines` and the PRACTICE tab.
- [ ] 12. Tune Builder (`tune-builder.md`) and the TUNE tab. Model done
      (`Source/Tune`, `TuneBuilderTests`, d284547). Remaining: the TUNE tab.
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

- [x] 2g. Trademark sweep: guitar types, amps, speakers, mics, bridge types,
      bleed and slide names, factory presets and their descriptions, genre
      kits, tooltips, 14 factory parts and 2 guitars renamed to reference-style
      names; old part and guitar names still resolve; `Tools/trademark_scan.py`
      and the `Trademarks` tests keep them out.

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
