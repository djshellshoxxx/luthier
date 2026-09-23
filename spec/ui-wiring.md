# UI WIRING SPEC

How UI components attach to backend state. If you are building a UI
panel and this document does not answer a question, the answer is in
`gui-integration.md` (what the panel is) or `engine.md` (what the module
does). This file covers only the plumbing between the two, plus the
patterns the realism specs need: physical-parameter wrapping, the
`GuitarSpec` swap flow, shadow specs for Workshop audition, and the
NoiseEngine pool.

## 0. Ground rules

1. Every parameter lives in the single `AudioProcessorValueTreeState`
   (APVTS) owned by the processor. There is no side-channel state for
   audible behaviour.
2. UI components attach to APVTS via `SliderAttachment`,
   `ButtonAttachment`, `ComboBoxAttachment`. Never write to a parameter
   through a raw pointer set from the UI.
3. Non-parameter UI state (which tab is selected, which snapshot slot is
   showing, Workshop A / B slot, Slide Mode on/off) lives in a
   `ValueTree` called `uiState`, saved alongside the preset.
4. Every listener the UI attaches is removed in the component's
   destructor.
5. Message-thread work stays on the message thread. Audio-thread work
   stays on the audio thread. Cross the boundary only through the
   documented mechanisms in section 4.
6. Structural state (mod matrix, patterns, IRs, `GuitarSpec`, parts,
   snapshot bank) never flows through parameters. It flows through the
   command / result queue with atomic pointer swaps.

## 1. Parameter definition contract

Every parameter is declared once, in `ParameterIDs.h` (constants) and
`ParameterLayout.cpp` (the layout the APVTS is built from).

Each parameter has:
- **ID**: stable, snake_case, versioned when semantics change.
- **Display name**: human-readable, translated via the i18n catalog.
- **Range**: for a physical parameter, this is a `PhysicalRange`
  (advanced-ranges.md), i.e. a pair of ranges (stock, advanced) plus a
  default and unit. For a non-physical parameter, a single range as
  before.
- **Default**.
- **Unit**: from the shared unit enum (Hz, dB, ms, %, semitones, cents,
  H, F, ohm, mm, g, count, index, boolean).
- **Text -> value / value -> text** functions.
- **Category tag**: matches the feature-to-location index in
  `gui-integration.md`.

`PhysicalRange` is the wrapper for the realism specs: at construction
time the parameter's live min / max are set from the stock range; when
the preset's `ranges` block flips a family to advanced, the parameter
switches to the wider min / max in place, and the current value is left
alone unless it was already at a clamped boundary. See
`advanced-ranges.md` for the semantics; this file covers only the
wiring.

The 342 parameters PROGRESS.md reports are the baseline; realism specs
add more as they land. Any new parameter appears in the feature-to-
location index.

## 2. UI component base classes

Four shared base classes handle 95% of the plumbing.

### `AttachedKnob`
- Wraps `juce::Slider` in rotary mode with the theme knob look.
- Constructor takes APVTS reference and parameter ID.
- Owns its own `SliderAttachment`.
- Renders the value arc, mod arc (when routes exist), label and value.
- If the parameter is a `PhysicalRange`, the arc portion past the stock
  max is drawn in the warning colour and the readout gains a `*` suffix
  (gui-integration.md 21).
- Exposes `setLabel`, `setSize` (small / medium / large per theme.md),
  `setColour` (for grouping panels).
- Provides the right-click menu per gui-integration.md 16, including
  the two per-control range-unlock items when the preset is locked.

### `AttachedSwitch`
- Wraps `juce::TextButton` in toggle mode.
- Same attachment pattern.
- Momentary vs latching per constructor argument.

### `AttachedCombo`
- Wraps `juce::ComboBox`.
- Populates from a `StringArray` at construction; for parameter-driven
  option changes (adding an amp model, adding a pickup type when the
  parts library changes), the combo listens for parameter metadata
  changes and rebuilds.

### `PartSlotWidget`
- New: represents a slot in a `GuitarSpec` (a pickup slot, the bridge,
  the nut, a string index).
- Not backed by APVTS; backed by `uiState` (which part is selected) and
  the command queue (a swap writes a new `GuitarSpec`).
- Renders the card summary or the illustration hit region depending on
  where it lives.

Every panel uses these four plus a small set of custom widgets
(fretboard, step grid, meter, spectrum-delta pane, buzz heatmap, noise-
event strip).

## 3. Panel structure

Every panel is a `juce::Component` subclass named `<Name>Panel`:

