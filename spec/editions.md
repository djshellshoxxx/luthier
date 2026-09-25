# EDITIONS SPEC: LUTHIER PRO AND LUTHIER FREE

Luthier ships as two editions built from one codebase:

- **Luthier Pro** (paid, ~$200): every feature in every spec, plus the
  licensing of `licensing.md`.
- **Luthier Free**: every *fundamental* feature present in full, the
  basic effects, all plugin formats, the same engine and the same sound
  quality. It must feel like a commercial product (because it is one,
  with the headline features taken out) and be more complete than typical
  free plugins, so that it sells Pro by being good, not by being crippled.

This document is the design only. **Nothing here is implemented yet**:
the coordinator performs the split once every feature is complete
(section 9). Until then, all code is Pro code and nothing in the tree
should anticipate the split beyond not making it harder.

Status: proposal for the product owner. Decisions taken here are marked
**D-n** with a reason; questions the owner must answer are in section 11.

## 0. Ground rules

1. **Same engine, same sound.** Free runs the identical DSP at the
   identical quality: same oversampling, same string, body, pickup, amp
   and cabinet models, same polyphony, same sample-rate range. A Free
   user who loads a Free preset hears exactly what a Pro user hears with
   that preset. Nothing is watermarked, time-limited, noise-injected or
   degraded. (The Pro nag of `licensing.md` 7 is for *unlicensed Pro*,
   never for Free.)
2. **Remove headline features, not fundamentals.** A feature is Pro-only
   when it is a reason to pay $200 (section 1). A fundamental (playing,
   the rig, presets, undo, MIDI learn, accessibility) is in Free in full.
3. **Never gate accessibility, safety or privacy.** Screen reader,
   keyboard navigation, colour-vision palettes, UI scale, localisation,
   panic, kill switch, crash recovery and the privacy dashboard are
   identical in both editions.
4. **Never break or rewrite user data.** Free loads any Pro file, plays
   everything Free can play, bypasses the rest audibly-transparently, says
   so once, and writes the Pro data back unchanged on save (section 5).
   Free never crashes, asserts or refuses a file because it came from Pro.
5. **Compile-time, not run-time, gating.** Pro-only features are not
   *present* in the Free binary (section 7). A byte patch cannot turn
   Free into Pro, and a cracked Free binary gives nothing away.
6. **Side by side.** Both editions can be installed and loaded in the
   same host session: different names, plugin IDs, bundle IDs and
   content folders (section 6).
7. **One upgrade path.** Everything made in Free opens in Pro unchanged:
   presets, guitars, loops, setlists, host projects via preset, user
   data folder.
8. **Upsell is quiet.** Locked features show an "Available in Luthier
   Pro" badge and a single explanatory panel. No modal dialogs on load,
   no timers, no pop-ups while playing, never anything on the audio
   thread (section 4).

## 1. What is "headline" and what is "fundamental"

The $200 argument, from the specs, the onboarding flow and
`gui-integration.md` 20 ("discoverability rules"), rests on eight things.
These are what reviewers and forum threads will talk about, and what a
buyer is paying for. They are Pro-only or strongly limited in Free:

| # | Headline feature | Why it is headline |
|---|---|---|
| H1 | **Workshop bench** (`guitar-workshop.md`, `workshop-ui.md`, `part-acoustics.md`) | Unique: build a guitar from parts on a live-drawn bench and hear every part change. No competitor has it. The onboarding "discovery" beat for it is the product's signature moment. |
| H2 | **Tune Builder** (`tune-builder.md`) | Turns an instrument into a songwriting tool: the "three-minute tune" workflow. A separate product category. |
| H3 | **Techniques tab** (`gui-techniques-updates.md` and the six technique specs, `technique-cascade.md`) | Real-time control of scrape, slide, slap, mute grid, tapping, microtonal bends: what makes a modelled guitar *playable* like a real one by an expert. |
| H4 | **Tone Match** (`tone-match.md`) | Cab match, EQ match and capture: "make it sound like my record". |
| H5 | **Notation and MIDI export** (`notation-export.md`, `midi-export.md`) | MusicXML / Guitar Pro / ASCII / Luthier-profile MIDI with every event class: a working musician's and educator's feature. |
| H6 | **Advanced ranges** (`advanced-ranges.md`) | Beyond-physical parameter ranges for sound design. |
| H7 | **Studio routing** (`routing-io.md` Layouts B-D) | 7 aux pairs and 12 per-string outputs: a mixing engineer's feature. |
| H8 | **The deep realism panels** (CHARACTER tab groups, SUSTAIN column, the extended realism controls) | Hands-on control of wear, aging, environment, tuning stability, buzz setup, noise floor, freeze / E-Bow. |

