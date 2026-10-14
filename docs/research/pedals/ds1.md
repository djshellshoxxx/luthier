# Boss DS-1 Distortion (shipped as: Orange Distortion)
- Verdict: SKIP (full R/C/semiconductor set could not be corroborated from >=2 independent sources; see Missing)
- Category / position: pre-amp; mono
- Sources: https://www.electrosmash.com/boss-ds1-analysis ; https://en.wikipedia.org/wiki/Boss_DS-1 ; https://forum.pedalpcb.com/threads/boss-ds-1-diy-works-wrong.25298/latest
## Signal chain, stage by stage
Partially corroborated only (search snippets; no schematic could be read in this session):
- Input stage, transistor booster, op-amp gain stage with 1N4148 hard-clip diodes, tone/level stage (ElectroSmash outline).
- 1978 original used Toshiba TA7136AP; later Mitsubishi M5223AL (two sources agree).

Missing / not corroborated:
- All R/C/pot values, transistor type and bias, tone network values.

No complete per-stage R/C/pot table is given here on purpose: values must not be invented. Re-run with a source that exposes the full ElectroSmash page or a verified schematic/BOM.
## Nonlinearity
Silicon hard clip about +/-0.7 V; exact transfer unsourced.
## Linear filters
Not derived: requires the full tone/filter component values listed under Missing.
## Controls
Not documented to circuit level (pot values/tapers uncorroborated unless stated above).
## Modelling plan
Deferred until values are sourced. Expected approach once known: op-amp stage as biquad/ODE with diode-pair nonlinearity (WDF or implicit solve), 4x-8x oversampling.
## Visual reference (face)
Not researched this pass (circuit-level focus); no colours or layouts asserted.
## Known sonic quirks to preserve
Boosted mids from transistor stage; bright edge.
## Test vectors
Cannot be stated without component values. To add once sourced: gain at 1 kHz at max drive, -3 dB corners, THD at -20 dBFS.
