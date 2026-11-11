# Luthier

Luthier is a C++17/JUCE physically modelled guitar instrument. It generates string, body, pickup, amplifier and room sound in real time, without sampled notes. The repository contains the engine, plugin and standalone targets, an offline renderer, tests, factory resources, packaging scripts, and a large design specification.

**Project status:** active development, not release-qualified. Linux builds and its test suite runs; no native macOS or Windows release qualification exists for the current source. Read [Known Issues](https://github.com/djshellshoxxx/luthier/blob/master/docs/KNOWN_ISSUES.md), especially "Platform qualification", before relying on any build. Specs describe intended behaviour and are not a list of shipped features.

These pages are drafts for the GitHub wiki. They are short and link to the canonical file in `docs/` or `spec/` instead of repeating it. Links point at `master`; use your branch's files when working elsewhere.

## Users

- [[Installation]]: what to install and where
- [[Getting Started]]: first sound
- [[Pedalboard]]: the reference pedalboard
- [[Effects and Controls]]: every effect control
- [[Guitar Workshop]]: building and editing instruments
- [[Presets]]: factory and user presets
- [[Plugin Installation Paths]]: folders for each format and system
- [[DAW Compatibility]]: what has and has not been tested
- [[Troubleshooting]]: when something is wrong
- [[Features]]: product areas and specification boundaries

## Developers

- [[Architecture]]: audio path and source map
- [[DSP Processing]]: the engine and its rules
- [[Real-Time Rules]]: what the audio thread may not do
- [[Pedal Implementations]]: legacy racks and board models
- [[Parameter Management]]: host parameters and the append-only rule
- [[GUI Architecture]]: interface structure
- [[Plugin Formats]]: VST3, AU, CLAP, Standalone
- [[Build System]]: building on each platform
- [[Testing]]: running the suite and validators
- [[Benchmarking]]: CPU measurements
- [[Release Procedure]]: CI, packaging and release hazards
- [[Contributing and Status]]: how to contribute
