# STATE MODEL SPEC

How preset / snapshot / setlist / tune / loop / guitar / parts /
ranges / midi-mappings / uiState nest, what a load does to each
concurrent activity, and every intersection between concurrent state
changes.

Without this, every intersection is a guess. This file provides the
answers.

## 0. Ground rules

1. **State layers nest.** Guitar is inside Preset. Snapshot is inside
   Preset. Tune references Presets but is not inside one. Loop
   references a MIDI stream but is not inside a preset.
2. **A load is atomic per layer.** Loading a preset changes preset-
   level state at one moment (the audio-thread swap); it does not
   interrupt mid-block.
3. **Concurrent activities have documented policies.** Every "load X
   while Y is active" combination has an answer in section 8.
4. **Undo respects state boundaries.** Loading a preset pushes a
   boundary. Undo across a boundary requires Shift-modifier
   (gui-integration.md 18).
5. **Structural state never flows through parameters.** Mod matrix,
   patterns, snapshot bank, `ranges`, `GuitarSpec`, `Tune` all cross
   thread boundaries via the command / result queue.
6. **UI state does not travel with presets.** Which tab you had open,
   which snapshot slot you were looking at: those live in `uiState` and
   are per-plugin-instance.

## 1. The state layers

Top to bottom, largest scope to smallest:

```
+---------------------------------------------------------------+
|  USER-GLOBAL SETTINGS (Options; not per-preset)               |
|  audio device, MIDI ports, telemetry toggles, keyboard        |
|  shortcuts, expression cal, locale, UI palette, tab defaults  |
+---------------------------------------------------------------+
|  PLUGIN-INSTANCE UI STATE (uiState VT; per-instance, saves    |
|  with the host project but not with a preset)                 |
|  mode (Easy/Advanced), current tab, Live Mode, Slide Mode,    |
|  Workshop A/B slots, Practice drawer state                    |
+---------------------------------------------------------------+
|  SESSION STATE (runtime; not saved anywhere)                  |
|  undo stack, MIDI Learn arm state, tap tempo, session         |
|  recorder ring buffer, A/B compare buffers                    |
+---------------------------------------------------------------+
|  SETLIST (`.luthierset`; references presets)                  |
|  ordered list of preset + snapshot entries                    |
+---------------------------------------------------------------+
|  TUNE (`.luthiertune`; references presets and guitars)        |
|  sections, chords, melody, per-section rhythm and layers      |
+---------------------------------------------------------------+
|  PRESET (`.luthierpreset`)                                    |
|  guitar reference, parameters, ranges block, modulation,      |
|  snapshots, MIDI mappings, rhythm engine state, effects       |
+---------------------------------------------------------------+
|  SNAPSHOT (inside preset; up to 128)                          |
|  parameter values, effect bypass, rhythm engine state,        |
|  colour tag, label                                            |
+---------------------------------------------------------------+
|  GUITAR (`.luthierguitar`; referenced by preset)              |
|  bill of parts, setup, character seed                         |
+---------------------------------------------------------------+
|  PART (`.luthierpart`; referenced by guitar)                  |
|  single swappable part, physical fields                       |
+---------------------------------------------------------------+
```

Sibling to preset, not nested:
- **Loop** (`.luthierloop`): captured audio + MIDI + per-layer
  settings. References the preset it was recorded with by name only.

Sibling to preset, referenced by preset:
- **Rhythm patterns** (`.luthierpattern`): referenced by the preset's
  rhythm engine state.
- **IRs** (WAV): referenced by preset's tone-match state.

## 2. What "load a preset" does

Message thread:
1. Parse preset file.
2. Resolve guitar reference:
   - If preset's `guitar.override` present, use it inline.
   - Else load `guitar.reference` from disk.
   - Else fall back to a default and post a warning banner.
3. Resolve every referenced IR and pattern; missing references log
   for the banner.
4. Build `PresetData` in memory.
5. Post `LoadPresetCommand` to audio thread.

Audio thread (at next block boundary):
1. Apply parameters to APVTS.
2. Swap `GuitarSpec` pointer atomically. Old spec pointer returned
   for message-thread destruction.
3. Swap mod matrix state.
4. Swap snapshot bank.
5. Swap MIDI mappings.
6. Apply `ranges` block: for each family, set every `PhysicalRange`
   parameter to stock or advanced mode. Clamp any current value that
   falls outside the new range.
7. Crossfade affected DSP state over 5-30 ms (per module).
8. Post `PresetLoadedResult` with the old pointers.

Message thread:
1. Release old state.
2. Push a state boundary onto the undo stack.
3. Push any missing-reference / clamp notifications to the banner.

