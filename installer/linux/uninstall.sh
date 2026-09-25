#!/usr/bin/env bash
# Luthier for Linux - removes what install.sh installed (spec/installer.md 3.2,
# 0.3): exactly the paths in the install manifest, nothing else. User data
# (presets, guitars, tunes, settings) stays unless --purge is given.
#
#   ./uninstall.sh                 finds the user install, else the system one
#   ./uninstall.sh --user|--system pick one
#   ./uninstall.sh --purge         also delete ~/Documents/Luthier, ~/.config/Luthier
#   ./uninstall.sh --yes           no questions
#
# For a .deb install use `sudo apt remove luthier` instead.
set -euo pipefail

MODE=""
PURGE=0
ASSUME_YES=0

while [ $# -gt 0 ]; do
    case "$1" in
        --user) MODE=user ;;
        --system) MODE=system ;;
        --purge) PURGE=1 ;;
        --yes|-y) ASSUME_YES=1 ;;
        --help|-h) sed -n '2,11p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "uninstall.sh: unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

USER_APP="${XDG_DATA_HOME:-$HOME/.local/share}/luthier"
SYS_APP="/opt/Luthier"

if [ -z "$MODE" ]; then
    if [ -f "$USER_APP/install-manifest.txt" ]; then MODE=user
    elif [ -f "$SYS_APP/install-manifest.txt" ]; then MODE=system
    fi
fi

case "$MODE" in
    user) APP_DIR="$USER_APP" ;;
    system) APP_DIR="$SYS_APP" ;;
    *)
        if [ "$PURGE" -eq 1 ]; then APP_DIR=""; else
            echo "uninstall.sh: no Luthier install manifest found in $USER_APP or $SYS_APP" >&2
            echo "  (a .deb install is removed with: sudo apt remove luthier)" >&2
            exit 1
        fi ;;
esac

MANIFEST="${APP_DIR:+$APP_DIR/install-manifest.txt}"

if [ -n "$APP_DIR" ] && [ ! -f "$MANIFEST" ]; then
    echo "uninstall.sh: no manifest at $MANIFEST" >&2
    exit 1
fi

if [ "$MODE" = system ] && [ "$(id -u)" -ne 0 ] && [ -n "$APP_DIR" ]; then
    echo "uninstall.sh: removing a system install needs root (sudo)" >&2
    exit 1
fi

if [ -n "$APP_DIR" ]; then
    echo "Removing the Luthier $MODE install listed in $MANIFEST"
fi
[ "$PURGE" -eq 1 ] && echo "and PURGING user data: ~/Documents/Luthier, ~/.config/Luthier"

if [ "$ASSUME_YES" -ne 1 ] && [ -t 0 ]; then
    read -r -p "Continue? [y/N] " ok
    case "$ok" in y|Y|yes|YES) ;; *) echo "Cancelled."; exit 1 ;; esac
fi

removed=0
if [ -n "$APP_DIR" ]; then
    # Files and links first; the manifest itself and this script go last, and
    # the directories in reverse order of creation, only when empty.
    mapfile -t lines < "$MANIFEST"
    self="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"

    for line in "${lines[@]}"; do
        kind="${line%% *}"; path="${line#* }"
        case "$kind" in
            file|link)
                [ "$path" = "$MANIFEST" ] && continue
                [ "$path" = "$self" ] && continue
                if [ -e "$path" ] || [ -L "$path" ]; then rm -f "$path"; removed=$((removed + 1)); fi ;;
        esac
    done

    # A copied bundle may have picked up host-written files (e.g. a DAW's
    # cache inside the .vst3); those are not ours and are left where they are.
    for (( i=${#lines[@]}-1; i>=0; i-- )); do
        line="${lines[$i]}"; kind="${line%% *}"; path="${line#* }"
        [ "$kind" = dir ] || continue
        [ "$path" = "$APP_DIR" ] && continue
        rmdir "$path" 2>/dev/null || true
    done

    rm -f "$MANIFEST"
    [ "$self" = "$APP_DIR/uninstall.sh" ] && rm -f "$self" || rm -f "$APP_DIR/uninstall.sh"
    rmdir "$APP_DIR" 2>/dev/null || true

    if [ "$MODE" = user ]; then
        DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
        APPS_DIR="$DATA_HOME/applications"; MIME_DIR="$DATA_HOME/mime"; ICON_DIR="$DATA_HOME/icons/hicolor"
    else
        APPS_DIR="/usr/share/applications"; MIME_DIR="/usr/share/mime"; ICON_DIR="/usr/share/icons/hicolor"
    fi
    [ -d "$APPS_DIR" ] && command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q "$APPS_DIR" 2>/dev/null || true
    # The compiled database keeps the removed types until it is rebuilt, and
    # the tool insists on a packages/ folder even when empty.
    if [ -d "$MIME_DIR" ] && command -v update-mime-database >/dev/null 2>&1; then
        mkdir -p "$MIME_DIR/packages"
        update-mime-database "$MIME_DIR" >/dev/null 2>&1 || true
    fi
    if [ -f "$ICON_DIR/index.theme" ] && command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q -t -f "$ICON_DIR" 2>/dev/null || true
    fi

    echo "Removed $removed files."
    [ -d "$APP_DIR" ] && echo "Left $APP_DIR in place: it holds files this installer did not write."
fi

if [ "$PURGE" -eq 1 ]; then
    docs="${XDG_DOCUMENTS_DIR:-$HOME/Documents}/Luthier"
    conf="${XDG_CONFIG_HOME:-$HOME/.config}/Luthier"
    for d in "$docs" "$conf"; do
        if [ -d "$d" ]; then rm -rf "$d"; echo "Purged $d"; fi
    done
fi

exit 0
