# PIANO ROLL AND CHORD DISPLAY SPEC

A small piano roll that mirrors what the guitar plays, and plays the guitar
from the keys; and a chord name that appears on the guitar illustration as
a chord or note sounds, then fades.

Two audiences:
- Guitarists: see the notes and chord names of what they are playing.
- Piano players who do not play guitar: play keys, hear a real guitar
  voicing, and see where it lies on the fretboard.

Added 2026-09-24 at the product owner's request. Additive to
`gui-integration.md`; it moves nothing that is already placed.

## 0. Ground rules

1. **Display-only on the audio path.** Reading notes for display never
   allocates or locks on the audio thread. Playing from the keys uses
   the same MIDI path as the existing on-screen keyboard (input-routing.md
   consumer order), so everything downstream (voicer, rhythm engine,
   techniques, capture, MIDI out) treats it as ordinary MIDI.
2. **What sounds is what shows.** The roll lights a key for every note a
   string is actually sounding, including notes the voicer or rhythm engine
   placed. It does not only show what the player pressed. The source is
   the engine's string activity (routing-io 6), the stream the capture and
   MIDI out already use.
3. **Both toggles live in Options**, next to the tooltips switch, and are
   remembered per user (UiPreferences) like it.

## 1. The piano roll

A strip with two parts:
- **Keyboard:** a horizontal piano keyboard, 18-28 px tall.
- **Roll:** above the keyboard, the last 4 seconds of notes as horizontal
  bars scrolling right to left, 40-80 px tall.

**Range.** Follows the instrument: from the lowest open string (after
tuning and capo) to the highest string's last fret, padded to whole
octaves. That is E2-E6 on a standard guitar and B0-G4 on a 5-string bass.
The keys outside the playable range are drawn dimmed but still work.

**Colours.** A key lit by a string takes that string's colour (the same
per-string palette the fretboard uses), at 85% opacity. A held key stays
lit. On release, the key fades over 150 ms. Roll bars use the string
colour at 60% opacity, and their length is the note's duration.

**Octave labels.** Each C is labelled C1, C2, and so on, in small text.
Middle C (C4) is marked.

**Location:**
- **Advanced mode:** a collapsible strip directly under the fretboard in
  column 2, full column width. It opens at 72 px (keyboard 24 + roll 48)
  and a drag handle sizes it from 40 to 140 px. The collapsed or expanded
  state is saved in UiState.
- **Easy mode:** a strip under the guitar illustration, 56 px (keyboard
  only plus a 32 px roll).

The strip's own header holds three controls (section 3):
- **ROLL** / **KEYS** toggle: keys only, or keys plus roll.
- **Latch.**
- **Show fingering.**

## 2. Guitar to piano (mirroring)

Every note a string sounds lights its key and starts a roll bar at the
moment it starts, and the bar ends at note-off.

The data flow is in the style of gui-engine-dataflow.md:
- **Source:** a new `SoundingNotes` snapshot. The audio thread publishes
  it after each block: a 128-bit set of sounding MIDI notes, plus, per
  string, the note (-1 for none) and the note's start sample. It is
  double-buffered with an atomic sequence number. The audio thread only
  stores; it never waits.
- **UI:** drained by a 30 Hz UI timer.
- **Staleness:** if nothing has been published for 250 ms (transport
  stopped, plugin bypassed), no keys are lit.

A slide or bend moves the lit key when the pitch crosses to the next
semitone. The key is the nearest semitone, and the roll bar is drawn with
a small pitch-offset tick when the note is bent more than 20 cents.

## 3. Piano to guitar (playing from the keys)

**Click or drag.** Clicking a key plays it. Dragging across keys plays a
glissando: note-off for the old key, note-on for the new one. Velocity
comes from the vertical click position on the key (top 40, bottom 110),
like JUCE's MidiKeyboardComponent. The notes go into the processor's
existing UI keyboard state and reach the engine as MIDI channel 1 notes.
The MidiInterpreter assigns the string and fret, and the fretboard shows
where they landed.

**Latch.** With Latch on, clicks toggle keys into a held set instead of
playing them. The **Play** button (or Enter while the strip has focus)
sends the held set as one chord. The rhythm engine strums it if it is on;
otherwise it is a strummed chord at the global crossing speed
(strum-dynamics.md). **Clear** (or Escape) empties the set.

**Show fingering.** With it on, the latched set, or the held keys without
Latch, is voiced with the same voicer the engine uses
(`RubricVoicer`, ambiguity-resolutions 4). The resulting fingering is
drawn on the fretboard as hollow "ghost" dots before anything is played,
so a pianist can see the guitar shape. It uses the fretboard's existing
overlay layer. A note that no fingering can reach (below the lowest string)
is shown on the key with a small x and a tooltip: "below this guitar's
range".

**Computer keyboard.** When the strip has keyboard focus, the standard
two-row layout (A-L white keys, W-P black keys, Z / X octave down / up)
plays notes. This only applies while the strip is focused, so the global
shortcuts (accessibility 9) are not stolen.

