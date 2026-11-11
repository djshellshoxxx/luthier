# Real-Time Rules

Code reachable from `processBlock` must not allocate, lock, log, do file or network I/O, loop without bound or throw. Buffers are allocated in `prepareToPlay`. DSP is `double`, with a DC blocker and NaN/Inf guard on every recursive stage, smoothed parameters, sample-rate-independent state (times in seconds or Hz), denormals disabled, and a `reset()` on every module.

Canonical text: [CONTRIBUTING.md](https://github.com/djshellshoxxx/luthier/blob/master/CONTRIBUTING.md) (summary), [spec/engine.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/engine.md) section 0, [spec/JUCE_CLAUDE_GUIDELINES.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/JUCE_CLAUDE_GUIDELINES.md) section 2.