Everything else is fundamental: the instrument and how it plays, the rig
(amp, cab, room, the common pedals), presets, both UI modes, the rhythm
engine, MIDI learn, undo, the practice basics, every format, standalone.

**D-1** The realism *engines* (string noise, pick noise, fret buzz,
body coupling, harmonic realism, string interaction, sustain and decay,
string aging at its preset value...) **run in Free** at the values the
preset or guitar sets. Only their *deep editing* is Pro. Reason: they are
the sound. Removing them would make Free sound like every other free
guitar plugin and misrepresent Pro; keeping them is the best advert Pro
could have.

## 2. Feature-by-feature table

Legend:
- **Both**: identical in Free and Pro.
- **Pro**: absent from Free; shown locked with the upsell panel.
- **Free-limited**: present in Free with the exact limit given.

### 2.1 Instrument and engine

| Feature | Spec | Edition | Free limit / notes |
|---|---|---|---|
| String model, body, pickups, whammy, polyphony, sample rates, oversampling | `spec.md`, `engine.md` | Both | Rule 0.1 |
| Playing modes Mono / Poly / Chord, humanize macro | `gui-integration.md` 3.3 | Both | |
| Character macro (one knob) | `character-wear.md`, `gui-integration.md` 3.3 | Both | The macro works in full; the per-group deep controls are H8 |
| Factory guitars | `factory-content.md` 2 | Free-limited | 6 guitars: Vintage Double-Cut, Vintage Single-Cut, Classic T-Style, Dreadnought, Classical, P-Style Bass (one per family). The others (semi-hollow, archtop, offset, 7- and 8-string, grand auditorium, parlor, 12-string, flamenca, resonator, Selmer-style, J-style and hollow bass) are Pro |
| Guitar library, tuning, capo, temperament popover | `gui-integration.md` 3.1, 4.1 | Both | Temperaments: equal and the 4 most common historical; custom .scl/.tun is Pro (with H3 BEND) |
| Loading a user `.luthierguitar` made in Pro | `file-formats.md` | Both (play) | Plays as designed; not editable; see 5.3 |
| Pickup selector, per-pickup gain / phase | `gui-integration.md` 4.2 | Both | |
| Workshop bench, inspector, parts library, spectrum delta, bench A/B | `workshop-ui.md`, `guitar-workshop.md`, `part-acoustics.md` | **Pro** (H1) | The wrench glyph opens a locked Workshop preview (4.2) |
| Guitar circuit: volume, tone knobs (physical) | `volume-knob-interaction.md` | Both | |
| Circuit deep panel: pots, tone cap, treble bleed, cable, active/passive | `volume-knob-interaction.md` 10 | **Pro** (H8) | Free uses the guitar's own values |
| Strings: per-string material, gauge | `gui-integration.md` 4.1 | Free-limited | Choice among the factory sets for the guitar family; per-string mixing is Workshop (Pro) |
| String age slider | `string-aging.md` | Both | Single slider; per-string oxidation / contamination / fatigue editing is Pro |
| Environment (temperature, humidity, session drift) | `environment.md` | **Pro** (H8) | Free runs at the neutral 21 C / 45 % |
| Body coupling, wolf notes, sympathetic ring | `body-coupling.md`, `string-interaction.md` | Both (engine) | Controls Pro (H8) |
| Natural, pinch, tapped, artificial harmonics | `harmonic-realism.md` | Both | Playing, not editing |
| Fingerstyle attack profiles | `fingerstyle-attack.md` | Free-limited | Pick, pad, nail; thumb / hybrid / Travis / classical strokes / per-string tool assignment Pro |
| Sustain and decay model | `sustain-and-decay.md` | Both (engine) | Sustain macros Pro |
| Tuning stability (settling, nut binding, drift, auto-retune) | `tuning-stability.md` | **Pro** (H8) | Free runs perfectly stable, which is the default anyway |
| Character: dead spots, fret wear, tuner drift, aged electronics, body break-in | `character-wear.md` | Free-limited | Via the macro only; per-effect controls Pro |
| Pick noise | `pick-noise.md` | Free-limited | Material and thickness; tip, bevel, angle, wear, click / chirp / scrape amounts Pro |
| String squeak | `string-squeak.md` | Free-limited | One amount knob; probability, moisture, pressure, profiles, style presets, noise-event strip Pro |
| Fret buzz (engine) / SETUP group | `fret-buzz.md` | Both (engine) / **Pro** (group) | Free uses the guitar's setup |
| Noise floor (hum, hiss, microphonics...) and Aux 8 noise bus | `noise-floor.md` | **Pro** (H8) | Idle by default in both editions, so nothing changes for Free users |
| Strum dynamics (crossing velocity, acceleration, strikers, chucks) | `strum-dynamics.md` | Free-limited | Crossing velocity and evenness via Feel; striker / chuck editing Pro |
| Slide Mode (bottleneck, lap steel, dobro) | `slide-guitar.md`, `slide-technique-controls.md` | **Pro** (H3) | |
| Freeze, E-Bow (SUSTAIN column) | `ambiguity-resolutions.md` 2 | **Pro** (H8) | Emergent amp feedback physics stays in both |
| Bass techniques: fingerstyle, pick, ghost notes | `bass-techniques.md` | Both | |
| Slap / pop / double thump (bass and generalised) | `bass-techniques.md`, `string-slap-technique.md` | **Pro** (H3) | |
| Techniques tab (SCRAPE, SLIDE, SLAP, MUTE, TAP, BEND, CASCADE) | `gui-techniques-updates.md`, technique specs | **Pro** (H3) | See next row for what Free plays |
| Basic techniques from MIDI: hammer-on, pull-off, legato slides between notes, pitch-bend bends, palm mute (key switch / CC), vibrato | `engine-technique-layer.md`, `technique-cascade.md` | Both | At default settings; the cascade resolver runs in both so combinations stay clean |
| Playing-strip technique pills (Easy Mode) | `gui-techniques-updates.md` 2 | Free-limited | Palm mute and legato pills; the others shown locked |
| Microtonal bends, quantise scales, .scl / .tun, pre-bend | `microtonal-bends.md` | **Pro** (H3) | Standard pitch bend works in both |
| Advanced ranges (padlock, per-preset `ranges` block) | `advanced-ranges.md` | **Pro** (H6) | Free is always in stock ranges; see 5.2 for Pro presets |

