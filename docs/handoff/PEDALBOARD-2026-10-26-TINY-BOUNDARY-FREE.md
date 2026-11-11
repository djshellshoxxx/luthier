# Tiny-buffer boundaries and Linux Free continuation — 2026-10-26

Latest focused fixture correction: code commit
`77ade0bcd4d04f8dd3eb0bf083ec85ae120ce959`, test-source SHA256
`cfb54de1ad2fb7dd540f9e21cf1f3d4545f20596575aa5a9a236f90fe3af1b96`.
Free Release and Pro Release each pass2tests/11,260checks (28.5s/33.0s).
Strict final Pro ASan/LSan/UBSan passes2/11,260checks/320.0s/exit0, including
shutdown; no sanitizer diagnostic. The historical unfiltered Free RED below remains preserved;
there is no completed full-suite rerun on this changed test source.

## Source and scope

Same repository/executor; branch `codex/luthier-pedalboard-completion`.
Starting HEAD `f1efeded706b9dc57da7e941ca856b17eaad5183`. Only tests and
handoffs have changed; product source and original acceptance limits are intact.
The tested extension and handoffs are committed locally as
`d5b64ab650665d94825eaaf460f0024558825f38`; no push was attempted. The
incremental bundle `d5b64ab6-over-f1efeded.bundle` verified successfully
(SHA-256 `0766645982df50b4fa61b648c3b169ee916f7ff06951cc0dd0a0ab62d752d800`),
and requires the original f1efeded source. The private Library checkpoint
`libfile_6170df4828e88191be1b452634243d3c` preserves this completed sanitizer
milestone plus an explicitly partial Free build log; it is not a completed
Free qualification report.
Normal workspace, patch and build actions succeeded in this directly
user-authorized continuation. No alternate route around a rejection was used.

`Source/Tests/QA_Robustness.cpp` SHA-256 after the test extension:
`6221828e6b695d5a07e5485c7e7e97b22a02795fda5465afdf31fb74e333c2f4`.
The original file is preserved in the evidence directory as
`QA_Robustness.f1ef.cpp` (SHA-256
`1a6bbaba92fce8f0289655e963e2b1db331a42e9ff1c66af0ef5f3d230a6a5d5`).
The test-only patch has SHA-256
`bdceccfbea29ed8bf6fe0c273f9dfb01368d3061b5ada20f0c025c6b851f112d`.

The original ASan runner was copied before rebuilding, retaining SHA-256
`198d528b5ad544f9346bf946cd5adb63222c5ad0062e565408b54cba0b2c793d`.
The incremental ASan rebuild passed (exit 0); the extended runner is
`efa1f4baf48d905d9461434cadcb5fddf252c93aca5ebfa6f746f5a11f7cd75f`.
All files/logs here are under
`/workspace/luthier-evidence/tiny-boundary-free-20261026/`.

## Test extension

The original regression still renders **20,000 blocks per preset** at 8 kHz
and host sizes 1/2/3/4, using the same load/install/reload/finite/nonzero-output,
destruction and allocation-churn assertions. Those checks are factored into a
shared test helper; no predicate or original long-run count was reduced.

The added `Fuzz.convolutionTinyHostRateAndCapacityBoundaries` covers the 17
additional cells of `{8,44.1,192 kHz} x {1,2,3,4,5,7,8 samples}`, excluding the
four original long-run cells. Each added cell renders 2,000 blocks for each of
the same two presets. Each cell is logged before construction so an abort can
be attributed to a sample rate/buffer size.

Both tests completed together with `detect_leaks=1`,
`alloc_dealloc_mismatch=1`, no `LSAN_OPTIONS`, and halt/abort on ASan/UBSan
errors, under a 900-second process timeout (not reached). **2 tests / 11,029
checks / 407.5 seconds / exit 0**. The original long test kept its 2,102
checks (175.9 seconds); the additional matrix passed 8,927 checks (231.6
seconds). No sanitizer diagnostic occurred during execution or shutdown.
The log `asan-boundary-strict.log` has SHA-256
`61a270294fafdc81c60ebdb00b46843a6e64063f849a3177b515d2ed7e737349`.
This is selected-test qualification of the extension, not a new full-suite
pass or proof of the original byte-level overwrite cause.

