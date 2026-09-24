## animated-strings.md

No part of string animation exists on this checkout: no `SoundingNotes`, `StringMotion`, `StringAnimator`, `stringLooks`, no VISUAL AIDS options and no `AnimatedStringsTests.cpp`. The only related code is the static layer-26 glow driven by `GuitarBodyComponent::timerCallback` (level x4 normaliser) and the fretboard "excitement" stroke, both reading the racy `LuthierEngine::getStringLevel/getStringFret` doubles. `origin/claude/luthier-visual` carries a `Source/Support/SoundingNotes.h` (piano-roll version: note/bend/start only, double-buffered) that this spec must extend. No FEAT branch pushed yet; all rows OWNED.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| AS-0 (§0) | Off by default, amplitude from engine follower, swept-envelope ghost, zero audio cost, dirty-rect only — not built | - | - | - | OWNED |
| AS-2.1 (§2.1) | Per-string geometry: stop point from published stopFret (capo/fret/tap/slide contact), bridge point, static segment behind stop — not built | - (`DSP/Slide/SlideEngine.h:102 contactFret` exists) | - | - | OWNED |
| AS-2.2 (§2.2) | Envelope A(u,t) with K-mode pluck shape relaxing, harmonic nodes, `L_n=clamp(level*4)` moved to `StringMotion::normaliseLevel`, `A_max`=0.4 spacing, 0.5 px floor — not built (x4 normaliser inline in `UI/GuitarBodyComponent.cpp:131`) | - | - | - | OWNED |
| AS-2.3 (§2.3) | Motion-ghost drawing table High/Low, replaces layer-26 glow, high-contrast Low style, fretboard via `stringLooks`, 12-string per-index — not built | - | - | - | OWNED |
| AS-2.4 (§2.4) | Bend lateral displacement d=spacing*sqrt(cents/200), direction by half, finger-bend only (no whammy/slide/drift), vibrato sampled — not built | - | - | - | OWNED |
| AS-2.5 (§2.5) | Damping mask/decay per `StringEngine::Damping` (LightTouch x0.5, PalmMute pin, Choked/Chuck 25 ms visual decay) — not built | - (`DSP/String/StringEngine.h:48,100 Damping/getDamping` exist) | - | - | OWNED |
| AS-2.6 (§2.6) | Gating (pref, reduced motion, showing, relief<2, stale), VBlank throttled 60 Hz / 30 Hz timer fallback, 250 ms stale ease-to-rest, relief hook — not built | - | - | - | OWNED |
| AS-3 (§3) | Surfaces: Easy + Advanced illustrations and Advanced fretboard animate; Workshop bench and thumbnails never — not built | - | - | - | OWNED |
| AS-4.1 (§4.1) | `Support/SoundingNotes.h` per-string record (level, stopFret, bendCents, pluckPosition, start, note, damping, harmonicPartial, stopKind), seqlock, `LuthierEngine::publishSoundingNotes` at end of `processSubBlock`, `getHarmonicPartial`, getters become wrappers (closes data race) — absent here; on visual: `Support/SoundingNotes.h` with note/bend/start only | - | n/a | - | OWNED |
| AS-4.2 (§4.2) | `UI/Guitar/StringMotion.h/.cpp` pure model, fixed arrays, dirty rects — not built | - | n/a | - | OWNED |
| AS-4.3 (§4.3) | `UI/Guitar/StringAnimator.h/.cpp` frame driver + test hooks — not built | - | n/a | - | OWNED |
| AS-4.4 (§4.4) | Renderer: `PaintLayers::omitSpeakingLengths`, `paintSpeakingLengths` layer 25a, `GuitarOverlay::motionActive`, clip-skipping, fretboard equivalent, GPU-friendly paths — not built | - | - | - | OWNED |
| AS-5 (§5) | Options > APPEARANCE > VISUAL AIDS: `animateStringsToggle` (off), `animateQualityBox` (High), `animateStatusLabel`; `UiPreferences` keys, immediate save — absent from `UI/OptionsPages.h:104 AppearancePage` | - | - | - | OWNED |
| AS-6 (§6, §6.1) | No parameters/state; two `ui.json` keys; `GuitarRenderer::stringLooks` extracted from `build` — not built | - | n/a | - | OWNED |
| AS-7 (§7) | Nothing undoable — trivially true but feature absent | - | n/a | - | OWNED |
| AS-8 (§8) | A11y: tab order, names, "Toggle string animation" rebindable unbound shortcut, reduced motion wins, catalog keys — not built | - | - | - | OWNED |
| AS-9 (§9) | Both editions identical — not built | - | n/a | - | OWNED |
| AS-10 (§10) | Interactions table (slide, tap, chord name, overlays, harmonics, muting, rhythm/tune/piano-roll sources, sympathetic, capo, family switch, multi-instance) — not built | - | - | - | OWNED |
| AS-11 (§11) | Perf: 0 audio units, frame median <1 ms / p99 <2 ms, dirty union <=25%, idle free, zero alloc, fps caps — not built | - | n/a | - | OWNED |
| AS-12 (§12) | Failure modes (corrupt ui.json, no vblank, seqlock retry, stale, NaN clamp, empty scene, over-budget drop to Low + log) — not built | - | n/a | - | OWNED |
| AS-T1 (§13 AS-01..11) | Model tests: default off, proportional amplitude, floor, nodes, pluck relax, harmonics, bend, whammy no-push, muting, slide, tap — none | - | - | - | OWNED |
| AS-T2 (§13 AS-12..22) | Render/driver tests: dirty-rect correctness, no full repaints, idle, frame budget, no alloc, audio bit-identical, RT-safe publish, reduced motion, hidden editor, stale, fps caps — none | - | - | - | OWNED |
| AS-T3 (§13 AS-23..30) | Look/UI/persistence/modes/sources/relief/edition tests — none | - | - | - | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=23 -->
