# UI SCALING SPEC

Fills a specific gap left open by `accessibility.md` 4 and 9 and
`gui-integration.md` 0.8: both already say the window scales from 75% to
200%, but neither says where that number lives between sessions, or what
happens when the same person opens Luthier as a VST3 in one DAW, an AU in
another, and the Standalone, on the same machine. This file is the
persistence and cross-format contract. It does not re-specify the reflow
rules (`gui-integration.md` 0.8, 13, `accessibility.md` 4), the palette or
font-sizing behaviour (`accessibility.md` 3, 4), or the Options control
itself (`accessibility.md` 9, `gui-integration.md` 5) - it says how the
number those sections already use gets stored, read and propagated.

Test ID prefix: **UISC-**.

## 0. Ground rules

1. **UI scale is a machine-and-person preference, not a sound.** It never
   lives in a preset, a snapshot, a `GuitarSpec`, a tune, or the host
   session's `uiState` (`state-model.md` 1). It belongs next to the CPU
   quality preference (`cpu-quality-modes.md` 3): about this installation,
   not about what plays.
2. **One stored value, every format.** VST3, AU, CLAP, AAX and Standalone
   are separate processes that read the same file. Opening any of them
   on the same machine shows the same scale, with no format-specific
   override.
3. **Six steps, not a continuum.** 75, 100, 125, 150, 175, 200%
   (`accessibility.md` 4). A stored value outside that set (a hand-edited
   file, a future build's finer step) snaps to the nearest step on load.
4. **Message thread only.** UI scale never reaches the audio thread, the
   processor's parameter tree, or `getStateInformation`. Changing it
   triggers a layout pass, never a `prepareToPlay`.
5. **Never clip silently.** A stored scale that would clip the current
   window is handled by the existing auto-pick-smaller-scale rule
   (`accessibility.md` 4); this file only makes sure that rule runs against
   the right stored value at the right time (section 3).
6. **Applies without a restart**, the same way a locale switch does
   (`ui-wiring.md` 20): the open editor re-lays-out in place.

## 1. Storage

New file `Documents/Luthier/config/ui.json`, beside `performance.json`
(`cpu-quality-modes.md` 3, `file-formats.md` 1), written temp-and-rename
(`file-formats.md` 13):

```json
{ "schema": 1, "magic": "luthier.ui", "ui_scale_pct": 100 }
```

- Missing or corrupt file reads as `{ "ui_scale_pct": 100 }` and is
  rewritten on the next change - never an error, matching
  `performance.json`'s own rule.
- A value not in the six-step set (section 0.3) is snapped to the nearest
  step on load and the corrected value is written back.
- This is the same file family as the tab-memory setting
  `gui-integration.md` 4.4 calls "the plugin's user-global settings"; a
  later spec that names that store formally may fold `ui.json` into it
  without changing this schema.

`UiPreferences` (new class, `Source/Support/UiPreferences.{h,cpp}`) owns
the file: load on first editor construction in the process, write on
change, broadcast to every editor already open in this process
(`juce::ChangeBroadcaster`, the same shape `PerformanceSettings` uses for
`performance.json`). A second process (a second DAW, or the Standalone,
running at the same time) does not see a live update; it picks up the new
value the next time one of its own editors is constructed (section 3).
Live cross-process propagation is not attempted: a plugin editor already
open in another host has no reason to resize itself because a different
program's window changed.

## 2. Where it is set

The control is `accessibility.md` 9's existing UI scale slider, in
Options -> APPEARANCE (`gui-integration.md` 5). This file fixes its wiring:

1. The slider's handle snaps to the six steps; a numeric readout
   ("125%") sits beside it and updates live while dragging.
2. The layout pass runs on **release**, not on every drag frame, so
   dragging across the strip does not resize the window six times in a
   fifth of a second. Dragging previews the target size as a dashed
   outline over the current window (a static overlay - `AnimationPolicy`
   `Decorative`, gone at Low / reduced motion per `cpu-quality-modes.md`
   6).
3. On release, `UiPreferences::setUiScalePct` writes `ui.json`, broadcasts
   to this process's editors, and this editor requests its new size
   (section 3).
4. `Options -> Diagnostics -> Reset to defaults` (`accessibility.md` 9,
   `gui-integration.md` 5) resets `ui_scale_pct` to 100 along with the
   other global options it already lists.

Keyboard: `accessibility.md` 2's arrow-key convention applies to the
slider like any other control (Shift for a coarser step is meaningless
here since the steps are already coarse; arrows simply move one step).
No new global shortcut is added - `gui-integration.md` 17's binding table
is fixed and out of scope for this file.

## 3. Per-format resize behaviour

**Standalone.** The application window resizes immediately on release.
No host to negotiate with.

**Plugin formats (VST3, AU, CLAP, AAX).** The editor asks its host to
resize via JUCE's `AudioProcessorEditor::setSize`, which each wrapper
turns into the format's own resize request. Most hosts on
`host-integration.md`'s matrix honour it. Where a host's own quirks entry
says otherwise (some AAX hosts refuse a resize while a plugin window is
docked), the editor keeps its current size, `ui.json` still records the
new preference, and a one-time banner explains it:

> "This host doesn't support resizing the plugin window live. Close and
> reopen the plugin to use 150%."

The banner follows `gui-integration.md` 15's rules (dismissible,
auto-dismiss 5 s) and is shown once per host quirk per session, not once
per change. Reopening the editor (host panel close/reopen, or a fresh
instance) reads the current `ui_scale_pct` and lays out correctly the
first time, so the workaround is always available.

