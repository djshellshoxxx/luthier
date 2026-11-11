#!/bin/bash
# Create an isolated worktree for one worker, sharing the compiler cache with every other
# worktree of this repository.
#
# Usage: scripts/agents/worktree.sh <name> <new-branch> [base-ref]
#   <name>        directory name under $LUTHIER_WT_ROOT
#   <new-branch>  branch created for the worker
#   [base-ref]    start point (default: the branch checked out in the main checkout)
# Environment: NO_CONFIGURE=1 skips the CMake configure (documentation-only work);
#              EXTRA_CMAKE_ARGS adds configure arguments (e.g. -DLUTHIER_SANITIZE=address,undefined
#              with a different build directory is NOT supported here: use a separate script).
#
# Why this works: ccache is set up once with base_dir pointing at a common parent of all
# worktrees (see README.md), the pinned JUCE and clap-juce-extensions checkouts are symlinked
# from the main checkout, and every worktree builds in <worktree>/build. Paths relative to the
# build directory are then identical everywhere, so a fresh worktree mostly links.
set -euo pipefail
. "$(dirname "$0")/common.sh"
NAME="$1"; BRANCH="$2"
BASE="${3:-$(git -C "$MAIN_ROOT" rev-parse --abbrev-ref HEAD)}"
WT="$WT_ROOT/$NAME"
if [ ! -d "$WT" ]; then
    git -C "$MAIN_ROOT" worktree add -q -b "$BRANCH" "$WT" "$BASE"
fi
for dep in JUCE clap-juce-extensions; do
    [ -e "$MAIN_ROOT/ThirdParty/$dep" ] || { echo "worktree.sh: $MAIN_ROOT/ThirdParty/$dep is missing; run scripts/setup_linux.sh in the main checkout first" >&2; exit 1; }
    [ -e "$WT/ThirdParty/$dep" ] || ln -s "$MAIN_ROOT/ThirdParty/$dep" "$WT/ThirdParty/$dep"
done
if [ "${NO_CONFIGURE:-0}" != 1 ] && [ ! -f "$WT/build/build.ninja" ]; then
    mkdir -p "$WT/build"
    launcher=()
    command -v ccache >/dev/null && launcher=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
    # LUTHIER_BUILD_STRING is pinned so the one file that embeds it also hits the cache.
    cmake -S "$WT" -B "$WT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLUTHIER_LTO=OFF \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ "${launcher[@]}" \
        -DLUTHIER_BUILD_STRING=dev ${EXTRA_CMAKE_ARGS:-} > "$WT/build/configure.log" 2>&1 \
        || { tail -20 "$WT/build/configure.log"; exit 1; }
fi
echo "worktree: $WT  branch: $(git -C "$WT" rev-parse --abbrev-ref HEAD)  head: $(git -C "$WT" rev-parse --short HEAD)"
