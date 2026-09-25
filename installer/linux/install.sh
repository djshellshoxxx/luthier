#!/usr/bin/env bash
# Luthier for Linux - installer for the .tar.gz bundle (spec/installer.md 3.2).
#
# Run from the extracted folder. Offers a user install (no root) or a system
# install (root). Writes a manifest of every path it creates, which
# uninstall.sh removes and nothing else (installer.md 0.3, 13).
#
#   ./install.sh                  ask user / system (system when run as root)
#   ./install.sh --user           ~/.vst3, ~/.local/bin, ~/.local/share/luthier
#   sudo ./install.sh --system    /usr/lib/vst3, /usr/bin, /opt/Luthier
#
# Options:
#   --vst3-dir DIR    where the VST3 bundle goes (default per mode)
#   --no-vst3         skip the plugin (the standalone-only bundle has none)
#   --no-standalone   skip the standalone application and launcher
#   --no-desktop      skip the desktop entry, file types and icons
#   --yes             no questions
#   --help
set -euo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE=""
VST3_DIR=""
WANT_VST3=1
WANT_STANDALONE=1
WANT_DESKTOP=1
ASSUME_YES=0

usage() { sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }

while [ $# -gt 0 ]; do
    case "$1" in
        --user) MODE=user ;;
        --system) MODE=system ;;
        --vst3-dir) VST3_DIR="$2"; shift ;;
        --no-vst3) WANT_VST3=0 ;;
        --no-standalone) WANT_STANDALONE=0 ;;
        --no-desktop) WANT_DESKTOP=0 ;;
        --yes|-y) ASSUME_YES=1 ;;
        --help|-h) usage; exit 0 ;;
        *) echo "install.sh: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

[ -x "$SRC/Luthier" ] || [ -d "$SRC/Luthier.vst3" ] || {
    echo "install.sh: run this from the extracted Luthier folder (no Luthier or Luthier.vst3 here)" >&2
    exit 1
}
[ -d "$SRC/Resources" ] || { echo "install.sh: Resources folder missing beside install.sh" >&2; exit 1; }
[ -d "$SRC/Luthier.vst3" ] || WANT_VST3=0
[ -x "$SRC/Luthier" ] || WANT_STANDALONE=0

if [ -z "$MODE" ]; then
    if [ "$(id -u)" -eq 0 ]; then
        MODE=system
    elif [ "$ASSUME_YES" -eq 1 ] || [ ! -t 0 ]; then
        MODE=user
    else
        echo "Install Luthier for:"
        echo "  1) this user only   (~/.vst3, ~/.local/bin, ~/.local/share/luthier)"
        echo "  2) the whole system (/usr/lib/vst3, /usr/bin, /opt/Luthier; needs sudo)"
        read -r -p "Choice [1]: " choice
        case "${choice:-1}" in
            2) MODE=system ;;
            *) MODE=user ;;
        esac
    fi
fi

if [ "$MODE" = system ] && [ "$(id -u)" -ne 0 ]; then
    echo "install.sh: a system install needs root; run with sudo, or use --user" >&2
    exit 1
fi

# Where things go. The layout mirrors the .deb (installer/linux/Packaging.cmake)
# so the two install methods leave the same shape behind.
if [ "$MODE" = user ]; then
    DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
    APP_DIR="$DATA_HOME/luthier"
    BIN_DIR="$HOME/.local/bin"
    [ -n "$VST3_DIR" ] || VST3_DIR="$HOME/.vst3"
    APPS_DIR="$DATA_HOME/applications"
    MIME_DIR="$DATA_HOME/mime"
    ICON_DIR="$DATA_HOME/icons/hicolor"
else
    APP_DIR="/opt/Luthier"
    BIN_DIR="/usr/bin"
    [ -n "$VST3_DIR" ] || VST3_DIR="/usr/lib/vst3"
    APPS_DIR="/usr/share/applications"
    MIME_DIR="/usr/share/mime"
    ICON_DIR="/usr/share/icons/hicolor"
fi

VERSION="$(sed -n 's/^Luthier \([0-9][0-9.]*\) for Linux.*/\1/p' "$SRC/README.txt" 2>/dev/null | head -1)"
[ -n "$VERSION" ] || VERSION="unknown"

MANIFEST="$APP_DIR/install-manifest.txt"

if [ -f "$MANIFEST" ]; then
    echo "An earlier Luthier install is recorded in $MANIFEST; removing it first."
    if [ -x "$APP_DIR/uninstall.sh" ]; then
        "$APP_DIR/uninstall.sh" --"$MODE" --yes
    else
        "$SRC/uninstall.sh" --"$MODE" --yes
    fi
fi

echo "Installing Luthier $VERSION ($MODE):"
[ "$WANT_STANDALONE" -eq 1 ] && echo "  standalone  $APP_DIR/Luthier  (launcher $BIN_DIR/luthier)"
[ "$WANT_VST3" -eq 1 ]       && echo "  VST3        $VST3_DIR/Luthier.vst3"
echo "  content     $APP_DIR/Resources"
[ "$WANT_DESKTOP" -eq 1 ]    && echo "  desktop     $APPS_DIR, $MIME_DIR/packages, $ICON_DIR"

if [ "$ASSUME_YES" -ne 1 ] && [ -t 0 ]; then
    read -r -p "Continue? [Y/n] " ok
    case "$ok" in n|N|no|NO) echo "Cancelled."; exit 1 ;; esac
fi

