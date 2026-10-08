# Cable popover

**IDs:** none (UI; cable tiers are verified in CB-07 of `cables-and-brands.md`). **ED:** 2.0. **Status:** missing (global `cable_quality` and `cable_length` exist).

**Summary.** A popover on each patch cable edits its tier, length, balance and delete. The guitar input cable edits the existing global parameters.

## User-facing behaviour
- Click a cable's tier badge or its midpoint to open the popover.
- Fields: tier (Unmodelled, Economy, Standard, Pro, Boutique), length (0.3-10 m, default 3 m), balanced (on/off), delete.
- Tier meaning per `cables-and-brands.md`. New cables default to Standard; migrated cables are Unmodelled.
- The guitar input cable (source to first pedal or AMP IN) edits `cable_quality` and `cable_length`, the existing v1 parameters, instead of cable state.
- Each edit is one undo entry (`action-and-undo.md` 3.13).

## Engine and data model
- Cable model: `{ id, from, to, tier, lengthM, balanced }` in `BoardModel` (file 14). Tier and length feed the cable's filter, which the compiler computes (file 16); popover edits are structural edits and recompile.
- Length and tier changes recompile only the affected input's filter coefficients if the compiler supports it; otherwise a full compile (still off-thread).
- Delete is a structural edit: the cable is removed and the input is left unconnected (validation, PB-13 notice).

## Parameters and data
No host parameters. Cable state only (`file-formats`).

## State and migration
- Board block cable entries (file 13). Migrated cables: Unmodelled, length as in v1 input cable, balanced false.
- The input cable's values stay in `cable_quality` and `cable_length`; do not duplicate them into the board block.

## Edition
- Free: Standard tier only. Other tiers show read-only with "Board features need Pro." Length and balance editable.
- Pro: all tiers.

## Performance budget
Cable filters: per cable one short filter (small, included in routing estimate, file 14). No change to totals.

## Tests
Covered by file 14 and file 05 for structure; the popover itself has no PB ID. Manual check under PB-U01 (keyboard path, file 11). The cable-tier audibility is PB-09 (file 12).

## Effort and dependencies
ED 2.0. Depends on 14 (cable model), 16 (recompile), 11 (keyboard), 05 (tier gating).
