#!/bin/bash
# Build the macOS installer from the universal products scripts/ci_build.sh staged:
#
#   dist/installers/Luthier-<version>-macOS.dmg   containing Luthier-<version>.pkg
#
# One component package per part (installer.md 2.1, 2.2), combined by
# productbuild into a distribution package with a component picker:
#   AU        /Library/Audio/Plug-Ins/Components/Luthier.component
#   VST3      /Library/Audio/Plug-Ins/VST3/Luthier.vst3
#   CLAP      /Library/Audio/Plug-Ins/CLAP/Luthier.clap
#   App       /Applications/Luthier.app  (+ /Applications/Luthier/Uninstall.command)
#   Content   /Library/Application Support/Luthier/Resources   (required)
#
# Signing and notarisation run only when their secrets are present; without them
# the bundles stay ad-hoc signed and the .pkg unsigned, which is fine for testing
# and is reported, never silently skipped.
#   MACOS_CERT_P12_BASE64, MACOS_CERT_PASSWORD   Developer ID Application + Installer
#                                                identities in one .p12, base64
#   MACOS_DEV_ID_APP        "Developer ID Application: Name (TEAMID)"
#   MACOS_DEV_ID_INSTALLER  "Developer ID Installer: Name (TEAMID)"
#   APPLE_ID, APPLE_APP_PASSWORD, APPLE_TEAM_ID  notarytool credentials
# Other environment: VERSION (default from CMakeLists.txt), DIST_DIR (dist).
set -euo pipefail
cd "$(dirname "$0")/.."

DIST_DIR="${DIST_DIR:-dist}"
STAGE="$DIST_DIR/macos"
OUT="$DIST_DIR/installers"
VERSION="${VERSION:-$(sed -nE 's/^project\(Luthier VERSION ([0-9.]+).*/\1/p' CMakeLists.txt | tr -d '\r')}"
ID_PREFIX="com.luthieraudio.luthier"

[ -e "$STAGE/Luthier.vst3" ] || { echo "Nothing staged in $STAGE: run scripts/ci_build.sh first" >&2; exit 1; }
mkdir -p "$OUT"
work="$(mktemp -d)"
KEYCHAIN=""
cleanup() {
    [ -n "$KEYCHAIN" ] && security delete-keychain "$KEYCHAIN" >/dev/null 2>&1
    rm -rf "$work"
}
trap cleanup EXIT

#------------------------------------------------------------------ identities
APP_ID="-"          # ad hoc
INSTALLER_ID=""
if [ -n "${MACOS_CERT_P12_BASE64:-}" ] && [ -n "${MACOS_DEV_ID_APP:-}" ]; then
    KEYCHAIN="$work/signing.keychain-db"
    kc_pass="$(uuidgen)"
    security create-keychain -p "$kc_pass" "$KEYCHAIN"
    security set-keychain-settings -lut 3600 "$KEYCHAIN"
    security unlock-keychain -p "$kc_pass" "$KEYCHAIN"
    echo "$MACOS_CERT_P12_BASE64" | base64 --decode > "$work/cert.p12"
    security import "$work/cert.p12" -k "$KEYCHAIN" -P "${MACOS_CERT_PASSWORD:-}" \
        -T /usr/bin/codesign -T /usr/bin/productsign -T /usr/bin/pkgbuild -T /usr/bin/productbuild
    rm -f "$work/cert.p12"
    security set-key-partition-list -S apple-tool:,apple: -s -k "$kc_pass" "$KEYCHAIN" >/dev/null
    # Put it on the search list so codesign and productsign find the identities.
    # shellcheck disable=SC2046
    security list-keychains -d user -s "$KEYCHAIN" $(security list-keychains -d user | tr -d '"')
    APP_ID="$MACOS_DEV_ID_APP"
    INSTALLER_ID="${MACOS_DEV_ID_INSTALLER:-}"
    echo "Signing with: $APP_ID"
else
    echo "::warning::MACOS_CERT_P12_BASE64 / MACOS_DEV_ID_APP not set: ad-hoc signing, no notarisation"
fi

sign() { # sign <bundle> <entitlements>
    local flags=(--force --timestamp --options runtime --entitlements "$2" --sign "$APP_ID")
    [ "$APP_ID" = "-" ] && flags=(--force --options runtime --entitlements "$2" --sign -)
    codesign "${flags[@]}" "$1"
    codesign --verify --strict --verbose=2 "$1"
}

