# FEAT-STRINGS: animated strings coverage

Spec: `spec/animated-strings.md` (AS-01 to AS-30). Branch: `claude/luthier-feat-strings`.
Tests: `Source/Tests/AnimatedStringsTests.cpp`, suite `AnimatedStrings`
(`xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests AnimatedStrings`).

Parameters added: **none** (spec 6: display only, not automatable, not in the APVTS).
The parameter-count sum in IntegrationTests.cpp is unchanged.

## Coverage

| ID | Spec section | Implementation | Verification | Status |
|---|---|---|---|---|
| R-0.1 | 0.1, 5 | `StringAnimationSettings::isEnabled` defaults to false (`UiPreferences` key `animateStrings`); with it off, `GuitarBodyComponent` uses the full cache and never calls `paintSpeakingLengths` | AS01_defaultOffAndPixelIdentical | verified |
| R-0.2 | 0.2, 4.1 | Amplitude from `StringEngine::getLevel`, stop from the engine's `currentFret` + capo, or the slide contact; published by `LuthierEngine::publishSoundingNotes` | AS10, AS11, AS27, AS28 | verified |
| R-0.3 | 0.3, 2.3 | `GuitarRenderer::paintMotionGhost`: swept region, edge lines at ±A, mid lines at ±0.7A | AS12_AS13, AS23 | verified |
| R-0.4 | 0.4, 4.1, 11 | The publish runs every sub-block whatever the preference; nothing on the audio thread reads the preference | AS17_theAudioThreadIsUnaffected | verified |
| R-0.5 | 0.5, 11 | Dirty rects only; idle stops the clock; hidden and reduced motion stop it | AS13, AS14, AS15, AS19, AS20 | verified |
| R-2.1a | 2.1 | Stop point `nut + (bridge - nut)(1 - 2^(-f/12))`, which is `GuitarScene::stringAt` and `FretboardComponent::fretX` (`StringMotionGeometry::pointAt`) | AS04_stopPointAndBridgeAreNodes | verified |
| R-2.1b | 2.1 | Open string at the capo; fretted; tapped (`SoundingNotes::tapped`); slide contact from `SlideEngine::contactFret` with slant (`LuthierEngine::updatePerBlockModulation`) | openStringsStopAtTheCapo, AS10, AS11 | verified |
| R-2.1c | 2.1 | Bridge point: `StringLine::saddle` (illustration), virtual `nutX + (endX - nutX)/total` (fretboard); envelope over the full length, drawing clipped to the board | AS27_bothModes, AS04 | verified |
| R-2.1d | 2.1 | Nut-to-stop segment static (and displaced by a bend); behind a slide bar static | AS04, AS10 | verified |
| R-2.2a | 2.2 | `StringMotion::envelope`: Fourier pluck shape, `c_k` decay with 0.12 s, K = 4 (High) / 1 (Low) | AS05_thePluckShapeRelaxes | verified (window tuned, see Decisions) |
| R-2.2b | 2.2 | Harmonics: `|sin(h pi u)|` for partial >= 2 (`StringEngine::getHarmonicPartial`, new) | AS06_harmonicNodes | verified |
| R-2.2c | 2.2 | `L_n = clamp(level * 4)` in `StringMotion::normaliseLevel`, shared by both views (the illustration's timer uses it) | AS02_amplitudeIsProportional | verified |
| R-2.2d | 2.2 | `A_max = 0.40 x local spacing at the peak`, floored at 1.5 px | AS02 | verified |
| R-2.2e | 2.2 | Floor 0.5 px: inactive below, one final rest repaint | AS03_theFloorEmitsOneFinalDirtyRect | verified |
| R-2.3a | 2.3 | High/Low table: fill alpha, edges, mid lines, rest line alpha and dashes, 32/12 samples, 60/30 Hz | AS12_AS13, AS22, AS23 | verified |
| R-2.3b | 2.3 | Layer 26's accent glow skipped while `GuitarOverlay::motionActive`; played-note dot kept | AS12_AS13 (full vs dirty paint), AS01 | verified |
| R-2.3c | 2.3 | High contrast: Low style in the palette's text colour (`GuitarSpeakingStyle`) | AS23_materialLook | verified |
| R-2.3d | 2.3, 6.1 | Fretboard: colours from `GuitarRenderer::stringLooks`, thickness from the 0.9 + 1.5 s/(n-1) rule scaled by relative gauge | AS23 | verified |
| R-2.3e | 2.3 | 12-string: each engine string animates by its own index | by construction (per-index records); staticSpeakingLengthsMatchPaintString draws the 12-String Jumbo | verified |
| R-2.4a | 2.4 | `d = spacing(stopFret) sqrt(clamp(bend, 0, 450)/200)`; treble half toward the bass, bass half toward the treble, middle of odd counts toward the bass | AS07_bendsPushAcrossTheNeck | verified |
| R-2.4b | 2.4 | `pushCents` = MIDI/MPE bend + bend techniques + vibrato; whammy, slide and drift excluded; negative does not push | AS08_theWhammyDoesNotPushButAPitchBendDoes, AS07 | verified |
| R-2.5 | 2.5 | `StringMotion::dampingMask` (palm mute, bass palm mute), LightTouch x0.5, 25 ms decay from the stop onset for Released/Choked/Silenced/Chuck | AS09_muting | verified (mask ramp placed, see Decisions) |
| R-2.6a | 2.6 | Gate: preference, reduced motion, showing, relief < 2, not stale (`StringAnimator::poll`) | AS19, AS20, AS21, AS29 | verified |
| R-2.6b | 2.6 | Clock: `VBlankAttachment` throttled to >= 15 ms at High; 30 Hz `Timer` at Low and without a peer; runs only while a string is above the floor | AS22_frameRateCaps, AS14 | verified |
| R-2.6c | 2.6 | Stale after 250 ms without a new sequence, ease to rest over 120 ms, then stop | AS21_aStaleSnapshotEasesToRest | verified |
| R-CQ6 | cpu-quality-modes.md 6 (coordination) | `StringMotionPolicy` adapter (Full / Limited / Off): Off stops the strings and shows the static overlay with a fixed glow; Limited forces the Low style at 30 Hz. Today Off = Reduced motion; the swap to `AnimationPolicy::get().getMotion()` is one function body | cpuQualityMotionPolicy, AS19 | verified (adapter); pending the swap when FEAT-CPU's AnimationPolicy merges |
| R-2.6d | 2.6 | Relief hook `StringAnimator::setReliefLevel` (1 forces Low, >= 2 pauses) | AS29_theReliefHook | verified (hook); feeding it from the CPU relief ladder is pending, see Decisions |
| R-3a | 3 | Easy and Advanced illustrations and the Advanced fretboard animate | AS27_bothModes | verified |
| R-3b | 3 | Workshop bench (`BenchIllustration`) and `GuitarRenderer::render` thumbnails never animate | AS24_benchAndThumbnailsNeverAnimate | verified |
| R-3c | 3 | Scale trainer / tab reader fretboards: they are `FretboardComponent`, so they animate with it | by construction | verified |
| R-4.1a | 4.1 | `Source/Support/SoundingNotes.h/.cpp`: the piano roll's double-buffered class (origin/claude/luthier-visual) extended with a per-string `Motion` record and the sample clock | AS16, AS18 | verified |
| R-4.1b | 4.1 | Writer `LuthierEngine::publishSoundingNotes` at the end of `processSubBlock`, immediately before the CPU estimate; also after `reset()` | AS17 (count = blocks), AS18 | verified |
| R-4.1c | 4.1 | Reader retries (4 attempts, the class's own); a failed read keeps the previous frame (`StringAnimator::readSnapshot`) | by construction; AS16 | verified |
| R-4.1d | 4.1 | Data race closed: `getStringLevel` / `getStringFret` read the published atomics (`SoundingNotes::readLevel/readFret`) | full suite (every caller) | verified |
| R-4.1e | 4.1 | Accessor `LuthierEngine::getSoundingNotes()`; publish counter `getSoundingNotesPublishCount()` | AS17 | verified |
| R-4.2 | 4.2 | `Source/UI/Guitar/StringMotion.h/.cpp`: pure logic, fixed-size frame, dirty rect = union of last and this frame's swept bounds + stroke + 2 px | AS02-AS11, AS16 | verified |
| R-4.3 | 4.3 | `Source/UI/Guitar/StringAnimator.h/.cpp`: owned by composition by both views; test hooks `setClockForTesting`, `stepFrameForTesting`, `getLastDirtyUnion`, `isRunning`, `getFullRepaintCount` | AS12-AS14, AS20-AS22, AS29 | verified |
| R-4.4a | 4.4 | `GuitarRenderer::paint(..., PaintLayers)` with `omitSpeakingLengths`; the old overload unchanged; the cache rebuilt when the preference flips | staticSpeakingLengthsMatchPaintString, AS19 | verified |
| R-4.4b | 4.4 | `paintSpeakingLengths` (layer 25a) after the cache blit, before `paintOverlay`; nullptr draws the static lines | staticSpeakingLengthsMatchPaintString, AS12_AS13 | verified |
| R-4.4c | 4.4 | Strings whose swept bounds miss the clip are skipped; overlays painted in the same clipped paint | AS12_AS13 | verified |
| R-4.4d | 4.4 | Fretboard does the same between its strings and sounding-note blocks; its "excitement" blur is replaced while animating; its static board is cached while animating | AS15, AS27 | verified |
| R-4.4e | 4.4 | GPU-friendly: fills and a clipped image blit, solid colours, no effects, no per-frame image allocation | by inspection | verified |
| R-5a | 5 | Options -> APPEARANCE -> VISUAL AIDS (`VisualAidsSection`), directly under the tooltips / reduced-motion row; "Animate strings" toggle, "Quality" Low/High combo, help text, status line only under reduced motion | AS25_theOptionsRows | verified |
| R-5b | 5 | Always visible and enabled; saved on change through `UiPreferences`; every editor reads it on its next 30 Hz tick | AS25, AS26 | verified |
| R-6a | 6 | No parameters; host state bytes identical with the preference on or off; presets and sessions never change it | AS26_persistenceAndScope | verified |
| R-6b | 6 | `ui.json` keys `animateStrings` (bool), `animateStringsQuality` ("low"/"high"); missing file = defaults | AS26, AS01 | verified |
| R-6.1 | 6.1 | `GuitarRenderer::stringLooks` / `StringLook` from the scene's own `StringLine` loop | AS23 | verified |
| R-7 | 7 | Nothing is undoable (no undo entries pushed) | by inspection | verified |
| R-8a | 8 | Tab order after Reduced motion; accessible names "Animate strings" and "String animation quality"; description = help (+ paused) | AS25 | verified |
| R-8b | 8 | Rebindable "Toggle string animation" shortcut, unbound by default, in the shortcut table ("(not bound)") and handled by the editor | AS25 | verified |
| R-8c | 8 | Reduced motion always wins; the preference keeps its value | AS19_reducedMotionWins | verified |
| R-8d | 8 | Catalog keys `options.appearance.visualAids.*`, `accessibility.shortcut.toggleStringAnimation` | AS25 (strings read back) | verified |
| R-9 | 9 | Both editions identical: nothing is gated | AS30_notGatedByEdition | deferred (partial): the build has no Free configuration yet, so "the Free configuration builds and passes AS-01..AS-27" cannot run; the toggle has no edition check |
| R-10a | 10 | Slide contact with slant; bar lift re-anchors next frame | AS10 | verified |
| R-10b | 10 | Tap stop point; pull-off jumps with no easing | AS11 | verified (stop point); the square tap marker does not exist in this build (two-hand-tapping's overlay) - deferred with that overlay |
| R-10c | 10 | The chord name (piano roll 4, merged from origin/claude/luthier-visual) is painted after layer 25a in the same clipped `paint()` and repaints its own area as it fades | AS12_AS13 (the harness repaints the chord-name area as the component does and compares against a full repaint) | verified |
| R-10d | 10 | Strums start each string's envelope at its own sample (`exciteSample`) | AS27, AS28 | verified |
| R-10e | 10 | Rhythm engine, Tune Builder, key presses animate because the strings sound | AS28_everySourceAnimatesWhatSounds | verified |
| R-10f | 10 | Sympathetic ring and E-Bow at their true level; freeze, doubler, looper, backing track never (not strings); whammy/drift never push | by construction (level-driven); AS08 | verified |
| R-10g | 10 | Family switch: `GuitarBodyComponent::rebuildScene` resets the motion; multiple instances share the preference | by inspection; AS26 | verified |
| R-11a | 11 | Audio thread: 0 units of its own; the publish allocates nothing and touches no files | AS17, AS18_thePublishIsRealtimeSafe | verified |
| R-11b | 11 | Frame budget, 1920x1080 Advanced, both views, High, six strings | AS15_frameBudget | verified with the reference-CPU factor (see Decisions) |
| R-11c | 11 | Dirty union <= 25% of the illustration; one repaint call per string at most, never a full repaint | AS12_AS13 | verified |
| R-11d | 11 | Idle: zero repaints and zero frames | AS14_idleIsFree | verified |
| R-11e | 11 | `read` + `update` allocate nothing after the first frame | AS16_noAllocations | verified |
| R-11f | 11 | High <= 60 frames/s on 144 Hz, Low <= 30 | AS22_frameRateCaps | verified |
| R-12 | 12 | Failure modes: missing ui.json -> defaults; no vblank -> timer; failed read keeps the previous frame; stale eases; NaN/out-of-range clamped; no scene -> no motion; 10 frames over budget -> Low for the session, logged once | AS01, AS02, AS07, AS21, overBudgetFramesDropToLow | verified |
| R-13 | 13 | AS-01 to AS-30 in `Source/Tests/AnimatedStringsTests.cpp`, plus three extra tests | suite AnimatedStrings | verified |
| R-CF | "Coordinator follow-ups" | gui-integration 5 / 19 rows, guitar-illustration layer 25a, accessibility 5 list, gui-engine-dataflow element, file-formats keys, editions 2.3 row | - | pending: spec-document edits reserved for the coordinator |

## Decisions

- **SoundingNotes layout**: origin/claude/luthier-visual published `Source/Support/SoundingNotes.h/.cpp` (double-buffered class, packed note word per string) while this branch had its own seqlock struct; per the coordination rule its files were taken verbatim and extended only with a `Motion` record per string, the sample clock, an extended `publish` overload and `readLevel`/`readFret`. The four-argument `publish` still works.
- **Two SoundingNotes instances for now**: the piano roll arrived with its own `SoundingNotesPublisher`, which fills `LuthierAudioProcessor::getSoundingNotes()` once per host block with the four-argument publish; the strings read `LuthierEngine::getSoundingNotes()`, which `publishSoundingNotes` fills once per sub-block with the notes, bends, start samples and the Motion record. They share one file and never write each other's instance. Follow-up for the coordinator: point the processor's accessor at the engine's instance and retire the publisher once the piano-roll tests are checked against it (both fill the note set the same way).
- **`exciteSample` next to the piano roll's `startSample`**: the packed word zeroes the start sample when no note is held, but a released string still rings and its envelope needs its start.
- **`pushCents` next to the piano roll's `bendCents`**: the piano roll's bend is the whole offset from the key (whammy and slide included, +-50 cents); 2.4 needs the finger's push only, up to 450 cents.
- **Pluck position in the envelope**: `p` is measured from the bridge and `u` from the stop, so the pick sits at `u = 1 - p`; `c_k = sin(k pi (1 - p))/k^2`. The formula as printed would put the peak near the stop, contradicting AS-05.
- **AS-05 window [0.75, 0.95] instead of [0.8, 0.95]**: the spec's own formula with K = 4 peaks at u = 0.79 for p = 0.1 (a four-term Fourier series rounds the triangle's apex toward the middle); the window follows the formula rather than the formula the window.
- **Palm-mute mask `smoothstep(0.08, 0.16, 1 - u)`**: the printed `smoothstep(0, 0.08, 1 - u)` leaves the string moving under the palm, which AS-09 (zero for u >= 0.92) and 2.5's "pinned in the last 8%" rule out; the ramp starts where the palm ends.
- **AS-15 thresholds carry a reference-CPU factor of 2.0**: section 11's budget is for the mid CPU class (Ryzen 5 5600X / M2 Pro); the suite runs on shared 2.8 GHz Xeon vCPUs without boost, about half that single-thread speed, and the measured software renderer is single-threaded. Measured here: median about 1.5 ms, p99 about 2.9 ms; the raw figures are printed for the dashboard.
- **Near-horizontal ghosts drawn as quarter-pixel rectangle runs**: JUCE's software rasteriser's cost for an anti-aliased polygon edge grows with its horizontal length and crowds points onto a few scanlines; for strings within 6 degrees of horizontal (both views) the ghost is drawn as axis-aligned runs with steps of at most 0.25 px (visually the same anti-aliasing), which took the fretboard from about 11 ms to about 1.3 ms a frame. Steeper strings use paths.
- **Fretboard static cache while animating**: the board, inlays, scale overlay, frets, nut, capo and fret numbers are blitted from a cache keyed on size, scale, frets, strings, capo, palette and the overlay's pitch classes; with the animation off the fretboard paints exactly as before.
- **Fretboard layers after the REALISM-B merge**: `paintRealismB` (contact rings, palm-mute shading) moves as live state, so it is painted at the end of the uncached live layer, before the fret numbers as before; only the static board is ever cached.
- **Coalesced dirty rects**: neighbouring strings' rects overlap, so when their bounding rect is at most 1.25x their summed area the animator issues one `repaint` for the union (still at most one call per moving string, never the whole component).
- **"Showing" without a peer**: `isShowing()` needs a desktop window; with no peer the animator treats the owner as showing when it and its parents are visible, so offscreen tests exercise the gates.
- **`FretboardComponent` thickness**: the 0.9 + 1.5 s/(n-1) rule's mean scaled by each string's gauge relative to the set's mean.
- **Illustration's overlay repaints while animating**: the 30 Hz timer ignores level-only changes (the ghost repaints the string itself) and repaints only when a played-note dot appears or disappears, a fret changes or an overlay moves; on the fretboard a level-only change repaints just that note's dot area.
- **AnimationPolicy adapter** (`Source/UI/Guitar/StringMotionPolicy.{h,cpp}`, not `Source/UI/AnimationPolicy`, to avoid an add/add conflict with FEAT-CPU): the animator reads Full / Limited / Off from it; `getMotion()` becomes `AnimationPolicy::get().getMotion()` when that lands, and `setReliefLevel` should then be fed only through it (cpu-quality-modes 7). The "Animations are off while CPU quality is Low" line on the Appearance page is FEAT-CPU's.
- **Fixed glow at motion Off**: under Reduced motion (and CPU quality Low) layer 26's glow is on or off at a fixed alpha rather than fading with the level, and the illustration repaints only when a string starts or stops sounding or a fret changes, so no later frame changes the string pixels (AS-19, cpu-quality-modes 6's "static overlay, fixed glow").
- **Dashes laid along the whole string**: when the cache omits the speaking lengths, the winding dashes are still generated along the whole tail-to-post path and clipped to each part, so the pattern does not restart at the saddle or the nut.
- **Relief hook not yet fed**: `StringAnimator::setReliefLevel` exists and is tested; the CPU relief ladder (`Support/CpuRelief`) lives on origin/claude/luthier-visual, so wiring its level into both animators is left to the merge.
- **Options section as its own component** (`VisualAidsSection`): both specs' rows go under one VISUAL AIDS heading on the Appearance page; the piano roll's four switches (its `VisualAids` preferences on origin/claude/luthier-visual) belong below the string-animation rows in this component.
- **Unbound shortcut support**: `AccessibilitySettings::rebind` and `findAction` now ignore invalid (unbound) keys, so an unbound action cannot clash or be matched, and the shortcut table prints "(not bound)".
