#!/bin/bash
# One-shot Linux setup: build dependencies, JUCE 8.0.10, and a Release Ninja build tree.
# Usage: scripts/setup_linux.sh   (from the repo root; safe to run again)
set -e
cd "$(dirname "$0")/.."
if command -v apt-get >/dev/null; then
  (apt-get install -y libasound2-dev libxrandr-dev libxcursor-dev libxinerama-dev libxcomposite-dev \
     libfontconfig1-dev libfreetype-dev libcurl4-openssl-dev libgl1-mesa-dev xvfb ninja-build clang >/dev/null 2>&1) || \
  (apt-get update >/dev/null 2>&1 && apt-get install -y libasound2-dev libxrandr-dev libxcursor-dev libxinerama-dev \
     libxcomposite-dev libfontconfig1-dev libfreetype-dev libcurl4-openssl-dev libgl1-mesa-dev xvfb ninja-build clang >/dev/null)
fi
[ -d ThirdParty/JUCE ] || git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
# CLAP builds through clap-juce-extensions (CMakeLists picks it up when present).
[ -d ThirdParty/clap-juce-extensions ] || git clone --depth 1 --recurse-submodules --shallow-submodules \
    https://github.com/free-audio/clap-juce-extensions.git ThirdParty/clap-juce-extensions
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
echo "Configured. Build tests:  ninja -C build LuthierTests"
echo "Run tests:  xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests [SuiteOrTestName ...]"
