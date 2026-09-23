# MICROTONAL BENDS SPEC

Guitar is one of the most microtonal instruments in common use.
Bends slide the pitch continuously; vibrato does subtle
quarter-tone wobbles; pre-bends release into pitch. This spec
formalises user-modifiable microtonal control beyond the standard
MIDI pitch-bend range.

## 0. Ground rules

1. **Pitch is continuous per string.** No fixed semitone grid.
2. **Bend sources are user-selectable.** MIDI pitch bend, MPE Y-axis,
   aftertouch, expression pedal, custom CC, on-screen fretboard drag.
3. **Multiple bend sources can layer.** Global source (all strings)
   plus per-string source (MPE) plus vibrato source add together.
4. **Bend ranges are per-string configurable.** A whole-tone bend
   on the G is one throw; a semitone on the low E is another.

## 1. Bend sources

Each string has an ordered stack of bend sources:
1. **Global bend**: applies to all strings equally.
2. **Per-string bend**: applies to one string (MPE per-note pitch
   bend by default).
3. **Vibrato**: cyclic modulation on top of the current bent
   position.
4. **Pre-bend release**: scripted event that starts at a bent
   position and releases toward nominal.
5. **Slide contribution**: if Slide Mode is on, slide position adds
   to the bend stack.

Each source has a scale factor (cents per source unit) and an
optional latency compensation.

## 2. User controls

Techniques tab, Microtonal Bends sub-tab:

- **Global bend source**: MIDI pitch bend, expression, CC (dropdown).
- **Global bend range** (cents): default 200 (whole tone).
  Advanced range up to 2400 (2 octaves).
- **Per-string bend source**: MPE Y (default), custom CC per string,
  none.
- **Per-string bend range**: 6 sliders. Defaults 200 cents each.
- **Vibrato source**: LFO (rate + depth), aftertouch, or MPE Z.
- **Vibrato rate**: 3-10 Hz default 6.
- **Vibrato depth**: 5-50 cents default 20.
- **Vibrato onset delay**: how long after a note-on before vibrato
  engages. Default 200 ms.
- **Bend quantise**: none (default), quarter-tone grid, semitone
  grid, 24-EDO grid, custom scale.
- **Custom bend scale**: load a .scala or .tun file for micro-tonal
  scale grids.
- **Pre-bend**: keyswitch or CC triggers a pre-bent note-on. Amount
  configurable (default -200 cents = start a whole tone flat).
- **Bend curve**: linear, exponential (matches finger mechanics),
  or user-drawn envelope shape.
- **Release curve**: same options for the release side of a bend
  and vibrato onset.

## 3. Bend quantise (microtonal scales)

By default, bends are continuous. Users making Middle Eastern,
Indian, Turkish, or bespoke microtonal music can quantise to a
scale grid:
- Quarter-tone (24-EDO): every 50 cents.
- 22-EDO, 31-EDO, 53-EDO: presets.
- Custom .scala / .tun file: any scale.

Quantise mode: bends snap to the nearest scale degree with a
configurable snap strength (0 = pure snap, 1 = only slight
attraction).

## 4. Engine integration

No new engine module. Uses `StringEngine`'s existing per-string
pitch offset input, and `ModulationMatrix`'s existing per-source
signal chain.

Additions:
- `StringEngine::setBendSourceStack(string, sources[])` configures
  which sources contribute to a string's pitch.
- `StringEngine::setBendQuantise(mode, snap_strength)`.
- A new modulation source class `PreBendEvent` in
  `modulation-matrix.md`.

Per-string bend range change takes effect at next note-on to avoid
mid-note pitch jumps.

## 5. GUI location

- **Techniques tab, Microtonal Bends sub-tab**: full controls.
- **Easy Mode Playing strip**: existing bend / vibrato indicator
  gains a small "…" to open a compact popover with global bend
  range and vibrato settings.
- **Advanced Mode Col 3 CHARACTER PLAYING group**: gains a
  "Microtonal" section.
- **Fretboard illustration**: bent notes render with a small
  cents-offset badge; heavy bends push the played-note dot along
  the fret gap toward the target pitch.

## 6. Cascade compatibility

- Bends + muting: fully compatible.
- Bends + slap: bent slap notes (bass technique).
- Bends + slide: bend adds on top of slide position.
- Bends + tapping: fully compatible; tap-and-bend is standard.
- Bends + scraping: bend during scrape shifts the entire catch
  stream in pitch.

Everything cascades cleanly with bends because bends are a pitch
offset, not a competing control channel.

## 7. Presets

- **Standard Whole-Tone Bend**: global range 200, per-string 200,
  no quantise.
- **Quarter-Tone Blues**: quantise quarter-tone with snap 0.3
  (light attraction).
- **Middle Eastern Maqam**: quantise 24-EDO, snap 0.8.
- **Wide Vibrato**: vibrato depth 40, rate 5, onset 100 ms.
- **Whammy-Style Two-Octave**: global range 2400, expression pedal
  drives it.

## 8. MIDI export

- Luthier profile: preserves per-source bend contributions and
  quantise settings as SysEx alongside standard pitch-bend messages.
- Generic profile: encodes as standard pitch bend on the note's
  channel; per-string bends split to per-channel (MPE style).

## 9. Tests

- Global bend at +100 cents: every ringing note reads +100 cents.
- Per-string bend on string 3 at +200 cents: only string 3 shifts;
  others unchanged.
- Vibrato onset delay: no pitch modulation for the configured delay
  after note-on, then engages.
- Quarter-tone quantise snap 1.0: every bend held pitch lands on
  the nearest 50-cent step.
- Custom scale .scala: bend pitches match the loaded scale within
  0.5 cents.
- Pre-bend keyswitch: next note-on starts -200 cents flat, releases
  to nominal.
- Bend range change mid-note does not cause a pitch jump.
- Vibrato depth reaches configured amplitude within 50 ms of onset.
- Cascade: bend + slap on low E: bent slap event pitches correctly.
- Preset save / restore round-trips every added field.
