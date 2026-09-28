#!/bin/bash
# Build the Linux release packages from the products scripts/ci_build.sh staged:
#
#   dist/installers/Luthier-<version>-linux-x64.tar.gz   (+ install.sh / uninstall.sh)
#   dist/installers/luthier_<version>_amd64.deb
#   *.asc detached PGP signatures when GPG_PRIVATE_KEY is set (installer.md 0.2)
#
# Usage: scripts/package_linux.sh            (after: scripts/ci_build.sh ... stage)
# Environment:
#   VERSION          default: project(VERSION) from CMakeLists.txt
#   DIST_DIR         default: dist
#   BETA_README      completed beta README to include in both packages (optional)
#   GPG_PRIVATE_KEY  armoured secret key; GPG_PASSPHRASE its passphrase
#   SOURCE_DATE_EPOCH  timestamps for reproducible archives (default: last commit)
set -euo pipefail
cd "$(dirname "$0")/.."

DIST_DIR="${DIST_DIR:-dist}"
STAGE="$DIST_DIR/linux"
OUT="$DIST_DIR/installers"
VERSION="${VERSION:-$(sed -nE 's/^project\(Luthier VERSION ([0-9.]+).*/\1/p' CMakeLists.txt | tr -d '\r')}"
SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct 2>/dev/null || date +%s)}"
export SOURCE_DATE_EPOCH

if [ -n "${BETA_README:-}" ]; then
    [ -s "$BETA_README" ] && [ "${BETA_README##*/}" != 'README-BETA.template.md' ] || {
        echo "BETA_README must point to a nonempty completed README-BETA.md" >&2; exit 1;
    }
    if grep -Eq '\[[A-Z][A-Z _/-]*\]|\b(TODO|TBD)\b' "$BETA_README"; then
        echo "BETA_README still contains a placeholder" >&2; exit 1
    fi
fi

[ -e "$STAGE/Luthier.vst3" ] || { echo "Nothing staged in $STAGE: run scripts/ci_build.sh first" >&2; exit 1; }
mkdir -p "$OUT"

# Deterministic archives (installer.md 0.5): fixed order, owner and mtime.
deterministic_tar() { # deterministic_tar <out.tar.gz> <dir> <name>
    tar --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 --numeric-owner \
        --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime \
        -C "$2" -cf - "$3" | gzip -9n > "$1"
}

#------------------------------------------------------------------ .tar.gz
name="Luthier-$VERSION-linux-x64"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
root="$work/$name"
mkdir -p "$root/share/applications" "$root/share/mime/packages" "$root/share/icons/hicolor/256x256/apps"

cp -R "$STAGE/Luthier.vst3" "$STAGE/Resources" "$root/"
[ -e "$STAGE/Luthier.clap" ] && cp "$STAGE/Luthier.clap" "$root/"
cp "$STAGE/luthier" "$root/luthier"
[ -e "$STAGE/luthier-render" ] && cp "$STAGE/luthier-render" "$root/"
cp packaging/linux/install.sh packaging/linux/uninstall.sh "$root/"
cp packaging/linux/luthier.desktop "$root/share/applications/"
cp packaging/linux/luthier-mime.xml "$root/share/mime/packages/luthier.xml"
cp Resources/icon.png "$root/share/icons/hicolor/256x256/apps/luthier.png"
echo "$VERSION" > "$root/VERSION"
[ -z "${BETA_README:-}" ] || cp "$BETA_README" "$root/README-BETA.md"
cat > "$root/README.txt" <<EOF
Luthier $VERSION for Linux (x86-64)

Install for your user:   ./install.sh
Install system-wide:     sudo ./install.sh --system
Uninstall:               ~/.local/share/luthier/uninstall.sh
                         (or /usr/local/share/luthier/uninstall.sh for --system)

Needs: ALSA, and JACK or PipeWire for low-latency audio; X11 (or XWayland).
Your presets, guitars and tunes live in ~/Documents/Luthier and are never
removed unless you run uninstall.sh --purge.
EOF
chmod 755 "$root/install.sh" "$root/uninstall.sh" "$root/luthier"
find "$root" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +

tarball="$OUT/$name.tar.gz"
deterministic_tar "$tarball" "$work" "$name"
echo "Wrote $tarball"

