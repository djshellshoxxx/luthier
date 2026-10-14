# Boss DM-2 Delay (shipped as: Bucket Echo)
- Verdict: SKIP (component values/topology could not be corroborated from >=2 independent sources via search snippets)
- Category / position: post-amp (time-based); mono
- Sources: 
  - https://www.scribd.com/doc/133292293/BOSS-DM-2 (OCR transcription, unverified)
  - https://vintageandrare.com/product/Boss-DM-2-Delay-Black-Label-114341
  - https://reverb.com/au/item/95419998-dod-fx90-delay-pedal-mn3005-chip-vintage
  - https://forum.allaboutcircuits.com/threads/boss-dm-2-pcb-problem.210371/
## Signal chain, stage by stage
Not documented: full per-stage values were not obtainable. Facts that were found (do not treat as a complete circuit):
- Original: MN3005 4096-stage BBD with MN3101 clock driver; later revision MN3205 + MN3102 (low current). Dated after 1982 for the change.
- Listing figures: MN3005 ~75 dB S/N, ~1% distortion; MN3205 ~67 dB, 0.8%.
- Scribd OCR (unverified): R39 100k, R38 100k, R34 470, C30 1u, R35 1M trim, R42 1M-B pot.
## Nonlinearity
Not established. See missing list below.
## Linear filters
Not established; no closed-form derived because inputs are uncorroborated.
## Controls
Only controls named in sources are listed above; ranges/tapers unverified.
## Modelling plan
Hold until a schematic is obtained. Missing: Companding-free input/output filter values, clock range and delay-time formula (delay = stages/(2 fclk)), BBD gain, feedback network. No second source for any value.
## Visual reference (face)
Not researched in this pass (no verified source). Design traits to be added later; no logos to be reproduced.
## Known sonic quirks to preserve
Not verified from circuit-level data.
## Test vectors
None; cannot be defined without verified values.
