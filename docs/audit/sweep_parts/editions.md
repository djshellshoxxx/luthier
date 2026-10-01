## editions.md

The editions split is deferred by design: §0 says "Nothing here is implemented yet" and the coordinator does the split after feature freeze (§9). This checkout has no `LUTHIER_EDITION` CMake option, `cmake/Editions.cmake`, `Source/Edition.h`, `ProFeatureGuard`, `ProLockedPanel`, `EditionTests.cpp`, `packaging/content/free.txt`, `Resources/Demos/` or Free installer config (re-verified). The current build is effectively Pro with today's IDs, which is what D-4 wants to keep. All rows are DEFERRED to the release helper.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| ED-0 (§0) | Ground rules: same engine/sound, remove headline features only, never gate a11y/safety/privacy, never rewrite user data, compile-time gating, side-by-side install, one upgrade path, quiet upsell | - | - | - | DEFERRED |
| ED-1 (§1) | Headline features H1-H8 Pro; D-1 realism engines run in Free at preset values | - | n/a | - | DEFERRED |
| ED-2.1 (§2.1) | Instrument/engine feature table (6 Free guitars, Pro Workshop/deep panels/slide/slap/techniques/microtonal/advanced ranges, Free-limited strings/fingerstyle/pick noise/squeak/strum) | - | - | - | DEFERRED |
| ED-2.2 (§2.2) | Rig table (7 of 13 amps, 4+4 rack slots, 15 of 22 pedals, ~130 IRs, one user IR, Tone Match Pro) | - | - | - | DEFERRED |
| ED-2.3 (§2.3) | Presets/workflow table (16 of 36 presets, 1x4 snapshots, setlists Pro, mod matrix limits, 8 genre kits, Tune Builder Pro with demo playback, controller profiles) | - | - | - | DEFERRED |
| ED-2.4 (§2.4) | Practice/notation/export/routing table (looper 1 layer 60 s, trainers Pro, notation/MIDI export Pro, audio export 5 min, Layouts B-D Pro) | - | - | - | DEFERRED |
| ED-2.5 (§2.5) | Platform table (all formats both, licensing Pro only, content packs by edition) | - | - | - | DEFERRED |
| ED-3 (§3) | Free content counts and `packaging/content/free.txt` manifest read by `ci_build.sh stage` | - | - | - | DEFERRED |
| ED-4 (§4.1-4.5) | Locked marking (muted + lock glyph, never hidden, "More in Luthier Pro" row, tooltip, SR text), non-modal upsell panel, pre-rendered demos, Free onboarding page, frequency limits | - | - | - | DEFERRED |
| ED-5 (§5.1-5.4) | Pro files in Free: `ProFeatureGuard` detection, effective values, banner, verbatim write-back + `savedBy`, ranges carried, per-file-type behaviour, host sessions by plugin ID + "Import Luthier Free state" | - | - | - | DEFERRED |
| ED-6 (§6, D-4) | Edition identity table (names, codes, bundle/CLAP ids, AppId, content folder, config scoping, update channel, installer names); Pro keeps today's IDs — Free identity absent; current IDs at `CMakeLists.txt:42-46,107` | - | - | - | DEFERRED |
| ED-7 (§7.1-7.6) | Single CMake option + `Editions.cmake`, `Source/Edition.h` (`Feature`, `Limits`, `isFreeAmp/Pedal`), identical parameter layout with non-automatable " (Pro)" params, `ProLockedPanel` factories, neutralisation on message thread, CI edition matrix, staging, installers | - | - | - | DEFERRED |
| ED-8 (§8) | Maintenance rule: every spec states its edition and adds a §2 row — process rule, not code | - | - | - | DEFERRED |
| ED-9 (§9) | Fork step file list (Editions.cmake, Edition.h, ProFeatureGuard, ProLockedPanel, EditionTests, free.txt, Demos, render_demos.sh, Free .iss, docs/EDITIONS.md) and changed files | - | - | - | DEFERRED |
| ED-T (§10.1-10.7) | Both editions build/pass pluginval & clap-validator, no Pro symbols in Free, Pro presets in Free round-trip, Free presets null in Pro, side-by-side install, locked-element soak, identical parameter layout | - | - | - | DEFERRED |
| ED-Q (§11) | Open questions Q-1..Q-8 for the product owner | - | - | - | DEFERRED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 DEFERRED=16 -->
