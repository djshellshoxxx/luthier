All rows are OWNED by the release helper (editions split deferred to after feature freeze per §9); no Phase-2 fixes. Pre-existing code the owner must integrate with:

- NOTE `CMakeLists.txt:42-46` (`BUNDLE_ID com.luthieraudio.luthier`, `PLUGIN_MANUFACTURER_CODE Ltha`, `PLUGIN_CODE Lthr`, `PRODUCT_NAME "Luthier"`) and `:107` (`CLAP_ID`) — D-4 keeps these for Pro; only PRODUCT_NAME changes. `LuthierTests` (:213) and `LuthierRender` (:241) targets stay.
- NOTE `Source/Updates/Telemetry.h:301 class License` — placeholder to split into a Pro-only file (see licensing.md).
- NOTE Packaging: `packaging/windows/Luthier.iss` (AppId), `packaging/macos/`, `packaging/linux/deb`, `scripts/ci_build.sh` / `ci_build.ps1` (assume `Luthier.vst3`); no `cmake/` directory exists yet.
- NOTE Several FEAT specs already rely on edition hooks that do not exist here (`edition::Feature` additions in riff-library §11, `edition::limits.snapshotsPerBank` in global-search §12, `Edition.h` in the output-normalization hash, Free style fallbacks in auto-articulation/jam-mode). They will need stubs or will have to wait for this split.
- NOTE `Source/Support/IrLibrary.cpp` resources folder (content folder from `Edition.h`), `Source/Presets/PresetManager` (guard + `savedBy`), `Source/Parameters.cpp` (automatable flag / " (Pro)" suffix in Free).
