# REVIEW: existing Luthier specs

## What the four spec files cover well

**spec.md** — top-level product brief. Body/pickup/string models, guitar types,
tuning, MIDI modes, GUI layout, presets, humanization, deliverables, ship
criteria. Reads like a PRD.

**engine.md** — the DSP contract. Ground rules (double precision, no allocs in
processBlock, DC blockers, NaN guards, denormals off, sample-rate independence),
per-module algorithms, coupling matrix, amp modelling, oversampling policy.
Reads like an engineering spec Claude Code cannot misinterpret.

**include.md** — mandatory VST features (help, preset browser, right-click,
tooltips, randomize, debug, easter egg, custom icon, drag-and-drop, save/open).
Cross-project boilerplate.

**theme.md** — visual identity. Palette, typography, knob/slider/meter shapes,
layout grid, header, signature notch, output LED, data-stream animation.

## Gaps and underspecified areas

1. **No rhythm/pattern engine.** spec.md mentions "strum simulation" in Poly
   mode and "practice tools: metronome, chord progression looper, backing track
   player" but neither has algorithms, UI, or parameter definitions. See
   `rhythm-engine.md` and `practice-tools.md`.
2. **No live-performance layer.** No snapshot banks, no morph, no MIDI program
   change routing, no tap tempo, no monitor mix, no kill-switch, no expression
   pedal calibration. See `live-performance.md`.
3. **No modulation matrix.** LFOs and envelopes exist inside effects but there
   is no user-facing modulation source that can route to any parameter, nor
   step sequencers or macro morph curves. See `modulation-matrix.md`.
4. **IO is stereo-out only.** No separate dry/wet/DI/per-string outputs, no
   sidechain input, no MIDI out. Re-amping and stem workflows are not possible.
   See `routing-io.md`.
5. **No user IR loading or tone matching.** The README notes any WAV loads,
   but there is no UI, no folder convention, no EQ/cab-match tool, no capture
   workflow. See `tone-match.md`.
6. **No notation output.** spec.md mentions "tab display, exportable" in one
   line. No format, no schema, no editor. See `notation-export.md`.
7. **Controller integration named but not specified.** Roland GK, Fishman
   TriplePlay, Jamstik, MPE controllers get one sentence each. Per-source
   latency compensation, calibration, and hex-channel routing need their own
   document. See `controllers.md`.
8. **Character/aging is one string-age selector.** No dead spots, no worn
   frets, no loose tuner drift, no capacitor aging. The realism story stops
   short of what real guitars actually do over time. See `character-wear.md`.
9. **No accessibility or localization spec.** No screen reader story, no
   colourblind palette, no keyboard-only navigation, no i18n string catalog,
   no RTL. See `accessibility.md`.
10. **Update, telemetry and crash reporting undefined.** include.md's debug
    section covers on-device diagnostics but not delivery back to support, nor
    auto-update. See `updates-telemetry.md`.

## Ambiguities in the existing specs, worth resolving

- **"Chord mode auto-fingering"** in spec.md is not defined algorithmically.
  engine.md's ChordVoicer references it but the constraint set (hand span,
  playability score, barre-vs-open preference weights, capo state) needs an
  explicit rubric. Add to `rhythm-engine.md` or a dedicated voicing addendum.
- **"Feedback simulation"** in spec.md is one line. Whether it is modelled as
  a physical mic-to-speaker loop or a heuristic overlay is left to Claude
  Code. Pick one.
- **"Freeze / sustain infinite"** is undefined. E-Bow simulation via feedback
  loop, or a captured-loop overlay? Pick one.
- **"Doubler"** has no timing/pitch/pan defaults.
- **Preset morph** is not mentioned at all yet A/B compare is. Users will
  expect at least a smooth transition; specify or explicitly rule out.

## Recommended ordering

Once the current build is at feature parity with spec.md/engine.md/include.md
(PROGRESS.md says it is), the new specs are additive. Suggested milestone
order for the additions:

1. `routing-io.md` — foundational, blocks nothing else
2. `modulation-matrix.md` — depended on by rhythm, live-perf, controllers
3. `rhythm-engine.md`
4. `live-performance.md`
5. `controllers.md`
6. `practice-tools.md`
7. `tone-match.md`
8. `notation-export.md`
9. `character-wear.md`
10. `accessibility.md`
11. `updates-telemetry.md`
