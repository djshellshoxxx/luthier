#!/usr/bin/env bash
# Luthier Linux install / uninstall tests (spec/installer.md 13).
#
# Needs no root: the .deb is unpacked with dpkg-deb -x into a scratch root and
# the tarballs are installed with install.sh --user into a scratch HOME, so
# nothing on the machine is touched. Every check prints PASS / FAIL / SKIP.
#
#   installer/linux/test-install.sh                 packages from dist/ (else build/packages)
#   installer/linux/test-install.sh --dist DIR
#   installer/linux/test-install.sh --real          also dpkg -i / dpkg -P for real (root)
#   installer/linux/test-install.sh --keep          leave the scratch folder behind
#
# What is covered:
#   .deb      control fields, maintainer scripts, file layout (installer.md 3.1),
#             symlinks, VST3 entry point, shared-library needs, headless start
#   .tar.gz   layout, portable run, install.sh --user, manifest == files on
#             disk, headless start via the launcher, re-install (upgrade path),
#             uninstall removes exactly the manifest, user data survives,
#             --purge removes it
#   standalone .tar.gz   the same without the VST3
#   --real    dpkg -i, postinst marker, run, dpkg -P, nothing left
#
# Not covered here (installer.md 13): upgrade A -> B across versions (one
# version builds at a time), file-association double-click (needs a desktop
# session; the standalone does not open files from argv yet), signature
# verification (keys are outside the repo; see scripts/release.sh).
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DIST=""
REAL=0
KEEP=0

while [ $# -gt 0 ]; do
    case "$1" in
        --dist) DIST="$2"; shift ;;
        --real) REAL=1 ;;
        --keep) KEEP=1 ;;
        --help|-h) sed -n '2,26p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

