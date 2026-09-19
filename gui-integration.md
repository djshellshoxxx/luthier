# GUI INTEGRATION SPEC (MASTER)

This is the authoritative map from every backend feature to a concrete,
reachable UI location. If a feature exists in the engine and does not
appear in this document, the UI is incomplete. If a feature appears in
this document and does not exist in the engine, the engine is incomplete.
Nothing else in the spec set may contradict this file.

Written to close the gap PROGRESS.md's extension milestones opened: many
subsystems were built without a home in the window.

## 0. Ground rules

1. **Every automatable parameter has exactly one canonical UI control**
   somewhere in the window. Duplicate mirrors (a knob that also appears in
   a header strip) are allowed and encouraged, but one location is the
   canonical edit surface, and that is the one the parameter's automation
   ID resolves to for right-click, MIDI Learn, mod-matrix drag targets, and
   accessibility focus.
2. **Every feature is reachable in at most three interactions** from the
   header. A snapshot recall is one press. Editing an LFO shape is header
   -> Advanced tab -> MOD tab -> LFO card. If it takes four, the layout is
   wrong.
3. **Easy Mode never hides an audible behaviour**. Anything that is on and
   making sound in Advanced Mode is either visible in Easy Mode or has an
   Easy-Mode summary control. Panels can be missing; behaviour cannot be
   invisible.
4. **Nothing is discoverable only by right-click.** Right-click is a
   power-user shortcut, not a first-class access path. Any feature reached
   by right-click must also have a visible button, menu item, or panel.
5. **Panel layout is fixed.** Users may resize the window and switch
   between Easy/Advanced, but panels do not drag, dock or reorder. The
   layout is the product; predictability beats flexibility here.
6. **Every panel has a header**. The header is section name (uppercase,
   accent bar left), a compact status line (right), and a collapse chevron
   (far right). Collapsed panels remember their state per preset.
7. **A panel is either present or absent.** Panels are never disabled
   with grey-out. If the current guitar has no whammy, the WHAMMY panel is
   not shown. The user is not shown a control they cannot use.
8. **The window minimum size at 100% UI scale is 1280 x 800.** Below that
   the UI reflows into a stacked layout described in section 12. Above
   that it grows proportionally, capped at 2560 x 1600 at 100% scale.

## 1. Window structure

```
+----------------------------------------------------------------------+
|  HEADER STRIP (32 px tall, always present)                            |
+----------------------------------------------------------------------+
|                                                                      |
|                       MAIN AREA                                      |
|                                                                      |
|         (Easy Mode: single layout, section 3)                        |
|         (Advanced Mode: 4-column layout, section 4)                  |
|                                                                      |
+----------------------------------------------------------------------+
|  LIVE STRIP     (32 px, present when Live Mode on, section 8)         |
+----------------------------------------------------------------------+
|  PRACTICE DRAWER (32 - 360 px, resizable, section 9)                 |
+----------------------------------------------------------------------+
|  FOOTER (16 px, version + scrolling data stream, section 11)         |
+----------------------------------------------------------------------+
```

Layout rules:
- Header, footer and (when active) live strip are always visible.
- Practice drawer collapses to 32 px, expanded state persists per preset.
- Main area consumes all remaining vertical space.
- All strips honour the 8 px base grid defined in theme.md.

## 2. Header strip

Left to right, with pixel budgets at 1280 px window width:

| Region | Width | Contains |
|---|---|---|
| Brand | 96 px | Plugin name "LUTHIER" + signature notch |
| Preset controls | 320 px | Prev, preset name (opens browser), next, save, A/B compare |
| Snapshot strip | 320 px | 8 snapshot buttons + prev/next arrows |
| Meters | 160 px | Input meter, output meter, output LED (theme.md) |
| Mode toggle | 96 px | Easy / Advanced pill |
| Utility | 128 px | Panic, MIDI Learn, tap tempo, kill (visible only in Live Mode) |
| Overflow | 160 px | Gear (Options), Help (?), Randomize dice, Reset circle |

