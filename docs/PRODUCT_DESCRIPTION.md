# Luthier Product Description

**Living draft — refresh after the current expansion (new instruments + tier features) lands.**

A physically-modelled realistic guitar synthesizer plugin. No samples anywhere.

---

## Elevator Pitch

Luthier is a physically-modelled guitar VST, AU, CLAP and standalone app that synthesizes every note from the physics of real strings, bodies, pickups, and amplifiers. Every technique composes: a bend into a slide into a vibrato into a harmonic works because the engine knows what a string is, not because someone recorded that combination. Indistinguishable from a real guitar in a mic'd room when played by a competent guitarist.

---

## Full Description

### The Sound of Physics, Not Samples

Most guitar plugins play back recordings of techniques the programmer anticipated. Luthier models the physics instead. Every note is a vibrating string: a digital waveguide with real mass, tension and stiffness, coupled to the other strings through a bridge, coloured by a body whose resonances come from its actual dimensions, sensed by a pickup that is an inductor with parasitic capacitance, and pushed through a valve amp, a speaker and a room.

The result: techniques compose. A bend into a slide into a vibrato into a harmonic works. A palm mute into a slap into a harmonically-rich fingerstyle passage works. Every articulation reacts correctly because the engine understands the guitar, not because it pre-recorded that combination. This is what makes Luthier feel alive under your fingers—not because it sounds sampled (it doesn't), but because it *behaves* like an instrument.

### 25 Instruments, Infinitely Customizable

Luthier ships with 25 factory guitars: electric single-cuts, double-cuts, semi-hollows and offset models; 7-, 8- and 12-string variants; acoustic dreadnoughts and parlors; classical nylon-strings; a Gypsy-jazz Selmer-style; slide-oriented steel resonators; and a full bass section. Each carries its own body, woods, bracing, scale length, pickups, string set, tuning and default rig.

But factory guitars are just the start. The Workshop lets you build your own from ~90 factory parts: swap bodies (mahogany, ash, alder, spruce, cedar), necks (bolt-on or set), fretboards (rosewood, ebony, maple), pickups (single-coil, humbucker, P90, piezo, acoustic), bridges, tuners, even strings down to gauge and material. Every part change lands as a physical parameter change—not an EQ tweak, a physical one. You hear it instantly on the live-drawn guitar, and a spectrum-delta panel shows you exactly what changed. Want a semi-hollow with a set maple neck and a single-coil bridge pickup? Build it. Sound is good.

### Play Like You Mean It

Luthier gives you real-time control over the techniques that make a guitar *playable* by an expert: scrape, slide, slap, palm mute, tapping, and microtonal bends. A fretboard overlay shows you bend and slide ranges. The Techniques tab has seven sub-tabs—one per technique—where you dial in attack, range, feel and style. Techniques combine cleanly under cascade rules: you can slap into a slide into a harmonic without the engine fighting itself.

### Every Detail Matters

**String noise** is right: finger slides squeak on wound strings (more on E and A than on G, B, E), picks click at attack, frets buzz if the action is light, strings ring sympathetically through the bridge. **Amp modeling** is real: passive tone stacks tuned to specific amp models, cascaded preamp stages with tube saturation, push-pull power amps with sag, output transformers that interact with cabinet impedance. **Cabinets and mics** are measured: ten cabinets, eight speaker types, seven mic placements, blended with phase correction and time-of-flight alignment. **Pickup positioning** is continuous: a pickup at the bridge sounds different than one at the neck because the string's harmonic content changes—just like a real guitar.

Shipping with 36 factory presets across electric, acoustic, classical, bass and utility categories, each one is a gateway to exploring what Luthier can do: clean chime, driven crunch, metal chug, ambient wash, funk slap, bluegrass twang, jazz warmth, and more. The Tone Match suite (Pro) lets you load custom amp and cabinet impulse responses and match your recorded tone to a reference.

### Compose, Not Just Play

Tune Builder (Pro) turns Luthier into a sketchpad for actual tunes and melodies. Draw a chord progression, add a melody line, pick a rhythm pattern, and watch a three-minute loop play back with the engine doing the playing. Export as MIDI, MusicXML, Guitar Pro or audio. It's the difference between having an instrument and having a songwriter in a box.

### Live Performance Ready

Snapshots let you store and recall complete rigs in a finger-tap. Morphing smoothly interpolates between snapshots—perfect for slow tonal evolution during a song. A live strip shows tap tempo, snapshots, setlists and a kill switch. The rhythm engine detects chords you play and voices them across strings automatically, with 28 strum and fingerpick patterns (rock, blues, funk, metal, bossa, reggae, country, and more). Edit patterns, save your own, adjust the feel macro, and watch Luthier humanize the timing and velocity for you.

### Export Everything

Live tab view shows ASCII tab while you play. Notation export sends MusicXML to Finale, Sibelius, Dorico or MuseScore; Guitar Pro tabs are exportable; audio exports at your session sample rate. MIDI export includes every event class: every bend, slide, mute, harmonic and articulation as a proper MIDI controller or note property. The offline renderer (`luthier-render`) does batch MIDI-to-audio conversion from the command line—useful for regression testing and batch work.

### Workflow Matters

**Easy Mode** is one column: tone, amp/cab, a little reverb and delay. Perfect for learning, perfect for live tweaks. **Advanced Mode** is four columns: every parameter ever—body dimensions, pickup position, circuit topology, string noise, techniques, tuning stability, realism controls. A **Character macro** is one knob controlling dead spots, fret wear, tuner drift, aged electronics and body break-in; slide the String Age knob from fresh to dead and hear decades of oxidation and contamination in the tone. The Workshop is a visual guitar you drag parts onto; hit-tested, with spectrum preview.

Everything is undoable. Randomize draws from your current preset's valid space. MIDI Learn maps any CC to any parameter.

### Sound Quality to Match the Model

**Double-precision DSP throughout**: the waveguide feedback loops accumulate error too fast in single precision. **4x oversampling by default** for nonlinear stages (amp, drive), adjustable to 2x or 8x. **No allocations, locks or file I/O in the audio callback**—every buffer pre-allocated, every computation sample-rate-independent, denormals disabled. The result: clean audio at 44.1 kHz to 192 kHz, on any host, with proper latency reporting.

Polyphony is 6 strings by default (7–12 for extended range), 12 for 12-string mode. CPU quality modes (High, Medium, Low, Auto) let you dial quality vs. CPU on lower-end machines without losing musicality.

### Factory Content That Ships Complete

- **12+ guitars** in every style (electric, acoustic, classical, bass, slide)
- **36 presets** across 11 genres: every major guitar category has an easy preset, a medium one, and a showcase one
- **720 impulse responses** (synthesized, not measured—same physics the engine uses, no licensing fees)
- **6 factory tunes**: fingerstyle etudes, jazz comping, country strums, metal riffs, slide blues, funk slaps
- **~28 rhythm patterns and genre kits**: country, blues, funk, metal, reggae, bossa, folk, and more
- **6 backing tracks**: 12-bar blues, funk groove, bossa, rock, country shuffle, ambient pad
- **10 example setlists**: curated preset sequences for solo acoustic, rock covers, blues trio, and more
- **Metronome, looper, backing-track player, scale trainer, ear training, tab reader, session recorder** (Pro)

---

## Highlights

- **Physically modelled**. No samples. Every note is physics: strings vibrate, bodies resonate, pickups transduce, amps distort, speakers color, rooms reflect. Techniques compose because the engine understands the guitar.

- **25 instruments**. Electric, acoustic, classical, bass, slide. Each with authentic body, woods, scale length, pickups, tuning. Plus a Workshop to build your own from ~90 factory parts.

- **Every technique**. Bend, pre-bend, vibrato, slide, hammer-on, pull-off, palm mute, harmonics (natural, pinch, tapped, artificial), tapping, bottleneck, whammy bar (vintage, Floyd Rose, TransTrem). Real-time control over scrape, slap, mute grid, microtonal bends. Techniques combine cleanly.

- **Authentic amps and cabinets**. 13 valve amp models with passive tone stacks, cascaded preamp, power-tube saturation and sag. 10 cabinets, 8 speakers, 7 mics with dual-mic blending and phase correction. 720 impulse responses (synthesized from physics). User IR loading.

- **Rhythm engine**. Chord detection, voicing, strum and fingerpick patterns, 28 genre kits. Play a chord, engine voices it. Pick a pattern, engine plays it. Humanize feel adjustable.

- **36 factory presets** across 11 genres (rock, blues, jazz, country, classical, metal, funk, reggae, etc.); curated difficulty ladder so beginners get a great sound on day one, and professionals hear what Luthier can do.

- **Tone Match** (Pro). Load custom amp and cabinet IRs. Match tone to a reference. Capture utility to generate IRs from your own gear.

- **Compose, not just play** (Pro). Tune Builder: draw a chord progression and melody, pick a rhythm, export as MIDI, MusicXML, Guitar Pro or audio. Three-minute loops that sound finished.

- **Notation and MIDI export** (Pro). MusicXML to Finale/Sibelius/Dorico, Guitar Pro tabs, ASCII tab. MIDI export with full event classes (every bend, slide, harmonic, technique). Live tab view during play.

- **Double-precision DSP, 4x oversampling, no callback allocations**. Clean audio from 44.1 kHz to 192 kHz on any host.

- **Easy and Advanced modes**. One column for beginners and live tweaks, four columns for deep sound design. Workshop for visual guitar building. Everything undoable, MIDI learnable, randomizable.

- **Accessibility**. Screen reader, keyboard-only navigation, color-blind palettes, UI scale 75–200%, high contrast, safe mode, panic button, kill switch.

- **Cross-platform**. VST3, AU, CLAP, Standalone on Windows, macOS, Linux. Tested in Ableton, Logic, Cubase, Studio One, Reaper, FL, Bitwig, Pro Tools.

- **Free and Pro editions**. Same engine and sound quality. Free has 6 guitars, 7 amps, 15 pedals, 16 presets, 1 snapshot bank, no Workshop, Tune Builder, advanced techniques or notation export. Pro has everything.

---

## Who It's For

**For guitarists and bassists** who want a realistic, responsive instrument that behaves like a real guitar—every technique composes, every playing style is possible.

**For recording engineers and producers** who want a great guitar sound without mic'ing an amp, with full control over tone, technique, and mix.

**For educators** who need a tool that teaches technique through physical understanding, not sample playback.

**For composers and songwriters** who want to compose guitar parts by ear, sketch ideas quickly, and export to notation or DAW.

**For anyone working in a DAW** who needs a guitar that sounds like a recording but plays like an instrument.

---

## What Makes It Different

Luthier is not a sample library with fancy playback. It's not a model trained to imitate recordings. It's a physics engine that *is* a guitar: strings vibrate, bodies resonate, pickups transduce, amps distort, and every change you make lands as a physical parameter change, not a cosmetic one. You build a guitar in the Workshop, and it sounds like what you built because the engine knows what you built.

Techniques don't fight. A slap into a slide into a harmonically-rich fingerstyle passage works because the cascade resolver understands technique interactions. An expert player on Luthier sounds like an expert because the instrument responds to playing nuance the way a real guitar does.

And it's not slow. Double-precision DSP throughout, 4x oversampling, no audio-thread allocations, proper sample-rate independence, and CPU quality modes mean you can run it on a MacBook Air during a live show without glitching.

---

## License & Pricing

**Luthier Pro**: $200. Every feature, every instrument, every effect, Workshop, Tune Builder, Tone Match, Techniques tab, notation and MIDI export, all 36 presets, all 720 IRs, all plugin formats, standalone.

**Luthier Free**: Unlimited. Same engine and sound quality. 6 guitars, 7 amps, 15 pedals, 16 presets, 1 snapshot bank, looper (1 layer, 60 s), audio export (5 min), no Workshop, Tune Builder, advanced techniques, tone match, notation export, MIDI export. All plugin formats, standalone, same accessibility.

---

## Technical Highlights

- **Physics-based synthesis** of strings, bodies, pickups, amps, cabinets, rooms
- **12-precision DSP**, double throughout; 4x oversampling for nonlinear stages
- **No allocations in audio callback**: every buffer pre-allocated, every parameter time-independent
- **Sample rates**: 44.1 kHz to 192 kHz, live rate switching
- **Polyphony**: 6-12 strings depending on mode
- **Latency**: reported to host, proper MIDI/audio sync
- **VST3, AU, CLAP, Standalone** on Windows, macOS, Linux
- **~720 impulse responses** (synthesized from physics)
- **Offline renderer** CLI for batch MIDI-to-audio and regression testing

---

## Getting Started

1. **Load a preset.** 36 to choose from. They sound good on factory guitars.
2. **Try Easy Mode.** One column: tone, amp, reverb, delay.
3. **Switch to Advanced.** Four columns: body, amp/rig, effects, modulation. Explore.
4. **Visit the Workshop.** Click the wrench. Drag parts onto the guitar. Watch the spectrum change.
5. **Play.** The engine responds to every technique: bends, slides, harmonics, palm mutes, techniques you invent.
6. **Export.** Audio, MIDI, notation (Pro), or render via CLI.

---

## What's Next

Luthier is live. Roadmap includes expanded instrument families (resonators, 5-string basses, steel guitars), additional amp and effect models, content update packs (vintage amp pack, slide pack, Latin pack), and continued physical realism refinements.