### 2.2 Rig and effects

| Feature | Spec | Edition | Free limit / notes |
|---|---|---|---|
| Amp models | `AmpEngine.h` | Free-limited | 7 of 13: Blackface Twin, Deluxe, Plexi, AC30, Rectifier, SVT (bass), Acoustic DI. Pro adds Tweed, Champ, JCM800, Ecstasy, VH4, OR120 and the Custom model |
| Amp controls (gain, EQ, presence, master, sag, bright, bias) | `gui-integration.md` 4.3 | Both | |
| Pre / post effects racks | `gui-integration.md` 3.2 | Free-limited | 4 + 4 slots (Pro 8 + 8) |
| Pedals | `Pedal.h` | Free-limited | 15 of 22: Compressor, Noise Gate, Wah, Overdrive, Distortion, Fuzz, Boost, Volume, Chorus, Phaser, Tremolo, Delay, Reverb, Spring Reverb, Graphic EQ. Pro adds Envelope Filter, Octaver, Pitch Shifter, Flanger, Rotary, Parametric EQ, Doubler |
| Cabinet: model, 2 mics, blend, phase, delay | `gui-integration.md` 4.3 | Both | |
| Factory IRs | `factory-content.md` 10 | Free-limited | 1 body IR per free guitar family, and per cabinet the on-axis and off-axis positions of the two most used mics (~130 IRs of 720); Pro ships all 720 |
| Room | `gui-integration.md` 4.3 | Both | |
| Tone strip: input, output, wet / dry, width | `gui-integration.md` 3.4 | Both | |
| User IR loader | `tone-match.md` 1 | Free-limited | One user IR in cab slot 1 (WAV, up to 1 s); body IR and cab 2 user slots Pro |
| Cab match, EQ match, capture, IR library browser | `tone-match.md` 2-5 | **Pro** (H4) | |

### 2.3 Presets, state and workflow

