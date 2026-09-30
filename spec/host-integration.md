# HOST INTEGRATION SPEC

The plugin-to-host surface. Automation, parameter change notification
batching, latency reporting, bus layout negotiation, transport
follow, DAW-specific quirks worth flagging.

Every DAW is a slightly different consumer of the plugin's API. This
document captures what to advertise, what to accept, and where each
major host has quirks that need workarounds.

## 0. Ground rules

1. **Follow the host's contract exactly.** Don't guess; if VST3 says
   `getStateInformation` returns bytes, return bytes.
2. **The plugin is a good citizen.** No blocking calls on the audio
   thread. No GUI drawing outside `paint`. No `std::cout`.
3. **Announce every capability.** Latency, bus layouts, MPE
   compatibility, MIDI I/O: all correctly reported so hosts can plan.
4. **Follow the host's transport.** When the host plays, its tempo
   and position win over any internal tap. When the host stops,
   internal transport can take over (rhythm engine free-run).
5. **Never surprise the host.** Bus layout changes, parameter count
   changes, latency changes happen only at documented moments
   (usually `prepareToPlay` or state restore).
6. **Save what the host asks for and no more.** Don't put IR blobs
   in the plugin state; put references. Bloated states break session
   saves in hosts.

## 1. Plugin formats and versions

- **VST3** (Steinberg): primary format, all platforms.
- **AU** (Apple): macOS.
- **CLAP** (Bitwig et al.): planned v1.1, not v1.0.
- **AAX** (Avid, Pro Tools): planned v1.5, not v1.0.
- **Standalone**: bundled on every platform.

Version reported to host: same as `LUTHIER_VERSION` at build time.
Both `major.minor.patch` and a `build` string.

## 2. Bus layouts

Advertised via `BusesProperties`. Every layout must run correctly.

Per routing-io.md 1:
- Layout A: main stereo (default; always supported).
- Layout B: main stereo + 8 auxiliary stereo pairs.
- Layout C: main stereo + up to 12 mono per-string.
- Layout D: main stereo + 8 aux stereo + 12 mono per-string.

