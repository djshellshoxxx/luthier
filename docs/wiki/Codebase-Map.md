# Codebase Map: where to find what

| I want to... | Look in |
|---|---|
| Plugin entry points, host-facing parameters | `Source/PluginProcessor.*`, `Source/PluginEditor.*`, `Source/Parameters.*` |
| Engine orchestration | `Source/LuthierEngine.*` |
| Parameter ranges and locks | `Source/PhysicalRange.*`, `spec/advanced-ranges.md` |
| MIDI interpretation and modes | `Source/Model/Playing/`, `Source/Controllers/` |
| Technique detection and triggering | `Source/DSP/Techniques/`, `Source/Model/Playing/` |
| Tuning, temperaments | `Source/Model/Playing/` (tuning), `Source/Tune/` |
| String physics | `Source/DSP/String/` (`StringEngine`, `Excitation`, `FractionalDelayLine.h`) |
| Sympathetic coupling | `Source/DSP/Coupling/` |
| Body resonance | `Source/DSP/Body/` |
| Pickups and guitar circuit | `Source/DSP/Pickup/`, `Source/DSP/Circuit/` |
| Pedals and racks | `Source/DSP/Effects/` |
| Amp, cabinet, room | `Source/DSP/Amp/` |
| Instrument, string and body models | `Source/Model/Guitar/`, `Source/Model/Workshop/` |
| Presets: format, morphing, factory bank | `Source/Presets/` |
| Shared DSP helpers (filters, smoothing, oversampling) | `Source/DSP/Common/` |
| MIDI learn, export, diagnostics, error log | `Source/Support/` |
| Offline renderer | `Tools/RenderCli.cpp` |
| Icon, IR and factory-content generators | `scripts/`, `Tools/generate_factory_*.py` |
| Packaging and CI | `scripts/package_*.sh`, `scripts/ci_build.*`, `.github/workflows/` |
| Tests | `Source/Tests/`, fixtures in `Source/Tests/Fixtures`, golden data in `Source/Tests/Golden` |
| Specs and decisions | `spec/` (index: `spec/INDEX.md`), `spec/DECISIONS.md` |
| Editions | `cmake/Editions.cmake`, `spec/editions.md` |
| UI | `Source/UI/` (guitar illustration, fretboard, faces, preset browser) |
