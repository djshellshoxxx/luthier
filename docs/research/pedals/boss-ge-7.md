# Boss GE-7 Equalizer (shipped as: Seven Band Graphic)
- Verdict: SKIP (component values/topology could not be corroborated from >=2 independent sources via search snippets)
- Category / position: pre or post; mono
- Sources: 
  - https://cushychicken.github.io/ltspice-boss-ge7-equalizer/
  - https://www.hobby-hour.com/electronics/s/ge7-equalizer.php
  - https://forum.pedalpcb.com/goto/post?id=144018
  - https://www.adorama.com/col/productManuals/BSGEB7.pdf
  - https://avlgear.com/products/boss-ge-7-7-band-graphic-equalizer-guitar-pedal
  - http://dirtboxlayouts.blogspot.com/2023/05/boss-ge-7-eqqualizer.html
## Signal chain, stage by stage
Not documented: full per-stage values were not obtainable. Facts that were found (do not treat as a complete circuit):
- Labelled centres 100, 200, 400, 800 Hz, 1.6, 3.2, 6.4 kHz; +/-15 dB; six peaking filters plus one HF shelf (retail/manual).
- PedalPCB user calculation: the '6.4k' band's parts yield ~4 kHz (single source, unverified).
- LTSpice write-up: input is AC-coupled non-inverting x10 stage; six EQ circuits; no inductors (enclosure 13x7x5.5 cm).
- Op-amps listed: TL022/IR9022 (IC1-3), uPC4558C (IC4).
- A layout commenter reports schematic/layout discrepancies (470R vs 4.7k, missing 100n).
## Nonlinearity
Not established. See missing list below.
## Linear filters
Not established; no closed-form derived because inputs are uncorroborated.
## Controls
Only controls named in sources are listed above; ranges/tapers unverified.
## Modelling plan
Hold until a schematic is obtained. Missing: Per-band R/C values and Q (schematic exists at hobby-hour/LTSpice page but contents not retrievable; one source disputes it anyway). Retry with direct page access could allow MODEL.
## Visual reference (face)
Not researched in this pass (no verified source). Design traits to be added later; no logos to be reproduced.
## Known sonic quirks to preserve
Not verified from circuit-level data.
## Test vectors
None; cannot be defined without verified values.
