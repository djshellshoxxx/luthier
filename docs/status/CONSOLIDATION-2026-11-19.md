# Luthier consolidation and release qualification — 2026-11-19

The owner resumed consolidation, incomplete-feature work and release publication.
The existing beta scope remains Windows/macOS Free and Pro, VST3 and Standalone,
plus AU on macOS, with one newly addable reference pedal (the Four-Stage Phaser).
Linux and CLAP are development targets. Deferred pedal models are retained for
future work; existing generic/prototype sessions remain loadable.

## Source and branch evidence

The latest release-tested source at discovery was
`7fcb88c601288e7a7a02cf64548a822b9c3fcfc4` (PR #105).
Its ancestry contains PR #104 at `35fe3a2e`, integration PR #89 at `dbd36089`,
and the older consolidated pedalboard work. The default branch was still
`master` at `0f62c3d6`.

Every remote head was inventoried through GitHub's matching-refs API and compared
against that exact release-tested source. Of **195 branches**, **154 were already
contained or equal** and **41 had unmatched commits**. The complete machine-readable
results are in [BRANCH_ANCESTRY_2026-11-19.json](BRANCH_ANCESTRY_2026-11-19.json).
These are ancestry counts, not claims that all 41 branches contain useful new code.

A further branch absent from the open-PR list was found:
`claude/luthier-fuzz-qa` at
`c26e1985c199ad517fcd4259abab57b3f97eeb23`. It is **eight commits ahead and none
behind** the release-tested source, with 59 changed files. Its work includes a
libFuzzer harness, malformed-input regression fixtures, strict external-text
decoding, bounded imported-score arithmetic, and seven configure-time JUCE parser
patches. At discovery it had **zero Actions runs, zero check runs and no commit
statuses**. Its commit messages record rebuilds as pending. This is valuable
unique work requiring review and fresh qualification; the older release run does
not qualify it.

[Exact fuzz branch comparison](https://github.com/djshellshoxxx/luthier/compare/7fcb88c601288e7a7a02cf64548a822b9c3fcfc4...c26e1985c199ad517fcd4259abab57b3f97eeb23).

### Decisions for open branches

| Branch or PR | Current evidence | Disposition |
|---|---|---|
| #73, #77, #79, #80, #82, #83, #85 | Their head commits are ancestors of #105 | No source recovery needed; consolidate through the latest chain |
| #89, #104, #105 | Stacked current release work | Preserve their implementation and evidence in the final integration |
| #99 MIDI continuity | Its entire regression is present in #105, with the missing TestContext argument repaired | Semantically superseded; do not restore the unbuildable version |
| #97 recorder | Deterministic native SessionRecorderRate cases and concurrency fixes supersede the unbuilt proposal | Preserve the newer implementation |
| #98 rack lifetime | Native EffectsChainLifetime cases and the production-source sanitizer harness implement the retirement rule | Keep historical whole-processor stress proposal separate from the verified bounded repair |
| #100 chorus curvature | #104 contains the qualified BBD response repair and unchanged THD gates | Do not replace it with the uncompiled curvature proposal |
| #90 small ports | Backend note-release retry is already present; DD3T meter correction remained absent | Recover only the missing correction and its regression |
| #91 / RC-5 tap branch | Unique changes extend a frozen interface and target a deferred model | Preserve for separate model qualification; do not activate it in this beta |
| #92–94 GUI work | Tests/screenshots and drafts, not completed product features | Preserve as unfinished work; no completion claim |
| #95 processor stress | An unbuilt harness with placeholder thresholds | Keep separate until compiled, calibrated and reviewed |
| #96 CI speed | Broad workflow proposal; current #105 already provides successful native test sharding | Do not replace the qualified workflow wholesale |
| #76 / #84 / #86 SoundGrind | Repeat-runner work, with #84 repairing the original false-green issue | Preserve the newest runner branch for independent qualification |
| #78 documentation | Older README/wiki with stale counts and scope | Reuse verified information; do not overwrite current release documentation |
| #81 native diagnostics | Code fix previously incorporated; remaining differences include generated Python bytecode | Do not merge the whole stale branch |
| #72 trial lock, #87 autonomous-agent workflow | Licensing policy and external API-spend behavior, independent of beta repairs | Do not introduce these product/operational changes as part of release consolidation |

## Four recovered bounded changes

Each slice was compared against the actual current production file before porting.
No whole stale branch or model implementation was imported.

| Original commit | Change | Verification |
|---|---|---|
| `8dc4ca0719cfdafbc70b0b63a7b769b9ad3bc3ce` | Replace per-sample LFO floor wrapping with one bounded subtraction | Independent original-algorithm oracle: 1,342,784 bitwise phase/sine/triangle checks |
| `5be9c0bc265770675c8d9cf90bd85c387be26ce5` | Reject tap tempo while an external host/MIDI master governs the clock; discard ignored tap history | Native ClockSync regression across Internal/Host/MIDI/Auto and fallback |
| `8419a70520bd6aa8d551f251e29e7bb43c53666f` | Reserve feedback capacity for clock status and retry notices after a hidden UI's queue fills | Two native MIDI-router feedback regressions, including the no-allocation assertion |
| `ee3d7c0d5ef90d106a4e565eebefd22e314afd5b` | Flush only the DD3T meter accumulator below its public floor | Five-rate, 40-second silent-tail regression with host FTZ/DAZ disabled, plus the existing DD3T suite |

Focused local native verification uses GCC C++17 and the exact official JUCE
8.0.10 core/audio-basics sources. Production MIDI routing, map lookup and DD3T
rendering are compiled unchanged apart from the proposed fixes. The new clock
test body is isolated from the larger test translation unit for the focused
binary. The normal native test files retain it for full product CI.

Result: **19 tests, 1,346,119 checks, zero failures**, including all 15 DD3T tests.
With the three behavioral fixes removed and the same regression bodies retained,
the actual baseline production sources report **12 failures across four cases**:
hidden-UI tempo/overload feedback (3), hidden-UI clock loss (1), DD3T underflow (2),
and tap policy/history (6). The other existing DD3T cases continue to pass.
The LFO change is an arithmetic-equivalent optimization, so it is checked against
the independent original floor algorithm rather than expected to fail beforehand.
This is focused Linux evidence, not a full application build or Windows/macOS
qualification. The allocation observer covers C++ new as the existing tests do;
it is not a claim to intercept every raw allocator.

## Additional integrated fixes and focused qualification

The final candidate is based on `c26e1985`, preserving the complete current release
chain and the newer QA branch. The four recovered changes above are combined with:

| Fix | Evidence |
|---|---|
| Construct shorter IR convolution banks only when they are useful; recreate banks when preparation changes latency | 2 native lifecycle tests, 33 checks, zero failures; the original implementation fails 6 checks |
| Reserve the last MIDI output slots for footswitch releases and record ownership only for admitted presses | 2 native regressions, 1,105 checks, zero failures on the combined router; original implementation fails 66 assertions |
| Bound content-package input, manifest/signature bytes, per-entry bytes and aggregate expansion; verify exact decoded sizes | 10 native cases, 98 checks, zero failures, including ordinary signed installation and rollback |

Independent review checked the combined MIDI changes and the lazy IR lifetime
boundary. It also corrected the content reader's extra-byte check after tracing
JUCE's stream-length clamping; the corrected package suite was rerun successfully.
All focused binaries use actual JUCE 8.0.10 sources, not substitute containers.

The native IR measurement found two loader threads created by an unused variant
bank before the change and none afterward. In this focused Linux run, 30 short-IR
load/destruction cycles took about 2,029 ms before and 1,341 ms afterward. This is
a local directional measurement at the same compiler settings, not a Windows/macOS
startup claim. Existing platform timing limits remain unchanged.

The seven pinned JUCE parser patch operations were applied to isolated copies of
the four actual 8.0.10 source files, then reapplied and checked. Reapplication was
byte-identical and check mode passed. The full candidate still needs native CI;
patch idempotence does not substitute for the complete application tests.

Release publication has 20 subprocess/fake-GitHub regression cases; default-branch
consolidation has 12 guard cases. The existing 14 release-source guard cases also
pass. These cover exact source/asset identity, checksums and downloaded bytes,
stale candidates, failed qualification, API failures and concurrent master changes.
No timing limit or failing native shard is waived.

## Exact latest completed native release attempt

[Run 44259292225](https://github.com/djshellshoxxx/luthier/actions/runs/44259292225)
tested `7fcb88c601288e7a7a02cf64548a822b9c3fcfc4` with LTO, strictness 10 and four
test shards per edition/platform.

All four Windows/macOS Free/Pro build, validator and packaging jobs passed.
Windows edition-coexistence checks passed. Twelve of sixteen test shards passed;
macOS Free passed all four. Each failing shard reported one performance failure:

| Platform/edition | Failing gate | Observed | Existing limit | Job |
|---|---|---:|---:|---|
| Windows Free | Workshop part swaps, p95 | 54.4 ms | 50 ms | [134709022076](https://github.com/djshellshoxxx/luthier/actions/runs/44259292225/job/134709022076) |
| Windows Pro | Workshop part swaps, p95 | 57.0 ms | 50 ms | [134709022155](https://github.com/djshellshoxxx/luthier/actions/runs/44259292225/job/134709022155) |
| macOS Pro | Warm instantiation | 1287.5 ms | 1000 ms | [134709022118](https://github.com/djshellshoxxx/luthier/actions/runs/44259292225/job/134709022118) |
| macOS Pro | Guitar load, p95 | 1414.3 ms | 1200 ms | [134709022142](https://github.com/djshellshoxxx/luthier/actions/runs/44259292225/job/134709022142) |

The release job was skipped because the native matrix failed. No gate is relaxed,
no failed shard is ignored, and the earlier passes do not qualify later changes.

## Publication and remaining work

### Latest exact-source qualification

[Run 44662100996](https://github.com/djshellshoxxx/luthier/actions/runs/44662100996)
tested `51fd4de677fd0d4a056ded5ee2e8845b9e81584d`. Release preflight, Windows
edition coexistence, all four Windows/macOS Free/Pro build-validator-package
jobs, and fifteen of sixteen native test shards passed. The only product failure
was macOS Pro shard 3/4: `Boot.coldAndWarmInstantiationStayInBudget` measured
1,033.4 ms warm against its unchanged 1,000 ms limit. Its cold measurement was
678.7 ms. A narrow job rerun could not reproduce the test because the archived
build artifact had expired; it failed during artifact download before executing
the test binary.

The remediation following `51fd4de` removes the redundant Workshop-guitar DSP
build from processor construction. Construction still loads the selected parts
file and establishes parameter defaults. The selected engine structure is built
once, after the host supplies its pre-prepare parameter writes and the engine
knows its sample rate. Unprepared save and restore paths explicitly materialize
the same engine state before capturing or applying per-string state. A focused
regression checks both editions: a pre-prepare guitar selection remains deferred,
then the selected Workshop guitar exists after first prepare. Pro and Free
production and regression translation units pass C++17 syntax compilation with
JUCE 8.0.10. All 46 publication, release-source, and guarded-consolidation tests
also pass. No performance threshold, assertion, validator strictness, or release
matrix requirement changed. Complete Windows/macOS qualification remains required
for this new exact source.

- The current release chain retires the old PR-#77-triggered beta publisher.
  Consolidating the latest chain includes this repair; the older integration
  snapshot alone does not.
- `release.yml` freezes one commit and creates a new source tag and complete draft
  only after all selected tests, validators and packaging succeed. The owner's
  authorized beta2 candidate then verifies all uploaded and downloaded assets and
  publishes that exact prerelease. Other invocations retain draft behavior.
  Existing tags and release assets are never moved or overwritten.
- After verified beta2 publication, a separate job fast-forwards `master` to the
  same qualified source only if it still matches the original observed
  `0f62c3d67bdba055b41e66c7ab8ac9ab4b728662`. It verifies ancestry and atomically
  checks the prior head without forcing the update or changing branch protection.
  Concurrent work stops consolidation for review without withdrawing the beta.
- The remediation successor of `51fd4de` needs a new complete native release run.
- Phaser CPU/reference-listening/bypass-criterion questions and real DAW/hardware
  acceptance remain as described in the beta notes. They are not implied by the
  focused tests above.
- Signing and notarisation must be established from the actual platform logs.
  No signing claim follows from a successful build.
- Historical November 11 documents are retained as dated records and linked here.
  They should not be used as the current release status.

