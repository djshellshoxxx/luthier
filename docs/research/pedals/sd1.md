# Boss SD-1 Super OverDrive (shipped as: Yellow Overdrive)
- Verdict: SKIP (full R/C/semiconductor set could not be corroborated from >=2 independent sources; see Missing)
- Category / position: pre-amp; mono
- Sources: https://forum.pedalpcb.com/threads/maxon-st-9-w-sd-1-inspired-mods-aionfx-cirrus.27740/latest ; https://forum.fractalaudio.com/threads/super-od-clip-type.126105/ ; https://generalguitargadgets.com/pdf/ggg_its8_sd1_bom.pdf
## Signal chain, stage by stage
Partially corroborated only (search snippets; no schematic could be read in this session):
- Asymmetric clipping: three silicon diodes in op-amp feedback, 2 one way and 1 the other (two forum sources agree).
- Diodes listed as 1S2473 on one revision; 1N4148 as modern substitute (single sources).
- Replica BOM mentions R 4k7 and C 0.047 uF, unconfirmed as stock.

Missing / not corroborated:
- Stock BOM, drive/tone values, op-amp part by revision, supply/bias.

No complete per-stage R/C/pot table is given here on purpose: values must not be invented. Re-run with a source that exposes the full ElectroSmash page or a verified schematic/BOM.
## Nonlinearity
2+1 silicon diode asymmetric feedback clipper; Is/n unsourced.
## Linear filters
Not derived: requires the full tone/filter component values listed under Missing.
## Controls
Not documented to circuit level (pot values/tapers uncorroborated unless stated above).
## Modelling plan
Deferred until values are sourced. Expected approach once known: op-amp stage as biquad/ODE with diode-pair nonlinearity (WDF or implicit solve), 4x-8x oversampling.
## Visual reference (face)
Not researched this pass (circuit-level focus); no colours or layouts asserted.
## Known sonic quirks to preserve
Asymmetry produces even harmonics; slightly less mid hump than TS.
## Test vectors
Cannot be stated without component values. To add once sourced: gain at 1 kHz at max drive, -3 dB corners, THD at -20 dBFS.
