#!/usr/bin/env bash
set -euo pipefail
TASK_ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
TASK_BUILD=${1:-/tmp/luthier-prepared-diode}
mkdir -p "$TASK_BUILD"
cd "$TASK_ROOT"
JUCE_CORE_OBJECT=${JUCE_CORE_OBJECT:-/workspace/luthier-toolchain/objects/juce_core.o}
c++ -std=c++17 -O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer -DNDEBUG \
  -DLUTHIER_ALLOCATION_COUNTER=1 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_USE_CURL=0 \
  -IThirdParty/JUCE/modules docs/coverage/pedalboard-dsp-evidence/named-candidate-runner.cpp \
  docs/coverage/pedalboard-dsp-evidence/named-candidate-allocation-counter.cpp \
  Source/Tests/RefPedalPreparedDiodeRootTests.cpp "$JUCE_CORE_OBJECT" \
  ThirdParty/JUCE/modules/juce_core/juce_core_CompilationTime.cpp -pthread -ldl -lrt -o "$TASK_BUILD/tests"
ASAN_OPTIONS=detect_leaks=1 "$TASK_BUILD/tests"
c++ -std=c++17 -O2 -DNDEBUG docs/coverage/pedalboard-dsp-evidence/measure-prepared-diode.cpp \
  -o "$TASK_BUILD/measure"
"$TASK_BUILD/measure"
