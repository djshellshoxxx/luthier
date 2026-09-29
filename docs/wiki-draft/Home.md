# Luthier

Luthier is a C++17/JUCE physically modelled guitar instrument. It generates string, body, pickup, amplifier and room sound in real time, without sampled notes. The repository contains the engine, plugin and standalone targets, an offline renderer, tests, factory resources, packaging scripts, and a large design specification.

**Project status:** active development. Linux is the documented integration and test platform. Windows and macOS targets and packaging scripts exist, but the [project handoff](https://github.com/djshellshoxxx/luthier/blob/master/docs/HANDOFF.md) says cross platform validation is paused. Specs describe intended behaviour and should not be read as a list of shipped features. Check [Known Issues](https://github.com/djshellshoxxx/luthier/blob/master/docs/KNOWN_ISSUES.md) and the current branch before making release claims.

## Wiki pages

- [[Getting Started]] — prerequisites, first build and first sound
- [[Build by OS]] — platform commands and target availability
- [[Architecture]] — audio path and source map
- [[Features]] — implemented product areas and specification boundaries
- [[Testing]] — local checks and validation
- [[Contributing and Status]] — source of truth, workflow and current caveats

The detailed [user manual](https://github.com/djshellshoxxx/luthier/blob/master/docs/USER_MANUAL.md), [specification index](https://github.com/djshellshoxxx/luthier/blob/master/spec/INDEX.md), and [release guide](https://github.com/djshellshoxxx/luthier/blob/master/docs/RELEASING.md) provide deeper information. These links target `master`; use your branch's corresponding files when working on another branch.
