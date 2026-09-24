#!/bin/bash
# Luthier for Linux: uninstaller (installer.md 3.2, 1.3).
#
#   uninstall.sh [--user|--system] [--purge] [--yes] [--quiet]
#
# Removes exactly the files listed in install-manifest.txt, which install.sh
# wrote. User data in ~/Documents/Luthier is kept unless --purge is given
# (and confirmed, unless --yes).
set -euo pipefail

MODE="" PURGE=0 ASSUME_YES=0 QUIET=0
for arg in "$@"; do
    case "$arg" in
        --user) MODE=user ;;
        --system) MODE=system ;;
        --purge) PURGE=1 ;;
        --yes|-y) ASSUME_YES=1 ;;
        --quiet|-q) QUIET=1 ;;
        -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
        *) echo "uninstall.sh: unknown option $arg" >&2; exit 2 ;;
    esac
done
if [ -z "$MODE" ]; then
    if [ "$(id -u)" -eq 0 ]; then MODE=system; else MODE=user; fi
fi
if [ "$MODE" = system ]; then
    CONTENT_DIR=/usr/local/share/luthier
else
    CONTENT_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/luthier"
fi
MANIFEST="$CONTENT_DIR/install-manifest.txt"
say() { [ $QUIET = 1 ] || echo "$@"; }

if [ ! -f "$MANIFEST" ]; then
    say "No $MODE install of Luthier found ($MANIFEST is missing)."
    exit 0
fi

if [ $ASSUME_YES = 0 ] && [ -t 0 ]; then
    read -r -p "Remove Luthier ($MODE install)? [y/N] " answer
    case "${answer:-n}" in [Yy]*) ;; *) echo "Cancelled."; exit 1 ;; esac
fi

# The manifest lists parents before children; remove in reverse order.
tac "$MANIFEST" | while IFS= read -r path; do
    [ -n "$path" ] || continue
    if [ -d "$path" ] && [ ! -L "$path" ]; then
        rmdir "$path" 2>/dev/null || true
    else
        rm -f "$path"
    fi
done
rm -f "$MANIFEST"
rmdir "$CONTENT_DIR" 2>/dev/null || true

for d in "${XDG_DATA_HOME:-$HOME/.local/share}" /usr/local/share; do
    command -v update-desktop-database >/dev/null && update-desktop-database -q "$d/applications" 2>/dev/null || true
    command -v update-mime-database    >/dev/null && update-mime-database "$d/mime" >/dev/null 2>&1 || true
    [ -d "$d/icons/hicolor" ] && command -v gtk-update-icon-cache >/dev/null && \
        gtk-update-icon-cache -q -t "$d/icons/hicolor" 2>/dev/null || true
done

if [ $PURGE = 1 ]; then
    USER_DATA="$HOME/Documents/Luthier"
    if [ -d "$USER_DATA" ]; then
        ok=$ASSUME_YES
        if [ $ok = 0 ] && [ -t 0 ]; then
            read -r -p "Also delete ALL your Luthier presets, guitars, tunes and recordings in $USER_DATA? [y/N] " a
            case "${a:-n}" in [Yy]*) ok=1 ;; esac
        fi
        [ $ok = 1 ] && rm -rf "$USER_DATA" && say "Removed $USER_DATA."
    fi
    rm -rf "${XDG_CONFIG_HOME:-$HOME/.config}/Luthier"
fi

say "Luthier has been removed."
[ $PURGE = 1 ] || say "Your presets and other files in ~/Documents/Luthier were kept."
