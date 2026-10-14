# Fuzz Face, silicon NPN (shipped as: Silicon Two-Transistor Fuzz (suggest "Sharp Fuzz"))
- Verdict: SKIP (Only one source (ETO schematic note) says 470R->330R and polarity reversal; transistor part numbers (BC108/BC183 etc.) and values not corroborated.)
- Category / position: pre-amp; mono
- Sources: https://www.electro-tech-online.com/custompdfs/2005/11/fuzzface.pdf (single); https://electrosmash.com/fuzz-face (snippet only)
## Signal chain, stage by stage
Shares Ge values above except 330R in place of 470R, reversed polarity (one source). Nothing else corroborated.
## Nonlinearity
Not sourced. Do not model until values/topology corroborated.
## Linear filters
Not sourced.
## Controls
Fuzz, Volume.
## Modelling plan
Blocked pending sources. Same as germanium topology once confirmed, with Si Ebers-Moll.
## Visual reference (face)
Not researched (circuit research blocked first).
## Known sonic quirks to preserve
Gatier, brighter than Ge; less temperature drift.
## Test vectors
None until topology is confirmed.
