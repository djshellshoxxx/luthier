#!/bin/bash
# installer.md 10 / 13: checksums and a signed manifest for a release.
#
#   scripts/release_manifest.sh [artefact ...]
#
# With no arguments it takes the packages cpack wrote to build/ (*.deb,
# *.tar.gz, *.rpm, *.zip, *.pkg, *.exe, *.dmg). Writes build/manifest.txt:
#   Luthier <version> release manifest
#   <sha256>  <size>  <file name>
# and build/SHA256SUMS (sha256sum -c format). When a GPG secret key is
# available (LUTHIER_GPG_KEY names it, or gpg's default key), the manifest is
# detach-signed as build/manifest.txt.asc; without one it says so and exits 0,
# because a developer build has no release key.
set -euo pipefail
cd "$(dirname "$0")/.."

OUT="build"
mkdir -p "$OUT"

if [ $# -gt 0 ]; then
    FILES=("$@")
else
    shopt -s nullglob
    FILES=("$OUT"/*.deb "$OUT"/*.tar.gz "$OUT"/*.rpm "$OUT"/*.zip "$OUT"/*.pkg "$OUT"/*.exe "$OUT"/*.dmg)
fi

[ ${#FILES[@]} -gt 0 ] || { echo "nothing to list: run cpack first" >&2; exit 2; }

VERSION="$(sed -n 's/^project(Luthier VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"
MANIFEST="$OUT/manifest.txt"
SUMS="$OUT/SHA256SUMS"

{
    echo "Luthier ${VERSION} release manifest"
    echo "# sha256  bytes  file"
} > "$MANIFEST"
: > "$SUMS"

for f in "${FILES[@]}"; do
    [ -f "$f" ] || { echo "missing: $f" >&2; exit 2; }
    sum="$(sha256sum "$f" | awk '{print $1}')"
    size="$(stat -c %s "$f" 2>/dev/null || stat -f %z "$f")"
    name="$(basename "$f")"
    echo "$sum  $size  $name" >> "$MANIFEST"
    echo "$sum  $name" >> "$SUMS"
done

echo "wrote $MANIFEST and $SUMS (${#FILES[@]} artefacts)"

if command -v gpg >/dev/null 2>&1 && gpg --list-secret-keys ${LUTHIER_GPG_KEY:+"$LUTHIER_GPG_KEY"} >/dev/null 2>&1 \
   && [ -n "$(gpg --list-secret-keys ${LUTHIER_GPG_KEY:+"$LUTHIER_GPG_KEY"} 2>/dev/null)" ]; then
    gpg --batch --yes --armor ${LUTHIER_GPG_KEY:+--local-user "$LUTHIER_GPG_KEY"} \
        --output "$MANIFEST.asc" --detach-sign "$MANIFEST"
    echo "signed: $MANIFEST.asc"
else
    echo "no GPG secret key: the manifest is unsigned (release builds set LUTHIER_GPG_KEY)"
fi
