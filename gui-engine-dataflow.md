# GUI-ENGINE DATA FLOW SPEC

For every live UI element, the specific data path from the audio thread
(or the parameter tree) to what the user sees. Rates, staleness rules,
what shows when data goes dark. `ui-wiring.md` covers the general
attachment pattern; this file names each live element and pins its
plumbing.

## 0. Ground rules

1. Every live element reads from **one** of: APVTS (parameter atomics),
   display FIFO (audio-thread posts), uiState VT (message-thread edits),
   parts model (GuitarSpec pointer).
2. **Drain rate is fixed per element.** The audio side writes at its
   own schedule; UI drain rate is chosen for the element's needs.
3. **Staleness rule for every element**: what shows when the last
   received sample is older than the "stale after" threshold.
4. **Never busy-wait for data.** If the FIFO is empty on drain, the
   element paints its stale state.
5. **Elements don't read from each other.** A meter does not read the
   fretboard. If two elements need the same underlying data, both
   subscribe to the FIFO field.
6. **UI never triggers audio work.** Repaint is a consumer, not a
   producer.

## 1. Element inventory

Every live element in the plugin, with its data flow. Rows follow the
same structure: reader (UI element), source (audio-side owner), field
type, audio schedule, UI drain rate, stale-after threshold, stale
state.

## 2. Meters

### Input meter (header)
- Source: `MasterBus::input_meter`.
- Field: `MeterSample { peak_l, peak_r, rms_l, rms_r }`.
- Audio schedule: computed per block.
- UI drain: 30 Hz.
- Stale after: 200 ms.
- Stale state: needle at -inf, peak hold decays at 20 dB/s.

### Output meter (header)
- Source: `MasterBus::output_meter`.
- Field, schedule, rate, staleness: as input.

### Aux bus meters (ROUTING tab)
- Source: `Routing::aux_meters[8]`.
- Field: `MeterSample` per aux.
- Audio schedule: per block; buses with mute off skip meter compute.
- UI drain: 30 Hz.
- Stale after: 200 ms.
- Stale state: -inf.

### Per-string activity strip (fretboard)
- Source: `StringEngine::string_activity[N]`.
- Field: `StringActivity { fret, amplitude, is_ringing, note_id }`.
- Audio schedule: per block.
- UI drain: 60 Hz for fretboard, 30 Hz for compact per-string strip.
- Stale after: 100 ms.
- Stale state: string dark, no fret highlight.

## 3. Output LED (theme.md)

- Source: `MasterBus::output_meter.peak_max`.
- Field: single float per block.
- UI drain: 60 Hz.
- Stale after: 100 ms.
- Stale state: unlit (`#5A5F66`).
- Colour mapping: `-inf` -> `#5A5F66` (dark grey), linear brighten
  to `#FFFFFF` at 0 dBFS, red (`#F2544E`) if peak > 0 dBFS, holds red
  for 400 ms then re-evaluates.

## 4. Chord readout (Easy rhythm strip, Col 4 RHYTHM live indicators)

- Source: `RhythmEngine::current_chord`.
- Field: `ChordSymbol { root, quality, bass, extensions, confidence }`.
- Audio schedule: per NoteOn burst (throttle to at most once per 30 ms).
- UI drain: 10 Hz.
- Stale after: 3 s (a chord is presumed still valid a bit after it was
  detected).
- Stale state: last known chord, dimmed.

## 5. Next-strum arrow (rhythm strip)

- Source: `RhythmEngine::next_strum { time_beats_ahead, direction }`.
- Audio schedule: per block while rhythm engine enabled.
- UI drain: 60 Hz.
- Stale after: 500 ms.
- Stale state: hidden.
- Behaviour: arrow lights on the beat; direction from field.

## 6. Fretboard (Easy illustration, Workshop bench, scale trainer,
tab reader)

The fretboard component paints multiple layers on top of the static
neck. Data sources per layer:

### 6.1 Played notes layer
- Source: `StringEngine::string_activity[N]`.
- Layer: dot per string at the current fret, size scaled by amplitude,
  alpha decaying over 60 ms after amplitude drops below threshold.
- UI drain: 60 Hz.

### 6.2 Scale highlight layer (scale trainer, Workshop reference)
- Source: `uiState.scaleTrainerMode` + `uiState.scaleKey` +
  `uiState.scaleMode`.
- Layer: dots on every note in the scale.
- Not from the audio thread; purely UI-side.

