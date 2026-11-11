# Testing

After configuring a Linux Release tree with `scripts/setup_linux.sh`, build and run the test target:

```bash
ninja -C build -j2 LuthierTests
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests
```

`xvfb-run` supplies a display for GUI tests in headless Linux environments. The runner accepts suite or test filters for focused work; run the complete suite before integration. CMake also registers `LuthierUnitTests` for `ctest` where a display is available.

The CI build wrappers (`scripts/ci_build.sh`, `scripts/ci_build.ps1`) support configure, build, test, validator and staging steps. Plugin validation is distinct from the unit suite: workflows describe pluginval for VST3/AU, clap-validator for CLAP, and auval for AU. Check the current workflow run logs rather than assuming a validation result from the presence of a workflow file.

[TESTING.md](https://github.com/djshellshoxxx/luthier/blob/master/docs/TESTING.md) is the canonical guide (filters, `--list`, `--skip`, sanitizer builds, `LUTHIER_SOAK`, `LUTHIER_PERF`, validators). The state of the tests at the current commit is in [Known Issues](https://github.com/djshellshoxxx/luthier/blob/master/docs/KNOWN_ISSUES.md), "Platform qualification"; the older handoff statements about paused platforms and a spending limit were dated 2026-09-26 and are out of date. Manual host, hardware controller, installer and listening checks are separately described in [QA](https://github.com/djshellshoxxx/luthier/blob/master/spec/qa-polish.md) and [Releasing](https://github.com/djshellshoxxx/luthier/blob/master/docs/RELEASING.md).
