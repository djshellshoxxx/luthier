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
- [x] M9  MidiInterpreter
- [x] M10 TechniqueEngine + ChordVoicer
- [x] M11 WhammyEngine
- [x] M12 CableSim
- [x] M13 PreEffectsChain
- [x] M14 AmpEngine
- [x] M15 PostEffectsChain
- [x] M16 CabinetEngine
- [x] M17 RoomEngine
- [x] M18 MasterBus
- [x] M19 Preset system
- [x] M20 UI Theme + widgets
- [x] M21 UI Easy mode
- [x] M22 UI Advanced mode
- [x] M23 MIDI Learn + right-click
- [x] M24 Export (audio + MIDI)
- [x] M25 Help / Options / Randomize / Reset
- [x] M26 IR libraries generated
- [x] M27 Test suite
- [x] M28 Debug + troubleshooting features
- [x] M29 Easter egg
- [x] M30 Docs complete

## Current state

Feature complete. 98 of 98 tests pass. VST3 and standalone build clean.

| | |
|---|---|
| Source | ~34 600 lines of C++ across 99 files |
| Parameters | 342, every one automatable, named and text-round-tripping |
| Guitars | 25 |
| Factory presets | 36 |
| Impulse responses | 216 body, 504 cabinet (synthesised — see `docs/KNOWN_ISSUES.md`) |
| Tests | 98, covering DSP, model, parameters, presets, fuzz and integration |

### Targets

| Target | What it is |
|---|---|
| `Luthier_VST3` | the plugin |
| `Luthier_Standalone` | the same thing, hosted |
| `LuthierTests` | the test suite; exit code is the result |
| `LuthierRender` | offline renderer, for regression listening and CI |

Build with `scripts/build.ps1`, or:

```
cmake --build build --config Release
build/LuthierTests_artefacts/Release/LuthierTests.exe
```

### Not done

Listed honestly in `docs/KNOWN_ISSUES.md` under "Not yet implemented": CLAP and
Linux builds, signed installers, the manual per-host test matrix, drag-out export,
and the practice tools (metronome, looper, backing tracks).

## History

See `docs/CHANGELOG.md`.
