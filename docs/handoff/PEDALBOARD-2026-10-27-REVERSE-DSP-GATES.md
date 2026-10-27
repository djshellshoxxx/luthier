# Reverse-core milestone and preserved DSP gates — 2026-10-27

Integrated reverse-core source36f16887281496f0f6dfa2e1067bed2da0131e38;
99-memory contract-only2ee410bef58fe8a37cef7e10c4df61236b4f5857. Earlier source
4080be11/7d20d18a and every historical failure remain. Root is sole integrator.

## Integrated / verified: stopped/empty reverse core

Newtyped setter rejects unprepared/invalid/Recording/Playing/OD/audibleStopfade
without mutation. Empty/fullStopped selection survivesRecord/Clear/reset like
existingseamsetting. Recording writes forward; reverse-selected closure/play starts
atN−1. Directionalheadwrap,Ready/prime and everybase/active/retired/scratch read
agree with storage direction, including replacement-window compaction handoff.
ReverseOD returns beforecloseRecord/startOverdub/service sideeffects. No existing
overdub commit/cancel or active-reversal phase policy was invented.

Direction is included in the same atomic/sequenced runtime tuple. ResumeCaptured
validates before mutation and primes its captured direction/head/gain/clock,
includingEmpty andstopfade; defaultStopped restore explicitlyForward/head0.
JSON deliberately omits runtime, so this does not persist memoryreverse settings.
NeutralminimumWindowFrames preserves oldforwardcompatibility wrapper. Existing
forward geometry tests retain identical byte prefix; independentreverse enumeration
covers4,704cases. Actualsizeof accounting remains55,328,256bytes at192k/90min,
below64,000,000; the original>=2seconds geometry and memory gates remain intact.

Missing-API RED and explicitly recordedAPI-only/forwardbehavior scaffold RED
1test/6,400checks/5,810expectedfailures are retained as missingfeature evidence,
not a claim that oldforward playback was defective. Finalnewreverse9tests/11,066
checks pass. Full originalstorage/core/queue/compaction/graph/codec/resume + new
direction/geometry/reverse closure:

| Configuration | Actual result | Wall time |
|---|---|---:|
| Pro Release |71tests/56,661checks/exit0|2.505s|
| Free Release |71tests/56,661checks/exit0|2.505s|
| Strict ProASan/LSan/UBSan |71tests/56,661checks/exit0|5.261s|

No sanitizer diagnostics; detect_leaks=1,alloc_dealloc_mismatch=1,halt_on_error=1;
UBSANhalt_on_error=1,print_stacktrace=1; LSANunset. ExactnonconstantstereoPCM,
allfive supportedrates/shortblocks,partialtail/fullwindow,gatedonce-onlyretirement,
Undo/Redo,ODrefusal,forwardrecording,Ready/faults,runtimecapture/concurrenttuple/
invalidrestore/JSONomission and callbackallocations are covered. NoTsan/racefreedom,
activeflip/click,half-speed,preset,wrapper/registration/native/fullsuite claim.
Generation is a media-graph revision: futurefinallivehandoff must compare the
coherent runtime tuple too. Independentfreshreview found no required issue.

Evidence: paged-reverse-20261027/OUTCOME.md, source/build/test manifests, logs,
tested-owned.patchSHA58035d20406ba74dddf0ff7e5410f81f07fa65fb7bb205f4b773ac5d22c2b9b0.
Rootreview bookkeeping initially expected'owned'instead ofmanifest'files'; shell
continuedlocalcommit. Corrected postcommit verification confirmed ALLfourcommitted
files equal testedbytes across3editions, plus scopedsecretscan, before anyupload.
No tests were rerun unnecessarily. ROOT-REVIEW.json preserves this discrepancy.

## Private candidates rejected / never applied

Phase90 explicitSSE2two-lane stage candidate was a distinct mechanism grounded in
actualprofile/disassembly: fourpacked sqrtpd/divpd pairs versus scalarbaseline.
Original CPU workload/flags/denominator unchanged, all10candidate0.54068944–
0.64838144% FAIL<0.4%; same-runbaseline0.50059758–0.57756422% alsoFAIL.
Printedchecksums/FR/THD equal do not prove fullsample/state/exceptionalFPparity.
No broaderparity/SAN/iteration afterfailedgate. NonLinux/exceptionalFP/NaNpayload
remain unverified. ProductionCPP1716466b68c007d7c51d322984389a44b40c74f2f212417da2f87fb769406401
andheader7ed7cc1c67dacfac7e30877b6bd8d8a3b6cfba0f2ed1909d86745ae61a348d18 unchanged.
CandidateSHA07642f3529354439f4f2ba7ea50a970eb8bd8ec3f3db53eb46c6b2f4ba8a8405.

