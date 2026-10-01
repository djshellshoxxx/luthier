# Luthier Product Overview

**Living draft — refresh after the current expansion (new instruments + tier features) lands.**

A complete, organized list of Luthier's features and capabilities, grouped by category.

---

## Sound Engine & Physical Model

**String Engine**
- Digital waveguide model for each string with physical mass, tension, and stiffness
- 6-string standard tuning, plus 7-string, 8-string, 12-string and fretless modes
- Inharmonicity modeling: partials stretch naturally as real strings do
- Frequency-dependent damping with two-stage decay (fast transient, slow sustain)
- Sympathetic coupling through bridge: other strings ring at matching frequencies
- Per-string parameters: scale length (500-750 mm), tension, damping, material

**String Materials & Gauges**
- 12 string materials: NPS, phosphor bronze, 80/20 bronze, silk & steel, nylon, stainless roundwound, flatwound, coated variants
- 11 gauge sets: extra light to heavy for electric, acoustic, classical, and bass
- Wire properties computed from gauge: tension, mass, inharmonicity, sustain, brightness

**Guitars & Instrument Library**
- 25 factory instruments: electric (single-cut, double-cut, offset, semi-hollow, full hollow archtop, T-style, 7-string, 8-string), acoustic (dreadnought, grand auditorium, parlor, 12-string, resonator), classical (classical, flamenca), Gypsy jazz (Selmer-style), bass (P-style, J-style, hollow)
- Fully editable guitar workshop: mix and match body, neck, fretboard, frets, nut, bridge, pickups, pickguard, strings, capo, slide, wiring
- ~90 factory parts library for customization
- Each guitar carries unique body, woods, bracing, scale length, pickups, tuning and default rig
- User `.luthierguitar` files for saving custom instruments

**Body Modeling**
- Two rendering modes: convolution (measured IR) or modal synthesis (resonant filter bank)
- Body parameters: wood type (mahogany, maple, alder, ash, rosewood, spruce, cedar, koa), size (parlor to jumbo), depth, top thickness, bracing pattern (X-brace, fan-brace, ladder-brace)
- Air resonance frequency adjustable (100-140 Hz typical)
- Age simulation (reduced damping for older wood character)
- User-adjustable body dimensions with live spectral preview

**Pickup Modeling**
- Single-coil (bright, quack, hum-prone)
- Humbucker (thick, less bright, hum-cancelling)
- P90 (between single-coil and humbucker, grittier)
- Piezo (under-saddle, percussive, bright)
- Magnetic soundhole (acoustic electric, darker)
- Internal condenser mic (high-end acoustics, woody)
- Blended piezo + mic (balance adjustable)
- Per-pickup parameters: position along string, coil count, coil spacing, resistance, inductance, capacitance, magnet type (Alnico 2/3/5, ceramic), pole spacing, height above strings
- Dual-pickup blending with phase control
- Coil tap for humbuckers (split to single-coil)

**Temperaments & Tuning**
- 17 factory tunings: standard, open, drop, half-step down, etc.
- 7 temperaments: equal, just intonation, meantone, well-tempered, Werckmeister, Kirnberger, custom via .scl/.tun files
- Per-string fine-tuning
- Capo support (partial and full)
- Custom tunings with per-string control
- Tuning stability modeling: string settling, nut binding, tuner backlash, saddle creep, bend memory, auto-retune

**Tone & Character Control**
- Guitar circuit: physical volume and tone knobs with realistic pot behavior
- Tone control parameters: pots, tone cap, treble bleed, cable loading, amp-input impedance
- Character macro: one knob controlling dead spots, fret wear, tuner drift, aged electronics, body break-in, environment
- String age slider: fresh to dead string progression (oxidation, contamination, corrosion, core fatigue)
- Environment effects: temperature and humidity impact on tuning stability, action, body resonance (session drift profiles)
- Cable simulation: capacitance roll-off from cable length and type

---

## Playing Techniques

