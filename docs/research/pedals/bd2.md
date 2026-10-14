# Boss BD-2 Blues Driver (shipped as: Blue Blues Drive)
- Verdict: SKIP (full R/C/semiconductor set could not be corroborated from >=2 independent sources; see Missing)
- Category / position: pre-amp; mono
- Sources: https://www.premierguitar.com/gear/boss-bd-2-mods ; https://www.diystompboxes.com/smfforum/index.php?msg=764967 ; https://www.boss.info/sk/products/bd-2/
## Signal chain, stage by stage
Partially corroborated only (search snippets; no schematic could be read in this session):
- Discrete buffer, two cascaded discrete op-amp-like stages (FET pair + bipolar), dual-gang 250k gain pot, then IC op-amp recovery/bass boost, Fender-style 3-band-type tone stack with fixed values.
- R31/C22 set a corner just above 700 Hz; R35 1M gives ~72 Hz HPF (as reported).
- Boss spec: input 1 Mohm, nominal levels -20 dBu.

Missing / not corroborated:
- Transistor/FET part numbers and bias, almost all R/C values, tone stack values, clipping diode details.

No complete per-stage R/C/pot table is given here on purpose: values must not be invented. Re-run with a source that exposes the full ElectroSmash page or a verified schematic/BOM.
## Nonlinearity
Discrete FET/BJT stage soft clipping; parts and bias unsourced.
## Linear filters
Not derived: requires the full tone/filter component values listed under Missing.
## Controls
Not documented to circuit level (pot values/tapers uncorroborated unless stated above).
## Modelling plan
Deferred until values are sourced. Expected approach once known: op-amp stage as biquad/ODE with diode-pair nonlinearity (WDF or implicit solve), 4x-8x oversampling.
## Visual reference (face)
Not researched this pass (circuit-level focus); no colours or layouts asserted.
## Known sonic quirks to preserve
Touch-sensitive, gritty bass-heavy drive.
## Test vectors
Cannot be stated without component values. To add once sourced: gain at 1 kHz at max drive, -3 dB corners, THD at -20 dBFS.
