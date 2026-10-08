# Pedal face

**IDs:** none directly (PB-08 pop and PB-11 footswitch are shown here, owned by files 12 and 10). **ED:** 3.0. **Status:** partial (`PedalParam` exists in `Pedal.h` 22-79; no board face).

**Summary.** The per-pedal panel: header with name, bypass mode, pop, tier, footswitch and latency, and one control per `PedalParam`.

## User-facing behaviour
- Header: name; bypass mode selector (Soft, True, Buffered); pop toggle (on by default); tier selector; footswitch (MIDI Learn target); nominal latency (read-only).
- Migrated pedals: bypass Soft, tier Unmodelled. New pedals: bypass True, tier Standard. (**Conflict:** ground rule 7 says new pedals "start soft-bypassed"; section 2.4 says True. Recommended: new pedals use True and start bypassed, so "soft-bypassed" in rule 7 is read as "bypassed". Owner to confirm.)
- Each control has a tooltip stating its effect in one sentence.
- Every control is keyboard reachable (file 11).

## Engine and data model
- Controls bind to the pedal instance's `PedalParam` values (existing), and to state fields for bypass mode, pop, tier, footswitch (file 13).
- Mode changes are structural only when the bypass mode changes the path (True and Buffered change the graph op; Soft does not). Those recompile (file 16); a pop toggle does not.
- Nominal latency is read from the pedal type (file 01); it is not editable.

## Parameters and data
Uses existing pedal parameters. Bypass mode, pop, tier, footswitch are state (section 5), not host parameters.

## State and migration
State fields `bypassMode`, `popEnabled`, `tier` in the board block (file 13). Migrated: Soft, true, Unmodelled.

## Edition
Bypass modes and tiers gated (file 05). Locked options show read-only "Board features need Pro."

## Performance budget
None (UI only).

## Tests
- Covered by PB-08 and PB-09 (file 12) and PB-11 (file 10). No separate PB ID.
- Manual: each tooltip matches its control (review).

## Effort and dependencies
ED 3.0. Depends on 12 (bypass modes), 10 (footswitch), 14 (instance model).
