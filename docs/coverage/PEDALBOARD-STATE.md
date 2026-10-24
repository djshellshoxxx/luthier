# Pedalboard state coverage (WP4)

This implementation reconstructs the codec from the saved schema and the empty
WP4 skeleton. No prior uncommitted codec was present in this environment.

## Delivered contract

- `BoardState` offers atomic `encode/decode` for typed snapshots and `save/restore`
  for `BoardModel`, with `juce::var` and JSON string overloads. Failed operations
  leave their output/model unchanged. Model snapshots/replacement use WP2's hooks.
- Schema 1 is current; no earlier schema was shipped, so there is no invented
  migration from schema 0. Invalid/missing schema is rejected. A newer positive
  integer schema loads an empty disabled board and preserves the entire block.
- String restore/save preserves a newer block verbatim until a represented board
  value changes. The var API preserves its complete tree, without promising source
  whitespace that the caller has already discarded. `extractFromHostJson` extracts
  the exact top-level `referencePedals` object before generic host parsing. Absent
  blocks succeed with an empty result; malformed hosts/nonobject blocks/oversized
  blocks fail without overwriting the previous result.
- Strict parsing bounds nesting at 64, rejects trailing input/duplicate JSON keys,
  invalid escapes/nonfinite numbers/integer overflow. Integer parsing uses
  `from_chars`; floating parsing uses explicit C-locale `strtod_l`/`_strtod_l`.
  Serialization uses classic-locale streams with `max_digits10` so
  unknown doubles survive repeated string round trips. Current-schema output is
  canonical JSON; unknown field values survive, while their whitespace/number
  spelling may change. Newer-schema string output retains original spelling.
- Validation covers scalar types, integral enums, normalized controls, finite
  values, numeric bounds, up to 16 instances, nonnull valid UUIDs, duplicate UUIDs,
  exact order membership, loop endpoints, feedback direction/runaway opt-in,
  and eight snapshots. Multiple instances of the same model are valid. Conflicting
  automation slots are assigned in authoritative order, reserving other explicit
  slots before filling omissions. MIDI objects call WP6's `MidiState::validate`.
- Known parameters use descriptor IDs and normalized descriptor defaults. Missing
  models are flagged missing and disabled; all original controls/extras/unknown
  nested fields remain available. Audio bypass creation/publication is WP2's job.
- `BoardSettings.unknown` contains a private envelope (`source`, plus `baseline`
  and `verbatim` for forward state). `source` is the original complete tree, used
  as a merge template. Consumers should edit `midi` and `ui` directly rather than
  interpret or rewrite this envelope. The serializer overlays typed controls onto
  cloned source objects; it never mutates the stored template.
- UI window preference bridges legacy root `window` and `ui.window`; root wins
  on decode, UI edits are mirrored on save, and unknown string choices survive.
- `BoardSnapshots::capture/recall/get` provides indices 0..7. The bank persists under
  `referencePedals.snapshots`; individual snapshots contain the board schema and a
  name, without recursively embedding the bank. Capture and recall validate the
  entire resulting state and share the 256 KiB cap.
- `AssetStore` verifies SHA-256 before writes and reads (path checks stream the
  file rather than allocate its full size), uses same-directory
  temporary files plus atomic replacement, keeps prior targets on failure,
  serializes async work on an owned worker, drains jobs at destruction, and
  reports deduplicated missing/corrupt assets. Persistent references use
  `sha256:<digest>` and contain no user path. Loop audio is never emitted by the
  state codec; audio producers store assets and emit their content references.

## Verification

The focused harness compiles actual BoardModel, PedalRegistry, BoardState,
BoardSnapshots, AssetStore, MidiState, and RefPedalStateTests against real JUCE
core/cryptography. Unused package registration functions are discarded with
function-section garbage collection; no fake JUCE or registry implementation is
substituted. The descriptor used for normalization/audio tests is explicitly a
small test model, not a claim about all shipping registered models.

Pre-UI-bridge focused result: **15 cases, 5,245 checks, 0 failures**, in both the normal
and ASan-instrumented harness. A `-Wall -Wextra -Wpedantic` syntax pass over the
state/snapshot/asset implementation produced no warnings. The additional UI
window-preference bridge passes the normal harness: **16 cases, 5,254 checks,
0 failures**.

Reproduce in this saved environment:

