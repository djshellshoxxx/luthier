#!/bin/bash
# Build the Linux BETA validation package for Luthier.
#
# Produces, under $DIST_DIR (default: dist):
#   dist/luthier-<edition>-<version>-linux-x64.tar.gz   VST3, optional CLAP, standalone + Resources + INSTALL.md
#   dist/luthier-<edition>-<version>-linux-x64.SHA256SUMS   sha256 of the archive and each artifact
#
# <version> is project(Luthier VERSION ...) from CMakeLists.txt plus the git
# short SHA, e.g. 1.0.0-30ba998.
#
# The archive is assembled from the products scripts/ci_build.sh's `stage` step
# lays out in $DIST_DIR/<platform>/: the VST3 and CLAP with one shared copy of
# the factory content, which is exactly how the installers place them. This
# script does not build; run the build first, then:
#
#   BUILD_DIR=build scripts/ci_build.sh stage   # stage from the build tree
#   scripts/package_beta_linux.sh               # wrap it into the beta package
#
# Environment (all optional):
#   LUTHIER_EDITION  PAID (default) or FREE; legacy Luthier staging also supported
#   BUILD_DIR   build tree the products came from   (default: build)
#   DIST_DIR    output directory                    (default: dist)
#   VERSION     override the CMake version           (default: from CMakeLists.txt)
#   GIT_SHA     override the git short SHA            (default: git rev-parse --short HEAD)
#   SOURCE_DATE_EPOCH  timestamps for a reproducible archive (default: last commit)
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build}"
DIST_DIR="${DIST_DIR:-dist}"
STAGE="$DIST_DIR/linux"
VERSION="${VERSION:-$(sed -nE 's/^project\(Luthier VERSION ([0-9.]+).*/\1/p' CMakeLists.txt | tr -d '\r')}"
GIT_SHA="${GIT_SHA:-$(git rev-parse --short HEAD 2>/dev/null || echo unknown)}"
PKGVER="$VERSION-$GIT_SHA"
SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct 2>/dev/null || date +%s)}"
export SOURCE_DATE_EPOCH

case "${LUTHIER_EDITION:-PAID}" in
    PAID) PRODUCT_NAME="Luthier Pro"; EDITION_SLUG=pro ;;
    FREE) PRODUCT_NAME="Luthier Free"; EDITION_SLUG=free ;;
    *) echo "LUTHIER_EDITION must be PAID or FREE" >&2; exit 1 ;;
esac
[[ "$VERSION" =~ ^[A-Za-z0-9][A-Za-z0-9._+-]*$ && "$GIT_SHA" =~ ^[A-Za-z0-9]+$ ]] || {
    echo "VERSION and GIT_SHA must be safe filename components" >&2; exit 1;
}

# Prefer the requested current edition. Old unqualified bundles remain usable,
# with their original archive names; they are not relabelled as Pro or Free.
select_staged_product() {
    if [ -d "$STAGE/$PRODUCT_NAME.vst3" ]; then return 0; fi
    if [ -d "$STAGE/Luthier.vst3" ]; then PRODUCT_NAME=Luthier; EDITION_SLUG=""; return 0; fi
    return 1
}
if ! select_staged_product; then
    if [ -d "$STAGE/Luthier Pro.vst3" ] || [ -d "$STAGE/Luthier Free.vst3" ]; then
        echo "Requested $PRODUCT_NAME is not staged; refusing the other edition" >&2; exit 1
    fi
    echo "==> Staging products from $BUILD_DIR"
    LUTHIER_EDITION="${LUTHIER_EDITION:-PAID}" BUILD_DIR="$BUILD_DIR" DIST_DIR="$DIST_DIR" scripts/ci_build.sh stage
    select_staged_product || { echo "Nothing staged in $STAGE: build first, then ci_build.sh stage" >&2; exit 1; }
fi
name="luthier${EDITION_SLUG:+-$EDITION_SLUG}-$PKGVER-linux-x64"
vst3so="$PRODUCT_NAME.vst3/Contents/x86_64-linux/$PRODUCT_NAME.so"
for required in "$vst3so" luthier; do
    [ -s "$STAGE/$required" ] && [ -f "$STAGE/$required" ] || {
        echo "Missing or empty staged binary: $required" >&2; exit 1;
    }
done
[ -x "$STAGE/luthier" ] || { echo "Staged standalone is not executable" >&2; exit 1; }
[ -d "$STAGE/Resources" ] && [ -n "$(find "$STAGE/Resources" -type f -print -quit)" ] || {
    echo "Missing or empty staged Resources" >&2; exit 1;
}
HAVE_CLAP=0
CLAP_STATUS="not included"
if [ -e "$STAGE/$PRODUCT_NAME.clap" ]; then
    [ -s "$STAGE/$PRODUCT_NAME.clap" ] && [ -f "$STAGE/$PRODUCT_NAME.clap" ] || {
        echo "Empty or invalid staged CLAP" >&2; exit 1;
    }
    HAVE_CLAP=1
    CLAP_STATUS="included"
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
root="$work/$name"
mkdir -p "$root"

