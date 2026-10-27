# Luthier current completion assessment — 2026-10-27

Assessment source **4ceb1b5d339692a1c76eaa6053368d6b9b34fac9**, local branch
`codex/luthier-pedalboard-completion`, same saved executor. This updates affected
rows of [the prior feature inventory](LUTHIER-FEATURE-INCOMPLETENESS-2026-10-26.md)
and [the agreed six-model scope](PEDALBOARD-SIX-MODEL-SCENARIO-2026-10-26.md).
It is not a new master plan or a complete new source/remote audit. The frozen
private plan ZIP remains unavailable under its recorded transfer failure; no
retry or newly recovered bytes are claimed. The existing source-pinned finish
instructions, implementation contracts and acceptance ledger govern this report.

**Rough overall engineering estimate:55–65% complete,35–45% remaining for the
agreed six-pedal release. Implementation alone:65–75% complete,25–35% remaining.**
Confidence is low-to-medium. These are effort ranges, not measured acceptance,
percentages of tests/files, a schedule or an average of the table rows. The
largest weight is the six named models (especially RC-5), followed by shared
board/state/host/UI behavior and final safety/native/manual/release qualification.
Shared plumbing receives credit once, and the same native work is not counted
again for each pedal. The lower implementation range reflects that substantial
core code exists while the largest named looper wrapper does not.

Native release qualification is separate: **0/4 requested platform/architecture
cells are qualified at this source** (Windows x64, macOS Intel, macOS ARM, Linux
x64; each needs standalone/VST3/CLAP). Historical binaries exist; zero is not a
claim that no artifact bytes or successful older builds exist. No selected named
pedal is registered or accepted: **5/6 have integrated DSP prototypes, RC-5's
model wrapper is still a skeleton,0/6 registered/accepted**. Full18-model retained
inventory is now5real integrated named prototypes and13integrated skeletons;
other retained branches contain substantial deferred candidates. The ten generic
registrations and three compatibility prototypes are not counted as named pedals.

## What changed since the previous inventory

- Dyna Comp is selectively integrated from the preserved DSP lane with exact
  provenance; invalid-policy reservation and deterministic hostile-input/reset
  coverage added. Original tonal/recovery/CPU failures remain open.
- Phase90 single-envelope composition, bypassed sync freeze and accurate memory
  admission are corrected. Original CPU<0.4% remains RED in latest retained
  baseline trials0.52346–0.62389%; both rejected optimization candidates and the
  unresolved24-bit click criterion remain preserved. Old faster trials are not
  substituted for this current open gate.
- DD-3T memory, tap/history coverage, beat-boundary sync and held-tail scheduling
  are corrected; CE-2/CE-2W memory and CE-2W rapid retargeting are corrected.
- RC-5 prerequisites improve: rhythm DC, history eligibility, rapid paged restart,
  immediate press-cycle helper, complete redo graph/JSON migration and blocked/
  failing redo-worker publication coverage. Runtime-only captured-boundary resume
  now preserves head/gain/clock with explicit opt-in; ordinary memory restore stays
  stopped. This is not a completed wrapper or live final-boundary replacement.
- Latest focused cohort: **32tests/31,122checks per Pro,Free,strict Pro
  ASan/LSan/UBSan, exit0**;0.951/0.941/2.719seconds. Known nonzero PCM, both fade
  directions, actual depth change, timed commands, malformed Empty/Ready metadata,
  concurrent tuple capture and signed-clock boundary are covered. No suppressions;
  sanitizer stderr only JUCE v8.0.10. This is not a full product/native suite.

## Feature table

Ranges mean estimated remaining engineering plus the named feature's required
verification. They are deliberately broad; difficulty reflects uncertainty,
not elapsed time. Most unchanged rows reuse the prior inspected inventory and
are not claimed freshly re-measured. E=engineering; O=owner/access/manual gate.

