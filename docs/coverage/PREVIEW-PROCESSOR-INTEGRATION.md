# Preview processor transport — ABI-UNQUALIFIED private checkpoint

This branch preserves partial production wiring for review, not an integration-ready fix. The separate ABI-neutral backend release retry is qualified independently at `9167c62011ddc2be4305744097adc2618efc3e32` on `codex/luthier-backend-release-retry`; use that isolated commit for coordinator integration.

Base is exact `280e182a8f033457f3d84384b9917a024b1d1ed9`; isolated transport dd36 was replayed as ba6a618a. Private PluginProcessor.h/.cpp wiring and PreviewMidiState are now committed here. The original preview lock and legacy fret conversion remain. This does not remove any of the measured 20 trylocks per callback.

The expanded actual-processor baseline has 10 cases / 31 checks, with 7 failing checks. A private matched new constructor + new test object passes 8 cases after transport wiring; the two backend admission cases then pass with the separate retry fix (10 / 31 total). These results are LIMITED: the 819 other dependencies retain old processor headers. Appending previewState changes sizeof(LuthierAudioProcessor), so other translation units may embed old allocations, stack object sizes or inline offsets. Matching only constructor and new tests does not close that ABI. No whole-host, sanitizer, safe lifetime, release qualification or root merge is claimed for this branch.

Evidence and full original RED logs: `/workspace/luthier-evidence/20261025-preview-processor-integration/`. The pre-build8b WIP archive retains exact initial working files and patch. Frozen 280e resource files were not altered. Backend-only qualification uses original headers/constructor and is documented separately in BACKEND-RELEASE-ADMISSION.md on the isolated backend branch.

Before integration, coherently rebuild every affected dependency from its matched header (including sizeof/new/inline users), or establish an ABI-preserving design with proven callsite compatibility. Then run actual held-note/release/flood/epoch/reprepare/concurrent-producer cases under correctly instrumented dependencies. The incomplete tuning ownership boundary below still blocks moving fret conversion and removing previewLock. A changed-scenario callback probe is also pending; the original whole-processor RT gate stays RED.

## Contract before production code

- Keep every existing public preview signature and channel convention. Keyboard
  notes/chords use channel1; fret preview uses stringIndex+1. Existing valid
  fractional fret inputs remain the legacy converter's responsibility until
  tuning ownership is solved. Do not silently drop unsupported fret commands
  through a guessed snapshot or another string's note.
- Keyboard, fret and audition-stop messages currently share insertion order in
  previewMidi. A separate keyboard buffer drained before/after it can reverse
  successful same-sample commands. The mixed-source test freezes that behavior.
  A partial transport migration therefore needs one serialized ordering handoff
  for all producers, while retaining the legacy fret conversion path/lock. A
  queue implementation alone does not authorize changing that event order.
- Queue producers are control-only and serialized off audio. All attacks and
  releases, both chord methods, stopAudition and stopEverythingButTheEngine need
  explicit handling. The callback never calls producer methods or resets rings.
  FIFO/epoch rules are those already qualified by PreviewCommandQueue tests.
- Audio tracks the exact MIDI note/channel actually accepted into the engine
  input, not what an unrelated current engine string happens to play later.
  A note-off is an obligation until insertion succeeds; a full callback buffer
  must not clear it. Drop new attacks on saturation; retry retained releases.
  A same-address fresh attack cannot bypass its pending release.
- The general MIDI address bound is16channels*128notes=2048, or18432 bytes of
  short releases before other input. A keyboard-only state needs128 addresses
  (1152 bytes). Neither bound guarantees room in a full64KiB buffer. Scans and
  retries must have fixed work bounds; normal host events retain existing
  BoundedMidi policy. Report genuinely dropped attacks separately from release
  attempts that were deferred and remain pending.
- A panic epoch invalidates queued old attacks and cached heads. Release only
  generated preview addresses, not an unrelated host note. Two sources sharing
  the exact MIDI address still share the legacy interpreter's voice semantics;
  no separate per-source synthesizer voice is invented. Audition note tracking
  must reflect accepted phrase events if stopAudition moves to that ownership
  path. Global engine panic is intentionally broader and must acknowledge old
  release obligations without forgetting fresh attacks in the same slice.
- Actual host prepare/reprepare is a quiescent engine lifecycle boundary. Before
  adopting a new engine incarnation, invalidate earlier queued attacks and clear
  audio-owned held state only after the real engine reset. Do not reset SPSC
  storage while producers may post. Mere absence of recent callbacks is not a
  lifecycle boundary; queued commands remain live if callbacks resume without
  prepare. Destruction requires the usual stopped callbacks and joined producers.

## Safe conversion boundary is still missing

Moving the bridge trylock earlier would serialize only ParameterBridge's writers.
It is not sufficient protection for all TuningEngine mutation. Confirmed relevant
production paths include PluginProcessor::applyPartialCapoMask (around799),
PresetManager::applyExtraState (around1060), TabPlaybackTuningSession::begin/end,
and legacy engine tuning/guitar structural setters. PP4241 is a tuning reader
(drainPerformanceCapture), not a writer; the earlier broad-search classification
was corrected. PresetManager is currently owned by the UI state-fixture worker;
this branch makes no changes there.

The resource owner could not certify these writers under the bridge lock.
ScopedStructuralChange's250ms timeout is not quiescence proof. A future safe
boundary must publish an immutable, complete authoritative tuning POD or establish
an audited request/acknowledgement handoff for every writer. It must carry actual
string count, capo mask/fret, temperament/reference pitch and per-string inputs
used by computeFrequency, with a non-reused source generation. Reading a partial
copy of mutable tuning concurrently is not publication. Audio-owned dynamic
pitch/drift layers and structural replacement must have explicit ordering too.

The currently authorized next slice is private keyboard/chord queued ownership,
with legacy fret conversion and lock retained. It must pass all baseline success
cases as well as the new RED cases before consideration. Expected trylocks stay20
until the fret ownership migration is actually qualified; no19-count or green
whole-processor claim is authorized by this partial dependency. Concurrent actual
producer, flood/drop/epoch/lifecycle sanitizer cases and a changed-scenario libc/
pthread probe remain required after real wiring. Main/root integration is held.
