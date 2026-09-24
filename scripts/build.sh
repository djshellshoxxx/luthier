#!/usr/bin/env bash
# Serialised Linux build + test helper. Several agents share one build tree, and
# two concurrent ninja runs in one build directory corrupt each other, so every
# build goes through this flock. Usage:
#   scripts/build.sh                 # build LuthierTests
#   scripts/build.sh test [filters]  # build, then run the suite (optionally filtered)
#   scripts/build.sh obj <path/to/Source/File.cpp>  # compile one TU only
set -o pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
LOCK="$BUILD/.build.lock"
mkdir -p "$BUILD"
exec 9>"$LOCK"
flock 9
cmd="${1:-build}"; shift || true
case "$cmd" in
  obj)
    src="$1"; rel="${src#$ROOT/}"
    obj=$(ninja -C "$BUILD" -t targets all 2>/dev/null | grep -F "LuthierTests.dir/$rel.o" | head -1 | cut -d: -f1)
    [ -n "$obj" ] || { echo "no object for $rel"; exit 2; }
    ninja -C "$BUILD" "$obj" 2>&1 | tail -80
    exit "${PIPESTATUS[0]}"
    ;;
  build)
    ninja -C "$BUILD" LuthierTests 2>&1 | grep -E "error|FAILED|warning: unused|Linking|ninja:" | head -80
    exit "${PIPESTATUS[0]}"
    ;;
  test)
    ninja -C "$BUILD" LuthierTests 2>&1 | grep -E "error|FAILED|ninja:" | head -60 || true
    [ -x "$BUILD/LuthierTests_artefacts/Release/LuthierTests" ] || { echo "no runner"; exit 3; }
    cd "$BUILD/LuthierTests_artefacts/Release" && xvfb-run -a ./LuthierTests "$@" 9>&- 2>&1 | grep -E "^\S|FAIL\]|^\s+line [0-9]+:|tests failed|tests,|ALL PASSED|====" | grep -vE "^\s*\[pass\]" | tail -150
    exit "${PIPESTATUS[0]}"
    ;;
  *) echo "unknown: $cmd"; exit 1;;
esac
