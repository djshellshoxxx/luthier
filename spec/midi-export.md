# MIDI EXPORT AND IMPORT SPEC

Every event class the plugin produces (notes, bends, slides, strums,
squeaks, pick clicks, buzz, slide bar position, workshop changes,
character events, bass technique events) exported as MIDI in one of two
profiles:

- **Luthier profile**: lossless. Every event class round-trips.
  Re-importing a Luthier-profile file renders identical audio to the
  original within -60 dBFS RMS null.
- **Generic profile**: notes, standard CCs, standard pitch bend. Realism
  events export as human-readable text meta events for reference. A
  DAW that doesn't know Luthier can still play the file back through
  any GM-ish instrument.

Also covers live MIDI-out on the routing panel, drag-out export from
the session recorder, and MIDI in for the tune-builder and looper.

## 0. Ground rules

1. Export runs on a worker thread from a captured `PerformanceScore`
   or from the tune-builder's `Tune`. Never on the audio thread.
2. Import parses to `PerformanceScore` or `Tune`; the audio thread
   receives the parsed data via the standard swap.
3. **Luthier profile is self-describing**: every extension event
   carries its schema version in the meta header so future readers
   handle old files.
4. Timestamps are **sample-accurate** on the audio-thread event stream
   and **beat-accurate** when written to standard MIDI ticks (default
   960 PPQ).
5. Nothing in either profile depends on a specific host or DAW.

## 1. Standard MIDI subset (both profiles)

Both profiles write:
- Standard MIDI file, format 1 (multi-track).
- 960 PPQ ticks per quarter note (configurable 96 - 3840 in Options).
- Track 0: meta (title, copyright, tempo map, time signature, key
  signature).
- Per-instrument tracks: notes, pitch bend, aftertouch, program
  change, CC.

Note-on / note-off carry channel, note number, velocity. Pitch bend at
14-bit resolution.

For MPE-style exports (one channel per string), tracks are per string;
otherwise one track per instrument.

## 2. Luthier profile

Adds an "LUTHIER" chunk in the meta track's first message and uses a
reserved SysEx range for extension events. Every event is preceded by
a `LUTHIER-BEGIN <event-class> <schema-version>` text meta and followed
by a `LUTHIER-END` marker, so a stripped file (Luthier chunks removed)
still plays as valid Generic MIDI.

### 2.1 Event classes

| Class | Contents |
|---|---|
| `NOTE` | Note on / off, plus per-note technique flags (palm mute, ghost, harmonic type, tap, pinch, natural, artificial) |
| `BEND` | Curve, start pitch, end pitch, duration, articulation (whole, half, release, ghost bend) |
| `SLIDE` | Legato / shift / in / out, start fret, end fret, duration |
| `VIBRATO` | Rate, depth, delay, technique (finger, wrist, whammy) |
| `WHAMMY` | Bar events with curve, target semitones, duration |
| `STRUM` | Direction, crossing_velocity_sps, striker, mute, string mask |
| `RASGUEADO` | Sub-strum sequence with per-finger data |
| `PICK` | Material, thickness, angle, tip, wear, chirp intensity (per note) |
| `SQUEAK` | Trigger type (slide / shift / drag / release), duration, intensity, material |
| `BUZZ` | Fret, duration, intensity, sitar flag |
| `SLIDE_BAR` | Position (continuous), pressure state, slant, material change |
| `CLANK` | Trigger type, string mask, intensity |
| `CHARACTER` | Seed changes, environment changes (temperature, humidity) as they occur |
| `WORKSHOP` | Part swaps that happen during a performance (rare) |
| `BASS_TECH` | Slap, pop, ghost, LH slap, double thump, pluck position |
| `RANGES` | Advanced-range toggles inside the performance (rarer) |
| `SNAPSHOT` | Snapshot recalls at their sample-accurate time |
| `SECTION` | Tune-builder section boundaries |

Each class has a stable byte-level encoding documented in
`docs/MIDI_EXPORT_LUTHIER_PROFILE.md`. Encoding is compact but
readable when the file is inspected in a hex editor: fields are
tagged.

### 2.2 Round-trip guarantee

Re-importing a Luthier-profile file into the same plugin version
renders audio identical to the source within -60 dBFS RMS null across
the full test suite (qa-polish.md 6).

Re-importing into a later plugin version reads every event class the
version supports; unknown event classes are preserved as opaque blobs
on save, so no data is lost even in a downgrade / upgrade cycle.

### 2.3 SysEx compatibility

Every extension event doubles up: a text meta description for
human-readability, plus a SysEx encoding for compact machine parsing.
The plugin reads either; it writes both for redundancy on export.

## 3. Generic profile

Standard MIDI as any DAW expects. Realism events export as text meta
events with a `LUTHIER:` prefix so a DAW that doesn't parse them
ignores them, but a human reading the MIDI file sees exactly what
was there:

```
0:00:03.500 [Text] LUTHIER: STRUM Down cv=200 striker=pick mute=0
0:00:05.100 [Text] LUTHIER: SQUEAK slide dur=140 intensity=0.6 material=phosphor
```

Notes, pitch bend, and CC 1 (mod wheel), CC 11 (expression), CC 64
(sustain), CC 74 (timbre) carry as much of the performance as the
standard MIDI protocol can express.