At narrower windows, snapshot strip collapses to a numeric readout with
prev/next; overflow icons collapse into a single overflow menu (three
dots).

Each header control:
- Has a tooltip.
- Has a documented keyboard shortcut (see accessibility.md).
- Is a canonical edit surface for its underlying parameter (preset select,
  snapshot select, tap bpm, master input, master output).

The **A/B compare buttons** hold two parameter snapshots that are separate
from the snapshot bank of section 8. A/B is transient and does not
serialize; snapshot bank persists in the preset.

## 3. Easy Mode layout

Easy Mode fits every essential control in a single view. No tabs, no
column of tabs. The goal is that a new user can play a great sound within
30 seconds of opening the plugin.

```
+----------------------------------------------------------------------+
|  HEADER                                                              |
+----------------------------------------------------------------------+
|                                                                      |
|   GUITAR ILLUSTRATION (interactive, section 3.1)      |  RIG STRIP   |
|                                                       |  (3.2)       |
|                                                       |              |
|                                                       |              |
+-------------------------------------------------------+              |
|  PLAYING STRIP (3.3)                                  |              |
+-------------------------------------------------------+              |
|  TONE STRIP    (3.4)                                  |              |
+-------------------------------------------------------+              |
|  RHYTHM STRIP  (3.5)                                  |              |
+-------------------------------------------------------+--------------+
```

### 3.1 Guitar illustration

Live-rendered from `GuitarSpec`. Interactive hit regions:
- **Headstock**: click opens the tuning popover (per-string tuning, capo,
  temperament). Alt-click cycles temperament. Middle-click reroll character
  seed.
- **Nut area**: click opens the string material and gauge popover.
- **Body / soundhole / pickups**: click a pickup to select it; drag a
  pickup to change its position (electric only). Click the body to open
  the body popover (size, wood, bracing).
- **Bridge**: click for bridge type, saddle materials, whammy assignment.
- **Fretboard**: displays played notes in real time; click a fret to
  audition a note.

Right-click any region opens its full editor as an overlay.

### 3.2 Rig strip (right column, 280 px wide)

Vertical stack of module cards, top to bottom:
1. **Cable / DI** (small)
2. **Pre-effects rack** (compact, 8 slots)
3. **Amp** (large card: model dropdown, gain, bass, mid, treble, presence,
   master)
4. **Post-effects rack** (compact, 8 slots)
5. **Cabinet** (model, mic 1 select, mic 2 select, blend)
6. **Room** (size, wet/dry)

Each slot in the effects racks is a click target that opens the pedal's
full controls as a popover.

### 3.3 Playing strip

Left to right:
- **Playing mode** toggle: Mono / Poly / Chord.
- **Humanize** knob (single macro that drives the underlying matrix).
- **Character** knob (single macro that drives dead spots, tuner drift,
  fret wear, body age together).
- **Whammy** display (only visible if the current guitar has a whammy).

### 3.4 Tone strip

Left to right:
- Input gain
- Output gain
- Wet/dry (post-master)
- Stereo width

### 3.5 Rhythm strip

Compact rhythm engine surface per rhythm-engine.md section 8:
- Genre kit dropdown with dice
- Feel knob
- Enable switch
- Small readout: current chord symbol + next strum arrow

### 3.6 What Easy Mode intentionally omits

Modulation matrix, notation export, IR loading, expression pedal
calibration, controller profile editor, character deep panel, accessibility
options, updates, telemetry. All are reachable via Options or the mode
toggle. None are audible without user action, so their absence in Easy
Mode does not hide any sound-affecting state.

## 4. Advanced Mode layout

Four columns, each with its own tab strip, each column independently
scrollable.