`isBusesLayoutSupported`:
- Returns true for A, B, C, D and any subset (e.g. main + 3 aux).
- Returns false for any layout with a mono main out (guitar plugin
  requires stereo output; hosts requesting mono get a "not
  supported" and can fall through to layout A).

Sidechain input:
- Advertised as an optional stereo input on layouts B, C, D.
- Layout A also accepts sidechain if the host requests it (dual-mono
  input becomes stereo input).

Bus layout changes trigger a `prepareToPlay`. Never a crash; the
worst case is a brief silence gap while buffers reprepare.

## 3. Parameters

- APVTS is the single source of parameters.
- Parameter count is stable across the plugin lifetime; no
  parameters added or removed after construction.
- Parameters are grouped by `ParameterCategory` (matches
  gui-integration.md sections).
- Every parameter has:
  - Stable string ID.
  - Display name (translated per locale).
  - Range (per PhysicalRange if applicable).
  - Default.
  - Text-to-value and value-to-text converters.

### 3.1 Parameter change notifications

Host reads parameter changes via `getRawParameterValue` polling or via
`AudioProcessorParameter::Listener`. The plugin also emits
`sendParamChangeMessageToListeners` when internal changes (a snapshot
recall, a preset load) modify a parameter's value.

Batching: within one block, if multiple internal writes touch the
same parameter, only the last is notified.

### 3.2 Automation vs modulation

Per ambiguity-resolutions.md and modulation-matrix.md 7:
- **Automation** writes to the base parameter value (moves the
  on-screen control).
- **Modulation** adds on top (does not move the control).

The distinction is invisible to the host: host automation reads the
final value which reflects both.

### 3.3 Discrete parameters

Discrete parameters (amp model, cab model, pickup selection) expose
integer values 0..N-1. Host automation writes an integer; the plugin
smoothly crossfades between the discrete states per each module's
crossfade rule (5-30 ms, module-defined).

## 4. State serialization

`getStateInformation` returns a binary blob containing:
- Format version tag (u32).
- APVTS state (XML via `AudioProcessorValueTreeState::createXml`).
- uiState VT (XML).
- Structural state:
  - Mod matrix (JSON blob).
  - Snapshot bank (JSON blob).
  - MIDI mappings (JSON blob).
  - Ranges block (JSON).
  - GuitarSpec reference (path or inline).
  - Circuit state (redundant with parameters).
  - MIDI export profile selection.
- Padding for future extensions.

Total size target: < 200 KB for a typical preset, < 2 MB for a
preset with inline GuitarSpec and full mod matrix. Never MB of IR
data (IRs are file references only).

`setStateInformation` accepts the blob, parses, applies via the swap
pattern in ui-wiring.md 5. Handles old formats via migration.

### 4.1 Forward compatibility

Older builds loading newer blobs: read the format version; if
newer, load only the sections the older build understands, warn the
user, save preserves the unknown sections on write-back.

### 4.2 Backward compatibility

Newer builds loading older blobs: run migrations, back up the old
blob to the plugin's diagnostics folder before overwriting.

## 5. Latency reporting

- `getLatencySamples` returns the main-output latency in samples at
  the current sample rate.
- Latency changes at `prepareToPlay` and may change at other
  moments (bus layout change, IR load); a change triggers
  `updateHostDisplay` so hosts can recompute delay compensation.
- Per-output latency (routing-io.md 7): reported via the JUCE bus
  latency API where the host supports it.
- CPU quality (cpu-quality-modes 2.2) never changes the reported latency:
  a capped stage is padded to the nominal factor's latency.
- `setNonRealtime` (cpu-quality-modes 2.6): Luthier overrides it and also
  checks `isNonRealtime()` in `prepareToPlay` and each `processBlock`. An
  offline render runs at High (with "Always render offline at High", and
  always under Auto), switched hard, so bounces are bit-identical whatever
  the live level.

## 6. Transport follow

Every `processBlock`:
- Call `getPlayHead()->getPosition()` to get transport info.
- Read `tempoBpm`, `timeSignature`, `isPlaying`, `isRecording`,
  `positionInBeats`, `sampleRate` (host, may differ from prepare).
- RhythmEngine, TuneBuilder, metronome, LivePerf tap all consume
  these.

Missing playhead: fall back to internal transport (rhythm engine's
tap-tempo state).

## 7. MIDI I / O

- `acceptsMidi`: true.
- `producesMidi`: true (per routing-io.md 6).
- MIDI channel range: 1-16.
- MIDI clock, transport, note, CC, pitch bend, aftertouch, program
  change, SysEx: all accepted.
- MPE: full support (controllers.md).
- Timing: input events carry sample-accurate timestamps; output
  events written to the same buffer with correct timestamps.

## 8. Multiple instances

Each instance is fully independent (state-model.md 11). No shared
memory except:
- User-global settings file (read-only from the plugin's view;
  written only via the Options overlay).
- Content-update folder (read-only from the plugin's view).
- Factory content folder (read-only).

Two instances can save presets to the same file (last writer wins,
per file-formats.md 13).

## 9. Host-specific quirks

Every DAW behaves slightly differently. Known quirks and how the
plugin handles each. This list will grow as bug bashes reveal more.

### 9.1 Ableton Live

- **Follows-focus behaviour**: Live re-focuses plugin windows on
  clip start. Handle: the plugin ensures its window position is
  stable (uses JUCE's default position handling).
- **Automation shape**: Live sends "point" automation (single
  values), not continuous. Handle: standard APVTS parameter reads
  cover this.
- **Rack presets vs plugin presets**: Live's rack presets bypass
  the plugin's own preset system. Handle: nothing needed; both work.
- **Program change after state restore**: Live calls
  `setCurrentProgram(0)` immediately after `setStateInformation`, to
  "restore" the plugin to its first program. Because section 12 makes
  `setCurrentProgram` actually load a preset, obeying that call
  overwrites the state Live has just restored - the user reopens a
  project and gets a factory preset instead of the sound they saved.
  Handle: the first program change after a state restore is swallowed.
  State the host restored wins over the program change that follows
  it; every later Program Change works normally, so section 12's
  Program-Change addressing is unaffected.
- **Send-only sidechain**: some Live versions don't route sidechain
  correctly to VST3 side inputs. Handle: user must configure the
  send in Live; the plugin can't force it.
- **MPE**: Live 11+ supports MPE inputs correctly; earlier
  versions did not. Handle: detect via the MidiInterpreter's
  auto-detect on channel-1-plus-member-channels traffic.

### 9.2 Logic Pro

- **AU state size limit**: Logic has historically had issues with
  AU states > 1 MB. Handle: keep state under 500 KB by using file
  references for GuitarSpec (path, not inline) whenever possible.
- **PC handling**: Logic's Bank Select + PC combination differs
  from most hosts. Handle: routing panel exposes the PC mapping
  mode explicitly.
- **Reboot on preset change**: Logic sometimes reprepares the
  plugin on preset change. Handle: `prepareToPlay` must be
  idempotent and fast.

### 9.3 Cubase

- **VST3 note expression**: Cubase supports note expression via
  VST3. Handle: MPE inputs work; native VST3 note expression is a
  v1.5 target.
- **Track archive**: Cubase's track archive preserves plugin state
  correctly if the state size is reasonable.

### 9.4 Studio One

- **Automation write filter**: Studio One filters redundant
  automation writes. Handle: no special handling needed.
- **Show / hide plugin window**: Studio One's window management
  works as expected.

### 9.5 Reaper

- **Automation shape**: Reaper sends continuous automation. Handle:
  standard.
- **JS scripts**: Reaper users often want JSFX integration. Not a
  v1 goal.
- **Multi-out routing**: Reaper's multi-output support is
  excellent; layouts B / C / D work out of the box.

### 9.6 FL Studio

- **Wrapper**: FL wraps VST3 in its own wrapper. Occasional issues
  with state persistence. Handle: none needed; the wrapper handles
  the plugin correctly for state save / restore.
- **Piano roll**: FL's piano roll sends notes correctly.

### 9.7 Bitwig Studio

- **CLAP support**: Bitwig prefers CLAP; VST3 works too. CLAP
  planned for v1.1.
- **The Grid**: users may want to route The Grid signals to
  Luthier's sidechain. Works.
- **Modulator system**: Bitwig's modulators can route to any
  parameter Luthier exposes. Works.

### 9.8 Pro Tools

- **Format**: Pro Tools requires AAX; VST3 support is limited or
  absent depending on version. AAX planned for v1.5.
- **State size**: Pro Tools has been strict about state size in
  the past. Handle: same as Logic (< 500 KB target).

### 9.9 Standalone

- **Audio device**: user selects; the plugin polls for
  disconnection per error-recovery.md 9.
- **MIDI input**: user selects; virtual MIDI-out toggle available
  for routing to another app.
- **Windowing**: standard JUCE window, resizable, minimum size
  enforced.
- **File dialogs**: OS-native.

## 10. pluginval

The plugin passes pluginval strictness 10 on every merge to main
per qa-polish.md 2. This is the single best host-integration
regression guard; it catches parameter thread-safety, bus-layout,
parameter-fuzz, and state-round-trip bugs.

## 11. Undo integration

Some hosts (Cubase, Studio One) support plugin-owned undo
integration. Luthier's undo stack is per-plugin-instance; it does
not integrate with the host's undo. This is intentional: the plugin
should not undo the host's edits.

## 12. Preset browsing

- Hosts that offer their own preset browsing (via VST3 preset
  discovery, AU preset discovery) see the plugin's factory presets
  as .luthierpreset files enumerated at `getNumPrograms` /
  `getProgramName` (Program Change addressable).
- The plugin's own preset browser (header) is preferred for user
  workflows because it shows richer metadata (thumbnails,
  categories, tags). The host's browser is for automation-based
  preset changes.

## 13. Threading contract with the host

- `processBlock` is on the audio thread (real-time constraints).
- `getStateInformation` / `setStateInformation` are on the message
  thread but may be called during audio playback; state save uses
  the swap pattern to snapshot state without blocking audio.
- `prepareToPlay` is on the audio thread with prepare guarantees.
- Editor construction / destruction is on the message thread.
- Every timer callback is on the message thread.

## 14. Special features

### 14.1 VST3 units

VST3 units group parameters for the host's automation lanes. Luthier
groups parameters per gui-integration.md's tabs (one unit per Column
4 tab, one per Easy Mode strip).

### 14.2 AU cocoa view

Standard JUCE editor construction; no special handling.

### 14.3 State chunks vs typed params

Luthier uses the state chunk (opaque blob) for structural state and
the typed parameter list for automatable parameters. This is
standard JUCE behaviour.

## 15. Tests

- pluginval strictness 10 on every platform, every format.
- Multi-instance: 32 instances in one host, all playing, verify no
  cross-instance state leakage.
- Host format switching: VST3 -> AU -> VST3 mid-session (macOS).
- Sample rate change mid-play, block size change mid-play.
- Bus layout change mid-play.
- Transport follow: play / stop / position-change with rhythm
  engine and tune builder both active; verify sync.
- State round trip in every host: load a preset, save the host
  project, reload, verify byte-identical state (allowing for the
  host's own state wrapper).
- MIDI I / O in every host: send notes and CCs, verify plugin
  receives; verify MIDI out is written back correctly where the
  host supports plugin MIDI out.

## 16. Support-facing documentation

Every quirk in section 9 is documented in
`docs/HOST_COMPATIBILITY.md` in user-facing language, so a support
ticket about "Luthier isn't receiving sidechain in Live 10" can be
resolved from that doc.
