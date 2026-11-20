# Beta 2 qualification execution — November 19, 2026

Checkpoint: November 19, 2026, 8:07 p.m. Pacific (2026-11-20 04:07 UTC).

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
| Named-phaser state/audio round trip | **Pass** | 2 supplemental tests, 5,420 checks; 40 rate/block/clock-mode combinations; bit-identical fresh restored audio and zero observed callback allocations |
| Extended phaser render | **Pass** | Original soak template selected for the phaser: 1 test, 8 checks; 1,800 seconds at each of 44.1/48/96/192 kHz, 684,180,000 samples per channel; 101.7 seconds wall time |
| Original concurrent generic-board soak | **Pass** | 1,800.14 wall-clock seconds; 180,000 attempted edits; 9 checks, zero failures |
| Concurrent phaser-board soak | **Pass** | 1,800.14 wall-clock seconds; 180,000 attempted edits; 9 checks, zero failures; eleven block sizes and parameter edits |
| Phaser CPU below 0.4% of one core | **Fail on this machine** | Ten existing-probe trials: best 0.483628860%, maximum 1.468951710%; none below 0.4% |
| Bypass “peak delta <1 LSB at 24 bits” | **Blocked: measurement definition** | No replacement metric or inferred pass |
| Physical reference listening | **Not run** | Requires reference hardware and a listening comparison |

Local execution is on Linux x86_64. It supplements the Windows/macOS matrix;
it does not establish native host, device-driver, installer, or signature status.
Allocation observations cover ordinary unaligned `new`/`new[]` and nothrow forms,
not aligned allocation or direct `malloc`/`calloc`/`realloc`. Lock observations
cover ThreadProbe-instrumented blocking locks only.

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

The general suite uses fixture pedals for its seeded round trips. The separate
named-phaser check closes that model-specific gap: two real phasers with distinct
nondefault parameters, trims and reversed order are saved and restored across
sample-rate changes. Fresh direct-state and JSON-restored boards produced
bit-identical audio at 44.1/48/88.2/96/192 kHz and blocks of 1/37/64/512 samples,
both free-running and host-synced. A changed host tempo affected synced output
and left free-running output unchanged. An additional fresh control preserves
slot trims, routing, all other parameters and host timing while setting only
both phasers' wet-trim parameters to zero. Every render cell exceeded the
`1e-6` output-difference guard (observed peaks 0.0435626–0.0440371), establishing
digital model-parameter response without making a listening claim. This is board-state
evidence, not a full-processor or DAW project-reopen claim. The log label `free`
means free-running clock, not the product's Free edition.

### Completed 30-minute concurrent soaks

Both processes completed successfully using the same component executable,
SHA-256 `ddf5b51b79810de0184244ebc7bdee5e139817b68d5090704381ccb79e304c27`.
Each ran for 1,800 wall-clock seconds, with a separate audio-processing thread
and 100 attempted edits per second. Audio runs as fast as possible in this
harness; it is not paced by an audio-device callback or a DAW.

| Observation | Original generic-board test | Supplemental phaser-board test |
|---|---:|---:|
| Attempted edits | 180,000 | 180,000 |
| Recorded rejected edits | 18,439 | 16,129 |
| Processed blocks | 5,657,526 | 7,729,469 |
| Samples per channel | 2,495,911,887 | 3,211,945,709 |
| Nonfinite or absolute magnitude above 16 detections | 0 | 0 |
| Counted ordinary C++ callback allocations | 0 | 0 |
| ThreadProbe-instrumented callback blocking locks | 0 | 0 |
| Maximum sampled resident memory | 25,228 KiB | 14,100 KiB |
| Test checks / failures | 9 / 0 | 9 / 0 |

The harness records rejected random edit attempts; it does not require every
attempt to be accepted. Memory observations are diagnostics, not a new memory
acceptance threshold or proof of no leaks. These results establish component
concurrency and numerical evidence for this candidate. Hardware callback
deadlines, DAW interaction, audio-device dropouts and listening remain untested.

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
selection have passed. macOS Pro and Windows Free builds are running; macOS Free
and Windows Pro are queued. Five
supplemental Linux jobs passed: looper publication under ThreadSanitizer, IR
handoff under ThreadSanitizer, IR handoff under AddressSanitizer/UndefinedBehaviorSanitizer,
recorder thread safety, and native-library portability. Both application
preflights also passed: 199 tests/560,107 checks in Pro and 199 tests/560,091
checks in Free, zero failures. All seven jobs in
[run 44662141836](https://github.com/djshellshoxxx/luthier/actions/runs/44662141836)
have passed. Those pull-request jobs checked out synthetic merge commit
`527085a20570e69626c5e0aa88d624906fa4294a`; its complete source tree is identical
to the candidate's `fae7e7ab0f70dfa533d2857f1df904f89a20cf5d` tree.

No `v0.1.0-beta2` release exists at this checkpoint. The candidate branch remains
at the source above; `master` remains `0f62c3d67bdba055b41e66c7ab8ac9ab4b728662`.
Full native unit shards, validators, packaging and publication verification
remain pending. The completed component tests do not bypass those gates.

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
