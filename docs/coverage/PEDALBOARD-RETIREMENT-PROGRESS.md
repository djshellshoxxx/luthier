# Finite completed-reader retirement contract (pre-implementation)

Source6d9b5943, unchanged SourceTree9f790ebe. One non-nested audio reader; one serialized control writer/reclaimer; stopped-reader+writer lifecycle barrier for prepare/release/reset. All three live-acquiring entry points (syncRouting, beginEvents, process) announce before SC live.load; every persistent borrowed RenderPlan alias is one of the three segment current/pending pairs. Local borrowed references die before scope finish.

## Publication and acknowledgement

Keep an SC audio reader epoch (even between reads, odd during read). At finish publish a separate completed snapshot: store snapshot sequence=odd current epoch (SC), release fence, store six atomic pointers relaxed, store reader epoch=next even (SC), store snapshot sequence=that same even (SC). Thus the snapshot sequence itself is the completed epoch and cannot be mismatched with an independently sampled epoch. Snapshot remains stable throughout the next active reader except its finish.

The control writer first replaces live with an SC store, then loads the reader epoch SC. A retired plan requires the sampled even epoch, or the next even epoch if sampled odd. Retire metadata is control-thread-only and fixed-size; no vector-entry growth/layout change. Existing prepublication reserve/failure atomicity stays unchanged.

Reclaim makes at most two snapshot attempts. It loads an even snapshot sequence SC, six pointer payloads relaxed, an acquire fence, then validates the same sequence SC. The paired writer release fence and reader acquire fence ensure any payload observed from a later write puts that later odd marker before the validation load; mixed payload cannot validate against an older marker. Failed snapshot leaves all ownership intact. It frees only plans whose required completion epoch is <= the snapshot epoch AND whose pointer is absent from all six aliases.

## Safety cases

1. Reader announced before retirement: writer samples that odd epoch or a later completion. Odd requires completion after all that scope's dereferences. If completion already published, retained aliases cover all future use.
2. Reader announces after sampled even retirement epoch: SC live replacement precedes announcement/live acquisition in the SC order, so it cannot acquire the removed plan from live. It can only retain it through an alias already in the completed snapshot.
3. Snapshot copied while a later reader is active: any old eligible plan is either in its inherited retained aliases or already removed before that later reader could acquire live. Any live plan newly acquired then retired during this reader requires a strictly later completed epoch.
4. Never-adopted plans become eligible after the overlapping reader completes; they are absent from the snapshot and can be collected during the next active reader.
5. Six current/pending aliases permit structural transitions across all placements; neither minimum topology version nor just currently rendered pointer is sufficient.
6. Snapshot changes while copied: guard mismatch or odd rejects. Concurrent destruction only occurs control-side after validated eligibility. No raw audio cache outside this inventory may be added without extending it.

## Finite epoch and lifecycle

No modular comparison/half-range assumption. Epochs increase without wrapping. At UINT64_MAX the reader announcement freezes odd permanently; finish publishes no newer snapshot. Required retirement epoch MAX is never eligible (completed epoch<=MAX-1). New publication reports exhaustion/reprepare; a publication already in flight may retire with MAX and stays pinned. Audio keeps using protected owners without allocation/wait/retry. Stopped lifecycle clears live/segment aliases, retires all owners, then resets epoch+snapshot0/null. No outstanding writer snapshot may cross reset. Constructor snapshot0/null is valid because all segment aliases start null.

## Explicit progress condition

After a reader holding a saturated eight-entry queue completes, a later held reader must not prevent reclamation of eligible never-adopted entries. Deterministic actual-engine regression holds first reader, fills exactly8, advances to second held reader, and requires new publication while that second reader remains held. This repeats over multiple completed readers; original first-held-reader saturation/refusal and unchanged model/history remain required. No arbitrary success percentage, queue-limit increase, or requirement to accept unbounded publication while the SAME reader is held.

## Qualification before integration

Original held-reader/fault/allocation/API/routing tests unchanged. Add deterministic progress RED on original implementation; helper epochs near high bit/MAX, exhaustion/reset; copied completed snapshot while next reader active; publication before/after live load, finish/end-before-final-marker, and overwrite concurrency. Native+ASan/UBSan+TSan focused engine closure; original unpaced stress comparison. RetireList limit8 remains. Private BoardEngine layout changes require coherent Board.cpp/header users recompile. Full host/CMake integration remains root-owned later. No universal model-history/JSON OOM claim.

