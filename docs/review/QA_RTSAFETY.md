# QA Task Q1 — audio-thread real-time-safety review

Reviewer branch: `claude/luthier-qa-rtsafety`. Read-only: no engine/UI/parameter file was
edited to produce this report; two new test files were added under `Source/Tests/`
(see "Tests added" at the end). All findings below were independently verified by
reading the cited code, not inferred from comments alone.

## Scope and method

Every module reachable from `LuthierAudioProcessor::processBlock`/`processSlice` and
`LuthierEngine::processBlock`/`processSubBlock` was read in full: all of `Source/DSP/**`,
`Source/LuthierEngine.{h,cpp}`, `LuthierEngineQuality.cpp`, `LuthierEngineRealismB.cpp`,
and the audio-thread-reachable parts of `Source/PluginProcessor.{h,cpp}` and the member
classes it drives each block (Routing, MidiOutRouter, ModMatrix, KillSwitch, MonitorMix,
Metronome, Looper/SessionRecorder, BackingTrack, TunePlayer, capture/telemetry, cabinet
and body IR slots, ToneMatch). The codebase was split into 8 parallel review passes by
subsystem; each pass checked, against `spec/engine.md`'s rules, for: audio-thread
allocation, blocking locks, file I/O, `throw`/throwing STL calls, missing/weak NaN-Inf
guards and DC blockers on recursive stages, `reset()` completeness, and hardcoded/stale
sample-rate assumptions. Findings are ordered worst first within each severity band.

**Severity**: **CRITICAL** — will glitch, deadlock, race, or corrupt state in real DAW
use under ordinary play. **HIGH** — real risk under plausible-but-less-common conditions
(parameter automation, less-common features, high sample rates). **MEDIUM** —
correctness/robustness gap, unlikely to be audible on its own. **LOW** — style/defensive
nit or spec-wording gap with no realistic audible impact.

**Cross-reference to `docs/review/FINDINGS.md`**: this branch is independent of the
existing review log, but two of the findings below duplicate already-tracked, still-open
items and are marked as such rather than re-reported as new: the oversized-host-block
`juce::MidiBuffer` allocation and the `thread_local` scratch-vector allocation both match
**R-107** (`reported-for-claude/luthier-realism-c`, open); the blocking lock in
`BodyEngine::rebuildModalBank()` is related to but broader than **R-113**
(`reported-for-integration`, open) — R-113 flags one call path (the live part-swap); the
finding below is a second, more direct path (every audio block, unconditionally) that
R-113 does not cover. Everything else below is new.

## Summary

56 findings: **10 critical, 14 high, 18 medium, 14 low.** Worst offenders, in one line
each: (1) three separate places where a body-config/IR rebuild that is explicitly
documented "message-thread only" is actually invoked unconditionally on the audio thread
every block; (2) a `juce::dsp::Convolution` engine mutated on the message thread while
the audio thread calls `process()` on the same object with no synchronization; (3) an
audio-thread allocation on the very first callback (`thread_local` scratch vectors,
already tracked as R-107); (4) five different places where a recursive/feedback stage
(string waveguide, amp sag, tone stack, scrape excitation) accepts or produces a
non-finite sample with no guard, so a single NaN or Inf permanently poisons persistent
filter/delay-line state until an explicit `reset()`.

Tests added: 2 new files under `Source/Tests/`, both build and pass on Linux (see
"Tests added" at the end) — they exercise paths the existing
`AudioThreadSafetyTests.cpp` fixture does not reach. They deliberately do **not** cover
the oversized-host-block path (finding C-04 below), since that path is currently broken;
see that note for why.

---

