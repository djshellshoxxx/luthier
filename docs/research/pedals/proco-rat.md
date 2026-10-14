# ProCo RAT (shipped as: Black Distortion)
- Verdict: SKIP (full R/C/semiconductor set could not be corroborated from >=2 independent sources; see Missing)
- Category / position: pre-amp; mono
- Sources: https://www.electrosmash.com/proco-rat ; https://forum.pedalpcb.com/threads/muroidea-proco-rat.12237/ ; https://www.coda-effects.com/2016/01/proco-rat-white-face-1985.html
## Signal chain, stage by stage
Partially corroborated only (search snippets; no schematic could be read in this session):
- LM308 non-inverting gain stage, two 1N914 hard-clip diodes, variable low-pass tone ('Filter', ~475 Hz to ~32 kHz per forum), JFET output buffer, 4.5 V virtual ground from divider.
- ElectroSmash list (via snippet): C1,C9 22 nF; C2 1 nF; C3 30 pF; C4 100 pF; C5 2.2 uF; C6,C7 4.7 uF; C8 3.3 nF; R1,R2,R8 1M; R3,R6 1k; R4,R10 47; R5 560; R7 1.5k.
- 560R+4.7uF ~60 Hz and 47R+2.2uF ~1.5 kHz gain-shaping pairs (forum, consistent with list).
- 100k dual distortion pot, 100k filter pot (snippet).

Missing / not corroborated:
- Which parts sit where (netlist), exact pot taper, JFET buffer part and bias, compensation details; a forum warns of wrong homemade schematics. Not independently corroborated beyond ElectroSmash/derivatives.

No complete per-stage R/C/pot table is given here on purpose: values must not be invented. Re-run with a source that exposes the full ElectroSmash page or a verified schematic/BOM.
## Nonlinearity
1N914 pair hard clipping inside LM308 feedback region; LM308 slew-rate limiting is a key part of sound.
## Linear filters
Not derived: requires the full tone/filter component values listed under Missing.
## Controls
Not documented to circuit level (pot values/tapers uncorroborated unless stated above).
## Modelling plan
Deferred until values are sourced. Expected approach once known: op-amp stage as biquad/ODE with diode-pair nonlinearity (WDF or implicit solve), 4x-8x oversampling.
## Visual reference (face)
Not researched this pass (circuit-level focus); no colours or layouts asserted.
## Known sonic quirks to preserve
LM308 GBW/slew limiting, gain-dependent bandwidth.
## Test vectors
Cannot be stated without component values. To add once sourced: gain at 1 kHz at max drive, -3 dB corners, THD at -20 dBFS.
