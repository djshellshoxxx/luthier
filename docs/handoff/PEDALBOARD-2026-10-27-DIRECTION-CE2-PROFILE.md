# Directional cache, CE2 diagnostics and Phase90 profile — 2026-10-27

Code checkpoint4080be11a3c83aaddb9a12108793446fcf8d0bf8 plus contract-only
23b95676cae17ec6b495c5665c530bb9838da991. Base da984fa23e5527291445791c379ab7f33d51803e.
Root is sole integrator. Source remains local; no push/CI/merge/release/deploy.
Active uncommitted core-reverse work is separate, not counted in this checkpoint.

## Integrated / actually validated

- Directional LooperStorage prerequisite7d20d18a89994ff4323aefb4fc1f80ca1cf41419:
  typedForward/Reverse, Forward default, currentpage/predecessor read-ahead and
  matching protected eviction window. Actual partial-tail page entry triggers
  reverse prefetch, including implicitzero/cached/stale/miss paths. No transport
  direction state, PCM reversal, extra buffers/metadata/locks/I/O/allocations.
  Existing parity/epoch/version/FIFO/tokenCAS protocol retained. Read-only
  independent review and root review verified exact tested3file hashes.
- Missing-API compile RED retained. API-only scaffold with oldforward behavior
  gives6tests/1,217checks/sixreadinessfailures; not mislabeled as original API
  runtime. Finalnew6tests/8,056checks pass. Full storage/core/queue/graph/resume/
  compaction/forwardgeometry closure: **61tests/45,592checks/exit0 per Pro,Free,
  strictASanLSanUBSan**,2.293/2.331/4.598s. All original oracles retained. Maximum
  tested55,328,256byte reservation unchanged/below64,000,000. New tests use fresh
  real hashed assets, worker-only I/O gating, fullwindow eviction, three loops,
  partialtails, queued direction changes and exact stereo PCM/noalloc/faults.
- CE2descriptor4080be11 appends only defaultcpuScale1 andEqualPower to meet its
  explicit buffered5mslaw. Actualnode/separateactiveDSP/separate15Hzdryreference
  test covers60cells: BBDoff/on,fiverates,blocks1/64/2048,bothdirections. Corrected
  originaldescriptor RED1test/420checks/60lawfailures; final Pro/Free/strictSAN
  each1/420/exit0,.0480/.0479/.3063s. All60activefingerprints matchRED, maximum
  lawerror4.14e−15; linearcontrol,exactbuffereddry,finite/noalloc/latency pass.
  Initial setup missingBbdCorelink and superseded lifetime/startup fixture failures
  remain separately labeled. Corrected reference follows actual always-render
  tail contract and fills prepared12mswetdelay before toggling; no loweredlimit.

Strictsanitizers keep detect_leaks=1,alloc_dealloc_mismatch=1,halt_on_error=1;
UBSANhalt_on_error=1,print_stacktrace=1; LSANunset. No diagnostics. Separate focused
binaries are not a full product/native pass or one combined-suite test count.

## Preserved RED / measured diagnostics

CE2originalrawjump remains RED12/60warmedcells at44.1/48k disabling,bothmodes,
allblocks,max0.0014989543382636535 versus original0.001FS. The new law test does
not assert or close that separate gate; originalrawnull wording is not redefined.
CE2BBD-on low-level originalgate stillRED at allfive rates; no local calibration
writer or DSPcorrection. No namedmodel registration/fidelity/CPU/host acceptance.

CE2supplementaldiagnostic at frozen da984fa2/tested802201: all six commands exit0,
30/30instrumented-vs-baseline stereo hashes and projection metrics exactly equal.
New1.7swindow retains allfive−12dBFS BBDon failures; fullharmonicrange also stays
below0.5%. Actualshaperinput0.251058–0.251160 never enters >0.5 nonlinearbranch
in any lowlevelon/offcase. Nonharmonicresidue is reported separately, notTHD+N
or claimedhardwareclockenergy. All30finite,max|DC|1.963e−6,projectionclosure<=
1.013e−14. At−3dBFS bothmodes retain2–6%/thirdharmonicdominant. Currentdescriptor
diff does not change activeDSP, but those30measurements remain pinned to their
actual frozen source; they are not claimed rerun on newsource.

