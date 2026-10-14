# MXR Phase 90 (script logo) (shipped as: Four-Stage Sweep Phaser)
- Verdict: SKIP (full component values/topology not corroborated from >=2 sources via search snippets; see Missing)
- Category / position: pre-amp; mono
- Sources: https://electrosmash.com/mxr-phase90 ; https://github.com/polimi-ispl/mxrphase90 (WDF model, not read) ; madbeanpedals NomNom PDF ; diystompboxes threads
## Signal chain, stage by stage
Corroborated only (no invented values):
- Four first-order all-pass stages, JFET (2N5952) as voltage-variable resistor, driven by a simple op-amp LFO; dry+wet summed gives two notches (ElectroSmash, Wikipedia, forum).
- ElectroSmash BOM lists 47nF caps (C1-C4,C6,C9), 24k resistors (R6,R23,R25,R26,R28), 2N5952 x4 (Q2-Q5), TL061 op-amps, 2N4125 PNP input (Q1), 56k R4, 250k trimmer. Mapping of each part to stage is NOT confirmed.
- A fixed resistor (~22k per a forum post) in parallel with each FET - conflicts with the 24k in ElectroSmash BOM; unresolved.
- Zener supply regulator; one forum says 5.1V/500mW (single source).

MISSING (needed before MODEL): per-stage R/C mapping, FET bias trimmer network, LFO RC/speed pot, feedback (none on script), mix resistors, supply voltage.
## Nonlinearity
Not established. Do not model from this note.
## Linear filters
Not established.
## Controls
Not established beyond the facts above.
## Modelling plan
Blocked. Next step: obtain the full schematic (ElectroSmash page text, service manual, or a traced schematic archive) and confirm each value in a second independent source; web fetch is unavailable to this worker, only search snippets.
## Visual reference (face)
Not researched (no verified source).
## Known sonic quirks to preserve
- 4 stages sweeping 2 notches; JFET Vgs spread matters (match FETs).
## Test vectors
None yet (require verified values).
