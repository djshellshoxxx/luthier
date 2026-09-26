# GUI INTEGRATION SPEC (MASTER)

This is the authoritative map from every backend feature to a concrete,
reachable UI location. If a feature exists in the engine and does not
appear in this document, the UI is incomplete. If a feature appears in
this document and does not exist in the engine, the engine is incomplete.
Nothing else in the spec set may contradict this file.

Covers the eleven phase-1 extensions, the twelve realism specs
(advanced-ranges, GuitarCircuit, pick / squeak / buzz noise, slide, the
Workshop and parts model, strum dynamics, bass techniques, MIDI export)
and everything already in `spec.md`, `engine.md`, `theme.md`,
`include.md`.

## 0. Ground rules

1. **Every automatable parameter has exactly one canonical UI control**
   somewhere in the window. Duplicate mirrors (a knob that also appears in
   a header strip) are allowed and encouraged, but one location is the
   canonical edit surface, and that is the one the parameter's automation
   ID resolves to for right-click, MIDI Learn, mod-matrix drag targets and
   accessibility focus.
2. **Every feature is reachable in at most three interactions** from the
   header. A snapshot recall is one press. Editing an LFO shape is header
   -> Advanced tab -> MOD tab -> LFO card. Editing a pickup's position is
   header -> Workshop -> drag. If it takes four, the layout is wrong.
3. **Easy Mode never hides an audible behaviour.** Anything that is on and
   making sound in Advanced Mode is either visible in Easy Mode or has an
   Easy-Mode summary control. Panels can be missing; behaviour cannot be
   invisible.
4. **Nothing is discoverable only by right-click.** Right-click is a
   power-user shortcut, not a first-class access path. Any feature reached
   by right-click must also have a visible button, menu item, or panel.
5. **Panel layout is fixed.** Users may resize the window and switch
   between Easy / Advanced / Workshop / Slide overlays, but panels do not
   drag, dock or reorder. The layout is the product; predictability beats
   flexibility here.
6. **Every panel has a header**. The header is section name (uppercase,
   accent bar left), a compact status line (right), and a collapse chevron
   (far right). Collapsed panels remember their state per preset.
7. **A panel is either present or absent.** Panels are never disabled
   with grey-out. If the current guitar has no whammy, the WHAMMY panel is
   not shown. If the current guitar is not a bass, the SLAP group is not
   shown. The user is not shown a control they cannot use.
8. **The window minimum size at 100% UI scale is 1280 x 800.** Below that
   the UI reflows into the stacked layout in section 13. Above that it
   grows proportionally, capped at 2560 x 1600 at 100% scale.
9. **Advanced ranges are marked, not hidden.** When a preset has the
   advanced-ranges flag on, the portion of a knob's value arc past the
   stock range is drawn in the warning colour and the readout gains a `*`
   suffix. A padlock icon sits at the top of any tab holding advanced
   values; unlocked = the preset opts in.

## 1. Window structure

```
+----------------------------------------------------------------------+
|  HEADER STRIP (32 px tall, always present)                           |
+----------------------------------------------------------------------+
|                                                                      |
|                       MAIN AREA                                      |
|                                                                      |
|         Easy Mode: single layout, section 3                          |
|         Advanced Mode: 4-column layout, section 4                    |
|         Workshop: takes over Columns 3 + 4 while active, section 6   |
|                                                                      |
+----------------------------------------------------------------------+
|  LIVE STRIP     (32 px, present when Live Mode on, section 9)        |
+----------------------------------------------------------------------+
|  PRACTICE DRAWER (32 - 360 px, resizable, section 10)                |
+----------------------------------------------------------------------+
|  FOOTER (16 px, version + scrolling data stream, section 12)         |
+----------------------------------------------------------------------+
```

Header, footer and (when active) live strip are always visible. Practice
drawer collapses to 32 px, expanded state persists per preset. Main area
consumes the remaining vertical space.

## 2. Header strip

Left to right, at 1280 px window width:

| Region | Width | Contains |
|---|---|---|
| Brand | 96 px | "LUTHIER" + signature notch (Easter egg pixel per include.md) |
| Preset controls | 260 px | Prev, preset name (opens browser), next, save, A/B compare |
| Mode + workshop | 128 px | Easy / Advanced pill, Workshop wrench glyph, Slide bar glyph |
| Snapshot strip | 260 px | 8 snapshot buttons + prev/next arrows |
| Meters | 160 px | Input meter, output meter, output LED (theme.md) |
| Utility | 128 px | Panic, MIDI Learn arm, tap tempo, kill (Live Mode only) |
| Overflow | 128 px | Gear (Options), Help, Randomize dice, Reset circle |

Wrench and slide glyphs use the secondary accent when their overlay /
mode is on, muted when off. Header collapses gracefully below 1280:
snapshot strip becomes a numeric readout + prev/next, overflow icons
fold into a three-dot menu.

Every header control has a tooltip, a documented keyboard shortcut
(section 17) and is the canonical edit surface for its underlying
parameter.

