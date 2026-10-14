# Dallas Rangemaster Treble Booster (shipped as: Single-Transistor Treble Boost (suggest "Bright Booster"))
- Verdict: SKIP (Sources conflict (5nF vs 6.8nF input; 3.9k vs 3.6k/68k vs 51k bias; output 8.2nF vs 25uF-tonepad); no authoritative schematic retrieved.)
- Category / position: pre-amp; mono
- Sources: https://electrosmash.com/dallas-rangemaster (not retrieved) ; https://diy-fever.com/effects/dallas-rangemaster/ ; https://www.tone3000.com/tones/dallas-rangemaster-treble-booster-ltspice-87264 ; diystompboxes msg=730617
## Signal chain, stage by stage
Single PNP Ge (OC44 mostly; OC71, NKT275 alt); bias R 3.9k giving about -6.6V collector (LTspice author); input cap 5nF-ish; 10k log pot.
## Nonlinearity
Not sourced. Do not model until values/topology corroborated.
## Linear filters
Not sourced.
## Controls
Boost (10k log).
## Modelling plan
Blocked pending sources. Single-BJT nodal when sourced.
## Visual reference (face)
Not researched (circuit research blocked first).
## Known sonic quirks to preserve
Strong low-cut; bias-dependent.
## Test vectors
None until topology is confirmed.