**Fretted & Continuous Play**
- Fretted mode: 24 frets by default, adjustable 12-27 per guitar type
- Fretless mode: continuous pitch, no quantization, different attack transient
- Fret noise: adjustable click when fingers land on frets
- Fret buzz: high-frequency rattle based on action height and fingering pressure
- Action height adjustable (mm): affects buzz, attack, bend ease
- Fret height and relief modeling

**Pitch Techniques**
- Bend: continuous upward pitch change, half-step to multi-octave, per-string or multi-string
- Pre-bend: bend before pluck, release after creates downward pitch move
- Vibrato: rate 3-8 Hz, depth 5-50 cents, shapes (sine, triangle, finger vibrato, classical vibrato, blues vibrato)
- Microtonal bends: custom bend amounts, quantize to scales, .scala/.tun file support
- Pitch bend via MIDI or whammy bar

**Articulation Techniques**
- Hammer-on: sound higher note without pluck
- Pull-off: sound lower note by pulling finger off
- Legato slide: continuous pitch change note-to-note
- Slide (glissando): glide with re-pluck option
- Slide guitar / bottleneck: lap steel, dobro, resonator slide modes with continuous bright attack
- Fretless slides with vocal character

**Dynamics & Muting**
- Palm mute: aggressive damping from 5 kHz down, shortened sustain, palm-mute grid control
- Muted picking: light touch damping without full palm mute
- Dynamic picking: crossing velocity, acceleration profiles, multiple strikers and chucks
- Dynamics control: per-note velocity, feel macro for humanize settings

**Harmonics & Overtones**
- Natural harmonic: touch at nodal points (12th, 7th, 5th frets) with no fret
- Pinch harmonic: pick + thumb excitation targeting specific overtones
- Artificial harmonic: fretting + nodal touch combination
- Tapped harmonic: two-handed technique with auto pull-off, multi-finger tap, hammer-on/pull-off promotion
- Harmonic boundary conditions: natural, pinch, tapped, artificial as real physical behavior

**Whammy & Modulation**
- Whammy bar modes: vintage (down-only), Floyd Rose (up-and-down), TransTrem (maintains intervals)
- Dive-bomb: multi-octave downstroke
- Range adjustable in semitones
- Smooth modulation tracking with no pitch stepping

**Noise & Character**
- Finger-slide squeak: squeak when fingers slide on wound strings, speed-dependent, per-material spectrum
- Pick attack transient: brief click at pick contact, shape and amount adjustable
- String release noise: soft thump on note off
- Body knock: percussive tap on body for fingerstyle
- Pickup switching clicks (subtle, optional)
- String scrape / pick scrape: dragging pick along wound strings, filtered noise excitation

**Bass-Specific Techniques**
- Fingerstyle: thumb, index, middle, ring assignments
- Pick play with attack and grit
- Ghost notes: light muted notes between main notes
- Slap: thumb attack, pop with finger, palm slap, body tap
- Double thump: rapid slap variations
- Muted pick and upright bass character modes

**Whammy Bar & Modulation Modes**
- Vintage tremolo, Floyd Rose, TransTrem, Bigsby modes
- Pitch modulation range: -12 to +12 semitones adjustable
- Smooth tracking without stepping
- Multi-string global pitch bend for chord play

---

## Amplifier & Effects

**Amplifiers (13 models)**
- Blackface Twin, Deluxe, Plexi, AC30, Rectifier (available in Free: 7 models)
- Tweed, Champ, JCM800, Ecstasy, VH4, OR120, Custom (Pro only)
- SVT (bass amplifier)
- Acoustic DI
- Per-amp controls: gain, EQ (bass/mid/treble), presence, master volume, sag, bright switch, bias

**Valve Amp Architecture**
- Cascaded preamp stages with tube saturation
- Push-pull power amp with power-tube-specific characteristics
- Passive tone stack (3-knob circuit per amp model)
- Output transformer modeling with impedance interaction
- Sag: supply voltage sag under load, adds responsive compression
- Bias point adjustment: changes breakup character

