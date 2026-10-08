# Edition gating (Free vs Pro)

**IDs:** PB-E01. **ED:** 2.0. **Status:** partial (`editions.md` 2.2 exists; no board gating).

**Summary.** Free boards are limited to 8 pedals on the board. Pro features show read-only with a reason on Free.

## User-facing behaviour
- Free: 8 pedals, host-automatable instances 0-7, soft bypass only, Standard tiers only, no splits, merges, in-loop, Buffer or Ground Isolator, no board save (load only).
- Pro: 24 pedals, instances 0-15 automatable, all bypass modes, all tiers, all utilities, board save, in-loop from M3.
- Locked items in the library read "Pro" (file 17). Locked settings show read-only with "Board features need Pro."
- A Free board renders exactly as Pro would with the same pedals.

## Engine and data model
- A single `EditionPolicy` query (`canAddPedal`, `canUseBypass(mode)`, `canUseTier(t)`, `canUseSplit`, `canUseMerge`, `canUseLoop`, `canUseUtility(t)`, `canSaveBoard`, `maxPedals`) read by the validator (file 14) and UI. The engine does not branch on edition in `process`.
- Gating happens in edit validation (message thread). A Pro-only feature in a loaded Free board is kept, rendered, and flagged, not removed.
- Rendering on Free equals Pro with the same content: no DSP difference.

## Parameters and data
None.

## State and migration
- A Pro board opened on Free keeps all pedals and cables; only edits are restricted until the features are removed.
- Board save on Free is disabled; load is allowed.

## Edition
This item is the edition logic itself.

**Open question (roadmap 10.2).** `editions.md` 2.2 says "15 of 22" pedal types; the enum has 23 types and v2 adds three (26 total). Section 6 says Free allows 8 pedals (4 + 4 slots today). These are two different counts (types vs. instances on the board). Decision needed: write the Free pedal-type list out in full in `editions.md`, marking Gater and Doubler explicitly, and confirm the 8-pedal board limit.

## Performance budget
None (policy checks run on the message thread).

## Tests
- **PB-E01.** Free: a ninth pedal is refused, a split is refused, a Standard-only tier is enforced, no board save.
- Add: a Pro board loaded on Free renders identically to Pro (bit-identical output).

## Effort and dependencies
ED 2.0. Depends on 14 (validator) and 13 (save/load).