```
+----------+----------+----------+----------------------+
| COLUMN 1 | COLUMN 2 | COLUMN 3 | COLUMN 4             |
| GUITAR   | PICKUPS  | AMP      | (tabbed workspace)   |
| BODY     | CABLE    | CAB      |                      |
| STRINGS  | PRE-FX   | POST-FX  |                      |
| WHAMMY   |          | ROOM     |                      |
+----------+----------+----------+----------------------+
```

Columns 1 to 3 are fixed panels. Column 4 is the workspace where every
extension-spec panel lives, chosen by a tab strip at the top.

### 4.1 Column 1: instrument body

Vertically stacked panels:
- **GUITAR**: instrument library, tuning, capo, temperament, scale length
  readout.
- **BODY**: model, dimensions, top, back and sides wood, bracing pattern,
  air resonance readout, character (link to character panel).
- **STRINGS**: material per string, gauge per string, age slider, tension
  readout.
- **WHAMMY**: type (Vintage / Floyd / TransTrem / Bigsby / none), range,
  return spring, dive stop, up stop. Absent if the guitar has none.

### 4.2 Column 2: signal capture

- **PICKUPS**: model, position, height, coil specs, pickup selector
  switch, blend, phase.
- **CABLE**: length, capacitance, roll-off toggle.
- **PRE-EFFECTS RACK**: 8 slots, drag to reorder, click for pedal
  controls, right-click for bypass or delete.

### 4.3 Column 3: amplification and space

- **AMP**: model, all tone controls, sag, bright switch, bias, master.
- **POST-EFFECTS RACK**: 8 slots.
- **CAB**: model, mic 1 (model, position, distance, axis), mic 2 (same),
  blend, phase, delay.
- **ROOM**: model, size, dampening, wet/dry.

### 4.4 Column 4: workspace tabs

Tab strip across the top of Column 4, in this fixed order:

`MOD | RHYTHM | LIVE | ROUTING | TONE MATCH | CHARACTER | PRACTICE | NOTATION | CONTROLLERS | HELP`

- **MOD** — modulation matrix (modulation-matrix.md section 5).
- **RHYTHM** — rhythm engine (rhythm-engine.md section 8).
- **LIVE** — snapshot bank editor, setlist editor, morph configuration,
  expression pedal calibration. Live-strip in the bottom of the window
  is the runtime surface; this tab is the setup surface.
- **ROUTING** — bus layout, aux gains, per-string strip, sidechain,
  MIDI out (routing-io.md section 8).
- **TONE MATCH** — IR slots, cab match wizard, EQ match wizard, capture,
  IR library browser (tone-match.md section 6).
- **CHARACTER** — dead spots, fret wear, tuner drift, aged electronics,
  body age, environment (character-wear.md section 10).
- **PRACTICE** — mirrors the practice drawer content in a larger surface
  for setup; drawer is the runtime surface.
- **NOTATION** — live TAB view, export dialog, chord symbol history
  (notation-export.md sections 3 and 5).
- **CONTROLLERS** — controller profile selector, latency wizard, custom
  CC map editor, multi-controller merge display (controllers.md).
- **HELP** — help panel per include.md, plus a live keyboard-shortcut
  cheat sheet.

Tab state is per-window not per-preset; last-used tab persists across
sessions in the plugin's user-global settings.

### 4.5 Column widths

At 1280 px window: Column 1 = 260, Column 2 = 260, Column 3 = 260,
Column 4 = fills. Columns 1 to 3 have a min width of 220 px. Column 4 has
a min width of 480 px. Below 1280 window width, columns 2 and 3 stack
into a single column at 260 px and Column 4 becomes 480 px. Below 1000 px
window width, Advanced Mode is unavailable and the mode toggle forces
Easy Mode with an inline notice.

## 5. The Options overlay

Opened from the header gear icon or `Ctrl+,`. Modal overlay, not a
separate window (so it inherits window scaling and cannot be lost behind
the host).

Tabs across the top:

`AUDIO | MIDI | APPEARANCE | ACCESSIBILITY | LOCALIZATION | EXPRESSION | UPDATES | PRIVACY | DIAGNOSTICS | FILE LOCATIONS`

