# Actual processor preview integration — private RED checkpoint

This is a test-first checkpoint, not a production fix. Private worktree
`host-preview` starts at exact280e182a8f033457f3d84384b9917a024b1d1ed9 with the
dd36 transport replayed asba6a618a. PluginProcessor.h/.cpp remain unchanged.
The new registered PreviewProcessor cases link819 verified immutable build6
objects plus their test object, using matching280e headers and constructor.

Actual baseline: **5 cases /18 checks, 2 failures, exit1**. Queued keyboard/chord
attacks replay after releaseResources/reprepare; a keyboard release is lost with
65529 of65536 host MIDI bytes occupied by unrelated note-offs. Ordinary
keyboard/chord/panic voicing, mixed keyboard-then-fret ordering and release
isolation from a different host note pass. Exact commands and complete output:
`/workspace/luthier-evidence/20261025-preview-processor-integration/`.
The resource link targets the coordinator's preserved280e products, not mutable
build7 outputs. No production mutation or sanitizer/RT acceptance is implied.

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
