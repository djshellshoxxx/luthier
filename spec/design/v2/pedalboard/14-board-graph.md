# Board graph and validation

**IDs:** PB-01 (wiring is order), PB-02 (split and merge), PB-13 (validation). **ED:** 6.0. **Status:** missing (v1 is two fixed racks; `moveSlot` works only inside one chain).

**Summary.** A value-type graph of nodes and cables that is the single source of signal order, with validation that rejects bad boards before compile.

## User-facing behaviour
- Any number of pedals, each with jacks, wired by cables. Wiring is the order (ground rule 1).
- Node kinds: Source (guitar DI, Aux 1, `routing-io.md` 2), Pedal, Split (1 in, 2 out, each output with a level), Merge (2 in, 1 out), AmpIn (one mono jack, exactly one per board), Sink (OUT L and OUT R), LoopSend and LoopReturn (in-loop, file 15, Pro).
- Notices (PB-13): empty board, dead end, unmerged split, over budget (amber, still plays), stereo into AMP IN. Cycles are rejected.

## Engine and data model
- `BoardModel` (`Source/Model/Board/BoardModel.*`): plain value type, copyable, no JUCE audio objects. Holds nodes, cables, split and merge ids, pedal instances, layout.
- Validation (message thread): acyclic, every input connected or flagged, every split merged or flagged, one AmpIn, Sink reachable from Source, budget (file 16 estimate). Returns a list of notices with ids.
- Merge rule: a plain sum of its inputs (mono). **Finding for PB-02:** a 0 dB split re-merged sums two full copies, so the output is +6.0 dB, not dry. The test as written (-90 dB match to dry) fails. Recommended: PB-02 asserts `merge == 2 x dry`, or split levels are set to -6 dB per branch. Owner to decide; the spec does not say.
- Depth and width bound the buffer pool: 24 pedals bound depth, 8 bound width (file 16).

## Parameters and data
Split level parameters are file 03. Nothing else.

## State and migration
Graph (de)serialised in the board block (file 13). Migrated graph is a linear chain (13, migration step 4).

## Edition
Splits, merges, in-loop and the 24-pedal limit are gated by file 05 at validation.

## Performance budget
Validation is off the audio thread; the graph itself costs nothing at runtime beyond the ops (file 16).

## Tests
- **PB-01.** A to B and B to A differ by more than -60 dB RMS; a canvas move is bit-identical (positions are not in the graph's signal path).
- **PB-02.** Split and merge as stated above, with the amended expectation.
- **PB-13.** Each notice fires on a constructed board; cycles rejected.

## Effort and dependencies
ED 6.0. Depends on 02 (seams). Blocks 03, 04, 06, 07, 09, 10, 11, 13, 15, 16, 17.