**Pre-Effects Rack**
- 8 slots (4 in Free), reorderable
- Compressor, Noise Gate, Wah, Overdrive, Distortion, Fuzz, Boost, Volume (all editions)
- Envelope Filter, Octaver, Pitch Shifter (Pro only)
- Per-pedal controls and A/B bypass

**Post-Effects Rack** (in amp loop)
- 8 slots (4 in Free), reorderable
- Chorus, Phaser, Tremolo, Delay, Reverb, Spring Reverb, Graphic EQ (all editions)
- Flanger, Rotary, Parametric EQ, Doubler (Pro only)
- Wet/dry control, width, feedback, modulation depth per pedal

**Cabinet & Microphone Modeling**
- 10 cabinets: open-back, closed-back, 2x12, 1x15, 4x12 vintage/modern variants
- 8 speaker types with distinct frequency responses
- 7 microphone types and placements (on-axis, off-axis, edge)
- Dual-mic blending with adjustable mix
- Phase inversion and time-of-flight alignment per mic
- 504 cabinet impulse responses (synthesized from physics)

**Room & Acoustic Space**
- Early reflections and ambient reverb
- Room size, damping, predelay adjustable
- Accounts for mic placement and room acoustic interaction

**Tone Control Strip**
- Input level, output level, wet/dry mix, stereo width
- Master gain, safety limiter at -0.3 dBFS

**Tone Matching & Custom IRs** (Pro)
- User WAV IR loading: up to 1 second, any sample rate
- Cab match: load measured cabinet impulse
- EQ match: match frequency response to reference
- Capture utility: generate IRs from external rig
- IR library browser with preview
- Per-body and per-cabinet custom IR slots

---

## Presets & Sound Design

**Factory Presets (36)**
- 20 Electric: Clean, Break-Up, Crunch, Overdrive, Metal, Ambient, Funk, Country, Blues, Jazz, Punk, Indie, Surf, Slide (16 in Free)
- 6 Acoustic: Folk, Bluegrass, Fingerstyle, Blues, 12-String, Gypsy Jazz (3 in Free)
- 3 Classical: Bossa, Flamenco, Etude
- 1 Reggae
- 5 Bass: Soul, Funk, Reggae, Punk, Jazz (2 in Free)
- Difficulty ladder: easy, medium, showcase for each category
- Genre coverage: rock, blues, jazz, country, folk, classical, metal, funk, reggae, latin, indie, ambient

**Preset Management**
- Save, load, browse with search and tags
- Favorite presets
- A/B compare presets side-by-side
- Randomize (draws from Free features in Free edition)
- Reset to factory settings
- User presets: unlimited storage in Documents/Luthier/Presets/User
- Preset round-trip with MIDI: export/import as `.luthierpreset` JSON

**Snapshots & Live Performance**
- Snapshot banks: 1-8 banks (Free: 1), 1-8 snapshots per bank (Free: 4)
- Instant or morphed switching between snapshots
- Morph control: smooth crossfade between snapshot states
- Tap tempo: tap-tempo control for delay and modulation rates
- Kill switch: instant mute for live performance
- Panic button: silence all notes, release all sustain

**Setlists & Session Organization**
- 10 example setlists (Pro): Solo Acoustic, Rock Cover, Blues Trio, Bossa Cafe, Metal Warmup, Bluegrass, Funk Trio, Ambient Loop, Country Bar, Jazz Combo
- Load presets in sequence during performance
- Setlist editor: add, remove, reorder presets
- Free can view setlist read-only

**Modulation Matrix**
- 2 LFOs, 1 envelope follower, 2 macros, 4 mod routes (Free)
- Full in Pro: every source (envelope generators, step sequencers, all LFOs, followers, 8 macros), unlimited routing
- LFO shapes: sine, triangle, square, sawtooth
- Envelope generators: ADSR with adjustable times
- Step sequencers for rhythmic modulation
- Macro knobs for player-controlled morphing

---

## MIDI & Host Integration

