#!/bin/bash
# Luthier for macOS: uninstaller (installer.md 2.3).
# Removes every file the Luthier installer wrote. Your presets, guitars, tunes
# and recordings in ~/Documents/Luthier are kept unless you answer yes below.
set -u
# LUTHIER_UNINSTALL_ROOT prefixes the system paths and drops sudo: it exists only
# so scripts/test_uninstall_macos.sh can run this file against a scratch tree.
ROOT="${LUTHIER_UNINSTALL_ROOT:-}"
SUDO="sudo"
[ -n "$ROOT" ] && SUDO=""
echo "This removes Luthier (AU, VST3, CLAP, the app and the factory content)."
read -r -p "Continue? [y/N] " answer
case "${answer:-n}" in [Yy]*) ;; *) echo "Cancelled."; exit 1 ;; esac

paths=(
  "$ROOT/Library/Audio/Plug-Ins/Components/Luthier.component"
  "$ROOT/Library/Audio/Plug-Ins/VST3/Luthier.vst3"
  "$ROOT/Library/Audio/Plug-Ins/CLAP/Luthier.clap"
  "$ROOT/Applications/Luthier.app"
  "$ROOT/Library/Application Support/Luthier"
  "$ROOT/Applications/Luthier"
)
echo "Administrator rights are needed to remove files in /Library and /Applications."
for p in "${paths[@]}"; do
  [ -e "$p" ] && $SUDO rm -rf "$p" && echo "Removed $p"
done
for id in com.luthieraudio.luthier.au com.luthieraudio.luthier.vst3 com.luthieraudio.luthier.clap \
          com.luthieraudio.luthier.app com.luthieraudio.luthier.content com.luthieraudio.luthier.uninstaller; do
  $SUDO pkgutil --forget "$id" >/dev/null 2>&1 || true
done
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

read -r -p "Also delete your presets, guitars, tunes and recordings in ~/Documents/Luthier? [y/N] " purge
case "${purge:-n}" in
  [Yy]*) rm -rf "$HOME/Documents/Luthier" "$HOME/Library/Application Support/Luthier"
         echo "Removed your Luthier data." ;;
  *)     echo "Kept ~/Documents/Luthier." ;;
esac
echo "Luthier has been removed."