**A / B compare** holds two transient parameter states separate from the
snapshot bank of section 8. It does not serialize; snapshots persist in
the preset.

**Range-lock indicator**: when the current preset's `ranges` block is on,
a small padlock (secondary accent, unlocked state) appears next to the
preset name. Clicking it opens `Options -> Ranges` for the per-preset
toggle and marks legend.

## 3. Easy Mode layout

Fits every essential control in a single view. No tabs, no column of
tabs. A new user plays a great sound within 30 seconds of opening the
plugin.

```
+----------------------------------------------------------------------+
|  HEADER                                                              |
+----------------------------------------------------------------------+
|                                                       |              |
|   GUITAR ILLUSTRATION (interactive, section 3.1)      |  RIG STRIP   |
|                                                       |  (3.2)       |
+-------------------------------------------------------+              |
|  PLAYING STRIP  (3.3)                                 |              |
+-------------------------------------------------------+              |
|  TONE STRIP     (3.4)                                 |              |
+-------------------------------------------------------+              |
|  RHYTHM STRIP   (3.5)                                 |              |
+-------------------------------------------------------+--------------+
```

### 3.1 Guitar illustration

Live-rendered from `GuitarSpec` (guitar-workshop.md and workshop-ui.md
section 2). Interactive hit regions on Easy Mode:
- **Headstock**: click opens the tuning popover (per-string tuning,
  capo, temperament).
- **Body / pickups**: click a pickup to select it as the active pickup.
- **Bridge**: click opens the whammy popover (only if a whammy is
  fitted).
- **Fretboard**: displays played notes in real time.

Deep edits (moving a pickup, swapping strings, dropping a new bridge)
require the Workshop, reachable from the header wrench, described in
section 6.

### 3.2 Rig strip (right column, 280 px)

Vertical stack of module cards, top to bottom:
1. **Guitar circuit** (compact card: volume, tone, live circuit
   visualiser miniature, per volume-knob-interaction.md 10)
2. **Pre-effects rack** (compact, 8 slots)
3. **Amp** (large: model, gain, bass, mid, treble, presence, master)
4. **Post-effects rack** (compact, 8 slots)
5. **Cabinet** (model, mic 1, mic 2, blend)
6. **Room** (size, wet/dry)

Each effect slot is a click target that opens the pedal's full controls
as a popover.

### 3.3 Playing strip

Left to right:
- Playing mode: Mono / Poly / Chord.
- Humanize macro.
- Character macro (dead spots, tuner drift, fret wear, body age, string
  noise amount together).
- Whammy display (only if fitted).

### 3.4 Tone strip

Input gain, Output gain, Wet/dry (post-master), Stereo width.

### 3.5 Rhythm strip

Compact per rhythm-engine.md section 8:
- Genre kit dropdown with dice.
- Feel knob (also scales strum crossing velocity and evenness per
  strum-dynamics.md 6).
- Enable switch.
- Small readout: current chord symbol, next strum arrow.
- JAM group at the right end (jam-mode.md 8.2): the 88 x 32 JAM pill
  (first press arms; ARMED / COUNT / PLAYING / ENDING), the band's style,
  a 5-dot intensity and a "Band" volume mini-knob.

### 3.6 What Easy Mode intentionally omits

Modulation matrix, notation export, IR loading, expression pedal
calibration, controller profile editor, character deep panel, string
noise / pick / buzz / slide advanced controls, Workshop deep edit,
accessibility options, updates, telemetry, advanced-range padlock,
MIDI-export tab. All are reachable via Options, the mode toggle or the
Workshop glyph. None are audible without user action, so their absence
in Easy Mode does not hide any sound-affecting state.

## 4. Advanced Mode layout

Four columns, each with its own tab strip, each column independently
scrollable.

```
+----------+----------+----------+----------------------+
| COLUMN 1 | COLUMN 2 | COLUMN 3 | COLUMN 4             |
| GUITAR   | PICKUPS  | AMP      | (tabbed workspace)   |
| BODY     | CIRCUIT  | POST-FX  |                      |
| STRINGS  | PRE-FX   | CAB      |                      |
| WHAMMY   |          | ROOM     |                      |
|          |          | SUSTAIN  |                      |
+----------+----------+----------+----------------------+
```

Columns 1 to 3 are fixed panels. Column 4 is the workspace, chosen by a
tab strip at the top.

### 4.1 Column 1: instrument body

- **GUITAR**: instrument library (from user's Guitars folder plus
  factory), tuning, capo, temperament, scale-length readout, "Open in
  Workshop" button.
- **BODY**: model, dimensions, top / back / sides wood, bracing pattern,
  air-resonance readout, link to CHARACTER tab.
- **STRINGS**: material per string (from strings library part),
  gauge per string, age slider, tension readout.
- **WHAMMY**: type, range, return spring, dive stop, up stop. Absent if
  the guitar has none.

### 4.2 Column 2: signal capture

- **PICKUPS**: pickup selector switch, per-pickup gain, per-pickup phase.
  Pickup model, position, height, coil specs live in the Workshop
  inspector; a "Edit in Workshop" button jumps there.
