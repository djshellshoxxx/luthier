# Luthier — full spec index

Companion documents to `spec.md`, `engine.md`, `include.md` and `theme.md`.

Five groups:

- The eleven original extension specs (routing through telemetry).
- The twelve realism specs (advanced-ranges through midi-export), which
  close the gap that made a modelled guitar sound and behave unlike a
  real one, and which make every part of the guitar a thing the user
  can change on a bench.
- The composition spec (tune-builder), which turns Luthier into a
  sketchpad for actual tunes and melodies, not just an instrument.
- The build-integration specs (gui-integration, ui-wiring,
  ambiguity-resolutions, qa-polish, onboarding, performance-budget,
  installer, CLAUDE_CODE_BRIEF and this file), which turn everything
  above into one $200 product.
- The deep-integration specs (file-formats, factory-content,
  error-recovery, state-model, gui-engine-dataflow,
  guitar-illustration, input-routing, host-integration,
  action-and-undo), which name every file schema, every shipped file,
  every failure response, every intersection between concurrent
  activities, every visual data flow, every part of the illustration,
  every input consumer, every host quirk, and every undo rule.

Read `CLAUDE_CODE_BRIEF.md` first if you are the implementer.

## Order to build in

### Phase 1: original extensions
1. `routing-io.md`
2. `modulation-matrix.md`
3. `rhythm-engine.md`
4. `live-performance.md`
5. `controllers.md`
6. `practice-tools.md`
7. `tone-match.md`
8. `notation-export.md`
9. `character-wear.md`
10. `accessibility.md`
11. `updates-telemetry.md`

