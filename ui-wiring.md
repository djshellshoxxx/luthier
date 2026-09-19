# UI WIRING SPEC

How UI components attach to backend state. Written so a Claude Code
implementer never has to guess the pattern; every panel wires the same way.

If you are building a UI panel and this document does not answer a
question, the answer is in `gui-integration.md` (what the panel is) or
`engine.md` (what the module does). This file covers only the plumbing
between the two.

## 0. Ground rules

1. Every parameter lives in the single `AudioProcessorValueTreeState`
   (APVTS) owned by the processor. There is no side-channel state for
   audible behaviour. Ever.
2. UI components attach to APVTS via JUCE's `SliderAttachment`,
   `ButtonAttachment`, `ComboBoxAttachment`. Never write to a parameter
   through a raw pointer set from the UI.
3. Non-parameter UI state (which tab is selected, which snapshot slot is
   showing) lives in a `ValueTree` called `uiState`, saved alongside the
   preset in the same XML/binary blob.
4. Every listener the UI attaches is removed in the component's
   destructor. No exceptions. Leaked listeners crash on preset load.
5. Message-thread work stays on the message thread. Audio-thread work
   stays on the audio thread. Cross the boundary only through the
   documented mechanisms in section 4.

## 1. Parameter definition contract

Every parameter is declared once, in `ParameterIDs.h` (constants) and
`ParameterLayout.cpp` (the layout the APVTS is constructed from).

Each parameter has:
- **ID**: stable, snake_case, versioned when semantics change (e.g. `amp_gain_v2`).
- **Display name**: human-readable, translated via the i18n catalog.
- **Range**: min, max, step, skew (log skew for time, frequency).
- **Default**: the value on a fresh preset.
- **Unit**: from a shared enum (Hz, dB, ms, %, semitones, cents, ratio,
  count, index, boolean).
- **Text -> value and value -> text** functions: for right-click value
  entry and automation display.
- **Category tag**: which section of the UI owns it (mirror of the
  feature-to-location index in `gui-integration.md`).

The 342 parameters PROGRESS.md reports are canonical. Any new parameter
increments the count and appears in the index.

## 2. UI component base classes

Three shared base classes handle 95% of the plumbing:

### `AttachedKnob`
- Wraps a `juce::Slider` in rotary mode with the theme knob look.
- Constructor takes APVTS reference and parameter ID.
- Owns its own `SliderAttachment`.
- Renders the value arc, mod arc (when routes exist), label and value.
- Exposes `setLabel`, `setSize` (small/medium/large per theme.md),
  `setColour` (for grouping panels).
- Provides right-click menu per gui-integration.md section 15.

### `AttachedSwitch`
- Wraps `juce::TextButton` in toggle mode.
- Same attachment pattern.
- Momentary vs latching per constructor argument.

### `AttachedCombo`
- Wraps `juce::ComboBox`.
- Populates from a `juce::StringArray` supplied at construction; for
  parameter changes (adding an amp model), the combo listens for
  parameter metadata changes and rebuilds.

Every panel uses these three plus a small set of custom widgets (fretboard,
step grid, meter). Custom widgets follow the same attachment pattern
where they represent a parameter, and use the `uiState` ValueTree for
non-parameter state.

## 3. Panel structure

Every panel is a `juce::Component` subclass named `<Name>Panel`. Convention:

```
class AmpPanel : public juce::Component,
                 private juce::ValueTree::Listener
{
public:
    AmpPanel (LuthierProcessor&);
    ~AmpPanel() override;   // removes all listeners

    void paint (juce::Graphics&) override;
    void resized() override;

    // Panel API used by the workspace container:
    static juce::String getPanelId();   // stable id for uiState
    juce::String getDisplayName() const;
    bool isCollapsed() const;
    void setCollapsed (bool);

private:
    LuthierProcessor& processor;
    juce::OwnedArray<AttachedKnob> knobs;
    juce::OwnedArray<AttachedSwitch> switches;
    juce::OwnedArray<AttachedCombo> combos;
    // ... custom widgets

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
};
```

Panels never own DSP state. They read parameter values through the
attachment or the parameter tree; they read display state (meters, chord
symbol readout, "current voice" indicators) through a lock-free FIFO
described in section 4.

