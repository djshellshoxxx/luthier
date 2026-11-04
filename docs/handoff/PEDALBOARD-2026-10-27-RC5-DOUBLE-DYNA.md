# RC-5 timed double press / Dyna bypass checkpoint — 2026-10-27

Combined source **371d6b572edec1e065ad88536fe48676a11f1fd4** on local
codex/luthier-pedalboard-completion. Dyna source84c463347acd9dac43028d07ce8e9edc456e62c2
integrated first, then RC5 source/tests/contract. Base645cc99ece068a7cb61055f0384ee069c97c7ce6.
One integrator owns all commits; disjoint OpenAI worker owned only Dyna descriptor
and separate test, independently reviewed by integrator. RC5 had separate read-only
contract/code review. No remote source integration/push/CI/release occurred.

## Dyna original bypass law

The named spec§3 requires5ms equal-power; descriptor omitted appended bypassFade,
selecting Linear. Original actual-model+node RED:5tests/82,567checks,90failures,
exit1; exactly the90audiolaw cells fail. The only production change appends
BypassFade::EqualPower. Existing default processForBoard renders once; no second
model fade, control/parameter/state/latency/CPU change or registration is added.

| Separate Dyna entire cohort | Result | Time |
|---|---|---:|
| Pro Release |5tests /82,567checks, exit0|0.096s|
| Free Release |5tests /82,567checks, exit0|0.090s|
| Pro strict ASan/LSan/UBSan |5tests /82,567checks, exit0|0.511s|

Five rates44.1–192k ×3quality/PDC15/23/0 ×blocks1/64/2048 ×bothdirections.
All90declared active-render fingerprints match original RED in each edition.
Maxanalyticfade error3.37e-15, maxdeclared20/31Hz stimulusstep0.00106646<original
0.01FS. Exact settled latency-matched dry, finite and allocation checks pass.
This does not prove every arbitrary-stimulus/hardware click criterion. Original
THD/recovery/attack/alias/CPU failures remain open. Source/patch hashes and prior
provenance40d3503d,b85b53b2,28b4c63e retained. Worker staged/committed nothing.

## RC-5 timed action selection

Missing-API compile REDexit1 is an implementation gap, not a reproduced audio
failure. Rc5PressCycle now has a bounded timed down overload returning0..2actions.
A Playing-origin first press emits provisionalOverdub; qualifying second emits
CancelOverdub thenStop at one boundary. An Overdubbing-origin first closes the
pass withPlay; qualifying secondStop preserves that accepted take. Other origins
keep ordinarycycle without arming this pair. Deadline is inclusive down-to-down
floor(rate*ms/1000), independent of release/hardness. Rate/ms/clock validation,
config changes, duplicate guards, consume/expiry/reset semantics are explicit.

Cancellation is authorized only by still-present owned provisionalOD. Caller MUST
invalidateCandidate before external transport/history/media actions, even when
state later looks identical. Failed/automatically closed first actions cannot
cancel another take. Release resets only held; untimed API invalidates the pair.
No wait/allocation/queue/I/O is hidden in the selector. No hold/Clear policy is
silently chosen, particularly the held second double-Stop ambiguity.

| Separate full real-core/codec/RC5 cohort | Result | Time |
|---|---|---:|
| Pro Release |34tests /34,207checks, exit0|1.918s|
| Free Release |34tests /34,207checks, exit0|2.010s|
| Pro strict ASan/LSan/UBSan |34tests /34,207checks, exit0|13.187s|

Ninety cells ×bothactualcores:5rates ×blocks1/64/2048 ×N−1/N/N+1 ×twoorigins.
Independent rational150.125ms conversion, actual core sample clocks, exact action
order, retained/cancelled/committed/redo PCM and callback allocation checks.
A single prepared64frame page holds each32frame loop, isolating ownership/timing
from disk pacing. Other cases cover zero interval, failedfirst, externalsame-state
invalidation, changedconfig whileheld, invalid/backward clocks and INT64_MAX.
First34/34,204 results are retained before the added explicit held-config check.

