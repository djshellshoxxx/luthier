#!/bin/bash
# One-shot Linux setup: build dependencies, JUCE 8.0.10, and a Release Ninja build tree.
# Usage: scripts/setup_linux.sh   (from the repo root; safe to run again)
set -e
cd "$(dirname "$0")/.."
if command -v apt-get >/dev/null; then
  (apt-get install -y libasound2-dev libxrandr-dev libxcursor-dev libxinerama-dev libxcomposite-dev \
     libfontconfig1-dev libfreetype-dev libcurl4-openssl-dev libgl1-mesa-dev xvfb ninja-build clang ccache >/dev/null 2>&1) || \
  (apt-get update >/dev/null 2>&1 && apt-get install -y libasound2-dev libxrandr-dev libxcursor-dev libxinerama-dev \
     libxcomposite-dev libfontconfig1-dev libfreetype-dev libcurl4-openssl-dev libgl1-mesa-dev xvfb ninja-build clang ccache >/dev/null)
fi
[ -d ThirdParty/JUCE ] || git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
# CLAP support (CMakeLists.txt adds Luthier_CLAP when this is present). Pinned to
# the commit CI uses: CLAP_JUCE_EXT_COMMIT in scripts/ci_build.sh.
if [ ! -d ThirdParty/clap-juce-extensions ]; then
  git clone https://github.com/free-audio/clap-juce-extensions.git ThirdParty/clap-juce-extensions
  git -C ThirdParty/clap-juce-extensions checkout -q "$(sed -n 's/^CLAP_JUCE_EXT_COMMIT="\(.*\)"/\1/p' scripts/ci_build.sh)"
  git -C ThirdParty/clap-juce-extensions submodule update --init --recursive --depth 1
fi
LAUNCHER=()
command -v ccache >/dev/null && LAUNCHER=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "${LAUNCHER[@]}"
echo "Configured. Build tests:  ninja -C build LuthierTests"
echo "Build plugins:  ninja -C build Luthier_VST3 Luthier_CLAP Luthier_Standalone"
echo "Full CI pipeline locally:  scripts/ci_build.sh   (see its header for the steps)"
echo "Run tests:  xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests [SuiteOrTestName ...]"
