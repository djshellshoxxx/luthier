#!/usr/bin/env bash
set -euo pipefail
TASK_ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
TASK_BUILD=${1:-/tmp/luthier-named-candidates}
mkdir -p "$TASK_BUILD"
cd "$TASK_ROOT"
JUCE_CORE_OBJECT=${JUCE_CORE_OBJECT:-/workspace/luthier-toolchain/objects/juce_core.o}
TEST_SOURCES=(
ReferencePedals_ibanez_ts808Tests ReferencePedals_boss_ds1Tests ReferencePedals_boss_sd1Tests
ReferencePedals_mxr_distortion_plusTests ReferencePedals_proco_ratTests RefPedalNamedDriveContractsTests
RefPedalNamedBypassTests ReferencePedals_ehx_big_muff_nycTests ReferencePedals_ehx_muff_wickerTests
ReferencePedals_ehx_green_russianTests ReferencePedals_ehx_deluxe_muffTests RefPedalMuffCircuitTests
RefPedalMuffModelsContractsTests RefPedalPeakFilterTests RefPedalShelfSlopeTests RefPedalRandomWalkLfoTests
)
MODEL_SOURCES=(ibanez_ts808 boss_ds1 boss_sd1 mxr_distortion_plus proco_rat ehx_big_muff_nyc ehx_muff_wicker ehx_green_russian ehx_deluxe_muff)
SOURCES=(docs/coverage/pedalboard-dsp-evidence/named-candidate-runner.cpp docs/coverage/pedalboard-dsp-evidence/named-candidate-allocation-counter.cpp)
for name in "${TEST_SOURCES[@]}"; do SOURCES+=("Source/Tests/$name.cpp"); done
for name in "${MODEL_SOURCES[@]}"; do SOURCES+=("Source/ReferencePedals/Models/$name.cpp"); done
c++ -std=c++17 -O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer -DNDEBUG \
-DLUTHIER_ALLOCATION_COUNTER=1 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_USE_CURL=0 \
-IThirdParty/JUCE/modules "${SOURCES[@]}" "$JUCE_CORE_OBJECT" \
ThirdParty/JUCE/modules/juce_core/juce_core_CompilationTime.cpp -pthread -ldl -lrt -o "$TASK_BUILD/tests"
ASAN_OPTIONS=detect_leaks=1 "$TASK_BUILD/tests" > "$TASK_BUILD/results.txt" 2> "$TASK_BUILD/stderr.txt"
cat "$TASK_BUILD/results.txt"