### Phase 2: realism
12. `advanced-ranges.md` (mechanism every later file depends on)
13. `volume-knob-interaction.md` (replaces CableSim; introduces GuitarCircuit)
14. `pick-noise.md`
15. `string-squeak.md`
16. `fret-buzz.md`
17. `slide-guitar.md`
18. `guitar-workshop.md` (parts data model; factory guitars become part files)
19. `part-acoustics.md` (every part's effect on the engine)
20. `workshop-ui.md`
21. `strum-dynamics.md`
22. `bass-techniques.md`
23. `midi-export.md` (last of the realism phase, captures every event class above)

### Phase 3: composition
24. `tune-builder.md` (writes into rhythm engine and note engine, exports through midi-export)

### Phase 4: gap-fills
25. `ambiguity-resolutions.md`
26. `gui-integration.md`
27. `ui-wiring.md`
28. `onboarding.md`
29. `performance-budget.md`
30. `qa-polish.md`
31. `installer.md`
32. `CLAUDE_CODE_BRIEF.md`

### Phase 5: deep-integration
33. `file-formats.md` (canonical schemas for every .luthier* file)
34. `factory-content.md` (every shipped preset, guitar, part, tune, pattern, kit, IR)
35. `error-recovery.md` (every failure mode and its response)
36. `state-model.md` (state layer nesting; intersection matrix for concurrent activities)
37. `gui-engine-dataflow.md` (per live UI element: source, drain rate, staleness rule)
38. `guitar-illustration.md` (deep spec: bodies, necks, bridges, pickups, strings, colours, finishes, family switching)
39. `input-routing.md` (MIDI, mouse, keyboard, file-drop, host transport routing order)
40. `host-integration.md` (VST3/AU/CLAP/AAX contract; per-host quirks)
41. `action-and-undo.md` (undo entry taxonomy, grouping window, state boundaries)

### Phase 6: ship
Run `qa-polish.md` section 8 (bug bash) and section 12 (final human
check). Neither is skippable.

## What each file adds

| File | Adds |
|---|---|
| `REVIEW.md` | Observations on the existing specs and where each new file plugs in |
| `routing-io.md` | Multi-out bus layouts, sidechain, per-string outputs, MIDI out, re-amp, Aux 8 noise bus |
| `modulation-matrix.md` | LFOs, EGs, step sequencers, envelope followers, macros, routing table |
| `rhythm-engine.md` | Chord detector, voicer, strum and fingerpick pattern engines, genre kits |
| `live-performance.md` | Snapshot banks, morph, setlist, tap tempo, kill switch, monitor mix, expression pedal calibration |
| `controllers.md` | Profiles for MPE, GK, TriplePlay, Jamstik, Osmose, latency compensation |
| `practice-tools.md` | Metronome, looper, backing track player, scale trainer, ear training, tab reader, session recorder |
| `tone-match.md` | User IR loading, cab match, EQ match, capture utility |
| `notation-export.md` | Live TAB view, MusicXML, Guitar Pro, ASCII tab, MIDI export |
| `character-wear.md` | Dead spots, fret wear, tuner drift, aged electronics, body break-in, environment |
| `accessibility.md` | Screen reader, keyboard-only, colourblind palettes, UI scale, localization |
| `updates-telemetry.md` | Update checks, opt-in telemetry, crash reporting, license activation, privacy dashboard |
| `advanced-ranges.md` | Stock vs advanced parameter ranges, per-preset opt-in, marking, clamping |
| `volume-knob-interaction.md` | GuitarCircuit: pots, caps, treble bleed, cable / amp-input loading; amp cleanup |
| `pick-noise.md` | Pick material / thickness / tip / bevel / angle / wear, click, chirp, scrape, fingerstyle |
| `string-squeak.md` | Finger-slide squeak on wound strings, per-material spectra, pickup routing, style presets |
| `fret-buzz.md` | Action, relief, nut depth, fret height, live buzz, setup styles |
| `slide-guitar.md` | Bottleneck / lap steel / dobro / hybrid |
| `guitar-workshop.md` | Bill-of-parts `GuitarSpec`, parts library |
| `part-acoustics.md` | Every part-field-to-engine-parameter mapping |
| `workshop-ui.md` | The bench: live-drawn guitar with hit-tested parts, inspector, spectrum delta, audition, A/B |
| `strum-dynamics.md` | Crossing velocity, acceleration profile, strikers, chucks |
| `bass-techniques.md` | Slap, pop, ghosts, double thump, fingerstyle, bass-specific defaults everywhere |
| `midi-export.md` | Luthier and Generic MIDI profile export / import for every event class, live MIDI-out alignment |
| `tune-builder.md` | Chord progression + melody + rhythm workflow; `.luthiertune` file; three-minute tune loop |
| `ambiguity-resolutions.md` | Feedback, freeze / E-Bow, doubler, chord auto-fingering, preset morph, strum-velocity source, .luthierguitar compatibility |
| `gui-integration.md` | Master GUI spec: window, Easy / Advanced, every feature's UI location, Workshop / Slide / Tune integration |
| `ui-wiring.md` | Attachment pattern, threading, undo / redo, MIDI Learn, parts-swap and shadow-spec patterns |
| `onboarding.md` | First-run, tour, sample content, Workshop / Slide / Tune discovery, advanced-range prompt |
| `performance-budget.md` | Per-module CPU / memory budgets |
| `qa-polish.md` | Ship gate: test matrix, crash policy, UI polish, audio polish, bug bash |
| `installer.md` | Windows / macOS / Linux install, uninstall, updates, portable, enterprise |
| `CLAUDE_CODE_BRIEF.md` | Front-door prompt: reading order, audit-then-build, conflict resolution, definition of done |
| `file-formats.md` | Canonical JSON schemas for every .luthier* file, migration rules, save atomicity, load-error handling |
| `factory-content.md` | Every preset, guitar, part, tune, pattern, kit, setlist, backing track, IR that ships |
| `error-recovery.md` | Every failure mode and its named response; banner priority; error log format |
| `state-model.md` | State layer hierarchy; load flows per layer; concurrent-activity intersection matrix |
| `gui-engine-dataflow.md` | For every live UI element: source, audio schedule, UI drain rate, stale threshold, stale state |
| `guitar-illustration.md` | Deep spec: coord system, rendering pipeline, body catalogue per family, necks/bridges/pickups/pickguards, string materials with hex colours, colours and finishes catalogue, family switching, workshop hit-tests |
| `input-routing.md` | Consumer chains for MIDI, mouse, keyboard, file drops, host transport, sidechain audio |
| `host-integration.md` | Format matrix, bus layouts, parameter model, state serialization, latency, per-host quirks (Ableton, Logic, Cubase, Studio One, Reaper, FL, Bitwig, Pro Tools, Standalone) |
| `action-and-undo.md` | Undo entry taxonomy per action class; grouping window; state boundaries; what skips the stack |

## Global rules

The rules from `engine.md` section 0 hold everywhere: double-precision
DSP, no allocations in the audio callback, DC blockers, NaN guards,
denormals off, sample-rate independence, seconds/Hz not samples, reset()
on every module, module isolation, oversampling for nonlinear stages.

Three further rules for realism:
- **Physical deltas only.** A part or setup change lands as a physical
  parameter change (`part-acoustics.md`), never as a downstream EQ.
- **Real ranges by default.** Every physical parameter declares stock
  and advanced ranges (`advanced-ranges.md`).
- **Honest magnitudes.** Small real effects are small, and the Workshop
  shows them as small.

Three rules for the composition layer:
- **Tune Builder is a MIDI writer.** It never touches the audio path.
- **Every event is undoable.** Regeneration preserves locked notes.
- **Every output is exportable.** Audio, MIDI, notation, project.

Three rules for integration:
- `gui-integration.md` is the single source of truth for UI location.
- `ui-wiring.md` is the single source of truth for backend attachment.
- `qa-polish.md` is the ship gate.

Every new file has its own "Tests" section. Add those tests to the
existing `LuthierTests` target.
