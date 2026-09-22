# Changelog

## Unreleased

The eleven extension specs in `INDEX.md`, built on top of 1.0.0. The version in
`CMakeLists.txt` is still 1.0.0: this is recorded as unreleased rather than
numbered, because what it should be called is a release decision.

### Added

- **Routing and IO** - a main stereo output plus seven auxiliary stereo buses and
  twelve mono per-string buses, all created disabled so a stereo-only host still
  sees layout A. Sidechain input, MIDI out, re-amp, per-output latency.
- **Modulation matrix** - 8 LFOs, 4 DAHDSR envelopes, 2 step sequencers, 2
  envelope followers, note/CC/macro/random sources and a 1024-route matrix.
  Modulation is applied in one place, `ParameterBridge::value`, so every one of
  the 351 parameters is a legal destination with no per-parameter plumbing. (The
  set grew from 1.0.0's 342 as the extension specs landed; the count is now
  asserted exactly by `Parameters::everyParameterHasAUniqueIdAndSaneDefault`,
  because a host indexes its saved automation against this list.)
- **Rhythm engine** - chord detector, voicer, strum and fingerpick schedulers, 37
  factory patterns (26 strum, 11 fingerpick), 28 genre kits.
- **Live performance** - 128 snapshots with crossfade and morph, setlists, tap
  tempo, kill switch, monitor mix, expression calibration.
- **Controllers** - 9 profiles, per-string channel map, latency wizard, pitch dead
  zone, lazy note-off handling, multi-controller merge.
- **Practice tools** - metronome, looper, backing-track player, scale and ear
  trainers, tab reader, progression looper, session recorder.
- **Tone match** - user IR loading, cab match by sweep/MLS/burst, EQ match, capture.
- **Notation export** - live TAB view, MusicXML, Guitar Pro, ASCII tab and MIDI,
  with importers.
- **Character and wear** - dead spots, fret wear, tuner drift, aged electronics,
  body break-in, temperature and humidity.
- **Accessibility** - screen reader, keyboard-only navigation, colourblind
  palettes, UI scale, localisation.
- **Updates and telemetry** - update checks, opt-in telemetry, crash reporting,
  license activation, privacy dashboard.
- **Advanced Mode columns** - gui-integration.md section 4's scheme, in section
  4's order: GUITAR/BODY/STRINGS/WHAMMY, PICKUPS/CABLE/PRE-FX,
  AMP/POST-FX/CAB/ROOM/SUSTAIN, and a tabbed workspace. Section 4.5's widths
  came with it - 260 points a column over a 220 floor, 480 for the workspace,
  columns 2 and 3 stacked into one slot below 1280 rather than one of them being
  hidden - and the mode is unavailable below 1000 points, where it now refuses
  and says why instead of laying out four columns that do not fit. CIRCUIT is
  still CABLE because `volume-knob-interaction.md` has not been written. The
  sections section 4 has no slot for are kept on the nearest column, each with a
  comment saying why: dropping a working control to match a layout list would
  have cost a feature to close a table row.
- **The column 4 tab strip** - MOD, RHYTHM, ROUTING, TONE MATCH, CHARACTER and
  CONTROLLERS in section 4.4's relative order, one on screen at a time, in place
  of the vertical stack the first five were in. The seven tabs section 4.4 lists
  that have no panel behind them are absent rather than present and empty. **The last-used tab persists
  across sessions** (4.4) in `Documents/Luthier/config/ui.json`, through a new
  `UiPreferences` store: settings global to this copy of the plugin, which is
  neither what `AccessibilitySettings` holds (the person) nor what `uiState`
  holds (the sound), and which had nowhere to live before.
- **Easy mode instrument interactions** - section 3.1's headstock and bridge hit
  regions on `GuitarBodyComponent`, joining the volume and tone knobs, the
  selector switch and the pickups, which were already live. The headstock opens a
  tuning popover carrying the six per-string offsets - a preset field that
  loading a guitar and recalling a preset could write but no control anywhere
  could author. The bridge opens the whammy setup, and on a hardtail says why it
  does nothing. The popover's six sliders write the engine directly, so they are
  not automatable and cost MIDI Learn on those controls: there is no per-string
  tuning parameter to attach them to, and adding one is a parameter-count and
  preset-schema change rather than a UI one. That is `GAPS.md` A4. The popover
  also says on its face that capo is not built, which four specs describe and no
  line of code implements.