Never touches:
- User-global settings.
- uiState VT (which tab is open, which snapshot slot you're viewing).
- Session state (undo stack, MIDI Learn arm). A/B compare is the
  exception: it clears, per 8.1.
- Setlist state.
- Tune state.
- Loop state.

## 3. What "recall a snapshot" does

Message thread:
1. Read snapshot data from the currently loaded preset.
2. Post `RecallSnapshotCommand { index }`.

Audio thread (at next block boundary):
1. Apply snapshot parameters to APVTS.
2. If snapshot includes modulation delta, apply mod matrix diff.
3. Crossfade continuous parameters over `snapshot_xfade_ms` (default
   30 ms).
4. Hard-switch discrete parameters at crossfade midpoint.
5. Preserve delay and reverb tails via double-buffer.
6. Post `RecallSnapshotResult`.

Never touches:
- The preset file (snapshots are inside the preset; recall is an
  in-preset operation).
- The `ranges` block (snapshots do not widen or narrow ranges).
- The `GuitarSpec` (snapshots do not change the guitar).
- Session recorder buffer (recording continues, capturing the
  transition).
- Any UI state layer above preset.

Pushes an undo entry, but not a state boundary (snapshot recall is
smaller than a preset load).

## 4. What "load a tune" does

Message thread:
1. Parse `.luthiertune`.
2. Resolve section references: each section that names a preset gets
   the preset loaded on demand at play time (not up front, to keep
   load fast).
3. If the tune bundles preset / guitar files, extract them to a temp
   folder and treat as inline references.
4. Post `LoadTuneCommand`.

Audio thread:
1. Swap the `Tune` pointer.
2. Stop any current tune playback.
3. Do not change preset, snapshot, or guitar unless the first section
   references different ones (in which case a preset load runs first).
4. Post `TuneLoadedResult`.

Pushes a state boundary onto the undo stack.

## 5. What "load a setlist" does

Message thread:
1. Parse `.luthierset`.
2. Verify every referenced preset exists; missing entries flagged in
   the setlist UI as unresolved.
3. Store in session state; setlist itself doesn't change audio state.
4. Load the first entry's preset and snapshot (per section 2 and 3).

Setlist state is separate from tune state. A setlist may live
alongside a tune; they don't interact unless the user chooses to
step the setlist while a tune plays.

## 6. What "load a guitar" (from Workshop or preset browser) does

Message thread:
1. Parse `.luthierguitar`.
2. Resolve every part reference; missing parts fall back to factory
   defaults with a banner.
3. Build a new `GuitarSpec`.
4. Post `LoadGuitarCommand`.

Audio thread:
1. Swap `GuitarSpec` pointer.
2. Refill string-engine coefficients from part-acoustics.
3. Reprepare pickup, body, circuit, string modules.
4. Crossfade over 30 ms.
5. Post result.

Preset parameters (amp gain, mod matrix, etc.) apply on top of the
new guitar. This is the mechanism that lets a preset load a guitar
without discarding tonal settings.

Pushes a state boundary.

## 7. What "swap a part" (in Workshop) does

Per `ui-wiring.md` 6.2. Never a state boundary; individual undo
entry.

## 8. Concurrent activity intersection matrix

Every "X while Y" combination.

### 8.1 Preset load while ...

| Y (concurrent activity) | Result |
|---|---|
| Snapshot recall | New preset load supersedes the recall; recall discarded. |
| Tune playing | Tune pauses at the next section boundary if within 4 s, else immediately; banner "Preset changed mid-tune; some section state reset." Tune continues with the new preset applied to the current section from now on. |
| Setlist advance | Setlist entry state updates to reflect the loaded preset (which may not be the setlist's next entry); if it doesn't match, the setlist marks "user override in effect" and the next PageDown resumes from the setlist's own position. |
| Looper recording | Recording continues; the load is captured as a state-boundary event in the layer's stored MIDI, so re-render will reapply the preset change at the same beat. |
| Session recorder recording | Recording continues; captures the state change. |
| MIDI Learn armed | Arm state persists; the new preset's MIDI mappings load; the arm still applies to the last-armed control. If the control no longer exists in the new preset, arm is silently disarmed. |
| Workshop bench open | Bench stays open; the illustration updates to the new guitar. Any unsaved workshop changes are offered a "Save your workshop changes?" prompt before the guitar reference switches (Yes / Discard / Cancel-load). |
| Slide Mode on | Persists; the new preset's slide state (if any) applies. |
| Live Mode on | Persists; snapshot strip refreshes to the new preset's bank. |
| Practice drawer open | Persists. |
| A / B compare active | Compare state clears; banner "A/B cleared by preset load." |
| Freeze layer active | Freeze layer clears; the captured sound was preset-specific. |
| E-Bow active | E-Bow clears. |
| Feedback loop resonating | Feedback loop damps over 100 ms. |
| Held notes | Held notes decay through the new preset's parameters (they don't retrigger); Panic clears them fully. |

### 8.2 Snapshot recall while ...

| Y | Result |
|---|---|
| Held notes | Continue on their pre-recall string state until released; new notes take the new state. |
| Tune playing | Applies; tune continues with the recall applied to the current section. |
| Setlist advance | Setlist entry updates to the recalled snapshot's index; if the recall was from the same preset, setlist proceeds normally. |
| Looper recording | Captured as a state-boundary event. |
| MIDI Learn armed | Persists. |
| Freeze / E-Bow active | Clears (snapshot may change the amp path). |
| A / B compare | Recall replaces the currently selected A/B slot with the recalled state. |
| Feedback loop | Damps over 100 ms. |

### 8.3 Tune load while ...

| Y | Result |
|---|---|
| Setlist advance | Setlist stays; tune load does not step the setlist. |
| Looper recording | Recording continues; the tune load is captured as a state-boundary event; on re-render the tune load replays at the same tick. |
| Live Mode on | Persists. |
| Workshop open | Persists. |
| Currently playing tune | Playback stops; new tune loads at bar 0. |

### 8.4 Guitar load while ...

| Y | Result |
|---|---|
| Held notes | Decay through the new guitar's parameters. If string count changes (electric to bass), extra strings silence immediately; missing strings' held notes decay. |
| Workshop unsaved changes | Prompt: Save / Discard / Cancel-load. |
| A part-swap in flight | Old part-swap discarded; guitar load takes precedence. |
| Looper recording | Captured as state-boundary event. |
| Slide Mode on | If the new guitar has no slide-compatible bridge / nut, banner "This guitar was not built for slide; open Workshop to fit slide-friendly parts." Slide Mode stays on; the fretboard overlay shows but the slide's acoustic effect is muted. |

### 8.5 Part swap while ...

| Y | Result |
|---|---|
| Held notes | Continue ringing; part change crossfades affected coefficients. |
| Another part swap | The second swap queues after the first; each applies at its own next-block boundary. |
| Shadow audition in flight | Audition drops back to committed; the swap applies as a normal swap on the new committed spec. |

### 8.6 Ranges toggle (advanced-ranges) while ...

| Y | Result |
|---|---|
| A parameter is being automated | Automation continues; if the new range is narrower and the automation value is outside, the value clamps and the automation writes the clamped value. |
| A mod matrix source is driving the parameter | Same. |
| Session recorder recording | Captured as a state-boundary event. |

### 8.7 MIDI Learn arm while ...

| Y | Result |
|---|---|
| MIDI flood (see error-recovery.md) | First matching event still wins the arm. |
| Snapshot recall | Arm persists; the new snapshot's MIDI mappings apply. |
| Preset load | Arm persists if the target control exists in the new preset; else silently disarmed. |

### 8.8 Undo / redo while ...

| Y | Result |
|---|---|
| Held notes | Undo of a parameter change applies to the parameter live; held notes continue and hear the change. |
| Tune playing | Undo cannot cross a state boundary without Shift; a tune-load boundary is one such boundary. |
| Looper recording | Undo cannot roll back recording events (recording is a separate log). |

## 9. Save-to-file interactions

- **Save preset while snapshot bank has unsaved edits**: the file
  captures the current bank.
- **Save preset while a part swap is in flight**: waits (message
  thread) for the swap's result to land, then saves.
- **Save guitar while workshop has unsaved shadow-audition**: the
  save writes the committed spec, not the shadow.
- **Save tune while playback is running**: writes current tune state
  including any live edits.

Every save follows the atomicity rule in `file-formats.md` 13.

## 10. Persistence across sessions

Persisted (survive plugin close):
- All files on disk (presets, guitars, tunes, parts, loops, setlists,
  IRs, factory content, user-global config).
- Host-saved plugin state (parameters, mod matrix, snapshot bank,
  midi mappings, ranges, guitar reference, uiState VT).
- Per-plugin-instance uiState (which tab was open, etc.).

Not persisted (lost on close):
- Undo stack.
- A/B compare buffers.
- MIDI Learn arm state.
- Tap tempo (unless the host is running and tempo comes from there).
- Session recorder ring buffer (unless the user saved a take).
- Freeze / E-Bow layers (audible, transient).
- Feedback loop resonance state.
- Shadow-audition state (session-only).

## 11. Multi-instance interaction

Two instances of Luthier in one host:
- Do not share state.
- Each has its own APVTS, mod matrix, snapshot bank.
- User-global settings are read by both (same folder).
- File writes race: two saves to the same file, last writer wins;
  the atomicity rule prevents corruption.
- MIDI Learn: each instance learns independently.
- Session recorder: each instance's ring buffer is independent.

## 12. Diagnostics

Options -> Diagnostics -> "State Inspector" shows a live tree view
of the state layers: current preset name, snapshot index, guitar
reference, tune load state, setlist position, undo depth, active
overlays. Refreshes at 4 Hz. Useful for support tickets and for
users curious what changed when.

## 13. Tests

Every intersection in section 8 has a test in
`Tests/StateModel/Intersections/`. Every load path in sections 2-7
has a test verifying the "never touches" list.

Fuzz test: 10 000 random operations across all layers in random
order, verify no crashes, no orphaned state, no memory growth.
