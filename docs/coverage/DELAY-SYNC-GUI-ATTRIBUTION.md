# Delay SYNC reachability attribution

The Delay SYNC control is present and writes its assigned host lane. The GUI
walker attributed some operations to unrelated earlier position notifications.
This checkpoint changes test attribution only; no descriptor, DSP, control,
attachment, host ID, visibility exception, or acceptance assertion changes.

## Reproduction and cause

Exact original native source: `b5bec374f01ede236350300770f5721709a39831`.
`everyAutomatableParameterHasAVisibleControl` under Xvfb fails 24 of 486 checks:
18 assertions for nine Delay SYNC lanes, plus six separate board scalar checks.
GitHub run 40109481251, Linux Pro job 120918194917 reports the same nine SYNC
slots: 1, 3, 5, 7, 9, 11, 13, 15, 16.

`PedalRegistry::buildLaneMap` maps Generic Delay descriptor parameter 7,
`gen_delay_sync`, to `ref_sNN_p08`. PedalView creates its actual enum ComboBox,
binds it to that host lane, and places it in the inspector. Instrumenting the
unchanged native walker shows all 16 controls visible and not previously
operated when visited. The initial raw-address-reuse hypothesis was disproved;
no deduplication change was made.

Every actual SYNC operation emits its own lane. Some also flush pending position
callbacks first. For example:

```
P08_WRITES ref_s01_p08 ref_s02_pos ref_s01_p08
P08_WRITES ref_s03_p08 ref_s02_pos ref_s04_pos ref_s03_p08
P08_WRITES ref_s04_p08 ref_s04_p08
```

The existing anonymous-control heuristic credits only the first ID. It therefore
credited a position lane even when the named SYNC binding had demonstrably
written its own value.

## Correction and scope

Capture the raw control's ComponentID before operating it (an operation can
rebuild or delete the component). If that exact ID is a registered parameter
and appears in the observed write list, credit that binding. Keep the existing
anonymous/style/custom fallback when no such binding was observed. Being
registered or visible alone is insufficient: the actual write must occur.

All original expected-reference-parameter, visible-control and operated-control
assertions remain intact. The correction neither clears pending intent nor
changes notification order. UI owner reviewed the mechanism and confirmed no
overlapping walker edits.

## Evidence

- `probes/delay-sync-guireach-original-b5.txt`: unchanged original failure.
- `probes/delay-sync-guireach-attribution.txt`: all 16 actual emitted-ID lists.
- `probes/delay-sync-guireach-candidate-b5.txt`: final corrected native run.

The corrected test removes all 18 SYNC failures. Six unrelated board scalar
visibility failures remain open; this is not a fully green GuiReach claim.
`operatingEachControlWritesItsParameter` also passes. Compilation replaces only
the test translation unit in an immutable b5 source snapshot and links a
separate binary against frozen native product objects. Full diagnostic source,
commands and logs remain under
`/workspace/luthier-evidence/timefx-cq03-p08/diagnostic`.
