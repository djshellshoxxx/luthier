# Getting Started

## Prerequisites

Clone the repository and install CMake 3.22+, a C++17 compiler, and the platform audio/UI development dependencies. JUCE 8.0.10 is required in `ThirdParty/JUCE`; the build does not vendor it. CLAP additionally requires `ThirdParty/clap-juce-extensions`. Linux users can use the setup script below to fetch these and configure the build. A host capable of loading VST3 (or CLAP when enabled) is needed for plugin use; the standalone target needs no DAW.

## Fast path on Linux

From the repository root:

```bash
scripts/setup_linux.sh
ninja -C build -j2 LuthierTests Luthier_VST3 Luthier_Standalone
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests
```

The script installs dependencies through `apt-get` when available, fetches pinned JUCE and CLAP extensions, and configures a Release Ninja tree with Clang. Package managers outside apt-based distributions require manual dependencies. Adjust `-j2` to available memory.

## First sound

Launch the standalone application from the build artifacts, or scan the built `Luthier.vst3` in a DAW. Select a factory preset, send MIDI notes, and check the instrument's master volume and amplifier standby setting. See the [user manual](https://github.com/djshellshoxxx/luthier/blob/master/docs/USER_MANUAL.md) for instrument controls and [troubleshooting](https://github.com/djshellshoxxx/luthier/blob/master/docs/TROUBLESHOOTING.md) for silence, loading and crackles.

For batch rendering, build `LuthierRender` and run:

```bash
luthier-render --midi riff.mid --preset "Modern Metal Chug" --out riff.wav
```

The CLI also accepts a `.luthierpreset` file. See `Tools/RenderCli.cpp` for its actual options.

Continue with [[Build by OS]] and [[Testing]].
