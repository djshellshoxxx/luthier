#!/bin/bash
# output-normalization.md 4.4: regenerates Resources/NormalizationFactory.json, the factory
# calibration table (measured loudness of every factory preset on its own guitar and on every
# guitar type, keyed by the configuration hash of 3.3).
#
# Run it when the factory presets change, when the engine's level moves (a merge that changes
# audio), or when kCalibrationRevision is bumped. ON-27 fails when a factory preset misses the
# table, and ON-03's drift gate when an entry is more than 0.5 LU from a fresh render.
# About 20 minutes.
set -euo pipefail
cd "$(dirname "$0")/.."
ninja -C build LuthierRender
out="$(pwd)/Resources/NormalizationFactory.json"
# The binary lands in a per-config subdirectory with a multi-config generator
# (…/Release/LuthierRender) and directly in the artefacts directory with a
# single-config one (…/LuthierRender, as scripts/setup_linux.sh configures). Use
# whichever exists so a regen is not silently skipped on a single-config tree.
if [ -x build/LuthierRender_artefacts/Release/LuthierRender ]; then
  render="build/LuthierRender_artefacts/Release/LuthierRender"
elif [ -x build/LuthierRender_artefacts/LuthierRender ]; then
  render="build/LuthierRender_artefacts/LuthierRender"
else
  echo "LuthierRender binary not found under build/LuthierRender_artefacts/." >&2
  exit 1
fi
"$render" --calibrate-factory "$out"
echo "Now check: git diff --stat Resources/NormalizationFactory.json"
