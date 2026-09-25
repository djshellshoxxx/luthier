#!/usr/bin/env bash
# Luthier Linux release: configure + build (through the shared build lock),
# package with CPack, checksum, and write a release manifest (installer.md 3,
# 4, 10). Output lands in dist/.
#
#   scripts/release.sh                  full run
#   scripts/release.sh --skip-build     package what build/ already holds
#   scripts/release.sh --no-standalone-bundle
#   scripts/release.sh --out DIR        somewhere other than dist/
#
# Produces (installer.md 3 naming):
#   luthier_<version>_amd64.deb
#   Luthier-<version>-linux-x64.tar.gz
#   Luthier-<version>-linux-x64-standalone.tar.gz     (installer.md 4)
#   SHA256SUMS.txt
#   manifest.json                                     (installer.md 10)
#   *.asc, manifest.json.asc   when a signing key is configured (see below)
#
# Signing (installer.md 0.2, 10). The key lives outside the repository. Set
#   LUTHIER_SIGNING_KEY=<gpg key id or fingerprint>
# and every package gets a detached ASCII-armoured signature and the manifest
# a clear-signed copy. Without it the run says "unsigned" and still succeeds,
# so a developer can package without the release key. A different signer
# (HSM, CI secret store) can be dropped in by setting
#   LUTHIER_SIGN_HOOK=/path/to/script     called as: script <file>  (must write <file>.asc)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
OUT="$ROOT/dist"
SKIP_BUILD=0
STANDALONE_BUNDLE=1

while [ $# -gt 0 ]; do
    case "$1" in
        --skip-build) SKIP_BUILD=1 ;;
        --no-standalone-bundle) STANDALONE_BUNDLE=0 ;;
        --out) OUT="$2"; shift ;;
        --build-dir) BUILD="$2"; shift ;;
        --help|-h) sed -n '2,27p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "release.sh: unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

# --- the version, from project(Luthier VERSION x.y.z) ----------------------------
VERSION="$(sed -n 's/^project(Luthier[[:space:]]\+VERSION[[:space:]]\+\([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)"
[ -n "$VERSION" ] || { echo "release.sh: could not read the version from CMakeLists.txt" >&2; exit 1; }

ARCH="$(uname -m)"
case "$ARCH" in x86_64) ARCH_TAG=x64; DEB_ARCH=amd64 ;; aarch64) ARCH_TAG=aarch64; DEB_ARCH=arm64 ;; *) ARCH_TAG="$ARCH"; DEB_ARCH="$ARCH" ;; esac

COMMIT="$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
COMMIT_TIME="$(git -C "$ROOT" log -1 --format=%ct 2>/dev/null || date +%s)"
# Timestamps inside the archives follow the commit, not the clock, which is as
# close to installer.md 0.5 (byte-identical rebuilds) as CPack gets.
export SOURCE_DATE_EPOCH="$COMMIT_TIME"

echo "Luthier $VERSION ($ARCH_TAG) at $COMMIT"

# --- build, serialised with every other agent's ninja run -------------------------
mkdir -p "$BUILD"
exec 9>"$BUILD/.build.lock"
flock 9

if [ "$SKIP_BUILD" -eq 0 ]; then
    if ! grep -q "^LUTHIER_BUILD_PACKAGES:BOOL=ON" "$BUILD/CMakeCache.txt" 2>/dev/null; then
        cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLUTHIER_BUILD_PACKAGES=ON
    fi
    ninja -C "$BUILD" Luthier_VST3 Luthier_Standalone
else
    [ -f "$BUILD/CPackConfig.cmake" ] || { echo "release.sh: $BUILD is not configured with LUTHIER_BUILD_PACKAGES=ON" >&2; exit 1; }
fi

# --- package -----------------------------------------------------------------------
PKG="$BUILD/packages"
mkdir -p "$PKG"
find "$PKG" -mindepth 1 -maxdepth 1 \( -name '*.deb' -o -name '*.tar.gz' -o -name '*.sha256' \) -delete

cpack --config "$BUILD/CPackConfig.cmake" -G DEB -B "$PKG"
cpack --config "$BUILD/CPackConfig.cmake" -G TGZ -B "$PKG"
if [ "$STANDALONE_BUNDLE" -eq 1 ]; then
    cpack --config "$BUILD/CPackConfig.cmake" -G TGZ -B "$PKG" -D LUTHIER_STANDALONE_ONLY=ON
