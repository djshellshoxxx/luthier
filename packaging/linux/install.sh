#!/bin/sh
# Luthier tarball installer (installer.md 3.2).
#
#   ./install.sh            asks: user (~/.local, ~/.vst3) or system (/usr/local, /usr/lib/vst3)
#   ./install.sh --user     no questions, into your home folder
#   ./install.sh --system   no questions, system-wide (run with sudo)
#   ./install.sh --prefix DIR [--vst3 DIR]   anywhere
#
# Everything installed is listed in <prefix>/share/luthier/.install-manifest,
# which uninstall.sh reads.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
mode=""
prefix=""
vst3=""

while [ $# -gt 0 ]; do
    case "$1" in
        --user)   mode=user ;;
        --system) mode=system ;;
        --prefix) shift; prefix="$1"; mode=custom ;;
        --vst3)   shift; vst3="$1" ;;
        -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

if [ -z "$mode" ]; then
    printf "Install Luthier for [u]ser only (no sudo) or [s]ystem-wide? [u/s] "
    read -r answer
    case "$answer" in s|S) mode=system ;; *) mode=user ;; esac
fi

case "$mode" in
    user)   prefix="${prefix:-$HOME/.local}"; vst3="${vst3:-$HOME/.vst3}" ;;
    system) prefix="${prefix:-/usr/local}";   vst3="${vst3:-/usr/lib/vst3}" ;;
    custom) vst3="${vst3:-$prefix/lib/vst3}" ;;
esac

manifest="$prefix/share/luthier/.install-manifest"
mkdir -p "$prefix/bin" "$prefix/share" "$vst3"
: > "$here/.manifest.tmp"

copy_tree() {   # source dir, destination dir
    (cd "$1" && find . -type f) | while read -r f; do
        mkdir -p "$2/$(dirname "$f")"
        cp -p "$1/$f" "$2/$f"
        echo "$2/${f#./}" >> "$here/.manifest.tmp"
    done
}

install -m 755 "$here/bin/luthier" "$prefix/bin/luthier"
echo "$prefix/bin/luthier" >> "$here/.manifest.tmp"
copy_tree "$here/share" "$prefix/share"
copy_tree "$here/lib/vst3" "$vst3"

mkdir -p "$(dirname "$manifest")"
sort -u "$here/.manifest.tmp" > "$manifest"
echo "$manifest" >> "$manifest"
rm -f "$here/.manifest.tmp"

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q "$prefix/share/applications" 2>/dev/null || true
command -v update-mime-database >/dev/null 2>&1 && update-mime-database "$prefix/share/mime" 2>/dev/null || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t -f "$prefix/share/icons/hicolor" 2>/dev/null || true

echo "Luthier installed:"
echo "  standalone  $prefix/bin/luthier"
echo "  VST3        $vst3/Luthier.vst3"
echo "  content     $prefix/share/luthier"
case ":$PATH:" in *":$prefix/bin:"*) ;; *) echo "  (add $prefix/bin to your PATH to run 'luthier')" ;; esac
echo "Needs: ALSA, and JACK or PipeWire (pipewire-jack) for low-latency audio; X11 or XWayland."
