# LUTHIER — Build Progress

Resumable build log. Update after every milestone.

## Environment
- Windows 10 Pro 19045, 4 logical cores, 3.88 GB RAM (build with low parallelism)
- CMake 4.4.3, MSVC 14.44.35207 (VS2022 BuildTools), Windows SDK 10.0.26100.0
- JUCE 8.0.10 at `ThirdParty/JUCE`
- Python 3.14.5 (used for IR generation scripts only, not at runtime)

## Milestones
- [x] M0  Specs read (spec.md, engine.md, include.md, theme.md), tree scaffolded
- [x] M1  Build system + JUCE + trivial plugin compiles
- [x] M2  DSP Common (guards, filters, smoothing)
- [x] M3  StringEngine (fixed pitch)
- [x] M4  StringEngine (bend/vibrato/slide/technique)
- [x] M5  TuningEngine
- [x] M6  PickupEngine
- [x] M7  BodyEngine (convolution + modal)
- [x] M8  CouplingMatrix
- [ ] M9  MidiInterpreter
- [ ] M10 TechniqueEngine + ChordVoicer
- [x] M11 WhammyEngine
- [x] M12 CableSim
- [x] M13 PreEffectsChain
- [x] M14 AmpEngine
- [x] M15 PostEffectsChain
- [x] M16 CabinetEngine
- [x] M17 RoomEngine
- [x] M18 MasterBus
- [ ] M19 Preset system
- [ ] M20 UI Theme + widgets
- [ ] M21 UI Easy mode
- [ ] M22 UI Advanced mode
- [ ] M23 MIDI Learn + right-click
- [ ] M24 Export (audio + MIDI)
- [ ] M25 Help / Options / Randomize / Reset
- [ ] M26 IR libraries generated
- [ ] M27 Test suite
- [ ] M28 Debug + troubleshooting features
- [ ] M29 Easter egg
- [ ] M30 Docs complete

## Current state
See CHANGELOG.md. Build with `scripts/build.ps1`.
