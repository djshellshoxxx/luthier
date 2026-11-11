# F-C1 legacy rack pedal use-after-free: state at wind-down

Worktree /home/user/lw/rt-uaf, branch agents/luthier/audio/effects-chain-retire, base ca14934e.
STATE: confirmed by reading the code, NOT fixed, NOT reproduced (nothing was built or run; the one build I started was stopped while it waited for the machine lock).

## Ownership
`EffectsChain::Slot::pedal` (std::unique_ptr<Pedal>, Source/DSP/Effects/EffectsChain.h:93) owns the pedal. `retired` (h:115) is a vector that `setSlotType` clears straight after the swap (cpp:109), as do `clear()` (cpp:235) and `prepare()` (cpp:34). Nothing is ever kept back for the audio thread.

## Readers / writers and their real synchronisation
| Path | Thread | Sync on slot.pedal | Verdict |
|---|---|---|---|
| setSlotType swap (EffectsChain.cpp:96-106) then free (cpp:109) | message (also host thread via setStateInformation) | ScopedLock swapLock for the swap only; the free runs after the unlock | WRITER |
| processStereo (cpp:285) | audio, LuthierEngine.cpp:3486/3552 | ScopedTryLock swapLock, re-reads under lock | safe |
| applySlotState (cpp:145), from bridge.applyToEngine Parameters.cpp:2006 | audio, under the engine try-lock | try-lock swapLock | safe |
| resetFromAudioThread (cpp:51) via panic LuthierEngine.cpp:1307/1309 | audio | try-lock | safe |
| getLatencySamples (cpp:268-277) | audio: PluginProcessor.cpp:2417 updateLatency -> LuthierEngine.cpp:3740-3741, routing report 3772/3779 (runs even when the engine try-lock at PluginProcessor.cpp:1948 failed); feedbackLoop LuthierEngine.cpp:3130; message: the timer at PluginProcessor.cpp:4566 | NONE | DEFECTIVE |
| setExpression (cpp:248) | audio, every block, LuthierEngine.cpp:3469-3470 (under the engine lock only) | NONE | DEFECTIVE: the adopt path takes no engine lock |
| setTempoBpm (cpp:239) | audio, every block, PluginProcessor.cpp:1705 (before the engine try-lock) -> LuthierEngine.cpp:1269 | NONE | DEFECTIVE |
| setOversamplingFactor (cpp:257) | audio, applyQualityForBlock PluginProcessor.cpp:4944-4968 -> LuthierEngineQuality.cpp:32-33 (engine try-lock) | NONE | DEFECTIVE with adopt |
| setSlotBypassed / setSlotMix (cpp:166/185) | no production callers (tests only) | NONE | latent |
| moveSlot (cpp:196) | test only (EngineTests.cpp:1246) | swapLock; nothing is freed | no UAF, but a formal data race for unlocked readers |
| getPedal: UI PedalRack.cpp:65,169,435, EasyPanel.cpp:51, Parameters.cpp:2428,2648 | message | none; same thread as the writer | safe while writers stay on the message thread; unsafe if setStateInformation runs on a host thread |
| Writer paths: PresetManager.cpp:970-971 fromVar -> onPedalTypesLoaded (PluginProcessor.cpp:150) -> adoptPedalTypesFromParameters (Parameters.cpp:2394-2419) -> setSlotType :2415 | message/host | NO engine lock | |
| applyStructural pedal loop, Parameters.cpp:2621-2633 | message, engineLock held (handleAsyncUpdate/applyAllNow :2461/:2469) | engine lock, but the getLatencySamples/setTempoBpm readers run outside it | still DEFECTIVE |

## Sibling paths (verdicts from reading only)
- Cabinet / mic IR (CabinetEngine.cpp:427/481 load under the convolutionLock SpinLock; audio side try-locks at :565/:590; the Convolution objects are created only in prepare): no object is freed under the audio thread. Safe (clicks: see F-C2).
- Body IR (BodyEngine.cpp:385/439 vs try-lock :511): safe, same pattern.
- Room (RoomEngine.cpp:121-138, rebuild :253): mutated in place with no reallocation (sized in prepare), under the engine lock. Safe.
- Amp model / oversampling (AmpEngine.cpp:111, :329): the twin is allocated in prepare and parameters change in place. Safe.
- Doubler: now a post-amp rack pedal, so it is covered by F-C1.
- Part swap: retiredPartSwap.store at LuthierEngine.cpp:2580 can overwrite an earlier swap that was never freed (a leak, not a UAF). Not traced.

## Heap-corruption hypothesis
Unlikely. Fuzz.convolutionPreparesSafelyForTinyHostBlocks and presetLoadAtTrulyExtremeBufferSizes (Source/Tests/QA_Robustness.cpp:399/427) call fromVar and processBlock on the same thread, so setSlotType never runs at the same time as a reader. The block-size 1-4 dependence also points away from F-C1. Not run.

## Intended fix (not implemented)
- Add `std::atomic<Pedal*> live` to each slot (published with a seq_cst store under swapLock). Make bypassed, mix, type, tempo, expression and the factors atomics.
- Add an `unlockedReaders` counter. The unlocked readers (getLatencySamples, setTempoBpm, setExpression, setOversamplingFactor, setSlotBypassed, setSlotMix) do a seq_cst fetch_add before loading any `live` and a release fetch_sub after their last use. The locked readers keep using `pedal` under swapLock.
- `reclaimRetired()` runs on the message thread only. Under swapLock it frees `retired` (outside the lock) only if `unlockedReaders` reads 0 (seq_cst); otherwise it defers.
- Invariant: a pedal is freed only after it has been unpublished from every `live` slot and the counter has afterwards been seen at zero, so any reader that could have loaded it has finished. The audio thread never frees and takes no new lock.
- It is called from setSlotType, clear, moveSlot, prepare, the destructor, and LuthierAudioProcessor::timerCallback (PluginProcessor.cpp:4563, one line). With audio stopped the counter is 0, so frees are immediate and nothing leaks.
- moveSlot is rewritten field by field and republishes `live`.
- Tests: the stress file below under ASan for the RED; a deterministic rule test (a reader scope held, then setSlotType, then retired==1; release, reclaim, retired==0; audio stopped, freed at once).

## Files
Source/Tests/EffectsChainRetireStressTests.cpp (new, uncompiled). It has two stress tests: a processor with fromVar loads plus 3 latency-probe threads, and an engine with direct setSlotType swaps. It uses only the existing API, so it is meant to show RED under ASan before the fix.