# The three formats plus the one shared factory-content tree they all read.
cp -R "$STAGE/$PRODUCT_NAME.vst3" "$root/"
if [ "$HAVE_CLAP" = 1 ]; then cp "$STAGE/$PRODUCT_NAME.clap" "$root/"; fi
cp "$STAGE/luthier" "$root/luthier"
cp -R "$STAGE/Resources" "$root/Resources"
echo "$PKGVER" > "$root/VERSION"

#------------------------------------------------------------------ INSTALL.md
cat > "$root/INSTALL.md" <<EOF
# $PRODUCT_NAME $PKGVER — Linux beta (x86-64)

This archive contains VST3, the standalone application, optional CLAP and one shared \`Resources/\`
folder of factory content (guitars, cabinets, presets, tunes, fonts). Every
format reads the same \`Resources/\` tree, so keep it beside the plugins or copy
it to the shared location shown below.

## Contents

| Item | What it is |
|------|------------|
| \`${PRODUCT_NAME}.vst3\` | VST3 plugin bundle |
| \`${PRODUCT_NAME}.clap\` | CLAP plugin ($CLAP_STATUS) |
| \`luthier\` | Standalone application (ELF x86-64) |
| \`Resources/\` | Factory content shared by all formats |
| \`VERSION\` | \`$PKGVER\` |

## Quick install (per user)

Copy each format to the directory your host scans, and put the factory content
where the plugins look for it (\`~/.local/share/luthier/Resources\`):

\`\`\`bash
# VST3  ->  ~/.vst3
mkdir -p ~/.vst3 && cp -R "$PRODUCT_NAME.vst3" ~/.vst3/

# CLAP  ->  ~/.clap
if [ -f "$PRODUCT_NAME.clap" ]; then mkdir -p ~/.clap && cp "$PRODUCT_NAME.clap" ~/.clap/; fi

# Factory content (shared by every format)
mkdir -p ~/.local/share/luthier/Resources && cp -R Resources/. ~/.local/share/luthier/Resources/

# Standalone  ->  anywhere on your PATH (optional)
mkdir -p ~/.local/bin && cp luthier ~/.local/bin/luthier
\`\`\`

Then rescan plugins in your DAW.

## System-wide install

As root, use the shared paths instead:

- VST3: \`/usr/local/lib/vst3/${PRODUCT_NAME}.vst3\`
- CLAP: \`/usr/lib/clap/${PRODUCT_NAME}.clap\`
- Content: \`/usr/local/share/luthier/Resources\`
- Standalone: \`/usr/local/bin/luthier\`

## Running the standalone

\`\`\`bash
./luthier            # from this folder (keep Resources/ beside it), or
luthier              # if you copied it onto your PATH
\`\`\`

The standalone opens an audio device on launch; pick your output in its audio
settings if it comes up silent. It needs ALSA (and JACK or PipeWire for
low-latency audio) and X11 (or XWayland).

## Verify the download

The \`$name.SHA256SUMS\` sidecar ships next to the archive.
Its lines are relative to the directory that holds the archive, so verify from
there after extracting:

\`\`\`bash
cd <folder containing the archive>
tar xzf $name.tar.gz
sha256sum -c $name.SHA256SUMS
\`\`\`

This checks the archive itself and each extracted binary (standalone, CLAP,
and the VST3's shared object).

## Uninstall

Remove the copied \`${PRODUCT_NAME}.vst3\`, \`${PRODUCT_NAME}.clap\`, the \`luthier\` binary and the
\`Resources\` folder from the locations above. Your presets, guitars and tunes in
\`~/Documents/Luthier\` are never touched.
EOF

find "$root" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} + 2>/dev/null || true

#------------------------------------------------------------------ archive
mkdir -p "$DIST_DIR"
tarball="$DIST_DIR/$name.tar.gz"
tar --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 --numeric-owner \
    --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime \
    -C "$work" -cf - "$name" | gzip -9n > "$tarball"
echo "Wrote $tarball"

#------------------------------------------------------------------ SHA256SUMS
# Checksums of the archive and each shipped artifact, all relative to $DIST_DIR.
# The artifact lines carry the package-directory prefix, so after
#   cd dist && tar xzf <archive> && sha256sum -c <archive>.SHA256SUMS
# every line resolves: the archive itself, plus the extracted binaries. A VST3
# bundle is represented by its one loadable object, so each line is one artifact.
sums="$DIST_DIR/$name.SHA256SUMS"
{
    ( cd "$DIST_DIR" && sha256sum "$name.tar.gz" )
    ( cd "$work" && sha256sum "$name/luthier" )
    if [ "$HAVE_CLAP" = 1 ]; then ( cd "$work" && sha256sum "$name/$PRODUCT_NAME.clap" ); fi
    ( cd "$work" && sha256sum "$name/$vst3so" )
} > "$sums"
echo "Wrote $sums"

echo
echo "=== $name ==="
echo "Archive:  $tarball"
echo "Checksums: $sums"
cat "$sums"
