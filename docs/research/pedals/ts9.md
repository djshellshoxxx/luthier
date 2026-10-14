# Ibanez Tube Screamer TS9 (shipped as: Mint Overdrive)
- Verdict: SKIP (full R/C/semiconductor set could not be corroborated from >=2 independent sources; see Missing)
- Category / position: pre-amp; mono
- Sources: https://forum.pedalpcb.com/threads/only-two-differences-between-the-808-and-ts9.14799/latest ; https://www.diystompboxes.com/smfforum/index.php?msg=103777
## Signal chain, stage by stage
Partially corroborated only (search snippets; no schematic could be read in this session):
- Same topology as TS808 per forum consensus.
- Differences reported: output resistors 470R and 100k (vs 100R/10k); input cap 0.027 uF (vs 0.02 uF); diodes 1S1588 and op-amps varied (JRC4558 intended, many TA75558). One forum thread claims only R17/R18 differ; sources conflict.

Missing / not corroborated:
- Full values, plus resolution of the conflict above; no ElectroSmash TS9 page text was retrieved.

No complete per-stage R/C/pot table is given here on purpose: values must not be invented. Re-run with a source that exposes the full ElectroSmash page or a verified schematic/BOM.
## Nonlinearity
As TS808 (diode feedback clipper); values unsourced.
## Linear filters
Not derived: requires the full tone/filter component values listed under Missing.
## Controls
Not documented to circuit level (pot values/tapers uncorroborated unless stated above).
## Modelling plan
Deferred until values are sourced. Expected approach once known: op-amp stage as biquad/ODE with diode-pair nonlinearity (WDF or implicit solve), 4x-8x oversampling.
## Visual reference (face)
Not researched this pass (circuit-level focus); no colours or layouts asserted.
## Known sonic quirks to preserve
Output-impedance difference vs TS808 affects loading.
## Test vectors
Cannot be stated without component values. To add once sourced: gain at 1 kHz at max drive, -3 dB corners, THD at -20 dBFS.
