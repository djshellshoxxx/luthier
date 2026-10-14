# Klon Centaur (shipped as: Gold Transparent Drive)
- Verdict: SKIP (full R/C/semiconductor set could not be corroborated from >=2 independent sources; see Missing)
- Category / position: pre-amp; mono
- Sources: https://www.electrosmash.com/klon-centaur-analysis ; https://www.coda-effects.com/p/klon-centaur-circuit-analysis.html ; https://www.rig-talk.com/forum/threads/deep-dive-all-about-the-klon-centaur-germanium-clipping-diodes.267175/
## Signal chain, stage by stage
Partially corroborated only (search snippets; no schematic could be read in this session):
- Split clean and saturated paths recombined by a summing stage; TL072 op-amps; germanium clipping diodes (1N34A type, measured Vf ~0.35 V in one build); feedforward networks; charge-pump style supply (analysis section outline).
- Mod posts cite C15 3.9 nF and R23 4.7k but are labelled mods, labels may differ.

Missing / not corroborated:
- All stock R/C/pot values, feedforward network, tone network, supply voltage (the ElectroSmash tables were not visible).

No complete per-stage R/C/pot table is given here on purpose: values must not be invented. Re-run with a source that exposes the full ElectroSmash page or a verified schematic/BOM.
## Nonlinearity
Germanium diode shunt clipping after gain op-amp; Is/n unsourced.
## Linear filters
Not derived: requires the full tone/filter component values listed under Missing.
## Controls
Not documented to circuit level (pot values/tapers uncorroborated unless stated above).
## Modelling plan
Deferred until values are sourced. Expected approach once known: op-amp stage as biquad/ODE with diode-pair nonlinearity (WDF or implicit solve), 4x-8x oversampling.
## Visual reference (face)
Not researched this pass (circuit-level focus); no colours or layouts asserted.
## Known sonic quirks to preserve
Clean blend: low gain stays transparent; diodes mostly inactive at low gain.
## Test vectors
Cannot be stated without component values. To add once sourced: gain at 1 kHz at max drive, -3 dB corners, THD at -20 dBFS.