**MIDI Input & Interpretation**
- Standard MIDI note on/off with velocity
- MPE (Polyphonic Expression): per-string pitch bend, pressure, slide
- Per-string MIDI routing: assign notes to specific strings or auto-detect
- Chord detection and voicing: play chords on single MIDI track, engine voices to strings
- Strum simulation: delay between strings for natural strum timing
- CC mapping: fully customizable MIDI learn for any parameter
- Pitch bend and aftertouch support

**MIDI Output & Export** (Pro)
- Live MIDI out: real-time MIDI events from performance
- MIDI export: session/performance as `.mid` file with Luthier-profile events
- Generic MIDI profile: standard note on/off + CCs for other DAWs
- Event class export: every technique mapped to MIDI (harmonics, slides, mutes, etc.)
- Drag-and-drop MIDI out to host
- Offline MIDI export via CLI renderer

**Rhythm Engine**
- Chord voicing: play chord shapes, auto-finger on fretboard
- Strum patterns: 28 patterns across genres (Country, Delta Shuffle, Piedmont, Bossa, Samba, Reggae, Funk, Punk, Metal, Djent, etc.)
- Fingerpick patterns: fingerstyle voicing and picking order
- Genre kits: ~28 bundled pattern sets with voicer, humanize, pick style, setup style presets
- Pattern editor: create custom patterns (Pro)
- Strum feel: humanize macro adjusts timing and velocity variation
- Crossing velocity: dynamics between string strokes
- Humanization: note timing, velocity and micro-random adjustments

**Host Integration**
- VST3 (all platforms)
- AU (macOS)
- CLAP (all platforms)
- Standalone application
- Proper latency reporting to host
- State serialization: plugin state saved/loaded by host
- Per-host quirk handling: Ableton, Logic, Cubase, Studio One, Reaper, FL, Bitwig, Pro Tools
- Sidechain input support
- Per-string audio outputs (Pro)
- Bus layout switching (7 aux pairs, re-amp routing in Pro)

**Expression & Automation**
- Host automation: any parameter automatable except Pro-only in Free
- MIDI Learn: map any CC or note to any parameter
- Expression pedal: calibration wizard (Pro), control via MIDI Learn (all)
- Macro controls: player-facing morphing controls in preset
- Feel macro: single knob for humanize, dynamics, character blend

---

## Practice & Performance Tools

**Metronome**
- Tempo adjustable 20-280 bpm
- Time signatures: 2/4, 3/4, 4/4, 5/4, 7/8, custom
- Visual and audio click
- Tap-tempo input from keyboard or MIDI
- Beat count-in before play

**Looper**
- 1 layer, up to 60 seconds (Free)
- Unlimited layers with overdub (Pro)
- Event-class tagging for export (Pro)
- Saving as `.luthierloop` file (Pro)
- Undo/redo within loop
- Crossfade on layer switch

**Backing Tracks**
- 6 factory royalty-free tracks: 12-Bar Blues (E, 120 bpm), Funk Groove (D, 100 bpm), Bossa Nova (Bb, 140 bpm), Rock Jam (A, 140 bpm), Country Shuffle (G, 110 bpm), Ambient Pad (Am, 60 bpm)
- WAV / MP3 playback from user files
- Tempo sync to Luthier tempo
- Volume adjustable

**Scale & Mode Trainer**
- 30+ scales and modes
- Visual fretboard display
- Ear training exercises
- Scale generator with custom scales via .scala/.tun

**Chord Progression Looper**
- Loop chord progressions
- Auto-voicing display
- Rhythm pattern integration
- Session history tracking

**Ear Training & Skill Tools**
- Interval recognition
- Chord identification
- Rhythm exercises
- Tab reader: display and play from imported tab files
- Session recorder: capture performances for review

**Practice Routines**
- Goal-based practice workflows
- Timer and BPM control
- Progress tracking

---

## Notation & Export

**Live Tab View**
- Real-time ASCII tab display during play
- Chord symbol history
- Technique indicators on tab
- Scroll or pause for review

**Notation Export** (Pro)
- MusicXML: imports into Finale, Sibelius, Dorico, MuseScore
- Guitar Pro: `.gp5` format with techniques
- ASCII tab: text-based tab for sharing
- Every played note exported with rhythm and articulation

