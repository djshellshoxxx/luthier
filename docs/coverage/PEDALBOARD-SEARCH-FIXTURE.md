# Search fixture failure and qualification

The original root280 GS06 crashed while copying `*w.item(id)`. Exact unchanged native inputs reproduce exit139; the exact baseline SearchEditor test TU under UBSan reports null reference binding at line710. An actual PresetManager probe reports `/home/agent/Documents/Luthier/Presets/User exists=0 writable=0 mkdir=Read-only file system saved=0 index=-1`. The test continued after its failed save prerequisite. This establishes a harness prerequisite/null-copy fault; it does not establish a product use-after-free.

`ScopedUserPresetFolderForTesting` exists only in native test builds defining `LUTHIER_ALLOCATION_COUNTER`. Production retains the existing Documents path and has no override branch or object-layout change. The scope copies/restores a locked root, requires message-thread construction/destruction, supports nesting and is non-copyable. Tests declare the temporary directory and override before their editor/processor so teardown completes before restoration and directory deletion. PresetManager scanning is synchronous in this source; the preview renderer's destructor signals, joins its thread and releases the renderer. The seam never changes HOME, XDG, user preferences, filesystem grants or OS settings.

GS06 retains every success save/query/delete/stale-activation assertion in a writable temporary fixture. Save/index prerequisites now record a useful failure and return before a null copy. A deterministic missing-result test verifies that the checked copy records a failure and that a retained result survives an actual index refresh. Nested fixture teardown restores both outer and default roots. An ordinary file used as the root deterministically rejects saving and leaves the preset unindexed, without relying on permission bits or elevated privileges.

Qualification on independent root280 native link inputs plus route40f44176 and this fixture slice:

- Full SearchEditor + RefPedalSearch: **31 cases / 8,952 checks / exit0 / 67.1s; no exclusions**.
- GS06 plus missing-result/lifetime and root-reset/failed-save cases: **3 cases / 311 checks / exit0**, native2.6s and scoped ASan/UBSan/LSan4.1s. GS06 executes289 checks, including all prior save/delete/stale activation assertions.
- Sanitizer instrumentation covers PresetManager.cpp, SearchEditorTests.cpp and the already-qualified X11 helper TU. Remaining frozen JUCE/backend/processor/UI/test inputs are uninstrumented. No suppressions, threshold changes, recovery or allocator mismatch overrides. Frozen root280 already includes the qualified paired nothrow allocator hooks.
- Test-macro-absent PresetManager branch is syntax checked separately. No fresh-root product or broad release/hardware/DAW/performance acceptance is implied.

Evidence: `/workspace/luthier-evidence/pedalboard-ui/reference-search-gap/gs06-diagnostic/`. Original native stack, UBSan baseline and denied-save probe are retained with red-manifest.json. Green logs are fixture-native.log, full-search-native.log and fixture-asan-run.log. fixture-qualified-manifest.json records exact executable/log/link-input hashes, and compile/link commands are adjacent. The immutable fixture-snapshot source is retained; tracked CRLF normalization is equivalent after newline normalization.

This resolves the GS06 gate recorded in the first route contract for the qualified snapshot. The earlier20 legacy GS02 no-control diagnostics, independent full-project gates, named-model support, wall-time paint failures and manual UI/DAW acceptance remain separate.

Integration note: the coordinator keeps the byte-identical fixture class in
Tests/PresetFolderTestSupport.h rather than the public PresetManager.h. The
production header and its timestamp are untouched; only the test TU and the
conditional PresetManager.cpp include it. This avoids unrelated recompilation
and leaves the independently reviewed fixture behavior unchanged. Fresh root
build/31-case qualification is required after this small file-boundary adaptation.
