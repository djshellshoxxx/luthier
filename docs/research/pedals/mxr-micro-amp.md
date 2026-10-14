# MXR Micro Amp (shipped as: Clean Boost (suggest "Micro Boost"))
- Verdict: SKIP (Single source (ElectroSmash BOM) for values; no connectivity or gain equation; naive gain estimate contradicts +26dB spec.)
- Category / position: pre-amp; mono
- Sources: https://electrosmash.com/mxr-microamp ; Dunlop manual (+26dB max, snippet)
## Signal chain, stage by stage
BOM (one source): C1 0.1u, C2 47p, C3 4.7u, C4 1u, C5 15u, D1 1N4001, R1 22M, R2 10M, R3 1k, R4 56k, R5 500k reverse-log, R6 2.7k, R7/R8 100k, R9 470R, R10 10k, U1 TL061. Revisions differ.
## Nonlinearity
Not sourced. Do not model until values/topology corroborated.
## Linear filters
Not sourced.
## Controls
Gain (500k rev-log), max +26dB.
## Modelling plan
Blocked pending sources. Linear gain stage + op-amp rail clip; no oversampling.
## Visual reference (face)
Not researched (circuit research blocked first).
## Known sonic quirks to preserve
Flat, clean; slight HF roll-off.
## Test vectors
None until topology is confirmed.
