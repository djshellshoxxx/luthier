# Parameter Management

Host parameters are APPEND-ONLY and position-indexed: host automation is stored against the parameter's position. Never reorder, remove or rename a parameter. Append inside the `// ==== BEGIN <name> params ====` markers at the end of the layout in `Source/Parameters.cpp`, mirror the id in `Source/Parameters.h` and the bridge, update the expected total in `Source/Tests/IntegrationTests.cpp`, and give every automatable parameter a visible control (test `everyAutomatableParameterHasAVisibleControl`).

The pedalboard's slot parameters (`ref_s{nn}_*`) and lane order are append-only too: [BOARD_CORE.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/reference-pedalboard/specs/BOARD_CORE.md). Rules and steps: [CONTRIBUTING.md](https://github.com/djshellshoxxx/luthier/blob/master/CONTRIBUTING.md). Wiring pattern: [spec/ui-wiring.md](https://github.com/djshellshoxxx/luthier/blob/master/spec/ui-wiring.md).
