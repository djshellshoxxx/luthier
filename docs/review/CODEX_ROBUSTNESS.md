# Codex Q3: robustness, presets, and host state

Branch: `codex/luthier-qa-robustness`  
Base reviewed: `1a0805a679ac1677898bfd37d75fb9a52961058a`  
Reproduction seed: `0xC0DE513` (`202237203` decimal)  
Test: `Source/Tests/CodexQA_Robustness.cpp`

## Coverage added

`CodexRobustness.everyFactoryPresetAtVaryingHostConfigurations` walks every definition in `FactoryPresets::getNumPresets()`, loads it into a processor, and rotates sample rates 44.1/48/96 kHz and block sizes 1/17/257/1024 samples. It feeds seeded MIDI and checks every rendered channel/sample for finite output and a peak below 4.0. For each preset, it saves host state, randomizes parameters, restores it, and compares persistent normalized parameters within 0.0002. Jam transient controls and preset morph position are excluded, matching their preset policy in `Source/Tests/PresetQaTests.cpp`.

`CodexRobustness.seededParameterMidiAndBlockMatrix` traverses 12 rate/block combinations with all parameters at normalized minimum, maximum, or seeded interior values. It sends seeded notes on random MIDI channels and applies the same output checks. The first two iterations use boundary values; later iterations repeat the extremes and randomized interiors.

The 4.0 peak is a gross runaway guard, not a guarantee of suitable output loudness. Three blocks per case catch immediate faults; longer delayed failures need a separate soak test. A crash aborts the test process. The seed and preset index make the case deterministic for a given factory bank; use a debugger or temporary local trace to isolate an abort.

## Existing coverage considered

- `Source/Tests/RobustnessTests.cpp:82-150`: 100,000 random parameter states in the opt-in performance run; fixed 48 kHz/256 sample blocks and partial rendering.
- `Source/Tests/PresetQaTests.cpp:43-101`: factory preset load/save fidelity at one host configuration.
- `Source/Tests/HostStateTests.cpp:28-54`: session restore across prepare at a different rate and block size.

The new matrix combines these dimensions and tests host state after each factory preset.

## QA progress review (2026-09-28)

Static review against integration commit `4e191393deff6f67aa9edda01b63843c6a4963c1`
found that the three-block renderer did not actually finish with a clean release:
block 3 inserted All Notes Off only for channel 1 at sample zero, then also inserted
a new note-on at a random later sample on channels 1-4. The fixture therefore
usually ended with an active voice and did not isolate the release path. The test
now schedules note-ons only in the first two blocks and sends All Notes Off for
all four used channels in the third. This is a test-harness correction; it does
not claim a production defect.

**NOT RUN:** the correction has not been compiled or executed. The integration
commit still has no associated GitHub Actions run, and this connector-only
workspace has no authenticated checkout/JUCE build. Required gate remains the
focused `CodexRobustness` command below.

## Findings, in priority order

1. **Verification pending:** I could not run a Linux compile or test. The GitHub connector can read and write this private repository, but the shell has no authenticated Git checkout (`git ls-remote https://github.com/djshellshoxxx/luthier.git HEAD` returned `could not read Username`). The handoff also records that Actions jobs fail at account spending limit (`docs/HANDOFF.md:37-38`). No preset or parameter combination is therefore claimed to pass or fail. The coordinator should compile and execute this test before merging.
2. **No observed preset-specific defect:** A static inspection cannot establish one. If the test fails, the failure message includes preset name or random iteration, rate, block size, seed, and for state drift the parameter ID; the runner prints test and source line.

## Linux verification command

```sh
scripts/fixeol.sh
ninja -C build -j2 LuthierTests
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests CodexRobustness
```

Reproduce a failure by running the same test on this branch; the fixed seed and case labels identify its inputs. `scripts/fixeol.sh` could not be run against the private repository from this connector-only environment. The two files were created as LF UTF-8 text through GitHub's contents API.

No production or spec files were changed. No merges were performed.
