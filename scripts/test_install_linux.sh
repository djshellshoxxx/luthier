#!/bin/bash
# Install / upgrade / downgrade / uninstall cycle test for the Linux tarball
# installer (installer.md 13: IN-2, IN-26, IN-44, IN-45; qa-polish install rows).
#
#   scripts/test_install_linux.sh [Luthier-<v>-linux-x64.tar.gz ...]
#
# With no argument it builds two small synthetic packages (versions 1.0.0 and
# 1.1.0, the second dropping one file) from the real packaging/linux files, so
# it runs anywhere with bash and needs no build. With one or two tarballs it
# tests those instead (A then B; a single tarball is used for both).
#
# Everything happens under a throw-away HOME/XDG_DATA_HOME: the machine's own
# ~/.vst3, ~/.local and ~/Documents are never touched. Exit status is the
# number of failed checks.
set -uo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
export HOME="$work/home" XDG_DATA_HOME="$work/home/.local/share" XDG_CONFIG_HOME="$work/home/.config"
mkdir -p "$HOME"
fail=0
ok()   { echo "  ok    $*"; }
bad()  { echo "  FAIL  $*"; fail=$((fail + 1)); }
check() { local what="$1"; shift; if "$@" >/dev/null 2>&1; then ok "$what"; else bad "$what"; fi; }
absent() { local what="$1"; shift; if "$@" >/dev/null 2>&1; then bad "$what"; else ok "$what"; fi; }