- **CIRCUIT** (replaces the old CABLE panel; volume-knob-interaction.md
  10): guitar volume, guitar tone, pot value dropdown, tone-cap
  dropdown, treble-bleed dropdown with R/C editor, active/passive
  toggle, cable length + quality dropdown, small live circuit-response
  visualiser.
- **PRE-EFFECTS RACK**: 8 slots, drag to reorder, click for pedal
  controls, right-click for bypass or delete.

### 4.3 Column 3: amplification, space and sustain

- **AMP**: model, tone controls, sag, bright, bias, master.
- **POST-EFFECTS RACK**: 8 slots.
- **CAB**: model, mic 1, mic 2, blend, phase, delay.
- **ROOM**: model, size, dampening, wet/dry.
- **SUSTAIN** (from ambiguity-resolutions.md 2): Freeze row (enable,
  capture ms, level, attack, release, tonal LP / HP). E-Bow row (enable,
  string mask, intensity, harmonic). A small feedback readout LED.

### 4.4 Column 4: workspace tabs

Tab strip at the top, in this fixed order:

`WORKSHOP | MOD | RHYTHM | TUNE | JAM | LIVE | ROUTING | TONE MATCH | CHARACTER | PRACTICE | NOTATION | MIDI OUT | CONTROLLERS | HELP`

(jam-mode.md 8.1: JAM, the backing band, sits directly after TUNE.)

- **WORKSHOP** — enters the bench (section 6). While active, the tab
  expands across Columns 3 + 4 as one workspace; Column 3's panels stack
  below the bench when the window is tall, or hide behind an inline
  chevron when short.
- **MOD** — modulation matrix, per modulation-matrix.md 5.
- **RHYTHM** — chord voicer, strum + fingerpick editors, feel controls,
  the **STRUM** group (strum-dynamics.md 6), the bass step grid when the
  current guitar family is bass.
- **TUNE** — the composition workspace (tune-builder.md 3). Section
  strip, chord-progression editor, melody piano roll, per-section
  rhythm and layers, transport, export dialog. The three-minute tune
  workflow (tune-builder.md 2) fits on this one tab. Writes into the
  rhythm engine and note engine; every audible detail from the realism
  specs applies to playback here as it does to live play.
- **LIVE** — snapshot bank editor, setlist editor, morph configuration,
  expression-pedal calibration. The live-strip in the bottom of the
  window is the runtime surface; this tab is the setup surface.
- **ROUTING** — bus layout, aux gains (including **Aux 8** noise bus),
  per-string strip, sidechain, MIDI out.
- **TONE MATCH** — IR slots (body, cab 1, cab 2), cab match wizard, EQ
  match wizard, capture, IR library browser.
- **CHARACTER** — dead spots, fret wear, tuner drift, aged electronics,
  body age, environment, plus:
  - **STRING NOISE** group (string-squeak.md 9): squeak amount,
    probability, finger moisture, finger pressure, per-material profile
    selector, style presets, noise-event strip.
  - **PICK** group (pick-noise.md): pick / strum striker dropdowns,
    material, thickness, tip, bevel, wear, angle, click and chirp
    amounts, pick-scrape amount.
  - **SETUP** group (fret-buzz.md 6): action treble / bass, relief, nut
    depth per string, fret height, buzz threshold, sitar mode; live
    buzz heatmap on a small fretboard.
  - **SLIDE** group (slide-guitar.md 7): pressure state, slant, slide
    material and mass mirror (canonical in Workshop), noise amount,
    clank amount; only shown when Slide Mode is on.
  - **CIRCUIT** group (volume-knob-interaction.md 10) mirror of the
    Column 2 panel for full editing.
- **PRACTICE** — setup surface for practice-tools.md; the drawer is the
  runtime surface.
- **NOTATION** — live TAB view, notation-export dialog (MusicXML, Guitar
  Pro, ASCII, MIDI), chord-symbol history.
- **MIDI OUT** — MIDI-export profile editor (midi-export.md): Luthier
  vs Generic profile toggle, per-event-class checkboxes (squeak, pick,
  buzz, slide, workshop, strum, character), CC-map editor, drag-out
  export button. Shares the routing panel's MIDI-out enables so
  changes stay in sync.
- **CONTROLLERS** — controller profile selector, latency wizard, custom
  CC-map editor, multi-controller merge display.
- **HELP** — help panel per include.md, plus a live keyboard-shortcut
  cheat sheet.

Tab state is per-window not per-preset; last-used tab persists across
sessions in the plugin's user-global settings.

### 4.5 Column widths

At 1280 px window: Col 1 = 260, Col 2 = 260, Col 3 = 260, Col 4 = fills.
Cols 1 to 3 have a min width of 220 px. Col 4 has a min width of 480 px.
Below 1280, Cols 2 and 3 stack into a single column at 260 px, Col 4
becomes 480 px. Below 1000, Advanced Mode is unavailable; the mode
toggle forces Easy Mode with an inline notice. When the Workshop tab
takes over Cols 3 + 4, Col 4's tab strip stays visible so the user can
exit the Workshop without a keyboard shortcut.

