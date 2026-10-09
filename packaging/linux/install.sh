#!/bin/bash
# Luthier for Linux: installer for the .tar.gz release (installer.md 3).
#
# The bundles carry the edition's product name, <product> below: "Luthier Pro"
# or "Luthier Free" (whichever .vst3 / .clap sits beside this script).
#
#   ./install.sh            per-user install (the default when not root):
#                             ~/.vst3/<product>.vst3
#                             ~/.clap/<product>.clap
#                             ~/.local/bin/luthier, ~/.local/bin/luthier-render
#                             ~/.local/share/luthier/Resources   (factory content)
#                             ~/.local/share/applications, mime, icons
#   sudo ./install.sh --system
#                           system-wide install (the default when root):
#                             /usr/local/lib/vst3/<product>.vst3
#                             /usr/lib/clap/<product>.clap       (the CLAP system path)
#                             /usr/local/bin/luthier, luthier-render
#                             /usr/local/share/luthier/Resources
#                             /usr/local/share/applications, mime, icons
#   Options:
#     --user | --system     choose the mode explicitly
#     --no-vst3 --no-clap --no-standalone --no-content
#                           leave a component out (content is needed by all three)
#     --yes                 do not ask for confirmation
#
# Every file written is recorded in <content dir>/install-manifest.txt, which
# uninstall.sh reads, so uninstalling removes exactly what was installed.
# Nothing under ~/Documents/Luthier (user presets, guitars, tunes...) is touched.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
MODE=""
WANT_VST3=1 WANT_CLAP=1 WANT_APP=1 WANT_CONTENT=1 ASSUME_YES=0

# A package may carry install.defaults (e.g. "--no-vst3 --no-clap" for the
# standalone-only archive); command-line options add to them.
DEFAULTS=""
[ -r "$HERE/install.defaults" ] && DEFAULTS="$(tr '\n' ' ' < "$HERE/install.defaults")"

# shellcheck disable=SC2086
for arg in $DEFAULTS "$@"; do
    case "$arg" in
        --user) MODE=user ;;
        --system) MODE=system ;;
        --no-vst3) WANT_VST3=0 ;;
        --no-clap) WANT_CLAP=0 ;;
        --no-standalone) WANT_APP=0 ;;
        --no-content) WANT_CONTENT=0 ;;
        --yes|-y) ASSUME_YES=1 ;;
        -h|--help) sed -n '2,30p' "$0"; exit 0 ;;
        *) echo "install.sh: unknown option $arg (try --help)" >&2; exit 2 ;;
    esac
done

if [ -z "$MODE" ]; then
    if [ "$(id -u)" -eq 0 ]; then MODE=system; else MODE=user; fi
fi

if [ "$MODE" = system ]; then
    [ "$(id -u)" -eq 0 ] || { echo "A system install needs root: sudo $0 --system" >&2; exit 1; }
    VST3_DIR=/usr/local/lib/vst3
    CLAP_DIR=/usr/lib/clap
    BIN_DIR=/usr/local/bin
    SHARE=/usr/local/share
else
    VST3_DIR="$HOME/.vst3"
    CLAP_DIR="$HOME/.clap"
    BIN_DIR="$HOME/.local/bin"
    SHARE="${XDG_DATA_HOME:-$HOME/.local/share}"
fi
CONTENT_DIR="$SHARE/luthier"
MANIFEST="$CONTENT_DIR/install-manifest.txt"
VERSION="$(cat "$HERE/VERSION" 2>/dev/null || echo unknown)"

# Pro and Free ship as differently named bundles; install whichever is here.
shopt -s nullglob
vst3_found=("$HERE"/*.vst3)
clap_found=("$HERE"/*.clap)
shopt -u nullglob
VST3_SRC="${vst3_found[0]:-}"
CLAP_SRC="${clap_found[0]:-}"
if [ $WANT_VST3 = 1 ] && [ -z "$VST3_SRC" ]; then
    echo "install.sh: no .vst3 bundle beside this script (a standalone-only package? use --no-vst3)" >&2
    exit 1
fi
VST3_NAME="${VST3_SRC##*/}"
CLAP_NAME="${CLAP_SRC##*/}"

