#!/bin/bash
# qa-polish.md 2.9: validates the VST3 with pluginval at strictness 10.
#
#   scripts/pluginval.sh [path/to/Luthier.vst3]
#
# Downloads pluginval (Linux build) into build/tools/ if it is not already on
# PATH or there, runs it headless under xvfb, and fails on any failure or any
# warning line in the log. The log is kept at build/pluginval.log.
set -euo pipefail
cd "$(dirname "$0")/.."

PLUGIN="${1:-build/Luthier_artefacts/Release/VST3/Luthier.vst3}"
VERSION="${PLUGINVAL_VERSION:-v1.0.3}"
TOOLS="build/tools"
LOG="build/pluginval.log"

[ -d "$PLUGIN" ] || { echo "no plugin at $PLUGIN (build Luthier_VST3 first)" >&2; exit 2; }

PLUGINVAL="$(command -v pluginval || true)"

if [ -z "$PLUGINVAL" ]; then
    PLUGINVAL="$TOOLS/pluginval"

    if [ ! -x "$PLUGINVAL" ]; then
        mkdir -p "$TOOLS"
        curl -fsSL -o "$TOOLS/pluginval.zip" \
            "https://github.com/Tracktion/pluginval/releases/download/$VERSION/pluginval_Linux.zip"
        (cd "$TOOLS" && unzip -o -q pluginval.zip && chmod +x pluginval)
    fi
fi

RUN=("$PLUGINVAL" --strictness-level 10 --validate-in-process --output-dir build --validate "$PLUGIN")

if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null; then
    RUN=(xvfb-run -a "${RUN[@]}")
fi

set +e
"${RUN[@]}" 2>&1 | tee "$LOG"
status=${PIPESTATUS[0]}
set -e

if [ "$status" -ne 0 ]; then
    echo "pluginval FAILED (exit $status); see $LOG" >&2
    exit "$status"
fi

# pluginval exits 0 with warnings; the release gate does not.
if grep -Eiq '^\s*(!!!|\*\*\*)?\s*warning' "$LOG"; then
    echo "pluginval reported warnings; see $LOG" >&2
    grep -Ei 'warning' "$LOG" >&2
    exit 1
fi

echo "pluginval: strictness 10, no failures, no warnings."