## 5. Options overlay

Opens from the header gear (`Ctrl+,`). Modal overlay, not a separate
window (inherits window scaling, cannot be lost behind the host).

Tabs across the top:

`AUDIO | MIDI | APPEARANCE | ACCESSIBILITY | LOCALIZATION | EXPRESSION | RANGES | UPDATES | PRIVACY | DIAGNOSTICS | FILE LOCATIONS`

- **AUDIO** (Standalone): output device, buffer, sample rate, sidechain
  input.
- **MIDI** (Standalone): input port picker, virtual MIDI out toggle.
- **APPEARANCE**: theme accent tint, palette, reduced motion, UI scale,
  tooltip toggle, scrolling data-stream toggle, noise-event strip
  toggle.
- **ACCESSIBILITY**: screen-reader verbosity, full rebindable shortcuts
  table with search and reset, font override.
- **LOCALIZATION**: locale, fallback locale, custom string-catalog path.
- **EXPRESSION**: expression-pedal calibration wizard, per-CC
  calibration list, curve preview.
- **RANGES** (advanced-ranges.md): the master per-preset toggle, the
  "always show marked values as warning colour" preference, the
  "randomize respects stock range" preference, a summary list of any
  parameters currently outside stock range in the loaded preset.
- **UPDATES**: update-check enable, beta channel, "check now",
  changelog viewer.
- **PRIVACY**: telemetry toggles per updates-telemetry.md 6.
- **DIAGNOSTICS**: debug window, reset-to-defaults hard, export
  troubleshooting file, crash-log toggle, session-recorder toggle,
  Workshop / Slide / advanced-ranges booleans mirror (for confirming
  what telemetry would see).
- **FILE LOCATIONS**: buttons that open every user data folder in the
  OS file explorer, including `~/Documents/Luthier/Guitars/` and
  `~/Documents/Luthier/Parts/`.

Save-on-change; no OK / Apply button. Escape closes.

## 6. Workshop

The bench (workshop-ui.md). Enters via header wrench or Col 4 WORKSHOP
tab; while active it takes over Cols 3 + 4 with the layout in
workshop-ui.md 1:

```
+---------------------------------------------------------------+
| WORKSHOP  [guitar name (modified)]  [Save As Guitar]  [A/B]   |
+-----------------------------------------------------+---------+
|                                                     |         |
|         GUITAR ILLUSTRATION (interactive)           |INSPECTOR|
|      body, neck, headstock, every part hit-tested   |(selected|
|                                                     |  part)  |
|   [ruler: mm from saddle, pickup rail]              |         |
+-----------------------------------------------------+         |
| PARTS DRAWER: Body | Neck | Frets | Nut | Bridge |  |         |
|   Tuners | Strings | Pickups | Wiring | Preamp |    |         |
|   Pick | Slide | Capo                               |         |
+-----------------------------------------------------+         |
| SETUP STRIP: action H/L, relief, nut depth |    SPECTRUM      |
|                                            |    DELTA         |
+---------------------------------------------------------------+
```

Interactions (direct-manipulation on the illustration):
- **Pickup position**: drag with ruler snap; comb-notch lines shown on
  spectrum delta while dragging.
- **Pickup height** and **tilt**: scroll / drag screw handles.
- **Bridge**: click for cards, drag saddles for intonation.
- **Nut**: click for cards, drag slot depth per string.
- **Strings**: click a string to select; inspector shows per-string
  override.
- **Frets**: click fretwire to select fret part, brush wear (from
  CHARACTER).
- **Pick**: drag along string axis, rotate at corner.
- **Slide**: drag position, rotate for slant.

Bench A / B strip holds 8 stored `GuitarSpec` slots. Every action pushes
undo. Alt-hover on a part card auditions the swap on a shadow
`GuitarSpec` without committing.

Easy Mode: wrench in header opens the Workshop as an overlay; Escape
closes.

## 7. Slide mode

Slide is a header toggle (slide-guitar.md). When on:
- SLIDE group appears in the CHARACTER tab.
- The Workshop's Slide category is enabled in the parts drawer.
- The fretboard overlay draws the slide bar at the current position with
  the current slant; the Workshop's illustration draws the same bar.
- Pitch tracking is continuous; the tuning popover shows the current
  glide target.
- Squeak generation on strings the slide contacts is suppressed
  (slide-guitar.md, string-squeak.md 6).

Header glyph uses the secondary accent when on. Toggle is a keyboard
shortcut (`S`).

## 8. Snapshot strip

Header, 260 px. Runtime surface for live-performance.md.

Prev, 8 buttons (current bank), next, bank selector. Each button shows
its snapshot colour tag and label truncated to 12 chars. Active
snapshot has an accent outline.

Click loads; Shift-click writes; right-click for rename, colour, clear.
Full editor lives in Col 4 -> LIVE.

## 9. Live Strip

