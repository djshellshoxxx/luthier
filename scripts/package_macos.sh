#!/bin/bash
# Build the macOS installer from the universal products scripts/ci_build.sh staged.
# One edition per run (LUTHIER_EDITION, the same variable ci_build.sh reads):
# <Edition> is Pro or Free, <Product> is "Luthier Pro" or "Luthier Free".
#
#   dist/installers/Luthier-<Edition>-<version>-macOS.dmg   containing Luthier-<Edition>-<version>.pkg
#
# One component package per part (installer.md 2.1, 2.2), combined by
# productbuild into a distribution package with a component picker:
#   AU        /Library/Audio/Plug-Ins/Components/<Product>.component
#   VST3      /Library/Audio/Plug-Ins/VST3/<Product>.vst3
#   CLAP      /Library/Audio/Plug-Ins/CLAP/<Product>.clap
#   App       /Applications/<Product>.app  (+ /Applications/<Product>/Uninstall.command)
#   Content   /Library/Application Support/Luthier/Resources   (required, shared by both editions)
#
# Signing and notarisation run only when their secrets are present; without them
# the bundles stay ad-hoc signed and the .pkg unsigned, which is fine for testing
# and is reported, never silently skipped.
#   MACOS_CERT_P12_BASE64, MACOS_CERT_PASSWORD   Developer ID Application + Installer
#                                                identities in one .p12, base64
#   MACOS_DEV_ID_APP        "Developer ID Application: Name (TEAMID)"
#   MACOS_DEV_ID_INSTALLER  "Developer ID Installer: Name (TEAMID)"
#   APPLE_ID, APPLE_APP_PASSWORD, APPLE_TEAM_ID  notarytool credentials
# Other environment: LUTHIER_EDITION (PAID default, or FREE), VERSION (default from
# CMakeLists.txt), DIST_DIR (dist).
set -euo pipefail
cd "$(dirname "$0")/.."

# The edition decides which names scripts/ci_build.sh stage wrote: keep this
# case in step with the one there. ID_PREFIX is the edition's BUNDLE_ID from
# cmake/Editions.cmake, so the two editions' package receipts never collide.
LUTHIER_EDITION="${LUTHIER_EDITION:-PAID}"
case "$LUTHIER_EDITION" in
    PAID) PRODUCT_NAME="Luthier Pro";  EDITION_SLUG=Pro;  ID_PREFIX="com.luthieraudio.luthier" ;;
    FREE) PRODUCT_NAME="Luthier Free"; EDITION_SLUG=Free; ID_PREFIX="com.luthieraudio.luthierfree" ;;
    *) echo "package_macos.sh: LUTHIER_EDITION must be PAID or FREE" >&2; exit 1 ;;
esac

DIST_DIR="${DIST_DIR:-dist}"
STAGE="$DIST_DIR/macos"
OUT="$DIST_DIR/installers"
VERSION="${VERSION:-$(sed -nE 's/^project\(Luthier VERSION ([0-9.]+).*/\1/p' CMakeLists.txt | tr -d '\r')}"

[ -e "$STAGE/$PRODUCT_NAME.vst3" ] || {
    echo "Nothing staged in $STAGE for $PRODUCT_NAME: run LUTHIER_EDITION=$LUTHIER_EDITION scripts/ci_build.sh stage first" >&2
    exit 1
}
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

cp -R "$STAGE/$PRODUCT_NAME.component" "$(payload au /Library/Audio/Plug-Ins/Components)/"
cp -R "$STAGE/$PRODUCT_NAME.vst3"      "$(payload vst3 /Library/Audio/Plug-Ins/VST3)/"
HAVE_CLAP=0
if [ -e "$STAGE/$PRODUCT_NAME.clap" ]; then
    cp -R "$STAGE/$PRODUCT_NAME.clap" "$(payload clap /Library/Audio/Plug-Ins/CLAP)/"
    HAVE_CLAP=1
fi
cp -R "$STAGE/$PRODUCT_NAME.app" "$(payload app /Applications)/"
# The uninstaller is the repo's one script with this edition's product name and
# package-ID prefix written into its two settings lines (sed without -i: BSD
# and GNU sed disagree about it).
mkdir -p "$work/payload/app/Applications/$PRODUCT_NAME"
uninstaller="$work/payload/app/Applications/$PRODUCT_NAME/Uninstall.command"
sed -e "s|^PRODUCT=.*|PRODUCT=\"$PRODUCT_NAME\"|" -e "s|^ID_PREFIX=.*|ID_PREFIX=\"$ID_PREFIX\"|" \
    packaging/macos/Uninstall.command > "$uninstaller"
chmod 755 "$uninstaller"
grep -qxF "PRODUCT=\"$PRODUCT_NAME\"" "$uninstaller" && grep -qxF "ID_PREFIX=\"$ID_PREFIX\"" "$uninstaller" || {
    echo "package_macos.sh: could not set the edition in Uninstall.command" >&2; exit 1
}
cp -R "$STAGE/Resources" "$(payload content "/Library/Application Support/Luthier")/"

