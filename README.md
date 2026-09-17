# Luthier

A physically-modelled guitar. No samples anywhere in it.

Every note is a vibrating string: a digital waveguide with real mass, tension and
stiffness, coupled to the other strings through a bridge, coloured by a body whose
resonances come from its actual dimensions, sensed by a pickup that is an inductor
with parasitic capacitance, and pushed through a valve amp, a speaker and a room.

The point of modelling rather than sampling is that techniques compose. A bend into
a slide into a vibrato into a harmonic works because the engine knows what a string
is, not because someone recorded that combination.

---

## Contents

- [Build](#build)
- [What is in the box](#what-is-in-the-box)
- [Architecture](#architecture)
- [The offline renderer](#the-offline-renderer)
- [Tests](#tests)
- [Regenerating the impulse responses](#regenerating-the-impulse-responses)
- [Where things live at runtime](#where-things-live-at-runtime)
- [Documentation](#documentation)
- [Deliberate deviations from the brief](#deliberate-deviations-from-the-brief)
- [Licence](#licence)

---

## Build

### Requirements

| | |
|---|---|
| CMake | 3.22 or newer |
| Compiler | MSVC 2022, Xcode 14, or GCC 11 / Clang 14 |
| JUCE | 8.0.10, fetched into `ThirdParty/JUCE` |
| Python | 3.9+, only to regenerate the icon and the IR library |

JUCE is not vendored. Fetch it once:

```bash
git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
```

### Configure and build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Windows with Visual Studio:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

On a machine with limited RAM, add `--parallel 2`. The JUCE modules are the
expensive part of the first build; after that, incremental builds are quick.

### Targets

| Target | Output |
|---|---|
| `Luthier_VST3` | `Luthier.vst3` |
| `Luthier_Standalone` | standalone application |
| `Luthier_AU` | `Luthier.component` (macOS only) |
| `LuthierTests` | the unit and integration test runner |
| `LuthierRender` | `luthier-render`, the offline batch renderer |

Turn the console apps off with `-DLUTHIER_BUILD_TESTS=OFF -DLUTHIER_BUILD_CLI=OFF`.

### Install

The build copies `Resources/` beside each artefact, so a built plugin is
self-contained. To install, copy the whole bundle:

| Platform | Destination |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\Luthier.vst3` |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/Luthier.vst3` |
| macOS AU | `~/Library/Audio/Plug-Ins/Components/Luthier.component` |
| Linux | `~/.vst3/Luthier.vst3` |

---

## What is in the box

**25 instruments** across electric, acoustic, classical and bass, including
7- and 8-string, baritone, 12-string, resonator and fretless. Each carries its own
body, woods, bracing, scale length, pickups, string set, tuning and default rig.

**17 tunings** plus fully custom per-string tuning, and seven temperaments.

**12 string materials** and eleven gauge sets, with tension, mass, inharmonicity,
sustain and brightness all computed from the wire rather than looked up.

**21 pedals** across two eight-slot, reorderable racks - one before the amp, one in
the effects loop.

**13 amplifiers** with real passive tone stacks, cascaded valve stages, push-pull
power amps, sag and output transformers.

**Ten cabinets, eight speakers, seven microphones**, with dual-mic blending and
time-of-flight alignment.

**720 impulse responses** - 216 for bodies, 504 for cabinets - generated from the
same physics the engine uses.

**36 factory presets** across Electric, Acoustic, Classical, Bass and Utility.

---

## Architecture

```
MIDI in
   |
   v
MidiInterpreter ........ modes, MPE, CC map, chord grouping, strum
   |
   v
TechniqueEngine ........ pluck / hammer-on / pull-off / slide / bend / mute /
   |                     harmonic / pinch / tap / bottleneck
   v
TuningEngine ........... string + fret + bend -> Hz, with temperament,
   |                     detune, intonation and drift
   v
StringEngine x N <---> CouplingMatrix
   |                   (sympathetic resonance through the bridge)
   v
BodyEngine ............. partitioned convolution, or modal synthesis built
   |                     from the body's real dimensions
   v
PickupEngine ........... per-string positional comb, then the coil's LCR tank
   |
   v
CableSim -> PreEffectsChain -> AmpEngine -> PostEffectsChain
   |
   v
CabinetEngine -> RoomEngine -> MasterBus -> out
```

### Source layout

```
Source/
  DSP/
    Common/       guards, filters, smoothing, LFOs, oversampling
    String/       waveguide, fractional delay, excitation
    Coupling/     the sympathetic resonance matrix
    Body/         convolution and modal synthesis
    Pickup/       positional comb plus the electrical model
    Whammy/       vintage, Floyd Rose, TransTrem, Bigsby
    Cable/        capacitance roll-off
    Effects/      pedal base class, 21 pedals, the racks, the hidden effect
    Amp/          preamp, tone stack, power amp, cabinet, room
    Master/       gain, limiter, metering
  Model/
    Guitar/       string materials, body models, the instrument library
    Playing/      tuning, technique detection, chord voicing, MIDI interpretation
  Presets/        the preset format and the factory bank
  Support/        MIDI learn, MIDI capture, audio export, diagnostics, IR lookup
  UI/             theme, widgets, fretboard, guitar illustration, panels, overlays
  Tests/          the test suite
Tools/            the offline renderer
scripts/          icon and IR generation
```

### Rules the DSP obeys

These are not style preferences; each one exists because violating it causes an
audible failure in a real host.

1. All internal DSP is `double`. Waveguide feedback loops accumulate error too
   fast in single precision.
2. Every buffer is allocated in `prepareToPlay`. Nothing allocates, locks, or
   touches a file in `processBlock`.
3. Every recursive stage has a DC blocker and a NaN guard.
4. Every continuous parameter is smoothed; discrete switches crossfade over 5 ms.
5. Everything sample-rate-dependent is recomputed in `prepareToPlay`. There is no
   hardcoded sample rate anywhere.
6. Times are stored in seconds and Hz, never in samples, so presets are portable
   across sample rates.
7. `ScopedNoDenormals` at the top of `processBlock`, plus explicit flushing in the
   feedback loops.
8. Every module has `reset()`.
9. Every module is testable in isolation.
10. Every nonlinear stage is oversampled, 4x by default.

---

## The offline renderer

`luthier-render` runs the full engine from the command line.

```bash
luthier-render --midi riff.mid --preset "Modern Metal Chug" --out riff.wav
luthier-render --audition "Major Scale" --guitar "Les Paul" --out demo.wav --verbose
luthier-render --list-presets
luthier-render --help
```

It loads the same presets, uses the same engine and produces the same sound as the
plugin, which makes it useful for batch work and for regression-testing a build
against known-good renders.

---

## Tests

```bash
cmake --build build --config Release --target LuthierTests
./build/LuthierTests_artefacts/Release/LuthierTests
```

Filter by name:

```bash
./LuthierTests Tuning          # only the tuning suite
./LuthierTests --list          # list without running
```

The suite covers the DSP primitives, each engine module, the musical model, and
the whole plugin end to end - including a 10,000-state parameter fuzz, a
preset round trip verified by comparing rendered audio rather than just numbers,
and a mono-compatibility check. It returns a non-zero exit code on failure, so it
drops straight into CI.

For plugin-level validation, [pluginval](https://github.com/Tracktion/pluginval)
1.0.3 passes at strictness 10 with nothing reported - including the parameter
thread-safety, background-thread-state, bus-layout and parameter-fuzz tests, which
are the ones that catch what a plugin does wrong outside its own audio path:

```bash
pluginval --strictness-level 10 --validate Luthier.vst3
```

---

## Regenerating the impulse responses

```bash
python scripts/make_irs.py      # body and cabinet IRs
python scripts/make_icon.py     # application icon
```

The IRs are **synthesised**, not measured - see
[Deliberate deviations](#deliberate-deviations-from-the-brief). Both scripts are
deterministic, so regenerating produces byte-identical files.

---

## Where things live at runtime

| | Windows | macOS / Linux |
|---|---|---|
| User presets | `%USERPROFILE%\Documents\Luthier\Presets\User` | `~/Documents/Luthier/Presets/User` |
| Renders | `...\Documents\Luthier\Renders` | `~/Documents/Luthier/Renders` |
| Diagnostics | `...\Documents\Luthier\Diagnostics` | `~/Documents/Luthier/Diagnostics` |
| Factory presets | inside the plugin bundle under `Resources\Presets\Factory`, or `%USERPROFILE%\Documents\Luthier\Presets\Factory` if that folder is read-only | inside the bundle under `Resources/Presets/Factory`, or `~/Documents/Luthier/Presets/Factory` if that folder is read-only |

Options has a button for each of these.

---

## Documentation

| File | What is in it |
|---|---|
| [`docs/USER_MANUAL.md`](docs/USER_MANUAL.md) | how to play it, control by control |
| [`docs/GUITAR_PHYSICS.md`](docs/GUITAR_PHYSICS.md) | the maths behind the string, body and pickup |
| [`docs/PLAYING_TECHNIQUES.md`](docs/PLAYING_TECHNIQUES.md) | how each technique maps to model parameters |
| [`docs/PRESET_FORMAT.md`](docs/PRESET_FORMAT.md) | the `.luthierpreset` schema |
| [`docs/KEYBOARD_SHORTCUTS.md`](docs/KEYBOARD_SHORTCUTS.md) | every shortcut |
| [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md) | when something is wrong |
| [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) | what is not finished, honestly |
| [`docs/CHANGELOG.md`](docs/CHANGELOG.md) | what changed |
| [`docs/THEME_AS_BUILT.md`](docs/THEME_AS_BUILT.md) | the visual identity as built |

---

## Deliberate deviations from the brief

Every one of these was a choice, and each is documented where it lives in the code.

**Impulse responses are synthesised, not measured.** Shipping real captures would
mean licensing recordings of trademarked instruments and speakers. Instead the IRs
are generated from plate and Helmholtz theory for bodies, and from designed
minimum-phase magnitude responses for cabinets - the same physics the modal and
procedural paths use. The benefit is that the convolution and modal paths agree
with each other; the cost is that they lack the idiosyncratic detail of a real
capture. The engine loads any WAV, so commercial IRs work.

**The guitar is drawn, not photographed.** Same reason. The illustration is
generated from the live `GuitarSpec`, so it is always correct - including for an
instrument the user just built, which a photograph could never be.

**The waveguide uses Lagrange interpolation by default, not allpass.** The engine
spec recommends both. Allpass interpolation is an IIR whose state has to settle
whenever the delay changes, which is audible as a chirp under the fast modulation
dive-bombs and vibrato require. Fifth-order Lagrange has negligible HF loss below
0.4 fs and no modulation artefacts. All three are implemented and selectable.

**Damping is expressed as a filter cutoff, not as a raw coefficient.** The engine
spec's section 5.3 says a higher damping coefficient means brighter and longer,
while its section 4 says palm-muting lowers the cutoff from 5 kHz to 800 Hz. Those
contradict. The second reading is the physical one, so the engine follows it.

**The pickup's positional comb runs per string, before the sum.** The spec's block
diagram puts the whole pickup after the string sum, but the comb delay depends on
that string's length, so summing first would give every string the same comb. The
positional stage is per string; the electrical stage runs once on the sum, because
a real pickup has one coil for all six strings.

**Sympathetic coupling updates per sample, not per block.** The spec warns against
a per-sample N-to-N update on cost grounds, but for twelve strings it is at most
144 multiply-adds per sample, and updating per block injects a step into every
delay line at each buffer boundary - an audible tick. The saving is made elsewhere:
one receive filter per string rather than per pair.

**Inharmonicity is computed from the core diameter.** Only the core of a wound
string resists bending. Using the outside diameter would make a wound low E about
seventy times stiffer than it is, and it would sound like a piano.

**The window cutaway is drawn rather than clipped.** Plugin windows are rectangular
in every host. The cutaway is carved out of the panel surface, which reads as the
intended silhouette without host-dependent behaviour.

**The visual identity is re-tinted.** `theme.md` allows each plugin to re-tint one
accent; this one also warms the neutrals from blue-black to walnut-black and the
text to aged ivory, because a cool grey guitar looked wrong. The structure - the
8 px grid, the 270-degree external value arcs, the section rules, the corner radii,
the header layout, the signature notch, the output LED, the scrolling data stream -
is unchanged. See [`docs/THEME_AS_BUILT.md`](docs/THEME_AS_BUILT.md).

---

## Licence

Luthier is licensed, not sold; see Help > About inside the plugin for the full
text. Built with [JUCE](https://juce.com) under the JUCE licence applicable to this
build. Third-party notices are in `THIRD_PARTY_LICENCES.txt`.