Each tab is a scrollable form. All settings save on change; no OK/Apply
button. A close X sits top-right and Escape closes.

- **AUDIO** (Standalone only): output device, buffer size, sample rate,
  input device (for sidechain).
- **MIDI** (Standalone only): input port picker, virtual MIDI out toggle.
- **APPEARANCE**: theme accent tint, palette (default, deuteranopia-safe,
  protanopia-safe, tritanopia-safe, high contrast, light), reduced motion,
  UI scale slider, tooltip toggle, scrolling data stream toggle.
- **ACCESSIBILITY**: screen reader verbosity, full rebindable shortcuts
  table with search and reset-to-default, font override (accessibility.md
  section 9).
- **LOCALIZATION**: locale, fallback locale, custom string catalog path.
- **EXPRESSION**: expression pedal calibration wizard, per-CC calibration
  list, curve preview.
- **UPDATES**: update check enable, beta channel toggle, "check now",
  changelog viewer.
- **PRIVACY**: telemetry toggles per updates-telemetry.md section 6, view
  local logs, clear logs, endpoint editor, one-click purge.
- **DIAGNOSTICS**: debug window (include.md), reset-to-defaults (hard),
  export troubleshooting file, crash log toggle, session recorder toggle.
- **FILE LOCATIONS**: buttons that open every user data folder in the OS
  file explorer, per README.md section "Where things live at runtime".

## 6. The Preset browser overlay

Opened from the preset name in the header. Modal overlay.

Layout: left column category tree, centre column preset list, right
column preview (guitar illustration snapshot, tags, notes, save date).

Features:
- Search box top of centre column, live filter.
- Tag chips below search: click to filter.
- Right-click a preset for rename, duplicate, delete, reveal in file
  explorer, export.
- Drag-and-drop `.luthierpreset` files onto the overlay to import.
- New preset button, Save-as button (also in header).
- Sort by name, date, category, favourite.
- Star toggle for favourites.

Escape closes without loading; double-click or Enter loads.

## 7. The Snapshot strip

Runtime surface for live-performance.md.

Location: header strip, 320 px wide (mirrors also in Live Strip when
Live Mode is on).

Contents:
- Prev arrow
- 8 buttons showing current bank of 8 snapshots (or all if fewer)
- Next arrow
- Bank selector (small dropdown showing "1-8", "9-16" etc)

Each button shows the snapshot's colour tag as a left stripe and its
label truncated to 12 chars. Currently active snapshot has an accent
outline and its label lights.

Click loads the snapshot with the crossfade defined per live-performance.md.
Shift-click writes the current state to that slot (with a confirmation
inline, dismissible). Right-click opens rename, colour, clear.

The full snapshot editor lives in Column 4 -> LIVE tab.

## 8. The Live Strip (runtime surface)

Toggled by Live Mode in the header.

Left to right at 1280 window:

`Snapshot strip (repeated) | Setlist prev/current/next | Tap | Morph A [knob] B | Kill | Monitor level`

Purpose of the repeat: at 44 px hit targets (touch mode), the snapshot
buttons in the header shrink to fit and the live strip carries the large
touch-safe copies. Live Mode also suppresses tooltips and locks Advanced
Mode toggle to prevent accidental switching mid-set.

## 9. The Practice drawer

Runtime surface for practice-tools.md, docked to the bottom of the
window above the footer.

Collapsed (32 px): bpm readout, loop status LED, backing track title,
expand chevron.

Expanded (32 - 360 px, resizable by dragging the top border):
Tabs across the top: `METRO | LOOP | TRACK | SCALE | EAR | TAB | PROG | SESSION`.

Each tab renders per practice-tools.md section 9. The larger PRACTICE
tab in Column 4 hosts setup UI for the same tools (playlist editing,
click sample import, stats view).

## 10. Modulation UI overlays