- **Options pages** - the overlay now carries gui-integration.md section 5's tab
  list in section 5's order: AUDIO, MIDI, APPEARANCE, ACCESSIBILITY,
  LOCALIZATION, EXPRESSION, UPDATES, PRIVACY, DIAGNOSTICS and FILE LOCATIONS.
  Ten tabs - section 5's eleven minus RANGES, which advanced-ranges.md has not
  specified - with nothing present that section 5 does not name. The old GENERAL
  tab is gone, its contents distributed to the tabs that own them. Where a page
  could not build something section 5 lists - the accent tint, the changelog
  viewer, the feature-flag mirror, the Workshop folders - it says so instead of
  showing a dead control.
- **CONTROLLERS moved to Advanced column 4**, which is where gui-integration.md
  section 19 always put controller setup. It sat in RANGES' slot on the Options
  overlay only because that tab strip did not exist. The page is moved rather
  than copied: `ControllersPage` owns its `ControllerProfileLibrary` by value, so
  two pages would scan the Controllers folder separately and go stale against
  each other the moment either saved a profile - whereas one page has nothing to
  keep in sync. It refreshes when its tab is opened, as the overlay used to
  refresh it, because a controller can be unplugged while the tab is not looking.
- **Notification banners** (gui-integration.md section 15), which had not been
  built at all. Non-modal, under the header strip, 32 points, dismissible, one at
  a time with the rest queued. A banner auto-dismisses after five seconds unless
  it carries an action button, in which case the clock is never started - a
  banner offering to review a crash report should not evaporate while the user is
  reaching for it. Four of section 15's nine triggers are wired, all checked when
  the window opens: a crash dump from the last session (its button opens the
  Privacy page, which shows exactly what would be uploaded - Review, not Send), a
  licence in its grace period, settings managed by an administrator's policy
  file, and an available update. The update check is the only one that touches
  the network and runs only when the user has opted in, since opening a window is
  not a reason to make a request someone declined. The other five triggers need
  the preset loader to report what it substituted, or advanced ranges, or a place
  for the audio thread to leave a sample-rate message; GAPS.md A6 has them.
- **A preset that will not load now says so.** `loadPreset` already returned false
  and wrote an error-log line, but most callers discard the bool - the header's
  Open dialog worst of all, where a preset that would not load did nothing at all
  with no message anywhere the user could see. `PresetManager::getLastLoadError`
  carries a sentence naming the file, and the window raises it as a banner.
- **A missing IR now says so too.** A preset naming an IR that is no longer on
  disk already fell back to the built-in model with the reason recorded; nothing
  displayed it. The banner offers a button to the TONE MATCH tab. Both of these
  are read on the window's timer rather than pushed from the loader, because
  presets load from five places, and they post only when the message changes, so
  a dismissed banner stays dismissed.
- `AdvancedPanel::setWorkspaceTabNamed`, the column 4 counterpart of
  `showPageNamed`, for the same reason: a banner should not send the user to a
  tab index that a future tab would shift.
- `OptionsPanel::showPageNamed`, so a banner can send the user to a specific
  Options page by name rather than by an index that RANGES would shift.
- `scripts/build.ps1`, which PROGRESS.md had referenced without it existing.
- **Coverage for right-click → Modulate**, which had none. The menu itself is not
  new - it has offered every modulation source since the phase 1 extension work -
  but nothing in the plugin goes through it except a user, so every one of its
  behaviours could have broken in a release without a failure anywhere. It is now
  built by `buildParameterContextMenu` and performed by
  `applyParameterMenuResult`, either side of `showMenuAsync`, so a test can walk
  the real items and drive the real handler instead of a human opening the menu
  and looking at it.