| Feature/module | Implemented and validated portions | Largest remaining gap | Estimated remaining | Difficulty and concrete reason | Gate / next smallest slice |
|---|---|---|---:|---|---|
| Board editing/engine/routing | Immutable topology, stable slots/UUIDs, transactional undo, placements, branches/feedback, guards and publication have code/tests | Final full-topology workflow and integration closure |10–20%|High: graph publication and legacy paths interact|E; close one exact routing/UI contract; RT qualification separate below |
| Ten generic effects/shared DSP |Ten real registered processors; smoothing, buffers, tail residuals, finite/partition regressions |Complete original acoustic/alias/mode/CPU grids |20–35%|High: finite output does not establish tonal/alias targets|E; one retained original oracle at a time |
| Phase90 |Actual E58 DSP, reviewed notch/THD evidence; current fade/sync/memory fixes and focused sanitizers |CPU<0.4% RED;24-bit criterion unresolved; registered flow |25–40%|High: repeated behavior-preserving optimizations still miss CPU; click interpretation needs explicit closure|E plus genuine sound/contract decision if residual criterion changes; keep unregistered |
| CE-2 |Actual BBD/clock/mode/control DSP; headroom and memory tests |Stable CPU<0.4%, remaining calibration/transition and registered paths |20–40%|High: retained CPU trials straddle budget|E; reproduce one original budget/acoustic case; older trials not fresh qualification |
| DD-3T |Real delay/tap/sync/ShortLoop; memory, beat latch and held tails corrected |Original0.5%CPU and remaining acoustic/state/registered-host gates |20–40%|High: timing, tails and host automation compose across boundaries|E; unchanged CPU oracle then one end-to-end registered path when gates allow |
| CE-2W |Three actual mode histories; memory and rapid-retarget correction |CE1 wetTHD/color and rate-law criteria; final state/registration |25–45%|High: tonal requirement/calibration remains unsettled|E; original CE1 THD target unchanged; review literal conflicts independently |
| Dyna Comp |Real OTA/detector/OS prototype imported; memory, hostile/reset/partition coverage |Original THD/recovery/FAST attack and0.5%CPU targets |40–60%|Very high: large measured dynamics/CPU errors, not just missing wiring|E; rerun one corrected natural-control oracle before tuning; historical failures not called fresh |
| RC-5 advanced named pedal |Shared RAM/paged core, storage, codec,57×2rhythms/7kits; new prerequisites above |Skeleton wrapper;99memories/settings, full timed gestures, reverse/half-speed, quantize/clock/MIDI/readback and board integration |60–80%|Very high: largest missing multi-subsystem implementation|E; next real record/save/reload path; held second double-Stop press later needs explicit non-destructive policy decision |
| Shared loop storage/lifecycle |Actual paging, bounded budgets/queues, compaction, redo assets and JSON; new opt-in boundary resume tests |Live final-boundary revalidation and error-aware wrapper lifecycle |20–40%|High: old audio advances while off-thread candidate prepares|E; captured snapshot is not live-swap continuity; preserve failures/current working processor |
| Board state/presets/snapshots/assets |Strict atomic JSON, unknown/future preservation,8snapshots,373host intents, SHA assets; redo schema2 migration |Named-model extras lifecycle and current native exact-state closure |15–30%|High: cross-thread state and asset failures must reject atomically|E; named-wrapper roundtrip; O/access for Mac execution; old Mac JSON pair remains unavailable |
| Host automation/MIDI/clock/expression |Stable appended IDs/slots, exact offsets, bounded output, intent arbitration and basic learn |Full PLL/jitter/output clock, learn conflicts, macro/NRPN/scene/bank workflows |30–50%|High: multiple timing authorities and host buffer contracts|E; one specified missing workflow; native DAW follow-up is O |
| Board UI/faces/search/accessibility |Dock/popout, descriptor controls, inspectors, keyboard flows, original faces,7surfaces/mounts, existing GuiReach/lifecycle tests |Complex cabling, genuine RC5 LCD/ring, DPI/touch/a11y/goldens and real-host polish |25–45%|High: missing backend telemetry and native peer behavior|E for backend/UI; O for actual host/device/screen-reader checks; no blanket ref_* exemption |
| Strict callback/concurrency/memory qualification |Existing short edit/audio grind, retirement tests and strict focused component sanitizers; runtime atomics compile-time lock-free |Whole call-graph no-lock proof, legacy try-lock paths, whole-target TSan scope |Unknown|Very high: scope and residual defects not bounded by current instrumentation|E; audit actual call graph. Extra denied UI stress stays unexecuted, never rerouted |
| Full regression/long soak/performance |Historical full Pro pass at f1ef; Free fixture correction; current focused cohorts/logs; old model soaks retained |Final-source both-edition full suites, dedicated long board/MIDI soak, isolated CPU/denormal budgets and preserved compiler warning |50–75%|High: broad source changed since last full product run; hidden failures remain possible|E; stable accepted source first; no duplicate old-source suite; current32tests do not replace it |
| Windows x64 standalone/VST3/CLAP qualification |Existing native pipeline and older6d9b594 successful diagnostics/validators |Current accepted-source builds/full required tests/validators/device checks |80–100% of current qualification|Medium-to-high: pipeline exists, exact-source product result absent|Current no-push/no-CI restriction is O; code failures would be E. No new denial/retry claimed |
| macOS Intel standalone/VST3/CLAP qualification |Older universal compile/link and textual architecture-check fix |Current architecture/runtime/validators, exact JSON/crash evidence and manual hosts |90–100% of current qualification|High: corrected native run outcome unknown after403|O/access plus E if failure; never retry denied40292011494read or claim passed |
| macOS ARM standalone/VST3/CLAP qualification |Same older universal pipeline; no current ARM runtime pass |Actual ARM architecture/runtime/validators/manual host acceptance |90–100% of current qualification|High: universal compilation alone does not test ARM runtime|O/native capacity/authorization; no paid substitute or signing identity creation |
| Linux standalone/VST3/CLAP qualification |Genuine older f1ef products, VST3strictness5/30pairs, CLAP18pass/3unsupported skips; survival smoke |Fresh final-source products/both editions/full gates and actual audio device/DAW |60–90% of current qualification|Medium-to-high: local harness available but final product source not qualified|E locally; O for real device/DAW; current focused tests are not rebuilt product binaries |
| Packaging/Release assets |Existing scripts and older diagnostic bundles/private evidence |New immutable version, qualified payloads, checksums/provenance/install/notices/known issues and publication |70–90%|Medium: finite packaging depends on all source/platform gates|Current no-release/push/CI restriction O; no publication attempted; failed bundles not promoted |
| Manual/signing/notarization |No current manual or signed/notarized pass claimed |Actual device/DAW/listening matrix; signing only where policy requires |Unknown|High: hardware/hosts and identities are not present|O; unsigned/unnotarized prerelease can be disclosed only under applicable project policy/approval; no purchases/credentials/legal agreements |