Two things happen everywhere in the plugin, not just in the MOD tab:

### 10.1 Mod arcs on modulated controls

Any knob or slider that has one or more active mod routes shows a second
concentric arc outside the value arc (theme.md defines the value arc at
270 deg outside the knob body; the mod arc sits 4 px further out at 2 px
thick). Its colour is the source's assigned colour; when multiple sources
route to the same destination, the arc is segmented in each source's
colour proportional to that source's summed contribution.

### 10.2 Drag-to-modulate

Dragging any source card from the MOD tab (or, in Easy Mode, from a
compact source panel in Options) over any control creates a route with
default depth 25%. Release commits, Escape cancels.

Right-clicking any control includes a "Modulate ->" submenu listing every
source; selecting one creates a route. The route is then visible and
editable in the MOD tab.

## 11. The footer

16 px tall.

Left: plugin version in muted mono (theme.md).
Centre: current status line ("Ready", "Loading preset...", "MIDI Learn
armed for: gain").
Right: CPU % (rolling 1 s average of the plugin's callback cost as a
fraction of block budget), voice count.

The scrolling data stream from theme.md fills any empty vertical space
in the main area, not the footer. Footer text is always readable.

## 12. Reflow at small window sizes

Between 1000 and 1280 window width in Advanced Mode: Columns 2 and 3
stack.

Below 1000: Advanced Mode is unavailable; mode toggle snaps to Easy Mode
with an inline notice reading "Advanced Mode requires a larger window".

Easy Mode reflow: below 900 window width, the rig strip moves to a tab
above the guitar illustration ("Guitar / Rig" segmented control).

UI scale over 125% at any window size can force reflow; the layout engine
picks a smaller UI scale automatically if the requested one would clip
the current window, and shows a one-time notice.

## 13. Empty-state rules

Any panel that has no content shows a compact hint. Examples:
- Empty snapshot slot: "Shift-click to save current state here."
- No modulation routes: "Drag a source onto any control, or right-click a
  control to modulate."
- Empty setlist: "Drag presets here, or click + to add."
- No backing track loaded: "Drop a WAV / MP3 / FLAC here, or click Browse."

Empty states never leave a panel silent about how to fill it.

## 14. Notifications

Non-modal banners appear in the header just below the main strip, 32 px
tall, dismissible with an X. Colour follows severity per theme.md
(success green, warning yellow, error red).

Notification triggers:
- Preset load error: warning, "Failed to load preset X, using previous."
- Missing IR: warning, "IR file X not found, using built-in body."
- Sample-rate change: info, "Sample rate changed to 96 kHz, IRs re-resampled."
- Update available: info, "Version X.Y.Z available, click for changelog."
- Managed by policy: info, small pinned indicator that does not stack.
- Snapshot recall blocked (compare mode active): warning.
- Crash on last session: info, "Diagnostic bundle saved, upload?"
- License grace period: warning countdown in the last 3 days.

Banners auto-dismiss after 5 s unless they contain an action.

## 15. Right-click menus

Every control's right-click menu, in order:
1. Value entry (opens numeric text field).
2. Reset to default.
3. Copy value.
4. Paste value (if clipboard holds a numeric).
5. --- separator ---
6. MIDI Learn.
7. Assign to macro -> (submenu, 8 macros).
8. Modulate -> (submenu, all sources).
9. --- separator ---
10. Automation ID (readonly, for scripting).
11. Show in Options -> Shortcuts (if bound).

Every panel's right-click (empty area):
1. Collapse / expand.
2. Reset panel to default.
3. Screenshot panel to clipboard.
4. Docs (opens Help tab pinned to this panel's docs).

## 16. Keyboard shortcuts (canonical bindings)

All rebindable. Defaults:

| Action | Key |
|---|---|
| Panic | P |
| Kill switch | \ (hold) |
| Prev snapshot | [ |
| Next snapshot | ] |
| Snapshot 1-9 | 1-9 |
| Snapshot 10-18 | Shift+1-9 |
| Toggle Easy / Advanced | Tab |
| Toggle Live Mode | L |
| Toggle Practice drawer | D |
| Options | Ctrl+, |
| Preset browser | Ctrl+O |
| Save preset | Ctrl+S |
| Save preset as | Ctrl+Shift+S |
| New preset | Ctrl+N |
| Randomize | Ctrl+R |
| Reset all | Ctrl+Shift+R |
| A/B compare | Ctrl+/ |
| Tap tempo | T |
| MIDI Learn arm | Ctrl+L |
| Undo | Ctrl+Z |
| Redo | Ctrl+Shift+Z |
| Help | F1 |
| Show all shortcuts | Ctrl+? |
| Next Column 4 tab | Ctrl+] |
| Prev Column 4 tab | Ctrl+[ |
| Setlist next | PageDown |
| Setlist prev | PageUp |
| Reveal preset file | Ctrl+E |

## 17. Undo / redo

All parameter changes and structural edits (adding a mod route, editing
a rhythm pattern, moving a pedal in a rack) push onto an undo stack of
64 entries. Undo groups events within 200 ms into one entry.

Snapshot recalls, preset loads and setlist steps push a "state boundary"
onto the stack that undo respects; undoing across a boundary requires
holding Shift.

Undo state is per-plugin-instance and does not persist across sessions.

## 18. Feature-to-location index (canonical map)

Every backend feature. If a row is absent from this table, the feature is
not shipped in the UI.

| Feature | Backend module | Primary UI location | Secondary access | Shortcut |
|---|---|---|---|---|
| Instrument select | Model::GuitarLibrary | Adv Col 1 GUITAR | Easy guitar illustration headstock | - |
| Per-string tuning | TuningEngine | Adv Col 1 GUITAR tuning table | Easy: headstock click | - |
| Capo | TuningEngine | Adv Col 1 GUITAR | Easy: headstock click | - |
| Temperament | TuningEngine | Adv Col 1 GUITAR | Easy: alt-click headstock | - |
| Body dimensions | BodyEngine | Adv Col 1 BODY | Easy: body click | - |
| Body wood | BodyEngine | Adv Col 1 BODY | - | - |
| Bracing | BodyEngine | Adv Col 1 BODY | - | - |
| String material | StringEngine | Adv Col 1 STRINGS | Easy: nut click | - |
| String gauge | StringEngine | Adv Col 1 STRINGS | Easy: nut click | - |
| String age | StringEngine | Adv Col 1 STRINGS | Easy: character macro | - |
| Whammy type | WhammyEngine | Adv Col 1 WHAMMY | Easy: bridge click | - |
| Pickup select | PickupEngine | Adv Col 2 PICKUPS | Easy: pickup click | - |
| Pickup position | PickupEngine | Adv Col 2 PICKUPS | Easy: drag pickup | - |
| Pickup blend | PickupEngine | Adv Col 2 PICKUPS | Easy rig strip | - |
| Cable capacitance | CableSim | Adv Col 2 CABLE | - | - |
| Pre-effects rack | PreEffectsChain | Adv Col 2 PRE-FX | Easy rig strip | - |
| Post-effects rack | PostEffectsChain | Adv Col 3 POST-FX | Easy rig strip | - |
| Amp model | AmpEngine | Adv Col 3 AMP | Easy rig strip amp card | - |
| Amp tone stack | AmpEngine | Adv Col 3 AMP | Easy rig strip amp card | - |
| Amp sag | AmpEngine | Adv Col 3 AMP | - | - |
| Cabinet select | CabinetEngine | Adv Col 3 CAB | Easy rig strip | - |
| Cab mic 1/2 | CabinetEngine | Adv Col 3 CAB | Easy rig strip | - |
| Room | RoomEngine | Adv Col 3 ROOM | Easy rig strip | - |
| Playing mode | MidiInterpreter | Easy playing strip | Adv header | - |
| MIDI Learn | Support::MidiLearn | Header MIDI Learn button | Right-click any control | Ctrl+L |
| Preset browser | PresetSystem | Header preset name | - | Ctrl+O |
| A/B compare | PresetSystem | Header A/B | - | Ctrl+/ |
| Save preset | PresetSystem | Header save | - | Ctrl+S |
| Panic | MasterBus | Header panic button | - | P |
| Randomize | Support | Header dice | - | Ctrl+R |
| Reset | Support | Header reset | - | Ctrl+Shift+R |
| Export audio | Support::AudioExport | Options AUDIO tab -> Export, also File menu | - | - |
| Export MIDI | Support::MidiCapture | NOTATION tab, also Session recorder | - | - |
| Help | UI | Header ? | Column 4 HELP tab | F1 |
| Debug | Support::Diagnostics | Options DIAGNOSTICS | - | - |
| Easter egg | UI | Signature notch (12,12) pixel | - | - |
| Bus layout | Routing | Col 4 ROUTING tab | - | - |
| Aux buses | Routing | Col 4 ROUTING tab | - | - |
| Per-string outs | Routing | Col 4 ROUTING tab | - | - |
| Sidechain | Routing | Col 4 ROUTING tab | - | - |
| MIDI out | Routing | Col 4 ROUTING tab | - | - |
| LFOs 1-8 | ModMatrix | Col 4 MOD tab -> LFO cards | Right-click any control -> Modulate | - |
| Envelopes 1-4 | ModMatrix | Col 4 MOD tab -> ENV cards | Same | - |
| Step sequencers 1-2 | ModMatrix | Col 4 MOD tab -> STEP cards | Same | - |
| Envelope followers 1-2 | ModMatrix | Col 4 MOD tab -> FOLLOW cards | Same | - |
| Macros 1-8 | ModMatrix | Col 4 MOD tab -> MACROS | Header (in Live Mode) | - |
| Random source | ModMatrix | Col 4 MOD tab -> RAND card | Same | - |
| Mod routes | ModMatrix | Col 4 MOD tab -> route table | Drag/right-click any control | - |
| Chord detector output | RhythmEngine | Col 4 RHYTHM tab live readout | Easy rhythm strip | - |
| Voicer style | RhythmEngine | Col 4 RHYTHM tab | - | - |
| Strum pattern | RhythmEngine | Col 4 RHYTHM tab editor | - | - |
| Fingerpick pattern | RhythmEngine | Col 4 RHYTHM tab editor | - | - |
| Genre kit | RhythmEngine | Easy rhythm strip, Col 4 RHYTHM tab | - | - |
| Snapshots | LivePerf | Header strip, Col 4 LIVE tab setup | Live Strip | [ ] 1-9 |
| Setlist | LivePerf | Col 4 LIVE tab | Live Strip | PageUp/Dn |
| Morph | LivePerf | Col 4 LIVE tab | Live Strip knob | - |
| Tap tempo | LivePerf | Header tap button | Live Strip pad | T |
| Kill switch | LivePerf | Live Strip pill | - | \ |
| Monitor mix | LivePerf | Live Strip, Col 4 LIVE | - | - |
| Expression pedal cal | LivePerf | Options EXPRESSION | - | - |
| Controller profile | Controllers | Col 4 CONTROLLERS tab | - | - |
| Latency wizard | Controllers | Col 4 CONTROLLERS tab | - | - |
| Multi-controller | Controllers | Col 4 CONTROLLERS tab | - | - |
| Metronome | Practice | Practice drawer METRO | Col 4 PRACTICE tab setup | - |
| Looper | Practice | Practice drawer LOOP | Col 4 PRACTICE | - |
| Backing track | Practice | Practice drawer TRACK | Col 4 PRACTICE | - |
| Scale trainer | Practice | Practice drawer SCALE | Col 4 PRACTICE | - |
| Ear training | Practice | Practice drawer EAR | Col 4 PRACTICE | - |
| Tab reader | Practice | Practice drawer TAB | Col 4 PRACTICE | - |
| Progression looper | Practice | Practice drawer PROG | Col 4 PRACTICE | - |
| Session recorder | Practice | Practice drawer SESSION | Col 4 PRACTICE | - |
| User IR slots | ToneMatch | Col 4 TONE MATCH tab | - | - |
| Cab match | ToneMatch | Col 4 TONE MATCH tab wizard | - | - |
| EQ match | ToneMatch | Col 4 TONE MATCH tab wizard | - | - |
| Capture | ToneMatch | Col 4 TONE MATCH tab | - | - |
| IR library | ToneMatch | Col 4 TONE MATCH tab browser | - | - |
| Notation export | Notation | Col 4 NOTATION tab, File menu | - | - |
| Live TAB | Notation | Col 4 NOTATION tab, Practice drawer TAB | - | - |
| Chord symbol history | Notation | Col 4 NOTATION tab | - | - |
| Character seed | CharacterWear | Col 4 CHARACTER tab | Easy character macro | - |
| Dead spots | CharacterWear | Col 4 CHARACTER tab | - | - |
| Fret wear | CharacterWear | Col 4 CHARACTER tab | - | - |
| Tuner drift | CharacterWear | Col 4 CHARACTER tab | Easy character macro | - |
| Aged electronics | CharacterWear | Col 4 CHARACTER tab | - | - |
| Body age | CharacterWear | Col 4 CHARACTER tab | Easy character macro | - |
| Environment | CharacterWear | Col 4 CHARACTER tab | - | - |
| Accessibility opts | Accessibility | Options ACCESSIBILITY | - | - |
| Localization | Accessibility | Options LOCALIZATION | - | - |
| UI scale | Accessibility | Options APPEARANCE | - | - |
| Palette | Accessibility | Options APPEARANCE | - | - |
| Reduced motion | Accessibility | Options APPEARANCE | - | - |
| Updates | Updates | Options UPDATES | Header notification | - |
| Telemetry | Updates | Options PRIVACY | - | - |
| Crash reporting | Updates | Options PRIVACY | Post-crash prompt | - |
| License | Updates | Options UPDATES -> License | Help > About | - |
| Privacy dashboard | Updates | Options PRIVACY | - | - |

## 19. First-run experience

See `onboarding.md`. The first time the plugin loads, the header shows an
"Welcome, take the tour?" banner. Declined tours never reappear unless the
user opens Help -> Take the tour.

## 20. Discoverability rules

- Every panel with more than one row of controls has a "?" icon in its
  header. Click opens the Help tab pinned to that panel's docs.
- Every new feature added after version 1.0 shows a "NEW" dot on its
  entry point for one week after the user's first launch of the version
  that introduced it. Dot is dismissible.
- Options -> Diagnostics -> "What's on the audio path right now" shows a
  live block diagram of active DSP stages, useful for finding a rogue
  pedal that is inaudibly changing tone.

## 21. Tests

- Every entry in the feature-to-location index resolves to a real
  component. Automated: at startup in test mode, walk the table and
  attempt to programmatically focus each location; fail if any lookup
  returns null.
- Every parameter's automation ID resolves to exactly one focused control
  when triggered by right-click MIDI Learn's "focus source" action.
- Tab-order walk: hit Tab from the header, verify focus visits every
  visible interactive element in Column-1-through-4 order without
  revisiting or skipping.
- Reflow: instantiate at 800, 1000, 1280, 1600, 1920, 2560 window widths
  at 75%, 100%, 125%, 150% UI scale; verify no clipping in any panel.
- Empty-state hints: for every panel, force the empty state and verify
  hint text is present and legible.
- Right-click menu: for every automatable parameter, verify all 11 menu
  items are present and functional.
- Undo/redo across all feature areas: 1000-op random walk, verify final
  state matches initial after full undo.