- **Editor tests.** `Source/UI/` and `PluginEditor.cpp` now build into
  `LuthierTests`, which compiles with `LUTHIER_HEADLESS=0`, and the new `Editor`
  suite opens the window: the size the processor hands back, layout and paint at
  940x560, 1200x720 and 1920x1080, every overlay shortcut opening its own overlay
  and closing on escape, advanced mode and the practice drawer and Live Mode
  flipping and flipping back, and all five Options tabs putting their own page on
  screen. It renders into an offscreen image, so it needs no desktop window.
  Before this, nothing in the repository constructed a
  `LuthierAudioProcessorEditor` and the panels were verified only by running
  pluginval or a host by hand. The suite has since grown to cover the work above:
  every workspace tab putting its own panel - and only its own - on screen (the
  list there and the Options list are what stop CONTROLLERS existing in both
  places at once), the
  tab stepping and wrapping and surviving a round trip through the config file,
  Advanced Mode refusing both routes in below 1000 points with the notice
  painted, a grid swept over the whole illustration to collect what every hit
  region says, and the headstock popover reaching the engine one string at a
  time and actually moving the pitch, and the right-click menu offering every
  modulation source group by name, building a route at the depth it claims, and
  refusing to route past a destination's limit.

### Fixed

- **The ASCII tab importer read its own beat ruler as a string of the tab.**
  `looksLikeTab` tested only for "mostly dashes" and never for a bar line, which
  the writer's `1---2---3---4---` ruler passes. Every import therefore invented a
  note per beat and shifted every real string index down by one. The function's own
  comment had specified the bar-line rule; the code never implemented it.
- **`Capture` could not be default-constructed.**
  `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` declares a deleted copy
  constructor, which suppresses the implicit default constructor. `PluginProcessor`
  holds one by value, so this broke both the plugin and the test build.
- **`LuthierRender` did not compile.** `RenderCli.cpp` had a literal newline
  inside a string constant where a newline escape was meant, so the offline
  renderer had not built since the phase-1 extension commit. The executable on
  disk was two days older than the source and nobody had noticed - the same
  stale-binary trap as the test runner, and the reason the build script's exit
  code is now checked rather than the build's output being read.
- **`TuningEngine::reset()` was missing braces**, leaving
  `characterDriftCents = 0.0` outside the loop it belonged to, so character drift
  survived a reset.

### Fixed in the tests

- `eqMatchFitsKnownCurves` expected a low shelf to reach full gain at its corner
  frequency, which is by definition the half-gain point. The fit was correct to
  0.07 dB; the expectation was wrong.
- `deadSpotsReduceSustainWhereTheyAre` used a fixed eight-fret reference that could
  land on a deeper dead spot, so it sometimes compared two dead notes.

### Note on how these were found

Five test suites - ToneMatch, Notation, Character, Accessibility and Telemetry -
existed in the tree but had never been linked into a running binary, because the
build that should have produced it failed and left the previous executable in
place. `Source/UI/` was excluded from the test target at the time, so eight new
panels had never been compiled at all. Everything under "Fixed" above surfaced
the first time that code actually ran.

## 1.0.0

First release. Everything below is new, so rather than list every file this records
the shape of what was built and — more usefully — the defects found along the way
and what caused them.

### The instrument

- Extended Karplus–Strong waveguide strings: a fractional delay line, a one-pole
  loop filter whose gain comes from a measured T60, and a cascade of first-order
  all-pass sections for stiffness dispersion. Up to twelve strings.
- Inharmonicity from the real coefficient, `B = π³Qd⁴/(64TL²)`, using the **core**
  diameter of a wound string rather than its overall diameter.
- Excitation models for pick, finger, nail, thumb and slide, each with its own
  spectrum and its own contact noise.
- Sympathetic coupling through a symmetric bridge matrix, per sample, with the
  total energy capped so it can never run away.
- Bodies as Helmholtz air resonance plus a thin-plate modal bank, or as
  convolution against a body response.
- Pickups as a positional comb plus an LCR tank, with the coil's own resonance
  moving as the tone control loads it.
- 25 instruments, from a Telecaster to a 12-string to a five-string bass.

### The amplifier

- Passive three-band tone stacks solved by nodal analysis and bilinear-transformed
  to a third-order IIR, so the controls interact the way the real network does.