#------------------------------------------------------------------ payloads
# Each component gets its own payload root so pkgbuild packs exactly one thing.
payload() { mkdir -p "$work/payload/$1$2"; echo "$work/payload/$1$2"; }

cp -R "$STAGE/Luthier.component" "$(payload au /Library/Audio/Plug-Ins/Components)/"
cp -R "$STAGE/Luthier.vst3"      "$(payload vst3 /Library/Audio/Plug-Ins/VST3)/"
HAVE_CLAP=0
if [ -e "$STAGE/Luthier.clap" ]; then
    cp -R "$STAGE/Luthier.clap" "$(payload clap /Library/Audio/Plug-Ins/CLAP)/"
    HAVE_CLAP=1
fi
cp -R "$STAGE/Luthier.app" "$(payload app /Applications)/"
mkdir -p "$work/payload/app/Applications/Luthier"
cp packaging/macos/Uninstall.command "$work/payload/app/Applications/Luthier/"
cp -R "$STAGE/Resources" "$(payload content "/Library/Application Support/Luthier")/"

sign "$work/payload/au/Library/Audio/Plug-Ins/Components/Luthier.component" packaging/macos/plugin-entitlements.plist
sign "$work/payload/vst3/Library/Audio/Plug-Ins/VST3/Luthier.vst3"          packaging/macos/plugin-entitlements.plist
[ $HAVE_CLAP = 1 ] && sign "$work/payload/clap/Library/Audio/Plug-Ins/CLAP/Luthier.clap" packaging/macos/plugin-entitlements.plist
sign "$work/payload/app/Applications/Luthier.app" packaging/macos/entitlements.plist

#------------------------------------------------------------------ component pkgs
comp() { # comp <key> <identifier-suffix> [scripts-dir]
    local args=(--root "$work/payload/$1" --identifier "$ID_PREFIX.$2" --version "$VERSION"
                --install-location / --ownership recommended)
    [ -n "${3:-}" ] && args+=(--scripts "$3")
    # Plug-in and app bundles must land where they are put, never "relocated"
    # to a copy the user moved elsewhere: a component plist says so.
    pkgbuild --analyze --root "$work/payload/$1" "$work/$1.plist" >/dev/null
    local i=0
    while /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$work/$1.plist" >/dev/null 2>&1; do
        i=$((i + 1))
    done
    args+=(--component-plist "$work/$1.plist")
    pkgbuild "${args[@]}" "$work/pkgs/$1.pkg"
}
mkdir -p "$work/pkgs" "$work/scripts-au"
cp packaging/macos/scripts/postinstall-au "$work/scripts-au/postinstall"
comp au au "$work/scripts-au"
comp vst3 vst3
[ $HAVE_CLAP = 1 ] && comp clap clap
comp app app
comp content content

#------------------------------------------------------------------ distribution
clap_line="" clap_choice="" clap_ref=""
if [ $HAVE_CLAP = 1 ]; then
    clap_line="<line choice=\"clap\"/>"
    clap_choice="<choice id=\"clap\" title=\"CLAP plug-in\" description=\"Luthier.clap in /Library/Audio/Plug-Ins/CLAP\"><pkg-ref id=\"$ID_PREFIX.clap\"/></choice>"
    clap_ref="<pkg-ref id=\"$ID_PREFIX.clap\" version=\"$VERSION\" onConclusion=\"none\">clap.pkg</pkg-ref>"
fi
mkdir -p "$work/resources"
cp packaging/common/EULA.txt "$work/resources/License.txt"
cat > "$work/resources/Welcome.txt" <<EOF
Luthier $VERSION

This installs the Luthier instrument as an Audio Unit, VST3 and CLAP plug-in
and as a standalone application, together with its factory content (body and
cabinet impulse responses, guitars, parts, presets and tunes).

Your own presets, guitars and tunes live in ~/Documents/Luthier and are never
touched by the installer or the uninstaller.
EOF
# installer.md 2.1 (IN-19): productbuild cannot run a post-install button, so the
# last page says what to do next and where the uninstaller is.
cat > "$work/resources/Conclusion.txt" <<EOF
Luthier $VERSION is installed.

Next: open Luthier from /Applications (standalone), or rescan plug-ins in your
DAW (Logic: Settings > Plug-in Manager > Reset & Rescan Selection).

If your DAW does not list Luthier, see docs/TROUBLESHOOTING.md.

