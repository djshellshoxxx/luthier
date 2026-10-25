# Bounded native diagnostics draft

Dedicated push branch: `codex/luthier-native-diagnostics`; optional manual run only on that branch. Checkout and build provenance use immutable `github.sha`. Windows x64 Pro and macOS universal Pro run independently in parallel; Windows has reporting priority. A Windows-specific failure cannot skip macOS. Neither ordinary CI nor release acceptance changes.

Reuse existing pinned dependency fetch, configure/build/stage recipes and compiler/dependency caches. Release configuration, LTO OFF. Both platforms build Standalone, VST3, CLAP, Tests and Render. An explicit macOS build-diagnostic step selects the same target recipes except AU; ordinary build/all still builds AU. New explicitly named validator step selects only VST3+CLAP, requires both, uses the existing pinned tools and strictness5; ordinary `validate` still includes AU.

Run the existing unmodified test executable with HostState, GuiReach, JamPlugin, PluginBuses, Tune, routing, MidiOutPanel, ModelGaps and RefPedalRetirement substring filters. Save --list output and reject any requested filter selecting no tests. No skipped GUI, assertion edits or exclusions. All matched Tune/routing suites are included, not an invented smaller representative subset.

Verify arm64 and x86_64 with lipo on each macOS format, Tests and Render. Record actual host/runtime architecture; only that architecture executes. No Rosetta install, signing identity, credentials or Developer ID/notarization claim. Existing recipe ad-hoc signing is for local loadability only.

Each job is bounded to105 minutes; focused tests25 minutes, validation25 minutes. Timeout/failure remains failure. Build-success artifacts are still staged/uploaded after focused/validator failures, with status and logs, source archive/SHA, licensing and SHA256 manifest. Failed builds upload logs only. Artifact names and internal manifest say UNQUALIFIED-DIAGNOSTIC. No release API, tags, publication, installer or signing step.

Not run: full unit suite, strictness10 release validation, macOS AU/auval, execution under the other macOS architecture, Free edition, Linux, complete installer/signing/notarization tests, Render CLI smoke, model acceptance/CPU/alias/soak gates. A green diagnostic is not release acceptance.

This draft must be reviewed by coordinator and host owner before the coordinator pushes the qualified source batch to the dedicated branch. No remote action is authorized by creating this file.

Local validation: seven synthetic-tool/contract tests passed, plus YAML parse and bash syntax checks. No Windows/macOS build, native validator run or remote workflow dispatch has occurred.