```sh
bash /workspace/luthier-evidence/state/run.sh
bash /workspace/luthier-evidence/state/run-asan.sh
```

Harness source, build commands, and results are under
`/workspace/luthier-evidence/state/`. ASan instruments the codec, asset store,
model/registry, MIDI state, and tests; reused JUCE module objects are not ASan
instrumented. The tests include:

- Atomic rejection of malformed/schema/range/type input and the 256 KiB cap.
- Verbatim forward schema and replacement after an explicit board edit.
- Unknown root/nested fields, unknown styles/mounting, missing models, and
  high-precision vendor numbers.
- Duplicate UUID rejection, legal repeated model IDs, and authoritative order with
  conflicting slots.
- All eight named snapshots across save/restore/recall.
- 1,000 deterministically seeded boards of 0..16 known/missing model instances.
- 500 concurrent complete-generation saves while the other thread replaces state.
- Descriptor defaults, keyed parameters, and bitwise-equal output of a deterministic
  registered test model after restore.
- Asset write/hash failure preservation, blocked rename, SHA-256 empty-blob golden,
  missing references, callback delivery and destruction draining.
- Exact host-block extraction with nested decoys, strings containing braces,
  escaped property names, malformed hosts, and oversized blocks.

## Integration limits

The focused harness does not establish full plugin build acceptance, actual-engine
render equality across restore, transition smoothing/PDC, MIDI PC dispatch to
snapshots, UI restoration, or a TSan-clean full host session. These require the
WP2/WP6/host/UI integration gates. Capture/recall use atomic `replaceAll`, whose
current contract clears undo; undoable snapshot command integration belongs to
WP2. Existing unknown-field values are retained semantically; exact lexical bytes
of current-schema unknown JSON subtrees are not promised.

E56 momentary feedback kill: input `feedback.kill` requires a JSON boolean. Current-schema output always writes false so a held control cannot resume on load; unrelated and nested vendor feedback fields remain preserved. Focused normal suite: 17 cases / 5262 checks / 0 failures (2026-10-24).


macOS 10.13 portability correction: floating `from_chars`/`to_chars` calls were removed because recent Apple libc++ marks them unavailable at the supported deployment target. The parser still checks the complete JSON number grammar first, then calls the correctly rounded C-locale conversion without changing process locale. Representable subnormals are accepted even when libc reports ERANGE; nonfinite overflow and nonzero values rounded to zero are rejected. The immutable C locale is process-lived for concurrent saves. Integer charconv remains unchanged. Emission uses a classic-locale stream at max_digits10 and canonical exponent spelling, preserving signed zero and complete double precision.

Focused normal and ASAN/UBSAN suites: 20 cases / 9456 checks / 0 failures. Added 2060 binary values covering every finite exponent, explicit min/max/subnormal/signed-zero cases, bit-exact string round trips and deterministic repeated serialization, strict malformed/overflow/underflow atomic rejection, and both an actual German comma-decimal C locale and a custom C++ comma/grouping facet. Warning compilation is clean on Linux. macOS deployment10.13 compilation and Free/Pro CI remain pending the coordinator's next build; no deployment target was raised. Evidence: `/workspace/luthier-evidence/state/{test.log,portable-asan-full.log}`.

E62 footswitch mode: direct decode and encode validation require `instances[].extras.midi.fsMode`, when present, to be an int/int64 value0..2; double1.0, fractions, booleans, strings, null and out-of-range values fail atomically. Absence stays absent (consumer defaultLatch0); unknown nested MIDI fields survive. This mirrors BoardModel's engine validator without importing its broader checkpoint. Focused normal state suite:21 cases /9495 checks /0 failures.

Exact runtime-extra key helper: `BoardState::canonicalJson(var,String&,error)` exposes the same portable precision-preserving writer without interpreting a board schema. It preserves deterministic property order, distinguishes tiny opaque doubles (1e-200 versus2e-200), int versus double and signed zero, checks finite JSON/depth/256KiB limits, and leaves output unchanged on failure. This lets E55 runtime/recall comparisons avoid JUCE formatting collisions and preserves updated unknown payloads instead of reusing a stale model-specific template. Focused normal state suite:22 cases /9515 checks /0 failures. The portable writer itself previously passed ASAN/UBSAN20/9456; this small public wrapper's additional20 checks are normal-run evidence. Parent/engine owns consuming it in PedalPool.
