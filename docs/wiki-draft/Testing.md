# Testing

After configuring a Linux Release tree with `scripts/setup_linux.sh`, build and run the test target:

```bash
ninja -C build -j2 LuthierTests
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests
```

`xvfb-run` supplies a display for GUI tests in headless Linux environments. The runner accepts suite or test filters for focused work; run the complete suite before integration. CMake also registers `LuthierUnitTests` for `ctest` where a display is available.

The CI build wrappers (`scripts/ci_build.sh`, `scripts/ci_build.ps1`) support configure, build, test, validator and staging steps. Plugin validation is distinct from the unit suite: workflows describe pluginval for VST3/AU, clap-validator for CLAP, and auval for AU. Check the current workflow run logs rather than assuming a validation result from the presence of a workflow file.

The [handoff](https://github.com/djshellshoxxx/luthier/blob/master/docs/HANDOFF.md) reports Linux integration green as of 2026-09-26, says Windows and macOS builds are paused, and records a GitHub Actions spending limit blocking jobs at that time. These are dated observations, not guarantees about the latest commit. Manual host, hardware controller, installer and listening checks are separately described in [QA](https://github.com/djshellshoxxx/luthier/blob/master/spec/qa-polish.md) and [Releasing](https://github.com/djshellshoxxx/luthier/blob/master/docs/RELEASING.md).
