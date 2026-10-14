# Dunlop/Vox Cry Baby (GCB-95) wah, Fasel inductor (shipped as: Foot Filter)
- Verdict: SKIP (component values/topology could not be corroborated from >=2 independent sources via search snippets)
- Category / position: pre-amp; mono
- Sources: 
  - https://www.electrosmash.com/crybaby-gcb-95
  - https://guitarscience.net/papers/wahwah.pdf
  - https://forum.pedalpcb.com/threads/what-should-i-do-to-my-wah-pedal.27772/
  - https://diystompboxes.com/smfforum/index.php?msg=506606
  - https://cushychicken.github.io/ltspice-dunlop-crybaby/
  - https://stinkfoot.se/archives/549
## Signal chain, stage by stage
Not documented: full per-stage values were not obtainable. Facts that were found (do not treat as a complete circuit):
- ElectroSmash parts list: L1 500 mH, C2 0.01 uF, R7 33k sets resonance sharpness (Q); sweep roughly 450 Hz to 1.6 kHz, peak gain up to ~18 dB.
- 33k Q resistor corroborated by PedalPCB and DIYStompboxes forum threads (raise to 47k-100k for more Q).
- Dunlop listing (Fasel mode): ~550 Hz heel, 1.2-1.8 kHz toe.
- Plain f0=1/(2 pi sqrt(LC)) with 0.5 H, 10 nF gives ~2.25 kHz, which is not the real sweep: the active feedback stage shifts it; so closed-form needs the full netlist (guitarscience paper has the derivation but the extracted text was garbled).
- Fasel inductor is a 2-lead part; Q/frequency differ from vintage inductors (no values found).
## Nonlinearity
Not established. See missing list below.
## Linear filters
Not established; no closed-form derived because inputs are uncorroborated.
## Controls
Only controls named in sources are listed above; ranges/tapers unverified.
## Modelling plan
Hold until a schematic is obtained. Missing: Full netlist (transistor types/biasing, pot value and taper, emitter-follower resistors, supply), Fasel inductance/DCR/ Q figures, and the exact transfer function. Search gave only partial parts-list items; closed-form not verified by two sources. A derivation from the guitarscience paper could upgrade this to MODEL on a later pass.
## Visual reference (face)
Not researched in this pass (no verified source). Design traits to be added later; no logos to be reproduced.
## Known sonic quirks to preserve
Not verified from circuit-level data.
## Test vectors
None; cannot be defined without verified values.
