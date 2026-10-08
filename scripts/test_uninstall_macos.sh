#!/bin/bash
# Runs packaging/macos/Uninstall.command against a scratch tree with piped answers
# (installer.md 2.3, IN-22). It checks the script's logic anywhere with bash; it
# cannot check that Installer.app laid the files where the script looks, which
# scripts/package_macos.sh's payload paths and RELEASING section 6 cover.
#
#   scripts/test_uninstall_macos.sh        exit status = number of failed checks
set -uo pipefail
cd "$(dirname "$0")/.."
SCRIPT="$PWD/packaging/macos/Uninstall.command"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
fail=0
ok()  { echo "  ok    $*"; }
bad() { echo "  FAIL  $*"; fail=$((fail + 1)); }

lay_out() { # a fresh "installed" scratch machine
    rm -rf "$work/root" "$work/home"
    R="$work/root"; H="$work/home"
    mkdir -p "$R/Library/Audio/Plug-Ins/Components/Luthier.component/Contents" \
             "$R/Library/Audio/Plug-Ins/VST3/Luthier.vst3/Contents" \
             "$R/Library/Audio/Plug-Ins/CLAP" "$R/Applications/Luthier.app/Contents" \
             "$R/Applications/Luthier" "$R/Library/Application Support/Luthier/Resources" \
             "$R/Library/Audio/Plug-Ins/VST3/Other.vst3" \
             "$H/Documents/Luthier/Presets"
    touch "$R/Library/Audio/Plug-Ins/CLAP/Luthier.clap" "$R/Library/Audio/Plug-Ins/VST3/Other.vst3/keep"
    echo mine > "$H/Documents/Luthier/Presets/my.luthierpreset"
}
run() { LUTHIER_UNINSTALL_ROOT="$R" HOME="$H" bash "$SCRIPT" >"$work/out.txt" 2>&1; }

echo "1. answer no to the first question: nothing is touched"
lay_out; printf 'n\n' | run
[ -d "$R/Applications/Luthier.app" ] && ok "app still there" || bad "cancel removed something"

echo "2. remove, keep user data"
lay_out; printf 'y\nn\n' | run
for p in "Library/Audio/Plug-Ins/Components/Luthier.component" "Library/Audio/Plug-Ins/VST3/Luthier.vst3" \
         "Library/Audio/Plug-Ins/CLAP/Luthier.clap" "Applications/Luthier.app" "Applications/Luthier" \
         "Library/Application Support/Luthier"; do
    [ ! -e "$R/$p" ] && ok "removed $p" || bad "$p is still there"
done
[ -f "$R/Library/Audio/Plug-Ins/VST3/Other.vst3/keep" ] && ok "another vendor's plug-in untouched" || bad "removed another plug-in"
[ -f "$H/Documents/Luthier/Presets/my.luthierpreset" ] && ok "user presets kept" || bad "user presets deleted"
grep -q "Kept ~/Documents/Luthier" "$work/out.txt" && ok "says the data was kept" || bad "does not say the data was kept"

echo "3. remove and purge user data"
lay_out; printf 'y\ny\n' | run
[ ! -e "$H/Documents/Luthier" ] && ok "user data removed on request" || bad "purge left user data"

echo "4. running it on a machine where Luthier is already gone is harmless"
printf 'y\nn\n' | run && ok "exits cleanly" || bad "fails when nothing is installed"

echo
[ $fail -eq 0 ] && echo "macOS uninstall: all checks passed" || echo "macOS uninstall: $fail check(s) FAILED"
exit $fail
