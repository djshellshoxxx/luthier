#!/bin/bash
# Run LuthierTests from one checkout or worktree under xvfb.
#
# Usage: scripts/agents/test.sh <checkout-dir> [LuthierTests arguments]
#   Arguments are passed through: name fragments (case-insensitive substring of a suite or
#   test name), --list, --skip=<ExactSuite>. Environment variables pass through too
#   (LUTHIER_SOAK, LUTHIER_PERF, LUTHIER_SCREENSHOTS, LUTHIER_STRESS_JSON, ...).
# Exit: 75 = lock busy, run again; otherwise the test binary's exit code
#       (0 pass, 1 failures, above 128 = killed by a signal, i.e. a crash).
#
# Two things this wrapper does that a bare invocation does not:
#  - It runs from the build directory. With the shared compiler cache __FILE__ is relative to
#    the build directory, and several tests find fixtures and sources through __FILE__.
#  - It gives the run its own HOME, so parallel workers never share user folders.
set -uo pipefail
. "$(dirname "$0")/common.sh"
WT="$1"; shift
BIN="$WT/build/LuthierTests_artefacts/Release/LuthierTests"
[ -x "$BIN" ] || { echo "test.sh: test binary missing: $BIN (build target LuthierTests first)" >&2; exit 2; }
exec 9>"$LOCK_FILE"
if ! flock -s -w 540 9; then
    echo "test.sh: BUSY - a build holds the machine. Run the same command again." >&2
    exit 75
fi
LOG="$WT/build/last-test.log"
TH="$(mktemp -d "$WT/build/test-home.XXXXXX")"
( cd "$WT/build" && HOME="$TH" XDG_CONFIG_HOME="$TH/.config" XDG_DATA_HOME="$TH/.local/share" \
    XDG_CACHE_HOME="$TH/.cache" xvfb-run -a "$BIN" "$@" ) > "$LOG" 2>&1
rc=$?
rm -rf "$TH"
grep -E "\[FAIL\]" -A6 "$LOG" | head -80
tail -6 "$LOG"
echo "test.sh: rc=$rc - full log $LOG"
exit $rc
