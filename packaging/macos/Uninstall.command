#!/bin/bash
# Luthier for macOS: uninstaller (installer.md 2.3).
# Removes every file the Luthier installer wrote. Your presets, guitars, tunes
# and recordings in ~/Documents/Luthier are kept unless you answer yes below.
set -u
echo "This removes Luthier (AU, VST3, CLAP, the app and the factory content)."
read -r -p "Continue? [y/N] " answer
case "${answer:-n}" in [Yy]*) ;; *) echo "Cancelled."; exit 1 ;; esac

paths=(
  "/Library/Audio/Plug-Ins/Components/Luthier.component"
  "/Library/Audio/Plug-Ins/VST3/Luthier.vst3"
  "/Library/Audio/Plug-Ins/CLAP/Luthier.clap"
  "/Applications/Luthier.app"
  "/Library/Application Support/Luthier"
  "/Applications/Luthier"
)
echo "Administrator rights are needed to remove files in /Library and /Applications."
for p in "${paths[@]}"; do
  [ -e "$p" ] && sudo rm -rf "$p" && echo "Removed $p"
done
for id in com.luthieraudio.luthier.au com.luthieraudio.luthier.vst3 com.luthieraudio.luthier.clap \
          com.luthieraudio.luthier.app com.luthieraudio.luthier.content com.luthieraudio.luthier.uninstaller; do
  sudo pkgutil --forget "$id" >/dev/null 2>&1 || true
done
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

read -r -p "Also delete your presets, guitars, tunes and recordings in ~/Documents/Luthier? [y/N] " purge
case "${purge:-n}" in
  [Yy]*) rm -rf "$HOME/Documents/Luthier" "$HOME/Library/Application Support/Luthier"
         echo "Removed your Luthier data." ;;
  *)     echo "Kept ~/Documents/Luthier." ;;
esac
echo "Luthier has been removed."