| Feature | Spec | Edition | Free limit / notes |
|---|---|---|---|
| Factory presets | `factory-content.md` 1 | Free-limited | The 16 of 36 that need only Free features: 1, 2, 3, 4, 7, 10, 11, 14, 17, 21, 22, 27, 29, 31, 32, 35 (renumbered in the Free browser). The other 20 appear in the browser under "Luthier Pro" with a lock and a 20-second audio demo (4.3) |
| User presets: save, load, browse, tags, favourites | `file-formats.md`, `state-model.md` | Both | Unlimited |
| A/B compare, undo / redo, randomize, reset | `gui-integration.md` 2, `action-and-undo.md` | Both | Randomize draws only from Free features |
| Easy Mode | `gui-integration.md` 3 | Both | Full |
| Advanced Mode columns 1-3 | `gui-integration.md` 4.1-4.3 | Both | With the per-control limits above |
| MIDI Learn, host automation | `ui-wiring.md` | Both | Pro-only parameters are not automatable in Free (7.3) |
| Snapshots | `live-performance.md` 1-3 | Free-limited | One bank of 4 snapshots, instant switch; more banks, 8 per bank and morphing Pro |
| Setlists | `live-performance.md` 4 | **Pro** | Free can open a `.luthierset` read-only to see its list |
| Tap tempo, kill switch, panic | `live-performance.md` 5, 6, 9 | Both | Rule 0.3 for panic and kill |
| Monitor mix, expression-pedal calibration wizard | `live-performance.md` 7, 8 | **Pro** | Expression pedal via MIDI Learn works in both |
| Live Mode strip | `gui-integration.md` 9 | Free-limited | Snapshot, tap, kill; setlist and morph controls Pro |
| Modulation matrix | `modulation-matrix.md` | Free-limited | 2 LFOs, 1 envelope follower, 2 macros, 4 routes. Pro: every source (EGs, step sequencers, all LFOs and followers, 8 macros) and the full routing table |
| Rhythm engine: chord detector, voicer, strum and fingerpick schedulers, pattern editor, Feel | `rhythm-engine.md` | Both | Saving user patterns included |
| Genre kits and rhythm patterns | `factory-content.md` 5, 6 | Free-limited | 8 kits with their patterns: Country, Blues shuffle, Funk 16th, Reggae, Punk, Metal, Bossa, Folk arpeggio. Pro: all ~28 |
| Muting-rhythm 16-step mute grid, chuka, ghost | `muting-rhythm.md` | **Pro** (H3) | |
| Tune Builder (TUNE tab, `.luthiertune`) | `tune-builder.md` | **Pro** (H2) | Free can *play back* the six example tunes read-only in the practice drawer as a demo (4.3); it cannot edit or export |
| Controller profiles | `controllers.md` | Free-limited | Generic MIDI and MPE; GK, TriplePlay, Jamstik, Osmose profiles, latency wizard, CC-map editor and multi-controller merge Pro |
| Help tab, F1 help, onboarding | `include.md`, `onboarding.md` | Both | Onboarding has a Free variant of the Workshop / Tune discovery beats (4.4) |

### 2.4 Practice, notation, export, routing

| Feature | Spec | Edition | Free limit / notes |
|---|---|---|---|
| Metronome | `practice-tools.md` 1 | Both | |
| Looper | `practice-tools.md` 2 | Free-limited | One layer, up to 60 s; overdub layers, event-class tags and saving `.luthierloop` Pro |
| Backing-track player | `practice-tools.md` 3 | Both | The 6 factory tracks and the user's own WAV / MP3 |
| Scale and mode trainer, ear training, tab reader, chord progression looper, practice routines, session recorder | `practice-tools.md` 4-8, 11 | **Pro** | |
| Live TAB view, chord-symbol history | `notation-export.md` 3, 4 | Both | |
| Notation export (MusicXML, Guitar Pro, ASCII tab) | `notation-export.md` 2, 5 | **Pro** (H5) | |
| MIDI export / import, Luthier and Generic profiles, drag-out | `midi-export.md` | **Pro** (H5) | |
| Live MIDI out | `midi-export.md` 6, `routing-io.md` 6 | **Pro** (H5) | |
| Audio export / render of the current performance | `AudioExporter` | Free-limited | WAV at the session rate, up to 5 minutes; stems and the offline renderer CLI (`luthier-render`) Pro |
| Bus Layout A (stereo out) | `routing-io.md` 1 | Both | |
| Sidechain input, Layouts B, C, D (aux pairs, per-string outs), re-amp | `routing-io.md` 1-5 | **Pro** (H7) | |

### 2.5 Platform, formats, service

| Feature | Spec | Edition | Free limit / notes |
|---|---|---|---|
| VST3, AU, CLAP, Standalone | `host-integration.md` | Both | Format gating punishes the user for their DAW choice |
| Accessibility and localisation | `accessibility.md` | Both | Rule 0.3 |
| Theme, UI scale, colour-vision palettes | `theme.md`, `accessibility.md` | Both | |
| Update checks, telemetry, crash reports, privacy dashboard, policy file | `updates-telemetry.md` 1-4, 6, 7 | Both | Same opt-in defaults; Free's update manifest is its own channel |
| License activation | `updates-telemetry.md` 5, `licensing.md` | **Pro only** | Free contains no licensing code at all |
| Content updates (`.luthiercontent`) | `installer.md` 11 | Both | A pack declares its edition; Free rejects Pro packs with the upsell notice |
| Diagnostics, error recovery, safe mode | `error-recovery.md` | Both | |

Count: of the 79 rows above, 36 are Both (a few of them with Pro-only deep
controls), 21 Free-limited and 22 Pro.
Free keeps every row a beginner-to-intermediate guitarist or a producer
who needs "a great guitar" touches; Pro keeps the eight headline groups.

## 3. Free content

