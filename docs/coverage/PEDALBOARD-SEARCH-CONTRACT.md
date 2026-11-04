# Reference pedalboard search contract

Message-thread UI integration for the existing373 append-only host parameters. No parameter ID, index, automation range or saved host value changes. Every ID stays indexed; vacant/unused lanes are reported unavailable with a concrete reason, never credited as a control or silently exempted.

## Resolution and meaning

- Board globals: root enabled/mix/tempo plus the50 declared extended globals. Route to their actual toolbar or owned Settings control, including momentary feedback Kill.
- SlotNN fields: NN means the fixed storage/automation slot, not current visual order. Resolve through the authoritative model's instance.slot. Enable, input/output trim and position route to actual card/inspector controls.
- SlotNN p01..p16: resolve descriptor laneMap.laneToParam. A populated mapped lane routes to that actual descriptor control. Vacant slots, missing models and unused lanes return an unavailable reason; saved APVTS values remain untouched.
- Identity/model replacement invalidates titles/availability. No UI-only model, dummy slider, new host lane or cloned board view.

## Activation, focus and lifetime

Use the editor's existing dock/popout callbacks and the single Drawer-owned BoardRoot. Preserve a currently visible board window; otherwise open the saved mode with the existing small-editor popout fallback. Reveal the target's actual Settings/inspector only after the parent layout settles, then scroll all viewport ancestors, focus the native control and ring it in its own top-level window. Do not run an editor resize that destroys the just-opened popup. Weak references protect targets deleted/rebuilt between lookup, reveal and focus.

Unavailable activation, choice-option application, inline value changes and nudge return a clear refusal and never mutate vacant host values. The search highlighter remains Navigator-owned when reparented to the board window and cannot outlive its editor.

## Finite acceptance

1. Existing GS02 baseline: all373 reference IDs printed as NO CONTROL despite test success; retain this evidence and existing predicates/thresholds.
2. Add independent actual-editor regression: meaningful board globals, physical slot lanes, extended slot lanes, trims/enable/position activate through search in dock and popout; focus/ring actual matching component; operate it and observe its own APVTS write.
3. Enumerate all373 indexed IDs in empty and populated16-slot models. Every meaningful lane has a real route; every vacant/unused lane is explicitly unavailable. No APVTS IDs skipped or invented.
4. Reorder/replace/remove while results exist; resolve current storage-slot identity, reject stale/missing controls, avoid dangling popup/highlighter pointers.
5. Preserve existing GuiReach exhaustive expected-ID checks, SearchEditor predicates and all threshold values. Add explicit raw bound-control semantics rather than weakening LearnTarget identity checks.
6. Focused native tests first; actual fresh-root and sanitizer integration remain qualified by their precise source/object snapshots. No broad release, hardware/DAW or performance claim from these tests.

## Qualified slice and remaining gate (2026-10-25)

- Actual immutable root280 native test objects plus this UI/search slice: **28 tests / 8,641 checks / exit0 / 51.5s**. Five new Reference Search cases exercise actual palette selection, dock/popout focus and ring ownership, real APVTS writes, all373 IDs empty and mixed16 populated, unused-lane refusal, replacement/reorder/removal, and reserved mouse/context/inline/nudge write protection. Existing23 SearchEditor cases pass, including GS02. The original LearnTarget predicates remain; a reference native control must have the exact bound ID and type, focus request and highlight. A popout additionally needs a real BoardWindow ancestor and isShowing(). Missing reference routes fail GS02 instead of entering its legacy no-control diagnostic.
- Initial route RED: 2 tests / 3,368 checks / 962 failures. Palette mouse and Reset RED: 1 test / 10 checks / 4 failures, with reserved value0.37 changing to0.545455 then0.0. Both original logs retained.
- No parameter IDs, indices, ranges, counts, registry models, visibility skips or threshold values changed. The production source glob and test glob include the new files. Candidate ABI uses frozen root280 headers and native test flags; no product SharedCode objects mixed in. All linked inputs and candidate sources are hashed in route-qualified-manifest.json.
- **Full Search remains blocked:** unchanged GS06_contentIndexedAndFresh segfaults (exit139) in both the unchanged actual root280/build6 binary and the candidate, isolated and in a combined run. Its assertions remain unchanged. Only that named case was excluded from the bounded28-case run above. Diagnosis and a separate lifetime fix are pending; this slice does not claim full Search/release acceptance.
- Evidence root: /workspace/luthier-evidence/pedalboard-ui/reference-search-gap/. Key logs: route-red.log, palette-red.log, route-qualified.log, gs06-root280-baseline.log, gs06-isolated.log, search-combined-first.log. GS02 baseline gs02-before.log printed all373 reference IDs as NO CONTROL while passing one final check; the route fixes that product gap rather than treating the diagnostic as coverage.
- Executable SHA256: 867a846a682e07b09237677d7528dddfae5445e939106d05fc332ad387530bbd
- Qualified runtime log SHA256: ee569b8100b842f96a5ffa102aae6fb3589b0cbe78b8fa2789d82a2c8549c40a

Unattended native Xvfb scope only; no hardware/DAW, broad sanitizer or performance acceptance implied. Earlier paint wall-time gates and named-model/manual acceptance remain separate. Deferred telemetry WIP is preserved out of tree, not part of this slice.
