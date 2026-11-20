# Beta 2 qualification execution — November 19, 2026

## Candidate and decision

Source: `51fd4de677fd0d4a056ded5ee2e8845b9e81584d`. Scope: Windows x64 and
universal macOS, Free and Pro, standalone and VST3, plus Audio Unit on macOS.
The Four-Stage Phaser is the only newly addable reference pedal; existing
sessions retain their compatibility models.

The [native release workflow](https://github.com/djshellshoxxx/luthier/actions/runs/44662100996)
must pass before publication. Authorized publication and guarded default-branch
consolidation remain enabled. Supplemental evidence does not turn a pending or
failed release workflow into a pass.

## Supplemental execution

| Check | Result | Evidence and scope |
|---|---|---|
| Reference-board state persistence | **Pass** | 23 existing tests, 9,533 checks, zero failures; includes 1,000 seeded save/restore round trips |
| Phaser correctness and prepared memory | **Pass** | 17 existing tests, 55,056 checks, zero failures |
| Named-phaser state/audio round trip | Running | Supplemental actual-model test; does not establish DAW project reopen |
| Extended phaser render | **Pass** | Original soak template selected for the phaser: 1 test, 8 checks; 1,800 seconds at each of 44.1/48/96/192 kHz, 684,180,000 samples per channel; 101.7 seconds wall time |
| Original concurrent generic-board soak | Running | Existing 1,800-second wall-clock test; unchanged checks |
| Concurrent phaser-board soak | Running | Supplemental test derived from existing soak; eleven block sizes and parameter edits |
| Phaser CPU below 0.4% of one core | **Fail on this machine** | Ten existing-probe trials: best 0.483628860%, maximum 1.468951710%; none below 0.4% |
| Bypass “peak delta <1 LSB at 24 bits” | **Blocked: measurement definition** | No replacement metric or inferred pass |
| Physical reference listening | **Not run** | Requires reference hardware and a listening comparison |

Local execution is on Linux x86_64. It supplements the Windows/macOS matrix;
it does not establish native host, device-driver, installer, or signature status.

### State persistence

The existing `RefPedalState` suite ran against candidate implementations of the
board, model, registry, MIDI state, asset store and state codec. It includes the
production generic models, registered phaser and compatibility prototypes. Pinned
JUCE 8.0.10 parser patches were applied and verified in an isolated copy before
rebuilding `juce_core`, `juce_audio_basics` and `juce_cryptography`.

Coverage includes deterministic state, future schemas, unknown and missing
models, eight snapshots, concurrent saves, asset persistence, descriptor
defaults, numeric conversion and locale independence, and momentary controls.
Fetched production and existing-test files were checked against their Git blob
identities. The component runner and extracted allocation hooks are supplemental
test infrastructure, not the shipping executable.

### CPU measurement

The unchanged `docs/coverage/probes/phase90-e58-actual.cpp` probe ran with GCC
14.2.0, `-O3`, and no fast-math or architecture-specific instruction flags on an
AMD EPYC 9V74 host. It measures rendering-thread CPU time for ten seconds of
48 kHz stereo audio in 128-sample blocks, five trials each at zero and 180 degrees
stereo phase. The probe prints measurements and exits zero regardless of target;
its exit code is not a CPU acceptance result. The strict `<0.4%` target remains.

An isolated bounded phase-wrap replacement passed 10,592,070 bitwise comparisons
but still missed the CPU target: best 0.431147%, none of ten trials below 0.4%.
Paired median improvement was about 4.30%, with shared-host outliers. This trial
was not applied to the release candidate and does not resolve the gate.

## Native release and artifact qualification

Release-source checks, Windows edition-coexistence fixtures and platform
selection have passed. The macOS Pro build is running; the other three native
build jobs are queued. Five
supplemental Linux jobs passed: looper publication under ThreadSanitizer, IR
handoff under ThreadSanitizer, IR handoff under AddressSanitizer/UndefinedBehaviorSanitizer,
recorder thread safety, and native-library portability. Free and Pro focused
runners are building in [run 44662141836](https://github.com/djshellshoxxx/luthier/actions/runs/44662141836).

The release requires all four builds, every native test shard, strictness-10
VST3 validation, AU validation where applicable, and packaging. Publication then
checks the frozen source/tag, complete asset set, provenance, uploaded metadata,
and downloaded hashes. Actual platform logs must establish signing/notarization.
The Pro universal ZIP uses the ad-hoc-signed staging bundles; the Developer-ID
signing/notarization path, when configured, applies to installer payload copies.
Do not describe that ZIP as Developer-ID signed or notarized.

## Hands-on acceptance

| Target | Editions and formats | Result |
|---|---|---|
| Windows x64 | Free/Pro; standalone and VST3; FL Studio and a second VST3 host | Not run in this workspace |
| macOS Apple Silicon | Free/Pro; standalone, AU and VST3 | Not run in this workspace |
| macOS Intel | Free/Pro; standalone, AU and VST3 | Not run in this workspace |

Record the actual asset filename/SHA-256, source commit, OS/architecture, host
version/format, interface/driver, rate and buffer in `docs/beta/HOST_TEST_SHEET.md`.
Check discovery and first sound, MIDI release, controls/bypass, automation,
save/quit/reopen, offline render, device selection, edition coexistence, and
removal with user data preservation. Exercise controls during a live session.
Universal compilation does not prove execution on both architectures. A
validator pass does not fill an unobserved DAW or listening result.
