# ANIMATED STRINGS SPEC

The strings on the guitar illustration and on the fretboard visibly
vibrate while they sound. A hard pluck on the low E shows a wide blurred
envelope that shrinks as the note dies. A bend pushes the string across
the neck. A palm mute pins the string at the bridge, and a choke stops it
dead. A pinch harmonic shows its node.

Added 2026-09-24 at the product owner's request. It is additive to
`gui-integration.md`, `guitar-illustration.md` and
`piano-roll-chord-display.md`, and it moves nothing that is already placed.
It is a display feature only. It never changes the sound, and it adds no
work to the audio thread beyond the per-string state publication it
shares with the piano roll (section 4.1).

## 0. Ground rules

1. **Off by default.** It is a preference that a player opts into (Options
   -> Appearance -> Visual aids). A fresh install draws exactly what it drew
   before this spec.
2. **What vibrates is what sounds.** The amplitude comes from the engine's
   own per-string level follower (`StringEngine::levelFollower`), published
   once per block. The stop point is wherever the waveguide is actually
   stopped: fret, tap, slide contact or nut/capo. It is never taken from
   MIDI or from the UI's guess about what was played. That includes
   voicer-placed notes, rhythm-engine strums, Tune Builder playback,
   sympathetic ring and E-Bow sustain.
3. **Physically plausible, not literal.** Strings vibrate at 80-1300 Hz
   and the screen refreshes at about 60 Hz, so drawing the instantaneous
   displacement would alias into a meaningless wobble. The feature draws
   what the eye sees on a real string, which is the swept envelope: a
   blurred "motion ghost" that is brightest at its two edges, where a
   sinusoidally moving string spends most of its time (arcsine density).
4. **Zero audio-thread cost of its own.** Turning the feature on or off
   changes nothing on the audio thread. Offline renders are bit-identical
   either way.
5. **Cheap or absent.** Repaints are dirty-rect only. Nothing is repainted
   when nothing sounds. The whole editor's animation frame costs < 1 ms at
   1920x1080 (section 11). The feature pauses when the editor is hidden and
   turns off under Reduced motion.

## 1. User stories

- *Guitarist:* "I strum a chord and see all six strings shimmer, each one
  dying away at its own rate. The low E rings longest."
- *Learner:* "I can see which string is still ringing when I meant to mute
  it."
- *Pianist using the piano roll:* "I press keys and see which strings on
  the guitar actually move."
- *Low-vision or motion-sensitive user:* "Reduced motion turns it off, and
  I never have to find a second switch."
- *Laptop user on battery:* "Quality: Low halves the frame rate, and when
  the plugin window is closed it costs nothing."

## 2. Exact behaviour

### 2.1 Geometry per string

For each string *s*, with "speaking length" meaning the part that
vibrates:

- **Stop point** `P_stop`: `GuitarScene::stringAt(s, stopFret)` on the
  illustration, or `FretboardComponent::fretX(stopFret)` on the fretboard.
  `stopFret` is published by the engine (4.1) in absolute frets from the
  nut:
  - an open string gives the capo fret, or 0;
  - a fretted note gives its fret;
  - a tapped note gives the tap fret;
  - under a slide, it is `SlideEngine::contactFret(s, barFret, n, scale)`,
    which already includes the bar's slant.
- **Bridge point** `P_br`: `StringLine::saddle` on the illustration. The
  fretboard has no bridge, so there the bridge point is the virtual
  `x = nutX + (endX - nutX) / total`, the scale-length point implied by
  `fretX`. Only the part of the speaking length that lies on the board is
  drawn, but the envelope is computed over the full length.
- **Behind the stop:** the segment from the nut to the stop point never
  vibrates. It is drawn as a static line, displaced by a bend (2.4).
  Behind a slide bar it is drawn static too, because the fretting hand
  damps it (slide-guitar.md).

### 2.2 Envelope shape

With `u` in [0, 1] running from the stop point (0) to the bridge (1),
the half-width of the swept region is:

```
A(u, t) = A_max · L_n · E(u, t) · D(u)

E(u, t) = | Σ_{k=1..K} c_k(t) · sin(k π u) |  normalised so max_u E = 1
c_k(t)  = sin(k π p) / k²  ·  exp(-(k-1) · t / 0.12 s)
```

- `p` is the pluck position as a fraction of the speaking length measured
  from the bridge. It is the note's `Excitation::Params::pluckPosition`,
  published per string.
