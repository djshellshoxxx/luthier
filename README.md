# Luthier

A physically-modelled guitar plugin (VST3, AU, CLAP and standalone), built with
JUCE 8. Every note is a digital waveguide string coupled through a bridge to a
body, a pickup, a guitar circuit, an amp, a cabinet and a room. There are no
samples in it.

The full front page, with the content counts, architecture, DSP rules and the
deliberate deviations from the brief, is [spec/README.md](spec/README.md).

## Build

Requirements: CMake 3.22+, a C++17 compiler (MSVC 2022, Xcode 14, GCC 11 or
Clang 14) and JUCE 8.0.10 in `ThirdParty/JUCE`:

```bash
git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
```

On Windows use `-G "Visual Studio 17 2022" -A x64`. On Linux,
`scripts/setup_linux.sh` installs the system packages JUCE needs. The targets
are `Luthier_VST3`, `Luthier_Standalone`, `Luthier_AU` (macOS), `Luthier_CLAP`
(when `ThirdParty/clap-juce-extensions` is present), `LuthierTests` and
`LuthierRender` (the `luthier-render` offline renderer).

## Test

```bash
./build/LuthierTests_artefacts/Release/LuthierTests            # everything
./build/LuthierTests_artefacts/Release/LuthierTests Presets    # a suite, or any name fragment
./build/LuthierTests_artefacts/Release/LuthierTests --list
ctest --test-dir build -C Release -R LuthierRenderCli          # luthier-render smoke test
```

On a headless Linux machine, run the test binary under `xvfb-run -a`. CI
(`.github/workflows/build.yml`, driven by `scripts/ci_build.sh` and
`scripts/ci_build.ps1`) builds on Windows, macOS and Linux, runs the tests and
validates the plugin with pluginval (strictness 5 on push and pull request, 10
nightly) and clap-validator:

```bash
pluginval --strictness-level 10 --validate build/Luthier_artefacts/Release/VST3/Luthier.vst3
```

## Documentation

| | |
|---|---|
| [spec/README.md](spec/README.md) | product overview, architecture, source layout, runtime folders |
| [docs/USER_MANUAL.md](docs/USER_MANUAL.md) | every screen and control |
| [docs/PLAYING_TECHNIQUES.md](docs/PLAYING_TECHNIQUES.md) | how MIDI becomes technique, and the controller map |
| [docs/GUITAR_PHYSICS.md](docs/GUITAR_PHYSICS.md) | the physics behind the model |
| [docs/PRESET_FORMAT.md](docs/PRESET_FORMAT.md) | the `.luthierpreset` file format |
| [docs/MIDI_EXPORT_LUTHIER_PROFILE.md](docs/MIDI_EXPORT_LUTHIER_PROFILE.md) | the Luthier MIDI export profile |
| [docs/KEYBOARD_SHORTCUTS.md](docs/KEYBOARD_SHORTCUTS.md) | keyboard shortcuts |
| [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | install, presets, CPU, crashes |
| [docs/RELEASING.md](docs/RELEASING.md) | the release checklist |
| [docs/CHANGELOG.md](docs/CHANGELOG.md), [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | history and open issues |
| [spec/](spec/) | the specifications the code is built to; `spec/INDEX.md` lists them |