```
class AmpPanel : public juce::Component,
                 private juce::ValueTree::Listener
{
public:
    AmpPanel (LuthierProcessor&);
    ~AmpPanel() override;   // removes all listeners

    void paint (juce::Graphics&) override;
    void resized() override;

    static juce::String getPanelId();
    juce::String getDisplayName() const;
    bool isCollapsed() const;
    void setCollapsed (bool);

private:
    LuthierProcessor& processor;
    juce::OwnedArray<AttachedKnob> knobs;
    juce::OwnedArray<AttachedSwitch> switches;
    juce::OwnedArray<AttachedCombo> combos;

    void valueTreePropertyChanged (juce::ValueTree&,
                                   const juce::Identifier&) override;
};
```

Panels never own DSP state. They read parameter values through the
attachment or the parameter tree; they read display state (meters,
chord symbol, "current voice", noise events, buzz map) through the
display FIFO in section 4.

## 4. Audio-to-UI communication

The audio thread never touches UI. UI never touches audio-owned state.

### 4.1 APVTS parameters (bidirectional)
Automation reaches both threads through the standard APVTS lock-free
atomics. UI reads on the message thread via `getRawParameterValue`.

### 4.2 The display FIFO (audio -> UI)
A `juce::AbstractFifo` per subsystem carries display data: meters,
chord symbol, mod-source values, active-voice count, per-string
activity, per-string amplitude (for the buzz heatmap), noise events (a
tagged event class: squeak, buzz, pick click, pick chirp, clank, slide
noise, LH slap, ghost).

Ring buffer of fixed-size `DisplaySample` records. Audio thread writes
at block rate or lower (throttled per subsystem); message thread drains
at 30 Hz for meters, 60 Hz for fretboard, 10 Hz for chord symbol and
voice count, 15 Hz for the noise-event strip.

Never blocks. Never allocates.

### 4.3 The command / result queue (UI -> audio, structural changes)
A single-producer single-consumer lock-free queue. Small POD commands;
anything with heap ownership (a new IR, a new mod-matrix state, a new
`GuitarSpec`, a new part) is passed by pointer to a memory pool. The
audio thread swaps in the new pointer at the next block boundary and
posts the old pointer back for message-thread destruction.

Commands:
- `LoadPresetCommand { pointer to PresetData }`
- `RecallSnapshotCommand { index }`
- `LoadGuitarCommand { pointer to GuitarSpec }`
- `SwapPartCommand { slot_id, pointer to new part struct }`
- `ShadowAuditionCommand { slot_id, pointer to new part, on / off }`
- `AddModRouteCommand { source_id, dest_param_id, depth }`
- `LoadIrCommand { slot_id, pointer to Ir buffer }`
- `ArmMidiLearnCommand { on / off, param_id }`

## 5. Preset load and snapshot recall

Preset load:
1. Message thread: parse preset into `PresetData` (includes the
   `ranges` block).
2. Post `LoadPresetCommand`.
3. Audio thread: at next block boundary, apply parameter values to APVTS
   (which fires attachment updates on the message thread), swap
   structural state (mod matrix, patterns, IRs, `GuitarSpec` reference,
   `ranges`) in atomic pointer swaps per subsystem, then apply the
   ranges block by switching `PhysicalRange` parameters to stock or
   advanced mode.
4. Audio thread posts `PresetLoadedResult` with the old pointers.
5. Message thread releases old state.

Snapshot recall: same pattern; recall never touches disk. `ranges` block
belongs to the preset, not the snapshot; a snapshot never widens or
narrows a parameter's live range.

## 6. Workshop: `GuitarSpec` and part swap

The Workshop edits a `GuitarSpec` (guitar-workshop.md 3). The spec is a
POD that describes every part slot; it is owned by the audio thread and
lives outside the APVTS because most part fields are structural (they
change coefficient sets, sample rates, IRs) rather than continuous.

### 6.1 Loading a guitar
Same pattern as a preset. `LoadGuitarCommand` carries a new `GuitarSpec`
pointer; the audio thread swaps it in, refills string-engine coefficients
from `part-acoustics.md`, and posts back the old pointer.

### 6.2 Swapping a part
`SwapPartCommand` carries the slot id (e.g. "pickup.neck", "strings.3",
"bridge", "wiring") and a new part pointer.

Audio thread:
1. Builds a temporary new `GuitarSpec` by copying the current one and
   replacing the slot's part.
2. Runs the part-acoustics mapping to compute the new coefficient sets
   for affected engines.
3. Swaps the `GuitarSpec` pointer atomically at the next block.
4. Crossfades affected coefficients over 5 ms.
5. Posts the old pointer back.

### 6.3 Shadow audition (Alt-hover on a part card)
`ShadowAuditionCommand` with `on: true` and a candidate part:

Audio thread:
1. Keeps the committed `GuitarSpec` unchanged.
2. Builds a shadow `GuitarSpec` with the candidate part in place.
3. Renders the audio thread's next N blocks against the shadow (bypasses
   the committed spec for as long as the audition holds).