| Content | Free | Pro |
|---|---|---|
| Guitars | 6 | all (`factory-content.md` 2) |
| Factory parts | only the parts those 6 guitars reference (not browsable: no Workshop) | ~90 |
| Presets | 16 | 36 |
| Genre kits / patterns | 8 / their patterns | ~28 / ~28 |
| IRs | ~130 | 720 |
| Example tunes | 6 (playback-only demo) | 6 |
| Setlists | 0 | 10 |
| Backing tracks | 6 | 6 |
| Example MIDI clips | 8 (one per Free kit) | 12 |

The Free installer is therefore ~40 % of the Pro one. The content split
is a manifest (`packaging/content/free.txt`, section 9) read by
`scripts/ci_build.sh stage`, not a second copy of Resources.

## 4. User experience of locked features

### 4.1 Marking

- A locked tab, panel, pill, rack slot, pedal type or menu entry is drawn
  at the normal size with the muted foreground colour of `theme.md` and
  a small lock glyph; it is never hidden entirely. **D-2** Reason: hidden
  features cannot sell themselves, and a layout that differs between
  editions doubles the UI test matrix. Exception: Pro-only *parameters*
  inside a panel that is itself Free (e.g. pick bevel) are hidden behind
  a single "More in Luthier Pro" row rather than shown as a wall of
  greyed knobs, which looks broken.
- Tooltips on locked controls: "Available in Luthier Pro".
- Screen readers announce "locked, available in Luthier Pro" (rule 0.3:
  locked must not mean unexplained).

### 4.2 The upsell panel

Clicking any locked element opens one non-modal side panel, the same for
every feature: title, two sentences, a 20-second looping preview (a
pre-rendered image or short audio clip shipped in Free, never the real
feature), "Learn more" (opens the product page in the browser) and
"Close". No price in the plugin (prices change; the web page is current).
Esc closes it. It never opens by itself.

### 4.3 Demos

- The 20 Pro factory presets are listed, locked, with a *pre-rendered*
  20-second audio demo each (rendered by `luthier-render` at release
  time, shipped as small OGG/FLAC files). Free cannot load them.
- The six example tunes play back read-only (the Tune playback engine is
  the rhythm and note engine, which Free has; the Tune *editor* is not in
  the binary).

### 4.4 Onboarding

`onboarding.md`'s Workshop and Tune discovery beats become one "What
Luthier Pro adds" page at the end of the Free tour, shown once, skippable,
never repeated.

### 4.5 Frequency

- At most one upsell *notice* (the banner of 5.1) per preset load, and
  "Don't show again for this preset" is honoured and persisted in
  `config/plugin.json`.
- Nothing on a timer. Nothing while the transport runs. Nothing modal.

## 5. Pro files in the Free edition

The rule: **load, play what Free can, bypass the rest transparently, tell
the user once, keep the data verbatim.**

### 5.1 Presets

1. Free loads the preset JSON exactly as Pro would, into the same
   parameter tree and preset blocks (the parameter layout is identical in
   both editions, 7.3).
2. A `ProFeatureGuard` (section 9) walks the loaded state and lists every
   Pro feature the preset *uses*: a Pro-only parameter away from its
   neutral value, a Pro-only block present and enabled (Tune, mute grid,
   Workshop overrides, ranges block, routing layout, extra snapshots,
   mod routes beyond the Free limit...), a Pro amp model or pedal type.
3. The engine reads *effective* values: for each used Pro feature, the
   neutral / bypass value (effect slot bypassed, amp model replaced by the
   nearest Free model of the same voicing family for playback only, mod
   route inactive, advanced-range value clamped to the stock range...).
   The stored values are untouched.
