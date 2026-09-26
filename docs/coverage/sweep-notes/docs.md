# SPEC-SWEEP notes: docs worker

- [RM-21] make_irs.py seeds from a stable crc32 hash and takes `--out DIR`; the shipped 720 IRs are NOT regenerated — every file would change (they came from a randomised seed) and with them every convolution rig's sound and possibly tuned test thresholds. README states this. Regenerate deliberately in a separate change if wanted.
- [UM-20] Easy-mode data stream paragraph kept — visual constructs `DataStreamDisplay` (RM-30), so the doc becomes true on merge.
- [TS-22] Doc softened to the tested rig instead of adding a whole-factory-bank mono render (the Combo-style per-preset renders are already the slow part of the suite).
- [PT-19/PF-*/KS-22] Doc-only corrections to docs/PLAYING_TECHNIQUES.md, docs/PRESET_FORMAT.md and docs/KEYBOARD_SHORTCUTS.md done here at the coordinator's request; the matching part rows were updated.
- [UM-8] `copyAtoB` now copies the current sound into both slots — from B it used to copy the stored A into B and the next switch lost it; the manual's "copies the current one across" is the spec. Also `recallSlot` now keeps `slotBActive`: the recalled state blob carried the flag it was captured with, so switching A->B flipped it straight back and the next press did nothing.
- [TS-6] Doc corrected rather than code: a truncated `.luthierpreset` is listed (under its file name) and refuses to load with a reason; skipping it at scan time would hide it from the user who is looking for it.
- [JG-18] No processBlock change: `RoutingMatrix::distribute` already writes or clears every enabled non-main bus; a test now proves host garbage never survives.
- [INC-29] LICENCE/MIDI report sections come from a provider the processor installs on Diagnostics (state name and revalidation days only, never the key); no licensing behaviour touched.
- [SP-108] "Remove folder" added; added search folders are still not persisted across sessions (state worker / PresetManager: TROUBLESHOOTING step 5 implies they are).
- [MX-22] Deferred: the 4M-event cap cannot be tested cheaply until `kMaxEvents` is injectable.
- [UM-59/TS-9/INC-28/TS-8] Deferred: hard reset and the read-only factory fallback act on the real Documents/Luthier folder; they need a Documents-root test hook before a test can run them without deleting a developer's diagnostics files.
- [RM-17] CLI smoke test is a CMake script (`scripts/render_cli_smoke.cmake`) registered as CTest `LuthierRenderCli` and run from the test step of both `ci_build.sh` and `ci_build.ps1` (with `check_packaging_paths.cmake`).
- [RM-17] LuthierRender had stopped linking after the REALISM-C/TUNE-HELP merge (PluginEditorOnboarding/Tune.cpp in the engine list; PresetManager now reads UiPreferences). CMake engine list fixed; the CTest smoke test caught it.
- [UM-13] Tab (toggleAdvanced) is refused in Live Mode with an inline notice, matching the header's locked switch.