## 4. Audio-to-UI communication

The audio thread never touches UI. UI never touches audio-owned state.
Communication crosses through three channels:

### 4.1 APVTS parameters (bidirectional)
- Automation writes reach both threads with the standard APVTS lock-free
  atomics.
- UI reads happen on the message thread via `getRawParameterValue`.

### 4.2 The display FIFO (audio -> UI)
- A `juce::AbstractFifo` per subsystem carries display data: meter values,
  detected chord, mod-source current values, active-voice count,
  per-string activity.
- Structure: fixed-size ring buffer of `DisplaySample` records. The audio
  thread writes at block rate (or lower, throttled per subsystem);
  message thread drains at 30 Hz for meters, 60 Hz for the fretboard,
  10 Hz for chord symbol and voice count.
- Never blocks. Never allocates.

### 4.3 The command queue (UI -> audio, for non-parameter changes)
- A single-producer single-consumer lock-free queue.
- Used for structural changes that are not parameters: adding a mod
  route, changing a bus layout, loading an IR, arming MIDI Learn.
- Commands are small POD structs; anything with heap ownership (a new IR
  buffer, a new mod-matrix state) is passed by pointer to a memory pool
  the audio thread swaps in and returns the old pointer to for
  destruction on the message thread.

## 5. Preset load and snapshot recall

Preset load:
1. Message thread: parse preset file into an in-memory `PresetData`.
2. Post a `LoadPresetCommand` to the audio thread with the pointer.
3. Audio thread: at the next block boundary, applies the parameter values
   to the APVTS (which fires attachment updates on the message thread on
   the next repaint) and swaps any structural state (mod-matrix, patterns,
   IRs) in one atomic pointer swap per subsystem.
4. Audio thread posts a `PresetLoadedResult` back with the old state
   pointers.
5. Message thread: releases the old state.

Snapshot recall follows the same pattern. Because snapshots are stored
alongside the preset (see live-performance.md), the recall path never
touches disk.

UI never blocks on a preset load: while the swap is in flight, the UI
shows the previous state; the update arrives when the audio thread has
completed the swap.

## 6. MIDI Learn

Arming:
1. User clicks MIDI Learn in the header (or presses Ctrl+L).
2. UI enters "arm" mode: the next control the user right-clicks receives
   an `arm` flag.
3. The audio thread listens for the next non-note MIDI event on any
   channel; the first one becomes the mapping.
4. UI updates to show the mapping.

Storage: mappings live in the preset (per gui-integration.md section 15)
or, if the user chose "Save as global", in the plugin's user-global
settings file.

The mapping table itself is a `std::vector<MidiMapping>` owned by the
audio thread; the UI reads a snapshot via the display FIFO for the MIDI
Learn overlay list.

## 7. Meters and indicators

Every meter follows the same pattern:
- Audio-thread module computes peak / RMS per block into a `MeterState`
  struct.
- Struct is written to the display FIFO.
- UI component drains and paints at 30 Hz.
- Peak hold is computed on the UI side from the drained values (audio
  side sends raw peaks only).

Every "LED" indicator (output LED, snapshot active, bypass state) reads
either a parameter (for bypass) or a display FIFO field (for output).

## 8. The scrolling data stream

The "matrix stream" in empty panel areas per theme.md.

Source: the display FIFO carries a `LogSample` field per subsystem tick
containing short, human-readable strings like "note on 62 string 3" or
"mod route lfo1 -> amp_gain +12%". A `LogStream` component subscribes to
a filter of these, ring-buffers the last 200 lines, and paints them with
the alpha-fade per theme.md.

The stream stops when no lines have arrived for 500 ms and resumes on
the next arrival, per theme.md.

If reduced-motion is on, the stream is disabled entirely.

## 9. Modulation UI wiring

The mod matrix subscribes to a snapshot of source current values and
route depths via the display FIFO at 30 Hz for the mod-arc rendering. The
route table itself is edited via commands to the audio thread; the audio
thread returns the new state via the swap pattern in section 5.

Dragging a source card onto a control:
1. UI captures the source ID at drag start.
2. On drop, UI resolves the drop target's parameter ID via a hit test on
   `AttachedKnob` / `AttachedSlider` under the mouse.
