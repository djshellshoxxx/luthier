#!/bin/bash
# Build the Linux BETA validation package for Luthier.
#
# Produces, under $DIST_DIR (default: dist):
#   dist/luthier-<version>-linux-x64.tar.gz   the three plugin formats + Resources + INSTALL.md
#   dist/luthier-<version>-linux-x64.SHA256SUMS   sha256 of the archive and each artifact
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

name="luthier-$PKGVER-linux-x64"

# Stage the products from the build tree (reuses ci_build.sh's tested layout:
# the three formats with a single shared Resources/ tree beside them).
if [ ! -e "$STAGE/Luthier.vst3" ]; then
    echo "==> Staging products from $BUILD_DIR"
    BUILD_DIR="$BUILD_DIR" DIST_DIR="$DIST_DIR" scripts/ci_build.sh stage
fi
[ -e "$STAGE/Luthier.vst3" ] || { echo "Nothing staged in $STAGE: build first, then ci_build.sh stage" >&2; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
root="$work/$name"
mkdir -p "$root"

# The three formats plus the one shared factory-content tree they all read.
cp -R "$STAGE/Luthier.vst3" "$root/"
[ -e "$STAGE/Luthier.clap" ] && cp "$STAGE/Luthier.clap" "$root/"
cp "$STAGE/luthier" "$root/luthier"
cp -R "$STAGE/Resources" "$root/Resources"
echo "$PKGVER" > "$root/VERSION"

#------------------------------------------------------------------ INSTALL.md
cat > "$root/INSTALL.md" <<EOF
# Luthier $PKGVER — Linux beta (x86-64)

This archive contains the three plugin formats and one shared \`Resources/\`
folder of factory content (guitars, cabinets, presets, tunes, fonts). Every
format reads the same \`Resources/\` tree, so keep it beside the plugins or copy
it to the shared location shown below.

## Contents

| Item | What it is |
|------|------------|
| \`Luthier.vst3\` | VST3 plugin bundle |
| \`Luthier.clap\` | CLAP plugin |
| \`luthier\` | Standalone application (ELF x86-64) |
| \`Resources/\` | Factory content shared by all formats |
| \`VERSION\` | \`$PKGVER\` |

## Quick install (per user)

Copy each format to the directory your host scans, and put the factory content
where the plugins look for it (\`~/.local/share/luthier/Resources\`):

\`\`\`bash
# VST3  ->  ~/.vst3
mkdir -p ~/.vst3 && cp -R Luthier.vst3 ~/.vst3/

# CLAP  ->  ~/.clap
mkdir -p ~/.clap && cp Luthier.clap ~/.clap/

# Factory content (shared by every format)
mkdir -p ~/.local/share/luthier && cp -R Resources ~/.local/share/luthier/Resources

# Standalone  ->  anywhere on your PATH (optional)
mkdir -p ~/.local/bin && cp luthier ~/.local/bin/luthier
\`\`\`

Then rescan plugins in your DAW.

## System-wide install

As root, use the shared paths instead:

- VST3: \`/usr/local/lib/vst3/Luthier.vst3\`
- CLAP: \`/usr/lib/clap/Luthier.clap\`
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

The \`luthier-$PKGVER-linux-x64.SHA256SUMS\` sidecar ships next to the archive.
Its lines are relative to the directory that holds the archive, so verify from
there after extracting:

\`\`\`bash
cd <folder containing the archive>
tar xzf luthier-$PKGVER-linux-x64.tar.gz
sha256sum -c luthier-$PKGVER-linux-x64.SHA256SUMS
\`\`\`

This checks the archive itself and each extracted binary (standalone, CLAP,
and the VST3's shared object).

## Uninstall

Remove the copied \`Luthier.vst3\`, \`Luthier.clap\`, the \`luthier\` binary and the
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
vst3so="$(cd "$root" && find Luthier.vst3 -name '*.so' | head -1)"
{
    ( cd "$DIST_DIR" && sha256sum "$name.tar.gz" )
    ( cd "$work" && sha256sum "$name/luthier" )
    ( cd "$work" && [ -e "$name/Luthier.clap" ] && sha256sum "$name/Luthier.clap" )
    ( cd "$work" && [ -n "$vst3so" ] && sha256sum "$name/$vst3so" )
} > "$sums"
echo "Wrote $sums"

echo
echo "=== $name ==="
echo "Archive:  $tarball"
echo "Checksums: $sums"
cat "$sums"