Toggled by Live Mode in the header. Left to right at 1280 px:

`Snapshot strip (repeated) | Setlist prev/current/next | Tap | Morph A [knob] B | Kill | Monitor level`

Live Mode also raises minimum control hit-target to 44 px, suppresses
non-critical tooltips, and locks the Advanced Mode toggle.

## 10. Practice drawer

Bottom of the window above the footer. Collapsed strip (32 px) shows
bpm, loop LED, backing-track title, expand chevron. Expanded tabs:
`METRO | LOOP | TRACK | SCALE | EAR | TAB | PROG | SESSION`, each per
practice-tools.md 9.

Setup surface for practice tools lives in Col 4 PRACTICE.

## 11. Modulation UI overlays

Two things happen everywhere in the plugin.

### 11.1 Mod arcs
Any knob or slider with active mod routes shows a second concentric arc
outside the value arc (4 px further out, 2 px thick), coloured by
source. Multi-source arcs are segmented in each source's colour by that
source's summed contribution.

### 11.2 Drag-to-modulate
Dragging any source card from the MOD tab over any control creates a
route with default depth 25%. Release commits, Escape cancels.
Right-click any control includes a "Modulate ->" submenu.

## 12. Footer

16 px. Left: version. Centre: status line. Right: CPU %, voice count.

Scrolling data stream (theme.md) fills empty vertical space in the main
area, not the footer. Under reduced motion the stream is a static count
of recent events per class.

## 13. Reflow

- 1000 to 1280: Cols 2 and 3 stack in Advanced.
- Below 1000: Advanced unavailable; Easy Mode with an inline notice.
- Easy Mode below 900: rig strip becomes a "Guitar / Rig" tab above the
  illustration.
- UI scale > 125% at any size can force reflow; the layout engine
  auto-picks a smaller scale if the requested one clips, with a
  one-time notice.

## 14. Empty states

Every panel that can be empty shows a compact hint:
- Empty snapshot slot: "Shift-click to save current state here."
- No mod routes: "Drag a source onto any control, or right-click a
  control to modulate."
- Empty setlist: "Drag presets here, or click + to add."
- No backing track: "Drop a WAV / MP3 / FLAC here, or click Browse."
- Non-slide guitar with Slide Mode on: "This guitar was not built for
  slide. Open Workshop to fit a hi-nut or a different bridge."
- Bass mode active but a guitar-family instrument loaded: "Bass
  techniques are inactive. Load a bass to use them."
- Advanced-range editing without the preset's flag set: "This preset
  uses stock ranges. Options -> Ranges to unlock, or right-click to
  unlock this control only for this preset."

## 15. Notifications

Non-modal banners under the header strip, 32 px, dismissible.

Triggers:
- Preset load error / missing IR / missing guitar (fallback used).
- Missing part: "Bridge X not found, using factory default." with a
  jump-to-Workshop.
- Missing `.luthierguitar` referenced by preset: "Guitar Y not found,
  loaded closest factory match."
- Sample-rate change: "Sample rate changed to 96 kHz, IRs and circuit
  filters re-resampled."
- Update available.
- Managed by policy indicator.
- Advanced-range clamped on preset save (with details).
- Crash on last session.
- License grace period countdown.

Banners auto-dismiss after 5 s unless they contain an action.

## 16. Right-click menus

Every control:
1. Value entry.
2. Reset to default.
3. Copy / Paste value.
4. --- separator ---
5. MIDI Learn.
6. Assign to macro -> (8 macros).
7. Modulate -> (all sources).
8. --- separator ---
9. **Unlock advanced range for this control** (per-preset, only if
   locked at preset level).
10. **Restrict to stock range for this control** (only if unlocked at
    preset level).
11. --- separator ---
12. Automation ID (readonly).
13. Show in Options -> Shortcuts (if bound).

Every panel (empty area):
1. Collapse / expand.
2. Reset panel to default.
3. Screenshot to clipboard.
4. Docs (opens Help pinned to this panel).

## 17. Keyboard shortcuts (canonical bindings)

All rebindable. Defaults:

