## amp-cab-ir.md

Amp and cab bypass switches plus a quick IR picker on the CAB panel. Codex-owned lane (no branch on origin carries it; no `amp_bypass`/`cab_bypass` parameter exists on this checkout). Only the canonical IR slot editors in the TONE MATCH panel exist today.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| AC-0 (§0) | Ground rules, scope, no audio-path change beyond bypass | - | - | - | OWNED |
| AC-1 (§1) | Amp bypass (AmpEngine skipped, crossfade, state freeze) | - | - | - | OWNED |
| AC-2 (§2) | Cab bypass (cab stage skipped before RoomEngine) | - | - | - | OWNED |
| AC-3 (§3) | Where the switches live (CAB/AMP panels, rig strip) | - | - | - | OWNED |
| AC-4 (§4) | Quick IR picker on CAB panel mirroring the canonical slot | `Source/UI/ToneMatchPanel.cpp:IrSlotEditor` (cabSlot1/2, bodySlot) | TONE MATCH tab only, `IrSlotEditor`; no CAB-panel combo | `ToneMatch::cabinetSlotsReplaceTheirOwnMic`, `ToneMatch::anEngagedBodyIrChangesTheSound` | PARTIAL |
| AC-5 (§5) | CPU-quality awareness, latency constant, Auto independent | - | - | - | OWNED |
| AC-6 (§6) | Serialization, undo, accessibility of the switches | - | - | - | OWNED |
| AC-7 (§7) | Interactions with other specs | - | - | - | OWNED |
| AC-8 (§8) | Failure modes | - | - | - | OWNED |
| AC-T1 (§9 RIG-01..06) | Bypass signal path, click-free, state freeze, latency, CPU | - | - | - | OWNED |
| AC-T2 (§9 RIG-07..09) | Picker mirror, Mix knob identity, acoustic retitle | - | - | - | OWNED |
| AC-T3 (§9 RIG-10..13) | Auto-quality independence, serialization, undo, a11y | - | - | - | OWNED |