if [ -z "$DIST" ]; then
    if ls "$ROOT"/dist/*.deb >/dev/null 2>&1; then DIST="$ROOT/dist"
    elif ls "$ROOT"/build/packages/*.deb >/dev/null 2>&1; then DIST="$ROOT/build/packages"
    else echo "no packages in dist/ or build/packages; run scripts/release.sh first" >&2; exit 1; fi
fi

VERSION="$(sed -n 's/^project(Luthier[[:space:]]\+VERSION[[:space:]]\+\([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)"
DEB="$(ls -t "$DIST"/luthier_*_*.deb 2>/dev/null | head -1)"
TGZ="$(ls -t "$DIST"/Luthier-*-linux-*.tar.gz 2>/dev/null | grep -v -- '-standalone.tar.gz' | head -1)"
TGZS="$(ls -t "$DIST"/Luthier-*-linux-*-standalone.tar.gz 2>/dev/null | head -1)"

# The scratch folder. Everything the tests write goes under here and the trap
# removes it; --keep leaves it for a look.
T="$(mktemp -d "${TMPDIR:-/tmp}/luthier-install-test.XXXXXX")"
cleanup() { [ "$KEEP" -eq 1 ] || find "$T" -delete; }
trap cleanup EXIT

pass=0; fail=0; skip=0
PASS() { echo "PASS  $*"; pass=$((pass + 1)); }
FAIL() { echo "FAIL  $*"; fail=$((fail + 1)); }
SKIP() { echo "SKIP  $*"; skip=$((skip + 1)); }
check() { local msg="$1"; shift; if "$@" >/dev/null 2>&1; then PASS "$msg"; else FAIL "$msg"; fi; }
is_link_to() { [ -L "$1" ] && [ "$(readlink "$1")" = "$2" ]; }

# Starts a binary under Xvfb and waits: alive after the timeout is a pass
# (timeout exits 124), anything else is a crash or an early exit.
headless_run() {
    local exe="$1" home="$2" log="$3" rc
    if ! command -v xvfb-run >/dev/null 2>&1; then return 2; fi
    HOME="$home" XDG_CONFIG_HOME= XDG_DATA_HOME= XDG_DOCUMENTS_DIR= \
        timeout -s TERM 12 xvfb-run -a "$exe" >"$log" 2>&1
    rc=$?
    [ "$rc" -eq 124 ] && return 0
    return 1
}

echo "== packages in $DIST (version $VERSION)"
ls -l "$DIST"
echo

#==============================================================================
# .deb, unpacked into a scratch root
#==============================================================================
if [ -n "$DEB" ]; then
    echo "== $DEB"
    R="$T/debroot"; C="$T/debcontrol"; mkdir -p "$R" "$C"
    check "dpkg-deb --info reads the package"            dpkg-deb --info "$DEB"
    check "dpkg-deb -x unpacks into a scratch root"      dpkg-deb -x "$DEB" "$R"
    check "dpkg-deb -e extracts the control area"        dpkg-deb -e "$DEB" "$C"

    ctl="$(dpkg-deb --field "$DEB" 2>/dev/null)"
    check "Package: luthier"                             grep -q "^Package: luthier$" <<<"$ctl"
    check "Version: $VERSION"                            grep -q "^Version: $VERSION$" <<<"$ctl"
    check "Architecture: amd64"                          grep -q "^Architecture: amd64$" <<<"$ctl"
    check "Depends lists libasound2 (shlibdeps ran)"     grep -Eq "^Depends:.*libasound2" <<<"$ctl"
    check "Recommends lists a JACK provider"             grep -Eq "^Recommends:.*jack" <<<"$ctl"
    check "postinst present and executable"              test -x "$C/postinst"
    check "postrm present and executable"                test -x "$C/postrm"
    check "postinst parses (sh -n)"                      sh -n "$C/postinst"
    check "postrm parses (sh -n)"                        sh -n "$C/postrm"
    check "postinst writes /opt/Luthier/.installed_version = $VERSION" grep -q "\"$VERSION\" > \"/opt/Luthier/.installed_version\"" "$C/postinst"
    check "postinst refreshes desktop, MIME and icon caches" bash -c "grep -q update-desktop-database '$C/postinst' && grep -q update-mime-database '$C/postinst' && grep -q gtk-update-icon-cache '$C/postinst'"
    check "postrm removes the marker"                    grep -q '/opt/Luthier/.installed_version' "$C/postrm"

    SO="$R/usr/lib/vst3/Luthier.vst3/Contents/x86_64-linux/Luthier.so"
    check "VST3 .so at /usr/lib/vst3/Luthier.vst3/Contents/x86_64-linux/" test -f "$SO"
    check "VST3 moduleinfo.json"                          test -f "$R/usr/lib/vst3/Luthier.vst3/Contents/Resources/moduleinfo.json"
    check "VST3 exports GetPluginFactory"                bash -c "nm -D '$SO' | grep -q ' T GetPluginFactory'"
    check "VST3 exports ModuleEntry"                     bash -c "nm -D '$SO' | grep -q ' T ModuleEntry'"
    check "Luthier.vst3/Resources -> /opt/Luthier/Resources" is_link_to "$R/usr/lib/vst3/Luthier.vst3/Resources" "/opt/Luthier/Resources"
    check "standalone at /opt/Luthier/Luthier, executable" test -x "$R/opt/Luthier/Luthier"
    check "/usr/bin/luthier -> /opt/Luthier/Luthier"     is_link_to "$R/usr/bin/luthier" "/opt/Luthier/Luthier"
    for d in BodyIRs CabIRs Parts Guitars Tunes Fonts; do
        check "content: /opt/Luthier/Resources/$d"       test -d "$R/opt/Luthier/Resources/$d"
    done
    check "no Windows .ico in the content"               bash -c "! find '$R/opt/Luthier/Resources' -name '*.ico' | grep -q ."
    check "desktop file"                                 test -f "$R/usr/share/applications/luthier.desktop"
    if command -v desktop-file-validate >/dev/null 2>&1; then
        check "desktop file validates"                   desktop-file-validate "$R/usr/share/applications/luthier.desktop"
    else SKIP "desktop-file-validate not installed"; fi
    check "MIME xml"                                     test -f "$R/usr/share/mime/packages/luthier.xml"
    if command -v xmllint >/dev/null 2>&1; then
        check "MIME xml is well-formed"                  xmllint --noout "$R/usr/share/mime/packages/luthier.xml"
    fi
    for ext in luthierpreset luthierguitar luthierpart luthiertune luthierpattern luthierset luthierloop luthiercontent midprofile; do
        check "MIME glob *.$ext"                         grep -q "\*\.$ext\"" "$R/usr/share/mime/packages/luthier.xml"
    done
    check "icon 512x512"                                 test -f "$R/usr/share/icons/hicolor/512x512/apps/luthier.png"
    check "icon 128x128"                                 test -f "$R/usr/share/icons/hicolor/128x128/apps/luthier.png"
    check "THIRD_PARTY_LICENCES.txt"                     test -f "$R/usr/share/doc/luthier/THIRD_PARTY_LICENCES.txt"
    check "licences mention JUCE, VST 3 SDK, Lato, Bebas Neue" bash -c "f='$R/usr/share/doc/luthier/THIRD_PARTY_LICENCES.txt'; grep -q JUCE \$f && grep -q 'VST 3' \$f && grep -q Lato \$f && grep -q 'Bebas' \$f"
    check "Debian copyright file"                        test -f "$R/usr/share/doc/luthier/copyright"

    # The package's own file list against what dpkg-deb -x put on disk.
    dpkg-deb --fsys-tarfile "$DEB" | tar -t | sed 's|^\./||; s|/$||' | grep -v '^$' | sort > "$T/deb-listed.txt"
    (cd "$R" && find . -mindepth 1 | sed 's|^\./||' | sort) > "$T/deb-ondisk.txt"
    check "dpkg-deb -c list == files on disk"            diff -q "$T/deb-listed.txt" "$T/deb-ondisk.txt"

    check "standalone: no missing shared libraries (ldd)" bash -c "! ldd '$R/opt/Luthier/Luthier' | grep -q 'not found'"
    check "VST3: no missing shared libraries (ldd)"       bash -c "! ldd '$SO' | grep -q 'not found'"

    mkdir -p "$T/debhome"
    headless_run "$R/opt/Luthier/Luthier" "$T/debhome" "$T/deb-run.log"; rc=$?
    case $rc in
        0) PASS "standalone starts under Xvfb and is still alive after 12 s" ;;
        2) SKIP "xvfb-run not installed; headless start not tested" ;;
        *) FAIL "standalone under Xvfb exited early (see $T/deb-run.log)"; tail -20 "$T/deb-run.log" ;;
    esac
    check "standalone found no reason to complain about Resources" bash -c "! grep -qi 'No Resources folder' '$T/deb-run.log'"

    echo "-- package size and file count"
    ls -l "$DEB"; echo "$(wc -l < "$T/deb-listed.txt") entries"
    echo
else
    SKIP "no .deb found"
fi

#==============================================================================
# The tarballs: portable run, install.sh --user, uninstall.sh
#==============================================================================
test_tarball() {   # test_tarball <file> <expect-vst3 0|1> <label>
    local tgz="$1" want_vst3="$2" label="$3"
    echo "== $tgz ($label)"
    local X="$T/$label"; mkdir -p "$X"
    check "extracts"                                     tar -xzf "$tgz" -C "$X"
    local top; top="$(ls "$X")"
    check "one top-level folder: $top"                   test "$(ls "$X" | wc -l)" -eq 1
    local S="$X/$top"
    check "top folder is named Luthier-$VERSION-linux-*"  bash -c "[[ '$top' == Luthier-$VERSION-linux-* ]]"
    check "Luthier standalone, executable"               test -x "$S/Luthier"
    check "Resources/BodyIRs beside it"                  test -d "$S/Resources/BodyIRs"
    check "Resources/Fonts beside it"                    test -d "$S/Resources/Fonts"
    if [ "$want_vst3" -eq 1 ]; then
        check "Luthier.vst3/Contents/x86_64-linux/Luthier.so" test -f "$S/Luthier.vst3/Contents/x86_64-linux/Luthier.so"
        check "Luthier.vst3/Resources -> ../Resources"   is_link_to "$S/Luthier.vst3/Resources" "../Resources"
        check "that link resolves in place"              test -d "$S/Luthier.vst3/Resources/CabIRs"
    else
        check "no Luthier.vst3 in the standalone bundle" test ! -e "$S/Luthier.vst3"
    fi
    for f in install.sh uninstall.sh luthier.desktop luthier.xml README.txt THIRD_PARTY_LICENCES.txt icons/luthier-512.png icons/luthier-128.png; do
        check "ships $f"                                 test -e "$S/$f"
    done
    check "install.sh and uninstall.sh executable"       bash -c "test -x '$S/install.sh' && test -x '$S/uninstall.sh'"
    check "README names version $VERSION"               grep -q "^Luthier $VERSION for Linux" "$S/README.txt"

    # Portable: run in place.
    mkdir -p "$X/home-portable"
    touch "$X/before-portable-run"
    headless_run "$S/Luthier" "$X/home-portable" "$X/portable-run.log"; rc=$?
    case $rc in
        0) PASS "portable: ./Luthier runs in place under Xvfb" ;;
        2) SKIP "xvfb-run not installed; portable run not tested" ;;
        *) FAIL "portable: ./Luthier exited early (see $X/portable-run.log)"; tail -20 "$X/portable-run.log" ;;
    esac
    check "portable: nothing written into the extracted folder" bash -c "! find '$S' -newer '$X/before-portable-run' -type f | grep -q ."

    # User install into a scratch HOME.
    local H="$X/home"; mkdir -p "$H/Documents/Luthier/Presets/User"
    echo '{"magic":"luthier.preset"}' > "$H/Documents/Luthier/Presets/User/keep-me.luthierpreset"
    check "install.sh --user --yes succeeds"            env -i HOME="$H" PATH="$PATH" "$S/install.sh" --user --yes
    local A="$H/.local/share/luthier" M="$H/.local/share/luthier/install-manifest.txt"
    check "manifest written"                             test -f "$M"
    check "standalone at ~/.local/share/luthier/Luthier" test -x "$A/Luthier"
    check "~/.local/bin/luthier -> the standalone"       is_link_to "$H/.local/bin/luthier" "$A/Luthier"
    check "content at ~/.local/share/luthier/Resources"  test -d "$A/Resources/CabIRs"
    check ".installed_version = $VERSION"               bash -c "[ \"\$(cat '$A/.installed_version')\" = '$VERSION' ]"
    check "uninstall.sh copied beside the install"       test -x "$A/uninstall.sh"
    check "THIRD_PARTY_LICENCES.txt installed"           test -f "$A/THIRD_PARTY_LICENCES.txt"
    if [ "$want_vst3" -eq 1 ]; then
        check "VST3 at ~/.vst3/Luthier.vst3"            test -f "$H/.vst3/Luthier.vst3/Contents/x86_64-linux/Luthier.so"
        check "~/.vst3/Luthier.vst3/Resources -> installed content" is_link_to "$H/.vst3/Luthier.vst3/Resources" "$A/Resources"
    else
        check "no ~/.vst3 written by the standalone bundle" test ! -e "$H/.vst3"
    fi
    check "desktop entry with absolute Exec"            grep -q "^Exec=$H/.local/bin/luthier %f" "$H/.local/share/applications/luthier.desktop"
    check "MIME xml installed"                           test -f "$H/.local/share/mime/packages/luthier.xml"
    check "icons installed"                              bash -c "test -f '$H/.local/share/icons/hicolor/512x512/apps/luthier.png' && test -f '$H/.local/share/icons/hicolor/128x128/apps/luthier.png'"

    # installer.md 13: installed files match the manifest, and nothing else
    # was written apart from the caches the freedesktop tools compile.
    grep -E '^(file|link) ' "$M" | cut -d' ' -f2- | sort > "$X/manifest-files.txt"
    local missing=0 p
    while read -r p; do [ -e "$p" ] || [ -L "$p" ] || { missing=$((missing + 1)); echo "   missing: $p"; }; done < "$X/manifest-files.txt"
    check "every manifest entry exists on disk ($(wc -l < "$X/manifest-files.txt") entries)" test "$missing" -eq 0
    find "$H/.local" "$H/.vst3" \( -type f -o -type l \) 2>/dev/null | sort > "$X/ondisk.txt"
    comm -13 "$X/manifest-files.txt" "$X/ondisk.txt" | grep -Ev '/(mime/[^/]+$|mime/[^/]+/[^/]+\.xml$|mime/mime\.cache$|applications/mimeinfo\.cache$|icons/hicolor/icon-theme\.cache$)' > "$X/extra.txt" || true
    check "nothing on disk outside the manifest (caches aside)" test ! -s "$X/extra.txt"
    [ -s "$X/extra.txt" ] && sed 's/^/   extra: /' "$X/extra.txt"
    local count1; count1="$(wc -l < "$X/manifest-files.txt")"

    headless_run "$H/.local/bin/luthier" "$H" "$X/installed-run.log"; rc=$?
    case $rc in
        0) PASS "installed launcher runs under Xvfb" ;;
        2) SKIP "xvfb-run not installed" ;;
        *) FAIL "installed launcher exited early (see $X/installed-run.log)"; tail -20 "$X/installed-run.log" ;;
    esac

    # Upgrade path: installing over an install removes the old one first.
    check "re-install over itself succeeds (upgrade path)" env -i HOME="$H" PATH="$PATH" "$S/install.sh" --user --yes
    local count2; count2="$(grep -cE '^(file|link) ' "$M")"
    check "re-install leaves the same file count ($count1)" test "$count1" -eq "$count2"

    # Uninstall: exactly the manifest, user data kept.
    cp "$X/manifest-files.txt" "$X/manifest-before-uninstall.txt"
    check "uninstall.sh --yes succeeds"                 env -i HOME="$H" PATH="$PATH" "$A/uninstall.sh" --yes
    local left=0
    while read -r p; do if [ -e "$p" ] || [ -L "$p" ]; then left=$((left + 1)); echo "   left: $p"; fi; done < "$X/manifest-before-uninstall.txt"
    check "every manifest entry removed"                 test "$left" -eq 0
    check "~/.local/share/luthier removed"               test ! -e "$A"
    [ "$want_vst3" -eq 1 ] && check "~/.vst3/Luthier.vst3 removed" test ! -e "$H/.vst3/Luthier.vst3"
    check "user preset survived the uninstall"           test -f "$H/Documents/Luthier/Presets/User/keep-me.luthierpreset"
    check "uninstall.sh --purge --yes succeeds"         env -i HOME="$H" PATH="$PATH" "$S/uninstall.sh" --purge --yes
    check "--purge removed ~/Documents/Luthier"          test ! -e "$H/Documents/Luthier"

    echo "-- package size"; ls -l "$tgz"; echo
}

[ -n "$TGZ" ]  && test_tarball "$TGZ" 1 full   || SKIP "no full tarball found"
[ -n "$TGZS" ] && test_tarball "$TGZS" 0 standalone || SKIP "no standalone tarball found"

#==============================================================================
# --real: dpkg -i on this machine (root only)
#==============================================================================
if [ "$REAL" -eq 1 ] && [ -n "$DEB" ]; then
    echo "== real dpkg install of $DEB"
    if [ "$(id -u)" -ne 0 ]; then
        SKIP "--real needs root"
    else
        check "dpkg -i"                                  dpkg -i "$DEB"
        check "/opt/Luthier/.installed_version = $VERSION" bash -c "[ \"\$(cat /opt/Luthier/.installed_version)\" = '$VERSION' ]"
        check "/usr/bin/luthier resolves"                test -x /usr/bin/luthier
        check "dpkg -L lists the VST3"                   bash -c "dpkg -L luthier | grep -q '/usr/lib/vst3/Luthier.vst3/Contents/x86_64-linux/Luthier.so'"
        mkdir -p "$T/realhome"
        headless_run /usr/bin/luthier "$T/realhome" "$T/real-run.log"; rc=$?
        case $rc in 0) PASS "installed /usr/bin/luthier runs under Xvfb" ;; 2) SKIP "xvfb-run not installed" ;; *) FAIL "installed luthier exited early"; tail -20 "$T/real-run.log" ;; esac
        dpkg -L luthier | sort > "$T/real-files.txt"
        check "dpkg -P luthier"                          dpkg -P luthier
        left=0
        while read -r p; do
            case "$p" in /usr/lib/vst3|/usr/share/*|/usr/bin|/usr/lib|/usr|/opt|/.) continue ;; esac
            if [ -e "$p" ] || [ -L "$p" ]; then left=$((left + 1)); echo "   left: $p"; fi
        done < "$T/real-files.txt"
        check "every packaged path removed"              test "$left" -eq 0
        check "/opt/Luthier gone (marker removed by postrm)" test ! -e /opt/Luthier
    fi
    echo
fi

echo "== $pass passed, $fail failed, $skip skipped"
[ "$KEEP" -eq 1 ] && echo "scratch folder kept: $T"
[ "$fail" -eq 0 ]
