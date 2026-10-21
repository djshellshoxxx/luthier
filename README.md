# Luthier

Luthier is a physically modelled guitar plugin (VST3, AU, CLAP and standalone) built with
JUCE 8. There are no samples: every note is a digital waveguide string, coupled through a
bridge to a body, a pickup, a guitar circuit, an amp, a cabinet and a room.

It ships in two editions built from the same source:

- **Luthier Pro** (`LUTHIER_EDITION=PAID`, the default)
- **Luthier Free** (`LUTHIER_EDITION=FREE`)

The product overview is [spec/README.md](spec/README.md). The user manual is
[docs/USER_MANUAL.md](docs/USER_MANUAL.md).

## Build

Requirements: CMake 3.22+, a C++17 compiler (MSVC 2022, Xcode 14, GCC 11 or Clang 14) and
JUCE 8.0.10 in `ThirdParty/JUCE` (not vendored):

```bash
git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
```

On Windows use `-G "Visual Studio 17 2022" -A x64`. On Linux, `scripts/setup_linux.sh`
installs the system packages JUCE needs.

| Target | Output |
|---|---|
| `Luthier_VST3` | `Luthier.vst3` |
| `Luthier_Standalone` | standalone application |
| `Luthier_AU` | `Luthier.component` (macOS only) |
| `Luthier_CLAP` | CLAP build, when `ThirdParty/clap-juce-extensions` is present |
| `LuthierTests` | unit and integration test runner |
| `LuthierRender` | `luthier-render`, offline batch renderer |

Options: `-DLUTHIER_BUILD_TESTS=OFF`, `-DLUTHIER_BUILD_CLI=OFF`, `-DLUTHIER_LTO=OFF`,
`-DLUTHIER_EDITION=FREE`, and `-DLUTHIER_ENABLE_SENTRY=ON` for opt-in crash reporting (off by
default; the DSN is read from the environment, never hard-coded).

## Test

```bash
./build/LuthierTests_artefacts/Release/LuthierTests              # everything
./build/LuthierTests_artefacts/Release/LuthierTests Presets      # one suite or a name fragment
./build/LuthierTests_artefacts/Release/LuthierTests --list
ctest --test-dir build -C Release -R LuthierRenderCli            # renderer smoke test
```

On headless Linux, run the test binary under `xvfb-run -a`. CI (`.github/workflows/build.yml`,
`scripts/ci_build.sh`, `scripts/ci_build.ps1`) builds Windows, macOS and Linux and validates
the plugin with pluginval and clap-validator. See [spec/README.md](spec/README.md#tests) for
the test inventory.

## Install

The build copies `Resources/` beside each artefact. Copy the whole bundle:

| Platform | Destination |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\Luthier.vst3` |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/Luthier.vst3` |
| macOS AU | `~/Library/Audio/Plug-Ins/Components/Luthier.component` |
| Linux | `~/.vst3/Luthier.vst3` |

Runtime folders (presets, renders, diagnostics) are listed in
[spec/README.md](spec/README.md#where-things-live-at-runtime).

## Documentation

| | |
|---|---|
| [spec/README.md](spec/README.md) | overview, architecture, source layout, runtime folders |
| [spec/INDEX.md](spec/INDEX.md) | index of the specifications the code is built to |
| [docs/USER_MANUAL.md](docs/USER_MANUAL.md) | every screen and control |
| [docs/PLAYING_TECHNIQUES.md](docs/PLAYING_TECHNIQUES.md) | how MIDI becomes a technique, controller map |
| [docs/GUITAR_PHYSICS.md](docs/GUITAR_PHYSICS.md) | the physics behind the model |
| [docs/PRESET_FORMAT.md](docs/PRESET_FORMAT.md) | the `.luthierpreset` format |
| [docs/KEYBOARD_SHORTCUTS.md](docs/KEYBOARD_SHORTCUTS.md) | keyboard shortcuts |
| [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | install, presets, CPU, crashes |
| [docs/RELEASING.md](docs/RELEASING.md) | release checklist |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md), [docs/CHANGELOG.md](docs/CHANGELOG.md) | open issues and history |
| [docs/wiki/](docs/wiki/) | wiki pages (Home, User Guide, Developer Guide, Architecture, Codebase Map) |

## Contributing

Specifications come first: change the relevant file in `spec/` before changing public
behaviour, and record judgement calls in `spec/DECISIONS.md`. Keep DSP code on the rules in
[spec/README.md](spec/README.md#rules-the-dsp-obeys) (no allocation in `processBlock`, double
precision, DC blockers and NaN guards, oversampled nonlinear stages).

## Licence

Proprietary. Copyright © 2026 Sheldon Davidson. All rights reserved. See [LICENSE](LICENSE).
Third-party components are listed in [THIRD_PARTY_LICENCES.txt](THIRD_PARTY_LICENCES.txt).
