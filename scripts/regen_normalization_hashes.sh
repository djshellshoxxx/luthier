#!/bin/bash
# output-normalization.md ON-02: regenerates Source/Tests/Golden/NormalizationOffHashes.json,
# the SHA-256 of the main output with normalization OFF for every factory preset x guitar type
# x {NormalizationPhrase, combo chord}.
#
# Run it after a merge that legitimately changes audio (a new engine feature, a retuned preset),
# from the commit whose audio is the new reference, and commit the JSON with that change.
#
#   scripts/regen_normalization_hashes.sh            default grid (~2 min): every preset on its
#                                                    own guitar + preset 0 on every guitar type
#   LUTHIER_SLOW_TESTS=1 scripts/regen_normalization_hashes.sh
#                                                    the full grid (~35 min)
#
# The hashes only hold for the toolchain that wrote them (the file records it); the test skips
# on any other. The same-build A/B half of ON-02 (NormalizationTests) holds everywhere.
set -euo pipefail
cd "$(dirname "$0")/.."
ninja -C build LuthierTests
LUTHIER_NORMALIZATION_GOLDEN_WRITE=1 xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests NormalizationGolden
echo "Now check: git diff --stat Source/Tests/Golden/NormalizationOffHashes.json"