Dynaoutput-sideOTAcontrol candidate follows reviewedCA3080physics/§3diagram but
corrects conflicting§4pseudocode; it is not an equivalent optimization. Same-run
baseline7tests/82,667checks/20fails; candidate7/82,667/15fails, bothexit1.
DefaultTHD improves0.04238–0.04249%→0.23643–0.23788% and passes0.1–1%; maxsensitivity
still0.23640–0.23771% FAILS3–5%. FASTlight0.848–0.989ms/default0.371–0.509ms FAIL
original2.5–7.5ms; lightrelease1.140–1.142s passes0.5–1.5s. Heavydefault~1.57s
is retained separately and is NOT a light-GRfailure. SLOW15msnominal has no invented
tolerance. Finite/DC/silence/determinism/noalloc/latency/nodecases pass, DC<6.64e−12.
NoCPU/SAN/soak/newretune afterqualityfailure. Product unchanged; privatecandidate
SHAcd7d8217af41eb37c42869d3a8389535faefe2a33d1a04665268bebf859683bf.
Nextneed: revision-specificinputattenuation,biascurrent/feedback/detectornetwork,
not arbitrarysensitivitydistortion. Primarysource attribution retained:
[Intersil/RenesasAN6077](https://www.renesas.com/en/document/apn/an6077-ic-operational-transconductance-amplifier-ota-power-capability),pp1–2.

CE2localcalibration produced NOpatch/probe. IndependentfreshstrongDSP review agrees
that estimated behavioralmodels need not require transistor-level identification,
but suppliedE51/inputheadroom + linearclockhold give no justified additional
lowlevel nonlinearshape/normalization. Merely blendingtanh/fittingcubic/hidden gain
to meetfloor is unsupported here. Need aggregateconditionedtransfer/harmonic data
or defensible explicitestimatedbehavioral rationale. This is engineering evidence/
modeldefinition, notapprovaldenial; original0.5–3%/2–6%/othergates unchanged.
OFFcold/continuedbehavior/shareddefaults staybitexact; OFFafterchangedON maycarry
legitimatechangedhistory withboundedtransition, notunpromisedparallellegacyhistory.

ManufacturerXvivePDF opened but current tools delivered no readablepagepixels/text;
no fabricatedcurves. SeparatePanasonicmirror read returnednotaccessible and stopped.
No endpoint/Librarywrite/deniedextraUIstress was retried or bypassed.

## Queue / source ownership

| State | Slice | Ownership / next gate |
|---|---|---|
| IntegratedGREEN |Stopped/emptyreversecore |36f16887; noactiveflip/preset/wrapperclaim|
| Active |99-memorymessageindex/schema |rc5_tap_tempo owns onlyModels/rc5_storage.h/.cpp,newbanktest|
| Reviewing |Memoryindex contract |storageworker readonly; exact2ee410be contract, no fileworker/audioselector|
| Validating |CurrentDD3TCPUbaseline |phase90reviewer privateevidence only, historicalCPU maybestale; benchmarkquietlyatunchangedgate|
| Rejected/open |Phase90CPU,Dynaquality |privatefailedcandidates preserved; no speculativeiteration|
| Engineeringgap |CE2BBDtransferdefinition |no localwriter or externalassignment; allmeasuredRED retained|
| Open |RC5half/activeflip/holds/99integration/asyncI/O/UI/host |not silentlycountedcomplete|
| Open |Full/RT/stress/native/manual/release |0/6modelsregistered/accepted; originaltwelve deferred retained|

All paths below /workspace/luthier-evidence. Newproof directories:
paged-reverse-20261027,phase90-packed-stage-20261027,dyna-ota-control-20261027,
ce2-calibration-local-20261027. RootreservesallGit/docs/evidenceintegration.
The exact99memorycontract specifies finitebounds,Ready-onlysave,unknown/future
preservation and exact13h rationalduration via existingJUCEBigInteger; no assets
inline orinventedRhythmOnpolicy. Wholeboard256KiB/resample/pin-GC/filecommit/audio
selection remain explicitfuturewrappergates. It is a modulecontract, notcompletion.

Latest confirmed private sourcecheckpoint is sameidentitylibfile_1047f71682648191b5c2e265aed98eaf
version29,source1f7bfeed,13260532bytes,
SHA3ec7474d7e5ce51bbda0dc1b8e8460b01e16008b5c7b2556c1da67ad7b854b42.
Nextnormalguardedsource/QA save retainsallpredecessors. Separatetwo nativepacket
helperwrites remainfailed/stopped(tools/listnetwork), notsilentlyretried through
the normalcheckpoint. Sourcepush/CI/merge/releases/newdependencies/credentials/
security/paidinfrastructure remain unattempted. Preservef1efrecoverybridge and
published22d6b18d prerequisite; recoverisolated, neverresetanotherworkingtree.
