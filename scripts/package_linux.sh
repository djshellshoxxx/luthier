#!/bin/bash
# Build the Linux release packages from the products scripts/ci_build.sh staged.
# One edition per run (LUTHIER_EDITION, the same variable ci_build.sh reads):
# <Edition> is Pro or Free, <pkg> is luthier-pro or luthier-free, and the
# plug-in bundles keep the edition's product name ("Luthier Pro.vst3").
#
#   dist/installers/Luthier-<Edition>-<version>-linux-x64.tar.gz   (+ install.sh / uninstall.sh)
#   dist/installers/Luthier-<Edition>-<version>-standalone-linux-x64.tar.gz  (app + content only, installer.md 4)
#   dist/installers/<pkg>_<version>_amd64.deb
#   dist/installers/<pkg>-<version>-1.x86_64.rpm  (best effort, when rpmbuild exists; installer.md 3)
#   *.asc detached PGP signatures when GPG_PRIVATE_KEY is set (installer.md 0.2)
#
# Usage: LUTHIER_EDITION=PAID|FREE scripts/package_linux.sh   (after: scripts/ci_build.sh ... stage)
# Environment:
#   LUTHIER_EDITION  PAID (default) or FREE: which staged edition to package
#   VERSION          default: project(VERSION) from CMakeLists.txt
#   DIST_DIR         default: dist
#   BETA_README      completed beta README to include in both packages (optional)
#   GPG_PRIVATE_KEY  armoured secret key; GPG_PASSPHRASE its passphrase
#   SOURCE_DATE_EPOCH  timestamps for reproducible archives (default: last commit)
set -euo pipefail
cd "$(dirname "$0")/.."

# The edition decides which names scripts/ci_build.sh stage wrote: keep this
# case in step with the one there (and with cmake/Editions.cmake). The two
# editions are separate products that cannot share a package name, so their
# deb/rpm packages are named luthier-pro / luthier-free and conflict with each
# other (they install the same /usr/bin/luthier, desktop entry and content).
LUTHIER_EDITION="${LUTHIER_EDITION:-PAID}"
case "$LUTHIER_EDITION" in
    PAID) PRODUCT_NAME="Luthier Pro";  EDITION_SLUG=Pro;  PKG_NAME=luthier-pro;  OTHER_PKG=luthier-free ;;
    FREE) PRODUCT_NAME="Luthier Free"; EDITION_SLUG=Free; PKG_NAME=luthier-free; OTHER_PKG=luthier-pro ;;
    *) echo "package_linux.sh: LUTHIER_EDITION must be PAID or FREE" >&2; exit 1 ;;
esac

DIST_DIR="${DIST_DIR:-dist}"
STAGE="$DIST_DIR/linux"
OUT="$DIST_DIR/installers"
VERSION="${VERSION:-$(sed -nE 's/^project\(Luthier VERSION ([0-9.]+).*/\1/p' CMakeLists.txt | tr -d '\r')}"
SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct 2>/dev/null || date +%s)}"
export SOURCE_DATE_EPOCH

if [ -n "${BETA_README:-}" ]; then
    [ -s "$BETA_README" ] && [[ "$BETA_README" != *.template.md ]] || {
        echo "BETA_README must point to a nonempty completed README-BETA.md" >&2; exit 1;
    }
    # Keep the acceptance gate in line with the portable Windows package.
    # A Markdown link is valid, but an unlinked bracketed field is not.
    if python3 - "$BETA_README" <<'PY'
import pathlib
import re
import sys

text = pathlib.Path(sys.argv[1]).read_text(encoding='utf-8')
sys.exit(not bool(re.search(
    r'\[[^\]\r\n]+\](?!\()|\{\{[^}]+\}\}|\b(?:TODO|TBD|REPLACE ME)\b|'
    r'Release owner: replace every bracketed field', text, re.IGNORECASE)))
PY
    then
        echo "BETA_README still contains a placeholder" >&2; exit 1
    fi
fi