### 6.3 Buzz heatmap layer (SETUP group active)
- Source: `NoiseEngine::buzz_state[N][frets]`.
- Field: per-string per-fret headroom (dB below buzz threshold, or
  positive if buzzing).
- Audio schedule: computed per block on strings with amplitude > 0.
- UI drain: 30 Hz.
- Stale after: 500 ms.
- Stale state: heatmap fades to transparent over 200 ms.
- Colour mapping: > 6 dB headroom -> transparent, 0-6 dB -> warning
  colour, buzzing -> primary accent + dot glyph.

### 6.4 Slide bar overlay (Slide Mode on)
- Source: `SlideEngine::bar_position { fret_continuous, slant_deg, contacting_strings_mask, pressure_state }`.
- Audio schedule: per block while Slide Mode on.
- UI drain: 60 Hz.
- Stale after: 200 ms.
- Stale state: bar frozen at last position, alpha reduced 40%.
- Rendering: 6 px rounded bar in slide material colour, 80% opacity,
  rotated by slant.

### 6.5 Pick overlay (Workshop and Easy illustration when pick tool
selected)
- Source: `NoiseEngine::pick_state { position_mm, angle_deg }`.
- Audio schedule: per block.
- UI drain: 30 Hz.
- Stale after: 500 ms.
- Stale state: pick at last position, no highlight.

### 6.6 Pickup pulse layer (Workshop, when a pickup is sensing)
- Source: `PickupEngine::pickup_activity[pickup_id]`.
- Field: brief impulse per NoteOn on a string this pickup senses.
- UI drain: 60 Hz.
- Stale after: 100 ms.
- Rendering: pole pieces of the pulsed pickup brighten to 15% accent
  for 40 ms.

## 7. Mod arcs (every AttachedKnob)

- Source: `ModMatrix::destination_current_contribution[param_id]`.
- Field: per-destination, per-source contribution amount at the
  latest control-rate tick.
- Audio schedule: per control-rate tick (block/32, min 128 samples).
- UI drain: 30 Hz.
- Stale after: 500 ms.
- Stale state: arc holds; source colour reduced 30% saturation.
- Rendering: concentric arc outside the value arc, per source
  colour; multi-source arcs are segmented in each source's colour by
  contribution magnitude.

## 8. Circuit visualiser (Adv Col 2 CIRCUIT, CHARACTER -> CIRCUIT
mirror)

- Source: `GuitarCircuit::response_curve`.
- Field: 64-point log-spaced magnitude curve, updated when any
  circuit parameter changes.
- Audio schedule: recomputed at control rate on parameter change;
  message thread posts to display FIFO after receiving the parameter
  update notification.
- UI drain: 30 Hz on any change, else idle.
- Stale after: never; curve is stable until the next change.
- Rendering: EQ-style curve on the mini graph, updates within 100 ms
  of a knob movement.

## 9. Spectrum delta pane (Workshop bench)

- Source: worker thread renders a fixture through the committed
  `GuitarSpec` and the shadow `GuitarSpec` in parallel.
- Field: `SpectrumDelta { current_mag[64], pending_mag[64], t60_delta[N_strings], delta_summary_string }`.
- Audio schedule: none (worker thread only).
- UI drain: on worker completion; poll at 10 Hz.
- Stale after: 30 s (a stale delta is stale enough to hide).
- Stale state: pane shows "Auditioning a part will update this."
- Rendering: grey trace for committed, accent trace for pending, plus
  the single-line delta summary.

## 10. Noise event strip (CHARACTER tab groups)

- Source: `NoiseEngine::event_stream`.
- Field: `NoiseEvent { class, string, intensity, timestamp_samples }`.
- Audio schedule: per event, up to 500 events/s per string; if the
  FIFO backs up, oldest events drop silently.
- UI drain: 15 Hz.
- Stale after: 1 s per event (older events fall off the strip).
- Stale state: strip empty; under reduced motion, a per-class count
  shown as a small number replaces the strip and updates at 5 Hz.
- Rendering: 24 px scrolling strip; each event a small bar coloured
  by class, height = intensity.

## 11. Voice count and CPU % (footer)

- Source: `StringEngine::active_voice_count` and
  `AudioProcessor::cpu_load_rolling_1s`.
- Audio schedule: per block.
- UI drain: 4 Hz (footer updates need not be smooth).
- Stale after: 5 s.
- Stale state: "-" for the count, "-" for CPU.

## 12. Scrolling data stream (empty panel areas, theme.md)