## Linux Free

Fresh configure passed, using only the existing toolchain and local third-party
sources. Directory `/workspace/luthier/build-free-qualification-20261026`:
Release, FREE, LTO off, tests and renderer enabled, build string
`f1ef-boundary-free-qa`. No software was downloaded or installed. The explicit
build string plus source/test hashes above identify this test-only tree even
if a subsequent documentation/test commit advances HEAD.

The one fresh build completed with exit 0 (original session `72841`):

```sh
source /workspace/luthier-toolchain/env.sh
ninja -C /workspace/luthier/build-free-qualification-20261026 -j4 \
  LuthierTests LuthierRender Luthier_Standalone Luthier_VST3 Luthier_CLAP
```

All five requested targets built across 1,650 steps. Build log SHA-256:
`eeb2ab745fe0bb90e96b17c78e967863a6c1ffc17d4c734dfb7ae945b066503e`.
All product executables/modules were identified as genuine Linux x86-64 ELF.
The renderer is included so CLI tests can execute their real subprocess route
rather than skip due to a missing tool. Full Free unit acceptance failed as
recorded below; all2,790 registered tests ran. The single unfiltered clean-HOME
Free suite has completed: original PID `165131`, session `92865`,
log `linux-free-full-suite.log`, code/test commit `d5b64ab6`, documentation
HEAD at launch `f5658a4d7837dc10eb45cfc2a98acb7d3d4eeaa3`. Performance flags
are unset and the process has a three-hour timeout. Inspect the existing
process/log/adjacent `.exit` before suggesting any rerun.
The wrapper preserves `linux-free-generated-QA_ROBUSTNESS.md` and restores
`QA_ROBUSTNESS.before-free-full.md` to the tracked report afterward. Its
acceptance JSON checks unit exit, all 2,790 tests, zero report findings and
successful restoration; process exit alone is not the whole qualification.

| Product | SHA-256 |
| --- | --- |
| Standalone | `ad37d8c61da6f2dec2fabac84511e035c0b00b2d4e399e2b8aa0268f7f0d9121` |
| VST3 module | `34b0c885bab04d67ffde5721bd6555ccb924844111579b7ae7104a0917fca45b` |
| CLAP module | `af8bc40f1ead729ab56136827f63f0fd34bead8e3e8e6f16f7bed96ea3283e36` |
| Renderer | `9ff21228c5c6810717d17d6b4eecc92caf08c8f6b79504d44460bf556b9f5576` |
| Test runner | `1ec4c2717071c65fe475ab09d02f0a1d3cf23b9349770ebc18801e98a8afc675` |

The installed repository validation route passed (exit 0): VST3 pluginval
strictness 5 and CLAP 21 tests / 18 passed / 0 failed / 3 optional preset-factory
skips / 0 warnings. Log SHA-256
`224602c36efea6dea84a9c8d930108ee762dd479f4a7b7f08c5ba6c251d22d5e`.
Because pluginval's defaults covered only 15 pairs, a supplemental strictness-5
run explicitly tested 44.1/48/88.2/96/192 kHz x buffers 1/16/37/64/481/2048.
Both audio and automation logs contain all 30 pairs; the run exited 0 with
SUCCESS. Log SHA-256:
`371d760328ca27a4ec9bdfecb0408023839cf116c43365c75d35dab28584a91e`.
No GUI test was excluded. The external Steinberg validator was not installed
or run; pluginval explicitly reports that optional check unavailable.

