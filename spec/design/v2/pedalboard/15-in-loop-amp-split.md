# In-loop effects: AmpEngine split

**IDs:** PB-12. **ED:** 6.0. **Status:** missing (`AmpEngine::processSample` is one call; in-loop is not possible).

**Summary.** Split `AmpEngine::processSample` so a pedal can sit inside the amp (between the tone stack and the phase inverter), via LoopSend and LoopReturn.

## User-facing behaviour
- In-loop pedals sit on the loop segment of the amp divider (file 17). Pro, from M3 (roadmap editions).
- Before the split lands, in-loop pedals are placed post-amp, as in v1, and the UI says so ("Loop approximated post-amp").

## Engine and data model
- `AmpEngine` (`Source/DSP/Amp/AmpEngine.h`) is one object: preamp, tone stack, phase inverter, power amp, sag. v2 splits `processSample` into `processPreamp` and `processPower`, with LoopSend (after tone stack) and LoopReturn (before phase inverter) between.
- The compiler emits LoopSend and LoopReturn ops as `Sink`/`Source`-like boundaries; the segment between them runs the in-loop pedals.
- Sag and power state are per-sample; the split must not change the state update order, or the unsplit result changes.
- No allocation: loop buffers are preallocated in `prepare`.

## Parameters and data
None.

## State and migration
- `inLoop` field in the board block (roadmap 5.1, `null` when unused). v1 presets have no loop, so they are unaffected.
- A preset with in-loop pedals loaded on a build without the split plays the approximation and shows the notice.

## Edition
In-loop Pro, from M3 (file 05).

## Performance budget
Two extra boundary ops (0.005 each) plus in-loop pedal costs. Amp cost is unchanged.

## Tests
- **PB-12.** After the split, direct send-return (no pedals in the loop) matches the unsplit amp to -90 dB. Before the split, the approximation notice must show.

## Effort and dependencies
ED 6.0 (the `AmpEngine` split, a separate bucket in roadmap section 9). Depends on 14 (LoopSend and LoopReturn nodes), 16 (ops), `amp-cab-ir.md` (in-loop bypass).
