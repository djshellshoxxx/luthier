# MXR Distortion+ (M-104) (shipped as: Vintage Distortion Plus)
- Verdict: SKIP (full R/C/semiconductor set could not be corroborated from >=2 independent sources; see Missing)
- Category / position: pre-amp; mono
- Sources: https://www.electrosmash.com/mxr-distortion-plus-analysis ; https://forum.pedalpcb.com/goto/post?id=108908 ; https://www.diystompboxes.com/smfforum/index.php?msg=128786
## Signal chain, stage by stage
Partially corroborated only (search snippets; no schematic could be read in this session):
- Single 741-type op-amp, germanium diodes (1N270 original; 1N34A substitute) to ground, only Output and Distortion pots, no tone control (two sources agree).
- 1M and 4k7 resistors cited, but labelling conflicts between schematics.

Missing / not corroborated:
- Capacitor values, gain-stage network, supply bias, pot values and tapers, diode orientation details.

No complete per-stage R/C/pot table is given here on purpose: values must not be invented. Re-run with a source that exposes the full ElectroSmash page or a verified schematic/BOM.
## Nonlinearity
Germanium diode clipping with 741 slew limit; Is/n unsourced.
## Linear filters
Not derived: requires the full tone/filter component values listed under Missing.
## Controls
Not documented to circuit level (pot values/tapers uncorroborated unless stated above).
## Modelling plan
Deferred until values are sourced. Expected approach once known: op-amp stage as biquad/ODE with diode-pair nonlinearity (WDF or implicit solve), 4x-8x oversampling.
## Visual reference (face)
Not researched this pass (circuit-level focus); no colours or layouts asserted.
## Known sonic quirks to preserve
741 slew/hiss part of character; asymmetrical/soft Ge clip.
## Test vectors
Cannot be stated without component values. To add once sourced: gain at 1 kHz at max drive, -3 dB corners, THD at -20 dBFS.
