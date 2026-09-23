# INPUT ROUTING SPEC

The order in which every input reaches every consumer. Covers MIDI in
(the most complex path), mouse events on overlapping regions, keyboard
shortcuts vs text-field focus, drag-drop from the OS, and host
transport events.

If two consumers want the same input, this document names the order
and the veto rules. Without this, every "why isn't MIDI Learn seeing
my CC" question is a guess.

## 0. Ground rules

1. **Every input has one path**, described here. Consumers do not
   read inputs from each other.
2. **The path is documented per input class** in the sections below.
3. **Every consumer can veto** (consume the event so downstream sees
   nothing) or pass through, per its documented rule.
4. **Focus wins**. A text field with focus consumes every keystroke;
   nothing downstream sees them.
5. **Inputs don't cross the audio boundary through unusual paths.**
   MIDI reaches the audio thread through the host's buffer; mouse
   events reach it (rarely) through the command queue.

## 1. MIDI in path

A MIDI event arrives in the plugin's `processBlock` inside a
`juce::MidiBuffer`. It walks the consumer chain in this fixed order:

```
[host MIDI buffer]
        |
        v
1. MidiExport pass-through (if MIDI-out enabled for pass-through):
   echoes the raw event to MIDI-out unchanged. Passes through.
        |
        v
2. MidiLearn (if armed):
   consumes the first non-note event and completes the mapping.
   Consumed events do not propagate further. Passes note events
   through unchanged.
        |
        v
3. Controller profile mapping (controllers.md):
   remaps channels, applies per-source latency compensation.
   Passes through with modified timestamps and channels.
        |
        v
4. MidiInterpreter:
   applies MPE / hex mode / standard, converts to internal events
   (NoteOnEvent, ChannelPressureEvent, PitchBendEvent, etc.).
        |
        v
5. TechniqueEngine:
   applies technique tags (palm mute, harmonic, tap) based on
   velocity zones, keyswitches, or aftertouch.
        |
        v
6. RhythmEngine (if enabled):
   rewrites the note stream into strums, fingerpicks, chord
   voicings. Consumes NoteOn / NoteOff events for held chords.
        |
        v
7. TuneBuilder (if playing):
   ignores incoming notes for playback (its own MIDI plays); accepts
   melody-record events if recording is armed.
        |
        v
8. Practice tools:
   scale trainer, ear trainer, tab reader read MIDI as user input
   when their mode is active. Do not consume; note whether it was a
   "correct" note.
        |
        v
9. StringEngine and downstream DSP.
```

### 1.1 Veto rules

- **MidiLearn armed** consumes the first non-note event. If the
  event is a note, it passes through (learn only accepts CC / PC /
  aftertouch / channel pressure by default; note learn is opt-in in
  Options).