4. On `ShadowAuditionCommand { on: false }`, crossfades back to committed
   over 30 ms, discards the shadow.

The audition never mutates the committed spec or the undo stack.

### 6.4 Spectrum delta
A worker thread (message-thread pool) runs the fixture render against
current and shadow specs in parallel, computes the magnitude spectra,
posts the delta to the workshop bench's spectrum pane. Budget per
delta is 40 ms per workshop-ui.md 6. Nothing on the audio thread.

## 7. Advanced ranges

The `ranges` block in a preset lists which parameter families are on
advanced. Family = a set of related parameters (e.g. "circuit",
"squeak", "buzz", "pick", "slide", "modulation", "amp").

On preset load or per-preset toggle:
1. Message thread sends `SetRangeModeCommand { family, mode }` for each
   change.
2. Audio thread receives; for each parameter in the family, swaps its
   `PhysicalRange` mode from stock to advanced or vice versa.
3. If the current value is outside the new range, it is clamped and a
   `ClampNotification` posted for the UI banner.

The AttachedKnob repaints on range change to show the warning-colour arc
and `*` suffix.

Per-control unlock: same command with a `single-parameter-id` field
instead of `family`. Per-control state stored inside the preset's ranges
block.

## 8. MIDI Learn

Arming:
1. User clicks MIDI Learn in the header or presses Ctrl+L.
2. UI enters arm mode; the next right-clicked control receives the arm
   flag.
3. `ArmMidiLearnCommand { on: true, param_id: X }` sent.
4. Audio thread listens for the next non-note MIDI event; the first
   becomes the mapping.
5. `MidiLearnedResult { param_id, cc_number, channel }` posted.
6. UI shows the mapping.

Mappings live in the preset by default; a "Save as global" flag moves a
mapping to the user-global settings file.

## 9. Meters, indicators, buzz heatmap

Standard pattern:
- Audio module computes per block into a state struct.
- Written to display FIFO.
- UI drains at rate documented in 4.2 and paints.
- Peak hold is computed UI-side from drained values.

Buzz heatmap: audio module writes per-string amplitude and per-fret
clearance headroom; UI paints the map with the warning-colour /
accent scheme in gui-integration.md 21.

## 10. Noise event strip

Audio side: NoiseEngine posts a tagged event on trigger (see 14) into
the display FIFO. UI side: 24 px scrolling strip in the CHARACTER tab
groups; each event is a small bar coloured by type. Hidden under reduced
motion in favour of a static per-class count updated at 5 Hz.

## 11. Scrolling data stream

`LogStream` component subscribes to a filter of `LogSample` fields the
subsystems tick (theme.md). Ring-buffers the last 200 lines, paints
with alpha-fade. Stops when no new lines for 500 ms, resumes on next
arrival. Disabled entirely under reduced motion.

## 12. Modulation UI wiring

Mod matrix subscribes to a snapshot of source current values and route
depths via the display FIFO at 30 Hz for mod-arc rendering. Route table
is edited via commands to the audio thread; the audio thread returns
updated matrix snapshot.

Dragging a source card onto a control:
1. UI captures the source ID at drag start.
2. On drop, UI resolves drop target's parameter ID via hit-test on
   `AttachedKnob` / slider under the mouse.
3. UI sends `AddModRouteCommand { source_id, dest_param_id, depth: 0.25 }`.
4. Audio thread applies, returns updated matrix snapshot.

Ghost drag: 60% opacity copy of the source card following the cursor
with a glow in the source's colour.

## 13. Guitar illustration

The `GuitarIllustration` component:
- Reads `GuitarSpec` via a subscription (rebuilds on spec change).
- Renders scalable vector illustration procedurally (workshop-ui.md 2).
- Overlays hit regions per gui-integration.md 3.1 for Easy Mode and per
  workshop-ui.md 4 for the Workshop bench.
- Reads active fret / string data from display FIFO to draw played
  notes in real time.
- Reads slide bar position and slant from display FIFO to draw the slide
  overlay when Slide Mode is on.
- Reads pick position and angle to draw the pick overlay.

Same component subclass serves the Easy-Mode illustration and the
Workshop bench, with a `interactionMode` flag.

## 14. NoiseEngine

Squeak, pick click / chirp / scrape, fret buzz, clank and slide noise
share one pool structure (each with its own generator class but a
common lifecycle):

- Fixed pool of `N` generators (16 for squeak per string-squeak.md 12,
  smaller for the others) allocated in `prepareToPlay`.
- On trigger, oldest active generator is evicted if all in use.
- Each generator writes into a small per-string bus that is summed into
  the string's excitation input; also tapped into the Aux 8 noise-only
  bus per routing-io.md 2.
- Display FIFO event posted on trigger (type, string, intensity).

Zero allocations at trigger time.

## 15. GuitarCircuit (replaces CableSim)

