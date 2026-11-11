# Pedal Implementations

Two separate implementations exist:

- **Legacy racks**: 23 `PedalType` values in `Source/DSP/Effects/Pedal.h`, implemented in `PedalsDrive.cpp` and `PedalsMod.cpp`, hosted by `EffectsChain` (eight slots per rack). Each pedal declares `PedalParam` descriptors. New types are appended before `NumTypes`.
- **Reference pedalboard**: `Source/ReferencePedals/` (engine, `Generic/` effects, `Models/` named-model prototypes) with its interface in `Source/UI/ReferencePedals/`. Models derive from `PedalBase`, declare a `PedalDesc`, and register through `Registration.h`. Only registered models are visible to users; the named prototypes are not registered.

See [spec/reference-pedalboard/ARCHITECTURE.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/reference-pedalboard/ARCHITECTURE.md), [CATALOG.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/reference-pedalboard/CATALOG.md), [CONTRIBUTING.md](https://github.com/djshellshoxxx/luthier/blob/master/CONTRIBUTING.md) ("Adding a pedalboard model") and [EFFECTS_REFERENCE.md](https://github.com/djshellshoxxx/luthier/blob/master/docs/EFFECTS_REFERENCE.md).