4. A banner (`error-recovery.md` banner priority "info") says: "This
   preset uses N Luthier Pro features, which are bypassed: Techniques
   (slap), Rotary pedal, Advanced ranges. [Details] [Learn more]
   [Don't show again for this preset]". Details lists each feature and
   what Free does instead.
5. On save, Free writes every Pro value and block back **exactly as
   loaded** (same keys, same values, same order where the format
   defines one) plus an informational `"savedBy": {"edition": "free",
   "version": "x.y.z"}`. Pro ignores `savedBy`. A preset round-tripped
   Pro -> Free -> Pro is identical apart from `savedBy` and whatever the
   Free user deliberately changed.
6. A Free user editing a control that *shares* state with a bypassed Pro
   feature (e.g. moving a Free mod route in a preset whose Pro routes are
   preserved) changes only the Free part; the Pro part is carried.
7. If the preset uses a Pro feature whose absence makes it meaningless
   (the preset *is* a Tune, or Slide Mode is its point), the banner says
   so; it still loads.

### 5.2 Advanced ranges

A `ranges` block is carried, not applied: every value the engine sees is
clamped into the stock range, the saved value is kept. The padlock shows
locked; clicking it opens the upsell panel.

### 5.3 Guitars, parts, and the other file types

| File | Free behaviour |
|---|---|
| `.luthierguitar` (any, including Pro-built) | Loads and plays as designed (the engine has every part model; only the bench is absent). Not editable. Saved presets reference it as usual. |
| `.luthierpart` | Not openable (no Workshop); upsell notice |
| `.luthiertune` | Opens as read-only playback (4.3) |
| `.luthierloop` | Plays; overdub / save disabled beyond the Free limit |
| `.luthierset` | Opens read-only (the list is shown, loading presets from it works one at a time) |
| `.luthierpattern`, `.luthierkit` | Patterns load; a Pro factory kit shows locked |
| `.midprofile` | Upsell notice (MIDI export is Pro) |
| `.luthiercontent` | Free packs apply; Pro packs are refused with the upsell notice |

### 5.4 Host sessions

A host project saved with Luthier Pro references the Pro plugin ID, so
it never silently opens in Free. To move a project to Free, the user
saves a preset in Pro and loads it in Free (5.1 applies). A project
saved with Free opens in Pro via the same preset path, or with the
"Import Luthier Free state" button that the Pro edition offers in its
preset menu (it reads Free's state chunk from the clipboard / a file,
which is the same format). **D-3** Reason: VST3, AU and CLAP all key
state on the plugin ID; a shared ID would make the two editions
impossible to install side by side (rule 0.6) and would let a Pro session
open silently degraded in Free.

## 6. Identity of each edition

| | Luthier Pro (PAID) | Luthier Free (FREE) |
|---|---|---|
| `PRODUCT_NAME` / bundle file name | `Luthier Pro` (`Luthier Pro.vst3`, `.component`, `.clap`, `.app`, `Luthier Pro.exe`) | `Luthier Free` |
| Host display name | Luthier Pro | Luthier Free |
| Manufacturer code | `Ltha` | `Ltha` (same company: hosts group them) |
| Plugin code (AU subtype, VST3 class-ID seed) | `Lthr` (unchanged from today) | `Lthf` |
| `BUNDLE_ID` | `com.luthieraudio.luthier` (unchanged) | `com.luthieraudio.luthierfree` |
| `CLAP_ID` | `com.luthieraudio.luthier` (unchanged) | `com.luthieraudio.luthier-free` |
| AU main type | `aumu` | `aumu` |
| Windows installer AppId | the GUID in `packaging/windows/Luthier.iss` | a new GUID |
| Content folder | `.../Luthier/Resources` (as today) | `.../Luthier Free/Resources` |
| User data | `~/Documents/Luthier` | `~/Documents/Luthier` (shared, rule 0.7) |
| User-global config | `config/plugin.json` | same file, edition-scoped keys under `"free"` |
| License file | `licensing.md` 4 | none |
| Update manifest channel | `pro/stable`, `pro/beta` | `free/stable` |
| Installer names | `LuthierPro-<v>-Setup-win64.exe`, `LuthierPro-<v>-macOS.dmg`, `luthier-pro_<v>_amd64.deb` | `LuthierFree-<v>-...`, `luthier-free_<v>_amd64.deb` |

**D-4** Pro keeps today's codes and IDs. Reason: every build that has
existed so far (testers' projects, the factory presets' embedded
plugin state) is effectively Pro; keeping its IDs means none of them
breaks. Only the display / file name changes to "Luthier Pro", which
does not affect the VST3 class ID (JUCE derives it from the manufacturer
and plugin codes), the AU component description or the CLAP ID.
Renaming the product display name is still flagged as open question
Q-1.

## 7. How the split is implemented (minimal divergence)

### 7.1 One CMake option

```cmake
set(LUTHIER_EDITION "PAID" CACHE STRING "Which edition to build: PAID or FREE")
set_property(CACHE LUTHIER_EDITION PROPERTY STRINGS PAID FREE)
include(cmake/Editions.cmake)   # sets the variables below from the table in section 6
juce_add_plugin(Luthier
    PRODUCT_NAME  "${LUTHIER_PRODUCT_NAME}"
    PLUGIN_CODE   ${LUTHIER_PLUGIN_CODE}
    BUNDLE_ID     "${LUTHIER_BUNDLE_ID}"
    ...)
clap_juce_extensions_plugin(TARGET Luthier CLAP_ID "${LUTHIER_CLAP_ID}" ...)
target_compile_definitions(Luthier PUBLIC LUTHIER_PRO=$<BOOL:${LUTHIER_IS_PRO}>)
list(FILTER LUTHIER_SOURCES EXCLUDE REGEX "${LUTHIER_PRO_ONLY_REGEX}")   # FREE only
```

The CMake *target* stays `Luthier` in both editions (so `Luthier_VST3`,
`LuthierTests` and the CI scripts keep their names); only the product
name and IDs change. `LuthierTests` builds in both editions and runs the
edition tests of 10.

### 7.2 `Source/Edition.h`

The only header that knows about editions. Everything else asks it.

```cpp
namespace luthier::edition
{
    constexpr bool isPro = (LUTHIER_PRO != 0);

    enum class Feature { workshop, tuneBuilder, techniquesTab, slideMode, slap, muteGrid,
                         microtonal, toneMatch, notationExport, midiExport, liveMidiOut,
                         advancedRanges, studioRouting, sidechain, deepCharacter, environment,
                         tuningStability, noiseFloor, freezeEbow, setlists, morph, monitorMix,
                         pedalCalibration, controllerProfiles, practiceTrainers, looperLayers,
                         renderCli, licensing /* ... one per Pro row of section 2 */ };

    constexpr bool has (Feature f) noexcept { return isPro || freeHas (f); }  // freeHas: always false

    struct Limits { int guitars, ampModels, pedalTypes, rackSlotsPre, rackSlotsPost, snapshotBanks,
                    snapshotsPerBank, lfos, envFollowers, macros, modRoutes, genreKits,
                    looperSeconds, audioExportSeconds, userCabIrs; };
    constexpr Limits limits = isPro ? Limits { /* unlimited / full */ } : Limits { 6, 7, 15, 4, 4, 1, 4, 2, 1, 2, 4, 8, 60, 300, 1 };

    constexpr bool isFreeAmp (AmpModel) noexcept;      // the tables of 2.2
    constexpr bool isFreePedal (PedalType) noexcept;
    constexpr const char* productName = isPro ? "Luthier Pro" : "Luthier Free";
    constexpr const char* contentFolder = isPro ? "Luthier" : "Luthier Free";
}
```

Call sites use `if constexpr (edition::has (Feature::x))`. In a
non-template function the discarded branch is still compiled (so Free
catches API drift) but **odr-uses in a discarded statement need no
definition** ([stmt.if]), so the Pro `.cpp` files can be left out of the
Free link entirely: the Free binary contains no Workshop, Tune Builder,
Tone Match, exporter or licensing code (rule 0.5). Pro-only headers stay
in the tree and compile in both editions.

### 7.3 Parameters

The parameter layout (`Parameters.cpp`) is **identical** in both
editions: same IDs, same order, same ranges. In Free, a Pro-only
parameter is created with `isAutomatable = false` and a " (Pro)" name
suffix, and the engine reads its neutral value (5.1.3). **D-5** Reason:
one state schema, one migration path, one test suite, and 5.1's
verbatim carry-through comes for free because the values live in the
tree. Rejected alternative: omitting the parameters in Free (it would
need a side store for Pro values and two schemas).

### 7.4 UI

- Each Pro panel keeps its class; in Free the panel's factory returns a
  `ProLockedPanel` (upsell 4.2) instead, chosen by `if constexpr`.
- Limits (rack slots, snapshot count, mod routes, amp / pedal lists) are
  read from `edition::limits` where the UI builds its lists, so the Free
  lists are the Pro lists filtered, never a second copy.
- `ProFeatureGuard` supplies the banner text of 5.1.

### 7.5 Engine

The engine is shared. Where Free must neutralise a Pro feature
(effective values of 5.1.3), the neutralisation happens once, on the
message thread, when parameters are pushed to the engine (the existing
parameter-snapshot path), never per sample and never in `processBlock`
branches. Performance of the two editions is identical
(`performance-budget.md` unchanged).

### 7.6 Build, CI, installers

- CI adds an `edition: [PAID, FREE]` matrix axis to
  `.github/workflows/build.yml`; `scripts/ci_build.sh` and
  `scripts/ci_build.ps1` take `EDITION` / `-Edition`, pass
  `-DLUTHIER_EDITION`, and read the product name for artefact paths
  (today they assume `Luthier.vst3`).
- `stage` copies the content listed in `packaging/content/free.txt` for
  FREE and all of `Resources/` for PAID.
- Installers: the three packaging scripts take the edition, pick the
  names, IDs, AppId and content folder of section 6. One release tag
  builds both editions; the draft release carries six installers.
- pluginval and clap-validator run on both editions; the free edition
  also runs the Pro factory presets through the guard test of 10.

## 8. Maintenance rule after the split

Every new feature's spec states its edition in its header line
("Edition: Both / Pro / Free-limited: ..."), and its PR adds its row to
section 2. A feature without a row does not merge.

## 9. The fork step (what the coordinator creates)

Performed once, after feature freeze, on a branch
`claude/luthier-editions` cut from the integration branch; merged back
into it; no long-lived per-edition branch ever exists (**D-6**: two
branches would diverge within a week; one tree with a CMake option
cannot).

Files created:

| File | Content |
|---|---|
| `cmake/Editions.cmake` | The section 6 table as CMake variables, and `LUTHIER_PRO_ONLY_REGEX`, the Pro-only `.cpp` files left out of the Free link. As of this writing: `Source/Workshop/*.cpp` and `Source/UI/WorkshopPanel.cpp`; `Source/Tune/{TuneHarmony,TuneMelody,TuneSession,TuneTemplates,TuneMidi}.cpp` and `Source/UI/TunePanel.cpp` (Free keeps `TuneModel`, `TuneFile`, `TunePlayer`, `TuneTheory` for read-only playback); `Source/ToneMatch/ToneMatch.cpp` and `Source/UI/ToneMatchPanel.cpp` (the single user IR goes through `Support/IrLibrary`); `Source/Export/*.cpp` and `Source/UI/MidiOutPanel.cpp`, `MidiExportDefaults.cpp`; `Source/Notation/NotationExport.cpp`; `Source/Presets/PresetMorph.cpp`; the technique panels and engines that land from the techniques branch (scrape, slap, mute grid, tapping, microtonal); the licensing file split out of `Source/Updates/Telemetry.cpp`; and the `LuthierRender` target. The list is re-derived from section 2 at fork time, since features are still landing |
| `Source/Edition.h` | 7.2 |
| `Source/Presets/ProFeatureGuard.h/.cpp` | Detection, effective values, banner text (5.1) |
| `Source/UI/ProLockedPanel.h/.cpp` | Upsell panel and lock glyph drawing (4.1, 4.2) |
| `Source/Tests/EditionTests.cpp` | Section 10 |
| `packaging/content/free.txt` | The Free content manifest (section 3) |
| `Resources/Demos/` | Pre-rendered preset demos and upsell images (4.2, 4.3), generated by `scripts/render_demos.sh` (also created) |
| `packaging/windows/LuthierFree.iss` or an `#ifdef Free` block in `Luthier.iss` | The Free AppId and names |
| `docs/EDITIONS.md` | User-facing comparison table generated from section 2 |

Files changed: `CMakeLists.txt` (7.1), `Source/Updates/Telemetry.*`
(split `License` into its own Pro-only file), the panel factories
(7.4), `Source/Support/IrLibrary.cpp` (content folder from `Edition.h`),
`Parameters.cpp` (automatable flag and name suffix in Free only),
`PresetManager` (call `ProFeatureGuard`, write `savedBy`), the three
packaging scripts and the two CI scripts (7.6), `docs/RELEASING.md`.

## 10. Tests

1. Both editions build, pass `LuthierTests`, pluginval (strictness 10)
   and clap-validator.
2. Free binary contains none of the Pro-only symbols (a CI check greps
   the Free binaries for a list of Pro class names and license strings).
3. Every factory Pro preset loads in Free with no assertion, produces
   finite audio, shows the banner, and round-trips Pro -> Free -> Pro
   identical except `savedBy`.
4. Every Free preset loads in Pro and renders within -90 dBFS null of
   Free (same engine, rule 0.1).
5. Both editions installed side by side on each OS; a host scan lists
   both; both load in one session.
6. Locked elements: every locked control has the tooltip, the
   accessible description, and opens the upsell panel; nothing opens it
   unprompted during a 10-minute soak with transport running.
7. Parameter layout identical in both editions (IDs, order, ranges).

## 11. Open questions for the product owner

- **Q-1** Names: "Luthier Pro" + "Luthier Free" (recommended), or keep
  "Luthier" for the paid edition and call the free one "Luthier Free" /
  "Luthier Player" / "Luthier CE"?
- **Q-2** The eight headline groups of section 1: agree, or move any
  (Slide Mode and slap are the closest calls; both are classic
  techniques a free user will miss) to Free-limited?
- **Q-3** Free guitar list (six) and amp list (seven): the owner may
  prefer a Semi-Hollow or a 12-string as the "wow" guitar in Free.
- **Q-4** Should Free play Pro-built `.luthierguitar` files (5.3,
  recommended: it is viral and costs nothing) or refuse them?
- **Q-5** Is there an upgrade discount / crossgrade for Free users, and
  should Free show "Learn more" at all in educational / enterprise
  deployments (policy file switch)?
- **Q-6** Demo audio for locked presets (4.3): acceptable installer size
  cost (~6 MB)?
- **Q-7** Should the Free edition have its own opt-in telemetry of
  "which locked feature was clicked" (useful for pricing, but must follow
  `updates-telemetry.md` 0 exactly: off by default)?
- **Q-8** Time-limited Pro trial inside the Free binary: recommended
  **against** (it would put Pro code in the Free binary, rule 0.5); a
  separate Pro trial via `licensing.md` 6.4 instead. Confirm.
