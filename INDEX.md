# Luthier — full spec index

Companion documents to `spec.md`, `engine.md`, `include.md` and `theme.md`.
Each file extends a specific area of the plugin without contradicting what
is already built. Written to match the voice and rigour of `engine.md` so
Claude Code cannot misinterpret them.

Read `CLAUDE_CODE_BRIEF.md` first if you are the implementer. It tells you
how to use this set as a coherent whole rather than a pile of documents.

## Order to build in

### Phase 1: foundational extensions (already tracked in PROGRESS.md)
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

### Phase 2: gap-filling and ship-ready
12. `ambiguity-resolutions.md` — resolves the five underspecified items REVIEW.md flagged
13. `gui-integration.md` — MASTER map: every backend feature to a concrete UI location
14. `ui-wiring.md` — how UI attaches to backend, threading contract, undo/redo
15. `onboarding.md` — first-run and returning-user experience
16. `performance-budget.md` — per-module CPU and memory budgets, enforcement
17. `qa-polish.md` — the ship gate; what "consumer-ready" means as a checklist
18. `installer.md` — Windows/macOS/Linux install, uninstall, update, enterprise

### Phase 3: polish pass
Once every spec above is implemented, do the polish pass documented in
`qa-polish.md` section 8 (bug bash) and section 12 (the final human
check).

## What each file adds

| File | Adds |
|---|---|
| `REVIEW.md` | Observations on the existing specs and where each new file plugs in |
| `routing-io.md` | Multi-out bus layouts, sidechain, per-string outputs, MIDI out, re-amp |
| `modulation-matrix.md` | LFOs, EGs, step sequencers, envelope followers, macros, routing table |
| `rhythm-engine.md` | Chord detector, voicer, strum and fingerpick pattern engines, genre kits |
| `live-performance.md` | Snapshot banks, morph, setlist, tap tempo, kill switch, monitor mix, expression pedal calibration |
| `controllers.md` | Profiles for MPE, GK, TriplePlay, Jamstik, Osmose, plus latency compensation |
| `practice-tools.md` | Metronome, looper, backing track player, scale trainer, ear training, tab reader, session recorder |
| `tone-match.md` | User IR loading, cab match, EQ match, capture utility |
| `notation-export.md` | Live TAB view, MusicXML, Guitar Pro, ASCII tab and MIDI export |
| `character-wear.md` | Dead spots, fret wear, tuner drift, aged electronics, body break-in, environment |
| `accessibility.md` | Screen reader, keyboard-only, colourblind palettes, UI scale, localization |
| `updates-telemetry.md` | Update checks, opt-in telemetry, crash reporting, license activation, privacy dashboard |
| `ambiguity-resolutions.md` | Resolves feedback, freeze/E-Bow, doubler, chord auto-fingering rubric, preset morph |
| `gui-integration.md` | Master GUI spec: window structure, Easy/Advanced layout, every feature's UI location |
| `ui-wiring.md` | Component-to-backend attachment pattern, threading, undo/redo, MIDI Learn plumbing |
| `onboarding.md` | First-run experience, tour, sample content, discoverability |
| `performance-budget.md` | Per-module CPU / memory budgets, latency, boot time, CPU relief |
| `qa-polish.md` | Ship gate: test matrix, crash policy, UI polish checklist, audio polish checklist, bug bash procedure |
| `installer.md` | Installer, uninstall, update delivery, portable install, enterprise deployment |

## Global rules that apply across every new file

The rules from `engine.md` section 0 still hold everywhere: double-precision
DSP, no allocations in the audio callback, DC blockers, NaN guards,
denormals off, sample-rate independence, seconds/Hz not samples, reset() on
every module, module isolation, oversampling for nonlinear stages.

Anywhere a new module adds an audio-path stage, it must obey those rules.
Anywhere a new module is control-only (rhythm engine, mod matrix control
rate, notation export, practice tools UI), it must not touch the audio
thread's DSP path.

`gui-integration.md` is the single source of truth for where every feature
lives in the UI. If any other spec disagrees, gui-integration wins.

`ui-wiring.md` is the single source of truth for how UI attaches to
backend. If any other spec disagrees on threading, attachment, or undo,
ui-wiring wins.

`qa-polish.md` is the ship gate. A feature marked complete in PROGRESS.md
is not complete until it passes the checks in qa-polish.md for its area.

Every new file has its own "Tests" section. Add those tests to the
existing `LuthierTests` target.
