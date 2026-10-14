# Boss CE-2 Chorus (shipped as: Analog Bucket Chorus)
- Verdict: SKIP (full component values/topology not corroborated from >=2 sources via search snippets; see Missing)
- Category / position: pre-amp; mono
- Sources: https://www.electrosmash.com/boss-ce-2-analysis ; https://www.electrosmash.com/mn3007-bucket-brigade-devices ; https://tagboardeffects.blogspot.com/2014/06/boss-ce-2.html ; https://anasounds.com/working-analog-chorus-effect-boss-ce2/ ; diystompboxes threads
## Signal chain, stage by stage
Corroborated only (no invented values):
- MN3007 1024-stage BBD (IC3), MN3101 clock driver (IC4, not a BBD), uPC4558C dual op-amp for in/out, TL022 LFO op-amp (ElectroSmash); MN3007 runs on negative supply, ~-15V per datasheet (forum).
- Input impedance ~407k (470k bias resistor R2); pre-emphasis path 6n8 + 10k giving ~5.7x gain above ~2.4 kHz (secondary write-up); input LPF around 5 kHz (one source, unverified); LFO: Schmitt trigger square wave then integrator.
- Three hardware versions exist.

MISSING (needed before MODEL): clock/BBD delay range, LFO RC and depth, compander/filter values (filter orders), BBD loss, dry/wet mix, version differences.
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
- BBD HF loss, clock noise, LFO triangle shape.
## Test vectors
None yet (require verified values).