The tiny provisional input isolates history correctness, NOT the original−60dBFS
Stop continuity gate. CancelOverdub can immediately remove louder provisional
audio; separate boundary acceptance remains necessary. Do not change that gate
or commit the provisional pass merely to make continuity look green. Full
controller/wrapper/99memories/advanced playback/registration remain unfinished.

Both cohorts use strict detect_leaks=1,alloc_dealloc_mismatch=1,halt_on_error=1;
UBSANhalt_on_error=1,print_stacktrace=1; LSANunset. No findings through shutdown;
stderr only JUCE8.0.10. Source/module/object/binary hashes, real commands/logs/
exits and all RED results retained. No summed/combined full-suite claim: these
are distinct focused binaries; final product/native/manual gates remain open.
Dyna unique cache-j1 and rootcache-j3 stayed within4core quota, no new VM.

Evidence / reproducible local commands:
- `/workspace/luthier-evidence/dyna-node-bypass-20261027/`: build.py thenrun.py
  <pro|free|asan>, unique private cache; original descriptor/test and fingerprints.
- `/workspace/luthier-evidence/rc5-double-press-20261027/`: build.py thenrun.py
  <pro|free|asan>, retained dependency-tracked looper cache. Dyna evidence is copied
  as allowed text/source/log suffixes into this checkpoint; objects/binaries excluded.

## Ownership / authorization / ready states

The owner now explicitly permits necessary private Luthier source/spec/test
sharing with Anthropic through the EXISTING broker/shared$200budget. This supersedes
the earlier no-Claude preference only for that authorized broker. Codex remains
sole integrator/local build owner. No Anthropic call/new credential/binding/
environment occurred here. Earlier fixed-snapshot feature report stays historical.

| State | Work | Owner / exact boundaries |
|---|---|---|
| Integrated |Dyna node EqualPower correction |84c46334; original numeric/CPU gates stillopen|
| Integrated |RC5 timed double action selector |371d6b57; notfullgesture/model acceptance|
| Broker packet ready/reserved |Phase90 CPU proposal,Fable |only model.cpp/.h; no local duplicateoptimization|
| Broker packet ready/reserved |CE2 BBD-on original THD test,Sonnet |only ReferencePedals_boss_ce2Tests.cpp; no local duplicateimplementation|
| Read-only preparation |RC5 reverse/half-speed storage contract |no runtimewrite; officialmanual fact/streamreadiness investigation|
| Open |RC5 louderCancel/Stopcontinuity,Hold/Clear,99memories,wrapper/livehandoff |do not silently choose destructive held-double policy|
| Open |Original modelCPU/tonal gates,native/manual/release qualification |zero selected named registrations; thresholds unchanged|
| Restricted |Push/CI/merge/deployment/release,newsoftware/security/credentials |not attempted; deniedActions/UI/process reads not retried|

Verified native broker packets (no full private archive sent):
Fable libfile_0c8d8a01b5a08191be29a439cc4e381b v0,158189bytes,
SHA256ea1c660381a0a7d91459501f27b8a7161ae188e310dade984abc8cae88a598e7;
31complete source/spec/test/evidence sections verified. Attached coordinator CPU
script is syntaxchecked ONLY, notexecuted. Sonnet libfile_f3f14ca21444819197e0b1ad99b91914
v0,91052bytes,SHA25637714f9a9b40f581a7fc8b3dc6093932cb142902d6e15754656150a98da21d05;
23sectionsverified. Both pin645cc99e; their target dependencies remain unchanged
by local RC5/Dyna changes. Parentbroker controls invocation/budget,notthisexecutor.

The fixed-source feature report was delivered as native Library
libfile_685b1640f8448191824388e0b1ad0565 v0,source4ceb1b5d,
SHA2566a5022d6777ac83819ba98b3d1b0a851338b68fba675710f17a7b9a86845f1c7:
55–65%overall and65–75%implementation are qualified effort estimates,notpasses.
Do not transfer them to another scope or claim the new prerequisites finishRC5.
Private evidence checkpoint retains all predecessor bundles/bridge/worker history,
licenses/attribution and failure evidence; recover isolated without resetting edits.
Six selected/deferred twelve preserved. No binaries/caches/private intake packs/
credentials uploaded; no immutable assets/release tags overwritten.
