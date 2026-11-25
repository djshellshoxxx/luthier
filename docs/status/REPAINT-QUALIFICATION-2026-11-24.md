# Luthier native repaint qualification — 2026-11-24

The owner's Windows/macOS release work has resumed. The reviewed CQ23 test
correction has local production-source regression evidence and has passed all
four native Linux CQ23 cases. **Full Windows/macOS Free/Pro qualification remains
pending. This checkpoint does not claim a published or qualified beta2 release.**

## Source checkpoint

| Item | Exact source or run |
|---|---|
| Isolated correction branch | `codex/luthier-native-repaint-release-20261124` |
| Reviewed correction | [`953546081b9a614a2281f8a9879e116b4b8e33e1`](https://github.com/djshellshoxxx/luthier/commit/953546081b9a614a2281f8a9879e116b4b8e33e1) |
| Final native Linux verification | [45543111032](https://github.com/djshellshoxxx/luthier/actions/runs/45543111032), all four CQ23 tests / 62 checks passed at `95354608` |
| First corrected native verification | [45541669503](https://github.com/djshellshoxxx/luthier/actions/runs/45541669503), all four CQ23 tests / 62 checks passed at `f158be8a` |
| Earlier diagnostic source | `d44c2d4f21ebb22152657a0fac876297ca57a112` |
| Earlier diagnostic run | [45539982015](https://github.com/djshellshoxxx/luthier/actions/runs/45539982015), compiled successfully and reproduced the immediate-paint assertion failure |
| Existing release PR | [#106](https://github.com/djshellshoxxx/luthier/pull/106), targeting `claude/luthier-fuzz-qa` |
| PR #106 / release candidate before promotion | `c92c0022acd732f38d07984b8310501b2304aea9` |
| Previous native release attempt | [45326905803](https://github.com/djshellshoxxx/luthier/actions/runs/45326905803) |
| `master` at handoff | `0f62c3d67bdba055b41e66c7ab8ac9ab4b728662` |
| Intended release | `v0.1.0-beta2`, Windows/macOS Free and Pro |

The tested `Source/Tests/QualityModeUiTests.cpp` has SHA-256:

```text
9c7026db1696c0ca9f4cf5725e70e2a93ef0ce51aa1fdf419cfa9022044a2a3c
```

The coordinator has removed the temporary diagnostic workflow from the prepared
final tree. Compared with `c92c0022`, the intended release delta is the corrected
test file and this evidence. Re-read remote heads before integration; these are
dated observations, not permission to overwrite concurrent work.

Prior release evidence includes successful compilation and native validator
steps for the four Windows/macOS Free/Pro targets. CQ23 qualification still
blocked release. Earlier-source successes do not qualify this changed test code.

### Completed native baseline diagnosis

[Job 138955861570](https://github.com/djshellshoxxx/luthier/actions/runs/45539982015/job/138955861570)
compiled the actual Linux Free runner at `d44c2d4f` and reported:

- Entering Low: **1** immediate guitar paint.
- Leaving Low: **0** immediate guitar paints, failing the unchanged exact-one
  assertion.
- After 0.2 seconds of normal native message dispatch: **7** paints.

The seven later paints include startup/resumed High animation; they are not
evidence of seven duplicate synchronous requests. This run demonstrates that the
immediate call did not establish delivery. Its diagnostics artifact ID is
`14989697019`.

The first corrected native run `45541669503` passed all four CQ23 tests and all
62 checks: static overlay (4), exact repaint requests (9), native presentation
(13), and the unchanged native idle/performance test (36). Its unedited test log
is [native-f158be8.log](repaint-qualification-2026-11-24/native-f158be8.log).
Independent review then removed an unnecessary High-mode quiescence assumption
from the presentation fixture. High is allowed to animate. The request test is
byte-identical, and the revised native sequence compiled and its nine-check
request regression passed again. Run `45543111032`, job `138966138018`, then
compiled the final sequence and passed all four cases and 62 checks in 24.4
seconds on exact source `953546081b9a614a2281f8a9879e116b4b8e33e1`.
Its unedited test log is
[native-95354608.log](repaint-qualification-2026-11-24/native-95354608.log).
This final changed-source result is separate from the earlier native pass.

## Confirmed JUCE assumption and correction

The old `CQ23_policyTransitionRepaintsGuitarOnce` required exactly one native
`paint()` immediately after `ComponentPeer::performAnyPendingRepaintsNow()`.
Pinned JUCE 8.0.10 does not provide that cross-platform guarantee:

| Backend | Actual source behavior |
|---|---|
| Windows Direct2D | `D2DRenderContext::performAnyPendingRepaintsNow()` is empty in `ThirdParty/JUCE/modules/juce_gui_basics/native/juce_Windowing_windows.cpp`. Painting follows VBlank/swapchain events. |
| Linux/X11 | `juce_Windowing_linux.cpp` returns while an X paint is outstanding; the normal deferred path processes pending X paints first. |
| macOS | `juce_NSViewComponentPeer_mac.mm` calls `displayIfNeeded`, while requested rectangles initially enter `deferredRepaints` and reach display through the VBlank/notification path. |

An immediate zero does not prove that no redraw was requested. One compositor
paint also cannot distinguish one request from two coalesced requests. The
correction tests those two contracts separately.

**Exact synchronous requests:** `CQ23_policyTransitionRepaintsGuitarOnce` uses the
real processor and guitar with a visible test-only parent cache observer. The
guitar retains its normal policy-controlled cache. Scene construction, initial
rendering, and observer installation finish before measurement. It requires two
explicit repaint calls to produce two observations, exactly one request for
High → Low, exactly one for Low → High, and zero when the same policy is repeated.
It also checks the initial Full policy and reduced-motion overlay state.

The observer belongs to the parent because policy changes replace the guitar's
own cache. JUCE's `juce_StandardCachedComponentImage.h` returns `true` from both
invalidation methods, propagating each request to that parent. No native message
dispatch occurs inside the counted transition, and the observer references its
owning host rather than a shorter-lived child.

**Native delivery:** `CQ23_policyTransitionReachesNativeGuitar` uses the real editor
window and ordinary renderer. It requires a peer, a showing guitar, and nonempty
bounds. After all editor policy listeners exist, it records a paint baseline,
switches to Low, and requires a new actual paint of that static state. It then
switches to High and back to Low, each time recording a fresh monotonic baseline,
checking overlay and timer stop/restart state, and dispatching real messages
until a new guitar paint occurs, with a 1.5-second deadline. High need not become
idle: a paint delivered after the synchronous change renders the current state.
It does not manufacture a passing native paint through a snapshot or direct
offscreen drawing.

Those deadlines bound scheduling/liveness; they do not relax the exact-one
request assertion or an existing performance allowance. The existing
`CQ23_lowMeansNoAnimationRepaints` native idle/performance gate is unchanged. The
patch changes no production repaint code, release matrix, validator strictness,
or performance threshold. Reduced-motion state, forced quality overrides, and
the fixture's unique policy source have scoped cleanup. Independent source
review found no blocking issues. The native Linux gate passed; full native
Windows/macOS release qualification remains a separate gate.

## Local regression evidence

The focused runner used the actual processor/editor/guitar, test framework, and
JUCE 8.0.10, with Linux Free, GCC 14.2.0, Release, and LTO disabled. Unrelated test
translation units were omitted; real production and JUCE sources remained. This
is focused local evidence, not a full suite or Windows/macOS qualification.

The integrated product test has **nine checks**. Raw logs also include the earlier
five-check development scaffold
`CpuQualityUiHarness.policyTransitionRequestsExactlyOneGuitarRepaint`. Its
overlapping assertions are preserved but **must not be counted as additional
unique coverage**. The raw two-test/fourteen-check totals are not fourteen
independent product checks.

| Production implementation used | Integrated nine-check result | Raw evidence |
|---|---|---|
| Current implementation | 9 checks pass | [candidate.log](repaint-qualification-2026-11-24/candidate.log) |
| Extra callback repaint restored in temporary source | 2 checks fail: both directions request 2 instead of 1 | [duplicate.log](repaint-qualification-2026-11-24/duplicate.log) |
| Leaving-Low repaint removed in temporary source | 1 check fails: leaving Low requests 0 instead of 1 | [missing.log](repaint-qualification-2026-11-24/missing.log) |
| Original production source restored and runner rebuilt | 9 checks pass again | [restored.log](repaint-qualification-2026-11-24/restored.log) |
| Final native-sequence revision, unchanged request regression | 9 checks pass after recompilation | [final-request-regression.log](repaint-qualification-2026-11-24/final-request-regression.log) |

Both controls copied `Source/UI/GuitarBodyComponent.cpp` into a temporary directory,
kept its quoted includes bound to the original production headers, and replaced
only that translation unit in the focused target. Repository production files
and assertions stayed unchanged. The controls were mutually exclusive.

The duplicate control explicitly initialized the existing `motion` member in the
real constructor, overriding its normal in-class initializer with:

```cpp
motion (*this, AnimationPolicy::Decorative, "GuitarBodyComponent",
        [this] { staticRefresh(); repaint(); },
        [this] { staticRefresh(); })
```

The missing-request control removed only this block from the non-reduced branch
of `GuitarBodyComponent::staticRefresh()`:

```cpp
if (wasReduced)
    repaint();
```

The final restoration rebuilt against the untouched production implementation.
The initial controls used the test file at `f158be8a` (SHA-256
`06dac900d94804eeb1edff308912a6bdef6afb41391b401e393f4de3a882c258`). The final
native-only sequence revision leaves the integrated request test byte-identical;
the whole test file was recompiled and that regression rerun at the digest above.
The logs are copied without editing; their hashes are in
[SHA256SUMS.txt](repaint-qualification-2026-11-24/SHA256SUMS.txt). From that directory,
run `sha256sum --check SHA256SUMS.txt` to verify them.

## Release-path verification and environment limits

Existing release-path fixtures ran at baseline `c92c0022` before this test-only
correction:

| Command | Observed result |
|---|---|
| `python3 -m unittest discover -s scripts/tests -p test_release_workflow_guards.py -v` | 15 passed |
| `python3 -m unittest discover -s scripts/tests -p test_publish_qualified_beta.py -v` | 20 passed |
| `python3 -m unittest discover -s scripts/tests -p test_consolidate_qualified_beta.py -v` | 12 passed |
| `python3 -m unittest discover -s scripts -p test_release_packaging_compat.py -v` | 11 passed; signature fixture could not start `gpg-agent` |
| `bash scripts/test_uninstall_macos.sh` | All 28 simulation checks passed |

Thus **47 release/publication/consolidation guards passed**. Publication tests
used a fake GitHub CLI and temporary assets, not real publication. Uninstaller
checks used synthetic trees, not a native macOS Installer.app session.

GPG is installed, but this environment rejects its local socket with
`can't create socket: Operation not permitted`. The signature fixture failed
before its signature assertions; it was not skipped or weakened and must pass
in normal CI. The same socket restriction prevents local Xvfb/native windows.
The offscreen request proof therefore does not establish native redraw delivery.

## Remaining authorized work

1. Keep the temporary `.github/workflows/repaint-diagnostics.yml` out of the final
   release candidate. Integrate the reviewed correction and this evidence into
   #106, preserve concurrent work, and refresh PR provenance.
2. Advance `release/v0.1.0-beta2` to the reviewed final immutable source and run the
   complete Windows/macOS Free/Pro release matrix: installer coexistence, four
   native builds, strictness-10 validators with LTO, sixteen unit-test shards,
   and packaging. All required jobs must pass.
3. Let the existing guarded workflow verify the complete assets, source/run
   provenance, checksums, uploaded digests, and downloaded bytes before it
   publishes. Its subsequent consolidation step checks unchanged `master` and
   atomically fast-forwards to the same qualified source.

Do not bypass failed gates, reuse older-source success to qualify changed source,
revive retired `pro-beta2-release.yml`, move existing release tags, or replace
published assets. Actual signing/notarization must come from platform logs.
Existing phaser, hardware/DAW/listening, and signing limitations remain as
disclosed in the beta notes. **No premature publication is part of this handoff.**