Phase90candidate remains private/unapplied, all10originalCPUtrials fail<0.4%.
Newinstalledgprofdiagnostic uses original480000/48k/128 route100trials perphase,
freshmodel/inputs and profiling only duringprocessing. OriginalO3 plus diagnostic
g/pg; same-runthreadCPU10.666095s/wall13.103880s,761captured100Hzticks7.61s.
99.87%capturedself falls inrender. Sharedmathsymbols remain dynamic; missingtime
cannot be precisely assigned tolibm. Gproflineformatter failedexit1; independent
review verified exact4bytehistogram mapping and ownartifactanalysis corrected an
earlier1byteproportionalbias (oldanalysis retainedUNQUALIFIED). 423/761ticks55.585%
map to serialallpass/nonlinearity lines135–138;311/76140.867% map tosqr/divline138.
These are sampledPC/source regions, not exactinstructioncost or wholeCPU shares.
Explicitstaticlibm diagnostic relink failed beforeexecution on installedIFUNC
_dl_x86_cpu_features; no suchbinaryran and no furtherlinkrescue. No new paidcandidate
or acceptance from profiling. Originalmath/CPU/24bit/sound gates remain intact.

## Ownership and ready queue

| State | Slice | Owner / boundary |
|---|---|---|
| Integrated |Directional storage prerequisite |7d20d18a; not audible reverse transport|
| Integrated |CE2 bufferedlaw correction |4080be11; rawjump/THD remainRED|
| Reserved |CE2 BBD-mode calibration proposal |existingClaude broker; no localDSPwriter|
| Active |Stopped/empty reverse core |storageworker ownsCore.h/.cpp,newreverse test,additivegeometrytest only|
| Defined |Reverse core lifecycle |23b95676: inactive setter,retainedselection,forwardrecord,ODrefusal,coherentRuntime; JSONpresetselection still separate|
| Open |All originalDSP/CPU/RT/full/native/manual/releasegates |0/6selectedmodelsregistered; twelve deferred retained|

Root/worker do not duplicate calibration work. Full original18model roadmap,
authorship/notices and all failedcandidates remain. Feature percentages stay pinned
to prior4ceb1b5d report; backend prerequisites do not silently change that estimate.

## Durable evidence / exact transfer blocker

Latest confirmed private checkpoint before this milestone is sameidentity
libfile_1047f71682648191b5c2e265aed98eaf version28,12166577bytes,
luthier-window-broker-da984fa2-20261027.zip,
SHA8f40a11aad2caba38131577d98d8b89d4f6701318a8803e7b02ce0d2cd1b0ec3.
Keep every predecessor/recoverybridge/worker provenance; recover isolated from
published22d6b18d+verifiedbridge+overf1efbundle, never reset another working tree.

Two NEW native packet writes failed via current Library prepared-upload helper:
`hosted apps tools/list request failed: network`,exit1. No confirmed fileIDs or
Library creation. This is a generic discovery/network failure, not established
authorization denial. No retry/direct-action fallback. Library skill's prepared
upload instructions forbid switching a started helper write to directaction.
Need supported transfer capability restored or explicit applicable instruction
before resuming that action; no evidence requires newcredentials/securitychanges.

Localready packets (do not claim already saved):
- CE2 frozen4080be11,40fullsections/exactbytehash verification,all30baselinehashes:
  ce2-calibration-packet-20261027/LUTHIER-CE2-BBD-CALIBRATION-PACKET-4080be11-20261027.md,
  187989bytes,SHAa25e60d10fbf6ad7c3631e5c85ae0679b9b0295be735247281a0653e4ed88d6e.
- Phase90measured feedback: phase90-profile-20261027/LUTHIER-PHASE90-MEASURED-PROFILE-20261027.md,
  13339bytes,SHAfeec9ed5938c1b8458b6d717a8a0271562adb7255d1d12b2464ce5998d4f8c2d.

All paths relative to /workspace/luthier-evidence. Runtime commands/hashes/patches/
outcomes: storage-direction-20261027,ce2-node-bypass-20261027,
ce2-measurement-diagnostic-20261027,phase90-profile-20261027. Supplementary verified
CE2QAarchive404268bytes/SHA c07188e6739af73c74d54224e7077fe31b0f5ab7890aef2314924e8e94df96ce
includes frozen source/notices, no binaries/cache. Separate normal source/evidence
checkpoint remains authorized; failed nativepacket writes are not silently rerouted.
DeniedActions40292011494 and extraUIcallbackstress remain unexecuted/unretried;
no newsoftware/VM/paidrequest/security/credential/settings changes.