fi

flock -u 9

# --- collect, checksum, manifest ------------------------------------------------------
mkdir -p "$OUT"
DEB="luthier_${VERSION}_${DEB_ARCH}.deb"
TGZ="Luthier-${VERSION}-linux-${ARCH_TAG}.tar.gz"
TGZS="Luthier-${VERSION}-linux-${ARCH_TAG}-standalone.tar.gz"

FILES=("$DEB" "$TGZ")
[ "$STANDALONE_BUNDLE" -eq 1 ] && FILES+=("$TGZS")

for f in "${FILES[@]}"; do
    [ -f "$PKG/$f" ] || { echo "release.sh: cpack did not produce $f (see $PKG)" >&2; ls -la "$PKG" >&2; exit 1; }
    cp -f "$PKG/$f" "$OUT/$f"
    # A stale signature from an earlier run must not outlive its package.
    find "$OUT" -maxdepth 1 -name "$f.asc" -delete
done

(cd "$OUT" && sha256sum "${FILES[@]}" > SHA256SUMS.txt)

kind_of() {
    case "$1" in
        *-standalone.tar.gz) echo tarball-standalone ;;
        *.tar.gz) echo tarball ;;
        *.deb) echo deb ;;
        *) echo other ;;
    esac
}

# --- signing hook ----------------------------------------------------------------------
sign() {
    local file="$1"
    if [ -n "${LUTHIER_SIGN_HOOK:-}" ]; then
        "$LUTHIER_SIGN_HOOK" "$file"
    elif [ -n "${LUTHIER_SIGNING_KEY:-}" ]; then
        gpg --batch --yes --armor --local-user "$LUTHIER_SIGNING_KEY" --detach-sign --output "$file.asc" "$file"
    else
        return 1
    fi
}

SIGNED=false
for f in "${FILES[@]}"; do
    if sign "$OUT/$f"; then SIGNED=true; fi
done

{
    echo "{"
    echo "  \"schema\": 1,"
    echo "  \"product\": \"Luthier\","
    echo "  \"version\": \"$VERSION\","
    echo "  \"platform\": \"linux\","
    echo "  \"arch\": \"$ARCH_TAG\","
    echo "  \"commit\": \"$COMMIT\","
    echo "  \"built\": \"$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y-%m-%dT%H:%M:%SZ)\","
    echo "  \"signed\": $SIGNED,"
    echo "  \"checksums\": \"SHA256SUMS.txt\","
    echo "  \"files\": ["
    n=${#FILES[@]}; i=0
    for f in "${FILES[@]}"; do
        i=$((i + 1))
        sum="$(sha256sum "$OUT/$f" | cut -d' ' -f1)"
        size="$(stat -c %s "$OUT/$f")"
        sig="null"; [ -f "$OUT/$f.asc" ] && sig="\"$f.asc\""
        comma=","; [ "$i" -eq "$n" ] && comma=""
        echo "    { \"name\": \"$f\", \"kind\": \"$(kind_of "$f")\", \"size\": $size, \"sha256\": \"$sum\", \"signature\": $sig }$comma"
    done
    echo "  ]"
    echo "}"
} > "$OUT/manifest.json"

if [ "$SIGNED" = true ]; then
    find "$OUT" -maxdepth 1 -name manifest.json.asc -delete
    if [ -n "${LUTHIER_SIGN_HOOK:-}" ]; then
        "$LUTHIER_SIGN_HOOK" "$OUT/manifest.json"
    else
        gpg --batch --yes --armor --local-user "$LUTHIER_SIGNING_KEY" --clearsign --output "$OUT/manifest.json.asc" "$OUT/manifest.json"
    fi
else
    echo "release.sh: UNSIGNED - set LUTHIER_SIGNING_KEY (or LUTHIER_SIGN_HOOK) for a publishable release (installer.md 0.2, 10)"
fi

echo
echo "Release files in $OUT:"
(cd "$OUT" && ls -l "${FILES[@]}" SHA256SUMS.txt manifest.json $( [ "$SIGNED" = true ] && echo '*.asc' ) )
