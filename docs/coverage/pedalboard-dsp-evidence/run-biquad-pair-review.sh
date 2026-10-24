#!/usr/bin/env bash
set -euo pipefail
TASK_ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
TASK_BUILD=${1:-/tmp/luthier-biquad-pair-review}
mkdir -p "$TASK_BUILD"
cd "$TASK_ROOT"
for VARIANT in sanitizer fused-fallback; do
 FLAGS=(-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer)
 if [[ "$VARIANT" == fused-fallback ]]; then FLAGS=(-O3 -mavx2 -mfma); fi
 c++ -std=c++17 "${FLAGS[@]}" -DNDEBUG -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_USE_CURL=0 \
 -IThirdParty/JUCE/modules docs/coverage/pedalboard-dsp-evidence/named-candidate-runner.cpp \
 Source/Tests/RefPedalBiquadPairTests.cpp /workspace/luthier-toolchain/objects/juce_core.o \
 ThirdParty/JUCE/modules/juce_core/juce_core_CompilationTime.cpp -pthread -ldl -lrt -o "$TASK_BUILD/$VARIANT"
 ASAN_OPTIONS=detect_leaks=1 "$TASK_BUILD/$VARIANT" > "$TASK_BUILD/$VARIANT.txt" 2> "$TASK_BUILD/$VARIANT-stderr.txt"
done