**MIDI Export & Import** (Pro)
- Luthier profile: custom MIDI with event classes (bend, slide, mute, harmonic, etc.)
- Generic MIDI: standard note on/off + CC for compatibility
- Import MIDI files: play with Luthier engine
- Event alignment: proper timing for every articulation

**Audio Export**
- WAV export at session sample rate, up to 5 minutes (Free)
- Stems export: separate guitar, effects, room (Pro)
- Offline renderer CLI (`luthier-render`): batch render MIDI to audio with preset

---

## User Interface & Modes

**Easy Mode**
- Single-column simplified layout
- Core parameters only: tone controls, amp/cab selection, reverb/delay
- Character macro (one knob for wear, age, environment)
- String age slider
- Snapshot strip and practice drawer visible
- Ideal for beginners and live performance quick-tweaks

**Advanced Mode**
- 4-column layout: Character, Amp/Rig, Effects, Modulation
- Every parameter accessible: body, pickups, circuit, string noise, techniques, tuning stability, realism controls
- Padlocked advanced ranges when preset uses extended values
- Column 3 and 4 replaced by Workshop when bench is active

**Workshop (Guitar Customization Panel)**
- Live-drawn interactive guitar illustration
- Hit-tested parts: click and drag to swap body, neck, pickups, bridge, etc.
- Inspector panel: detailed part parameters and specs
- Spectrum delta: live frequency response comparison (body, pickup changes)
- Audition: A/B compare sound between builds
- Parts library browser
- Visual feedback of every change in real-time

**Live Mode Strip**
- Snapshot recall and morphing controls
- Tap tempo display
- Setlist navigation (Pro)
- Kill switch, panic button
- Monitor mix control (Pro)
- Expression pedal readout

**Fretboard Visualization**
- Interactive fretboard display in GUI
- Technique overlays: show bend, slide, hammer-on, pull-off ranges
- Scales and chord fingerings displayed
- Touch-friendly for iOS-style controllers

**Theme & Visualization**
- Warm, dark accent color (walnut-black neutrals, aged ivory text)
- 270-degree external value arcs on knobs
- Section rules and corner radii (8px grid)
- Signature notch on header (Easter egg)
- Output LED status indicator
- Scrolling data stream in footer (live parameters)
- UI scale: adjustable font size and control scaling
- Color-blind palettes: deuteranopia, protanopia, tritanopia
- Light and dark theme support

**Search & Navigation**
- Global preset search
- Parameter search
- Help tab with control tooltips
- Keyboard shortcuts documented (F1 help)
- Onboarding tour for first-run users

---

## Accessibility & Internationalization

**Accessibility**
- Screen reader support: all controls announced with accessible descriptions
- Keyboard-only navigation: full control without mouse
- Tab order: logical control focus flow
- Color-vision modes: three color-blind palettes (not color-dependent alone)
- UI scale: 50% to 200% adjustable
- High contrast mode option
- Kill switch and panic button (safety)
- Crash recovery and safe mode (reliability)

**Localization**
- Multi-language support (localizations TBD with product owner)
- Every user-facing name in locale catalog
- Preset names, guitar names, part names translated
- Interface text and help content localized

---

## Performance & Technical

**CPU & Quality**
- Double-precision DSP throughout (no single-precision accumulation)
- 4x oversampling for nonlinear stages (amp, drive, waveshapers), adjustable to 2x or 8x
- CPU quality modes: High (best quality), Medium, Low (lower CPU), Auto (adaptive)
- Denormals disabled via `ScopedNoDenormals`
- No allocations, locks, or file I/O in audio callback
- Polyphony: 6 strings standard, 7-8 for extended range, 12 for 12-string

**Sample Rate Support**
- 44.1 kHz, 48 kHz, 88.2 kHz, 96 kHz, 192 kHz
- Sample-rate-independent parameter storage (seconds, Hz)
- Live sample-rate switching without artifacts
- All resampling done at compile time for IRs

