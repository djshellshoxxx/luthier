#!/usr/bin/env bash
set -euo pipefail
TASK_ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
TASK_BUILD=${1:-/tmp/luthier-dsp-cpu-units}
mkdir -p "$TASK_BUILD"
cd "$TASK_ROOT"
SOURCES=(docs/coverage/pedalboard-dsp-evidence/named-candidate-runner.cpp Source/Tests/RefPedalDspCpuUnitsTests.cpp)
for name in Boost Overdrive Fuzz Compressor; do SOURCES+=("Source/ReferencePedals/Generic/$name.cpp"); done
for name in ibanez_ts808 boss_ds1 boss_sd1 mxr_distortion_plus proco_rat mxr_dynacomp ehx_big_muff_nyc ehx_muff_wicker ehx_green_russian ehx_deluxe_muff; do SOURCES+=("Source/ReferencePedals/Models/$name.cpp"); done
c++ -std=c++17 -O0 -DNDEBUG -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_USE_CURL=0 \
-IThirdParty/JUCE/modules "${SOURCES[@]}" /workspace/luthier-toolchain/objects/juce_core.o \
ThirdParty/JUCE/modules/juce_core/juce_core_CompilationTime.cpp -pthread -ldl -lrt -o "$TASK_BUILD/tests"
"$TASK_BUILD/tests" > "$TASK_BUILD/results.txt" 2> "$TASK_BUILD/stderr.txt"
cat "$TASK_BUILD/results.txt"
