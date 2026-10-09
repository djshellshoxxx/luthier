#!/bin/bash
# Luthier for macOS: uninstaller (installer.md 2.3).
# Removes every file the Luthier installer wrote. Your presets, guitars, tunes
# and recordings in ~/Documents/Luthier are kept unless you answer yes below.
set -u
# The edition this uninstaller belongs to. scripts/package_macos.sh rewrites
# these two lines (and only these) when it stages the uninstaller for the
# edition it packages; the values here are Pro's, for running the file by hand.
# ID_PREFIX is the edition's bundle ID, which prefixes its package receipts.
PRODUCT="Luthier Pro"
ID_PREFIX="com.luthieraudio.luthier"
# LUTHIER_UNINSTALL_ROOT prefixes the system paths and drops sudo: it exists only
# so scripts/test_uninstall_macos.sh can run this file against a scratch tree.
ROOT="${LUTHIER_UNINSTALL_ROOT:-}"
SUDO="sudo"
[ -n "$ROOT" ] && SUDO=""
echo "This removes $PRODUCT (AU, VST3, CLAP, the app and the factory content)."
read -r -p "Continue? [y/N] " answer
case "${answer:-n}" in [Yy]*) ;; *) echo "Cancelled."; exit 1 ;; esac

paths=(
  "$ROOT/Library/Audio/Plug-Ins/Components/$PRODUCT.component"
  "$ROOT/Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3"
  "$ROOT/Library/Audio/Plug-Ins/CLAP/$PRODUCT.clap"
  "$ROOT/Applications/$PRODUCT.app"
  "$ROOT/Library/Application Support/Luthier"
  "$ROOT/Applications/$PRODUCT"
)
echo "Administrator rights are needed to remove files in /Library and /Applications."
for p in "${paths[@]}"; do
  [ -e "$p" ] && $SUDO rm -rf "$p" && echo "Removed $p"
done
for id in $ID_PREFIX.au $ID_PREFIX.vst3 $ID_PREFIX.clap \
          $ID_PREFIX.app $ID_PREFIX.content $ID_PREFIX.uninstaller; do
  $SUDO pkgutil --forget "$id" >/dev/null 2>&1 || true
done
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

read -r -p "Also delete your presets, guitars, tunes and recordings in ~/Documents/Luthier? [y/N] " purge
case "${purge:-n}" in
  [Yy]*) rm -rf "$HOME/Documents/Luthier" "$HOME/Library/Application Support/Luthier"
         echo "Removed your Luthier data." ;;
  *)     echo "Kept ~/Documents/Luthier." ;;
esac
echo "$PRODUCT has been removed."