echo "Luthier $VERSION - $MODE install"
[ $WANT_VST3 = 1 ]    && echo "  VST3        -> $VST3_DIR/$VST3_NAME"
[ $WANT_CLAP = 1 ]    && [ -n "$CLAP_SRC" ] && echo "  CLAP        -> $CLAP_DIR/$CLAP_NAME"
[ $WANT_APP = 1 ]     && echo "  Standalone  -> $BIN_DIR/luthier"
[ $WANT_CONTENT = 1 ] && echo "  Content     -> $CONTENT_DIR/Resources"

if [ $ASSUME_YES = 0 ] && [ -t 0 ]; then
    read -r -p "Continue? [Y/n] " answer
    case "${answer:-y}" in [Yy]*) ;; *) echo "Cancelled."; exit 1 ;; esac
fi

# An earlier install is removed first, so no stale file outlives an upgrade
# (installer.md 13: "no A artefacts remain").
if [ -f "$MANIFEST" ] && [ -x "$HERE/uninstall.sh" ]; then
    echo "Removing the previous install..."
    "$HERE/uninstall.sh" --"$MODE" --yes --quiet || true
fi

mkdir -p "$CONTENT_DIR"
: > "$MANIFEST.tmp"
work_desktop="$(mktemp)"
trap 'rm -f "$work_desktop"' EXIT

record() { # every path written, deepest last; uninstall removes in reverse
    find "$1" -depth -print | tac >> "$MANIFEST.tmp"
}

put() { # put <source> <destination>
    local src="$1" dst="$2"
    mkdir -p "$(dirname "$dst")"
    rm -rf "$dst"
    cp -R "$src" "$dst"
    record "$dst"
}

if [ $WANT_VST3 = 1 ]; then
    put "$VST3_SRC" "$VST3_DIR/$VST3_NAME"
fi
if [ $WANT_CLAP = 1 ] && [ -n "$CLAP_SRC" ]; then
    put "$CLAP_SRC" "$CLAP_DIR/$CLAP_NAME"
fi
if [ $WANT_CONTENT = 1 ]; then
    put "$HERE/Resources" "$CONTENT_DIR/Resources"
fi
if [ $WANT_APP = 1 ]; then
    put "$HERE/luthier" "$BIN_DIR/luthier"
    chmod 755 "$BIN_DIR/luthier"
    if [ -e "$HERE/luthier-render" ]; then
        put "$HERE/luthier-render" "$BIN_DIR/luthier-render"
        chmod 755 "$BIN_DIR/luthier-render"
    fi
    # Launchers often lack ~/.local/bin on PATH, so the entry names the binary by
    # its absolute path (a bare "Exec=luthier" would silently fail to start).
    sed "s#^Exec=luthier #Exec=$BIN_DIR/luthier #" "$HERE/share/applications/luthier.desktop" \
        > "$work_desktop"
    put "$work_desktop" "$SHARE/applications/luthier.desktop"
    put "$HERE/share/mime/packages/luthier.xml"    "$SHARE/mime/packages/luthier.xml"
    put "$HERE/share/icons/hicolor/256x256/apps/luthier.png" \
        "$SHARE/icons/hicolor/256x256/apps/luthier.png"

    command -v update-desktop-database >/dev/null && update-desktop-database -q "$SHARE/applications" || true
    command -v update-mime-database    >/dev/null && update-mime-database "$SHARE/mime" >/dev/null 2>&1 || true
    command -v gtk-update-icon-cache   >/dev/null && gtk-update-icon-cache -q -t "$SHARE/icons/hicolor" 2>/dev/null || true
fi

cp "$HERE/uninstall.sh" "$CONTENT_DIR/uninstall.sh"
chmod 755 "$CONTENT_DIR/uninstall.sh"
echo "$CONTENT_DIR/uninstall.sh" >> "$MANIFEST.tmp"
echo "$VERSION" > "$CONTENT_DIR/VERSION"
echo "$CONTENT_DIR/VERSION" >> "$MANIFEST.tmp"
mv "$MANIFEST.tmp" "$MANIFEST"

echo
echo "Installed. Uninstall with: $CONTENT_DIR/uninstall.sh"
if [ "$MODE" = user ] && [ $WANT_APP = 1 ]; then
    case ":$PATH:" in *":$BIN_DIR:"*) ;; *) echo "Note: $BIN_DIR is not on your PATH." ;; esac
fi
echo "Audio: Luthier's standalone uses ALSA or JACK (PipeWire's JACK layer works)."
