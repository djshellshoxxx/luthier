# Pedalboard native diagnostic handoff

Updated2026-10-25UTC. Sole integration owner remains this existing Codex task.
No other repository, new Codex environment, force push, release overwrite or
timer was used. Claude's stopped preservation ref remains1f646a65.

## Exact frozen source and actual native run

Native Actions source: `6d9b5943217d6e0872f65aea6514ebf40fc789e1`, verified on
both `codex/luthier-native-diagnostics` and
`codex/luthier-checkpoint-20261025-build8b` at initial push.
Run: https://github.com/djshellshoxxx/luthier/actions/runs/40198638051
Windows job121184864851 and macOS universal job121184864619 were confirmed
in_progress, not merely requested. Windows was setting up MSVC; macOS was
building at that read. Re-query the run for current results; this file does not
claim either native compile/test/validator/package has passed.

Do not push again to the native branch just to update a handoff: its concurrency
policy cancels older runs on that branch. Keep the current run's source frozen.
Push documentation/continuation checkpoints separately. PR80 remains draft at
b5bec374; do not redispatch an old-head or speculative six-cell matrix.

The native workflow builds Pro Windows x64 and macOS arm64+x86_64 independently,
with focused unchanged tests, strictness5 VST3/CLAP validators, architecture
checks and logs. It retains explicitly UNQUALIFIED-DIAGNOSTIC binaries, source,
checksums, licenses, installation references, signing truth and outcomes even
after focused failures. It has no release/tag/publication action. Full unit,
strictness10/LTO release, Free edition, installer, second-architecture execution,
hardware/DAW/manual and signing/notarization gates remain separate.

## Fresh combined Linux evidence

Build7 at2fb21cab completed all five targets, actual CLAP18pass/0fail/3unsupported
skips, actual VST3 strictness5 all30 requested rate/buffer combinations with GUI
and automation. Engine155/213330, state112/26911, MIDI90/2236, UI12/7633 and
DSP15/1679 selections passed. Source-specific ledgers retain overlaps rather
than inventing an aggregate. Original35dB decay now passes all25 instruments.

Build8 atd86a03db FAILED before runtime because the coordinator's separated
test-support header referenced absent JuceHeader.h. Failure log/exit are retained.
5d2f4e58 fixes only that include to the explicit juce_core module. Fresh build8b
then completed all five targets,exit0. Compiled source:
`5d2f4e585318970aea3264eb686256e4156b7370`; Source tree:
`9f790ebe860c680cc989f461ad7355ea2a772892`.
Observed6d9 differs only in two artifact Python scripts/fixture README metadata;
Source/CMake/ci_build remain byte-identical. It is not a docs-only delta.

Actual build8b executable SHA256:
`fb57ec3720eded46380214b793ad5c6d7c97530554bd3393cec875a2c247b137`.
Actual CLAP18pass/0fail/3unsupported skips,zero warnings,exit0. Actual VST3
strictness5,seed0x51a7,all30 rate/buffer pairs through192kHz with GUI/automation:
SUCCESS,exit0. Exact hashes/logs:
`/workspace/luthier-evidence/20261025-host-validators-5d2f4e58-build8b`.

Actual engine/history/API/memory/host selection161/219579 plus original300s
grind1/8 passed:30,000 attempted edits,24,759 rejected,559,026blocks,
246,623,637samples,callback_new0,invalid0. Actual full Search+RefPedalSearch
31/8952 passes without exclusions,including GS06's289 checks. The test fixture
keeps all positive behavior while reporting failed-save/missing-index instead
of copying null. The public PresetManager header was never changed; the native
test-only class lives in Tests/PresetFolderTestSupport.h. All1253 original test
resources and70 active factory presets remain unchanged; backup additions are
recorded separately. Original permission-denied branch with zero checks is not
claimed exercised. Prior products and matching source/link-input snapshots are
preserved before each replacement,with hashes.

## Remaining gates and bounded ongoing work

- Whole processor still performs20 try-locks/callback (preview1,bridge1,slot
  state16,effects render2). No production lock has been removed or accepted.
  Private preview work reproduces stale attacks after prepare and lost releases
  at byte/backend event-capacity limits; shared producer order and tuning
  ownership must remain correct. Unwired module tests are not RT acceptance.
- CQ03 exact required ref_board_cpu_cap conflict remains unapplied pending the
  recorded decision. No broad ref_* exemption or predicate lowering was taken.
- Retirement remains safe/bounded at eight but continuous unpaced processing
  starves quiescent reclamation. Private matched30s tests classify2061 retirement
  refusals continuous versus ZERO paced48k refusals; paced296 rejects were only
  no-ops/empty history. A progress correction needs a proof and unchanged-limit
  regressions, not a raised queue bound. Current stress passes do not hide this.
- Registered13-model prepared-memory accounting passes independent actual
  capacity/allocation oracles; logical limit remains64,000,000bytes. RSS,shared
  immutable tables,routing scratch,metadata and separately budgeted looper media
  are explicitly inventoried,not silently represented as the same limit.
- History publication OOM staging is integrated and its original81 failures are
  fixed in tested scope. JUCE var::clone noexcept exhaustion and post-commit
  listener exceptions remain outside that guarantee.
- Named models/loopers remain unregistered where original gates fail. The old
  looper converter's20kHz bandwidth failure is preserved; corrected Kaiser
  candidate and full300s/spectral work are private and independently reviewed,
  not current product acceptance. Phase90 duplicate fade/click/scope and CPU
  criteria remain visible. No prototype was called a finished branded model.
- Original historical Mac exact JSON pair/backtrace remain unavailable. Newly
  collected native failure diagnostics must be labeled new. Library v30 input
  transfer failed before readable bytes; no private packets were recovered.

Current source is secret-checked and pushed. No usage denial has been encountered
in this continuation despite the owner's credit warning; if one occurs preserve
the exact action/error/checkpoint and stop retries or environment/model switching.
Next: inspect the current native run,finish its builds/validators/artifacts,
preserve results and blockers,and continue bounded fixes. No release is qualified
or published yet. Keep original numeric,licensing,realtime and native/manual gates.
