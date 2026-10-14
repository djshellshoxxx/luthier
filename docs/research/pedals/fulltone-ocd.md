# Fulltone OCD (shipped as: Amber Overdrive)
- Verdict: SKIP (full R/C/semiconductor set could not be corroborated from >=2 independent sources; see Missing)
- Category / position: pre-amp; mono
- Sources: https://dafx.de/paper-archive/details/FR5oopmtXLuPh9fiskDhrg ; https://www.dafx.de/paper-archive/2026/papers/DAFx26_paper_24.pdf ; https://forum.pedalpcb.com/goto/post?id=122472 ; https://diystompboxes.com/smfforum/index.php?msg=402054
## Signal chain, stage by stage
Partially corroborated only (search snippets; no schematic could be read in this session):
- Op-amp gain stage followed by MOSFET-based asymmetric clipping; a 1N34A germanium diode in the clipper (not certain in every revision; V3 may differ); HP/LP voicing switch. A DAFx paper models V2 with a WDF/piecewise-linear approach.

Missing / not corroborated:
- All component values, MOSFET part types, revision differences, HP/LP network. Only secondary descriptions seen.

No complete per-stage R/C/pot table is given here on purpose: values must not be invented. Re-run with a source that exposes the full ElectroSmash page or a verified schematic/BOM.
## Nonlinearity
MOSFET (body diode) plus Ge diode asymmetric clipper; parameters unsourced.
## Linear filters
Not derived: requires the full tone/filter component values listed under Missing.
## Controls
Not documented to circuit level (pot values/tapers uncorroborated unless stated above).
## Modelling plan
Deferred until values are sourced. Expected approach once known: op-amp stage as biquad/ODE with diode-pair nonlinearity (WDF or implicit solve), 4x-8x oversampling.
## Visual reference (face)
Not researched this pass (circuit-level focus); no colours or layouts asserted.
## Known sonic quirks to preserve
Revision-dependent behaviour; V1/V2/V3 differ.
## Test vectors
Cannot be stated without component values. To add once sourced: gain at 1 kHz at max drive, -3 dB corners, THD at -20 dBFS.
