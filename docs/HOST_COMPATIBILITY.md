# Luthier in your DAW

Luthier is a VST3 and Audio Unit instrument, with a CLAP build where the
installer provides one, plus a standalone app. It works in every major host.
Hosts differ in a few small ways, and this page lists what you might notice
and what to do about it.

## Everywhere

- **Your session comes back as you left it.** The host saves the whole
  instrument with the project: the preset, snapshots, modulation, routing, the
  tune you were building and the practice settings.
  - A project saved by a newer Luthier opens in an older one with a notice.
    Anything the older version does not understand is kept, and saved back
    unchanged.
  - When a newer Luthier opens an older project, it first copies the saved
    state to `Documents/Luthier/Diagnostics/state-backup-*.json`. It keeps the
    last ten copies. If an upgrade ever changes how a project sounds, that
    copy is the original.
- **MIDI out.** Luthier has a MIDI output: the notes as played, the rhythm
  engine, tune parts and Luthier's own events. Switch it on under
  ADVANCED > ROUTING / MIDI OUT, then route the track's MIDI output in your
  host. A few hosts do not route MIDI from instrument plugins at all. There,
  use Luthier's MIDI export (File > Export MIDI) instead.
- **Stereo only.** Luthier's main output is stereo. A host that asks for a mono
  instrument track is told the layout is not supported. Use a stereo track, or
  sum to mono after Luthier.
- **Tempo.** Luthier follows the host's tempo and time signature while the
  transport runs. With the transport stopped, a tapped tempo takes over, or an
  incoming MIDI clock if your setup sends one. The practice metronome follows
  the same tempo unless you type in a tempo of your own.
- **Program changes** step through Luthier's presets in list order.

## Ableton Live

- **Opening a project gives the sound you saved, not preset 1.** Live sends
  "program 0" straight after restoring a plugin. Luthier ignores that one
  program change, and every later one works normally.
- **Sidechain.** Some Live versions do not send audio to a VST3 side input
  unless you pick Luthier as the destination in the sending track's
  "Audio To" menu. Luthier cannot set that up from its side.
- **MPE.** Live 11 and later pass MPE through. If an MPE controller is playing
  and MPE is off in Luthier, a notice suggests switching it on so each note
  gets its own bend and slide.
- **Rack presets** store Luthier's state like any project, alongside
  Luthier's own presets.

## Logic Pro

- **Session size.** Logic has had trouble with very large Audio Unit states.
  Luthier refers to its guitars and IRs by file rather than storing them
  inline, so a typical session is well under 200 KB.
- **Bank Select + Program Change.** Logic sends these together. Luthier reads
  the pair as one preset choice, stepping through its presets in list order.
- **Changing a preset** can make Logic re-prepare the plugin. This is expected
  and fast; there is no gap in playback beyond the preset's own crossfade.

## Cubase and Nuendo

- **Note Expression.** MPE input works. Cubase's own VST3 Note Expression is
  not read yet.
- **Track archives** keep Luthier's state.

## Studio One

Nothing special: automation, window handling and state all behave as
expected.

## Reaper

- Continuous automation, and every output layout (the aux buses and the
  per-string outputs), work as expected. Route Luthier's extra outputs from
  the track's routing window.

## FL Studio

- FL wraps VST3 plugins in its own wrapper. State saves and restores
  correctly through it. If a project ever reopens with the wrong sound, the
  copy in `Documents/Luthier/Diagnostics` (see above) has what was saved;
  please report it with that file.

## Bitwig Studio

- Use the CLAP build where your installer provides one. The VST3 works too.
- Bitwig's modulators can drive any Luthier parameter.
- Signals from The Grid can feed Luthier's sidechain.

## Pro Tools

Pro Tools needs AAX, which Luthier does not ship yet. Use the standalone app,
or another host, until it does.

## Standalone app

- Choose the audio device and MIDI input in the app's audio settings. If the
  device disconnects, pick it again there once it is back.
- The window resizes down to a minimum size. File dialogs are your
  system's own.
