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
( cd build/LuthierRender_artefacts/Release && ./LuthierRender --calibrate-factory "$out" )
echo "Now check: git diff --stat Resources/NormalizationFactory.json"