The Standalone remained open for 10 seconds under Xorg :99; timeout exit 124
was expected. Its ALSA log says `/dev/snd/seq` is unavailable. This is launch
survival only, not audio-device/DAW/listening acceptance. Smoke log SHA-256:
`7f4899a2173c4c01d1325de76cd0bcfaa9311ef64dbfd90e0dd8ca45fe48ac12`.
The build directory is untracked and must be excluded from every Git add.
The GCC 14 optimized build emitted a retained `-Wstringop-overflow` diagnostic
at `CymbalPiece::render`'s `std::fill` length calculation; no runtime failure
has established an actual overflowing path. Do not silently drop that warning.

## Next actions and exclusions

1. The strict sanitizer selection is complete and green as recorded above.
2. The secret-checked local test/documentation checkpoint and incremental
   bundle are saved. No push is authorized in this bounded continuation.
3. Build and native Free validators are green; the one full Free suite is
   terminal RED. Diagnose the two recorded fixture failures with a focused
   effective-config/IR/counter A/B before any correction or fresh full suite.
   Preserve all assertions, tiny capacities and original long-run counts.
4. First ready Phase90 slice: independent5ms equal-power and literal24-bit
   delta measurement through the actual node executor, with single-fade ownership
   checked. Do not accept the existing linear-ramp probe as an equal-power pass,
   silently reinterpret the1LSB gate, or register models with zero CPU/memory cost.
   No new Phase90 test or implementation was started.

One coordinator, zero workers. No pushes, CI, new downloads/software,
environment replacement, paid infrastructure, credentials/security/network
changes, merges/deployments/releases, Library input retry, or separately
denied additional UI callback stress is part of this continuation.

## Owner scope update while the existing Free suite runs

The parent confirms initial release selection **Phase90, CE-2, DD-3T, CE-2W,
Dyna Comp and RC-5**. The explicit advanced-looper requirement replaces the
provisional Distortion+ slot with RC-5. Preserve all18 original model specs and
source; defer the other12, with no threshold relaxation or implementation claim.
`PEDALBOARD-SIX-MODEL-SCENARIO-2026-10-26.md` now records that decision and the
large remaining RC-5 wrapper/memory/clock/playback/readback gap. Shared paged
transport/storage and original rhythms do not constitute a finished RC-5.

No external Claude agent is running. The owner is considering cloud-only setup;
no key configuration, installation, paid call, new infrastructure or overlapping
model implementation was performed. Fresh permitted `git ls-remote` confirms
remote `codex/luthier-checkpoint-20261026-qa2` at production f1efeded and retained
DSP branch40ed1621. Remote completion remains oldb5bec374. Source/test checkpoint
d5b64ab6 and subsequent documentation are local; a fresh clone needs the verified
incremental bundle over f1efeded for that test extension. This lookup did not
retry the denied Actions run endpoint. The existing Free suite remains the only
running test and must be inspected through its original PID/session/log.

## In-progress Free RED evidence — not a terminal verdict

Historical observations in this section are superseded by the terminal result below.

The original PID165131 run has recorded failures in both
`Fuzz.convolutionPreparesSafelyForTinyHostBlocks` (2,102 checks) and
`Fuzz.convolutionTinyHostRateAndCapacityBoundaries` (8,927 checks). The reported
condition is QA_Robustness.cpp:332, “alternate preset did not reload the body
convolution”. The process continues; do not restart it, alter source under it,
or label Free full-suite acceptance green. PB02 passed776 checks with the
original0.5LU threshold intact. Generated robustness findings still need their
separate terminal inspection; report-only findings and failed assertions are
independent gates.

