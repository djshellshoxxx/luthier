# PRACTICE TOOLS SPEC

Fleshes out the one-line "practice tools" note in spec.md and the
"Not yet implemented" line in PROGRESS.md. These tools run inside the plugin
so a guitarist can practise without leaving Luthier.

## 0. Ground rules

1. Practice tools run in an isolated panel that can be closed. When closed
   they consume no CPU.
2. The metronome click is routed to the monitor bus (see
   `live-performance.md`) by default. It can optionally route to the main
   out.
3. Loops record the plugin's own audio output, not the raw MIDI. Users can
   change the tone after recording and the loop re-renders through the new
   tone using stored MIDI, up to the loop's max length.
4. Backing tracks stream from disk. Never load a whole track into RAM.
5. No practice tool touches the audio thread's DSP path. Practice audio is
   summed at the master bus, respecting the tool's own gain slider.

## 1. Metronome

- Time signatures: 2/4, 3/4, 4/4, 5/4, 6/8, 7/4, 7/8, 9/8, 12/8, custom
  (numerator 1-32, denominator 2/4/8/16).
- Tempo: 20-300 bpm. Follows plugin-global tap tempo and host tempo per
  live-performance.md rules.
- Accent map: per-beat accent on/off, three velocity levels (accent, normal,
  ghost).
- Subdivisions: quarter, eighth, triplet, sixteenth, dotted variants. Each
  subdivision has its own click sample and gain.
- Click sounds: wood block, cowbell, digital blip, side-stick, shaker, tap.
  Ship 6 pairs (accent + normal) at 24-bit / 48 kHz mono, 16 files total.
- Silent bars: mute every Nth bar to force internal timekeeping (N is 1-16).
- Progressive tempo: ramp bpm from A to B over N bars, useful for building
  speed.
- Visual: a 4-dot pulsing indicator in the practice panel.

## 2. Looper

- Loop length: 1 to 240 seconds, quantized to bars if the metronome is
  active.
- Layers: up to 8. Each layer records the plugin's MIDI plus a rendered
  audio snapshot for playback fidelity when the preset changes.
- Undo/redo per layer.
- Reverse and half-speed per layer (audio only, MIDI re-rendered would
  change pitch so it stays forward).
- Overdub, replace, and "play once" modes per layer.
- Per-layer volume, pan, low-cut, high-cut.
- Export: bounce all layers to a single WAV, or bounce each layer as a stem.
- Save loop state to `.luthierloop` file; includes MIDI, audio snapshots,
  and per-layer settings.

Storage: audio snapshots use 24-bit float WAV under a temp folder while the
loop is unsaved, moved to the loop file on save.

## 3. Backing track player

- Formats: WAV, AIFF, FLAC, MP3 (MP3 via dr_mp3 header-only, no LGPL
  dependency).
- Streaming from disk with a 4-second ring buffer per file.
- Independent volume, pan, mono/stereo, low-cut, high-cut.
- Loop points: start and end, snap to zero crossings.
- Pitch shift: -12 to +12 semitones without tempo change (phase vocoder or
  SoundTouch, decided at build time).
- Tempo shift: 25% to 200% without pitch change.
- Section marker support: user-added markers, jump-to hotkeys, name display.
- Auto-detect tempo on load (aubio-style tempo estimator).
- Playlist: sequence multiple backing tracks with gapless playback.

Location: user's own files. The plugin never bundles copyrighted audio.

## 4. Scale and mode trainer

Shows a scale on the fretboard, tests the user's knowledge.

Modes:
- **Explore**: pick a key and a mode, fretboard highlights all notes in
  scale. Optional overlays: intervals, degrees, notes.
- **Quiz**: fretboard hides notes; plugin asks "play the 5th of A Dorian",
  detects the played note via MIDI, scores.
- **Interval trainer**: two notes played, user picks the interval from a
  list. Progressive difficulty.
- **Chord tone trainer**: chord plays, user must play the 3rd and 7th
  within N seconds.

Ship all diatonic modes, harmonic minor, melodic minor, pentatonic and
blues in every key. Custom scales editable via interval list.

## 5. Ear training

- Interval recognition: 12 intervals, ascending, descending, harmonic.
- Chord quality recognition: major, minor, diminished, augmented, 7, maj7,
  m7, m7b5, dim7, sus2, sus4.
- Progression recognition: I-IV-V, I-vi-IV-V, ii-V-I, 12-bar blues,
  Andalusian cadence, and 10 more.
- Difficulty auto-adapts to user's success rate.
- Session stats stored in `~/Documents/Luthier/Practice/stats.json`.

Uses Luthier's own guitar sound for the exercises, so ear training happens
in-context.

## 6. Tab reader

- Formats: Guitar Pro 5 (.gp5), Guitar Pro 6+ (.gp), ASCII tab, MusicXML,
  PowerTab (.ptb).
- Displays a scrolling tab view with cursor, tempo control, section
  looping, count-in.
- Highlights the current fret on the built-in fretboard.
- "Speed trainer" mode: play a section, incrementally increase tempo N%
  each pass until the user misses notes.
- Note detection uses the same MIDI-in that drives the plugin: user plays
  along, plugin listens, scores accuracy.

Ship no bundled tab files. User loads their own.

## 7. Chord progression looper

- Enter a progression as chord symbols: `Am - F - C - G x4`.
- Luthier voices, strums (per rhythm-engine.md), and loops it.
- Adjustable tempo, feel, genre kit.
- Useful as a solo backing when no backing track is loaded.

## 8. Session recorder

- Records everything the plugin outputs plus incoming MIDI, continuously,
  into a ring buffer of user-set size (default 60 minutes at 48 kHz stereo
  float32 = ~1.4 GB).
- "Save last take" button freezes the buffer to WAV plus MIDI file, named
  by timestamp.
- Ring buffer lives in `~/Documents/Luthier/Sessions/tmp/`.
- Auto-cleanup after 24 hours unless saved.
- Disabled by default; enabled in Options.

## 9. UI

Practice panel is a slide-out drawer from the bottom of the plugin window,
32-360 px tall, resizable. When collapsed it is a 32-px strip showing
metronome bpm, loop status LED, and backing-track title.

Tabs across the top of the drawer:
`METRO | LOOP | TRACK | SCALE | EAR | TAB | PROG | SESSION`

Each tab has its own compact UI, sharing style with the main plugin.

Global practice controls in the drawer strip:
- Master practice volume (does not affect main out gain).
- Tap tempo (mirrors global tap).
- Panic (silences all practice tools instantly, does not affect main audio).

## 10. Data locations

| | Path |
|---|---|
| Metronome click samples | `Resources/Practice/Clicks/*.wav` |
| Backing tracks | wherever the user pointed |
| Loop files (user) | `~/Documents/Luthier/Loops/` |
| Practice stats | `~/Documents/Luthier/Practice/stats.json` |
| Session temp buffer | `~/Documents/Luthier/Sessions/tmp/` |
| Session saves | `~/Documents/Luthier/Sessions/` |

## 11. Tests

- Metronome accuracy: verify inter-click interval is +/- 0.5 ms at 48 kHz
  for 60 s at 120 bpm.
- Looper round trip: record a MIDI phrase, verify audio snapshot matches
  a fresh render of the same MIDI to within -80 dBFS null.
- Backing track playback: stream a 60-minute WAV, verify no memory growth
  beyond the ring buffer size.
- Tab reader: parse the 50 most-downloaded Guitar Pro files from a fixture
  set, verify no crashes and note count matches published counts.
- Session recorder: verify ring buffer never allocates in the audio thread.