[ -e "$STAGE/$PRODUCT_NAME.vst3" ] || {
    echo "Nothing staged in $STAGE for $PRODUCT_NAME: run LUTHIER_EDITION=$LUTHIER_EDITION scripts/ci_build.sh stage first" >&2
    exit 1
}
mkdir -p "$OUT"

# Deterministic archives (installer.md 0.5): fixed order, owner and mtime.
deterministic_tar() { # deterministic_tar <out.tar.gz> <dir> <name>
    tar --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 --numeric-owner \
        --pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime \
        -C "$2" -cf - "$3" | gzip -9n > "$1"
}

#------------------------------------------------------------------ .tar.gz
name="Luthier-$EDITION_SLUG-$VERSION-linux-x64"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
root="$work/$name"
mkdir -p "$root/share/applications" "$root/share/mime/packages" "$root/share/icons/hicolor/256x256/apps"

cp -R "$STAGE/$PRODUCT_NAME.vst3" "$STAGE/Resources" "$root/"
[ -e "$STAGE/$PRODUCT_NAME.clap" ] && cp "$STAGE/$PRODUCT_NAME.clap" "$root/"
# ci_build.sh stage writes the standalone as lowercase "luthier" on Linux.
cp "$STAGE/luthier" "$root/luthier"
[ -e "$STAGE/luthier-render" ] && cp "$STAGE/luthier-render" "$root/"
cp packaging/linux/install.sh packaging/linux/uninstall.sh "$root/"
cp packaging/linux/luthier.desktop "$root/share/applications/"
cp packaging/linux/luthier.xml "$root/share/mime/packages/luthier.xml"
cp Resources/icon.png "$root/share/icons/hicolor/256x256/apps/luthier.png"
echo "$VERSION" > "$root/VERSION"
[ -z "${BETA_README:-}" ] || cp "$BETA_README" "$root/README-BETA.md"
cat > "$root/README.txt" <<EOF
$PRODUCT_NAME $VERSION for Linux (x86-64)

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

#------------------------------------------------------------------ standalone-only .tar.gz
# installer.md 4 (IN-28): the app and its content without the plug-ins. Same
# installer; install.defaults tells install.sh to leave the plug-ins out.
sname="Luthier-$EDITION_SLUG-$VERSION-standalone-linux-x64"
sroot="$work/$sname"
mkdir -p "$sroot"
cp -R "$root/Resources" "$root/share" "$root/luthier" "$root/install.sh" "$root/uninstall.sh" "$root/VERSION" "$sroot/"
[ -e "$root/luthier-render" ] && cp "$root/luthier-render" "$sroot/"
[ -e "$root/README-BETA.md" ] && cp "$root/README-BETA.md" "$sroot/"
echo "--no-vst3 --no-clap" > "$sroot/install.defaults"
sed -e "s/^$PRODUCT_NAME $VERSION for Linux (x86-64)/$PRODUCT_NAME $VERSION standalone application for Linux (x86-64)/" \
    -e '/^Needs:/i The VST3 and CLAP plug-ins are not included in this package.\n' "$root/README.txt" > "$sroot/README.txt"
find "$sroot" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
deterministic_tar "$OUT/$sname.tar.gz" "$work" "$sname"
echo "Wrote $OUT/$sname.tar.gz"

