# Host compatibility

What each DAW does slightly differently, and what Luthier does about it.
This is host-integration.md section 9 in user-facing language, kept
current with the actual build rather than the spec's aspirations - a row
marked "known gap" here has no code behind it yet.

## Ableton Live

- **Program change right after loading a project.** Live calls
  `setCurrentProgram(0)` immediately after restoring a saved session, to
  put the plugin back on its first program. Luthier swallows that one
  program change so the session you saved is not immediately replaced by
  a factory preset. Every later program change (from Live's own PC
  automation, a controller, or the header) works normally.
- **Sidechain send.** Some Live versions do not route a sidechain send to
  a VST3 side input correctly. You need to wire the send in Live; Luthier
  cannot force it from its side.
- **MPE.** Live 11+ sends MPE correctly. Luthier does not yet auto-detect
  MPE traffic and switch profiles on its own (known gap) - pick an MPE
  profile under Options > Controllers.

## Logic Pro

- **State size.** Logic has historically had trouble with very large AU
  states. Luthier keeps a preset's guitar as a file reference rather than
  inlining it whenever the guitar is unedited, which keeps a typical
  state well under Logic's old ceiling.
- **Program Change / Bank Select mapping.** Logic's Bank Select + PC combo
  differs from most hosts. Luthier currently maps Program Change to
  snapshot recall and CC0 to preset selection, fixed; a routing-panel
  control to choose a different mapping (PC-to-preset, Bank-Select+PC, or
  off) does not exist yet (known gap).
- **Re-preparing on preset change.** Logic sometimes calls `prepareToPlay`
  again when you change presets. Luthier's `prepareToPlay` is idempotent
  and fast, so this is silent.

## Cubase / Nuendo

- **VST3 note expression.** Cubase's native note-expression lanes are not
  wired to Luthier's MIDI interpreter; MPE input works normally instead.
  Native VST3 note expression is a later-version target.
- **Track archive.** Cubase's track archive round-trips Luthier's state
  normally.

## Studio One

No known quirks. Automation and window handling behave as any well-formed
VST3 plugin.

## Reaper

No known quirks. Reaper's multi-output routing exposes every bus layout
(A-D, including the per-string layout and Aux 8) without extra setup.

## FL Studio

FL wraps VST3 in its own plugin wrapper. State save/restore and MIDI both
work through the wrapper with no special handling needed on Luthier's
side.

## Bitwig Studio

- **CLAP.** Bitwig prefers CLAP; Luthier ships an optional CLAP target
  alongside VST3.
- **The Grid / modulators.** Both can target Luthier's sidechain input and
  any exposed parameter normally.

## Pro Tools

Pro Tools requires AAX, which Luthier does not build (VST3-only hosts are
out of scope for v1). Requires an AAX target before Pro Tools is
supported at all.

## Standalone

- **Audio device disconnect.** The standalone does not yet poll for a
  disconnected audio device and offer to reconnect (known gap; JUCE's
  default standalone device handling applies until this is built).
- **Virtual MIDI output.** No in-app toggle yet to create a virtual MIDI
  output for routing Luthier's MIDI-out to another app (known gap).
- **File association.** Double-clicking a Luthier file (`.luthierpreset`,
  `.luthierguitar`, `.luthiertune`, `.luthierloop`, `.luthierset`,
  `.midprofile`) opens it in the standalone on Windows and Linux, where
  the installer registers the file types. macOS does not yet declare
  these document types in the app bundle (known gap; see installer.md
  IN-20).
- **Window.** Standard resizable JUCE window with a minimum size, native
  file dialogs.

## Every host

- **MIDI in:** channels 1-16, notes, CC, pitch bend, aftertouch, program
  change.
- **MIDI out:** the VST3 and AU builds announce a MIDI-out port
  (`NEEDS_MIDI_OUTPUT`); a host that never creates one for an instrument
  plugin (rare) simply will not offer Luthier's MIDI-out connections.
- **Main output:** stereo only. A host offering only a mono main bus
  cannot load Luthier (host-integration.md HI-10) - this matches most
  amp/cab/room-modelling plugins, which assume a stereo field.
- **State:** JSON, not XML, but content-equivalent to the spec's
  described sections; each state records a `formatVersion` so a future
  build can tell an older blob apart from its own and migrate safely.