## 4. Chord name on the guitar

When notes start sounding, the chord or note name appears on the guitar
illustration, semi-transparent, then fades.

**Naming.** Built from the set of sounding pitch classes, using the
existing `ChordDetector`:
- **One pitch class:** the note name without an octave, e.g. "G#".
  Spelling (sharp or flat) follows the tune's key signature when a tune
  is loaded, and otherwise uses sharps, except for Bb and Eb, which are
  always flats.
- **Two pitch classes:** a power chord is written "E5". Any other
  interval shows both notes, e.g. "C E".
- **Three or more:** the detected chord symbol, e.g. "Am7", "G/B",
  "Cadd9". If detection confidence is below its floor, the pitch classes
  are listed instead, e.g. "C D F#".

**Placement.** Centred over the lower bout, the tail end of the body
("the end of the guitar"), inside the illustration's bounds. This applies
in both Easy and Advanced, wherever `GuitarBodyComponent` is drawn.
- Font: the display face (theme.md / visual-polish).
- Size: 12% of the illustration's height, clamped to 28-96 px.
- Colour: the theme's text colour.
- Peak opacity: 0.35, so the guitar reads through it.

**Timing:**
- **Fade in:** 60 ms from the first note of the chord.
- **Merging notes:** notes arriving within ChordDetector's 30 ms burst
  window (a strum) update one name. They do not restart it.
- **Hold:** at peak while the notes sound, up to 1.2 s.
- **Fade out:** over 0.8 s, to 0.
- **New chord:** a new chord or note replaces the old one with a 60 ms
  crossfade.
- **Legato or bend:** the name updates in place without flashing.

**Reduced motion** (accessibility 5): no fades. The name appears at peak
opacity and disappears after the hold.

**Screen reader:** off by default. The Options page has "Announce chord
names" under the main toggle, which sends the name as a polite
announcement, rate-limited to one every 1.5 s.

## 5. Options

Options -> APPEARANCE (the page with "Show tooltips"; there is no
"General" page: output-normalization.md 5.1 checked the page names), a new section,
"Visual aids":
- **Show chord names on the guitar** (default on).
- **Announce chord names** (default off; enabled only when the one above
  is on).
- **Show piano roll** (default on in Advanced, off in Easy; one switch per
  mode, because Easy has less room).
- **Piano roll shows** Keys / Keys + Roll (default Keys + Roll).

All four are user preferences (UiPreferences, `ui_prefs` file), saved
immediately, like the tooltips switch. They are not preset data and not
parameters: they change what is shown, not the sound.

## 6. State

`UiState` gains:
- `pianoRollExpanded` (bool)
- `pianoRollHeight` (int)
- `pianoLatch` (bool)
- `pianoShowFingering` (bool)

The latched set is session state (state-model.md): kept in the plugin
state, not in presets.

Undo: none of this is undoable (display and performance), per
action-and-undo.md.

## 7. Performance

- **Audio thread:** publishing the snapshot is at most 12 stores and one
  atomic increment per block.
- **UI:** the roll repaints only its dirty region at 30 Hz. It stays
  within gui-engine-dataflow.md's 2 ms live-overlay budget at
  1920x1080.
- **Chord name:** drawn in the illustration's live overlay pass
  (guitar-illustration.md 2.2), not in the cached static scene.

## 8. Tests

- **PR-01:** A chord played on the guitar lights exactly its sounding
  keys within 2 UI frames, in the colours of the strings that sound
  them. That includes notes the voicer added.
- **PR-02:** Releasing clears the keys. After 200 ms none are lit.
- **PR-03:** Clicking a key produces a note-on for that MIDI note on the
  processor's keyboard state, and the engine sounds it (string activity
  shows it).
- **PR-04:** Latch plus three keys (C E G) plus Play sounds a strummed C
  major: three pitch classes sound, with one note-on per voiced string.
- **PR-05:** Show fingering draws ghost dots matching RubricVoicer's
  voicing for the latched set, before any note sounds.
- **PR-06:** The range follows tuning and capo. Drop D lowers the lowest
  key to D2, and capo 2 raises it by two semitones.
- **PR-07:** No allocation on the audio thread from the snapshot publish
  (allocation counter).
- **CD-01:** A single G#3 shows "G#". A strummed G major shows "G". An
  Am7 shows "Am7". An E5 power chord shows "E5".
- **CD-02:** The name reaches peak opacity 0.35 ±0.02 within 80 ms. It
  holds while the notes sound, up to 1.2 s, and reaches 0 by 2.1 s.
- **CD-03:** A strum spread over 25 ms produces one name, not a name per
  string.
- **CD-04:** With the option off, nothing is drawn and no chord detection
  runs.
- **CD-05:** With reduced motion on, there are no intermediate opacities.
- **OP-01:** The four options persist across editor close and reopen,
  and across processor instances (user prefs), and are not written into
  presets.
- **OP-02:** Every control named here is reachable in both modes, with
  keyboard focus and screen-reader labels (accessibility.md).
