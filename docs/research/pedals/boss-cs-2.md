# Boss CS-2 Compression Sustainer (shipped as: Sustain Comp C)
- Verdict: SKIP (component values/topology could not be corroborated from >=2 independent sources via search snippets)
- Category / position: pre-amp; mono
- Sources: 
  - https://support.roland.com/hc/en-us/articles/201947179-CS-2-Specifications
  - https://www.diystompboxes.com/smfforum/index.php?msg=949834
  - https://forum.pedalpcb.com/goto/post?id=229683
  - https://scrapbox.io/diyfx/BOSS_CS-2 (schematic image, values unreadable)
  - https://www.scribd.com/doc/51130814/COMPRESSOR
## Signal chain, stage by stage
Not documented: full per-stage values were not obtainable. Facts that were found (do not treat as a complete circuit):
- IMPORTANT correction to brief: CS-2 uses a BA662 OTA (SIL9), not CA3080; BA6110 is not a drop-in (different pinout, different sound).
- Roland spec: 9 V battery, 38 dB compression range, 1 MOhm input impedance (FET input).
- Forum: topology close to Dyna/Ross; attack control resembles Keeley approach.
## Nonlinearity
Not established. See missing list below.
## Linear filters
Not established; no closed-form derived because inputs are uncorroborated.
## Controls
Only controls named in sources are listed above; ranges/tapers unverified.
## Modelling plan
Hold until a schematic is obtained. Missing: All component values, BA662 bias network, detector time constants, FET input stage part. No value list retrievable.
## Visual reference (face)
Not researched in this pass (no verified source). Design traits to be added later; no logos to be reproduced.
## Known sonic quirks to preserve
Not verified from circuit-level data.
## Test vectors
None; cannot be defined without verified values.
