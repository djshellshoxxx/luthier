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
  "$ROOT/Applications/$PRODUCT"
)
echo "Administrator rights are needed to remove files in /Library and /Applications."
for p in "${paths[@]}"; do
  [ -e "$p" ] && $SUDO rm -rf "$p" && echo "Removed $p"
done
# Both editions use this factory directory. A plugin-only installation also
# needs it; do not decide ownership from the standalone application alone.
OTHER_PRODUCT="Luthier Free"
[ "$PRODUCT" = "Luthier Free" ] && OTHER_PRODUCT="Luthier Pro"
other_present=false
for p in "$ROOT/Applications/$OTHER_PRODUCT.app" \
         "$ROOT/Library/Audio/Plug-Ins/Components/$OTHER_PRODUCT.component" \
         "$ROOT/Library/Audio/Plug-Ins/VST3/$OTHER_PRODUCT.vst3" \
         "$ROOT/Library/Audio/Plug-Ins/CLAP/$OTHER_PRODUCT.clap"; do
  [ -e "$p" ] && other_present=true
done
if $other_present; then
  echo "Kept shared factory content for $OTHER_PRODUCT."
else
  $SUDO rm -rf "$ROOT/Library/Application Support/Luthier/Resources"
  # Preserve independently managed policy/settings beside the factory content.
  $SUDO rmdir "$ROOT/Library/Application Support/Luthier" >/dev/null 2>&1 || true
fi
# Scratch-tree tests must not alter the real machine's receipts or registrar.
if [ -z "$ROOT" ]; then
for id in $ID_PREFIX.au $ID_PREFIX.vst3 $ID_PREFIX.clap \
          $ID_PREFIX.app $ID_PREFIX.content $ID_PREFIX.uninstaller; do
  $SUDO pkgutil --forget "$id" >/dev/null 2>&1 || true
done
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
fi

read -r -p "Also delete your presets, guitars, tunes and recordings in ~/Documents/Luthier? [y/N] " purge
case "${purge:-n}" in
  [Yy]*) rm -rf "$HOME/Documents/Luthier" "$HOME/Library/Application Support/Luthier"
         echo "Removed your Luthier data." ;;
  *)     echo "Kept ~/Documents/Luthier." ;;
esac
echo "$PRODUCT has been removed."
