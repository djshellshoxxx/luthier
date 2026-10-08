# Instance binding and footswitches

**IDs:** PB-10 (identity), PB-11 (footswitch). **ED:** 3.0. **Status:** partial (v1 `preN_*`/`postN_*` exist; MIDI Learn exists; no instance binding beyond slot numbers).

**Summary.** Each pedal is an instance with a stable ID; automation and footswitches address the instance, not its place on the board.

## User-facing behaviour
- Instances 0-7 are `pre1`-`pre8`; 8-15 are `post1`-`post8`. Display names read "Pedal 1" etc.; IDs do not change.
- Instances 0-15 are host-automatable (Pro; Free 0-7).
- Instances 16-23 are saved and footswitchable but not automatable (roadmap 10.1).
- Deleting a pedal frees its instance and clears its automation after confirmation.
- Footswitch table: up to 24 entries, each a MIDI note (on/off) or CC (above 64 is on).
- Instances 0-15 write a standard MIDI Learn mapping to `preN_bypass`.
- A reserved note range (MIDI 0-23 by default) cannot be learned.

## Engine and data model
- `BoardModel` holds `instance` per pedal; the compiler maps instance to its parameter block (`Parameters.*`).
- Moving a pedal changes its place, never its instance. Automation reads by instance (roadmap ground rule 3).
- Footswitch dispatch on the message/MIDI thread writes a bypass request; the audio thread picks it up at the block boundary through the existing parameter smoother. A note-on and note-off within 5 ms must not double-toggle: debounce on the message side (edge-triggered, one toggle per note-on).
- No allocation in MIDI handling beyond the existing message path.

## Parameters and data
- Existing `preN_bypass`/`postN_bypass`, plus instance-16-to-23 bypass held as state (not host parameters).
- Footswitch table: state, 24 entries max.

## State and migration
- Instances 0-15 keep values in `preN_`/`postN_` keys. Instances 16-23 keep ten normalised values in `pedals[i].params` (file 13).
- v1 MIDI Learn mappings carry over unchanged.

## Edition
Free: instances 0-7 automatable; footswitches for 0-7 only. Pro: 0-15 automatable.

## Performance budget
None measurable (dispatch is per event).

## Tests
- **PB-10.** Automate `pre3_p2`, move instance 2 on the board, save, reload: value and automation kept.
- **PB-11.** A note toggles only its instance; note-on and note-off within 5 ms do not double-toggle.

## Effort and dependencies
ED 3.0. Depends on 13 (state), 14 (instance model), 12 (bypass).