# --------------------------------------------------------------- packages
make_pkg() { # make_pkg <version> <extra-file-yes|no> <out.tar.gz>
    local v="$1" extra="$2" out="$3" r="$work/pkg-$1/Luthier-$1-linux-x64"
    rm -rf "$work/pkg-$1"
    mkdir -p "$r/Luthier.vst3/Contents/x86_64-linux" "$r/Resources/Presets" "$r/Resources/BodyIRs" \
             "$r/share/applications" "$r/share/mime/packages" "$r/share/icons/hicolor/256x256/apps"
    echo "vst3 $v" > "$r/Luthier.vst3/Contents/x86_64-linux/Luthier.so"
    echo "clap $v" > "$r/Luthier.clap"
    printf '#!/bin/sh\necho luthier %s\n' "$v" > "$r/luthier"
    echo "preset" > "$r/Resources/Presets/a.luthierpreset"
    echo "ir" > "$r/Resources/BodyIRs/a.wav"
    [ "$extra" = yes ] && echo "only in this version" > "$r/Resources/Presets/old-only.luthierpreset"
    cp packaging/linux/install.sh packaging/linux/uninstall.sh "$r/"
    cp packaging/linux/luthier.desktop "$r/share/applications/"
    cp packaging/linux/luthier.xml "$r/share/mime/packages/luthier.xml"
    cp Resources/icon.png "$r/share/icons/hicolor/256x256/apps/luthier.png" 2>/dev/null \
        || echo png > "$r/share/icons/hicolor/256x256/apps/luthier.png"
    echo "$v" > "$r/VERSION"
    chmod 755 "$r/install.sh" "$r/uninstall.sh" "$r/luthier"
    tar -C "$work/pkg-$v" -czf "$out" "Luthier-$v-linux-x64"
}
if [ $# -ge 1 ]; then
    A="$1"; B="${2:-$1}"
else
    make_pkg 1.0.0 yes "$work/A.tar.gz"; make_pkg 1.1.0 no "$work/B.tar.gz"
    A="$work/A.tar.gz"; B="$work/B.tar.gz"
fi
unpack() { rm -rf "$work/run/$2"; mkdir -p "$work/run/$2"; tar -C "$work/run/$2" -xzf "$1"; echo "$work/run/$2"/*/; }
dirA="$(unpack "$A" A)"; dirB="$(unpack "$B" B)"

snapshot() { (cd "$HOME" && find . -type f -o -type l | sort); }
CONTENT="$XDG_DATA_HOME/luthier"
MANIFEST="$CONTENT/install-manifest.txt"

# --------------------------------------------------------------- 1. clean install
echo "1. per-user install into a clean HOME"
snapshot > "$work/before.txt"
"$dirA/install.sh" --user --yes >"$work/install.log" 2>&1 || { bad "install.sh exited non-zero"; cat "$work/install.log"; }
check "VST3 bundle is in ~/.vst3"             test -e "$HOME/.vst3/Luthier.vst3"
check "standalone is in ~/.local/bin"         test -x "$HOME/.local/bin/luthier"
check "content is in the share dir"           test -d "$CONTENT/Resources/Presets"
check "manifest exists"                       test -s "$MANIFEST"
check "desktop entry names an absolute Exec"  grep -q "^Exec=$HOME/.local/bin/luthier " "$XDG_DATA_HOME/applications/luthier.desktop"
check "MIME file installed for all six types" test "$(grep -c '<mime-type type=' "$XDG_DATA_HOME/mime/packages/luthier.xml")" -eq 6
if command -v update-mime-database >/dev/null; then
    check "update-mime-database registered *.luthierpreset" grep -q 'luthierpreset' "$XDG_DATA_HOME/mime/globs2"
fi

# Every manifest entry exists, and every file the install added is in it.
missing=0; while IFS= read -r p; do [ -n "$p" ] && [ ! -e "$p" ] && missing=$((missing + 1)); done < "$MANIFEST"
[ $missing -eq 0 ] && ok "every manifest entry exists" || bad "$missing manifest entries are missing on disk"
snapshot > "$work/after.txt"
unlisted=$(comm -13 "$work/before.txt" "$work/after.txt" | sed "s#^\./#$HOME/#" | grep -vxFf "$MANIFEST" | grep -vxF "$MANIFEST" \
           | grep -vE '/(mimeinfo\.cache|icon-theme\.cache|globs2?|magic|aliases|subclasses|generic-icons|icons|XMLnamespaces|types|treemagic|mime\.cache|luthier\.xml|.*\.desktop\.cache)$' | grep -v '/share/mime/' || true)
[ -z "$unlisted" ] && ok "the install wrote nothing outside its manifest" || { bad "files outside the manifest:"; echo "$unlisted" | sed 's/^/          /'; }
absent "nothing was written to ~/Documents" test -e "$HOME/Documents/Luthier"

# --------------------------------------------------------------- 2. user data survives
echo "2. user data is never touched by install/uninstall"
mkdir -p "$HOME/Documents/Luthier/Presets"; echo mine > "$HOME/Documents/Luthier/Presets/my.luthierpreset"

# --------------------------------------------------------------- 3. upgrade
echo "3. upgrade A -> B leaves no A-only artefacts"
"$dirB/install.sh" --user --yes >"$work/up.log" 2>&1 || { bad "upgrade exited non-zero"; cat "$work/up.log"; }
check "version marker is B"                   grep -qx "$(cat "$dirB/VERSION")" "$CONTENT/VERSION"
if [ "$A" != "$B" ]; then
    absent "a file only version A shipped is gone" test -e "$CONTENT/Resources/Presets/old-only.luthierpreset"
fi
check "user data still there after the upgrade" test -f "$HOME/Documents/Luthier/Presets/my.luthierpreset"

# --------------------------------------------------------------- 4. downgrade
echo "4. downgrade B -> A is clean too"
"$dirA/install.sh" --user --yes >"$work/down.log" 2>&1 || { bad "downgrade exited non-zero"; cat "$work/down.log"; }
check "version marker is A again"             grep -qx "$(cat "$dirA/VERSION")" "$CONTENT/VERSION"
check "A's own files are present"             test -d "$CONTENT/Resources/Presets"

# --------------------------------------------------------------- 5. uninstall keeps data
echo "5. uninstall removes everything installed, keeps user data"
"$CONTENT/uninstall.sh" --user --yes --quiet || bad "uninstall.sh exited non-zero"
absent "VST3 removed"                         test -e "$HOME/.vst3/Luthier.vst3"
absent "standalone removed"                   test -e "$HOME/.local/bin/luthier"
absent "content removed"                      test -e "$CONTENT/Resources"
absent "desktop entry removed"                test -e "$XDG_DATA_HOME/applications/luthier.desktop"
absent "manifest removed"                     test -e "$MANIFEST"
check "user presets kept"                     test -f "$HOME/Documents/Luthier/Presets/my.luthierpreset"
left="$(cd "$HOME" && find . -type f | grep -v '^./Documents/' | grep -v 'mimeinfo.cache\|icon-theme.cache\|share/mime/\|applications/.*cache' || true)"
[ -z "$left" ] && ok "no installed file is left behind" || { bad "left behind:"; echo "$left" | sed 's/^/          /'; }

# --------------------------------------------------------------- 6. purge
echo "6. --purge (confirmed with --yes) is the only way user data goes"
"$dirA/install.sh" --user --yes >/dev/null 2>&1
"$CONTENT/uninstall.sh" --user --purge --yes --quiet
absent "user data removed by --purge"         test -e "$HOME/Documents/Luthier"

# --------------------------------------------------------------- 7. component flags
echo "7. component flags and install.defaults (standalone-only package)"
echo "--no-vst3 --no-clap" > "$dirA/install.defaults"
"$dirA/install.sh" --user --yes >/dev/null 2>&1
absent "no VST3 with install.defaults"        test -e "$HOME/.vst3/Luthier.vst3"
absent "no CLAP with install.defaults"        test -e "$HOME/.clap/Luthier.clap"
check "standalone still installed"            test -x "$HOME/.local/bin/luthier"
check "content still installed"               test -d "$CONTENT/Resources"
"$CONTENT/uninstall.sh" --user --yes --quiet

echo
if [ $fail -eq 0 ]; then echo "install cycle: all checks passed"; else echo "install cycle: $fail check(s) FAILED"; fi
exit $fail
