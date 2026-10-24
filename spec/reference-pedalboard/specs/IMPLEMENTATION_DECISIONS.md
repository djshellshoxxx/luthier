# Implementation decisions and contradictory numerical requirements

2026-10-24. This supplements the implementation plan; it does not lower its
acceptance limits. A candidate remains unaccepted while any applicable test
fails. Original criteria and failed measurements remain in the evidence.

## Audible behavior and numerical corrections

| Decision | Original requirements in conflict | Proposed correction | Review and acceptance |
|---|---|---|---|
| E37, candidate | Required legacy oversampler wrapper versus exact latency/bypass matching and flat boost response. The legacy allpass path has approximate DC group delay and measured boost response around−5.13dB at20kHz/48kHz/4x. | Reference-only linear-phase polyphase FIR; preserve the legacy engine oversampler unchanged. Current drive candidate uses257 taps in first2x stage and129 in upper stages, integer128/160/176-sample latency. | Timefx independent read-only review found no indexing/history/RT or latency-composition issue. Original alias and CPU gates are still failing; this candidate is **not integrated or accepted**. FIR flatness alone is insufficient. |
| E38, candidate | GENERIC_PEDALS§1 prescribes HP20Hz and flat±0.3dB down to20Hz; that pole alone gives−3.01dB at20Hz. §2 literal drive/compensation gives default output≈+12.12dB versus±3dB unity acceptance, and exceeds its6dB full-drive loudness claim. | HP2Hz preserves DC removal and original flatness criterion; it extends subbass and makes DC settling10times slower. Prepare-time reference loudness normalization preserves the waveshaper's spectral ratios and original level limits. | Material audible tradeoffs explicitly recorded. Independent reviewer requested both peak and RMS conventions be tested: amplitude0.1258925 means−18dBFS **peak**, not RMS. Calibration and quality evidence remain candidates, not hardware measurements. |
| E40, adopted helper | GENERIC_PEDALS§5 mandates four-point Lagrange interpolation and wet interpolation flat±0.5dB through14kHz/48kHz. At half-sample phase the prescribed cubic loses≈1.9dB at14kHz. | Add16-tap Blackman-windowed sinc read with257 prepared fractional phases and coefficient interpolation. Preserve existing cubic read unchanged. Coefficients initialize off the audio thread, no callback allocation. The chorus may select the bandlimited read; no numeric threshold is weakened. | Original helper regressions pass. Independent DSP dense DFT sweep:4rates×257phases×281frequencies, worst0.003349171dB through14kHz. Relevant tests: RefPedalBandlimitedDelay. Actual chain, CPU and sanitizer gates remain required. |
| WP14 tap range | DD-3T§6/algorithm and implementation-plan gate specify overall12.5–800ms plus halving. §9 instead clamps a500ms tap with QT to the S knob's50ms maximum. | Apply the plan's stated priority: QT yields333.3ms in the overall range. Keep the conflicting original§9 criterion visible. | Dedicated333.3ms regression in worker tests; no narrowed range or timing slack. |
| WP14 headroom, unresolved | Literal CE-2/CE-2W clipping constants conflict with their harmonic-distortion targets in coherent, depth0/noise0 fixtures. | No correction adopted. Verify reference units, signal level convention and real transfer path before proposing a change. | Worker observed CE-2 total THD2.26% at−12dBFS peak versus<0.3%, and CE-2W CE1 wet THD0.806% at0dBFS peak versus1–3%. Named models stay unregistered while gates fail. |

The separate short31-tap wet-path oversampler candidate is required for BBD
paths whose physical minimum delay is shorter than the drive filter latency.
Its2x roundtrip delay is15samples (4x extension23samples). Internal wet delay
must absorb that cost while physical timing, dry behavior and all applicable
alias limits remain unchanged. It must be reviewed and tested in the actual
BBD chain, not accepted from an impulse peak alone.

The antialiasing investigation is informed by the primary
[Parker, Zavalishin and Le Bivic DAFx2016 paper](https://dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf).
Its continuous-time integration approach motivates independent first/second
order experiments; those experiments do not certify a production algorithm.
The coordinator's ideal static-tanh8x oracle finds stronger rejection from
second order than from first-order delay compensation. Production composed
fuzz, parameter modulation, latency, aliasing and CPU still require validation.

## Additive contracts

All additions preserve existing state/parameter identities. Per-lane coverage
contains actual commit and test evidence; an approved interface alone does not
mean its full behavior is implemented or accepted.

| ID | Contract and ownership |
|---|---|
| E32 | IPedal borrowed double sidechain samples for the immediately following process call; null means unavailable, no internal-detector substitution. |
| E33 | MidiRouter message-thread map reclamation after audio readers release snapshots. |
| E34 | BoardModel cloned UI metadata setter, no undo or audio recompile. |
| E35 | BoardWindow non-owning shared BoardRoot reparenting; detach before owner destruction. |
| E36 | ParamWrite slot0xff denotes named board mix/tempo/enabled target. |
| E39 | Board OFF preserves legacy audio and suppresses pedal/raw/output MIDI; clock tracking and enabled binding remain live solely for reenable/sync. |
| E41 | Pedal footswitch engagement policy plus consumed engagement-toggle requests; loopers own record/play gestures, board owns bypass truth. |
| E43 | Validated selected-instance MIDI focus, persisted without undo or audio topology recompilation. |
| E44 | Fixed323-value/serial host snapshot, audio-only facade input, compiled descriptor mapping and persistent audio-owned overrides. Position remains structural. |
| E45 | Borrowed float host sidechain, bounded conversion to prepared double scratch, explicit clearing/availability; real PRE reamp path. |
| E46 | Audio-only host-enabled override for MIDI gating before message-thread mirror catches up. |
| E47 | Audio-only actual per-pedal UI meter POD; engine atomic publication, bounded UI read, instance identity, unavailable flags rather than invented progress. |
| E48 | Model-enabled intent revision enters MIDI before fresh host serials; same-value explicit writes remain observable. |
| E49 | Green Russian width69.9/depth114.3mm resolves inconsistent orientation using control coordinates; RC5 keeps physical73×129mm and external CTL controls live in inspector. Original geometry retained in UI ledger. |
| E50 | Host-origin model persistence has no undo/audio-intent revision; explicit UI writes keep intent revisions. Prevent stale timer echoes from defeating newer audio host writes. |

IDs E42 and other gaps are unassigned. A final spec reconciliation must link
the independent reviews, original/revised conditions, exact accepted source
commit and passing regressions in the release evidence.
