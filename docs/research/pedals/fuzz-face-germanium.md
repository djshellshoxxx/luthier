# Fuzz Face, germanium PNP (Dallas Arbiter, 1966-style) (shipped as: Germanium Two-Transistor Fuzz (suggest "Round Fuzz"))
- Verdict: SKIP (Component values appear in 2 sources, but the netlist (which node each part connects to) was NOT confirmed from search snippets; WebFetch unavailable. Re-run with page access; likely flips to MODEL.)
- Category / position: pre-amp (wants guitar pickup impedance); mono
- Sources: https://electrosmash.com/fuzz-face ; https://www.electro-tech-online.com/custompdfs/2005/11/fuzzface.pdf ; https://coda-effects.com/p/circuit-analysis-fuzz-face.html ; diystompboxes threads msg=897639, msg=381538 (forum bias discussion)
## Signal chain, stage by stage
Corroborated by >=2 sources (snippet level): 9V supply (positive-ground, PNP); resistors 470R, 33k, 8.2k, 100k; input cap 2.2uF; 20uF electrolytic (some builders cite 22uF); output cap 0.1uF (some units .047uF; one forum builder prefers 0.01uF); volume pot 500k (one variant 250k); Q1,Q2 = AC128 PNP Ge, hfe quoted 45-165 or 55-175 by different sources; ElectroSmash suggests Q1 beta ~70-80, Q2 ~110-130. NPN version: the starred 470R becomes 330R, polarity reversed (ETO schematic only). Fuzz pot value (believed 1k) NOT confirmed in snippets. Q2 collector target ~4.5-5V (forum consensus, multiple threads). Connectivity: NOT verified here.
## Nonlinearity
Not sourced. Do not model until values/topology corroborated.
## Linear filters
Not sourced.
## Controls
Fuzz and Volume only. Fuzz pot value unconfirmed; Volume 500k (alt 250k).
## Modelling plan
Blocked pending sources. Expected: nodal/ODE with Ebers-Moll Ge PNP incl. leakage, 4-8x oversampling.
## Visual reference (face)
Not researched (circuit research blocked first).
## Known sonic quirks to preserve
Strong interaction with source impedance (cleans up with guitar volume); sensitive to Q2 collector bias.
## Test vectors
None until topology is confirmed.
