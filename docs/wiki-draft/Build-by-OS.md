# Build by OS

All commands start in the repository root. `CMakeLists.txt` defines VST3 and Standalone on all platforms, AU on Apple systems, and CLAP only when `clap-juce-extensions` is present. `LuthierTests` and `LuthierRender` are enabled by default and can be disabled with `-DLUTHIER_BUILD_TESTS=OFF -DLUTHIER_BUILD_CLI=OFF`.

## Linux

```bash
scripts/setup_linux.sh
ninja -C build -j2 LuthierTests Luthier_VST3 Luthier_Standalone Luthier_CLAP LuthierRender
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests
```

The setup script requires apt-based dependencies or equivalent manual installation. CLAP target exists after the script has fetched its extension. For packaging, consult `scripts/package_linux.sh` and [Releasing](https://github.com/djshellshoxxx/luthier/blob/master/docs/RELEASING.md). A local edit to the package script in some development worktrees does not imply a released installer.

## Windows

Install Visual Studio 2022 with the C++ toolchain and CMake 3.22+. Fetch JUCE 8.0.10 into `ThirdParty/JUCE`, then use a Developer PowerShell:

```powershell
scripts/build.ps1 -Target All -Jobs 2 -Test
```

Or configure explicitly with `cmake -B build -G "Visual Studio 17 2022" -A x64` and build `Luthier_VST3`, `Luthier_Standalone`, `LuthierTests`, and `LuthierRender`. The PowerShell wrapper defaults to two build jobs for memory constrained machines. Windows build success should be checked on Windows; it is not established by a Linux run.

## macOS

Install Xcode 14+ and CMake 3.22+, fetch JUCE 8.0.10 into `ThirdParty/JUCE`, then:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
```

The Apple configuration includes `Luthier_AU`. Signing and notarization require credentials and manual release checks; see [Releasing](https://github.com/djshellshoxxx/luthier/blob/master/docs/RELEASING.md). macOS validation is currently paused in the project handoff.

For CI-like steps, use `scripts/ci_build.sh` or `scripts/ci_build.ps1` and consult the scripts and workflows for current versions, validators, staging and package outputs. Built plugin artifacts and signed public releases are different states.