#------------------------------------------------------------------ .deb
if command -v dpkg-deb >/dev/null; then
    deb_root="$work/deb"
    mkdir -p "$deb_root/DEBIAN" "$deb_root/usr/lib/vst3" "$deb_root/usr/lib/clap" \
             "$deb_root/usr/bin" "$deb_root/usr/share/luthier" \
             "$deb_root/usr/share/applications" "$deb_root/usr/share/mime/packages" \
             "$deb_root/usr/share/icons/hicolor/256x256/apps" "$deb_root/usr/share/doc/$PKG_NAME"

    cp -R "$STAGE/$PRODUCT_NAME.vst3" "$deb_root/usr/lib/vst3/"
    [ -e "$STAGE/$PRODUCT_NAME.clap" ] && cp "$STAGE/$PRODUCT_NAME.clap" "$deb_root/usr/lib/clap/"
    cp "$STAGE/luthier" "$deb_root/usr/bin/luthier"
    [ -e "$STAGE/luthier-render" ] && cp "$STAGE/luthier-render" "$deb_root/usr/bin/"
    cp -R "$STAGE/Resources" "$deb_root/usr/share/luthier/Resources"
    cp packaging/linux/luthier.desktop "$deb_root/usr/share/applications/"
    cp packaging/linux/luthier.xml "$deb_root/usr/share/mime/packages/luthier.xml"
    cp Resources/icon.png "$deb_root/usr/share/icons/hicolor/256x256/apps/luthier.png"
    cp packaging/linux/deb/postinst packaging/linux/deb/postrm "$deb_root/DEBIAN/"
    chmod 755 "$deb_root/DEBIAN/postinst" "$deb_root/DEBIAN/postrm" "$deb_root/usr/bin/"*
    printf '%s %s\nCopyright Luthier Audio. All rights reserved.\n' "$PRODUCT_NAME" "$VERSION" \
        > "$deb_root/usr/share/doc/$PKG_NAME/copyright"
    [ -z "${BETA_README:-}" ] || cp "$BETA_README" "$deb_root/usr/share/doc/$PKG_NAME/README-BETA.md"

    # The oldest glibc the binaries accept is the newest GLIBC_ symbol version
    # they reference: builds on a new distro do not run on an old one.
    glibc="$(objdump -T "$deb_root/usr/bin/luthier" "$deb_root/usr/lib/vst3/$PRODUCT_NAME.vst3"/Contents/x86_64-linux/*.so 2>/dev/null \
             | { grep -oE 'GLIBC_[0-9]+\.[0-9]+' || true; } | sed 's/GLIBC_//' | sort -V | tail -1)" || glibc=""
    installed_kb="$(du -sk --apparent-size "$deb_root/usr" | cut -f1)"
    # JUCE opens X11, Xrandr, Xcursor, Xinerama and GL with dlopen, so they are
    # Recommends rather than link-time Depends; the hard link-time needs are
    # ALSA, FreeType and fontconfig (checked with ldd in docs/RELEASING.md).
    cat > "$deb_root/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $VERSION
Section: sound
Priority: optional
Architecture: amd64
Maintainer: Luthier Audio <support@luthieraudio.com>
Installed-Size: $installed_kb
Depends: libc6 (>= ${glibc:-2.31}), libstdc++6, libasound2t64 | libasound2, libfreetype6, libfontconfig1
Recommends: libx11-6, libxext6, libxrandr2, libxcursor1, libxinerama1, libgl1
Suggests: pipewire-jack | jackd2
Conflicts: $OTHER_PKG
Homepage: https://luthieraudio.com
Description: Physically modelled guitar instrument (VST3, CLAP, standalone)
 $PRODUCT_NAME models the strings, body, pickups and player of a guitar and plays
 it from MIDI. This package installs the VST3 and CLAP plugins, the
 standalone application, the offline renderer and the factory content.
EOF
    find "$deb_root" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
    deb="$OUT/${PKG_NAME}_${VERSION}_amd64.deb"
    dpkg-deb --root-owner-group -Zxz --build "$deb_root" "$deb" >/dev/null
    echo "Wrote $deb"
else
    echo "dpkg-deb not found: skipping the .deb"
fi

