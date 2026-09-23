# SLIDE TECHNIQUE CONTROLS SPEC

Extends `slide-guitar.md`. That file models slide physics: bar
material, contact area, damping, glass vs brass vs steel, lap-steel
vs bottleneck. This file adds the user-modifiable control layer:
what the player can tweak in real time to shape the slide gesture.

## 0. Ground rules

1. **Do not duplicate slide-guitar.md.** Bar material, contact
   physics, and Slide Mode toggle behaviour stay there. This file
   adds only the technique controls surfaced to the user.
2. **Every slide gesture is under user control.** No auto-slides
   between notes without a gesture trigger.
3. **Slide is a continuous position, not a note.** Position updates
   at control rate; the underlying string picks up whatever pitch
   corresponds.
4. **Slide integrates with pitch bend.** MIDI pitch bend can serve
   as the slide position control if the user chooses.

## 1. New user controls

Added to the Techniques tab (see `gui-techniques-updates.md` 2):

- **Slide position source**: which control drives bar position.
  Options: modwheel, pitch bend, MPE Y-axis, expression pedal,
  custom CC, on-screen fretboard drag.
- **Position mode**: absolute (position 0-1 maps to fret 0-24) or
  relative (control adds to the current fretted position).
- **Slant control**: bar slant in degrees; source selectable (any
  CC).
- **Pressure control**: 0-1 bar-against-strings pressure; source
  selectable.
- **Contact string mask**: which strings the bar contacts. Default
  all; user can restrict to bass 3 or treble 3 for partial-slide
  playing.
- **Slide speed limit**: max cents-per-second the bar can move.
  Prevents unrealistic jumps. Default 4800 (= 1 octave per second).
  Advanced range extends higher for effect play.
- **Auto-vibrato on hold**: when position holds still for > 300 ms,
  a subtle bar vibrato engages automatically. Depth and rate
  configurable. Off by default.
- **Slide gesture trigger**: keyswitch or CC to start a scripted
  slide (from A to B over T milliseconds). Useful for tune-builder
  automation.

## 2. Scripted slide gestures

For composition and tune playback, slides can be scripted:
```
struct SlideGesture {
    double from_fret_continuous;
    double to_fret_continuous;
    double duration_ms;
    SlideCurve curve;  // linear, easeIn, easeOut, easeInOut
    double slant_start_deg;
    double slant_end_deg;
    double pressure;
};
```

Triggered by keyswitch, MIDI meta, or tune-builder. Runs on the
`SlideEngine` from slide-guitar.md; this spec just adds the
gesture-scripting layer.

## 3. Engine integration

No new engine module. Adds control inputs to the existing
`SlideEngine`:
- `SlideEngine::setPositionSource(SourceRef)` picks which control
  drives position.
- `SlideEngine::triggerGesture(SlideGesture)` starts a scripted
  slide.
- `SlideEngine::setSpeedLimit(centsPerSecond)`.

Every new field is optional; slide-guitar.md's existing behaviour
is unchanged if the user doesn't touch these.

## 4. GUI location

Techniques tab, Slide sub-tab (per gui-techniques-updates.md 2).
Also mirrored in Easy Mode's Playing strip: the existing slide
glyph gains a small "…" that opens a popover with the source and
mode selectors. Advanced Mode Col 3 SLIDE group gains the same
controls under an expandable section.

Slide Mode toggle behaviour from slide-guitar.md is unchanged.

## 5. Cascade compatibility

Per `technique-cascade.md`. Slide is compatible with muting
(muted-slide is a real technique), microtonal bends (slide plus
finger vibrato), and scraping if slide bar is lifted between them.

Slide is not compatible with two-hand tapping on the contacted
strings, or with slap.

## 6. Presets

- **Standard Slide (modwheel-driven)**: modwheel drives position,
  standard bar, pressure 0.7.
- **Pitch-Bend Slide**: pitch-bend range 24 (2 octaves) drives
  position for MIDI keyboard players.
- **Lap Steel Full Control**: MPE Y for position, MPE Z for
  pressure, aftertouch for slant.
- **Auto-Vibrato Hold**: as above plus auto-vibrato at 5 Hz, depth
  10 cents.

## 7. Tests

- Modwheel drives position through the mapped range.
- Speed limit clamps a full-throw modwheel jump to the configured
  cents-per-second.
- Scripted gesture reaches target position within duration ± 5 ms.
- Auto-vibrato engages after 300 ms of position hold.
- Position source swap mid-play does not click (crossfade over
  10 ms).
- Partial string mask: bass-only slide leaves treble strings
  freely pluckable.
- Preset save / restore round-trips every added control.