**Buffer Sizes**
- All buffer pre-allocation in prepareToPlay
- No allocation in processBlock
- Handles arbitrary block sizes from host (32 to 4096 samples)

**Latency**
- Latency reported to host (primarily from oversampling and convolution)
- Proper alignment: MIDI events and audio time-code synchronized

**File Size & Bundling**
- Resources bundled beside each plugin (.vst3, .component, .clap, .app)
- 720 impulse responses (synthesized, not measured, deterministic generation)
- Icon and IR generation scripts (Python 3.9+)
- Self-contained: no external dependencies at runtime

---

## Factory Content & Libraries

**Factory Guitars (15 `.luthierguitar` files)**
- Vintage Single-Cut, Vintage Double-Cut, Classic T-Style, Semi-Hollow 335, Full Hollow Archtop, Offset Modern, 7-String Modern, 8-String Modern (electric)
- Dreadnought, Grand Auditorium, Parlor, 12-String Jumbo (acoustic)
- Classical, Flamenca Blanca (classical)
- Resonator Steel (slide-oriented)
- Selmer-Style (Gypsy jazz)
- P-Style Bass, J-Style Bass, Full Hollow Bass (bass)

**Factory Parts (~90 files)**
- Bodies (12): Alder, Ash, Mahogany, Basswood, Swamp Ash, Semi-Hollow, Full Hollow, Steel Resonator, Bass variations
- Tops (5): Flame Maple, Quilt, Plain, Spalted, Cedar
- Necks (8): Maple Bolt-On C/V, Mahogany Set-Neck, Maple Set-Neck, Multi-Scale, Bass variants
- Fretboards (5): Rosewood, Ebony, Maple, Pau Ferro, Bound Ebony
- Fret Sets (4): Medium-Jumbo Nickel-Silver, Jumbo Stainless, Vintage Small, Fretless
- Nuts (4): Bone, Graphite Locking, Brass
- Bridges (10): ABR-1, Modern TOM, Vintage/Modern Trem, Floyd Rose, Hardtail, Wraparound, Bass variations, Resonator Cone
- Tailpieces (3): Stopbar, Trapeze, Vibrola-Style
- Tuners (5): Kluson, Modern Sealed, Locking, Vintage Open-Back, Bass
- Strings: 12 materials & gauges (NPS, bronze, nylon, roundwound, flatwound, coated)
- Pickups (18): Humbuckers, Single-Coils, P90, Active, Bass, Acoustic (piezo, magnetic, condenser, blended)
- Wiring (8): 50s LP, Modern LP, Vintage Strat, Modern Strat, T-Style, Active EMG, Bass passive/active
- Pickguards (6): 1-ply/3-ply options, colors (cream, black, white, tortoise, mint)
- Picks (6): Celluloid, Nylon, Delrin, Ultex, Wooden, Thumbpick
- Slides (5): Glass, Brass, Steel Bar Lap Steel, Ceramic, Chrome

**Factory Tunes (6 `.luthiertune` files)**
- Fingerstyle Etude in Em: arpeggio study
- Late Night in a Minor Key: jazz comping with melody
- Highway Sketch: country strum progression
- Chug Test: 7-string metal riff
- Delta Slide: 12-bar slide blues
- Funk Slap Groove: funk slap with ghost notes

**Rhythm Patterns & Genre Kits (~28 files)**
- Country Boom-Chick, Delta Shuffle, Piedmont Alt-Bass, Bossa Comp, Samba, Reggae Skank, Rocksteady, Dub
- Funk 16th, Wah Rhythm, Punk Down, Metal Chug, Djent Grid
- Freddie Green, La Pompe, Rasgueado, Alzapua, Classical Etude, Classical Arpeggio, Ambient Swell, Post-Rock Arpeggio
- Bass patterns: Motown Fingerstyle, Funk Slap, Reggae One-Drop, Punk Pick, Walking Bass, Latin Tumbao, Root-Fifth Country, Dub Bass

