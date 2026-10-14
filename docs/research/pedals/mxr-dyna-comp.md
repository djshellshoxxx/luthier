# MXR Dyna Comp (shipped as: Sustain Comp)
- Verdict: SKIP (component values/topology could not be corroborated from >=2 independent sources via search snippets)
- Category / position: pre-amp; mono
- Sources: 
  - https://electrosmash.com/mxr-dyna-comp-analysis (primary; full parts list is on the page but not retrievable via search snippets)
  - https://www.dafx.de/paper-archive/2011/Papers/22_e.pdf (DAFx 2011 paper with labelled schematic; values only partly visible)
  - https://www.komponenten.es.aau.dk/fileadmin/komponenten/Data_Sheet/Linear/CA3080.pdf (CA3080 datasheet)
  - https://cushychicken.github.io/ltspice-mxr-dyna-comp-compressor/ (LTSpice sim, not read)
  - https://www.diystompboxes.com/smfforum/index.php?msg=807433 (forum: transistor swaps, 2N5088)
## Signal chain, stage by stage
Not documented: full per-stage values were not obtainable. Facts that were found (do not treat as a complete circuit):
- Five blocks per ElectroSmash: input buffer, CA3080 OTA gain stage, envelope detector, output stage, power supply.
- Input buffer: C1 10 nF, R2 1 MOhm bias, R1 10k (ElectroSmash quote via mirror); 2N5088 beta 300 used in analysis; input impedance ~1 MOhm. Single source (mirror of same page).
- OTA output cap C5 1 nF smooths HF hash (single source).
- Forum: CA3080 starts distorting above ~100 mV differential input, hence input attenuation network in front of OTA (single forum source; values not found).
- Original transistors reportedly 2SC1849-type; 2N5088 common substitute (ssguitar/diystompboxes).
## Nonlinearity
Not established. See missing list below.
## Linear filters
Not established; no closed-form derived because inputs are uncorroborated.
## Controls
Only controls named in sources are listed above; ranges/tapers unverified.
## Modelling plan
Hold until a schematic is obtained. Missing: Envelope detector values and diodes (one forum post claims diodes are only protection), OTA bias resistors, attack/release RC, output level pot network, supply and bias voltages. Only one primary source (ElectroSmash) actually exists; no second independent full-value source retrieved. Revision differences (e.g. missing 10 uF, no Q5) also reported.
## Visual reference (face)
Not researched in this pass (no verified source). Design traits to be added later; no logos to be reproduced.
## Known sonic quirks to preserve
Not verified from circuit-level data.
## Test vectors
None; cannot be defined without verified values.