**New instance / new editor construction.** Every editor, in every
format, reads `ui.json` at construction time and lays itself out at that
scale from its first paint. There is no "open at 100%, then jump" step,
because unlike `cpu-quality-modes.md`'s settings this file's value has no
audio-thread consequence to sequence around - it is read once, on the
message thread, before the first `resized()`.

## 4. Independence from preset and session state

- Not in `.luthierpreset` (`file-formats.md` 2): loading someone else's
  preset never changes your window size.
- Not in the host session's `uiState` (`state-model.md` 1): a saved
  project does not carry a scale, so opening the same project on a
  different machine uses that machine's own `ui.json`.
- Not in `A/B` slots, snapshots, or the ranges block: none of those are
  "about the window."
- Distinguished from `gui-integration.md` 4.4's per-window tab memory,
  which is legitimately global-settings state but is not this file's
  concern.

## 5. Interactions

- **Live Mode's 44 px minimum hit target** (`gui-integration.md` 9) is a
  post-scale figure: at 75% it is still 44 physical px, not 44 px of the
  unscaled layout, so Live Mode stays usable at the smallest step.
- **Reduced motion and CPU quality** are independent axes (this file adds
  no third one): the preview outline in section 2.2 is a `Decorative`
  animation and already follows `AnimationPolicy`
  (`cpu-quality-modes.md` 6).
- **Window-size recovery** (`accessibility.md` 4's "host restores a saved
  size that no longer fits") runs against whatever `ui_scale_pct` is
  current at that moment, not a value cached at some earlier point in the
  session.
- **Multiple instances in one host**: each instance's editor is its own
  `UiPreferences` reader within that process; a change in one instance's
  Options page updates every other instance's *already-open* editor in
  the same process (section 1), because they share the process's global
  object, but does not touch another host process.

## 6. Failure modes

| Case | Behaviour |
|---|---|
| `ui.json` missing on first run | Defaults to 100%, file created on first change |
| `ui.json` corrupt (bad JSON) | Defaults to 100%, overwritten on next change, debug log entry |
| `ui_scale_pct` not one of the six steps | Snapped to nearest step, corrected value written back |
| Host refuses live resize | Section 3's banner; window keeps its current size |
| Two processes change it within the same second | Last write to `ui.json` wins; each process already has what it wrote applied locally |

## 7. Tests

1. **UISC-01** Round trip: write each of the six steps, reload, value is
   exact.
2. **UISC-02** Corrupt / missing file: both read back as 100% and rewrite
   cleanly on the next change.
3. **UISC-03** Off-step snap: a hand-edited `113` loads as `100` (nearest
   step) or `125` per the documented nearest-step rule, and the file is
   corrected on the next save.
4. **UISC-04** New instance parity: two editors constructed against the
   same `ui.json` (Standalone and a plugin-format harness) lay out at the
   identical scale on first paint, with no intermediate 100% frame.
5. **UISC-05** Same-process broadcast: with two editors open in one
   process, changing scale in one resizes the other without either
   reading the file again.
6. **UISC-06** Cross-process independence: two separate processes each
   holding an editor; a scale change in process A does not alter
   process B's open window; a newly constructed editor in process B
   picks up A's change.
7. **UISC-07** Resize refusal fallback: with a harness host that rejects
   `setSize`, changing scale keeps the current window size, shows the
   one-time banner, and a fresh editor construction afterward lays out at
   the new scale immediately.
8. **UISC-08** No audio-thread involvement: 100 scale changes during
   active playback produce zero calls into `prepareToPlay` or the
   parameter tree, and zero audio dropouts (heap/lock trap clean).
9. **UISC-09** Reset to defaults restores 100% alongside the other global
   options `accessibility.md`'s Diagnostics reset already covers.
10. **UISC-10** Preset and session independence: loading a preset, an
    A/B slot, a snapshot, or a saved host session never changes
    `ui_scale_pct` or triggers a write to `ui.json`.
