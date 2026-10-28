# CE-2 endpoint experiment and continuation (2026-10-28)

Source baseline: `a8f878d509ed52163331d899b7e77b9545ae9cf2`, a descendant of
`1e7aaf1c939da793ca4d3d3f37a6c627a42aa539`. The local working tree retained its
pre-existing untracked build and Python cache directories. PR 83's source has
reverted the fitted CE-2 nonlinearity in `6e2cb7f7`; its original BBD-on THD
gate remains red. The additional high-level gate is retained. UI lifecycle and
paint diagnostics have a separate native owner; do not duplicate that work.

The private endpoint experiment frozen at `1e7aaf1c` built successfully and
completed 12 commands, 120 finite renders, at five rates, two input levels,
two observation windows, and BBD off/on. It mixed the existing E51 and literal
tanh transfer endpoints at three fixed weights. The current CE-2 and BBD source
files match the frozen production files; current tests include PR 83's added
high-level gate. This was an isolated experiment, not a product change.

| Weight | BBD-on −12 dBFS THD2–9 | BBD-on −3 dBFS THD2–9 |
|---|---:|---:|
| 0 | approximately 0–0.009750% | 2.01491–2.41809% |
| 0.5 | 0.889672–1.073021% | 6.500674–7.607029% |
| 1 | 1.773888–2.126281% | 10.605924–12.038474% |

Original limits remain 0.5–3% and 2–6%, respectively. The sampled weights
that satisfy the low-level range miss the high-level range. This is not proof
that every intermediate weight fails, and it supplies no physical calibration
rationale. No threshold, DSP or registration was changed. Full raw results,
commands and provenance are in the private local evidence directory
`/workspace/luthier-evidence/ce2-calibration-local-20261027/endpoint-feasibility-b54fc7c4/`.
Its frozen manifest SHA-256 is
`e6f73be5d574206911915968fbf27b4ea8972e3039b140c7876c76e53183136b`;
private diagnostic patch SHA-256 is
`c3beead2c66f264a1d740f2450cb881d001ecd3aab93a1c11f9acbc4c3794637`.
These local files are not implied to be on GitHub. Preserve the earlier failed
native packet writes and do not reroute them.

The original untraced 81 looper readiness failures remain unattributed; the
later traced diagnostic is separate evidence, not a retroactive pass. Prior
Phase90, DD-3T and Dyna failures and their unchanged limits remain open. Six
selected named models remain unregistered; the other twelve are deferred, not
deleted. Native builds, validators on the final source, hardware/DAW testing,
and release qualification remain incomplete. See the continuation and bank
grind handoffs for source-pinned results and retained failures.
