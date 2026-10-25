# Bounded native diagnostics draft

Dedicated push branch: `codex/luthier-native-diagnostics`; optional manual run only on that branch. Checkout and build provenance use immutable `github.sha`. Windows x64 Pro and macOS universal Pro run independently in parallel; Windows has reporting priority. A Windows-specific failure cannot skip macOS. Neither ordinary CI nor release acceptance changes.

Reuse existing pinned dependency fetch, configure/build/stage recipes and compiler/dependency caches. Release configuration, LTO OFF. Both platforms build Standalone, VST3, CLAP, Tests and Render. An explicit macOS build-diagnostic step selects the same target recipes except AU; ordinary build/all still builds AU. New explicitly named validator step selects only VST3+CLAP, requires both, uses the existing pinned tools and strictness5; ordinary `validate` still includes AU.

Run the existing unmodified test executable with HostState, GuiReach, JamPlugin, PluginBuses, Tune, routing, MidiOutPanel, ModelGaps and RefPedalRetirement substring filters. Save --list output and reject any requested filter selecting no tests. No skipped GUI, assertion edits or exclusions. All matched Tune/routing suites are included, not an invented smaller representative subset.

Verify arm64 and x86_64 with lipo on each macOS format, Tests and Render. Record actual host/runtime architecture; only that architecture executes. No Rosetta install, signing identity, credentials or Developer ID/notarization claim. Existing recipe ad-hoc signing is for local loadability only.

Each job is bounded to105 minutes; focused tests25 minutes, validation25 minutes. Timeout/failure remains failure. Build-success artifacts are still staged/uploaded after focused/validator failures, with status and logs, source archive/SHA, licensing and SHA256 manifest. Failed builds upload logs only. Artifact names and internal manifest say UNQUALIFIED-DIAGNOSTIC. No release API, tags, publication, installer or signing step.

Not run: full unit suite, strictness10 release validation, macOS AU/auval, execution under the other macOS architecture, Free edition, Linux, complete installer/signing/notarization tests, Render CLI smoke, model acceptance/CPU/alias/soak gates. A green diagnostic is not release acceptance.

This draft must be reviewed by coordinator and host owner before the coordinator pushes the qualified source batch to the dedicated branch. No remote action is authorized by creating this file.

Local validation: ten synthetic-tool/contract tests passed, plus YAML parse and bash syntax checks. No Windows/macOS build, native validator run or remote workflow dispatch has occurred.

The root280 collector is an explicit dependency: scripts/collect_native_failure_evidence.py. ci_build.sh already establishes the per-job marker during deps/build; the workflow also invokes its same `mark` CLI immediately before focused execution (same job key retains the first timestamp). Focus failure alone triggers its `collect` CLI with focused-tests.log. No historical rescue or raw OS report upload; only its sanitized current-job evidence is staged. Focus and collection are separate failing steps: a focus exit139 remains failed if collection also exits17. Neither status is overwritten or converted to success.

Native slice evidence is explicit: the architecture step establishes native hardware architecture, verifies both universal slices, exports the selected native architecture, and records it. Focused listing/execution use `arch -<selected>` and save exact JSON command arrays. Both downloaded macOS validators must contain that slice, then launch with the same arch prefix and record shell-escaped command lines. Both architectures present does not mean both were executed. Missing architecture or either required format fails. POSIX signal11 maps to shell exit139 with the signal recorded, rather than wrapping Python's negative subprocess status.