Coefficients recomputed at control rate when any circuit parameter
moves. Update cost per rate-change event is under 0.05% CPU
(volume-knob-interaction.md 13). Filters realise as biquads updated
lock-free via a coefficient FIFO from message thread to audio thread
for user-changed values, and inline on the audio thread for
automation-driven changes.

## 16. SlideEngine

State machine for pressure (Lifted / Light / Normal / Heavy / Fretted).
State transitions on MIDI or explicit control; audio thread applies the
appropriate string-engine damping and reflection adjustments per
slide-guitar.md. Slide bar position is a continuous parameter; the
tuning engine reads it in place of fret indices on strings the slide
contacts.

## 17. Preset load and snapshot recall integration for realism

Full state serialized by `getStateInformation`:
- APVTS state (parameters, including `PhysicalRange` current-mode
  flags).
- `uiState` VT (tab, mode, snapshot indices, Workshop A / B, Slide Mode
  on / off).
- Mod matrix, snapshot bank, setlist reference, MIDI mappings.
- `ranges` block.
- `GuitarSpec` reference (path if a saved `.luthierguitar`; inline blob
  if edited without saving).
- Circuit state (redundant with parameters but kept for
  forward-compatibility).
- MIDI export profile selection.

`setStateInformation` restores using the swap pattern; missing parts
fall back to factory defaults with a notification banner.

## 18. Undo / redo

Every parameter change captured by APVTS emits an undoable action.
Structural changes (mod routes, patterns, part swaps, guitar loads)
push a compound action.

Workshop actions push high-level entries whose display strings match
workshop-ui.md 8 ("Moved neck pickup 150 -> 142 mm").

Undo manager per plugin instance, owned by the processor. UI attaches
as a listener for enable / disable of shortcuts and the tooltip that
shows the last action name.

## 19. Threading contract summary

| Thread | Owns | Reads from other |
|---|---|---|
| Audio | DSP state, meters, parameter atomics, `GuitarSpec`, NoiseEngine pool, CircuitEngine coefficients, SlideEngine state | APVTS values, command queue |
| Message (UI) | Component tree, uiState VT | APVTS values, display FIFO |
| Worker (loads, exports, matches, spectrum delta) | Temporary buffers | File system, sends via command queue |

Worker pool: shared `juce::ThreadPool` with 2 threads. Long tasks (cab
match, notation export, IR resample, spectrum delta, guitar / part
file load) run there.

## 20. Localization wiring

Every user-visible string comes from `LocaleCatalog::get(id)`. Panels
do not hold string literals except as parameter IDs.

Locale change:
1. User picks locale in Options -> Localization.
2. Catalog reloads.
3. Every listening component (via `LocaleChanged` broadcaster) receives
   the change and calls `refreshStrings()`.
4. Layout re-runs on affected components.

No plugin restart required.

## 21. Accessibility wiring

Every attachable component sets its `AccessibilityHandler` in its
constructor. Custom widgets (fretboard, step grid, spectrum-delta
pane, buzz heatmap, workshop illustration) provide subclass handlers
with per-child accessibility per accessibility.md 1.

Label text for accessibility comes from the locale catalog; value
announcements use the parameter's value -> text function. Physical
parameters include the unit in the announcement ("neck pickup, 3.5
henries, mm 152, height 2.8 mm").

## 22. Save on close

Processor's `getStateInformation` serializes everything listed in
section 17. `setStateInformation` restores using swap. Preset files use
the same serializer with the plugin-instance UI state stripped.

## 23. Tests

- Attachment leak test: instantiate every panel 100 times, verify no
  listener remains attached after destruction.
- Cross-thread invariant: 60-second synthetic session with JUCE's
  message-manager lock detector; verify no audio-thread access to UI
  state.
- Preset load determinism: load 20 presets in random order 10 times
  each, verify final DSP state matches offline render of the same
  sequence.
- Undo / redo across all feature areas including Workshop swaps: 1000
  random parameter changes with random undo / redo, verify final state
  matches equivalent forward-only sequence.
- MIDI Learn: arm, send each of 128 CCs, verify each maps to intended
  parameter within one block.
- Display FIFO overflow: fire 10x drain rate for 60 s, verify no audio
  stalls and no crashes (oldest data dropped silently).
- Shadow audition: Alt-hover on 100 random parts, verify committed
  `GuitarSpec` is byte-identical after release.
- PhysicalRange switch: for every physical parameter, toggle stock <->
  advanced 100 times; verify clamped values match expected and no audio
  thread allocation occurs.
- Part swap crossfade: for every part slot, swap 100 times during
  playback, verify peak sample-to-sample delta below the click
  threshold.
- Spectrum delta: fixture render on committed vs shadow produces the
  same delta as an offline render within 0.2 dB.
