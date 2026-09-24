#!/bin/sh
# Removes what install.sh installed, from its manifest (installer.md 3.2).
#
#   ./uninstall.sh            finds the manifest in ~/.local, then /usr/local
#   ./uninstall.sh --prefix DIR
#
# Your own presets, guitars and recordings in ~/Documents/Luthier are not touched.
set -e
prefix=""
[ "$1" = "--prefix" ] && prefix="$2"

if [ -z "$prefix" ]; then
    for p in "$HOME/.local" /usr/local; do
        [ -f "$p/share/luthier/.install-manifest" ] && prefix="$p" && break
    done
fi

manifest="$prefix/share/luthier/.install-manifest"
[ -f "$manifest" ] || { echo "No Luthier install manifest found (looked in ~/.local and /usr/local)." >&2; exit 1; }

# The manifest lists itself last; read it into memory first.
files="$(cat "$manifest")"
echo "$files" | while read -r f; do
    [ -n "$f" ] && rm -f "$f"
done

# Empty folders the install created.
for d in "$prefix/share/luthier" "$(echo "$files" | grep '/Luthier.vst3/' | head -1 | sed 's|/Luthier.vst3/.*|/Luthier.vst3|')"; do
    [ -n "$d" ] && [ -d "$d" ] && find "$d" -depth -type d -empty -delete 2>/dev/null || true
done

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q "$prefix/share/applications" 2>/dev/null || true
command -v update-mime-database >/dev/null 2>&1 && update-mime-database "$prefix/share/mime" 2>/dev/null || true

echo "Luthier removed from $prefix."