sign "$work/payload/au/Library/Audio/Plug-Ins/Components/$PRODUCT_NAME.component" packaging/macos/plugin-entitlements.plist
sign "$work/payload/vst3/Library/Audio/Plug-Ins/VST3/$PRODUCT_NAME.vst3"          packaging/macos/plugin-entitlements.plist
[ $HAVE_CLAP = 1 ] && sign "$work/payload/clap/Library/Audio/Plug-Ins/CLAP/$PRODUCT_NAME.clap" packaging/macos/plugin-entitlements.plist
sign "$work/payload/app/Applications/$PRODUCT_NAME.app" packaging/macos/entitlements.plist

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
    clap_choice="<choice id=\"clap\" title=\"CLAP plug-in\" description=\"$PRODUCT_NAME.clap in /Library/Audio/Plug-Ins/CLAP\"><pkg-ref id=\"$ID_PREFIX.clap\"/></choice>"
    clap_ref="<pkg-ref id=\"$ID_PREFIX.clap\" version=\"$VERSION\" onConclusion=\"none\">clap.pkg</pkg-ref>"
fi
mkdir -p "$work/resources"
cp packaging/common/EULA.txt "$work/resources/License.txt"
cat > "$work/resources/Welcome.txt" <<EOF
$PRODUCT_NAME $VERSION

This installs the $PRODUCT_NAME instrument as an Audio Unit, VST3 and CLAP plug-in
and as a standalone application, together with its factory content (body and
cabinet impulse responses, guitars, parts, presets and tunes).

Your own presets, guitars and tunes live in ~/Documents/Luthier and are never
touched by the installer or the uninstaller.
EOF
# installer.md 2.1 (IN-19): productbuild cannot run a post-install button, so the
# last page says what to do next and where the uninstaller is.
cat > "$work/resources/Conclusion.txt" <<EOF
$PRODUCT_NAME $VERSION is installed.

Next: open $PRODUCT_NAME from /Applications (standalone), or rescan plug-ins in your
DAW (Logic: Settings > Plug-in Manager > Reset & Rescan Selection).

If your DAW does not list $PRODUCT_NAME, see docs/TROUBLESHOOTING.md.

To uninstall, run /Applications/$PRODUCT_NAME/Uninstall.command. Your own presets,
guitars and tunes in ~/Documents/Luthier are kept.
EOF
cat > "$work/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$PRODUCT_NAME $VERSION</title>
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
    <choice id="au" title="Audio Unit plug-in" description="$PRODUCT_NAME.component in /Library/Audio/Plug-Ins/Components (Logic, GarageBand, ...)"><pkg-ref id="$ID_PREFIX.au"/></choice>
    <choice id="vst3" title="VST3 plug-in" description="$PRODUCT_NAME.vst3 in /Library/Audio/Plug-Ins/VST3"><pkg-ref id="$ID_PREFIX.vst3"/></choice>
    $clap_choice
    <choice id="app" title="Standalone application" description="$PRODUCT_NAME.app in /Applications, and the uninstaller"><pkg-ref id="$ID_PREFIX.app"/></choice>
    <choice id="content" title="Factory content" description="Impulse responses, guitars, parts, presets and tunes (needed by every format)" enabled="false" selected="true"><pkg-ref id="$ID_PREFIX.content"/></choice>
    <pkg-ref id="$ID_PREFIX.au" version="$VERSION" onConclusion="none">au.pkg</pkg-ref>
    <pkg-ref id="$ID_PREFIX.vst3" version="$VERSION" onConclusion="none">vst3.pkg</pkg-ref>
    $clap_ref
    <pkg-ref id="$ID_PREFIX.app" version="$VERSION" onConclusion="none">app.pkg</pkg-ref>
    <pkg-ref id="$ID_PREFIX.content" version="$VERSION" onConclusion="none">content.pkg</pkg-ref>
</installer-gui-script>
EOF

pkg_name="Luthier-$EDITION_SLUG-$VERSION.pkg"
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
$PRODUCT_NAME $VERSION for macOS (Apple Silicon and Intel)

1. Double-click $pkg_name and follow the installer.
2. Open your DAW and rescan plug-ins, or open $PRODUCT_NAME from Applications.

To uninstall, run /Applications/$PRODUCT_NAME/Uninstall.command.
EOF
dmg="$OUT/Luthier-$EDITION_SLUG-$VERSION-macOS.dmg"
rm -f "$dmg"
hdiutil create -volname "$PRODUCT_NAME $VERSION" -srcfolder "$dmg_root" -fs HFS+ -format UDZO -ov "$dmg"
if [ "$APP_ID" != "-" ]; then
    codesign --force --timestamp --sign "$APP_ID" "$dmg"
    notarise "$dmg"
fi
echo "Wrote $dmg"
