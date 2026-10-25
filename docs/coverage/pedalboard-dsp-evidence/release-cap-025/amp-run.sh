#!/bin/bash
set -eu
base=$(cd "$(dirname "$0")" && pwd)
g++ -std=c++17 -O2 -DNDEBUG -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_USE_CURL=0 -I/workspace/luthier/ThirdParty/JUCE/modules "$base/probe.cpp" "$base/Source/DSP/Amp/AmpEngine.cpp" "$base/Source/DSP/Amp/ToneStack.cpp" /workspace/luthier-toolchain/objects/juce_core.o /workspace/luthier/ThirdParty/JUCE/modules/juce_core/juce_core_CompilationTime.cpp -lpthread -ldl -lrt -o "$base/probe"
"$base/probe"