To uninstall, run /Applications/Luthier/Uninstall.command. Your own presets,
guitars and tunes in ~/Documents/Luthier are kept.
EOF
cat > "$work/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Luthier $VERSION</title>
    <organization>$ID_PREFIX</organization>
    <domains enable_localSystem="true" enable_currentUserHome="false" enable_anywhere="false"/>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64" rootVolumeOnly="true"/>
    <os-version min="10.13"/>
    <welcome file="Welcome.txt" mime-type="text/plain"/>
    <license file="License.txt" mime-type="text/plain"/>
    <conclusion file="Conclusion.txt" mime-type="text/plain"/>
    <choices-outline>
        <line choice="au"/>
        <line choice="vst3"/>
        $clap_line
        <line choice="app"/>
        <line choice="content"/>
    </choices-outline>
    <choice id="au" title="Audio Unit plug-in" description="Luthier.component in /Library/Audio/Plug-Ins/Components (Logic, GarageBand, ...)"><pkg-ref id="$ID_PREFIX.au"/></choice>
    <choice id="vst3" title="VST3 plug-in" description="Luthier.vst3 in /Library/Audio/Plug-Ins/VST3"><pkg-ref id="$ID_PREFIX.vst3"/></choice>
    $clap_choice
    <choice id="app" title="Standalone application" description="Luthier.app in /Applications, and the uninstaller"><pkg-ref id="$ID_PREFIX.app"/></choice>
    <choice id="content" title="Factory content" description="Impulse responses, guitars, parts, presets and tunes (needed by every format)" enabled="false" selected="true"><pkg-ref id="$ID_PREFIX.content"/></choice>
    <pkg-ref id="$ID_PREFIX.au" version="$VERSION" onConclusion="none">au.pkg</pkg-ref>
    <pkg-ref id="$ID_PREFIX.vst3" version="$VERSION" onConclusion="none">vst3.pkg</pkg-ref>
    $clap_ref
    <pkg-ref id="$ID_PREFIX.app" version="$VERSION" onConclusion="none">app.pkg</pkg-ref>
    <pkg-ref id="$ID_PREFIX.content" version="$VERSION" onConclusion="none">content.pkg</pkg-ref>
</installer-gui-script>
EOF

pkg_name="Luthier-$VERSION.pkg"
productbuild --distribution "$work/distribution.xml" --resources "$work/resources" \
             --package-path "$work/pkgs" "$work/unsigned.pkg"
if [ -n "$INSTALLER_ID" ]; then
    productsign --timestamp --sign "$INSTALLER_ID" "$work/unsigned.pkg" "$work/$pkg_name"
    pkgutil --check-signature "$work/$pkg_name"
else
    echo "::warning::MACOS_DEV_ID_INSTALLER not set: the .pkg is unsigned"
    mv "$work/unsigned.pkg" "$work/$pkg_name"
fi

#------------------------------------------------------------------ notarise
notarise() { # notarise <file>
    if [ -n "${APPLE_ID:-}" ] && [ -n "${APPLE_APP_PASSWORD:-}" ] && [ -n "${APPLE_TEAM_ID:-}" ] \
       && [ "$APP_ID" != "-" ]; then
        xcrun notarytool submit "$1" --apple-id "$APPLE_ID" --password "$APPLE_APP_PASSWORD" \
              --team-id "$APPLE_TEAM_ID" --wait --timeout 2h
        xcrun stapler staple "$1"
        return 0
    fi
    echo "::warning::Apple notarisation credentials not set: $(basename "$1") is not notarised"
}
[ -n "$INSTALLER_ID" ] && notarise "$work/$pkg_name"

#------------------------------------------------------------------ dmg
dmg_root="$work/dmg"
mkdir -p "$dmg_root"
cp "$work/$pkg_name" "$dmg_root/"
cat > "$dmg_root/Read me first.txt" <<EOF
Luthier $VERSION for macOS (Apple Silicon and Intel)

1. Double-click $pkg_name and follow the installer.
2. Open your DAW and rescan plug-ins, or open Luthier from Applications.

To uninstall, run /Applications/Luthier/Uninstall.command.
EOF
dmg="$OUT/Luthier-$VERSION-macOS.dmg"
rm -f "$dmg"
hdiutil create -volname "Luthier $VERSION" -srcfolder "$dmg_root" -fs HFS+ -format UDZO -ov "$dmg"
if [ "$APP_ID" != "-" ]; then
    codesign --force --timestamp --sign "$APP_ID" "$dmg"
    notarise "$dmg"
fi
echo "Wrote $dmg"
