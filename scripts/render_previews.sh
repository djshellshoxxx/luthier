#!/bin/bash
# preset-browser-previews.md 2 ("Build time"): renders every factory preset's
# preview through the plugin's own PreviewRenderer and leaves them where
# luthier_copy_resources ships them from:
#
#   build/generated/Presets/Previews/<uid>.ogg
#   build/generated/Presets/Previews/previews.json        (the manifest, 5.3)
#   build/generated/Presets/descriptor-calibration.json   (6.3)
#
# Usage: scripts/render_previews.sh [build-dir] [--update-calibration]
#   --update-calibration also copies the calibration into Resources/Presets/,
#   the cold-start default committed with the source.
#
# CI runs this after building LuthierRender; editions.md 9's render_demos.sh
# calls it. Rebuild the plugin targets afterwards so the files are copied.
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="build"
UPDATE_CALIBRATION=0

for arg in "$@"; do
  case "$arg" in
    --update-calibration) UPDATE_CALIBRATION=1 ;;
    *) BUILD_DIR="$arg" ;;
  esac
done

if command -v ninja >/dev/null 2>&1 && [ -f "$BUILD_DIR/build.ninja" ]; then
  ninja -C "$BUILD_DIR" LuthierRender
else
  cmake --build "$BUILD_DIR" --target LuthierRender --config Release
fi

RENDER=$(find "$BUILD_DIR" -type f \( -name LuthierRender -o -name luthier-render -o -name 'luthier-render.exe' -o -name 'LuthierRender.exe' \) -perm -u+x 2>/dev/null | head -1)

if [ -z "$RENDER" ]; then
  echo "render_previews: could not find the LuthierRender binary under $BUILD_DIR" >&2
  exit 1
fi

OUT="$BUILD_DIR/generated/Presets"
rm -rf "$OUT/Previews"
mkdir -p "$OUT"

"$RENDER" --render-previews "$OUT"

if [ "$UPDATE_CALIBRATION" = "1" ]; then
  cp "$OUT/descriptor-calibration.json" Resources/Presets/descriptor-calibration.json
  echo "Updated Resources/Presets/descriptor-calibration.json"
fi

echo "Previews: $(ls "$OUT/Previews"/*.ogg | wc -l) clips, $(du -sh "$OUT/Previews" | cut -f1)"
