#!/bin/bash
# Build targets in one checkout or worktree, one compile at a time for the whole machine.
#
# Usage: scripts/agents/build.sh <checkout-dir> <ninja-target> [more targets]
#   Targets: LuthierTests, LuthierRender, Luthier_VST3, Luthier_CLAP, Luthier_Standalone.
# Waits up to 9 minutes for the machine lock, then builds with $LUTHIER_JOBS jobs.
# Exit: 75 = lock busy, run the same command again; otherwise ninja's exit code.
#
# Several workers compiling at once oversubscribe a small machine and can get the compiler
# killed for lack of memory, so builds take the lock exclusively and test runs (test.sh)
# take it shared: tests never run while something compiles.
set -uo pipefail
. "$(dirname "$0")/common.sh"
WT="$1"; shift
[ -f "$WT/build/build.ninja" ] || { echo "build.sh: $WT/build is not configured" >&2; exit 2; }
LOG="$WT/build/last-build.log"
exec 9>"$LOCK_FILE"
if ! flock -x -w 540 9; then
    echo "build.sh: BUSY - another build or test run holds the machine. Run the same command again." >&2
    exit 75
fi
ninja -C "$WT/build" -j"$JOBS" "$@" > "$LOG" 2>&1
rc=$?
if [ $rc -ne 0 ]; then
    echo "build.sh: FAILED (rc=$rc). First errors:"
    grep -n -m 12 -E "error:|FAILED:|undefined reference" "$LOG" || tail -30 "$LOG"
else
    echo "build.sh: OK ($(grep -c '^\[' "$LOG") steps) - full log $LOG"
fi
exit $rc