Pitch bend range: written to CC 100 / 101 / 6 at the start of every
track so hosts that respect it interpret bends correctly.

## 4. Export UI

Two paths: the export dialog and the drag-out.

### 4.1 Export dialog

Opens from Col 4 MIDI OUT tab, or from the tune-builder's export
dialog, or from File -> Export -> MIDI.

Fields:
- Profile: Luthier or Generic.
- Range: entire capture, current section (tune only), last N seconds,
  marked region.
- Track split: single track, per section, per instrument (main + bass),
  per string.
- Include realism: yes / no (Generic profile only; Luthier always
  includes).
- PPQ resolution.
- Destination path.
- Preview: shows the first section's opening bar as text summary.

### 4.2 Drag-out

The session recorder's "Save last take" button also drags: click and
hold, drag out of the plugin, drop into a DAW arrange view or a file
manager. The drop target receives a valid MIDI file (Luthier profile
by default; hold Alt to force Generic).

## 5. Import UI

File -> Import -> MIDI, or drag a `.mid` file onto the plugin window.

- Auto-detects Luthier profile by header chunk.
- Generic profile imports as a `PerformanceScore` with default realism
  values.
- Import target: current session (adds to session recorder), tune
  builder (loads as a new tune), or looper (loads as a layer).

## 6. Live MIDI-out (routing panel)

Per routing-io.md 6, the plugin sends MIDI on the host's MIDI-out
channel. Sources selectable in the routing panel:

- Note pass-through.
- Rhythm engine output.
- Tune-builder playback.
- String activity (per-string).
- CC broadcast (macros).
- Character / noise events (Luthier profile SysEx).
- Workshop changes (rare, mostly for automation lanes).

All alignment sample-accurate. Non-Luthier hosts silently drop SysEx.

## 7. File extension and profile registration

- `.mid` and `.midi` for both profiles (indistinguishable by
  extension; header chunk disambiguates).
- Installers register the plugin as a handler for `.midprofile`, a
  small JSON file that describes a user-customised profile subset
  (which event classes to include, PPQ, track split). Users can save
  their preferred export config as a `.midprofile` for reuse.

## 8. Options

Options -> MIDI includes:
- Default profile.
- Default PPQ.
- Default track split.
- "Always include realism in Generic profile" (default off; realism as
  text metas).
- SysEx redundancy on / off (Luthier profile only; off to save size).

## 9. Interaction with other specs

- **notation-export.md**: MusicXML / Guitar Pro exporters consume the
  same `PerformanceScore` used here. Notation and MIDI are two
  serialisations of the same data.
- **tune-builder.md**: the tune's export dialog calls this module for
  the MIDI portion.
- **practice-tools.md** session recorder writes a `.mid` alongside its
  WAV on save; profile follows Options default.
- **live-performance.md** snapshots exported per SNAPSHOT event class.
- **modulation-matrix.md**: macro CCs export as CC broadcast when the
  routing panel's macro-to-CC map is set.
- **rhythm-engine.md** strums and fingerpicks export as STRUM and
  PICK events (Luthier) or as per-string notes with velocity spread
  (Generic).
- **bass-techniques.md** bass events export as BASS_TECH.
- **advanced-ranges.md**: parameters outside stock range annotated in
  the Luthier profile so a re-import warns before applying.
- **tab-export.md**: a Tune's MIDI export may source its
  `PerformanceScore` from `TuneToScore` (a direct, symbolic conversion of
  the written tune) instead of from a live `PerformanceCapture` render;
  either way the same writer in sections 1-2 runs and the same round-trip
  guarantee (2.2) holds.

## 10. Backward compatibility

`.mid` files exported by earlier Luthier versions load in later
versions with any missing extension events reconstructed from the
plain-MIDI portion. Fields that can't be reconstructed use current
defaults; a load notification lists them.

## 11. Privacy

MIDI export contains no personal data. Character seed, guitar name,
preset name are omitted from the Generic profile by default; the
Luthier profile includes them for round-trip fidelity but with a
"strip identifiers" option in the export dialog.

## 12. Tests

- Luthier profile round trip: for every factory preset and every
  fixture performance, export -> import -> render matches source
  render within -60 dBFS RMS null.
- Generic profile round trip: export -> import -> render matches
  source render for the pitch / velocity / bend / CC content within
  -30 dBFS RMS null (realism events lost by design).
- SysEx redundancy: with SysEx off, Luthier profile still round-trips
  via text metas.
- PPQ scaling: same performance exported at 96, 480, 960, 3840 PPQ
  imports back with identical timing within 1 sample at each rate.
- Drag-out: initiate a drag from the session recorder, drop into a
  DAW fixture, verify the file is valid MIDI.
- Header disambiguation: a Luthier file with the "LUTHIER" chunk
  removed loads as Generic without warning.
- Import corrupt file: every byte flipped one at a time; import
  refuses gracefully (no crash, banner with line reference).
- Live MIDI-out timing: for a fuzz of 10 000 events across all
  sources, echoed timestamps match internal ticks within 1 sample.
- Live MIDI-out SysEx: non-Luthier host drops SysEx without error;
  Luthier receiving on another instance parses correctly.
