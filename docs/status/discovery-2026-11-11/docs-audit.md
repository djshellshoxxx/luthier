# Docs gap audit (I = ca14934e, read-only)

## 1. PR #78 (origin/docs/readme-and-wiki, 9842fa61, base 0f62c3d6 = master)
- Diff vs master: README.md (+/-), docs/wiki/{Home,User-Guide,Developer-Guide,Architecture-and-Design-Rationale,Codebase-Map,_Sidebar}.md (253+/59-).
- `git merge-tree --write-tree I origin/docs/readme-and-wiki`: ONE conflict, `README.md` (docs/wiki/* add cleanly). Cause: I edited README.md lines 1-16 (new "software artwork" intro, pedalboard paragraph linking docs/coverage/PEDALBOARD-ACCEPTANCE.md); #78 rewrote the whole file.
- Accuracy of #78 for I:
  - No pedalboard mention anywhere (README, User-Guide, Codebase-Map). Codebase-Map has no `Source/ReferencePedals/` row, no `spec/reference-pedalboard/`. STALE for I.
  - User-Guide.md "23 pedals across two racks" and "36 factory presets": counts copied from spec/README.md:102/161; not verified here (UNVERIFIED); omits the six-model pedalboard scope (docs/coverage/PEDALBOARD-ACCEPTANCE.md:3).
  - Formats/build steps: correct vs CMakeLists.txt:89-91 (VST3+Standalone, AU on APPLE), :47-56 and :234 (CLAP when ThirdParty/clap-juce-extensions present). Option list plausible (UNVERIFIED line-by-line).
  - README install table is Pro/Free-neutral and differs from docs/TROUBLESHOOTING.md:16-21 (system-wide `Luthier Pro.clap` paths, Free naming). Wiki says CLAP absent from install table.
  - #78 README drops the screenshots gallery (docs/screenshots/*) that master/I keep; drops docs/MIDI_EXPORT_LUTHIER_PROFILE.md row.
  - Wiki links use `/blob/main/` (User-Guide.md) while docs/wiki-draft uses `/blob/master/`; default branch is master (UNVERIFIED which). Possible broken links.
- Resolution advice: take #78 README, re-add I's pedalboard paragraph; add pedalboard row to Codebase-Map.

## 2. Gap table
| Topic | Status | Evidence |
|---|---|---|
| Installation per OS | EXISTS-STALE | README.md on I has no install table (only #78 has). docs/TROUBLESHOOTING.md:8-25 has paths incl. CLAP; docs/RELEASING.md:71-83 has table; spec/README.md:84-86 has VST3/AU/Linux but no CLAP. KNOWN_ISSUES.md:140-143 wrong (see 3). |
| Getting started | EXISTS-CURRENT | docs/USER_MANUAL.md:19; docs/wiki-draft/Getting-Started.md (draft, not published) |
| Pedalboard guide | MISSING (user); dev material EXISTS | USER_MANUAL.md:203 only "Pedalboard (before the amp)" = legacy rack. Nothing user-facing for Source/ReferencePedals. Dev: spec/reference-pedalboard/{ARCHITECTURE,CATALOG,HANDOFF}.md, docs/PEDALBOARD-CONTINUATION.md, docs/coverage/PEDALBOARD-*.md (ledger, not a guide). |
| Effects reference, six named pedals (Phase90, CE-2, DD-3T, CE-2W, Dyna Comp, RC-5) | MISSING as user docs; specs EXIST | spec/reference-pedalboard/specs/pedals/{mxr-phase90-script,boss-ce2,boss-dd3t,boss-ce2w,mxr-dynacomp,boss-rc5}.md. Scope: docs/coverage/PEDALBOARD-ACCEPTANCE.md:3-8 ("no selected named model is registered yet" at 10-26; later entry says RC-5 wrapper v1 implemented - registration state UNVERIFIED). |
| Legacy pedal racks reference | EXISTS-CURRENT (count unverified) | spec/README.md:102 (23 pedals, two 8-slot racks), USER_MANUAL.md:203 |
| Guitar workshop guide | EXISTS-CURRENT | USER_MANUAL.md (Workshop sections), docs/GUITAR_PHYSICS.md |
| Presets | EXISTS-CURRENT | USER_MANUAL.md:351, docs/PRESET_FORMAT.md |
| Plugin paths per format | EXISTS-STALE | TROUBLESHOOTING.md:16-21 current; spec/README.md:84-86 lacks CLAP; wiki #78 lacks CLAP |
| Host compatibility matrix | EXISTS-STALE / over-claims | docs/HOST_COMPATIBILITY.md:3-6 says intended, not tested (honest). But docs/PRODUCT_DESCRIPTION.md:102 says "Tested in Ableton, Logic, Cubase, Studio One, Reaper, FL, Bitwig, Pro Tools"; KNOWN_ISSUES.md:160-162 says the host matrix "has not been carried out"; HOST_COMPATIBILITY.md:61 Pro Tools needs AAX (no AAX built). No tested-versus-claimed column exists. |
| Troubleshooting | EXISTS-CURRENT | docs/TROUBLESHOOTING.md (12 sections); no pedalboard section |
| Architecture | EXISTS-STALE | spec/README.md:~150-170 (source layout, no ReferencePedals); spec/reference-pedalboard/ARCHITECTURE.md current for board; docs/wiki-draft/Architecture.md:9 cites `Source/GUI/` and `Source/WIP/` which do not exist (real: Source/UI/) |
| DSP / real-time rules | EXISTS-CURRENT | spec/README.md "Rules the DSP obeys"; spec/engine.md; spec/JUCE_CLAUDE_GUIDELINES.md |
| Pedal implementations (dev) | EXISTS-CURRENT | spec/reference-pedalboard/specs/pedals/*.md (18 files), GENERIC_PEDALS.md, IMPLEMENTATION_PLAN.md |
| Parameter mgmt, append-only rule | EXISTS but scattered | spec/reference-pedalboard/specs/BOARD_CORE.md:164-168 (ref_s{nn}_* IDs append-only, lane order append-only); Source/Parameters.h:485, Parameters.cpp:965 (jam params); docs/wiki-draft/Contributing-and-Status.md:5 and docs/HANDOFF.md. No single dev-facing rule doc; not in README/wiki #78 Developer-Guide. |
| GUI architecture | EXISTS | spec/gui-integration.md, spec/ui-wiring.md, docs/THEME_AS_BUILT.md; reference-pedalboard/specs/UI_BOARD_STYLES.md |
| Plugin formats | EXISTS-STALE | KNOWN_ISSUES.md:140; docs/RELEASING.md:10 current |
| Build system | EXISTS-CURRENT | README.md, docs/RELEASING.md:27-70, docs/wiki-draft/Build-by-OS.md |
| Testing guide | EXISTS (thin) | spec/README.md "Tests", docs/wiki-draft/Testing.md; docs/HANDOFF.md:55 "Current state (2026-09-26)" stale (last touched 2026-10-07) |
| Benchmark method | PARTIAL | docs/perf/OPTIMISATION_LOG.md only (log, not method); spec/cpu-quality-modes.md; test gates in commits (test(ui-perf)). No method doc. |
| Release procedure | EXISTS-STALE | docs/RELEASING.md duplicate section numbers: "## 9" at :206 and :255; "## 10" at :241 and :300 (two docs merged). |
| Contribution guide | MISSING | no CONTRIBUTING.md in repo root; #78 README has 4-line "Contributing"; wiki-draft/Contributing-and-Status.md. |
| CLI reference | EXISTS-STALE | see section 2a |
| Changelog currency | EXISTS-STALE | docs/CHANGELOG.md last edited 2026-10-16 (head "0.1.0-beta"); git log has 2026-10-27/28/11-04 work (RC-5, CE-2 gate, Dyna Comp, Phase90, ci digest) and no pedalboard entry (grep "pedalboard" CHANGELOG.md: 0 hits). |
| TODO currency | EXISTS-STALE | spec/TODO.md last commit 2026-10-07; header :9-11 own "Triage note (2026-10-01)" admits many unticked boxes; no pedalboard rows (ledger is docs/coverage/PEDALBOARD-ACCEPTANCE.md). |
| docs/standards/CDL_PLUGIN_BASELINE.md | EXISTS-CURRENT | 233 lines, reviewed 2026-10-06; shared standard, not Luthier-specific |

### 2a. luthier-render flags: Tools/RenderCli.cpp:216-245 vs docs
Parsed: --help/-h --midi --out/-o --preset --preset-file --guitar --audition --rate --depth --tail --tempo --verbose/-v --list-presets --list-guitars --list-phrases --calibrate-factory --export-riffs --profile --render-previews --write-rhythm-resources --convert --convert-batch --to --inspect --validate --transpose --retune --no-clobber --list-formats --normalise/--normalize [dB].
- spec/cli-tools.md documents only: --audition --calibrate-factory --convert --convert-batch --export-riffs --inspect --list-formats --list-presets --midi --no-clobber --out --preset --render-previews --retune --to --transpose --validate.
- In code, NOT in spec/cli-tools.md: --preset-file --guitar --rate --depth --tail --tempo --verbose/-v --list-guitars --list-phrases --profile --write-rhythm-resources --normalise/--normalize --help/-h.
- Documented but not parsed: none found. spec/README.md:221-227 shows --midi --preset --out --audition --guitar --verbose --list-presets --help (all parsed). wiki-draft/Getting-Started.md:25 says "see RenderCli.cpp" (no table). No flag reference table exists anywhere; --help text not compared (UNVERIFIED).

## 3. Contradictions (docs vs code)
1. docs/KNOWN_ISSUES.md:140-143 "CLAP and AU builds... only VST3 and Standalone are built and tested... CLAP needs the CLAP wrapper added" vs CMakeLists.txt:47-56,234-237,275-280 (Luthier_CLAP built when ThirdParty/clap-juce-extensions present), CMakeLists.txt:89-91 (AU appended on APPLE), scripts/ci_build.sh:9-16,39 (CLAP pinned commit 55525c98, validate with clap-validator), docs/RELEASING.md:10,67-69.
2. KNOWN_ISSUES.md:140 "Not yet implemented" lists "Linux builds" at :145 yet says "Linux is built and tested" (self-contradiction in section).
3. KNOWN_ISSUES.md:6 "Version 1.0.0" vs CMakeLists.txt:3 `project(Luthier VERSION 0.1.0` and docs/CHANGELOG.md:3 "0.1.0-beta".
4. KNOWN_ISSUES.md:156-160 "pluginval 1.0.3 passes at strictness 10" vs README.md/RELEASING.md:10 "strictness 5 on push/PR, 10 nightly" (README on I text) - no log path cited.
5. docs/PRODUCT_DESCRIPTION.md:102 "Tested in Ableton, Logic, Cubase, ... Pro Tools" vs KNOWN_ISSUES.md:160-162 (matrix not carried out) and HOST_COMPATIBILITY.md:3-6; also HOST_COMPATIBILITY.md:56-58 says AAX not built (Pro Tools unsupported) contradicting product description.
6. docs/PRODUCT_DESCRIPTION.md:11,148 lists AU on Windows/Linux-neutral "VST3, AU, CLAP, Standalone on Windows, macOS, Linux" vs CMakeLists.txt:89-91 AU macOS only.
7. docs/HANDOFF.md:7 says integration branch `claude/luthier-consolidate`, :55 "Current state (2026-09-26)"; git log shows 2026-10-28 native Windows Free/Pro evidence commits ("docs(handoff): native Windows ... 97d3b013"). wiki-draft Home/Testing repeat "Windows and macOS paused" from it.
8. docs/wiki-draft/Architecture.md:9 `Source/GUI/`, `Source/WIP/` - neither exists.
9. spec/README.md:84-86 install table omits CLAP; TROUBLESHOOTING.md:16-21 includes it.
10. README.md (I, line ~11) says pedalboard "in development"; spec/README.md:102,161 still state "23 pedals" with no mention of the board. Not a contradiction but incomplete.

## 4. claude.md
- Title "Interference VST specification workflow"; body uses `rg --files specs -g '*.md'` and `docs/spec-coverage.md`. `specs/` does not exist at repo root (this repo uses `spec/`; `docs/specs/` exists with one file TODO_SPECS_AND_BUILD_PLAN.md).
- Added 2026-09-23 by Sheldon Davidson in commit "WIP: load guitar types as parts guitars and retire pickup placement params" (not a docs commit).
- Verdict: very likely mis-copied from another project ("Interference"); it is wrong for this repo (running its inventory command returns nothing). The `docs/spec-coverage.md` convention is real here (3100 lines), so partial overlap. Not certain of origin: UNVERIFIED.
- References (outside skills/): spec/reference-pedalboard/HANDOFF.md, docs/specs/TODO_SPECS_AND_BUILD_PLAN.md, docs/coverage/PRO-V01-RELEASE.md mention "claude.md". No build/CI references. (Case-insensitive grep also hits skills/claude-token-efficient/* which refer to the generic CLAUDE.md, not this file.) Contents also instruct agents ("Before writing code: run...") - evidence only, not followed.

## 5. Claims of verified/tested/passing without evidence pointer
- KNOWN_ISSUES.md:145 "Linux is built and tested"; :156 "pluginval 1.0.3 passes at strictness 10 ... re-run 2026-09-19" (no log/artifact path); "twenty-five suites, no failures".
- PRODUCT_DESCRIPTION.md:102 "Tested in Ableton, Logic ..." (contradicted).
- README.md/spec/README.md: "validates the plugin with pluginval and clap-validator" (CI description; no run link).
- docs/CHANGELOG.md:3-7 "verified executable architectures", "Publication remains blocked until ... PRO-V01-RELEASE.md passes" (points at ledger: OK, but "verified" itself uncited).
- spec/README.md:252 "preset round trip verified by comparing rendered audio" (no test name).
- docs/HOST_COMPATIBILITY.md "Behaviour common to every host" table cites test names (GOOD example); per-host rows say intended, not tested (honest).
- docs/wiki-draft/Testing.md reports "Linux integration green as of 2026-09-26" (cites HANDOFF.md; dated).
- Not checked: docs/audit/*, docs/coverage/* internals (UNVERIFIED).

## 6. Prioritized edits
1. docs/KNOWN_ISSUES.md:140-146 rewrite: CLAP built when ThirdParty/clap-juce-extensions present (CMakeLists.txt:47-56,234); AU built on macOS (CMakeLists.txt:89-91); move "Linux builds" out of "Not yet implemented". Fix :6 version to 0.1.0 (CMakeLists.txt:3). Add pluginval log reference or drop the claim (:156).
2. docs/PRODUCT_DESCRIPTION.md:102,148: remove "Tested in ..." list; say "validated with pluginval/clap-validator in CI" and AU macOS-only; Pro Tools/AAX not supported (HOST_COMPATIBILITY.md:56-58).
3. Resolve PR #78 conflict in README.md: take #78, re-add I's pedalboard paragraph (README.md:11-12 on I), keep screenshot gallery or link; fix `/blob/main/` links; add CLAP row to Install table (TROUBLESHOOTING.md:16-21); add Codebase-Map row for `Source/ReferencePedals/` and `spec/reference-pedalboard/`; update "23 pedals" line to describe legacy racks plus the in-development board.
4. NEW docs/PEDALBOARD (user guide): what the board is, six scoped models, release status from docs/coverage/PEDALBOARD-ACCEPTANCE.md:3-8, per-pedal pages from spec/reference-pedalboard/specs/pedals/*.md. Mark registered-model state only after checking Source/ReferencePedals/PedalRegistry.cpp (UNVERIFIED).
5. docs/CHANGELOG.md: add entries after 0.1.0-beta for pedalboard WPs, RC-5 wrapper, CE-2 gate left RED, Phase90/Dyna Comp CPU work, CI native-crash digest (git log 2026-10-27..11-04).
6. spec/cli-tools.md: add table of base flags from Tools/RenderCli.cpp:216-245 (--preset-file --guitar --rate --depth --tail --tempo --verbose --list-guitars --list-phrases --profile --write-rhythm-resources --normalise [dB] --help); also reference from spec/README.md:221.
7. claude.md: delete or replace with Luthier workflow (spec/ not specs/, docs/spec-coverage.md); check the three referencing docs first.
8. docs/HOST_COMPATIBILITY.md: add "tested / claimed / date / evidence" columns; currently none tested per KNOWN_ISSUES.md:160.
9. docs/HANDOFF.md:55 refresh state (2026-10-28 Windows evidence, PR #85 integration branch); sync wiki-draft/Home.md and Testing.md which copy "paused".
10. docs/RELEASING.md: renumber duplicate sections (:206/:255 both "9", :241/:300 both "10").
11. NEW CONTRIBUTING.md (or Developer-Guide section): consolidate append-only param rule (BOARD_CORE.md:164-168; Parameters.h:485; update literal count in Source/Tests/IntegrationTests.cpp per wiki-draft/Contributing-and-Status.md:5), spec-first process, fixeol.sh (HANDOFF.md:18).
12. docs/wiki-draft/Architecture.md:9: `Source/GUI/` -> `Source/UI/`, drop `Source/WIP/`; decide whether wiki-draft or #78 docs/wiki is canonical (two overlapping wiki sets).
13. spec/TODO.md: refresh or mark superseded (header :9-11); add pointer to pedalboard ledger.
14. Add benchmark-method doc (docs/perf/OPTIMISATION_LOG.md is a log only).
15. TROUBLESHOOTING.md: add pedalboard section once shipped.