#------------------------------------------------------------------ .rpm (best effort)
if command -v rpmbuild >/dev/null; then
    rpm_top="$work/rpm"
    mkdir -p "$rpm_top"/{BUILD,RPMS,SOURCES,SPECS,SRPMS,tree}
    t="$rpm_top/tree"
    mkdir -p "$t/usr/lib/vst3" "$t/usr/lib/clap" "$t/usr/bin" "$t/usr/share/luthier" \
             "$t/usr/share/applications" "$t/usr/share/mime/packages" \
             "$t/usr/share/icons/hicolor/256x256/apps"
    cp -R "$STAGE/$PRODUCT_NAME.vst3" "$t/usr/lib/vst3/"
    [ -e "$STAGE/$PRODUCT_NAME.clap" ] && cp "$STAGE/$PRODUCT_NAME.clap" "$t/usr/lib/clap/"
    cp "$STAGE/luthier" "$t/usr/bin/luthier"
    [ -e "$STAGE/luthier-render" ] && cp "$STAGE/luthier-render" "$t/usr/bin/"
    cp -R "$STAGE/Resources" "$t/usr/share/luthier/Resources"
    cp packaging/linux/luthier.desktop "$t/usr/share/applications/"
    cp packaging/linux/luthier.xml "$t/usr/share/mime/packages/luthier.xml"
    cp Resources/icon.png "$t/usr/share/icons/hicolor/256x256/apps/luthier.png"
    chmod 755 "$t/usr/bin/"*
    cat > "$rpm_top/SPECS/$PKG_NAME.spec" <<EOF
Name:           $PKG_NAME
Version:        $VERSION
Release:        1
Summary:        Physically modelled guitar instrument (VST3, CLAP, standalone)
License:        Proprietary
URL:            https://luthieraudio.com
BuildArch:      x86_64
AutoReqProv:    no
Requires:       alsa-lib, freetype, fontconfig
Conflicts:      $OTHER_PKG
Recommends:     libX11, libXrandr, libXcursor, libXinerama, mesa-libGL
%define debug_package %{nil}
%define __strip /bin/true
%description
$PRODUCT_NAME models the strings, body, pickups and player of a guitar and plays it
from MIDI. This package installs the VST3 and CLAP plugins, the standalone
application, the offline renderer and the factory content.
%install
mkdir -p %{buildroot}
cp -a $t/usr %{buildroot}/usr
%post
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q /usr/share/applications || :
command -v update-mime-database >/dev/null 2>&1 && update-mime-database /usr/share/mime || :
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t /usr/share/icons/hicolor || :
%postun
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q /usr/share/applications || :
command -v update-mime-database >/dev/null 2>&1 && update-mime-database /usr/share/mime || :
%files
%defattr(-,root,root,-)
"/usr/lib/vst3/$PRODUCT_NAME.vst3"
/usr/lib/clap
/usr/bin/luthier*
/usr/share/luthier
/usr/share/applications/luthier.desktop
/usr/share/mime/packages/luthier.xml
/usr/share/icons/hicolor/256x256/apps/luthier.png
EOF
    if rpmbuild -bb --quiet --define "_topdir $rpm_top" --define "_buildhost luthier" \
                --define "source_date_epoch_from_changelog 0" --define "clamp_mtime_to_source_date_epoch 1" \
                --define "use_source_date_epoch_as_buildtime 1" "$rpm_top/SPECS/$PKG_NAME.spec" >"$work/rpmbuild.log" 2>&1; then
        cp "$rpm_top"/RPMS/x86_64/*.rpm "$OUT/"
        echo "Wrote $OUT/$PKG_NAME-$VERSION-1.x86_64.rpm"
    else
        # Best effort by spec: a broken rpm must not block the tarball and .deb.
        echo "::warning::rpmbuild failed, skipping the .rpm (see below)"; tail -20 "$work/rpmbuild.log"
    fi
else
    echo "rpmbuild not found: skipping the .rpm"
fi

#------------------------------------------------------------------ signatures
if [ -n "${GPG_PRIVATE_KEY:-}" ]; then
    export GNUPGHOME="$work/gnupg"
    mkdir -p "$GNUPGHOME" && chmod 700 "$GNUPGHOME"
    echo "$GPG_PRIVATE_KEY" | gpg --batch --import
    for f in "$OUT"/*.tar.gz "$OUT"/*.deb "$OUT"/*.rpm; do
        [ -e "$f" ] || continue
        gpg --batch --yes --pinentry-mode loopback --passphrase "${GPG_PASSPHRASE:-}" \
            --armor --detach-sign --output "$f.asc" "$f"
        echo "Signed $f"
    done
else
    echo "GPG_PRIVATE_KEY not set: packages are unsigned (fine for a test build)"
fi