- **RhythmEngine** consumes the notes it uses for chord detection
  (they don't reach StringEngine as literal notes); they emerge later
  as per-string PluckEvents.
- **TechniqueEngine** does not consume; it tags.

### 1.2 CC routing

CCs travel the same path but with additional consumers between
Controller Profile and MidiInterpreter:
- **Macro assignments** (mod matrix CC sources): each assigned CC
  updates its macro; passes through.
- **MIDI Learn parameter mappings** (post-learn): each mapped CC
  writes to its parameter; passes through.
- **Expression pedal calibration** (Options): the calibrated CC value
  is remapped to 0.0-1.0 before it reaches consumers; the raw
  original still passes through (rare).

CCs never reach the string engine or rhythm engine directly.

### 1.3 Pitch bend, aftertouch, channel pressure

Pitch bend and per-channel aftertouch go via MidiInterpreter into
per-note bend and vibrato depth. Channel pressure with a mapping
routes to whatever the mapping targets (commonly vibrato depth).

### 1.4 Program change

PC routes to snapshot recall per live-performance.md 2. Bank Select
(CC 0) selects preset if the routing panel's PC mapping is set to
"Bank + PC". PC never reaches the string engine.

### 1.5 SysEx

Luthier-profile SysEx (midi-export.md 2) parses per event class and
dispatches to the relevant engine. Non-Luthier SysEx is ignored.

Broadcast SysEx to a running plugin can trigger character events,
workshop changes, ranges toggles: this is how a DAW automation lane
can drive the plugin's non-parameter state.

### 1.6 MIDI clock and transport

Clock, Start, Stop, Continue, Song Position Pointer: routed to
LivePerf tap tempo and RhythmEngine transport. Never affect audio
directly.

## 2. Mouse event path

Mouse events on the plugin window walk this order:

```
[host mouse event delivered to plugin editor]
        |
        v
1. Overlay hit-test (top of z-order):
   - Notification banner buttons.
   - Modal overlays (Options, Preset Browser, Workshop overlay in
     Easy Mode).
   - Popovers.
   - Tooltips (hover events only).
   Overlays consume events entirely if they hit; the rest of the
   stack sees nothing.
        |
        v
2. Live Strip (if active):
   snapshot buttons, morph knob, tap pad, kill pill, monitor level.
        |
        v
3. Header strip:
   preset controls, snapshot strip, mode toggle, workshop wrench,
   slide glyph, meters (hover only), utility icons, overflow.
        |
        v
4. Main area:
   4a. Easy Mode:
       - Guitar illustration (hit-test into part regions per
         guitar-illustration.md 13.1).
       - Rig strip cards.
       - Playing / Tone / Rhythm strips.
   4b. Advanced Mode:
       - Column 1-3 panels.
       - Column 4 tab strip.
       - Column 4 workspace (the active tab's content, including
         the Workshop bench when WORKSHOP is selected).
        |
        v
5. Practice drawer (if expanded).
        |
        v
6. Footer (link clicks only).
```

### 2.1 Veto rules for mouse

- Overlays consume every mouse event inside their bounds. Escape or
  click-outside dismisses them; those events are also consumed by
  the overlay's dismiss handler.
- Popovers dismiss on any mouse-down outside their bounds; the
  outside click is consumed by the popover-dismiss handler (it does
  not fall through to whatever was clicked).
- Tooltips dismiss on mouse-move to a new target; movement events
  are not consumed.
- On the guitar illustration (workshop bench), the hit test in
  section 13.1 of guitar-illustration.md is the veto rule for
  overlapping parts.

### 2.2 Drag events

Drags initiate on mouse-down on a draggable element. Consumers of a
drag:
- **Part cards** dragged over the illustration: illustration is the
  drop target; other panels do not accept the drop.
- **Pedals** dragged in an effects rack: only the rack accepts.
- **Snapshots** dragged in the snapshot strip: only the strip
  accepts.
- **Presets** dragged into the setlist: only the setlist accepts.
- **Mod matrix source cards** dragged over any control: any
  `AttachedKnob` / slider accepts.
- **Files from the OS** dropped onto the plugin: root drop-handler
  routes by file type (preset, guitar, tune, IR, WAV, MIDI).

Multi-consumer drag targets (mod source over an AttachedKnob that's
inside a Workshop popover) resolve by z-order: overlay wins if the
event is inside its bounds.

## 3. Keyboard event path

Keyboard events walk:

```
[host key event]
        |
        v
1. Text field with keyboard focus (if any):
   consumes every keystroke. Escape releases focus and passes the
   Escape event through (only Escape is passed).
        |
        v
2. Overlay with focus (Options, Preset Browser, Workshop overlay):
   handles Tab / Shift-Tab focus movement inside itself, Enter for
   default action, Escape for close. Everything else falls through.
        |
        v
3. Popover with focus:
   handles arrow keys for value adjustment on the popover's target
   control. Escape closes.
        |
        v
4. Global shortcut table (gui-integration.md 17):
   matches the current binding; consumes matched events.
        |
        v
5. Focus-target component:
   handles arrow keys for the focused knob / slider (value
   adjustment), Space for toggle, etc.
        |
        v
6. Unhandled: dropped.
```

### 3.1 Text-field focus rule

Every text input in the plugin (search boxes, chord-progression
input, part name fields, preset save dialog, value entry popovers)
has focus that consumes keystrokes. When one has focus:
- Every alphanumeric key inserts into the field.
- Escape releases focus and the Escape event continues through the
  chain (for closing an overlay).
- Enter commits the value (or moves to next field for multi-field
  forms) and releases focus.
- Tab / Shift-Tab move to next / previous field.

The plugin-global shortcuts (P for Panic, W for Workshop, etc.) are
inactive while a text field is focused. This is intentional: a user
typing "P F Am G" into the chord-progression field should not trigger
panic on the P.

### 3.2 Keyboard shortcut conflicts

- If two shortcuts share a key, the more-specific consumer wins
  (component focus > global).
- Rebindings in Options -> Accessibility replace defaults; conflicts
  detected at rebind time (a modal warns and requires resolution).

### 3.3 IME

International input methods (Chinese, Japanese, Korean, Arabic, etc.)
are honoured on text fields. During IME composition, no shortcuts
match; only the composition characters reach the field.

## 4. File drop path

Files dropped onto the plugin window walk:

```
[OS drag-drop event with file paths]
        |
        v
1. Overlay drop-handler (Preset Browser drop zone, Workshop drop
   zone, MIDI import zone). Overlays consume drops in their bounds.
        |
        v
2. Root drop-handler:
   routes by file extension:
   - .luthierpreset -> preset load.
   - .luthierguitar -> guitar load (into current preset).
   - .luthiertune  -> tune load.
   - .luthierpart  -> part-swap prompt (which slot?).
   - .luthierset   -> setlist load.
   - .luthierloop  -> looper import.
   - .luthiercontent -> content-update install (via Options).
   - .midprofile   -> MIDI profile load.
   - .mid / .midi  -> MIDI import (into tune / looper / session
     recorder; prompt if ambiguous).
   - .wav / .aiff / .flac -> IR import (if in Tone Match tab) or
     backing-track import (if in Practice drawer TRACK tab); else
     prompt.
   - .mp3          -> backing-track import (Practice drawer TRACK
     tab).
   Unknown extension: reject with a banner "Cannot open X: unknown
   file type."
```

Multi-file drops: batch import if the type is homogeneous
(30 presets = batch import into user preset folder). Mixed type
drops: prompt for how to handle.

## 5. Host transport events

Host transport (play, stop, pause, tempo change, time-signature
change, position change) reaches the plugin via
`AudioPlayHead::getPosition()` on every `processBlock`.

Consumers:
- **RhythmEngine** reads transport for pattern sync; on Start,
  begins from bar 0; on Stop, stops writing new events; on Position
  change, re-syncs to the new position at the next block.
- **TuneBuilder** reads transport for tune playback sync when host
  is playing; otherwise uses its own internal transport.
- **LivePerf** tap tempo defers to host tempo when host is playing.
- **Metronome** reads transport for its click grid.
- **Session recorder** records regardless of transport (records
  everything the plugin outputs).

Transport events never affect audio directly; they inform control
modules.

## 6. Sidechain audio input

Sidechain audio arrives in the plugin's input buffer on aux inputs
per routing-io.md 4. Consumers:
- **Envelope followers** (mod matrix) read the sidechain envelope.
- **Sidechain compressor** pedal (if placed in the rack) reads.
- **Sidechain-to-amp** toggle (routing-io.md 5) replaces the string
  engine's contribution into the amp with the sidechain.
- **EQ match** capture path reads.
- **Cab match** capture path reads.

Sidechain never routes to the main audio path unless one of the above
explicitly consumes and mixes.

## 7. Audio input (Standalone only)

In Standalone, the audio input is available for:
- Sidechain (as section 6).
- Sung / hummed melody capture (tune-builder.md 13).
- Backing-track capture (rare; user could route their DAW into
  Luthier).
- Practice trainer input (bend trainer, ear trainer feedback).

The plugin does not process the raw audio input through the string
engine (no pitch-to-MIDI conversion in v1).

## 8. Debug / test inputs

Options -> Diagnostics -> "Inject fixture MIDI" and "Inject fixture
audio" let the user replay a saved fixture through the plugin. These
inject events at the front of the chain (before section 1), so all
consumers see them normally.

## 9. Test schedule

Every consumer / veto rule listed has a test in
`Tests/InputRouting/`. Test types:
- MIDI: send an event, verify each consumer either received or
  vetoed as documented.
- Mouse: click a stack of overlapping regions, verify z-order.
- Keyboard: focus a text field, press a global shortcut key, verify
  the shortcut does not fire.
- File drop: drop each file type, verify the correct routing.
- Transport: start / stop / position-change, verify RhythmEngine
  and TuneBuilder both re-sync correctly.

Integration test: a 60-second scripted session with concurrent MIDI
in, sidechain audio, mouse edits and file drops; verify no consumer
missed its intended events and none of the vetoes were bypassed.