Read-only source evidence supports a fixture hypothesis, not a demonstrated
production fix: FactoryPresets.cpp:164/179 select Strat then Tele;
Edition.h:128 maps Free Strat to Tele while Tele stays Tele. Parameters.cpp:2486
uses that effective guitar. BodyEngine.cpp:367 intentionally returns without
reloading when the installed file is unchanged. A focused runtime comparison of
actual effective guitar/body IR identity and counters is still required after
this existing full run ends. Preserve the original RED. Do not disable the
reload assertion or force unnecessary production reloads merely to pass; if
confirmed, choose genuinely distinct supported body/cabinet fixtures and retain
every original install/reload/output/destruction predicate and long-run count.

Read-only Phase90 preparation found another precise acceptance distinction:
the named spec:62 asks5ms equal-power, while the integrated model's
`dry+(wet-dry)*engage` at mxr_phase90_script.cpp:116 is linear. The existing
edge probe:22 checks that linear expected law. Its tiny residual is not proof of
the specified equal-power curve or closure of the separate literal24-bit
peak-delta wording. First bounded slice: independently compare the required
curve and audit board/model fade ownership, retain the original delta evidence,
then resolve the exact discrepancy before registration. No new probe/test,
implementation or acceptance change was executed for this preparation.

## Terminal Free result — 20:47:11 UTC

**FAILED:2 of2,790 tests,21 of14,943,980 checks,5,375.2seconds,exit1.**
2,788 tests passed. Only the two tiny-convolution test rows failed; all21
messages are the body-reload assertion described above. Source was immutable:
production f1efeded, test extension d5b64ab6, QA_Robustness.cpp SHA256
6221828e6b695d5a07e5485c7e7e97b22a02795fda5465afdf31fb74e333c2f4.
The runner remains SHA256
1ec4c2717071c65fe475ab09d02f0a1d3cf23b9349770ebc18801e98a8afc675.
Documentation HEAD at completion wasae67e429; no production source changed.

Full log SHA256:
35adaa26f97cd15e375d6904cd9a7c7a3559ba4d702a9b5f87a339881f38e0cd.
The preserved generated robustness report has**0findings**, SHA256
e8993815226dd95e871671ac0effdb7d2e482d2abe1cc80efb2a02d4441d78d6.
Its bytes equal both the pre-run backup and restored tracked report. This
report-only success does not override the21 failed assertions.

`linux-free-full-suite-acceptance.json` is preserved unchanged with exit1,
acceptancefalse, robustness0 and restorationtrue. Its `terminal:null` is a parser
limitation for the failed-summary spelling, not missing terminal output. The
independent `linux-free-full-suite-result.json` validates actual totals/pass rows,
hashes and failure counts without changing the original result.

The existing authorized `RefPedalUiLifecycle.tenThousandNativeOperationsAndEditorDestruction`
passes393,576 checks/174.284s:10,000operations,21/20 editor create/destroy count
at the progress marker and546 native closes. The separately denied extra callback
stress remains unexecuted. The standard30s board grind passes9checks with3000edits,
296rejections,122,790blocks/54,170,855samples, callback_new0,callback_locks0,invalid0.
These hooks do not prove all legacy try-locks absent.

Nineteen cases have zero checks under existing guards, including opt-in CPU/
growth/fuzz cases and an unreadable-file access guard; do not equate their pass
rows with measured budgets. The Clang18-specific normalization golden explicitly
skipped on GCC14. These are recorded coverage limits, not nineteen newly found
bugs. Highest observed cumulative VmHWM was1,535,348KiB in45s process samples;
it is not claimed as the exact final peak. This ordinary Free run is not ASan/
TSan qualification. No crash/nonfinite failure was reported.

At that terminal checkpoint the next local diagnosis was the source-backed
same-effective-guitar fixture hypothesis. The subsequent77ade follow-up below
confirms and corrects it. No full rerun, model implementation, API agent, CI,
push or release was started. The owner assigns
external Claude work elsewhere; there is no Luthier ownership reservation.

Phase90 preparation now identifies exact executor code: NodeTable.cpp:87–101
applies a linear5ms `runtime.enableMix`, while mxr_phase90_script.cpp:82/116
also applies its own linear `engage` blend. The first bounded integration probe
must measure their actual composition and the named spec:62 equal-power law,
with spec:130's literal delta criterion separately preserved. Correct single
fade ownership through review before registration; no such probe was executed.

