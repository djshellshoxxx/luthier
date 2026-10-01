# Cross-platform status (Windows / macOS / Linux)

Branch: `claude/luthier-crossplatform`. Baseline: `codex/luthier-beta`.

This tracks the Windows (MSVC) and macOS (AppleClang) bring-up. The project was
developed and tested on Linux; before this branch neither Windows nor macOS had
a clean CI pass. Everything below was driven through the real GitHub Actions
`build.yml` jobs (the container this was done from is Linux, so Windows/macOS
were verified on CI, not locally).

## Summary

| Platform | Configure | Compile | Link | Unit tests | pluginval / clap-validator |
|----------|-----------|---------|------|------------|----------------------------|
| Linux (gcc/clang, libstdc++) | ✅ | ✅ | ✅ | ✅ (baseline) | ✅ (baseline) |
| Windows x64 (MSVC 14.5x) | ✅ | ✅ | ✅ | ❌ 2 determinism tests (see below) | not reached |
| macOS universal (AppleClang, arm64+x86_64) | ✅ | ✅ | ✅ | pending / see below | not reached |

"✅" for Windows/macOS compile+link means a full `Configure and build` step
went green on CI (all ~1070–1090 translation units + all targets: VST3,
Standalone, CLAP, LuthierTests, LuthierRender, and Luthier_AU on macOS).

## Portability fixes made

All fixes are minimal and behaviour-preserving on Linux (verified: the Linux
build is unchanged and still compiles clean).

### 1. Windows — `juce::jmin` ambiguous template argument
`Source/Notation/AsciiTabReader.cpp:527` — `juce::jmin (100000, value*10 + (s[i] - '0'))`.
`juce::String::operator[]` returns `juce_wchar`, which is a 32-bit `wchar_t` on
Linux/macOS but `uint32` (unsigned) on Windows, because Windows `wchar_t` is a
16-bit UTF-16 unit and JUCE therefore defines `juce_wchar` as `uint32` there.
So `s[i] - '0'` was `unsigned int` on Windows and `jmin`'s two arguments
(`int`, `unsigned int`) could not deduce a single `Type`. Fixed by casting the
character to `int`; a no-op where `juce_wchar` is already a 32-bit signed type.

### 2. macOS — `juce::jmax<size_t>` instantiates an undefined SIMD type
`Source/Tests/DocsSweepTests.cpp:1385` and `Source/Tests/JamTests.cpp:921` called
`juce::jmax<size_t> (...)`. The explicit `<size_t>` makes the `juce_dsp`
`SIMDRegister` overload of `jmax` a candidate with `T = size_t`, which
instantiates `juce::dsp::SIMDNativeOps<size_t>`. On macOS `size_t` is
`unsigned long`, a distinct type from `uint64_t` (`unsigned long long`), and JUCE
only specialises `SIMDNativeOps` for the fixed-width integer types — so
`SIMDNativeOps<unsigned long>` is undefined and the instantiation is a hard
error. On Linux (`size_t` == `unsigned long` == `uint64_t`) and Windows
(`size_t` == `unsigned long long` == `uint64_t`) the specialisation exists, so
both compiled. Fixed by dropping the explicit template argument and casting the
`int` literal so the call deduces `jmax(size_t, size_t)`: the scalar overload is
chosen and the SIMD overload is never instantiated (a scalar is not deducible as
`SIMDRegister<T>`). The many *deduced*-`size_t` `jmin`/`jmax`/`jlimit` call sites
elsewhere are fine — the SIMD overload is only a candidate when a `SIMDRegister`
argument or an explicit `<...>` forces it.

### 3. macOS — `LuthierTests` link error, undefined `dispatchNextMessageOnSystemQueue`
`Source/Tests/QualityModeUiTests.cpp` forward-declared and called
`juce::detail::dispatchNextMessageOnSystemQueue`, which JUCE defines only in its
Linux and Windows messaging back-ends (`juce_Messaging_linux.cpp` /
`juce_Messaging_windows.cpp`). On macOS that free function does not exist — the
platform pumps its queue through CFRunLoop — so the universal `LuthierTests`
binary failed to link with "Undefined symbols … dispatchNextMessageOnSystemQueue".
Fixed by draining the system queue on macOS with
`CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true)` (loop while it returns
`kCFRunLoopRunHandledSource`), exactly how JUCE's own macOS `runDispatchLoopUntil`
advances the loop. Linux and Windows keep the existing call unchanged.

### CI aid — ninja keep-going
`scripts/ci_build.sh` and `scripts/ci_build.ps1` now pass `-k 0` to the ninja
backend so a single failing build reports **every** compiler error instead of
stopping at the first. Successful builds are unaffected. This let each CI run
surface a whole platform's error set at once during bring-up.

## Known remaining issue — determinism unit tests off Linux

Windows (and, pending confirmation, macOS) fail two tests in
`Source/Tests/CombinationTests.cpp`:

- `Combo.renderIsDeterministicAfterReset` — renders the same phrase twice in the
  same process (`releaseResources → prepareToPlay → reset → render`) and requires
  the two passes to match within `1e-4`. Observed max sample differences on
  Windows: mostly `1e-4`–`4e-3`, with one outlier of `0.105` ("Auto-Vibrato
  Hold").
- `Combo.sessionStateRoundTripReproducesAudio` — save state → load into a fresh
  instance → the two render the same audio within `1e-4`. Observed max diffs
  `~1e-4`–`4e-3`; parameter values round-trip correctly (only the audio differs).

These are **not** compiler/portability errors and **not** floating-point-model
differences (MSVC `/fp:precise` and AppleClang are deterministic run-to-run
within one binary). They are a latent non-determinism that stays under the `1e-4`
tolerance on Linux/libstdc++ but crosses it off Linux. The most likely cause is
a read of not-fully-initialised state across a `reset()`/reallocation: Linux's
allocator hands back freshly-zeroed pages so the stray read is `0` in both
passes, whereas the Windows/macOS heaps recycle the previous pass's memory, so
the second pass sees different residual values. (The one `getSystemRandom()` seed
in the DSP path, `CharacterEngine::reroll()`, was ruled out — it is only invoked
when the user rolls a new instrument, never during reset/render.)

Fixing this correctly means changing DSP state-reset / initialisation logic and
must be verified with an address/memory sanitizer on a Windows or macOS host. It
is deliberately **out of scope** for this portability pass (which is minimal and
must not refactor DSP logic), and the tests were left intact — not weakened,
skipped, or platform-guarded, because determinism is expected to hold on every
platform. This is the tracked follow-up to reach fully green Windows/macOS CI.

## How to reproduce / drive the CI

- Push to `claude/luthier-crossplatform` does **not** auto-run CI (the branch is
  not in `ci-cadence.yml`'s push list). Drive it by dispatching `build.yml`:
  Actions → build → Run workflow → `platforms: windows,macos` (or a single
  platform). macOS runners are free on this public repo; Windows builds ~45 min
  with a cold sccache.
- Windows/macOS unit-test steps are slow (MSVC/AppleClang Release, heavy DSP
  property tests — individual cases take 40–70 s; the full suite is 30–40 min).