Independent review: host/MIDI and timefx owners found no further protocol blocker under the stated reader/lifecycle assumptions. Host review also identified the control-side race below; its subsequent guard review found no new lock inversion. Reviews are source reviews, not independent reruns.

## Control-side serialization closure

A separate publisher/reclaimer/audio regression reproduced a pre-guard data race (TSan exit66) in retired vector push versus reclaim. Source8 already had the same unguarded publication/reclaim paths; this RED used the private protocol candidate, not an actual host binary. A control-only mutex now serializes publish/reclaim, owner pinning, retirement diagnostics, asset-store mutation and lifecycle internals. Locked internal helpers avoid recursive locking. Lock order is BoardModel -> engine; model snapshots and pedal saveExtra callbacks execute outside the engine guard. Shared runtime pins survive callbacks that reenter model removal and reclaim. No audio path acquires this mutex.

Prepare/release/destruction still require stopped audio and quiescent model mutations. The mutex does not newly promise arbitrary concurrent prepare/edit semantics. The echo ring still has its original single message-thread consumer contract. SaveExtra implementations remain responsible for their own audio-safe published-state readback. These boundaries are explicit, not inferred from TSan success.

The exact finish gap is covered by a synthetic helper-phase test. A stateless friend accesses the actual private stage/commit methods composed by production finish, with test-only waiting between them. There is no production test callback, flag, waiting, or extra object state. This is not a forced actual BoardEngine scheduler window.

## Focused qualification (final source)

- Deterministic original retirement RED:1test/81checks,4failures. Candidate32 held-reader succession cycles pass without changing the original eight-owner/fault test bodies.
- Pre-guard control race RED:TSan exit66, full diagnostic retained.
- Native:149tests/211752checks,0failures,35.86s. Original30s unpaced soak:3000attempts/296rejected,100314blocks/44255193samples,0callback allocations,0invalid samples.
- ASan+UBSan+leak:149tests/208852checks,0failures,64.86s,stderr0. Original30s unpaced:3000/296rejected,18444blocks/8136878samples,0allocations/invalid.
- TSan:retirement12tests/11626checks; independent publisher/reclaimer/capture/audio plus reentrant-pin2tests/1016checks; MIDI publication2tests/3012checks; original30s soak1test/8checks. All exit0,stderr0. TSan soak2998attempts/492rejected,8310blocks/3666095samples,0allocations/invalid. Rejections under instrumentation are retained, not threshold-relaxed.
- All private ReferencePedals/test TUs are instrumented in sanitizer binaries. Shared prebuilt JUCE core/audio_basics/cryptography objects are uninstrumented; no full-system sanitizer claim.
- 1380 Source files are hash-frozen across final runs; original retirement test prefix is byte-identical. No Model source changes, registry additions, queue limit changes or acceptance-threshold relaxations.

This is qualified focused source, NOT integrated root behavior or full-host/cross-platform/release acceptance. New final-source300s/10min stress, coherent CMake all-target/full-host tests and platform validation remain coordinator-owned. Existing root8b300s evidence belongs to the prior safe-but-rejecting implementation. Existing JUCE noexcept var-clone OOM limitation remains separate.


## 2026-10-26 integrated review and regression closure

Adapter SHA `7cc140070429516462b6f8883e345f1905e338ba9252cadd1d321fb814151047` was applied against the latest root Source after patch/tree review. A focused independent review approved epoch/lifetime and progress protocol, then found compile-time `saveExtra` callback reentry could deadlock under the new off-audio control guard. Root added a synchronous callback/context seam around exactly the two active-runtime `saveExtra` call sites. It pins the active shared runtime, unlocks the control guard around callback invocation, and reacquires before all pool continuation/catch paths. No active pool-vector iterator crosses either callback. Model reentry while the BoardModel compiler mutex is held remains unsupported; outer snapshot callback reentry remains supported.

Final-source regressions verify config replacement, explicit snapshot recall, engine reclamation/diagnostics reentry, throwing-callback retry, and existing control/audio concurrency. With the callback seam omitted the snapshot regression reproduced the deadlock (5s watchdog, exit124). With it restored, `RefPedalControlRace` passes5tests/1042checks; `RefPedalRetirement` passes12/11725checks. The broad root selection `RefPedal` passed411tests/12246497checks/219.1s, including seeded native UI10k-operation lifecycle stress and retained Phase90 oracles. These are current local native Linux unit/UI results. New-source TSan, LTO/strictness10 and native-platform requalification remain pending. Keep reader/model lifecycle preconditions and the eight-entry cap explicit.