## Confirmed fixture cause and bounded correction

Evidence directory: `/workspace/luthier-evidence/free-tiny-ir-fixture-20261026/`.
The original runner at its build path repeats2failures/21of11,029checks/27.8s.
Diagnostic traces verify every one of21cells: in Free both recipes playTele1,
request`solid/medium_ash_played.wav` and resolve to shipped
`solid/medium_alder_played.wav`; body loads stay2→2. Cabinet2x12→1x12 genuinely
changes, loads10→12. Pro's original pair instead changes offset/alder→solid/alder,
loads4→5, and passes2/11,029. Thus cache reuse is correct, not a demonstrated
product reload defect. The new physical-IR invariant first failed8/2110checks
in the original long four-cell reproducer, preserving test-first RED.

Both original preset loads and every20,000/2,000-block render count remain.
The alternate fixture now selects shipped mahogany through
`setValueNotifyingHost`, after the full preset apply, followed by the documented
`applyToEngine`/`flushPendingStructuralChange` path. A trial writing before
`applyAllNow` was reset by its factory refresh; that42-assertion RED is retained.
A discarded SG candidate had no matching shipped body IR and entered modal
fallback; it is not used. The final test adds real-file, active-convolution and
distinct-IR assertions; existing reload/nonzero/finite/destruction/churn checks
and names0/1 remain. Temporary DEBUG/candidate code is removed. A deliberate
single swap trace per cell records future fixture failures without audio-callback
instrumentation. File snapshots die before engine destruction/churn.

Final normal results verify all21cells: Free solid/alder→solid/mahogany,body2→3;
Pro offset/alder→solid/mahogany,body4→6; cabinet counters increase in both.
All1253 Free resource hashes equal their pre-diagnosis values. Normal logs:
Free SHA256`8f83ae5539e2376379509dff4978c85b9c19094ac67a6c639d0bd02873f24eae`,
Pro`5e4b612d801a041cfc02067a372da826fc0b6368eaba22b2b22acac272ac49aa`.
Full commands, separate fresh HOME receipts, binary hashes and actual traces are
in FINAL-SOURCE-PROVENANCE.json and FOCUSED-NORMAL-RESULTS.json. No production
DSP/loader/host code changed, no cache was forced and no predicate was disabled.

One copied-executable run could not resolve adjacent resources (126failures);
it is explicitly invalid reproduction evidence, preserved separately from the
correct original-build-path RED. A nonessential optional cancellation guard
read of`/proc/216038/exe` was denied(errno13) before any signal. It was not
retried or rerouted. That superseded sanitizer candidate finished naturally:
2/11,260/310.4s/exit0 at source7dcecc78; it does not qualify the finalcfb54de1
source. The final strict run has its own log/session. No owner approval is
pending for the failed guard read, and workspace builds remain available.

Next ready feature slice remains Phase90's actual node/model bypass composition
versus original5ms equal-power and literal24-bit delta gates, with independent
expected output and review of single fade ownership before registration. RC-5
remains selected and unimplemented as a named pedal; all12 deferred models stay
preserved. No Phase90/model implementation, new software, CI, push or release
was started during this fixture correction.

Final strict result: session87565 completed naturally,2tests/11,260checks,
320.0seconds,exit0. `detect_leaks=1`, `alloc_dealloc_mismatch=1`, abort/halt on
ASan error, UBSan halt/stack traces, LSAN_OPTIONS unset; no diagnostic through
shutdown. This is selected **Pro** sanitizer qualification, not a Free ASan or
whole-suite pass. FOCUSED-FINAL-RESULTS.json records21 verified swaps and exact
log/binary/source hashes. No unfiltered rerun was started after the bounded fix.
