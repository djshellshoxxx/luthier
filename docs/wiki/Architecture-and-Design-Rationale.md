# Architecture and Design Rationale

Sources: `spec/README.md`, `spec/DECISIONS.md`, `spec/editions.md`. Points marked *(inferred)* are read
from the code layout, not from a written decision.

## Signal flow

```
MIDI in -> MidiInterpreter -> TechniqueEngine -> TuningEngine
        -> StringEngine x N <-> CouplingMatrix (sympathetic resonance through the bridge)
        -> BodyEngine (convolution or modal synthesis)
        -> PickupEngine (positional comb, then coil LCR tank)
        -> GuitarCircuit -> PreEffectsChain -> AmpEngine -> PostEffectsChain
        -> CabinetEngine -> RoomEngine -> MasterBus -> out
```

## Why it is built this way

- **Modelling, not sampling.** Techniques compose because the engine knows what a string is. A
  bend into a slide into a vibrato works without recording that combination (`spec/README.md`).
- **Waveguide strings with Lagrange interpolation.** Lagrange (fifth order) was chosen over allpass
  because allpass state must settle whenever the delay changes, which is audible as chirp under
  fast modulation. Allpass is implemented and tested but not used (`spec/README.md`, deviations).
- **Synthesised impulse responses.** Shipping captures would mean licensing trademarked recordings.
  Bodies and cabinets are generated from plate/Helmholtz theory and minimum-phase designs. Commercial
  WAV IRs also load (`spec/README.md`).
- **Drawn guitar illustration.** Generated from the live `GuitarSpec`, so it is correct for user-built
  instruments too.
- **Damping as a filter cutoff.** The spec contradicted itself on the damping coefficient; the
  physical reading (palm-mute lowers cutoff) was adopted (`spec/README.md`).
- **Per-string pickup comb before the sum.** The comb delay depends on the string, so it must sit
  before summation (`spec/README.md`).
- **Two editions from one source.** Pro and Free are selected by CMake (`cmake/Editions.cmake`) and
  differ in product name, bundle ID, plugin code and content folder (`spec/editions.md`). *(inferred:
  feature gating is in code under `Source/Support/Edition.*`.)*
- **Real-time rules.** The ten DSP rules in `spec/README.md` exist because violating each causes an
  audible failure in a host.

## Source layout

Top-level `Source/` folders map to the signal chain: `DSP/` (String, Coupling, Body, Pickup, Circuit,
Whammy, Effects, Amp, Feedback, Noise, Slap, Slide, Master, Common), `Model/` (guitar, playing,
workshop), `Presets/`, `Modulation/`, `Routing/`, `UI/`, `Support/`, and feature folders such as
`Rhythm/`, `Tune/`, `Workshop/`, `Live/`, `Practice/`, `Export/`, `Notation/`, `ToneMatch/`.
`Source/WIP/` is not compiled.

## Known limits

Recorded in `docs/KNOWN_ISSUES.md`. Build and test status on each platform is determined by CI, not
by this page.