- `t` is the time since the note started, from the published start sample.
- At t = 0 the shape is the Fourier approximation of the pluck triangle,
  with its peak under the pick. Over about 0.3 s it relaxes to the
  fundamental's half-sine as the upper modes damp faster. That is what a
  real string does, and it is the only visible motion within a note apart
  from the decay.
- `K` = 4 at High quality. At Low quality `K` = 1, so the shape is a pure
  half-sine.
- **Harmonics** (harmonic-realism.md): when the published
  `harmonicPartial` *h* is at least 2, the shape is `|sin(h π u)|`, with
  nodes at `u = j/h`. This applies to natural, pinch, tapped and
  artificial harmonics.
- `D(u)` is the damping mask (2.5). It is 1 when the string is open.

**Amplitude.** `L_n = clamp(level · 4.0, 0, 1)`. This is the same
normaliser `GuitarBodyComponent::timerCallback` already uses, moved into
`StringMotion::normaliseLevel` so both views share it. The visual
amplitude is **linear in the engine level**, as the brief requires.
`A_max` is `0.40 × the local string spacing at the envelope's peak`, in
px. That is the largest ghost that never merges with its neighbour's.
It is floored at 1.5 px. Real peak excursions (about 1-2 mm) would be
sub-pixel at the illustration's typical 0.77 px/mm, so the exaggeration
is deliberate, but it stays proportional.

**Floor.** When `A_max · L_n` is below 0.5 px, the string is drawn at rest
and is not animated. When it crosses the floor downward, one final frame
repaints the string at rest.

### 2.3 Drawing (the "motion ghost")

The ghost is drawn in the string's material colour and at its width, from
`GuitarScene::StringLine` (`colour`, `winding`, `widthMm`, `minWidthPx`,
guitar-illustration.md 10). The width formula is the same as
`paintString`.

| Element | High | Low |
|---|---|---|
| Swept region (polygon along the normal, ±A) | fill, material colour, alpha `0.10 + 0.12·L_n` | same |
| Edge lines at ±A (arcsine peaks) | stroked at string width, alpha `0.35 + 0.45·L_n` | not drawn |
| Mid lines at ±0.7A | stroked at 0.6× width, alpha 0.5× edge | not drawn |
| Rest line (centre) | material colour, alpha `1 - 0.6·L_n`, winding dashes kept | plain line, no dashes |
| Samples per speaking length | 32 | 12 |
| Frame rate | 60 Hz (vblank, throttled) | 30 Hz |

- The accent-coloured glow that `paintOverlay` layer 26 currently strokes
  along a sounding string is **replaced** by the ghost while animation is
  on. The played-note dot stays.
- **High-contrast palette:** Low-style drawing, with the palette's primary
  text colour instead of the material colour. That palette uses no
  gradients and no translucency beyond a single fill (accessibility 3).
- **Fretboard component:** the same element table, using the fretboard's
  horizontal strings. Colour and width come from the new
  `GuitarRenderer::stringLooks(guitar)` (section 6.1). Its thickness is
  the current `0.9 + 1.5·s/(n-1)` rule scaled by relative gauge.
- **12-string:** each string of a course animates independently by its
  engine index.

### 2.4 Bends: the string pushed across

A finger bend moves the stop point sideways, perpendicular to the string
in the fretboard plane:

```
d = spacing(stopFret) · sqrt( clamp(bendCents, 0, 450) / 200 )
```

The square root is the physics. Pushing a string sideways by *d*
lengthens it by an amount proportional to *d²*. Tension rises with that
lengthening, and for small bends the pitch in cents rises roughly in
proportion to it. So a whole-tone bend (200 cents) moves the string about
one string spacing, which matches what players see, and a quarter-tone
moves it about 0.35 of a spacing.

- **Direction:** strings in the treble half (engine index < n/2) are
  pushed toward the bass side, and the bass half is pulled toward the
  treble side. On odd counts, the middle string goes toward the bass side.
- Both segments are drawn from the displaced stop point: nut to stop, and
  stop to bridge. The bent string is drawn over its neighbours.
- `bendCents` is the finger-bend part only: MIDI/MPE pitch bend, the
  microtonal-bends source stack, bend techniques and vibrato. Whammy, slide
  glide and tuning drift change pitch through tension or length rather
  than by pushing the string, so they **never** displace it. Negative bend
  values do not displace either, because a string cannot be pushed flat.
- Vibrato from an LFO or aftertouch animates the displacement at its own
  rate, sampled at the frame rate. This is truthful, because vibrato is
  slower than 15 Hz.