The platform ranges assess remaining qualification effort, not missing build
source.0/4current accepted cells is the factual result; older successful cells
cannot be transferred to the new tree. The overall range is not computed by
averaging these rows, several of which deliberately describe shared dependencies.

## Other inherited project groups and explicit exclusions

| Group retained from earlier report | Current judgment / remaining | Difficulty / scope |
|---|---|---|
| Existing instrument/techniques/practice/rhythm/tune/content |Substantial code and historical full Pro coverage; fresh final-source/manual qualification remains. Feature-wide remaining% unknown without exhaustive current rubric.|Medium qualification, higher if acoustic defect appears; preserve base compatibility, do not expand pedalboard scope. |
| Whammy-position knob/MIDI wiring |Prior source-supported wiring gap remains unreviewed in this continuation; percentage unknown.|Medium; precedence contract/audible regression needed if separately required. |
| Commercial activation/expanded edition capacities |Production rollout not qualified; remaining% unknown.|High plus owner service/key/terms gates; not invented as a new development-build prerequisite. |
| Extra import dialects/GPX/GPIF, premium/v2 additions, CLI easter egg |Historical plan membership/current completion unverified; remaining% unknown.|High or undefined; outside urgent lean denominator until explicitly required. |
| Non-board visual/support and golden/IR regeneration |Existing mechanisms; precise remaining count unknown. Deterministic regeneration only if final audible changes require it.|Low-to-medium per bounded item; owner details/listening where necessary. |
| Deferred twelve named pedals |DS1,TS808,SD1,Distortion+,RAT,NYC/Deluxe/GreenRussian/WickerMuffs,Ditto,Nano360,RC1; all source/specs/tests/provenance retained.|Explicitly outside six-pedal initial-release denominator, not declared complete or deleted. |

## Source/evidence and restrictions

Primary source: `Source/ReferencePedals/{BoardEngine,TopologyCompiler,PedalPool}.cpp`,
`Models/{mxr_phase90_script,boss_ce2,boss_dd3t,boss_ce2w,mxr_dynacomp,boss_rc5}`,
empty named RegisterModDelay/RegisterDriveB/RegisterRc5 entry points, Common/
PagedLooperCore/LooperStateCodec/Rc5Rhythm/Rc5PressCycle, host/MIDI/UI maps linked
in the prior inventory. Unchanged rows above intentionally reuse those inspected
paths rather than claim a fresh whole-repository audit.

Current evidence: `docs/coverage/PEDALBOARD-ACCEPTANCE.md`; handoffs
`PEDALBOARD-2026-10-27-{DYNACOMP-IMPORT,DYNA-BOUNDARIES,PHASE90-PAIRED-CPU,
PHASE90-MEMORY,DD3T-SYNC,DD3T-TAILS,CHORUS-MEMORY,CE2W-MODE,RC5-RHYTHM-DC,
LOOPER-HISTORY,LOOPER-RESTART,RC5-PRESS,LOOPER-REDO-GRAPH,LOOPER-REDO-JSON,
LOOPER-REDO-WORKER}.md`; current runtime resume evidence at
`/workspace/luthier-evidence/looper-resume-20261027/` with exact commands, hashes,
RED missing-API compile and final32/31,122 results. Historical full/native facts
remain in the prior inventory; none are relabeled current-source acceptance.

At reporting boundary: one coordinator,zero active editing workers,zero running
build/test/upload processes; completed read-only review. Source remains local,
private guarded checkpointing permitted. No source push,CI,merge,deployment or
release,new software/VM,paid model/infrastructure,Claude,credential/security
changes or denied operation retries. Actions403cause remains unknown, not a
proven credential-type limitation. Missing plan bytes and original Mac JSON pair
are not fabricated. Authorship/licenses and software-artwork descriptor remain.