# --- the manifest ------------------------------------------------------------
# One line per path: "file <path>", "link <path>" or "dir <path>". Files and
# links are removed by uninstall.sh; dirs only when empty afterwards.
mkdir -p "$APP_DIR"
: > "$MANIFEST"
note()      { printf '%s %s\n' "$1" "$2" >> "$MANIFEST"; }
mkdir_note() {
    # Records only the directories this run creates, outermost first, so a
    # pre-existing ~/.local/bin is left alone by uninstall.sh.
    local path="$1" parents=() p="$1"
    while [ ! -d "$p" ] && [ "$p" != "/" ]; do parents=("$p" "${parents[@]}"); p="$(dirname "$p")"; done
    mkdir -p "$path"
    local d; for d in "${parents[@]}"; do note dir "$d"; done
}
copy_file() {   # copy_file <src> <dst>
    mkdir_note "$(dirname "$2")"
    cp -f "$1" "$2"
    note file "$2"
}
copy_tree() {   # copy_tree <srcdir> <dstdir>   (every file recorded, symlinks kept)
    local src="$1" dst="$2" rel
    mkdir_note "$dst"
    (cd "$src" && find . -mindepth 1 -type d | sort) | while read -r rel; do
        mkdir -p "$dst/${rel#./}"; note dir "$dst/${rel#./}"
    done
    (cd "$src" && find . -mindepth 1 \( -type f -o -type l \) | sort) | while read -r rel; do
        rel="${rel#./}"
        cp -P -f "$src/$rel" "$dst/$rel"
        if [ -L "$dst/$rel" ]; then note link "$dst/$rel"; else note file "$dst/$rel"; fi
    done
}
make_link() {   # make_link <target> <linkpath>
    mkdir_note "$(dirname "$2")"
    ln -sfn "$1" "$2"
    note link "$2"
}

note dir "$APP_DIR"
note file "$MANIFEST"

# --- content -------------------------------------------------------------------
copy_tree "$SRC/Resources" "$APP_DIR/Resources"

# --- standalone --------------------------------------------------------------
if [ "$WANT_STANDALONE" -eq 1 ]; then
    copy_file "$SRC/Luthier" "$APP_DIR/Luthier"
    chmod 755 "$APP_DIR/Luthier"
    make_link "$APP_DIR/Luthier" "$BIN_DIR/luthier"
fi

# --- VST3 ---------------------------------------------------------------------
if [ "$WANT_VST3" -eq 1 ]; then
    rm -rf "$VST3_DIR/Luthier.vst3"
    copy_tree "$SRC/Luthier.vst3" "$VST3_DIR/Luthier.vst3"
    # The bundle's Resources link pointed at ../Resources in the extracted
    # folder; point it at the installed content instead.
    rm -f "$VST3_DIR/Luthier.vst3/Resources"
    ln -sfn "$APP_DIR/Resources" "$VST3_DIR/Luthier.vst3/Resources"
    grep -qxF "link $VST3_DIR/Luthier.vst3/Resources" "$MANIFEST" || note link "$VST3_DIR/Luthier.vst3/Resources"
fi

# --- desktop integration --------------------------------------------------------
if [ "$WANT_DESKTOP" -eq 1 ]; then
    if [ "$WANT_STANDALONE" -eq 1 ]; then
        mkdir_note "$APPS_DIR"
        # Exec= gets the launcher's absolute path: ~/.local/bin is not always on PATH.
        sed "s|^Exec=luthier|Exec=$BIN_DIR/luthier|" "$SRC/luthier.desktop" > "$APPS_DIR/luthier.desktop"
        note file "$APPS_DIR/luthier.desktop"
    fi
    copy_file "$SRC/luthier.xml" "$MIME_DIR/packages/luthier.xml"
    copy_file "$SRC/icons/luthier-512.png" "$ICON_DIR/512x512/apps/luthier.png"
    copy_file "$SRC/icons/luthier-128.png" "$ICON_DIR/128x128/apps/luthier.png"

    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q "$APPS_DIR" 2>/dev/null || true
    command -v update-mime-database    >/dev/null 2>&1 && update-mime-database "$MIME_DIR" >/dev/null 2>&1 || true
    if command -v gtk-update-icon-cache >/dev/null 2>&1 && [ -f "$ICON_DIR/index.theme" ]; then
        gtk-update-icon-cache -q -t -f "$ICON_DIR" 2>/dev/null || true
    fi
fi

# --- licences, uninstaller, marker ----------------------------------------------
copy_file "$SRC/THIRD_PARTY_LICENCES.txt" "$APP_DIR/THIRD_PARTY_LICENCES.txt"
copy_file "$SRC/uninstall.sh" "$APP_DIR/uninstall.sh"
chmod 755 "$APP_DIR/uninstall.sh"

# installer.md 6: the package-level marker. The plugin keeps its own per-user
# one in ~/Documents/Luthier/.installed_version, written on first load.
printf '%s\n' "$VERSION" > "$APP_DIR/.installed_version"
note file "$APP_DIR/.installed_version"

echo
echo "Done. Installed $(grep -c '^file\|^link' "$MANIFEST") files; manifest: $MANIFEST"
[ "$WANT_STANDALONE" -eq 1 ] && echo "Run it:    $BIN_DIR/luthier"
[ "$WANT_VST3" -eq 1 ] && echo "VST3 at:   $VST3_DIR/Luthier.vst3  (rescan plugins in your DAW)"
if [ "$MODE" = user ] && [ "$WANT_STANDALONE" -eq 1 ]; then
    case ":$PATH:" in *":$BIN_DIR:"*) ;; *) echo "Note:      $BIN_DIR is not on your PATH" ;; esac
fi
echo "Uninstall: $APP_DIR/uninstall.sh"
