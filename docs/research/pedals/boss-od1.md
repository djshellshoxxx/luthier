# Boss OD-1 OverDrive (shipped as: Early Overdrive)
- Verdict: SKIP (full R/C/semiconductor set could not be corroborated from >=2 independent sources; see Missing)
- Category / position: pre-amp; mono
- Sources: https://forum.seymourduncan.com/threads/assymetrical-vs-symmetrical-clipping.85799 ; https://forum.fractalaudio.com/threads/super-od-clip-type.126105/ ; https://tcgakki.com/en/pages/magazine-boss-od1
## Signal chain, stage by stage
Partially corroborated only (search snippets; no schematic could be read in this session):
- Asymmetric 2+1 silicon diode clipping in op-amp feedback (multiple forum mentions); early units use a quad op-amp, later a dual (single source).

Missing / not corroborated:
- No schematic or BOM located at all; JFET results in searches referred to OD-3, not OD-1.

No complete per-stage R/C/pot table is given here on purpose: values must not be invented. Re-run with a source that exposes the full ElectroSmash page or a verified schematic/BOM.
## Nonlinearity
Asymmetric feedback diode clipping; no data.
## Linear filters
Not derived: requires the full tone/filter component values listed under Missing.
## Controls
Not documented to circuit level (pot values/tapers uncorroborated unless stated above).
## Modelling plan
Deferred until values are sourced. Expected approach once known: op-amp stage as biquad/ODE with diode-pair nonlinearity (WDF or implicit solve), 4x-8x oversampling.
## Visual reference (face)
Not researched this pass (circuit-level focus); no colours or layouts asserted.
## Known sonic quirks to preserve
Bright, edgy, asymmetric.
## Test vectors
Cannot be stated without component values. To add once sourced: gain at 1 kHz at max drive, -3 dB corners, THD at -20 dBFS.