3. UI sends `CreateRouteCommand { source_id, destination_param_id, depth: 0.25 }`.
4. Audio thread applies, returns updated matrix snapshot.

Drag preview is a ghost of the source card following the cursor with a
subtle glow in the source's colour.

## 10. Guitar illustration

The `GuitarIllustration` component:
- Reads `GuitarSpec` from the processor via a subscription (rebuilt on
  spec change through the command / result pattern).
- Renders a scalable vector illustration procedurally.
- Overlays hit regions defined per gui-integration.md section 3.1.
- Reads active fret / string data from the display FIFO to draw played
  notes in real time.

The illustration is not a static asset. It responds to instrument change,
capo change, pickup drag, whammy assignment change.

## 11. Fretboard component

Used in three places (guitar illustration, scale trainer, tab reader) and
shares one component subclass with mode flags.

Data source:
- Static: from `GuitarSpec` (string count, fret count, tuning).
- Dynamic: per-string activity from the display FIFO.

Modes:
- Play display (default).
- Scale highlight (colours degrees or intervals).
- Quiz (single fret highlighted, hides others).
- Tab reader (moving cursor with upcoming notes).

Every mode uses the same paint routine with different data layers.

## 12. Undo/redo wiring

Every parameter change captured by APVTS emits an "undoable action" into
the undo manager. Structural changes (mod routes, patterns) push a
compound action.

The undo manager is per-plugin-instance and owned by the processor. UI
attaches to it as a listener for enable/disable of undo/redo shortcuts
and for the "undo/redo" tooltip that shows the last action name.

## 13. Threading contract summary

| Thread | Owns | Reads from other |
|---|---|---|
| Audio | DSP state, meters, parameter atomics | APVTS values, command queue |
| Message (UI) | Component tree, uiState VT | APVTS values, display FIFO |
| Worker (loads, exports, matches) | Temporary buffers | File system, sends via command queue |

The worker thread pool is a shared `juce::ThreadPool` with 2 threads.
Long tasks (cab match, notation export, IR resample) run there.

## 14. Localization wiring

Every user-visible string comes from `LocaleCatalog::get(id)`. Panels do
not hold string literals except as parameter IDs.

Locale change:
1. User picks locale in Options -> Localization.
2. Catalog reloads.
3. Every component listening (via a `LocaleChanged` broadcaster) receives
   the change and calls its own `refreshStrings()` implementation.
4. Layout re-runs on affected components.

No plugin restart required.

## 15. Accessibility wiring

Every attachable component sets its accessibility handler in its
constructor:
- `AccessibleRole::slider` for knobs and sliders.
- `AccessibleRole::button` for buttons and toggles.
- `AccessibleRole::comboBox` for dropdowns.
- Custom widgets (fretboard, step grid) provide `AccessibilityHandler`
  subclasses with per-child accessibility per accessibility.md section 1.

Label text for accessibility comes from the same locale catalog as
visible labels; value announcements use the parameter's value -> text
function.

## 16. Save on close

The processor's `getStateInformation` serializes:
- APVTS state (parameters).
- `uiState` VT (which tab, which mode, snapshot indices, expression cal).
- Mod matrix, snapshot bank, setlist reference, MIDI mappings.

`setStateInformation` restores all of the above, using the swap pattern
for structural state.

Preset files use the same serializer with the plugin-instance-specific
UI state stripped.

## 17. Tests

- Attachment leak test: instantiate every panel 100 times, verify no
  listener remains attached after destruction.
- Cross-thread invariant: run with JUCE's message manager lock detector
  through a 60-second synthetic session, verify no audio-thread access to
  UI-owned state.
- Preset load determinism: load 20 presets in random order 10 times each,
  verify final DSP state matches offline render of the same sequence.
- Undo/redo: 1000 random parameter changes with random undo/redo
  interleaved, verify final state matches the equivalent forward-only
  sequence.
- MIDI Learn: arm, send each of 128 CCs, verify each maps to the intended
  parameter within one block.
- Display FIFO overflow: fire 10x the drain rate for 60 s, verify no
  audio-thread stalls and no crashes (oldest data dropped silently).