**Example Setlists (10 `.luthierset` files)**
- Solo Acoustic Coffee Shop, Rock Cover Set, Blues Trio, Bossa Cafe, Metal Warmup, Bluegrass Jam, Funk Trio, Ambient Loop Show, Country Bar Set, Jazz Combo
- All reference factory presets

**Backing Tracks (6 WAV files)**
- Royalty-free, produced for the plugin
- 12-Bar Blues in E, Funk Groove in D, Bossa Nova in Bb, Rock Jam in A, Country Shuffle in G, Ambient Pad in Am
- 3-5 minutes each at 60-140 bpm

**Example MIDI Clips (12 files)**
- One per genre kit: genre-appropriate riffs and progressions

**Impulse Responses (720 total)**
- 216 body IRs: acoustic bodies at various sizes, woods, mic positions (synthesized)
- 504 cabinet IRs: 10 cabinets × 8 speakers × mic positions/axes (generated deterministically)

---

## Licensing & Editions

**Luthier Pro ($200)**
- Every feature in every spec
- 13 amplifiers, 21 pedals, 25 guitars, all effects
- Workshop bench, Tune Builder, Techniques tab, Tone Match, Studio routing, Advanced ranges
- Deep realism controls (environment, tuning stability, noise floor, sustain macros)
- All 36 factory presets, 720 impulse responses
- Notation and MIDI export, live MIDI out
- All plugin formats and standalone
- License activation and online features

**Luthier Free**
- Same engine and sound quality: identical DSP, oversampling, polyphony
- 6 factory guitars (double-cut, single-cut, T-style, dreadnought, classical, P-style bass)
- 7 amplifiers, 15 pedals (core effects only)
- 16 factory presets (clean, break-up, blues, country, funk, folk, classical, bass)
- 4 snapshots per preset, single snapshot bank
- 8 genre kits and patterns
- Looper: 1 layer, 60 seconds max
- Audio export: 5 minutes max
- ~130 impulse responses
- User IR loading: 1 slot in cabinet position
- No Workshop, Tune Builder, Techniques tab, Tone Match, advanced realism panels, notation export, MIDI export, setlists, morph, monitor mix, multiple amps/pedals
- All plugin formats and standalone
- Same accessibility, safety, privacy features as Pro
- Pro files load transparently (bypass Pro features audibly, carry data verbatim on save)

---

## Offline Renderer (CLI)

**luthier-render Command-Line Tool**
- Batch MIDI-to-audio conversion with presets
- Audition mode: play a scale or performance with a guitar and preset
- List presets, guitars, parts, tuning information
- Deterministic output: same MIDI + preset = byte-identical render
- Useful for regression testing and batch work
- Returns non-zero exit code on error (CI-friendly)

---

## Updates & Analytics

**Update Checks**
- Automatic check for updates on startup
- Notification of available versions
- One-click update launcher
- Update channel: `pro/stable`, `pro/beta` (Pro); `free/stable` (Free)

**Telemetry & Crash Reporting** (Opt-In)
- Crash reports with minimal data (session ID, error type, version)
- Anonymized feature usage (which tabs opened, which presets, no audio content)
- Privacy dashboard: see what data has been collected
- Opt-in by default (user must enable explicitly)
- Policy file included

**Privacy**
- No audio sent anywhere
- No network access without opt-in
- Local data: presets, settings, user guitars in Documents/Luthier
- Crash reporting limited to error context, no personal data

---

## Supported Hosts & Platforms

**Plugin Formats**
- VST3: Windows, macOS, Linux
- AU: macOS only
- CLAP: Windows, macOS, Linux
- Standalone application: Windows, macOS, Linux

**Tested DAWs**
- Ableton Live
- Logic Pro
- Cubase
- Studio One
- Reaper
- FL Studio
- Bitwig Studio
- Pro Tools
- Plus any host supporting VST3, AU, or CLAP

**System Requirements**
- CMake 3.22+
- JUCE 8.0.10
- MSVC 2022, Xcode 14, GCC 11 / Clang 14
- 4+ GB RAM recommended
- Multicore CPU recommended (10+ GB disk for content)
