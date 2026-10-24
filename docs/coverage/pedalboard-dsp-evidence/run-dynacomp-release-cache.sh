#!/usr/bin/env bash
set -euo pipefail
TASK_ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
TASK_BUILD=${1:-/tmp/luthier-dynacomp-release-cache}
mkdir -p "$TASK_BUILD"
cd "$TASK_ROOT"
git show 378c8913:Source/ReferencePedals/Models/mxr_dynacomp.cpp > "$TASK_BUILD/baseline.cpp"
FLAGS=(-std=c++17 -DNDEBUG -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_USE_CURL=0 -IThirdParty/JUCE/modules -ISource/ReferencePedals/Models)
LINK=(/workspace/luthier-toolchain/objects/juce_core.o ThirdParty/JUCE/modules/juce_core/juce_core_CompilationTime.cpp -pthread -ldl -lrt)
PROBE=docs/coverage/pedalboard-dsp-evidence/measure-dynacomp-release-cache.cpp
for variant in before after; do
 MODEL="$TASK_BUILD/baseline.cpp"
 if [[ "$variant" == after ]]; then MODEL=Source/ReferencePedals/Models/mxr_dynacomp.cpp; fi
 c++ "${FLAGS[@]}" -O2 "$PROBE" "$MODEL" "${LINK[@]}" -o "$TASK_BUILD/$variant"
 "$TASK_BUILD/$variant" "$TASK_BUILD/$variant.bin" > "$TASK_BUILD/$variant.txt"
done
cmp "$TASK_BUILD/before.bin" "$TASK_BUILD/after.bin"
sha256sum "$TASK_BUILD/before.bin" "$TASK_BUILD/after.bin" > "$TASK_BUILD/comparison.txt"
c++ "${FLAGS[@]}" -O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer -DLUTHIER_ALLOCATION_COUNTER=1 \
 docs/coverage/pedalboard-dsp-evidence/named-candidate-runner.cpp \
 docs/coverage/pedalboard-dsp-evidence/named-candidate-allocation-counter.cpp \
 Source/Tests/ReferencePedals_mxr_dynacompTests.cpp Source/ReferencePedals/Models/mxr_dynacomp.cpp \
 "${LINK[@]}" -o "$TASK_BUILD/tests"
ASAN_OPTIONS=detect_leaks=1 "$TASK_BUILD/tests" > "$TASK_BUILD/tests.txt" 2> "$TASK_BUILD/stderr.txt"
cat "$TASK_BUILD/tests.txt"