- Source: `LogStream::events`.
- Field: `LogSample { text, category, ts }`.
- Audio schedule: subsystems tick these when they change; rhythm
  engine on note events, mod matrix on route creation, etc.
- UI drain: on receive; ring-buffers last 200 lines.
- Stale after: stream stops when no new lines for 500 ms; resumes on
  arrival.
- Under reduced motion: disabled entirely.
- Rendering: matrix-style scrolling text, light green, alpha-fade
  top / bottom two lines.

## 13. Snapshot strip

- Sources: `LivePerf::snapshot_bank` (structural) for names / colours,
  `LivePerf::current_snapshot_index` (parameter) for active state.
- Audio schedule: n/a; message-thread edits update the bank.
- UI drain: on change.
- Rendering: buttons per section 8 of gui-integration.md.

## 14. Setlist triptych (Live Strip)

- Source: `uiState.setlistState { current_index, prev_ref, current_ref, next_ref }`.
- Audio schedule: n/a; message-thread manages.
- UI drain: on change.
- Rendering: three cells, active centre.

## 15. Tap tempo LED (header)

- Source: `LivePerf::tap_state { current_bpm, is_beat, source }`.
- Audio schedule: on every beat when the plugin has tempo (host or
  internal tap or rhythm engine).
- UI drain: 60 Hz.
- Stale after: 500 ms.
- Stale state: LED off.
- Rendering: brief flash on beat, colour matches the tap accent.

## 16. Kill switch pill (Live Strip)

- Source: parameter `kill_switch_active` (boolean).
- No FIFO needed.
- UI drain: on parameter change.
- Rendering: red pill while held.

## 17. Guitar illustration (Easy Mode, Workshop bench)

Static layers from `GuitarSpec` (see `guitar-illustration.md`).
Live layers as documented in section 6.

## 18. Preset browser preview

- Source: on hover, thumbnail cache for the preset's guitar.
- Cache key: SHA of `GuitarSpec` canonical serialization.
- Cache miss: renders on the worker thread within 100 ms; UI shows a
  low-detail placeholder until ready.
- Cache size: 200 thumbnails at 128x256, ~15 MB.

## 19. A/B compare state

- Source: `PresetSystem::ab_state { a_snapshot, b_snapshot, active_slot }`.
- Not from audio thread; edits push through the parameter path.
- UI drain: on change.
- Rendering: A/B buttons highlight active.

## 20. MIDI Learn arm indicator

- Source: `MidiLearn::armed_state { armed, target_param_id }`.
- Audio schedule: on user arm and on first MIDI receipt.
- UI drain: on change.
- Rendering: MIDI Learn button in header pulses at 1 Hz when armed;
  target control pulses at 1 Hz.

## 21. Practice drawer LED (loop status)

- Source: `Practice::loop_state { recording, playing, layer_count }`.
- UI drain: on change and 4 Hz for the recording pulse.
- Rendering: LED red while recording, green while playing, off when
  idle.

## 22. Feedback readout LED (Col 3 SUSTAIN)

- Source: `AmpEngine::feedback_state { is_resonating, magnitude }`.
- Audio schedule: per block.
- UI drain: 30 Hz.
- Stale after: 200 ms.
- Rendering: LED brightness = magnitude, colour: primary accent.

## 23. Session recorder buffer indicator

- Source: `Practice::session_buffer { seconds_used, seconds_total }`.
- UI drain: 1 Hz.
- Rendering: horizontal bar in the SESSION tab strip.

## 24. Tune Builder transport indicator

- Source: `TuneBuilder::transport { section_index, beat_in_section,
  playing }`.
- Audio schedule: per block while playing.
- UI drain: 30 Hz.
- Rendering: playhead in the section strip; beat marker in the
  chord-progression strip.

## 25. Rendering pipeline for the illustration

Detailed in `guitar-illustration.md` 3. Summary: static parts render
from GuitarSpec once per spec change (cached), live overlays paint on
top per section 6, hit-test tree invalidates on spec change.

## 26. Test schedule

Every element in this document has:
- A unit test verifying source-to-render latency (drain-rate correct,
  stale-state correct).
- An integration test with real audio thread activity.
- A stale-state test that stops the audio schedule and verifies the
  UI degrades to the documented stale state.
- A reduced-motion test where applicable.

Tests live in `Tests/Ui/Dataflow/` grouped by element.

## 27. Debugging

Options -> Diagnostics -> "Show data-flow overlay" adds a small
per-element counter on every live UI element showing (a) drain rate
observed, (b) time since last sample, (c) FIFO fill level. Useful
for developers and for support tickets.