| Action | Key |
|---|---|
| Panic | P |
| Kill switch | \ (hold) |
| Prev / Next snapshot | [ / ] |
| Snapshot 1-9 | 1-9 |
| Snapshot 10-18 | Shift+1-9 |
| Toggle Easy / Advanced | Tab |
| Toggle Workshop | W |
| Toggle Slide Mode | S |
| Toggle Live Mode | L |
| Toggle Practice drawer | D |
| Options | Ctrl+, |
| Preset browser | Ctrl+O |
| Save preset | Ctrl+S |
| Save preset as | Ctrl+Shift+S |
| Save As Guitar | Ctrl+G |
| New preset | Ctrl+N |
| Randomize | Ctrl+R |
| Reset all | Ctrl+Shift+R |
| A / B compare | Ctrl+/ |
| Tap tempo | T |
| Jam band start / stop, fill, arm (jam-mode.md 8.2) | J / Shift+J / Alt+J |
| MIDI Learn arm | Ctrl+L |
| Undo / Redo | Ctrl+Z / Ctrl+Shift+Z |
| Help | F1 |
| Show all shortcuts | Ctrl+? |
| Next / Prev Col 4 tab | Ctrl+] / Ctrl+[ |
| Setlist next / prev | PageDown / PageUp |
| Export (context-aware: tune / preset / take) | Ctrl+E |
| Reveal preset file | Ctrl+Alt+E |
| Reveal guitar file | Ctrl+Shift+E |
| New tune | Ctrl+T |
| Play / pause (tune transport) | Space |

## 18. Undo / redo

Every parameter change and structural edit (adding a mod route, editing
a rhythm pattern, moving a pedal, swapping a part, moving a pickup)
pushes onto an undo stack of 64. Undo groups events within 200 ms.

Snapshot recalls, preset loads, guitar loads and setlist steps push a
state boundary that undo respects; undoing across a boundary requires
holding Shift.

Undo state is per-plugin-instance and does not persist across sessions.

## 19. Feature-to-location index (canonical map)

Every backend feature. Rows absent from this table are not shipped in
the UI.

| Feature | Backend module | Primary UI location | Secondary access | Shortcut |
|---|---|---|---|---|
| Instrument load | Model::GuitarLibrary | Adv Col 1 GUITAR, Preset browser | Header preset | Ctrl+O |
| Save As Guitar | Workshop | Workshop header | - | Ctrl+G |
| Per-string tuning | TuningEngine | Adv Col 1 GUITAR | Easy: headstock click | - |
| Capo (fret / partial) | TuningEngine | Adv Col 1 GUITAR, Workshop capo drag | - | - |
| Temperament | TuningEngine | Adv Col 1 GUITAR | - | - |
| Body dimensions / wood / bracing | BodyEngine | Workshop body part | Adv Col 1 BODY summary | - |
| String material / gauge (per string) | StringEngine | Workshop strings part | Adv Col 1 STRINGS | - |
| String age | StringEngine | Adv Col 1 STRINGS | Easy character macro | - |
| Whammy | WhammyEngine | Adv Col 1 WHAMMY, Workshop bridge part | Easy: bridge click | - |
| Pickup model / coil / magnet | PickupEngine | Workshop pickup part | Adv Col 2 PICKUPS summary | - |
| Pickup position | PickupEngine | Workshop drag on illustration | - | - |
| Pickup height / tilt | PickupEngine | Workshop scroll / drag screws | - | - |
| Pickup selector switch | PickupEngine | Adv Col 2 PICKUPS | Easy: pickup click | - |
| Guitar volume / tone | GuitarCircuit | Adv Col 2 CIRCUIT, Easy rig strip | Col 4 CHARACTER -> CIRCUIT mirror | - |
| Pot values / tone cap / bleed / cable / active | GuitarCircuit | Adv Col 2 CIRCUIT | Col 4 CHARACTER -> CIRCUIT, Workshop wiring / preamp parts | - |
| Pre-effects rack | PreEffectsChain | Adv Col 2 PRE-FX | Easy rig strip | - |
| Post-effects rack | PostEffectsChain | Adv Col 3 POST-FX | Easy rig strip | - |
| Amp model / tone / sag | AmpEngine | Adv Col 3 AMP | Easy rig strip | - |
| Cabinet, mic 1, mic 2 | CabinetEngine | Adv Col 3 CAB | Easy rig strip | - |
| Room | RoomEngine | Adv Col 3 ROOM | Easy rig strip | - |
| Feedback simulation | AmpEngine | Adv Col 3 SUSTAIN (feedback readout) | - | - |
| Freeze | MasterBus overlay | Adv Col 3 SUSTAIN | - | - |
| E-Bow | MasterBus / Feedback | Adv Col 3 SUSTAIN | - | - |
| Playing mode | MidiInterpreter | Easy playing strip | Adv header | - |
| MIDI Learn | Support::MidiLearn | Header MIDI Learn | Right-click any control | Ctrl+L |
| Preset browser | PresetSystem | Header preset name | - | Ctrl+O |
| A / B compare | PresetSystem | Header A / B | - | Ctrl+/ |
| Save preset | PresetSystem | Header save | - | Ctrl+S |
| Panic | MasterBus | Header panic | - | P |
| Randomize | Support | Header dice | - | Ctrl+R |
| Reset | Support | Header reset | - | Ctrl+Shift+R |
| Export audio | AudioExport | File menu, Options AUDIO | - | - |
| Export MIDI | MidiCapture | Col 4 MIDI OUT tab, drag-out on session recorder | Col 4 NOTATION | - |
| Import MIDI | MidiCapture | File menu, drag onto plugin | - | - |
| Help | UI | Header ?, Col 4 HELP | - | F1 |
| Debug | Diagnostics | Options DIAGNOSTICS | - | - |
| Easter egg | UI | Signature notch pixel | - | - |
| Bus layout / aux 1-8 / per-string / sidechain / MIDI out | Routing | Col 4 ROUTING | - | - |
| Aux 1 pre / post circuit toggle | Routing | Col 4 ROUTING | - | - |
| Aux 8 noise bus | Routing | Col 4 ROUTING | - | - |
| LFOs 1-8 | ModMatrix | Col 4 MOD -> LFO cards | Right-click any control -> Modulate | - |
| Envelopes 1-4 | ModMatrix | Col 4 MOD -> ENV cards | Same | - |
| Step sequencers 1-2 | ModMatrix | Col 4 MOD -> STEP cards | Same | - |
| Envelope followers 1-2 | ModMatrix | Col 4 MOD -> FOLLOW cards | Same | - |
| Macros 1-8 | ModMatrix | Col 4 MOD -> MACROS | Header (Live Mode) | - |
| Random source | ModMatrix | Col 4 MOD -> RAND card | Same | - |
| Mod routes | ModMatrix | Col 4 MOD -> route table | Drag / right-click any control | - |
| Chord detector / voicer style / density / hand pos | RhythmEngine | Col 4 RHYTHM tab | Easy rhythm strip | - |
| Strum pattern editor | RhythmEngine | Col 4 RHYTHM tab | - | - |
| Fingerpick pattern editor | RhythmEngine | Col 4 RHYTHM tab | - | - |
| Crossing velocity / accel / striker | StrumDynamics | Col 4 RHYTHM tab -> STRUM group | Col 4 CHARACTER -> PICK group mirror | - |
| Genre kit | RhythmEngine | Col 4 RHYTHM, Easy rhythm strip | - | - |
| Bass step grid | RhythmEngine (bass) | Col 4 RHYTHM tab (auto for bass) | - | - |
| Snapshots | LivePerf | Header strip, Col 4 LIVE setup | Live Strip | [ ], 1-9 |
| Setlist | LivePerf | Col 4 LIVE | Live Strip | PgUp/Dn |
| Morph | LivePerf | Col 4 LIVE, Live Strip knob | - | - |
| Tap tempo | LivePerf | Header tap | Live Strip pad | T |
| Jam band (jam-mode.md 8) | Jam | Col 4 JAM, Easy rhythm strip JAM group | Live Strip JAM pill (while `jam_enabled`) | J, Shift+J, Alt+J |
| Kill switch | LivePerf | Live Strip pill | - | \ |
| Monitor mix | LivePerf | Live Strip, Col 4 LIVE | - | - |
| Expression cal | LivePerf | Options EXPRESSION | - | - |
| Controller profile / latency wizard / multi | Controllers | Col 4 CONTROLLERS | - | - |
| Metronome / Looper / Track / Scale / Ear / Tab / Prog / Session | Practice | Practice drawer + Col 4 PRACTICE setup | - | - |
| User IR slots, Cab match, EQ match, Capture, IR library | ToneMatch | Col 4 TONE MATCH | - | - |
| Notation export | Notation | Col 4 NOTATION, File menu | Col 4 MIDI OUT for the MIDI portion | - |
| Live TAB view | Notation | Col 4 NOTATION, Practice drawer TAB | - | - |
| Chord symbol history | Notation | Col 4 NOTATION | - | - |
| MIDI export profile (Luthier / Generic) | MidiExport | Col 4 MIDI OUT | - | - |
| Character seed / dead spots / fret wear / tuner drift / body age / environment | CharacterWear | Col 4 CHARACTER | Easy character macro | - |
| String squeak amount / probability / material / style | StringNoise | Col 4 CHARACTER -> STRING NOISE | Easy character macro (amount only) | - |
| Pick material / thickness / tip / bevel / angle / wear / click / chirp / scrape | PickNoise | Col 4 CHARACTER -> PICK | - | - |
| Fret buzz thresholds / setup style / sitar mode / heatmap | FretBuzz | Col 4 CHARACTER -> SETUP | Workshop setup strip | - |
| Slide material / mass / wall / pressure / slant / noise / clank | SlideEngine | Col 4 CHARACTER -> SLIDE (Slide Mode only), Workshop slide part | Header S | S |
| Guitar circuit visualiser | GuitarCircuit | Adv Col 2 CIRCUIT, Col 4 CHARACTER -> CIRCUIT | - | - |
| Advanced-range per-preset toggle | AdvancedRanges | Options RANGES, header padlock | Right-click a control for per-control unlock | - |
| Workshop bench | Workshop | Header wrench, Col 4 WORKSHOP | - | W |
| Parts drawer (Body / Neck / Frets / Nut / Bridge / Tuners / Strings / Pickups / Wiring / Preamp / Pick / Slide / Capo) | Workshop | Workshop bench | Right-click illustration part | - |
| Inspector | Workshop | Workshop bench right column | - | - |
| Spectrum delta pane | Workshop | Workshop bench bottom right | - | - |
| Bench A / B (8 slots) | Workshop | Workshop bench header | - | - |
| Audition (Alt-hover) | Workshop | Workshop cards | - | - |
| Guided build (templates) | Workshop | Workshop custom mode rail | - | - |
| Bass slap / pop / ghost / LH slap / double thump / two-finger alternation / pluck position | BassTechniques | Col 4 RHYTHM (bass mode), CHARACTER (bass amounts) | Live from MIDI velocity zones | - |
| Tune builder (progression, melody, sections, setlist) | TuneBuilder | Col 4 TUNE | File menu New Tune | - |
| Chord progression editor | TuneBuilder | Col 4 TUNE progression strip | - | - |
| Melody piano roll | TuneBuilder | Col 4 TUNE melody strip | - | - |
| Melody generators (Auto / Draw / Record / Improvise / Style transfer / Sing) | TuneBuilder | Col 4 TUNE melody strip buttons | - | - |
| Section strip + setlist | TuneBuilder | Col 4 TUNE section tabs | - | - |
| Tune templates | TuneBuilder | File menu New Tune -> template picker | - | - |
| Bass line per section | TuneBuilder | Col 4 TUNE layers | - | - |
| Layers (pad / arp / countermelody / percussion) | TuneBuilder | Col 4 TUNE layer strip | - | - |
| Tune export (audio / MIDI / notation / project) | TuneBuilder | Col 4 TUNE export dialog | Header preset export menu | Ctrl+E |
| MIDI export profile / drag-out / SysEx / PPQ | MidiExport | Col 4 MIDI OUT tab, session recorder drag-out | - | - |
| MIDI import | MidiExport | File menu Import MIDI, drag-drop onto window | - | - |
| Accessibility opts / Localization / UI scale / Palette / Reduced motion | Accessibility | Options ACCESSIBILITY, LOCALIZATION, APPEARANCE | - | - |
| Updates / Telemetry / Crash reporting / License / Privacy dashboard | Updates | Options UPDATES, PRIVACY | Header notification | - |
| Noise floor | NoiseFloor | Adv Col 4 CHARACTER NOISE FLOOR | ROUTING Aux 8 switch; Options AUDIO default mains region | - |
| Sustain shape | StringEngine | Adv Col 1 STRINGS DECAY (style) + Col 4 CHARACTER SUSTAIN SHAPE | - | - |
| Tuning stability | StabilityModel | Adv Col 4 CHARACTER TUNING STABILITY | Easy headstock popover (per-string offset, Retune) | - |

## 20. Discoverability rules

- Every panel with more than one row of controls has a `?` icon; click
  opens Help pinned to that panel's docs.
- Every new feature added after 1.0 shows a "NEW" dot on its entry point
  for one week after first launch of the introducing version.
- Options -> Diagnostics -> "What's on the audio path right now" shows a
  live block diagram of active DSP stages.
- First unlock of an advanced range triggers a one-time explainer per
  advanced-ranges.md; explained once, never shown again unless the user
  resets first-run.

## 21. Empty-state, hint and marking rules for realism specs

- **Advanced-range portion of arc**: warning colour, `*` in readout.
- **Padlock icon** in CHARACTER and WORKSHOP tab headers: secondary
  accent when unlocked (advanced ranges on), muted when locked.
- **Slide bar overlay** on the fretboard: 6 px rounded bar in the slide
  material colour at 80% opacity, rotated by slant, 80 ms ease.
- **Pick overlay** in the illustration: pick drawn at true relative
  size, rotated by attack angle, drag handles 8 px accent circles.
- **Buzz heatmap**: warning colour for near-threshold, accent for
  buzzing frets, plus a small dot glyph so the map reads in monochrome.
- **Circuit visualiser**: mini EQ curve, updates live as volume / tone
  move.
- **Noise event strip**: 24 px scrolling strip of recent squeak / buzz /
  click events, height = intensity, colour by type. Hidden under
  reduced motion in favour of a static count.
- **Pressure-state readout** (slide): mono-font text label, no colour
  coding required.

## 22. Tests

- Every entry in section 19 resolves to a real component. Automated at
  startup in test mode: walk the table, attempt to programmatically
  focus each location; fail if any lookup returns null.
- Every parameter's automation ID resolves to exactly one focused
  control when triggered by right-click MIDI Learn's "focus source".
- Tab-order walk: Tab from the header, verify focus visits every
  visible interactive element in Col 1..4 order without revisit or skip.
- Reflow: instantiate at 800, 1000, 1280, 1600, 1920, 2560 window widths
  at 75, 100, 125, 150, 175, 200% UI scale; verify no clipping.
- Empty-state hints: for every panel, force the empty state and verify
  hint text is present and legible.
- Right-click menu: for every automatable parameter, verify all 13 menu
  items are present and functional; verify the two range-unlock items
  are shown only in the correct state.
- Undo / redo: 1000-op random walk across all feature areas including
  Workshop swaps, verify final state matches initial after full undo.
- Workshop hit-testing: 10 000 random clicks on the illustration select
  the intended part, never a part below it in z-order.
- Slide Mode toggle: on / off 100x during playback, verify no click and
  correct panel visibility.
- Advanced-range marking: verify warning-colour arc portion appears
  when a knob passes the stock max, disappears when returned.
