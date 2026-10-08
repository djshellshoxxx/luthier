# Codex task board (for the ChatGPT multi-agent coordinator)

Repo: https://github.com/djshellshoxxx/luthier
Base branch for ALL tasks: `claude/luthier-cloud-session-5lzlix`

RULES: read docs/HANDOFF.md and docs/helpers/EXTERNAL_AGENT_BRIEF.md first — they
hold the build/test commands, parameter-marker rules, CRLF rule, and the
one-merger policy. The Claude coordinator is the ONLY merger; subagents push
`codex/*` branches and open DRAFT PRs, never merge, never touch `claude/*`
branches or spec/ files. No Windows/macOS work. Run scripts/fixeol.sh before
each commit. Verify on Linux before each PR:
  scripts/setup_linux.sh            # first time
  ninja -C build -j$(nproc) LuthierTests
  xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests "<filter>"

COORDINATION NOTE for your 3 subagents: Tasks 2 and 3 both add parameters, so
they edit Parameters.cpp/.h, ParameterBridge, and the literal sum in
Source/Tests/IntegrationTests.cpp. Use DISTINCT marker names (below) and
append-only blocks; SERIALIZE the moments those shared files are edited (one
subagent at a time) to avoid clobbering. Task 1 and Task 4 are mostly new-file /
non-parameter and can run fully parallel.

DO NOT ALSO run the single "overnight Codex" prompt on these same branches —
pick one setup. This multi-agent board supersedes it.

---
## Task 1 — branch codex/luthier-tab-export — marker FEAT2-TAB  (parallel-safe)
Also read spec/midi-export.md. NEW files (e.g. Source/Export/TabExport.h/.cpp).
Implement guitar TAB export: (a) ASCII tab — per-string lines, bar divisions,
glyphs b/ /=slide-up \=slide-down h p ~=vibrato PM=palm-mute; (b) if feasible
MusicXML or Guitar-Pro tab. Wire into File -> Export.
ACCEPT: pure function renders a fixed PerformanceScore fixture to deterministic
ASCII tab; test (Source/Tests/TabExportTests.cpp) asserts exact output incl >=1
bend, >=1 slide, >=1 palm-muted note. Coverage: docs/coverage/FEAT2-TAB.md.

## Task 2 — branch codex/luthier-amp-cab-ir — marker FEAT2-AMP  (adds params)
NEW files (Source/DSP/Amp/CabIR.* + UI group). Amp/cab with user WAV IR loading:
partitioned/linear convolution on the main signal; bypass; wet/dry; in/out trim;
mono + stereo IRs. Real-time safe (load off-thread, atomic buffer swap; no
audio-thread alloc/lock/IO). Save/restore with state. GUI "AMP / CAB" group per
gui-integration.md.
ACCEPT: test — known IR convolves a unit impulse to itself (tolerance); bypass
bit-neutral; no audio-thread allocation. Coverage: docs/coverage/FEAT2-AMP.md.

## Task 3 — branch codex/luthier-tuner — marker FEAT2-TUNER  (adds a param)
NEW files for tuner UI + detector. (a) Tuner: pitch detection, note name + cents,
in-tune indicator. (b) Global tuning reference param A=432..446 Hz (default 440)
offsetting all pitches.
ACCEPT: test — synth sine at known freq gives correct note+cents; 440->442
shifts rendered pitch by the expected ratio. Coverage: docs/coverage/FEAT2-TUNER.md.

## Task 4 — branch codex/luthier-ui-scaling — marker FEAT2-UISCALE  (parallel-safe)
User UI scale 75..200% (default 100%), applied to the editor, remembered per
machine (UiPreferences if present, else a properties file). Small marked edit to
PluginEditor; control in Options/View.
ACCEPT: setting scale updates editor transform/bounds; persists across editor
close/reopen (test the persistence path). Coverage: docs/coverage/FEAT2-UISCALE.md.

---
DEFERRED (do NOT take yet — they touch the parameter/automation core broadly and
would conflict with running Claude helpers): midi-learn, randomize+A/B.
When each PR is up, the Claude coordinator (wakes every 4h) tests it on Linux and
merges the green ones. Report per task: branch, files, tests, verification, blockers.
