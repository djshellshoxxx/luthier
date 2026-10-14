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

lay_out() { # lay_out [product]: a fresh "installed" scratch machine (default: Luthier Pro)
    local P="${1:-Luthier Pro}"
    rm -rf "$work/root" "$work/home"
    R="$work/root"; H="$work/home"
    mkdir -p "$R/Library/Audio/Plug-Ins/Components/$P.component/Contents" \
             "$R/Library/Audio/Plug-Ins/VST3/$P.vst3/Contents" \
             "$R/Library/Audio/Plug-Ins/CLAP" "$R/Applications/$P.app/Contents" \
             "$R/Applications/$P" "$R/Library/Application Support/Luthier/Resources" \
             "$R/Library/Audio/Plug-Ins/VST3/Other.vst3" \
             "$H/Documents/Luthier/Presets"
    touch "$R/Library/Audio/Plug-Ins/CLAP/$P.clap" "$R/Library/Audio/Plug-Ins/VST3/Other.vst3/keep"
    echo mine > "$H/Documents/Luthier/Presets/my.luthierpreset"
}
run() { LUTHIER_UNINSTALL_ROOT="$R" HOME="$H" bash "$SCRIPT" >"$work/out.txt" 2>&1; }

echo "1. answer no to the first question: nothing is touched"
lay_out; printf 'n\n' | run
[ -d "$R/Applications/Luthier Pro.app" ] && ok "app still there" || bad "cancel removed something"

echo "2. remove, keep user data"
lay_out; printf 'y\nn\n' | run
for p in "Library/Audio/Plug-Ins/Components/Luthier Pro.component" "Library/Audio/Plug-Ins/VST3/Luthier Pro.vst3" \
         "Library/Audio/Plug-Ins/CLAP/Luthier Pro.clap" "Applications/Luthier Pro.app" "Applications/Luthier Pro" \
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

echo "5. the Free edition's uninstaller (settings written the way scripts/package_macos.sh does)"
FREE_SCRIPT="$work/Uninstall-free.command"
sed -e 's|^PRODUCT=.*|PRODUCT="Luthier Free"|' -e 's|^ID_PREFIX=.*|ID_PREFIX="com.luthieraudio.luthierfree"|' "$SCRIPT" > "$FREE_SCRIPT"
lay_out "Luthier Free"
# Pro is installed beside Free: Free's uninstaller must leave it alone.
mkdir -p "$R/Applications/Luthier Pro.app/Contents" "$R/Library/Audio/Plug-Ins/VST3/Luthier Pro.vst3/Contents"
printf 'y\nn\n' | LUTHIER_UNINSTALL_ROOT="$R" HOME="$H" bash "$FREE_SCRIPT" >"$work/out.txt" 2>&1
for p in "Library/Audio/Plug-Ins/Components/Luthier Free.component" "Library/Audio/Plug-Ins/VST3/Luthier Free.vst3" \
         "Library/Audio/Plug-Ins/CLAP/Luthier Free.clap" "Applications/Luthier Free.app" "Applications/Luthier Free"; do
    [ ! -e "$R/$p" ] && ok "removed $p" || bad "$p is still there"
done
[ -d "$R/Applications/Luthier Pro.app" ] && [ -d "$R/Library/Audio/Plug-Ins/VST3/Luthier Pro.vst3" ] \
    && ok "Pro's app and VST3 untouched" || bad "Free's uninstaller removed Pro"
grep -q "Luthier Free has been removed" "$work/out.txt" && ok "names the edition it removed" || bad "does not name the edition"

echo
[ $fail -eq 0 ] && echo "macOS uninstall: all checks passed" || echo "macOS uninstall: $fail check(s) FAILED"
exit $fail
