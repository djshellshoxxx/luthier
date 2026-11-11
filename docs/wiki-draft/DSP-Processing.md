# DSP Processing

The engine is double-precision DSP with per-string waveguides, a sympathetic coupling matrix, body convolution or modal synthesis, pickups, a guitar circuit, pedal racks, amp, cabinet, room and master. Nonlinear stages are oversampled (default 4x, 2x selectable).

- The rules every DSP module follows: [spec/engine.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/engine.md), section 0, and [[Real-Time Rules]].
- Physics and modelling approximations: [GUITAR_PHYSICS.md](https://github.com/djshellshoxxx/luthier/blob/master/docs/GUITAR_PHYSICS.md), and [Known Issues](https://github.com/djshellshoxxx/luthier/blob/master/docs/KNOWN_ISSUES.md), "Modelling approximations".
- Source: `Source/DSP/` (String, Coupling, Body, Pickup, Circuit, Effects, Amp, Master, Common), orchestrated by `Source/LuthierEngine.*`.