- The microtonal-bends cents badge and the bend arc
  (gui-techniques-updates 4) stay and are drawn above the string.

### 2.5 Damping and muting

`D(u)` and the extra decay come from the published `StringEngine::Damping`:

| Damping | Mask `D(u)` | Visual decay |
|---|---|---|
| Open | 1 | follows the level |
| LightTouch | 1 | follows the level, with `A_max` × 0.5 |
| PalmMute | `smoothstep(0, 0.08, 1-u)`: pinned in the last 8% before the bridge, where the palm sits | follows the level |
| Released, Choked, Silenced, Chuck | 1 | `L_n` is multiplied by `exp(-Δt / 25 ms)` from the damping onset, so the visual dies even if the follower's release is slower |

On the fretboard component, the palm zone lies beyond the board, so a palm
mute shows only as the smaller amplitude.

### 2.6 Frame clock, gating and staleness

**Effective state:**

```
animate = pref.animateStrings
          && !AccessibilitySettings::isReducedMotion()
          && component.isShowing()
          && reliefLevel < 2
          && !snapshotStale
```

- **Reduced motion:** the strings render as the existing static overlay,
  where layer 26 glows at a fixed width. That matches guitar-illustration
  16 ("no vibrating strings"). The preference keeps its stored value, so
  turning Reduced motion off brings the animation back.
- **Editor hidden, closed or minimised, or the component not showing:**
  this covers Easy's illustration while in Advanced and the other way
  round, the Workshop covering columns 3 and 4, and the host minimising
  the window. The vblank attachment is dropped, and there are no frames
  and no repaints. When the component shows again, it resumes on the
  first frame.
- **Frame clock:** a `juce::VBlankAttachment`, throttled so frames come at
  least 15 ms apart. That caps a 120/144 Hz display at 60 Hz. At Low, and
  whenever the component has no peer (tests), it falls back to the
  component's existing 30 Hz `juce::Timer`. The clock runs only while at
  least one string is above the floor.
- **Stale:** when no new snapshot sequence has arrived for 250 ms (host
  bypass, audio device stopped), all strings ease to rest over 120 ms, and
  then the clock stops. This uses the same threshold as the piano roll.