#------------------------------------------------------------------ .deb
if command -v dpkg-deb >/dev/null; then
    deb_root="$work/deb"
    mkdir -p "$deb_root/DEBIAN" "$deb_root/usr/lib/vst3" "$deb_root/usr/lib/clap" \
             "$deb_root/usr/bin" "$deb_root/usr/share/luthier" \
             "$deb_root/usr/share/applications" "$deb_root/usr/share/mime/packages" \
             "$deb_root/usr/share/icons/hicolor/256x256/apps" "$deb_root/usr/share/doc/luthier"

    cp -R "$STAGE/Luthier.vst3" "$deb_root/usr/lib/vst3/"
    [ -e "$STAGE/Luthier.clap" ] && cp "$STAGE/Luthier.clap" "$deb_root/usr/lib/clap/"
    cp "$STAGE/luthier" "$deb_root/usr/bin/luthier"
    [ -e "$STAGE/luthier-render" ] && cp "$STAGE/luthier-render" "$deb_root/usr/bin/"
    cp -R "$STAGE/Resources" "$deb_root/usr/share/luthier/Resources"
    cp packaging/linux/luthier.desktop "$deb_root/usr/share/applications/"
    cp packaging/linux/luthier-mime.xml "$deb_root/usr/share/mime/packages/luthier.xml"
    cp Resources/icon.png "$deb_root/usr/share/icons/hicolor/256x256/apps/luthier.png"
    cp packaging/linux/deb/postinst packaging/linux/deb/postrm "$deb_root/DEBIAN/"
    chmod 755 "$deb_root/DEBIAN/postinst" "$deb_root/DEBIAN/postrm" "$deb_root/usr/bin/"*
    printf 'Luthier %s\nCopyright Luthier Audio. All rights reserved.\n' "$VERSION" \
        > "$deb_root/usr/share/doc/luthier/copyright"
    [ -z "${BETA_README:-}" ] || cp "$BETA_README" "$deb_root/usr/share/doc/luthier/README-BETA.md"

    # The oldest glibc the binaries accept is the newest GLIBC_ symbol version
    # they reference: builds on a new distro do not run on an old one.
    glibc="$(objdump -T "$deb_root/usr/bin/luthier" "$deb_root"/usr/lib/vst3/Luthier.vst3/Contents/x86_64-linux/*.so 2>/dev/null \
             | grep -oE 'GLIBC_[0-9]+\.[0-9]+' | sed 's/GLIBC_//' | sort -V | tail -1)"
    installed_kb="$(du -sk --apparent-size "$deb_root/usr" | cut -f1)"
    # JUCE opens X11, Xrandr, Xcursor, Xinerama and GL with dlopen, so they are
    # Recommends rather than link-time Depends; the hard link-time needs are
    # ALSA, FreeType and fontconfig (checked with ldd in docs/RELEASING.md).
    cat > "$deb_root/DEBIAN/control" <<EOF
Package: luthier
Version: $VERSION
Section: sound
Priority: optional
Architecture: amd64
Maintainer: Luthier Audio <support@luthieraudio.com>
Installed-Size: $installed_kb
Depends: libc6 (>= ${glibc:-2.31}), libstdc++6, libasound2t64 | libasound2, libfreetype6, libfontconfig1
Recommends: libx11-6, libxext6, libxrandr2, libxcursor1, libxinerama1, libgl1
Suggests: pipewire-jack | jackd2
Homepage: https://luthieraudio.com
Description: Physically modelled guitar instrument (VST3, CLAP, standalone)
 Luthier models the strings, body, pickups and player of a guitar and plays
 it from MIDI. This package installs the VST3 and CLAP plugins, the
 standalone application, the offline renderer and the factory content.
EOF
    find "$deb_root" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
    deb="$OUT/luthier_${VERSION}_amd64.deb"
    dpkg-deb --root-owner-group -Zxz --build "$deb_root" "$deb" >/dev/null
    echo "Wrote $deb"
else
    echo "dpkg-deb not found: skipping the .deb"
fi

#------------------------------------------------------------------ signatures
if [ -n "${GPG_PRIVATE_KEY:-}" ]; then
    export GNUPGHOME="$work/gnupg"
    mkdir -p "$GNUPGHOME" && chmod 700 "$GNUPGHOME"
    echo "$GPG_PRIVATE_KEY" | gpg --batch --import
    for f in "$OUT"/*.tar.gz "$OUT"/*.deb; do
        [ -e "$f" ] || continue
        gpg --batch --yes --pinentry-mode loopback --passphrase "${GPG_PASSPHRASE:-}" \
            --armor --detach-sign --output "$f.asc" "$f"
        echo "Signed $f"
    done
else
    echo "GPG_PRIVATE_KEY not set: packages are unsigned (fine for a test build)"
fi
