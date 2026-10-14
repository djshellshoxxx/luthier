# Boss TR-2 Tremolo (shipped as: Opto-Style Gain Tremolo (VCA))
- Verdict: SKIP (full component values/topology not corroborated from >=2 sources via search snippets; see Missing)
- Category / position: pre-amp/post-amp; mono
- Sources: https://mirosol.kapsi.fi/?p=556 ; https://music-electronics-forum.com/forum/effectification/guitar-effects/13103-mods-to-reduce-volume-drop-in-a-boss-tr-2 ; boss.info TR-2 spec page
## Signal chain, stage by stage
Corroborated only (no invented values):
- Note: TR-2 uses an M5207 VCA (not an opto), M5218/NJM4558 op-amps for LFO and gain recovery.
- R12 (IC2A feedback) 22k stock (two sources agree). Spec: input Z 1M, output Z 1k, nominal -20 dBu, LFO period 90-900 ms (retailer).
- R9 is a trimpot on some units.

MISSING (needed before MODEL): complete schematic values, waveform shaping, VCA control law. Fender-style bias/opto tremolo not researched at all.
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
- Volume drop when engaged; VCA control-voltage leakage.
## Test vectors
None yet (require verified values).