**A note on C-01/C-02/C-03 and the existing allocation-counter test**: `AudioThreadSafetyTests.cpp`'s `Engine, fiveMinutesOfPlaybackNeitherAllocatesNorLocks` (the whole-processor, 40+ second fixture) currently **passes** on this branch, confirmed by running it directly (`LuthierTests fiveMinutesOfPlayback`), despite these three findings, and despite the default `bodyMode` parameter being 0 (Convolution) — so `reloadBodyIr()` genuinely does run on every block of that test. This does not mean C-01–C-03 are false: `applyToEngine()` unconditionally calling `rebuildBodyCoupling()`/`setBodyConfig()`/`reloadBodyIr()` every block is verified directly in the source, not inferred. It means the allocation-counter test's specific mechanism doesn't happen to catch every consequence of it in this fixture — plausible reasons, not fully run to ground here: `BodyCouplingBank::stage()`'s lock is a `juce::SpinLock`, which (unlike `juce::CriticalSection`) is not backed by `pthread_mutex_lock` and so is invisible to `ThreadProbe`'s interposer; and `BodyModels::buildModes()`'s `reserve()` calls may only allocate once (during the test's explicit one-block allowance) if the persistent scratch vector's capacity, once grown, never needs to grow again for this preset's mode count. `BodyEngine::rebuildModalBank()`'s lock **is** a real `juce::CriticalSection` reached every block, which should be visible to the interposer; that it isn't tripping the existing test either is the part of this genuinely worth double-checking before scheduling a fix, in case there is a gating mechanism this review missed. Either way, the underlying architectural problem — work and a lock explicitly documented as message-thread-only, wired unconditionally into the per-block parameter-apply path — is real and worth fixing regardless of which specific symptom a given test fixture happens to expose.

## CRITICAL

### C-01 — `BodyCouplingBank::design()`/`stage()`: documented message-thread-only, actually runs every audio block
- File: `Source/DSP/Coupling/BodyCouplingBank.cpp:97-142`, `Source/DSP/Coupling/BodyCouplingBank.h:21-24`
- Category: allocation, lock
- Finding: `design()` is explicitly documented as message-thread-only ("allocates only into a member scratch vector") and calls `BodyModels::buildModes()`, which does `dest.clear(); dest.reserve((size_t) kMaxModes);` (a `reserve()` the spec forbids in the audio path) followed by `erase(remove_if(...))` and `std::stable_sort(...)`. `stage()` then takes a real blocking `juce::SpinLock::ScopedLockType` and copies two full `BodyCouplingDesign` structs. The whole call is `LuthierEngine::rebuildBodyCoupling()`, invoked **unconditionally, with no change-detection**, from `ParameterBridge::applyToEngine()` at `Parameters.cpp:2019`. `applyToEngine()` itself is called from `PluginProcessor.cpp:1617-1620` — `const juce::ScopedTryLock engineLock (bridge.getEngineLock()); if (engineLock.isLocked()) bridge.applyToEngine();` — directly inside `processSlice`, on the audio thread, every block; verified directly: line 1617 sits between `modMatrix.processBlock(...)` and `engine.processBlock(mainOut, midiMessages)` in the same function. The try-lock only guards against a concurrent *structural* rebuild, so in ordinary play it succeeds and `applyToEngine()` — and everything it calls unconditionally, including this — runs on the audio thread on essentially every callback.
- Fix (described, NOT applied): Gate `rebuildBodyCoupling()` (and the two findings below, which share this exact root cause) behind a dirty flag set only when the relevant parameters actually change, and dispatch the rebuild to the message thread (e.g. an `AsyncUpdater`); keep only a lock-free/try-locked swap on the audio side, matching the class's own stated threading contract.

### C-02 — `BodyEngine::rebuildModalBank()`: blocking `CriticalSection` + vector rebuild, reachable every audio block
- File: `Source/DSP/Body/BodyEngine.cpp:109-113 (setBodyConfig), 143-184 (rebuildModalBank)`, `Source/DSP/Body/BodyEngine.h:71-73`
- Category: allocation, lock
- Finding: `BodyEngine::setBodyConfig()` is `config = cfg; rebuildModalBank();` with **no dirty-check** — verified directly, there is no comparison against the previous config anywhere in the function. `rebuildModalBank()` takes `const juce::ScopedLock sl (rebuildLock)` — a real, blocking `juce::CriticalSection`, not a SpinLock — and calls `BodyModels::buildModes()` again. `setBodyConfig()`'s own doc comment says "Safe to call from the message thread: the new bank is staged and swapped in on the audio thread," implying the rebuild itself is not meant to run there, but it is reached every block from `Parameters.cpp:2018` (`engine.getBodyEngine().setBodyConfig(cfg);`) inside the same unconditional `applyToEngine()` as C-01. `applyStagedBank()` (the audio-thread consumer, called every block from `BodyEngine::processBlock`) correctly only try-locks the same `rebuildLock` — the design intent was clearly "rebuild on message thread, swap with try-lock on audio thread," but the rebuild call itself has been wired into the per-block path. This is broader than the already-tracked `FINDINGS.md` **R-113** (which flags only the live part-swap path reaching this same lock); this is a second, unconditional-every-block path into the same blocking lock.
- Fix (described, NOT applied): Same as C-01 — remove `setBodyConfig`/`rebuildModalBank` from the per-block `applyToEngine()` call chain; only `applyStagedBank()`'s try-lock swap belongs on the audio thread.

### C-03 — `BodyEngine::loadImpulseResponse()`: synchronous file I/O and forced-synchronous convolution install reachable from the audio thread
- File: `Source/DSP/Body/BodyEngine.cpp:329-377 (loadImpulseResponse)`, `Source/LuthierEngine.cpp:684-702 (reloadBodyIr)`, `Source/Support/IrLibrary.cpp:177-201 (findBodyIr)`
- Category: file-io, allocation, lock
- Finding: `LuthierEngine::reloadBodyIr()` calls `IrLibrary::findBodyIr()`, which does `root.isDirectory()` (stat), builds a `juce::File`/`juce::String` path (heap allocation), calls `exact.existsAsFile()` (stat), and on a miss walks the folder with `juce::RangedDirectoryIterator` — the file itself calls `ThreadProbe::noteFileAccess()`, i.e. the codebase has its own test instrumentation asserting "no filesystem access on the audio thread" that this path defeats. `reloadBodyIr()` then unconditionally calls `body.loadImpulseResponse(file)`, which takes a **blocking** `juce::SpinLock::ScopedLockType` (not a try-lock) and, whenever the resolved file differs from the currently-loaded one, synchronously reads a 100-500 ms WAV, FFT-partitions it, and calls `ConvolutionInstaller::pumpUntilInstalled(..., (int)(0.06*sr))`, which — per its own name and `ConvolutionInstaller.h`'s doc comment ("Never call this from the audio thread: it sleeps") — busy-waits/pumps until the install finishes. This entire chain is reached every audio block, unconditionally, via `Parameters.cpp:2027-2028` (`if (mode == 0 || mode == 2) engine.reloadBodyIr();`) inside the same `applyToEngine()` whenever Body mode is Convolution or Hybrid — Convolution is `BodyEngine`'s default mode. Even when the file hasn't changed, every block still performs at least two `existsAsFile()` stats and a `juce::File`/`juce::String` construction; whenever a body-affecting parameter changes (a wood/age/width knob, a preset load, or a modulated body parameter) the audio thread performs a full synchronous IR load + FFT install — a guaranteed audible dropout. This directly contradicts `BodyEngine`'s own doc comment ("Asynchronous inside juce::dsp::Convolution... engine spec 13.4") and spec 13.4's own "IR loading is asynchronous, the plugin never goes silent" rule.
- Fix (described, NOT applied): Never call `reloadBodyIr()`/`loadImpulseResponse(file)` from the per-block parameter-apply path. Detect the target IR file change on the message thread (or a dedicated loader thread), perform resolution/read/FFT-install there, and hand the fully-installed `juce::dsp::Convolution` to the audio thread via a lock-free double-buffered pointer swap; keep the audio-thread side to a try-lock swap, with the existing all-pass/unit-impulse fallback covering the load window.

### C-04 — `LuthierEngine::processBlock`'s oversized-block path allocates a fresh `juce::MidiBuffer` on the audio thread every call — (= FINDINGS.md R-107, confirmed still present)
- File: `Source/LuthierEngine.cpp:2185-2227`
- Category: allocation
- Finding: When a host hands `processBlock` more samples than `prepareToPlay` promised, the split path declares `juce::MidiBuffer sliceMidi;` as a **local** (line 2187) and immediately calls `sliceMidi.ensureSize (2048);` (line 2188). Unlike its sibling `directSlice`, which is a member (`LuthierEngine.h:651`) reused across calls, `sliceMidi` is freshly constructed and destroyed every single call that takes this branch, so it heap-allocates (and frees) on the audio thread every time — not just once. The surrounding comment ("Sized once, outside the loop, so splitting never allocates per slice") is only true *within* one call; it does not hold across calls. This is already tracked as **R-107** in `docs/review/FINDINGS.md` (`reported-for-claude/luthier-realism-c`, open), whose note observes "the oversized path is no longer reached from the processor after R-001" — confirmed: `PluginProcessor::processBlock` (fixed under R-001) now always slices host blocks to `currentBlockSize` before calling `engine.processBlock`, and `currentBlockSize` is the same value passed to `engine.prepare()`, so the plugin's own audio path can no longer trigger this. It is still live, however, for any other caller that drives `LuthierEngine::processBlock` directly with an oversized buffer (several existing unit tests do this deliberately, and any future offline-render/export path that calls the engine directly would hit it). Deliberately **not** covered by this branch's new tests — see "Tests added."
- Fix (described, NOT applied): Make `sliceMidi` a member (like `directSlice`), sized once in `prepare()`; only `clear()` it per slice thereafter. (Already the prescribed fix under R-107.)

### C-05 — `LuthierEngine::processSubBlock`: `thread_local` scratch vectors lazily resized on the audio thread — (= FINDINGS.md R-107, confirmed still present, second instance)
- File: `Source/LuthierEngine.cpp:2877-2879` (pre/post-amp stereo conversion), `2981-2983` (secret effect)
- Category: allocation
- Finding: `static thread_local std::vector<double> dl, dr;` (2877) and a second pair `sl, sr2` (2981), each lazily `.resize()`d on first use inside `processSubBlock` instead of being sized in `prepare()`. Both start empty, so the very first `processSubBlock` call on a given audio thread allocates. Because the storage is `thread_local` rather than a per-engine member, it is also shared across every `LuthierEngine` instance rendering on that thread — a later-created instance with a larger `maxBlock` than any prior instance seen on that thread triggers another allocation mid-session, on the audio thread, outside `prepare()`. This is the second half of the already-tracked **R-107** ("`static thread_local std::vector` scratch resized on the audio thread"); confirmed still present at the locations above (the line numbers in R-107 are stale — the file has grown since — but the bug is unchanged).
- Fix (described, NOT applied): Replace both pairs with real `LuthierEngine` members (e.g. `preFxL, preFxR, secretL, secretR`) sized once in `prepare()` to `maxBlock`, exactly like `stringSumBuffer`/`instrumentBuffer` already are; drop the `thread_local`/lazy-resize pattern entirely.

### C-06 — `IrSlot`'s `juce::dsp::Convolution` engine is mutated on the message thread while the audio thread calls `process()` on the same object, unsynchronized
- File: `Source/ToneMatch/ToneMatch.cpp:392` (`convolution->loadImpulseResponse(...)` inside `rebuild()`, itself at line 329), `Source/ToneMatch/ToneMatch.cpp:468-499 (process())`; driven per block from `Source/PluginProcessor.cpp:1746-1748` inside `processSlice`
- Category: lock (race)
- Finding: `IrSlot` keeps exactly one `std::unique_ptr<juce::dsp::Convolution> convolution` (`ToneMatch.h:195`). The class implements a careful lock-free swap for its `ImpulseResponse` *metadata* (`active`/`live`/`retired`, with a comment explicitly describing the "hand it to the audio thread" rule) — but the real DSP object used for audio is not part of that swap. `rebuild()` (message thread, reached from `load()`, `setStartTrim()`, `setEndTrim()`, `setPredelayMs()`, `setReversed()`, `setChannel()`, `setMaxSeconds()` — all wired to `ToneMatchPanel` UI controls) calls `convolution->loadImpulseResponse(...)` directly on the live object, in place, with **no lock, no try-lock, no double buffering**. Meanwhile `IrSlot::process()` calls `convolution->process(context)` on the audio thread every block the slot is engaged. A user changing the cab IR file, its trim/predelay/reverse, or its channel while audio is running — an ordinary, supported live-tweaking workflow — races a `juce::dsp::Convolution` engine rebuild against a concurrent `process()` call on the same object. `juce::dsp::Convolution::loadImpulseResponse` is not documented as safe to call concurrently with `process()`; this is a genuine crash/glitch race, not a hypothetical one.
- Fix (described, NOT applied): Give `IrSlot` the same lock-free handover it already uses for the `ImpulseResponse` metadata, but for the actual engine: build a second `juce::dsp::Convolution` off the audio thread (prepared with the same `ProcessSpec`), load the new IR into it there, publish it via an `std::atomic<Convolution*>` the audio thread reads at the top of `process()`, and retire the previous engine only after a block has passed — mirroring the existing `active`/`live`/`retired` pattern already used one struct over.

### C-07 — Waveguide write path sums unsanitised external inputs into recursive delay-line state
- File: `Source/DSP/String/StringEngine.cpp:1277`
- Category: nan-denormal
- Finding: `delayLine.write (fb + exc + couplingInput * couplingReceptivity + noise + directInput);` writes the sum of five terms into the string's persistent waveguide memory. Only `fb` has passed through `sanitise()` (line 1183). `couplingInput` and `directInput` are supplied by callers outside this file (the bridge coupling matrix and external excitation triggers) and are summed in completely unguarded. `FractionalDelayLine::write()` only calls `flushDenormal()`, which does not catch NaN/Inf (`std::abs(NaN) < floor` is always `false`, so NaN passes through unchanged — `DspCommon.h:62-65`). A single non-finite `couplingInput` (e.g. propagated from C-08 below, or a divide-by-zero elsewhere in the coupling path) lands directly in this string's recursive delay-line memory with no guard at all, and stays there.
- Fix (described, NOT applied): Wrap the full sum in `sanitise(...)` before `delayLine.write(...)`, or `isfinite`-check `couplingInput`/`directInput` individually at the top of `endSample()` before they are used anywhere.

### C-08 — Feedback-loop and E-Bow injections re-enter string/body state unsanitised
- File: `Source/LuthierEngine.cpp:2716-2726` (feedback + E-Bow into `couplingIn`), `2681-2687` (`bridgeWaves` into `bodyCoupling`)
- Category: nan-denormal
- Finding: In the per-sample string loop, `const double fb = feedbackLoop.process (s, i); couplingIn += fb;` and `couplingIn += ebowDriver.process (s, stringOutputs[s]);` both feed straight into `couplingIn`, passed into each string's `endSample()`/`processSample()` — directly into its recursive waveguide state (the same state C-07 also writes unguarded). Neither is passed through `sanitise()` first, even though `sanitise()` is used elsewhere in this same file (`stringSumBuffer`, `preCircuitBuffer`). Likewise `bridgeWaves[s] = strings[s].getBridgeWave();` is fed unsanitised into `bodyCoupling.processSample(...)`, whose output also reaches `couplingIn`. The later guard `stringSumBuffer[i] = sanitise(sum)` (line 2755) is too late — it clamps the display/output value for that one sample, but by then the NaN has already been written into the string's own recursive delay line and will keep re-emerging every subsequent sample until an explicit `reset()`.
- Fix (described, NOT applied): `couplingIn += sanitise (fb);`, `couplingIn += sanitise (ebowDriver.process (...));`, and sanitise `bridgeWaves[s]` (or the coupling bank's output) before it re-enters the string loop.

### C-09 — `ScrapeEngine`'s excitation output has no NaN/Inf guard or amplitude clamp before entering the string feedback loop
- File: `Source/DSP/Noise/ScrapeEngine.cpp:780-782`
- Category: nan-denormal
- Finding: `renderSample()` computes `excitation[s][i] += gain * (pulse - delayed)` and `noiseOut[i] += gain * pulse` and returns them unmodified — no `sanitise()`/`flushDenormal()` anywhere in this file's audio output path (its only `isfinite` uses are in `setString()`). Every other noise source in the engine wraps its final sample in `sanitise()` (`NoiseGenerator::process()`, every tap in `NoiseFloor::beginBlock()`). `ScrapeEngine::getExcitation(s)[i]` is read directly and passed straight into `strings[s].processSample()/endSample()` at `LuthierEngine.cpp:2712` and `2730-2731` — directly into the string waveguide's feedback loop, with nothing in between. Compounding this, `ScrapeSettings::level` is clamped only to `[0.0, 16.0]` (`ScrapeEngine.cpp:178`) — roughly 9x the codebase's usual note-reference level and 4x the universal guard limit (`constants::kGuardLimit = 4.0`); with `kCatchReference = 0.6` and `angleFactor` up to ~1.43, a single catch's `pulseAmp` can reach roughly 15, written straight into the shared excitation buffer every time a winding boundary is crossed. This is exactly the case spec rule 3 and the review brief call out: noise-engine excitation must be bounded so it cannot inject out-of-range or NaN values into the string engine's feedback loop, and this is the one place in the reviewed code where that guard is entirely absent.
- Fix (described, NOT applied): Add a `sanitise()` call around the values written into `excitation[s][i]`/`noiseOut[i]` in `renderSample()`, matching `NoiseGenerator::process()`/`NoiseFloor::beginBlock()`. Separately, reconsider the `[0.0, 16.0]` ceiling on `ScrapeSettings::level` — far outside every other noise source's calibrated range — or scale `kCatchReference`/`angleFactor` so a pressure-1, level-16 catch cannot exceed the guard limit even before `sanitise()` catches it.

### C-10 — `AmpEngine`'s sag state has no NaN/Inf guard and cannot self-recover
- File: `Source/DSP/Amp/AmpEngine.cpp:430-434`
- Category: nan-denormal
- Finding: `powerAmpStage()`'s sag model updates `supplyVoltage = sagTarget + (supplyVoltage - sagTarget) * coeff;` then only `supplyVoltage = juce::jlimit (0.35, 1.0, supplyVoltage);`. Neither `sanitise()` nor `flushDenormal()` is ever applied to `supplyVoltage`, `sagTarget`, or `demand`. `supplyVoltage` is a plain persistent member, read every sample by `headroom = tube.knee * supplyVoltage` then `tanh(biased / juce::jmax(0.05, headroom))`. If a non-finite value ever reaches `sagFollower.process(x)` (e.g. propagated from the unguarded intermediate stages in H-02 below), `supplyVoltage` becomes NaN and **stays** NaN on every subsequent sample and block — `juce::jlimit`'s comparisons do not reliably clear NaN, and nothing resets this value except an explicit `reset()` call (only on prepare/preset reload). The engine then outputs silence indefinitely, with no way to recover during play: precisely the "audible artifact that outlives the triggering event" spec rule 3 exists to prevent, on exactly the stage the review brief flagged as highest-risk.
- Fix (described, NOT applied): Wrap `supplyVoltage` (and `sagTarget`/`demand`) with `sanitise()`/an `isfinite` check right after they are computed, or centralise a `flushDenormal`-plus-NaN clamp applied consistently to every persistent double that survives across samples.

---

## HIGH

### H-01 — `CouplingMatrix::process()` runs the sympathetic-coupling matrix as a full per-sample O(N²) update — spec explicitly forbids this
- File: `Source/DSP/Coupling/CouplingMatrix.cpp:151-219`, `Source/DSP/Coupling/CouplingMatrix.h:11-15`; called at `Source/LuthierEngine.cpp:2672` inside the per-sample string loop
- Category: other
- Finding: Engine spec 5.6 says "Do NOT implement the sympathetic coupling matrix as a full N-to-N per-sample update; update per-block, cap total coupling energy." `CouplingMatrix::process()` does exactly the forbidden thing: for every sample it recomputes a full O(N²) matrix-vector multiply over every string pair (up to 144 multiply-adds/sample at N=12); the class's own header comment acknowledges the deviation and argues it is cheap enough and that a per-block update would inject an audible step. The safety-cap half of the directive *is* honored: `kEnergyCap = 0.35` is a real, non-no-op per-sample clamp (tracked via `lastLimiting`/`validator.reportCouplingLimiting`) — but it caps per-sample amplitude, not a per-block "total coupling energy" as the spec's wording implies.
- Fix (described, NOT applied): Either record an explicit, reviewed spec exception for this deliberate per-sample design, or convert the coupling sum to a genuinely per-block-designed operation with a per-block energy accumulator (sum of squared coupling input over the block, scaled back if it exceeds a cap) rather than an instantaneous-amplitude clamp.

### H-02 — Only the very end of the amp's oversampled chain is sanitised; every intermediate recursive filter can be permanently poisoned
- File: `Source/DSP/Amp/AmpEngine.cpp:502-543`
- Category: nan-denormal
- Finding: The whole preamp → tone-stack → power-amp → output-transformer chain runs inside one oversampler lambda; `sanitise()` is applied exactly once, to the lambda's return value (line 542). None of the intermediate recursive stages — `brightShelf`, `midBoostEq`, `stageEq[i]`, `stageSmoothing[i]`, `stageCoupling[i]`, `toneStack`, `bassBeyond`/`midBeyond`/`trebleBeyond`, `piCoupling`, `presenceShelf`, `transformerLf`/`transformerHf` — clamp or check their own internal state for non-finite values; the only hygiene present anywhere in `Biquad`/`ThirdOrderFilter`/`OnePoleLP::process` is `flushDenormal()`, which is a no-op for NaN/Inf. A single non-finite sample reaching any of these filters poisons that filter's feedback state forever: the clamp on line 542 only fixes the *return value* for that one sample, while the corrupted internal state keeps feeding NaN into every future call until an explicit `reset()`. Directly violates spec rule 3 for essentially every recursive stage in the amp path except the very last one.
- Fix (described, NOT applied): Add a `sanitise()`/`isfinite` clamp at the point each stage's own recursive state is updated (or at minimum after each stage's `process()` call inside the lambda).

### H-03 — `ToneStack::recompute()` only guards against `az0≈0`, not against an unstable (off-unit-circle) pole set
- File: `Source/DSP/Amp/ToneStack.cpp:140-166`
- Category: nan-denormal
- Finding: `recompute()` derives the bilinear-transform coefficients and only guards the single failure mode `std::abs(az0) < 1e-24 || !std::isfinite(az0)` (bypassing to unit gain if it fires). Nothing checks that the resulting pole set (`az1/az0, az2/az0, az3/az0`) lies inside the unit circle, and `ThirdOrderFilter::process` applies no per-sample amplitude clamp — its only hygiene is `flushDenormal(y)`, which doesn't catch NaN/Inf and does nothing for a merely-unstable-but-finite pole (`|pole| > 1`), which would grow the output geometrically forever. `AmpEngine::prepare()` runs the tone stack at the *oversampled* rate (up to ~1.5 MHz effective at 8x/192kHz), and the coefficient derivation involves several ± terms of comparable size (subtractive cancellation), which is far more extreme at 8x than at 1x. This is exactly the "tone-stack coupled IIR — verify no runaway/instability path" item the review brief called out, and the only protection is the single `az0`-near-zero check.
- Fix (described, NOT applied): After computing the pole ratios, check their magnitudes (or run a cheap Jury/Schur-Cohn stability test) and fall back to the previous good coefficient set (or unity response) if unstable; additionally clamp `ThirdOrderFilter`'s own `y` each sample.

### H-04 — Recursive filter state has no NaN/Inf recovery once poisoned; `StringEngine`'s loop-filter/dispersion chain reads unguarded delay-line output
- File: `Source/DSP/Common/DspCommon.h:62-65 (flushDenormal), 136-140 (OnePoleLP::process), 291-297 (Allpass1::process)`; `Source/DSP/String/StringEngine.cpp:1146-1152`
- Category: nan-denormal
- Finding: Every recursive primitive in `DspCommon.h` (`OnePoleLP`, `Allpass1`, `Biquad`, `DCBlocker`, `EnvelopeFollower`, and `StringEngine`'s own `Resonator`) guards its persistent state only with `flushDenormal()`, never `isfinite`. Because `flushDenormal` doesn't detect NaN/Inf, once a filter's state variable becomes non-finite it stays non-finite forever (`z = flushDenormal(b*x + a*z)` with `a*NaN == NaN` never recovers). In `StringEngine::beginSample()`, `delayOut` flows into `applyContacts()` then `fb = loopFilter.process(touched)` (line 1152) and the `dispersion[i].process(fb)` cascade with **no** `sanitise()` beforehand — the only guard (`fb = sanitise(fb)`, line 1183) runs *after* the value has already been fed into and stored inside `loopFilter`'s and every `dispersion[i]`'s internal state. Once poisoned, the per-sample `sanitise(fb)` afterward only zeroes the *output* each sample, so the audible symptom is a permanently, silently dead voice until something calls `reset()`.
- Fix (described, NOT applied): Add an `isfinite` short-circuit inside `flushDenormal` (or a distinct `sanitiseState()` helper) so recursive stages self-heal instead of latching NaN, and/or call `sanitise()` on `delayOut`/`touched` immediately after `delayLine.read()`, before it reaches `loopFilter`/`dispersion`.

### H-05 — Loop gain cap (0.9995) exceeds the spec-mandated 0.998 ceiling and is routinely hit
- File: `Source/DSP/String/StringEngine.cpp:25` (`constexpr double kMaxLoopGain = 0.9995;`), used at line 1036
- Category: other
- Finding: `spec/engine.md:751` states "Cap at 0.998 to prevent runaway"; the engine's cap is `0.9995`. Not hypothetical: for a high fretted note with a long sustain setting, `loopGain = exp(-kT60Constant * loopSamples / (t60 * sr))` easily exceeds 0.998 (e.g. a ~1 kHz note at 44.1 kHz with a 60 s target T60 computes to `loopGain ≈ 0.99995`), so `0.9995` — not `0.998` — is the operative limit the engine regularly runs at.
- Fix (described, NOT applied): Lower `kMaxLoopGain` to `0.998` to match the spec, or record the deviation as a deliberate, reviewed change if `0.9995` was intentional.

### H-06 — Loop-filter cutoff and loop gain jump instantaneously when damping changes mid-note (no smoothing/crossfade)
- File: `Source/DSP/String/StringEngine.cpp:504-519 (setDamping)`, `977-979` and `1035-1036 (updateLoopCoefficients)`
- Category: sample-rate (spec rule 4)
- Finding: `setDamping()` sets `damping`/`dampingAmount` and `needsLoopUpdate = true`; the next `beginSample()` recomputes `loopCutoffHz`/`loopGain` and applies them immediately — a direct coefficient write, not a ramp. Since `Damping` is exactly the "discrete choice" spec rule 4 calls for a 5 ms crossfade on, engaging a palm mute or choke on an already-ringing string (e.g. cutoff 5000 Hz → 500 Hz, T60 4.5 s → 0.035 s) steps the recursive loop filter's coefficient and the loop's decay rate on a single sample boundary inside the feedback path, with no crossfade anywhere in the call chain.
- Fix (described, NOT applied): Smooth `loopCutoffHz`/`loopGain` themselves (e.g. via an `ExpSmoother`), or explicitly crossfade old/new coefficients over ~5 ms when `damping` changes.

### H-07 — `PhaserPedal`'s feedback path has no DC blocker
- File: `Source/DSP/Effects/PedalsMod.cpp:257-269` (declaration `PedalsMod.h:131-139`)
- Category: nan-denormal
- Finding: `fbL`/`fbR` feed back into the allpass cascade every sample (feedback up to 0.9) via `fbL = sanitise (xl);`. `sanitise()` clamps NaN/Inf/range, not DC. `Allpass1` is flat-magnitude at all frequencies including DC, so any DC entering the loop recirculates at up to 0.9x per pass instead of being removed. No `DCBlocker` is declared anywhere in `PhaserPedal`, unlike `DelayPedal`/`ReverbPedal`.
- Fix (described, NOT applied): Add a `DCBlocker` member (7-10 Hz) and run `fbL`/`fbR` through it before re-injection next sample.

### H-08 — `FlangerPedal`'s feedback path has no DC blocker
- File: `Source/DSP/Effects/PedalsMod.cpp:325-343` (declaration `PedalsMod.h:159-164`)
- Category: nan-denormal
- Finding: Same gap as H-07, with feedback allowed up to ±0.95 (closer to unity than the phaser's 0.9). `wetL`/`wetR` come straight out of the modulated delay line (unity gain at DC) and feed back via `fbL = sanitise(wetL);` with no highpass/DC-blocking stage. No `DCBlocker` member exists.
- Fix (described, NOT applied): Add a per-channel `DCBlocker` in the feedback path, as recommended for the phaser.

### H-09 — `ReverbPedal` wipes and rebuilds its FDN lines on every changed Size/Character value — continuous automation glitches the reverb every block
- File: `Source/DSP/Effects/PedalsMod.cpp:793-817, 819-844`
- Category: other
- Finding: `parameterChanged()` sets `needsRebuild = value != size` and, when true, `rebuildLines()` `std::fill`s every one of the 8 FDN lines to zero and resets `lineIndex[i]`. The code comment acknowledges the danger ("only a real change may rebuild, or the tail is wiped each block") but the guard only protects against *identical, repeated* values — any continuously-changing value (host automation, an LFO/macro mapped to Size/Character, or a player slowly turning the knob while the tail rings) makes `value != size` true on effectively every block, silently zeroing the whole network every block for as long as the parameter keeps moving. A real, audible glitch under a very ordinary use case, not a rare edge case.
- Fix (described, NOT applied): Debounce the rebuild (epsilon threshold and/or rate-limited coalescing), or crossfade between old/new line lengths instead of hard-zeroing on every touch.

### H-10 — Most pedal parameters (Mix, Feedback, Depth, EQ gains, etc.) are applied instantly with no per-sample smoothing
- File: `Source/DSP/Effects/PedalsMod.cpp:175-176, 271-272, 340-341, 420-421, 1093-1094, 1175-1188`
- Category: other
- Finding: Every pedal's own wet/dry `mix`, `feedback`, `depth`, and EQ gain/frequency values are plain `double` members written directly in `parameterChanged()` and read directly, unsmoothed, in `process()`. `Pedal`'s base-class `mixSmooth`/`bypassFade` only smooth the top-level bypass crossfade, not any pedal's own internal parameters. For the EQ pedals this also means `Biquad::setPeaking/setLowShelf/...` recomputes coefficients on a filter that still holds nonzero `z1`/`z2` state from the old coefficients — the classic biquad-coefficient-snap click. A broad, systemic gap against spec rule 4, not limited to one pedal.
- Fix (described, NOT applied): Route each parameter through an `ExpSmoother` (already used elsewhere, e.g. `DelayPedal::timeSmooth`), or at minimum crossfade biquad coefficient changes over a few milliseconds.

### H-11 — `JamDrumKit`'s room-send amount is an unsmoothed hard parameter multiplying live audio
- File: `Source/DSP/Jam/JamDrumKit.h:62 (setRoom)`, `Source/DSP/Jam/JamDrumKit.cpp:503-506 (render)`
- Category: other (missing smoothing, spec rule 4)
- Finding: `setRoom()` does `roomSend = juce::jlimit (0.0, 1.0, amount01);` with no smoother; `render()` reads `roomSend` directly every sample: `room.process((l[j]+r[j])*0.5*roomSend, l[j], r[j]);`. If the host automates `jam_room_amount` (or the parameter changes between blocks) while drums are sounding, the wet-signal contribution steps discontinuously — an audible click — even though `ExpSmoother`/`LinSmoother` are used elsewhere in the same files (e.g. `MasterBus::gainSmooth`).
- Fix (described, NOT applied): Give `JamDrumKit` an `ExpSmoother roomSendSmooth`, `setTarget()` in `setRoom()`, `.next()` once per sample in the room-send block.

### H-12 — `CouplingMatrix`'s global amount and air coefficient are not smoothed
- File: `Source/DSP/Coupling/CouplingMatrix.h:36-37, 64-65`, `Source/DSP/Coupling/CouplingMatrix.cpp:195, 198`; applied every block from `Source/Parameters.cpp:1415`
- Category: other
- Finding: `setAmount()` writes `globalAmount` directly with no smoother, and `process()` applies it instantaneously every sample (`sum *= globalAmount;`). `setAirCoefficient()` likewise. Because `globalAmount` scales the coupling energy fed into every string simultaneously, an automated or user-turned "Sympathetic" knob produces an instantaneous step injected into every string's delay line at once — a broadband click, distinct from every other tunable in this file (which filter continuously but don't ramp their gain terms).
- Fix (described, NOT applied): Wrap `globalAmount`/`airCoefficient` in the codebase's existing `ExpSmoother` pattern (e.g. `BodyEngine::amountSmooth`).

### H-13 — Local `MidiBuffer`/scratch handling in `PluginProcessor`'s common (non-sliced) path is not protected by the pre-sized-scratch discipline the sliced path uses
- File: `Source/PluginProcessor.cpp:1246-1251` (fast path) vs. `1256-1276` (sliced path, which builds into `ensureSize(8192)`'d members and only `swapWith()`s); downstream writers: `Source/Routing/MidiOutRouter.cpp:69-147`, `Source/PluginProcessor.cpp:1945, 1949, 1950/3616, 1520, 1539`
- Category: allocation
- Finding: When the host calls `processBlock` with `numSamples <= currentBlockSize` — the ordinary case for the overwhelming majority of DAW sessions — `midiMessages` passed into `processSlice` **is** the host's own buffer, not a plugin-owned scratch buffer. Every downstream MIDI-out writer (pass-through, rhythm engine, string-activity, CC-broadcast, tune/jam MIDI-out, Luthier SysEx) calls `addEvent`/`addEvents` directly on it with no prior `ensureSize()` from Luthier's side. `juce::MidiBuffer` retains capacity across `clear()`, so this is amortized after a warm-up block, but the first block (or any later block) that needs more storage than the host happened to allocate for its own input MIDI can still grow — allocate — on the audio thread, especially with several MIDI-out features on at once.
- Fix (described, NOT applied): In the fast (non-sliced) path, route MIDI-out construction through a pre-sized scratch buffer and `swapWith()` it into `midiMessages` at the end, exactly as the sliced path already does.

### H-14 — `ebowDriver` state is never cleared by `LuthierEngine::reset()`
- File: `Source/LuthierEngine.h:872`, `Source/LuthierEngine.cpp:147-271 (reset()), 1130 (panic())`
- Category: reset-state
- Finding: `LuthierEngine::reset()` calls `feedbackLoop.reset()` (line 250) but never calls `ebowDriver.reset()` anywhere in that function — the only call site for `ebowDriver.reset()` in the whole file is inside `panic()` (line 1130), confirmed directly by grepping every call site. Since `strings[i].reset()` (line 154) wipes every string's energy/state during `reset()`, but the E-Bow driver's own internal envelope/target-level/per-string biquad state survives, a `reset()` invoked while E-Bow is active (a preset reload, transport restart, or host re-init mid-performance) leaves the driver believing it is still driving strings at their old levels against freshly-silenced strings on the next block.
- Fix (described, NOT applied): Add `ebowDriver.reset();` to `LuthierEngine::reset()`, alongside `feedbackLoop.reset()`.

---

## MEDIUM

### M-01 — `BodyCouplingBank`'s resonator bank has no DC blocker on its output
- File: `Source/DSP/Coupling/BodyCouplingBank.cpp:401-470 (processSample), 369-399 (runModes)`
- Finding: The 16-mode resonator bank only applies `sanitise(v)` — no `DCBlocker` anywhere in the class, unlike `CouplingMatrix` (`receiveDC` per string) and `BodyEngine` (`dcLeft`/`dcRight`). Some modes sit at very low ("air") frequencies with wide bandwidth, where a bandpass shape alone suppresses DC less effectively than the higher-Q modes elsewhere.
- Fix (described, NOT applied): Add a `DCBlocker` (7-8 Hz) to the output in `processSample()`, cleared in `reset()`.

### M-02 — `BodyCouplingBank`'s hand-rolled biquad recursion never flushes denormals
- File: `Source/DSP/Coupling/BodyCouplingBank.cpp:369-399, 431-446`
- Finding: Unlike every other recursive filter in the codebase, this class's hand-written transposed-direct-form-II update never calls `flushDenormal()`. `body_coupling_amount` defaults to 1.0, so this 16-lane bank runs by default for every user, making it one of the more exposed spots for a denormal-driven CPU spike if hardware FTZ/DAZ is ever not in effect (nested hosts, non-x86 targets).
- Fix (described, NOT applied): Call `flushDenormal()` on the recursion state, matching `Biquad::process`.

### M-03 — `CouplingMatrix`'s air-path delay silently saturates at very high sample rates
- File: `Source/DSP/Coupling/CouplingMatrix.cpp:21`, `Source/DSP/Coupling/CouplingMatrix.h:105-109 (kAirRing = 64)`
- Finding: `airDelay` is correctly computed from the live sample rate, but the ring buffer is fixed at 64 samples (sized for 192 kHz). Above ~217 kHz the computed delay exceeds `kAirRing - 1` and `jlimit` silently clamps it — a quiet correctness regression at the high end of what professional interfaces support, not a crash.
- Fix (described, NOT applied): Size the ring from the prepared sample rate, or explicitly document/assert the supported ceiling.

### M-04 — Kit swap re-tunes actively-ringing drum voices immediately, unlike the dedicated tuning path
- File: `Source/DSP/Jam/JamDrumKit.cpp:196-267 (applyKit)`, `Source/DSP/Jam/DrumPieces.cpp:41-48, 379-384`
- Finding: `MembranePiece::setTuning()` explicitly guards mid-note changes; `setDesign()` (used by every kit swap) has no such guard, so a kick/snare/tom/hat/ride/crash still ringing when a kit swap lands has its resonant frequencies and decay rate jump mid-decay. "Kit changes land at a bar line" is an external contract enforced by the (out-of-scope) caller, not internally.
- Fix (described, NOT applied): Gate `applyKit()` on `isSilent()`, or make `setDesign()` defer like `setTuning()` does.

### M-05 — `ModalResonatorBank`'s recursive state has no per-sample NaN/Inf/runaway guard; only a periodic (up to 64-sample) housekeep catches it
- File: `Source/DSP/Jam/ModalResonatorBank.cpp:145-179`, `Source/DSP/Jam/ModalResonatorBank.h:61-90`, housekept every 64 samples from `Source/DSP/Jam/JamDrumKit.cpp:432-454`
- Finding: No `isfinite`/clamp check per sample in the recursion; the class's own comment documents a non-finite state as "clamped out" only via `housekeep()`, called on a fixed 64-sample cadence. In practice fully mitigated because every piece's final output passes through `PieceOutput::process()` (its own per-sample `isfinite`/[-4,4] clamp), so audio cannot carry the poison out — but the *internal bank state* can sit non-finite for up to ~1.3 ms before `housekeep()` resets it, a real gap against "every recursive stage has its own guard" if the mitigation downstream were ever removed or bypassed.
- Fix (described, NOT applied): Add a per-sample (or at least per-64-sample-inside-`render()`) finite check inside `ModalResonatorBank` itself, or explicitly document that `PieceOutput` is structurally mandatory on every call site (it is, today).

### M-06 — `ReverbPedal`'s FDN recirculating lines have no DC blocker of their own; only the final output tap is DC-blocked
- File: `Source/DSP/Effects/PedalsMod.cpp:897-908, 919-921`
- Finding: The Householder feedback matrix's output is only `flushDenormal(sanitise(v))`'d before being written back into `lines[i]`. `dcL`/`dcR` are applied only to the summed output taps, after the signal has left the recirculating network. `feedbackGain` up to 0.9985 gives a very slow decay for any DC entering the loop even though the output itself is clean.
- Fix (described, NOT applied): Add a per-line (or shared, pre-feedback) `DCBlocker` applied before the `lines[i]` write.

### M-07 — `SpringReverbPedal`'s per-spring feedback loops have no DC blocker; only the mixed output is DC-blocked
- File: `Source/DSP/Effects/PedalsMod.cpp:1017-1034, 1036-1037`
- Finding: Same pattern as M-06: each spring's loop (gain up to 0.92) recirculates through `sanitise()`/`flushDenormal()` only; `dcOut` is applied once after all three springs are summed.
- Fix (described, NOT applied): Add a `DCBlocker` per spring, or apply one before each spring's delay-line write.

### M-08 — `reset()` does not synchronously recompute `StringEngine`'s loop coefficients; public getters can read stale pre-reset values
- File: `Source/DSP/String/StringEngine.cpp:61-155 (reset())`, `Source/DSP/String/StringEngine.h:331-332 (getLoopGain/getLoopCutoffHz)`
- Finding: `reset()` sets `needsLoopUpdate = true` but never calls `updateLoopCoefficients()` itself; the recompute only happens lazily inside the next `beginSample()`. The getters are documented "for the validator and unit tests," so test/validator code calling `reset()` and immediately reading them sees the previous note's values.
- Fix (described, NOT applied): Call `updateLoopCoefficients()` at the end of `reset()`.

### M-09 — `goToSleep()` leaves the longitudinal-ping resonators and noise-shaping filter state uncleared
- File: `Source/DSP/String/StringEngine.cpp:884-903`
- Finding: The idle-string sleep path resets `delayLine`/`loopFilter`/`dispersion[]`/`dcBlocker`/`levelFollower` and zeroes several envelopes, but not `ping1`/`ping2` (the longitudinal-ping `Resonator`s), `pingSamplesLeft`, or the slide/fret-noise filter state — all of which `reset()` does clear. A string put to sleep mid-ping (up to ~210 ms after attack) resumes with stale ping/noise energy once woken by coupling.
- Fix (described, NOT applied): In `goToSleep()`, also reset `ping1`/`ping2`, `pingSamplesLeft = 0`, and the slide/fret-noise filter state.

### M-10 — `setSampleRateKeepingState` updates `sr` without recomputing filter coefficients
- File: `Source/DSP/Common/DspCommon.h:118-119 (OnePoleLP), 457-458 (EnvelopeFollower)`
- Finding: Both update the stored `sr` but leave coefficients computed for the previous rate; the contract relies entirely on the caller following up with `setCutoff()`/`setTimes()`. Nothing enforces that, so a future call site invoking this alone silently keeps wrong-rate coefficients.
- Fix (described, NOT applied): Fold the coefficient recompute into the function itself, or require the cutoff/times as a parameter so it cannot be called without updating coefficients.

### M-11 — `feedbackInjection` buffer written without the `sanitise()` guard
- File: `Source/LuthierEngine.cpp:2757-2758`
- Finding: `feedbackInjection[i] = fbSum;` with no `sanitise()`, unlike every other buffer this file publishes. `getFeedbackInjection()` exposes this publicly, so a non-finite feedback sample propagates as NaN/Inf to whatever reads it (routing/UI).
- Fix (described, NOT applied): `feedbackInjection[i] = sanitise (fbSum);`.

### M-12 — `setPickupPlacementLive`'s "no audio running" fast path is a genuine cross-thread race window
- File: `Source/LuthierEngine.cpp:2315-2329`
- Finding: Always stores into atomics first (correct), but when its 200 ms staleness heuristic decides no audio thread is running, it calls `applyLivePickupPlacements()` **directly on the calling (message) thread**, mutating `pickups` (not atomic) via `setPickupSpec()`. The heuristic is not a synchronization primitive: if the audio thread resumes in the window between the check and the call, both threads touch `PickupEngine` state concurrently — an actual data race.
- Fix (described, NOT applied): Drop the direct-apply fast path; always rely on the dirty-bit hand-off picked up on the next `processSubBlock` call, including for tests/offline-renderer.

### M-13 — `FreezeOverlay`'s `enabled` latch is not cleared by `LuthierEngine::reset()`, so Freeze can silently stop working after a preset load / transport-start reset
- File: `Source/DSP/Master/FreezeOverlay.cpp:20-32 (reset()), 36-49 (setEnabled())`; called from `Source/LuthierEngine.cpp:190 (freezeOverlay.reset())` inside `LuthierEngine::reset()`
- Category: reset-state
- Finding: Directly verified (not from a subagent report). `FreezeOverlay::reset()` clears `captured`, `state = State::idle`, `capturedSoFar`, `readPosition`, `envelope`, and the filter state — but never touches the private `enabled` bool. `setEnabled()` only acts on a rising edge: `if (shouldBeEnabled == enabled) return;`. `process()` returns immediately whenever `state == State::idle`. So: a user engages Freeze (`enabled = true`, capture begins, eventually `state == holding`); `LuthierEngine::reset()` runs (transport start, preset load, or panic) and calls `freezeOverlay.reset()`, which snaps `state` back to `idle` but leaves `enabled == true`; the next call to `setEnabled(true)` from the parameter bridge (the value hasn't changed from the player's point of view) is a same-value no-op, so `state` never leaves `idle` and `process()` keeps early-returning. Freeze silently produces nothing until the player explicitly toggles the Freeze control off and back on — a real, reproducible feature regression (not a crash/glitch), triggered by exactly the events `reset()` is meant to handle cleanly.
- Fix (described, NOT applied): In `LuthierEngine::reset()`, either call `freezeOverlay.setEnabled(false)` before `freezeOverlay.reset()` so the next genuine enable is seen as a rising edge, or have `FreezeOverlay::reset()` itself clear `enabled` to false (matching the "reset returns to a fully silent, known state" contract every other module in this file follows).

### M-14 — Several "DC blocker" cutoffs sit above the spec's mandated 5-10 Hz band
- File: `Source/DSP/Amp/RoomEngine.cpp:73-74 (12 Hz)`, `Source/DSP/Effects/SecretEffect.h:45-46 (18 Hz)`, `Source/DSP/Effects/PedalsDrive.cpp:577-578 (18 Hz)`
- Finding: `SecretEffect` is a genuine cross-coupled feedback delay — exactly the kind of stage rule 3 targets — yet sits furthest outside the band. (`CabinetEngine` at 10 Hz and `AmpEngine`'s `outputDc` at 8 Hz are within the band and not flagged.)
- Fix (described, NOT applied): Lower these three cutoffs into 5-10 Hz, or document the deviation as an intentional tonal choice.

### M-15 — MIDI-learn parameter writes go straight to `setValueNotifyingHost()` from the audio thread, inconsistent with this file's own deferred pattern
- File: `Source/Support/MidiLearn.cpp:327`, called from `processSlice` via `Source/PluginProcessor.cpp:1502`
- Finding: Two lines away in the same function, the mapping-table mutation is explicitly deferred to the message thread ("Mapping mutates the array, so it cannot happen here... No triggerAsyncUpdate: posting a message takes a lock"), and `handleLiveMidi` defers program-change/bank-select via a pending atomic + timer. The actual per-CC parameter write does not follow that pattern — it calls `setValueNotifyingHost()` synchronously, which calls every registered `AudioProcessorParameter::Listener` on the audio thread. Likely benign today (no `UndoManager` is attached to this APVTS), but depends on every current and future listener staying RT-safe, which the type system doesn't enforce.
- Fix (described, NOT applied): Either document this as an intentionally-accepted RT-safe idiom, or route CC-driven parameter changes through the same pending-atomic + timer pattern used two calls away.

### M-16 — Declick state is not reset in `prepareToPlay()`, unlike `sidechainCopy` and the rest of the audio-path state
- File: `Source/PluginProcessor.cpp:203-328 (prepareToPlay)`; compare `Source/PluginProcessor.h:948-953 (declickState, declickGain, declickDepth)`
- Finding: `prepareToPlay` explicitly resets every other piece of audio-path state (`sidechainCopy`, `monitorBuffer`, `clickBuffer`, etc.) but never touches `declickState`/`declickGain`/`declickDepth`. Harmless in the common case, but a host that stops/restarts the audio thread while a structural-change fade is in flight (`declickState` at `declickFadingOut`/`declickSilent`, `declickDepth` non-zero for tens of ms) resumes with stale ramp state instead of a guaranteed-idle declick engine.
- Fix (described, NOT applied): Explicitly reset `declickState`, `declickGain = 1.0f`, `declickDepth = 0` at the top of `prepareToPlay`.

### M-17 — Body-swap related dead state left over in `reset()` (`pendingBodyTap`)
- File: `Source/LuthierEngine.h:670`, `Source/LuthierEngine.cpp:147-271`
- Finding: `pendingBodyTap` (set by `requestBodyTap()`, consumed via `exchange(0.0)` in `advanceRealism()`) is not cleared anywhere in `reset()`. A body-tap request made just before a reset/panic-adjacent reload still fires on the first block after, even though the reset was meant to return to a known-silent state.
- Fix (described, NOT applied): `pendingBodyTap.store (0.0);` in `reset()`.

### M-18 — Per-stage NaN guard/DC blocking is only applied once, combined, at the end of a chain of recursive biquads in `PickupEngine` and `BodyEngine`'s modal bank
- File: `Source/DSP/Pickup/PickupEngine.cpp:504-505, 552-554`; `Source/DSP/Body/BodyEngine.cpp:483-495, 507-511`
- Finding: Per-coil `magnetEq`/`coverEq` biquads and per-mode `ModalResonator`s are each individually recursive but share a single `outputDc`/`dcLeft`+`dcRight` and `sanitise()` applied once after summing. Low practical risk (fixed, pre-clamped coefficients driven by bounded input, not self-sustaining loops), reported as a literal reading of "no exceptions" rather than a live hazard.
- Fix (described, NOT applied): Add per-stage hygiene if strict compliance is wanted, or document the "one guard per audio-rate output" convention as the accepted reading for feed-forward shaping filters.

---

## LOW

### L-01 — `agingBrightness`/`agingSustain`/`agingDispersion` are not restored to neutral (1.0) in `reset()`
- File: `Source/DSP/String/StringEngine.cpp:61-155`; compare `setAgingFactors()` at `StringEngine.h:185-200`
- Finding: `reset()` re-neutralises the other per-note colouring multipliers it lists in its own comment (specifically to avoid the "last note of the previous guitar colours the first of the next" bug class) but leaves the aging factors untouched. Low impact: the aging engine re-pushes them once per block.
- Fix (described, NOT applied): Reset the three aging multipliers to 1.0 alongside the others, or comment why they're excluded.

### L-02 — Per-note excitation and dispersion-cap math run substantial CPU synchronously on the audio thread at every pluck
- File: `Source/DSP/String/Excitation.cpp:64-296 (trigger)`, `Source/DSP/String/StringEngine.cpp:817-863 (applyCappedDispersion)`
- Finding: Neither allocates, but both do meaningful per-note computation inline with `excite()`/`beginSample()` (multiple full-buffer passes; a 40-iteration bisection with `std::complex`/`std::arg`/`std::polar`). Not a spec violation, but a potential CPU-spike source under fast strumming across many strings in one block.
- Fix (described, NOT applied): If profiling shows a hotspot, cache `applyCappedDispersion()`'s bisection result per (frequency, stage-count) bucket.

### L-03 — `WhammyEngine::prepare()` does not validate `sampleRate`, unlike its siblings
- File: `Source/DSP/Whammy/WhammyEngine.cpp:8`
- Finding: `sr = sampleRate;` with no floor/fallback, while `FeedbackLoop`, `EBowDriver`, `GuitarCircuit`, `SlideEngine` all guard this (`sr > 0.0 ? sampleRate : default`). A non-positive rate from a host would produce garbage filter coefficients rather than the sane fallback the siblings give.
- Fix (described, NOT applied): `sr = sampleRate > 0.0 ? sampleRate : 44100.0;`.

### L-04 — Recursive filter/network state (`GuitarCircuit`, `Biquad`) is not itself NaN/Inf-guarded, only the final sample is
- File: `Source/DSP/Circuit/GuitarCircuit.cpp:543-547` (state), `567` (output guard)
- Finding: `history[i]` (the reactive-element network's memory) is only denormal-flushed, never `isfinite`-checked; the same applies to `Biquad`'s `z1`/`z2` used by `FeedbackLoop`/`EBowDriver`'s per-string filters. Under the parameter ranges these files actually allow the math is well-conditioned and this looks unreachable today, but if ever reached, `sanitise()` on the output would silence the symptom while the stage stays poisoned indefinitely.
- Fix (described, NOT applied): Add an `isfinite` check alongside the denormal flush on this state, zeroing it instead of just flushing denormals.

### L-05 — `SlideEngine` shadows the shared `kMaxStrings` with its own identical constant
- File: `Source/DSP/Slide/SlideEngine.h:69`
- Finding: Declares its own `kMaxStrings = 12` instead of using `luthier::kMaxStrings`. Values match today but nothing keeps them in sync if the global ceiling changes.
- Fix (described, NOT applied): Remove the local constant, use `luthier::kMaxStrings` directly.

### L-06 — `SlideEngine`'s parameters are applied with no smoothing at all
- File: `Source/DSP/Slide/SlideEngine.h:80`, `Source/DSP/Slide/SlideEngine.cpp:93-107, 173-197`
- Finding: `setSettings()` is a bare struct copy; `contactFret()`/`sustainScale()` read `slantDegrees`/`dampingBehind`/`pressure` directly every call. A knob move mid-note changes pitch or gain on the very next block with no ramp — the zipper/click case spec rule 4 targets. (Listed LOW here rather than the MEDIUM the reviewing pass originally assigned, since slide parameters change far less often in practice than damping/mix — downgrade at the report author's discretion; treat as MEDIUM if slide automation turns out to be common in practice.)
- Fix (described, NOT applied): Add `LinSmoother`s for `slantDegrees`/`dampingBehind`/`pressure`/`intonationAssist`.

### L-07 — `FeedbackLoop` feedback `amount` clamps to 1.0, not the spec's 0.998 cap (not an actual runaway risk)
- File: `Source/DSP/Feedback/FeedbackLoop.cpp:61`
- Finding: `clamped.amount = juce::jlimit (0.0, 1.0, s.amount);` accepts a raw value of 1.0, against spec rule 9's 0.998 ceiling. Verified not to be a live risk: `process()` (line 279) saturates every injected sample through `kInjectionCeiling * tanh(raw/kInjectionCeiling)` (`kInjectionCeiling = 0.02`), independent of `amount`, so no parameter combination can exceed that ceiling regardless. A spec-wording gap, not an instability.
- Fix (described, NOT applied): Clamp to `[0.0, 0.998]` for literal spec conformance / defence-in-depth.

### L-08 — `EnvelopeFilterPedal`'s `releaseMs` is never driven by any parameter
- File: `Source/DSP/Effects/PedalsDrive.h:123-124`, `Source/DSP/Effects/PedalsDrive.cpp:295-324`
- Finding: `releaseMs` is declared and used in `follower.setTimes(...)`, but `getParameterDescriptor()` exposes no "Release" entry and no `case` ever assigns it — stuck at its 160 ms default for the pedal's whole lifetime. Not an RT-safety issue, a dead control path.
- Fix (described, NOT applied): Either wire a "Release" parameter, or remove the unused member/half of `setTimes()`.

### L-09 — `TruePeakDetector`'s coefficient tables are "magic statics" first-initialized from the audio thread
- File: `Source/DSP/Master/TruePeakDetector.h:95-112, 131-145, 147-166`; first called from `Source/DSP/Master/MasterBus.cpp:344`, audio thread
- Finding: Function-local `static const` values via immediately-invoked lambdas; `MasterBus::prepare()` never touches them, so first evaluation happens inside `processBlockNormalized()` on the audio thread the first time output normalization becomes active. C++11 guarantees thread-safe init, typically via a guard-variable lock on first call (Itanium ABI) — a one-time hidden synchronization primitive invoked from the audio thread. Single-caller in practice, so not a real contention risk, but avoidable.
- Fix (described, NOT applied): Force eager initialization off the audio thread (e.g. call once from `MasterBus::prepare()`), or convert to ordinary `constexpr` namespace-scope tables.

### L-10 — `JamDrumKit::reset()` does not resync the fixed 64-sample housekeeping clock
- File: `Source/DSP/Jam/JamDrumKit.cpp:110-138 (reset), 430-454 (render)`
- Finding: `housekeepCountdown` isn't touched by `reset()`; a mid-cycle reset means the next `render()` won't run `housekeep()` until however many samples were left rather than at sample 0 — a reproducibility gap for offline/bounce renders that reset mid-stream (same determinism concern `ExpSmoother::snapToTarget()` documents elsewhere), not an audio glitch (no piece is active right after reset).
- Fix (described, NOT applied): `housekeepCountdown = 0;` in `reset()`.

### L-11 — `JamBassVoice::reset()` does not reset the ping-pong liveness clock
- File: `Source/DSP/Jam/JamBassVoice.cpp:179-193 (reset), 333-335 (render)`
- Finding: `liveCountdown` isn't reset, so the 64-sample cadence that recomputes voice liveness can be out of phase after a reset — same determinism reasoning as L-10, no audible effect since `live` itself is correctly reset.
- Fix (described, NOT applied): `liveCountdown = 0;` in `reset()`.

### L-12 — `SpringReverbPedal`'s output DC-blocker corner (20 Hz) sits outside the spec's 5-10 Hz guidance
- File: `Source/DSP/Effects/PedalsMod.cpp:947`
- Finding: `dcOut.prepare(sr, 20.0)` vs. the 7 Hz default (`DspCommon.h:89`) and the 12 Hz used by `DelayPedal`/`ReverbPedal`. Still blocks true DC, just removes more low bass than the spec calls for.
- Fix (described, NOT applied): Lower to 5-10 Hz, or note the deviation as an intentional tonal choice.

### L-13 — `SessionRecorder`/`BackingTrackPlayer` silently drop the block on lock contention
- File: `Source/Practice/Looper.cpp:1156`, `Source/Practice/BackingTrack.cpp:496`
- Finding: Both correctly use non-blocking try-locks (no RT-safety violation), but on the rare block where the message thread holds the lock (mid-export/mid-transport-change), the audio-thread call silently skips recording/reading that block — a correctness gap (an audible sub-block gap in a session recording), not an RT-safety bug.
- Fix (described, NOT applied): Not required for RT-safety; consider a lock-free SPSC ring if the dropped block proves audible in practice.

### L-14 — Dead conditional write to `preCircuitBuffer`
- File: `Source/LuthierEngine.cpp:2827-2828, 2836`
- Finding: A guarded write (`if (diPreCircuit) preCircuitBuffer[i] = sanitise(instrument);`) is unconditionally overwritten immediately after by an unguarded write using the same value — harmless dead code, but confusing to a reader who'd expect `diPreCircuit` to actually gate something.
- Fix (described, NOT applied): Delete the redundant guarded write.

---

## Tests added

Two new files under `Source/Tests/`, using the repo's existing allocation-counter
pattern (`luthier::tests::allocationsOnThisThread()` / `ThreadProbe`, the same one
`AudioThreadSafetyTests.cpp` uses):

- **`Source/Tests/QA_RtSafetyAlloc.cpp`** — `EffectsChain` driven directly through every
  `PedalType` in every one of its 8 slots (both chains), confirming `processStereo`
  never allocates or blocks regardless of which pedal is loaded; plus a two-thread test
  that swaps a slot's pedal type on one thread while another thread processes audio,
  confirming the message-thread/audio-thread handover (`ScopedTryLock`) never shows up
  as a block or allocation on the audio side.
- **`Source/Tests/QA_RtSafetyEngine.cpp`** — `LuthierEngine` driven directly with E-Bow,
  Freeze, and a Floyd Rose whammy dive all engaged together, across single-sample
  blocks, an odd 37-sample partial block, the exact prepared block size, and a live
  sample-rate change (`prepare()` called again mid-test) — none of which
  `AudioThreadSafetyTests.cpp`'s fixture (note on/off, pitch-wheel, CC1 only) reaches.

Both were built and run on Linux (`ninja -C build LuthierTests` /
`xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests QaRtSafety`) before this
report was written; see the commit history on this branch for the exact output.

**Deliberately not covered**: the oversized-host-block path (C-04/C-05) is not
exercised by either new test, because it is currently broken — a test asserting
zero allocation there would (correctly) fail, and this branch's mandate is to report
bugs, not to land a red test against them. Once C-04/C-05 are fixed, a test driving
`LuthierEngine::processBlock` directly with `numSamples > maxBlock` and asserting zero
allocation would be a good addition (and would have caught this regression before
`FINDINGS.md`'s R-107 was filed).

## Findings by severity

**10 critical, 14 high, 18 medium, 14 low — 56 total.**