- **CPU relief** (performance-budget 8): relief step 1 ("drop display
  drain rate") forces Low. Step 2 or higher pauses the animation.
  `StringAnimator::setReliefLevel(int)` is the hook, fed by whatever
  implements section 8.

## 3. Where it shows

| Surface | Animated? | Why |
|---|---|---|
| Easy Mode guitar illustration (`EasyPanel::guitarBody`) | Yes | primary surface |
| Advanced Mode guitar illustration (`AdvancedPanel::guitarBody`) | Yes | |
| Advanced Mode fretboard (`AdvancedPanel::fretboard`) | Yes | |
| Workshop bench (`BenchIllustration`, WorkshopPanel.cpp) | **No** | The bench is a precision editing surface. Strings are hit targets there (selection outline, string drag, workshop-ui 3), and moving them would fight the click. The bench also repaints its full uncached scene, so animation would break the bench's own budget. Its audition feedback is the spectrum delta and the pickup pulse. |
| Preset-browser thumbnails (`GuitarRenderer::render`) | **Never** | Static images from the worker thread (guitar-illustration 15: "no live overlays"). |
| Scale trainer and tab-reader fretboards | Same as the fretboard component that hosts them | |

## 4. Engine / processing design

### 4.1 Publication: `SoundingNotes` per-string record

`piano-roll-chord-display.md` 2 already introduces a `SoundingNotes`
snapshot that the audio thread publishes after each block. This spec
fixes its per-string record. Whichever spec lands first creates the file.
The other adds only its fields.

**New file** `Source/Support/SoundingNotes.h`. It is header-only and
POD-only.

```cpp
struct SoundingString            // one per engine string, kMaxStrings = 12
{
    std::atomic<float>   level;            // StringEngine::getLevel()
    std::atomic<float>   stopFret;         // absolute frets from the nut (2.1)
    std::atomic<float>   bendCents;        // finger-bend part only (2.4)
    std::atomic<float>   pluckPosition;    // last Excitation::Params::pluckPosition
    std::atomic<int64_t> startSample;      // note start (piano roll uses it too)
    std::atomic<int8_t>  midiNote;         // -1 = none (piano roll)
    std::atomic<uint8_t> damping;          // StringEngine::Damping
    std::atomic<uint8_t> harmonicPartial;  // 0/1 = none
    std::atomic<uint8_t> stopKind;         // open, fretted, tapped, slide
};
struct SoundingNotes
{
    std::array<SoundingString, 12> strings;
    std::atomic<uint64_t> noteSet[2];      // piano roll's 128-bit set
    std::atomic<int64_t>  samplePosition;  // engine sample clock at publish
    std::atomic<double>   sampleRate;
    std::atomic<uint32_t> sequence;        // seqlock: odd while writing
};
```

- **Writer:** the new `LuthierEngine::publishSoundingNotes()`, called once
  at the end of `LuthierEngine::processSubBlock` (LuthierEngine.cpp,
  immediately before the `cpuEstimate.store`). It increments `sequence`
  to an odd value, does relaxed stores of each field, then increments it
  to an even value with release ordering. It never waits.
  - `stopFret` is the value the block used for the string's length:
    `currentFret[s]`, replaced by the slide contact when
    `slideEngine.isActive()` and the string is in the contact mask.
  - `bendCents` is `midi.getStringBendCents(s)` minus the whammy and slide
    contributions (it is already separate from `WhammyEngine`).
  - `damping` comes from `StringEngine::getDamping()`, and
    `harmonicPartial` from a new
    `StringEngine::getHarmonicPartial() const noexcept`.
- **Cost:** 12 × 9 stores, plus 4 stores and 2 RMWs. That is under 0.005
  units, and it is booked to `MeterFIFO / display` (performance-budget 1).
  It runs **regardless** of the Animate toggle and of whether an editor
  exists, so the toggle cannot change the audio thread.
- **Reader:** `SoundingNotes::read(Snapshot&)` on the message thread is a
  seqlock copy. It retries at most 3 times, and after that it keeps the
  previous snapshot. It never blocks the writer.
- **Latent bug closed:** `LuthierEngine::getStringLevel` and
  `getStringFret` currently read plain `double`s written by the audio
  thread, which is a data race. `GuitarBodyComponent` and
  `FretboardComponent` switch to the snapshot, and the two getters become
  thin wrappers over it.
- **Accessor:** `const SoundingNotes& LuthierEngine::getSoundingNotes() const noexcept`.

### 4.2 Message-thread model: `StringMotion`

**New files** `Source/UI/Guitar/StringMotion.h/.cpp`. This is pure logic
with no Component, so it is unit-testable.

- `struct StringMotionInput` holds a snapshot string record, the per-view
  geometry (`P_stop` / `P_br` / nut in px, the local spacing, and the
  normal vector), the quality and the time.
- `struct StringMotionFrame` holds, per string: `active`, the
  `displacedStop`, `samples[33]` (px offsets along the normal), and
  `dirty` (a `juce::Rectangle<int>`). The arrays are fixed-size, so there
  is no allocation.
- `static float normaliseLevel(float level) noexcept`.
- `void StringMotion::update(const SoundingNotes::Snapshot&, const Geometry&, double nowSeconds, StringMotionFrame&) noexcept`
  applies sections 2.2, 2.4 and 2.5 and computes each string's dirty rect.
  The dirty rect is the union of last frame's and this frame's swept
  bounds, plus the stroke width, plus 2 px of anti-aliasing margin.

### 4.3 Frame driver: `StringAnimator`

**New files** `Source/UI/Guitar/StringAnimator.h/.cpp`. The illustration
and the fretboard each own one, by composition.

- It owns the gating of 2.6, the `VBlankAttachment` or the timer fallback,
  the throttle, the stale detection and the relief level.
- Each frame, it calls `StringMotion::update` and then
  `owner.repaint(rect)` once per dirty string. It never calls a
  full-component `repaint()`.
- Test hooks: `setClockForTesting(std::function<double()>)`,
  `stepFrameForTesting()`, `getLastDirtyUnion()`, `isRunning()` and
  `getFullRepaintCount()`.

### 4.4 Renderer and component changes

- `GuitarRenderer::paint(g, scene, mmToPx, PaintLayers layers)`:
  - `PaintLayers::omitSpeakingLengths` draws each string only from the tail
    to the saddle and from the nut to the post. The overload that exists
    today keeps its current behaviour.
  - `GuitarBodyComponent::rebuildCache` uses the omit variant while
    animation is enabled, and rebuilds the cache when the preference
    flips.
- New `GuitarRenderer::paintSpeakingLengths(g, scene, mmToPx, const StringMotionFrame*)`:
  - With `nullptr`, it draws static rest lines, which is identical to what
    `paintString` produces today. A golden-image test holds it to that.
  - It is called in `GuitarBodyComponent::paint` after the cache blit and
    **before** `paintOverlay`. That makes it layer **25a** in
    guitar-illustration.md 5's z-order, under played-note dots (26),
    buzz heatmap (27), slide bar (28), pick (29), tap markers and bend arcs
    (gui-techniques-updates, 33+), the chord name (piano roll 4), and
    hover (31).
- `GuitarOverlay` gains `bool motionActive`. When it is set, layer 26
  skips the accent glow along the string (2.3).
- `paint()` skips any string whose swept bounds do not intersect
  `g.getClipBounds()`, and the cache blit is clipped by JUCE to the dirty
  region. The chord name and every overlay above 25a are simply painted
  inside the same clipped `paint()`, so dirty rects stay correct without
  per-layer bookkeeping (test AS-12).
- `FretboardComponent::paint` does the same between the "strings" block
  and the "sounding notes" block. Its existing "excitement" blur stroke is
  replaced while the animation is active.
- **GPU-friendly rendering:** path fills and strokes, a clipped image blit
  from the existing cache, and solid colours only. There is no
  `ImageEffectFilter` or `DropShadow`, no per-frame image allocation, and
  no pixel read-back. That keeps the drawing on the accelerated path of
  JUCE 8's Direct2D and CoreGraphics renderers. `juce::Path` objects are
  members, cleared and reused each frame.

## 5. Options

On Options -> **APPEARANCE** (`AppearancePage`, OptionsPages.h/.cpp), a
**VISUAL AIDS** section starts directly under the row that holds "Show
tooltips on hover" and "Reduced motion". The piano roll's
`piano-roll-chord-display.md` 5 section goes into this same "Visual aids"
heading; the coordinator merges them.

```
THEME AND SIZE
[Palette      v] [100 % v]
[x] Show tooltips on hover     [ ] Reduced motion
VISUAL AIDS
[ ] Animate strings            Quality [ High v ]
    Strings vibrate on the guitar and fretboard while they sound.
    Display only: no effect on the sound.
    Paused while Reduced motion is on.        <- only when it applies
```

| Control | Type | Default | Storage |
|---|---|---|---|
| Animate strings | `juce::ToggleButton animateStringsToggle` | **off** | `UiPreferences` key `animateStrings` (bool) |
| Quality | `juce::ComboBox animateQualityBox` (Low, High) | High | `UiPreferences` key `animateStringsQuality` ("low" or "high") |
| Status line | `juce::Label animateStatusLabel` (muted) | hidden | derived |

- Both controls are always visible and always enabled, per
  gui-integration 0.7, which says nothing is greyed out. The quality can
  be set while the toggle is off.
- Changes save immediately (`UiPreferences::setBool` writes through). All
  open editors in the process pick the change up on their next 30 Hz tick.
- **Why `UiPreferences` rather than `UiState` (where `tooltipsEnabled`
  lives):** this setting is about the person's machine and eyes, like
  Reduced motion. It must not change when a different preset or host
  session loads, and it must not travel to a collaborator's machine inside
  a project.

## 6. Parameters, data model and files

- **Parameters: none.** It is display only. It is not automatable, it is
  not in the APVTS, and the parameter list is unchanged.
- **Presets, snapshots, `.luthier*` files and host state:** nothing. The
  `getStateInformation` bytes are identical whatever the preference.
- **`Documents/Luthier/config/ui.json`** (UiPreferences) gains the two
  keys above. A missing or unreadable file means defaults (off, High). The
  file-formats.md table of `ui.json` keys gains the two rows.

### 6.1 `GuitarRenderer::stringLooks`

`static std::array<StringLook, 12> stringLooks(const WorkshopGuitar&)`,
where `StringLook` is `{ colour, winding, widthMm, minWidthPx, wound }`.
It is extracted from the string loop in `GuitarRenderer::build`
(GuitarRenderer.cpp, the block building `GuitarScene::StringLine`), so
the fretboard and the illustration can never disagree on a material's
colour.

## 7. Undo

Nothing here is undoable. The toggle and the quality are options, like
the palette (action-and-undo 3.17). The animation is a live overlay
(action-and-undo 7).

## 8. Accessibility

- The toggle and the combo sit in the page's Tab order, after "Reduced
  motion". Space toggles, and the arrow keys change the quality.
- Accessible names and values: "Animate strings, off" and "String
  animation quality, High". The description is the help text, plus
  "Paused while reduced motion is on" when that applies.
- There is a rebindable shortcut, **"Toggle string animation"**, which is
  unbound by default and appears in the Accessibility page's shortcut
  table (accessibility 2 and 9).
- Reduced motion always wins (2.6). accessibility.md 5's list gains
  "string animation".
- No announcements, because it is purely visual. The strings' accessible
  descriptions (guitar-illustration 16) do not change.
- Nothing is conveyed by motion alone. Which strings sound is still shown
  by the played-note dots and the per-string activity strip.
- All strings go in the locale catalog, under
  `options.appearance.visualAids.*`.

## 9. Edition split

**Both editions, identical** (editions.md, which adds a row to table 2.3).
It is not a headline feature, it is accessibility-adjacent (0.3 says
never gate accessibility), and it sells the modelled strings in Free. The
bench exclusion (section 3) is moot in Free, which has no Workshop.

## 10. Interactions

| Feature | Behaviour |
|---|---|
| Slide bar overlay (slide-guitar 7, gui-engine-dataflow 6.4) | The stop point is the per-string contact with slant (2.1). The segment behind the bar is static. The bar (layer 28) draws over the strings. A bar that lifts (the stopFret switches back) re-anchors on the next frame. |
| Tap markers (two-hand-tapping) | The stop point is the tap fret. The square marker draws over the string. On a pull-off, the stop point jumps to the fretted note with no easing, because that is what the string does. |
| Chord name overlay (piano roll 4) | Drawn over the strings in the same clipped `paint()`. Both features work together and they share the snapshot and its 250 ms staleness rule. |
| Played-note dots, buzz heatmap, pick overlay, scrape trail, mute-zone shading, bend arc | Unchanged, all above 25a. Only the layer-26 glow is suppressed while the animation is on. |
| Harmonics (harmonic-realism) | Nodes (2.2). |
| Muting-rhythm, palm mute, chucks, strum-dynamics | By damping (2.5). A strum's staggered onsets start each string's envelope at its own start sample. |
| Rhythm engine, Tune Builder playback, audition phrase, piano-roll keys | They animate because the strings sound (0.2). |
| Sympathetic ring (string-interaction, body-coupling), E-Bow | They animate at their true, usually small, level. The E-Bow holds a steady envelope. |
| Freeze layer, doubler, looper, backing track | Not strings, so no motion. |
| Whammy, tuning stability, environment drift | No lateral displacement (2.4). |
| Capo | Open strings stop at the capo (2.1). |
| Presets, snapshots, morph, host automation, MIDI export, notation, offline render | No interaction. It is not state and not audio. |
| Workshop bench, thumbnails | Never animated (section 3). |
| Family switch | Geometry comes from the new scene on the next frame. The `StringMotionFrame` is reset. |
| Multiple instances | Each editor animates itself and holds its own budget. The preference is shared through `ui.json`. |

## 11. Performance budget

- **Audio thread:** 0 units attributable to this feature. The shared
  publication in 4.1 costs under 0.005 units.
- **Message thread, per animation frame, whole editor**, at 1920x1080 and
  100% scale, High quality, all strings at full level, both the Advanced
  illustration and the fretboard animating: **median < 1.0 ms, p99 <
  2.0 ms** on the mid CPU class (performance-budget 0). This covers the
  snapshot read, `StringMotion::update`, and painting every dirty rect.
  The combined live overlay of guitar-illustration 17 (≤ 2 ms) still
  holds with the animation included.
- **Dirty area:** each frame's repaint union is ≤ 25% of the
  illustration's area, and at most one `repaint(rect)` call per animated
  string. The animation never makes a full-component `repaint()`.
- **Idle:** zero repaints and zero frame callbacks once every string is
  under the floor, or while hidden or stale.
- **Allocations:** `SoundingNotes::read` and `StringMotion::update`
  allocate zero bytes after the first frame.
- **Frame rate:** High ≤ 60 frames/s even on 144 Hz displays, and Low ≤ 30.

## 12. Failure modes

| Failure | Response |
|---|---|
| `ui.json` missing or corrupt | Defaults (off, High). No banner (UiPreferences contract). |
| No vblank (no peer, a host that suspends the display link) | Timer fallback at the quality's rate. |
| The seqlock read fails 3 times | Keep the previous snapshot for that frame. |
| The snapshot goes stale (bypass, device stop) | Ease to rest in 120 ms, then stop (2.6). |
| `stopFret` or `bendCents` is NaN or out of range | Clamped to [0, numFrets] and [0, 450]. A non-finite level is treated as 0. |
| The scene is empty (the guitar is still loading) | No motion. Nothing is drawn until the scene exists. |
| A frame goes over budget for 10 consecutive frames | The animator drops itself to Low for the rest of the session and logs once to the diagnostics log (error-recovery.md log format). No banner. |

## 13. Tests

In the `LuthierTests` target, new file `Source/Tests/AnimatedStringsTests.cpp`
(suite `AnimatedStrings`). The GUI tests run under xvfb in the style of
`EditorTests.cpp` (offscreen `paintEntireComponent`), and they use
`StringAnimator::setClockForTesting` and `stepFrameForTesting` for
deterministic time.

- **AS-01 Default off.** With a fresh `UiPreferences` (after `reset()`),
  `animateStrings` is false. The Easy illustration, with six strings
  sounding, renders pixel-identical to the pre-feature golden render of
  `paintOverlay`, and `StringAnimator::isRunning()` is false.
- **AS-02 Proportional amplitude.** `StringMotion`, with a string at
  `level` 0.0625 and then 0.125 (L_n 0.25 and 0.5), same geometry: the
  peak half-width ratio is 2.00 ± 0.02. At level ≥ 0.25 the peak equals
  `A_max` ± 0.1 px.
- **AS-03 Floor.** When `A_max·L_n` < 0.5 px, the frame marks the string
  inactive. One final dirty rect is emitted on the transition, and none
  afterwards.
- **AS-04 Stop point and bridge node.** For fret 5 on string index 2, the
  envelope is ≤ 0.5 px at `stringAt(2, 5)` and at the saddle, and the
  nut-to-fret-5 segment has zero amplitude.
- **AS-05 Pluck shape relaxes.** With `pluckPosition` 0.1 and K = 4, at
  t = 0 the peak lies in u ∈ [0.8, 0.95] (near the bridge). At t = 0.5 s
  the peak lies at u = 0.5 ± 0.03. At Low, the peak is at 0.5 at every
  time.
- **AS-06 Harmonic nodes.** With `harmonicPartial` = 2, the amplitude at
  u = 0.5 is ≤ 5% of the peak. With partial 3, the same holds at 1/3 and
  2/3.
- **AS-07 Bend displacement.** String index 1, fret 7:
  - 200 cents displaces the stop point by one local spacing ± 10%,
    toward the bass side.
  - 50 cents displaces it by 0.5 spacing ± 10%.
  - String index 5 (low E) with 200 cents moves toward the treble side.
  - -100 cents gives zero displacement.
- **AS-08 Whammy does not push.** An engine render with a whammy dive of
  -300 cents gives `bendCents` = 0 in the snapshot and no lateral
  displacement. A MIDI pitch bend of +200 cents gives `bendCents` of
  200 ± 1.
- **AS-09 Muting.**
  - PalmMute: the envelope is 0 for u ≥ 0.92.
  - Chuck and Silenced: the visual `L_n` is ≤ 5% of its pre-mute value
    within 80 ms, even with a follower level still at 20%.
  - LightTouch: the peak is half of the Open peak at equal level.
- **AS-10 Slide contact.** Slide on, bar at fret 7.3 with a 6° slant: each
  string's node sits at `SlideEngine::contactFret(s, ...)` ± 0.5 px, and
  the segments behind the bar have zero amplitude. With the bar lifted,
  the stop returns to the fretted or open position on the next frame.
- **AS-11 Tap point.** A tap at fret 12 over fretted fret 5: the node is
  at fret 12 and the square marker is drawn over the string. After the
  pull-off, the node is at fret 5 within 1 frame.
- **AS-12 Dirty-rect correctness.** Take a 120-frame scripted performance
  (strum, bend, palm mute, slide, tap, chord name showing) and paint only
  the dirty rects each frame into a persistent image. After every frame
  that image matches a full repaint within 2/255 per channel, including
  the chord-name, slide-bar and tap-marker pixels.
- **AS-13 No full repaints.** During AS-12, `getFullRepaintCount()` stays
  at 0 and the per-frame dirty union is ≤ 25% of the component area.
- **AS-14 Idle is free.** After every string decays below the floor, 1 s
  of stepped time produces 0 repaint calls and `isRunning()` is false.
- **AS-15 Frame budget.** At the editor size for 1920x1080 in Advanced
  (illustration plus fretboard), High, 6 strings at full level, measure
  600 frames of `stepFrameForTesting` plus dirty-rect paints into a
  software image with `Time::getHighResolutionTicks`. The median is
  < 1.0 ms and the p99 < 2.0 ms. It is reported to the performance
  dashboard (performance-budget 9).
- **AS-16 No allocations.** With the allocation counter
  (`LUTHIER_ALLOCATION_COUNTER`, as in CircuitTests.cpp), 600 calls of
  `SoundingNotes::read` plus `StringMotion::update` make 0 allocations
  after the first.
- **AS-17 Audio thread unaffected.** A 10 s offline render of a fixed
  MIDI performance with the preference on and an editor animating is
  bit-identical to the same render with it off, with no editor. The
  publish runs once per sub-block in both cases, counted by a test
  counter.
- **AS-18 Publish is realtime-safe.** 5 minutes of playback with the
  allocation trap and the lock trap on the audio thread: zero triggers
  from `publishSoundingNotes`.
- **AS-19 Reduced motion.** With the animation on, turning Reduced motion
  on means that within 1 frame `isRunning()` is false, the illustration
  equals the static reduced-motion render, and no later frame changes the
  string pixels. Turning it off resumes within 1 frame, and the
  preference is still true.
- **AS-20 Hidden editor.** `setVisible(false)` on the Easy panel, or
  switching to Advanced so Easy's illustration is hidden: the hidden
  animator stops with 0 repaints over 1 s. Showing it again resumes
  within 1 frame. Destroying the editor while strings sound leaves no
  timers or vblank attachments alive (a leak detector plus a
  `Timer::isTimerRunning` check).
- **AS-21 Stale snapshot.** Stopping `processBlock` calls: after 250 ms
  plus 120 ms, every string is at rest and the animator is stopped.
- **AS-22 Frame-rate caps.** With a simulated 144 Hz vblank, High yields
  ≤ 60 frames per second. Low yields ≤ 30.
- **AS-23 Material look.**
  - Nylon trebles: the ghost's rest-line pixel matches `#F2E9D8` within
    1 hex step.
  - Phosphor bronze wound: it matches `#B5824A`.
  - The edge-stroke width equals `paintString`'s width ± 0.25 px.
  - The fretboard uses `stringLooks` colours.
  - High contrast uses the Low style and the text colour.
- **AS-24 Workshop bench and thumbnails never animate.** With the
  animation on and strings sounding, `BenchIllustration` string pixels
  are identical across 30 stepped frames. `GuitarRenderer::render`
  output hashes are identical with and without sounding strings, for
  every factory guitar.
- **AS-25 Options UI.**
  - "Animate strings" and "Quality" are on the Appearance page under
    VISUAL AIDS, directly below the "Show tooltips on hover" row.
  - Both are reachable by Tab, with the accessible names from section 8.
  - They are visible and enabled whether the toggle is on or off.
  - The status line shows only under Reduced motion.
- **AS-26 Persistence and scope.**
  - The toggle and the quality survive editor close and reopen, and a
    new processor instance (they are in `ui.json`).
  - `getStateInformation` bytes are identical with the toggle on or off.
  - Loading a preset or a host session never changes them.
- **AS-27 Both modes.** With the animation on and a strummed E major:
  - Easy's illustration animates 6 strings.
  - Advanced's illustration and fretboard animate the same 6 strings.
  - Each shows a non-zero dirty area within 2 frames of the note-on.
- **AS-28 Rhythm, piano roll and Tune sources (combination).**
  - Rhythm-engine strums animate the voiced strings.
  - Piano-roll key presses animate the string the MidiInterpreter
    assigned.
  - Tune Builder playback animates. In each case, the set of animated
    strings equals the set of strings with `SoundingNotes` level above
    the floor.
- **AS-29 Relief hook.** `setReliefLevel(1)` forces 30 Hz and the Low
  style, and `setReliefLevel(2)` stops the animator.
  `setReliefLevel(0)` restores the preference.
- **AS-30 Edition.** The Free configuration builds and passes AS-01 to
  AS-27. The toggle is present and unlocked in Free.

Coordinator follow-ups (not edited by this spec):
- gui-integration.md: add the Visual aids row to 5 APPEARANCE and add a
  row to 19.
- guitar-illustration.md: add layer 25a to 5 and cross-reference 16.
- accessibility.md 5: add string animation to the Reduced motion list.
- gui-engine-dataflow.md: add a `SoundingNotes` element (60 Hz drain,
  250 ms stale).
- file-formats.md: add the two `ui.json` keys.
- editions.md 2.3: add the row.