- Valve stages with grid conduction, bias shift and supply sag.
- Polyphase all-pass half-band oversampling, 1× to 8×, 4× by default.
- Cabinets as dual-microphone convolution with time-of-flight alignment, falling
  back to an analytic speaker model.
- An FDN room with a Householder feedback matrix.

### Playing

- MIDI interpretation with four modes, MPE, per-string channels, a chord window
  and strum modelling.
- A chord voicer that only produces fingerings a hand can make, and that stays
  near where the hand already was.
- Techniques inferred from what you play: hammer-ons, pull-offs, slides, palm
  mutes, harmonics, tapping.

### The interface

- Easy and Advanced panels, a drawn instrument that reflects the current build, a
  live fretboard, a pedal rack.
- 342 parameters, all automatable, all with proper names and text round-trips.
- Presets, A/B, randomise with locks, MIDI learn on every control, audio and MIDI
  export, a help section, a troubleshooting export, and a hidden effect.

---

## Defects found and fixed during the build

Kept because the causes are more interesting than the fixes, and because anyone
changing this code can walk into the same ones.

**The oversampler passed aliasing.** The polyphase all-pass difference equation
had a sign error: `a * (x - y2) + x2` had been written with the terms the other way
round. It sounded almost right, which is why it needed a sweep test rather than an
ear to find.

**Every strummed chord was nearly silent.** Notes scheduled past the end of the
current block were dropped instead of being carried, so a strum lost all but its
first note. Fixed with a persistent scheduled-event array that survives the block
boundary.

**Two crashes.** One in the excitation, where a negative pluck length was derived
before being clamped; one where a host block larger than the prepared size ran off
the end of the scratch buffers. Both now clamp before deriving, and the engine
splits oversized blocks.

**Acoustic and classical presets were silent.** Wound nylon strings were being
given nylon's density for the whole string, including the metal winding, which put
the tension far outside anything playable. Wound strings now carry a separate
winding density.

**Every bass was flagged unplayable.** The validator applied guitar tension limits
to a 34-inch scale. Tension range is now a function of scale length.

**65% of real time on one instance.** The loop filter coefficients were being
recomputed with `pow` and `exp` every sample. They are now recomputed only when the
pitch has actually moved, which took it to about a third of real time.

**Renders were not reproducible.** Three separate causes, all found by rendering
the same state twice and comparing:

- String and interpreter random state carried across a `reset()`. Both reseed now.
- Parameter smoothers were left mid-ramp, so the first render had a 20 ms fade the
  second did not. `reset()` now snaps them to their targets.
- Articulation state — fret positions, string assignments, LFO phases and the chord
  voicer's hand position — was not cleared by `reset()`, so the second note of a
  session started from a slide rather than from the nut.

**The first block after an impulse response changed was heard with no speaker on
it.** `juce::dsp::Convolution` loads on its own thread and only installs the new
response the next time `process()` is called, so for a block or two it is still a
unit impulse — meaning the raw amp, which is the loudest and harshest thing the
signal path can make, arriving exactly on the transient a preset change makes. It
also made rendering non-deterministic, since whether the response was in yet
depended on how many blocks had gone by.

The loader now drives the swap to completion on the thread that asked for it, and
the audio thread try-locks: for the one block a swap can overlap, it uses the
analytic speaker and body models instead. See
`Source/DSP/Common/ConvolutionInstaller.h`.

**A read-only install folder left the preset browser empty.** The factory bank is
written to the folder inside the installed bundle, which under Program Files a
standard user cannot write to. Nothing failed loudly; the bank simply was not
there. The folder is now probed by writing to it, and falls back to one under
Documents, which is what the troubleshooting guide already said happened. The
folder inside the bundle is still scanned, so a bank installed by hand beside the
plugin is found either way.

**The offline renderer listed no presets.** It set up the preset manager the way
the plugin does but never installed the bank first, so `--list-presets` printed
nothing and `--preset <name>` never matched.

---

## Testing

98 tests, covering the DSP primitives, the string and body physics, the model
layer, every parameter, every preset, a ten-thousand-state fuzz, and the whole
signal path end to end.

The suite found nine genuine defects, including both crashes above, the
oversampler sign error and the silent-strum bug. Where a test was asserting the
wrong thing it was changed and the reason written next to it.
