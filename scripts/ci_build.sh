#!/bin/bash
# Configure, build, test, validate and stage Luthier on Linux or macOS.
#
# The GitHub Actions workflows (.github/workflows/) call this script step by
# step, and it runs the same way on a developer machine, so a red CI job can be
# reproduced locally with the same command.
#
# Usage: scripts/ci_build.sh [step ...]
#   deps       fetch JUCE and clap-juce-extensions into ThirdParty/ (pinned)
#   configure  CMake configure into $BUILD_DIR (Ninja, compiler cache if present)
#   build      build every plugin format, LuthierTests and LuthierRender
#   test       run the unit tests (under xvfb on a headless Linux box)
#   validate   pluginval on the VST3 (and AU on macOS), clap-validator on the CLAP
#   stage      copy the built products into $DIST_DIR/<platform>/ for packaging
#   all        every step above, in order (the default)
#
# Environment (all optional):
#   BUILD_DIR            build tree                       (default: build-ci)
#   DIST_DIR             staged products                  (default: dist)
#   LOG_DIR              test and validator logs          (default: $BUILD_DIR/logs)
#   CONFIG               CMake build type                 (default: Release)
#   LUTHIER_LTO          ON / OFF link-time optimisation  (default: OFF)
#   JOBS                 parallel jobs                    (default: CPU count)
#   PLUGINVAL_STRICTNESS 1..10                            (default: 5)
#   PLUGINVAL_VERSION    pluginval release tag            (default: v1.0.4)
#   CLAP_VALIDATOR_VERSION                                (default: 0.3.2)
#   MACOS_ARCHS          CMAKE_OSX_ARCHITECTURES          (default: arm64;x86_64)
#   SKIP_VALIDATORS      1 = skip the validate step (no network, for instance)
set -euo pipefail

cd "$(dirname "$0")/.."

JUCE_TAG="8.0.10"
# clap-juce-extensions has no release tags; pin the commit this build was
# verified against so a new upstream commit cannot break CI without a change here.
CLAP_JUCE_EXT_COMMIT="55525c9858d4b25687be7759a5e0f70eccef218e"

BUILD_DIR="${BUILD_DIR:-build-ci}"
DIST_DIR="${DIST_DIR:-dist}"
LOG_DIR="${LOG_DIR:-$BUILD_DIR/logs}"
CONFIG="${CONFIG:-Release}"
LUTHIER_LTO="${LUTHIER_LTO:-OFF}"
LUTHIER_EDITION="${LUTHIER_EDITION:-PAID}"
# Keep in step with cmake/Editions.cmake (product name, plugin code). The
# packaging scripts (package_linux.sh, package_macos.sh, package_windows.ps1)
# derive the same product name from LUTHIER_EDITION to find what stage wrote.
case "$LUTHIER_EDITION" in
    PAID) PRODUCT_NAME="Luthier Pro";  PLUGIN_CODE=Lthr ;;
    FREE) PRODUCT_NAME="Luthier Free"; PLUGIN_CODE=Lthf ;;
    *) echo "ci_build.sh: LUTHIER_EDITION must be PAID or FREE" >&2; exit 1 ;;
esac
PLUGINVAL_STRICTNESS="${PLUGINVAL_STRICTNESS:-5}"
PLUGINVAL_VERSION="${PLUGINVAL_VERSION:-v1.0.4}"
CLAP_VALIDATOR_VERSION="${CLAP_VALIDATOR_VERSION:-0.3.2}"
MACOS_ARCHS="${MACOS_ARCHS:-arm64;x86_64}"
TOOLS_DIR="${TOOLS_DIR:-$BUILD_DIR/tools}"

case "$(uname -s)" in
    Darwin) PLATFORM=macos ;;
    Linux)  PLATFORM=linux ;;
    *) echo "ci_build.sh: unsupported platform $(uname -s); use scripts/ci_build.ps1 on Windows" >&2; exit 1 ;;
esac

if [ -z "${JOBS:-}" ]; then
    if [ "$PLATFORM" = macos ]; then JOBS="$(sysctl -n hw.ncpu)"; else JOBS="$(nproc)"; fi
fi

ARTEFACTS="$BUILD_DIR/Luthier_artefacts/$CONFIG"

step() { printf '\n==> %s\n' "$*"; }

#------------------------------------------------------------------------------
do_deps() {
    step "Fetching JUCE $JUCE_TAG and clap-juce-extensions"
    mkdir -p ThirdParty
    if [ ! -f ThirdParty/JUCE/CMakeLists.txt ]; then
        git clone --depth 1 --branch "$JUCE_TAG" https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
    fi
    if [ ! -f ThirdParty/clap-juce-extensions/CMakeLists.txt ]; then
        git clone https://github.com/free-audio/clap-juce-extensions.git ThirdParty/clap-juce-extensions
        git -C ThirdParty/clap-juce-extensions checkout -q "$CLAP_JUCE_EXT_COMMIT"
        git -C ThirdParty/clap-juce-extensions submodule update --init --recursive --depth 1
    fi
}

do_configure() {
    step "Configuring $BUILD_DIR ($CONFIG, LTO $LUTHIER_LTO)"
    local args=(-S . -B "$BUILD_DIR" -G Ninja
                -DCMAKE_BUILD_TYPE="$CONFIG"
                -DLUTHIER_LTO="$LUTHIER_LTO"
                -DLUTHIER_EDITION="$LUTHIER_EDITION"
                -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

    if command -v ccache >/dev/null; then
        args+=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
    fi

    if [ "$PLATFORM" = macos ]; then
        # One universal binary per format (installer.md 2.2).
        args+=("-DCMAKE_OSX_ARCHITECTURES=$MACOS_ARCHS" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13)
    else
        # clang is what the Linux setup uses; fall back to the default compiler.
        if command -v clang++ >/dev/null; then
            args+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)
        fi
    fi

    mkdir -p "$LOG_DIR"
    cmake "${args[@]}" 2>&1 | tee "$LOG_DIR/configure.log"
}

build_targets() {
    local targets=(Luthier_VST3 Luthier_Standalone LuthierTests LuthierRender)
    [ "$PLATFORM" = macos ] && targets+=(Luthier_AU)
    [ -d ThirdParty/clap-juce-extensions ] && targets+=(Luthier_CLAP)
    echo "${targets[@]}"
}

do_build() {
    local targets
    read -r -a targets <<< "$(build_targets)"
    step "Building ${targets[*]} with $JOBS jobs"
    # -k 0: keep going after a failed compile so one CI run surfaces every
    # compiler error, not just the first. A successful build is unaffected.
    mkdir -p "$LOG_DIR"
    cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$JOBS" --target "${targets[@]}" -- -k 0 \
        2>&1 | tee "$LOG_DIR/build.log"

    if [ "$PLATFORM" = macos ]; then
        # Resources are copied into each bundle after JUCE has signed it, which
        # breaks the seal. An ad-hoc signature keeps the bundles loadable on
        # Apple Silicon for the validators; release packaging re-signs them
        # with the Developer ID (scripts/package_macos.sh).
        for b in "$ARTEFACTS/VST3/${PRODUCT_NAME}.vst3" "$ARTEFACTS/AU/${PRODUCT_NAME}.component" \
                 "$ARTEFACTS/CLAP/${PRODUCT_NAME}.clap" "$ARTEFACTS/Standalone/${PRODUCT_NAME}.app"; do
            [ -e "$b" ] && codesign --force --deep --sign - "$b"
        done
    fi
}

do_test() {
    mkdir -p "$LOG_DIR"
    local runner="$BUILD_DIR/LuthierTests_artefacts/$CONFIG/LuthierTests"
    [ "$PLATFORM" = macos ] && [ -d "$runner.app" ] && runner="$runner.app/Contents/MacOS/LuthierTests"
    step "Running the unit tests ($runner)"

    local wrap=()
    if [ "$PLATFORM" = linux ] && [ -z "${DISPLAY:-}" ]; then
        wrap=(xvfb-run -a -s "-screen 0 1920x1080x24")
    fi

    # The runner exits non-zero on any failure; tee keeps the log either way.
    set +e
    ${wrap[@]+"${wrap[@]}"} "$runner" 2>&1 | tee "$LOG_DIR/unit-tests.log"
    local rc=${PIPESTATUS[0]}

    # Failure recap: re-print every [FAIL] row and its indented detail lines,
    # plus the summary, at the very end of the step. The per-test rows print
    # thousands of lines up, above the tail a truncated log viewer shows; this
    # keeps the failing names visible at the bottom so a CI failure can be read
    # without downloading the whole log.
    if [ "$rc" -ne 0 ] && [ -f "$LOG_DIR/unit-tests.log" ]; then
        local recap
        recap="$(grep -nE '\[FAIL\]|^ *line [0-9]+:|[0-9]+ of [0-9]+ tests failed' "$LOG_DIR/unit-tests.log" || true)"
        echo "===== UNIT TEST FAILURE RECAP ====="
        printf '%s\n' "$recap"
        echo "===== END FAILURE RECAP ====="

        # Also surface the recap as a GitHub Actions error annotation. The
        # per-test rows and this recap sit thousands of lines above the end of
        # the job (the validators below run even on a test failure, by design),
        # and the full log is not always downloadable (restricted egress blocks
        # the log/artifact blob host). Annotations are readable from the
        # check-runs API regardless, so the failing names are never lost.
        if [ -n "${GITHUB_ACTIONS:-}" ] && [ -n "$recap" ]; then
            local enc="$recap"
            enc="${enc//'%'/%25}"
            enc="${enc//$'\r'/%0D}"
            enc="${enc//$'\n'/%0A}"
            echo "::error title=Unit tests failed (${LUTHIER_EDITION:-?}/${PLATFORM})::${enc}"
        fi
    fi

    # SPEC-SWEEP (TROUBLESHOOTING TS-1): the documented install paths match the installers.
    cmake -P scripts/check_packaging_paths.cmake || rc=1

    # SPEC-SWEEP (README RM-17): luthier-render's documented flags, end to end.
    local render="$BUILD_DIR/LuthierRender_artefacts/$CONFIG/LuthierRender"
    if [ -x "$render" ]; then
        step "Smoke-testing luthier-render"
        cmake -DRENDER="$render" -DMIDI="$PWD/Tools/testdata/two_bars.mid" \
              -DOUT_DIR="$BUILD_DIR/render-cli-smoke" -P scripts/render_cli_smoke.cmake \
              2>&1 | tee "$LOG_DIR/render-cli.log"
        [ "${PIPESTATUS[0]}" -eq 0 ] || rc=1
    fi
    set -e
    return "$rc"
}

fetch_pluginval() {
    local exe
    case "$PLATFORM" in
        linux) exe="$TOOLS_DIR/pluginval" ;;
        macos) exe="$TOOLS_DIR/pluginval.app/Contents/MacOS/pluginval" ;;
    esac
    if [ ! -x "$exe" ]; then
        local zip; [ "$PLATFORM" = linux ] && zip=pluginval_Linux.zip || zip=pluginval_macOS.zip
        mkdir -p "$TOOLS_DIR"
        curl -fsSL -o "$TOOLS_DIR/$zip" \
             "https://github.com/Tracktion/pluginval/releases/download/$PLUGINVAL_VERSION/$zip"
        unzip -oq "$TOOLS_DIR/$zip" -d "$TOOLS_DIR"
        chmod +x "$exe"
    fi
    echo "$exe"
}

fetch_clap_validator() {
    local exe="$TOOLS_DIR/clap-validator"
    if [ ! -x "$exe" ]; then
        local tgz; [ "$PLATFORM" = linux ] && tgz="clap-validator-$CLAP_VALIDATOR_VERSION-ubuntu-18.04.tar.gz" \
                                          || tgz="clap-validator-$CLAP_VALIDATOR_VERSION-macos-universal.tar.gz"
        mkdir -p "$TOOLS_DIR"
        curl -fsSL -o "$TOOLS_DIR/$tgz" \
             "https://github.com/free-audio/clap-validator/releases/download/$CLAP_VALIDATOR_VERSION/$tgz"
        tar xzf "$TOOLS_DIR/$tgz" -C "$TOOLS_DIR"
        # The macOS tarball keeps the program in binaries/; the Linux one does not.
        [ -f "$TOOLS_DIR/binaries/clap-validator" ] && mv -f "$TOOLS_DIR/binaries/clap-validator" "$exe"
        chmod +x "$exe"
    fi
    echo "$exe"
}

do_validate() {
    if [ "${SKIP_VALIDATORS:-0}" = 1 ]; then
        step "Skipping validators (SKIP_VALIDATORS=1)"
        return 0
    fi
    mkdir -p "$LOG_DIR"
    local failed=0 pluginval
    pluginval="$(fetch_pluginval)"

    local wrap=()
    if [ "$PLATFORM" = linux ] && [ -z "${DISPLAY:-}" ]; then
        wrap=(xvfb-run -a -s "-screen 0 1920x1080x24")
    fi

    local plugins=("$ARTEFACTS/VST3/${PRODUCT_NAME}.vst3")
    [ "$PLATFORM" = macos ] && plugins+=("$ARTEFACTS/AU/${PRODUCT_NAME}.component")

    for p in "${plugins[@]}"; do
        local name; name="$(basename "$p")"
        step "pluginval strictness $PLUGINVAL_STRICTNESS: $name"
        if [ "$PLATFORM" = macos ] && [ "${name##*.}" = component ]; then
            # auval (and so pluginval's AU scan) only sees components installed
            # where the system looks for them.
            mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
            rm -rf "$HOME/Library/Audio/Plug-Ins/Components/$name"
            cp -R "$p" "$HOME/Library/Audio/Plug-Ins/Components/"
            p="$HOME/Library/Audio/Plug-Ins/Components/$name"
            killall -9 AudioComponentRegistrar 2>/dev/null || true
            auval -a > "$LOG_DIR/auval-list.log" 2>&1 || true
            # type, subtype (the edition's plugin code), manufacturer.
            auval -strict -v aumu "$PLUGIN_CODE" Ltha 2>&1 | tee "$LOG_DIR/auval.log" || failed=1
        fi
        set +e
        ${wrap[@]+"${wrap[@]}"} "$pluginval" --strictness-level "$PLUGINVAL_STRICTNESS" \
            --validate-in-process --timeout-ms 600000 \
            --output-dir "$LOG_DIR" --validate "$p" 2>&1 | tee "$LOG_DIR/pluginval-$name.log"
        [ "${PIPESTATUS[0]}" -eq 0 ] || failed=1
        set -e
    done

    local clap="$ARTEFACTS/CLAP/${PRODUCT_NAME}.clap"
    if [ -e "$clap" ]; then
        local validator; validator="$(fetch_clap_validator)"
        step "clap-validator: $clap"
        set +e
        # Quieten clap-validator's per-call DEBUG trace (thousands of
        # "TODO: Handle request_flush()" lines) so it does not bury the rest of
        # the job log; override with RUST_LOG=debug when a validator hang needs
        # tracing.
        RUST_LOG="${RUST_LOG:-error}" ${wrap[@]+"${wrap[@]}"} "$validator" validate "$clap" 2>&1 | tee "$LOG_DIR/clap-validator.log"
        [ "${PIPESTATUS[0]}" -eq 0 ] || failed=1
        set -e
    fi

    return "$failed"
}

do_stage() {
    local out="$DIST_DIR/$PLATFORM"
    step "Staging products into $out"
    rm -rf "$out"
    mkdir -p "$out"
    # PRODUCT_NAME has a space ("Luthier Pro"): every expansion below is quoted,
    # or the word-split names match nothing and the bundles are silently skipped.
    [ -e "$ARTEFACTS/VST3/${PRODUCT_NAME}.vst3" ] || {
        echo "ci_build.sh stage: $ARTEFACTS/VST3/${PRODUCT_NAME}.vst3 is missing (LUTHIER_EDITION=$LUTHIER_EDITION); build it first" >&2
        return 1
    }
    for f in "VST3/${PRODUCT_NAME}.vst3" "CLAP/${PRODUCT_NAME}.clap" "AU/${PRODUCT_NAME}.component"; do
        [ -e "$ARTEFACTS/$f" ] && cp -R "$ARTEFACTS/$f" "$out/"
    done
    if [ "$PLATFORM" = macos ]; then
        cp -R "$ARTEFACTS/Standalone/${PRODUCT_NAME}.app" "$out/"
    else
        cp "$ARTEFACTS/Standalone/$PRODUCT_NAME" "$out/luthier"
    fi
    # The console app's file is named after its target (LuthierRender); it
    # ships as luthier-render, the name its --help and the docs use.
    local render="$BUILD_DIR/LuthierRender_artefacts/$CONFIG/LuthierRender"
    [ -e "$render" ] && cp "$render" "$out/luthier-render"

    # One copy of the factory content, shared by every format; the installers
    # put it where IrLibrary::searchForResources looks.
    cp -R Resources "$out/Resources"
    rm -f "$out"/Resources/icon*.png "$out"/Resources/*.ico

    # Strip the per-bundle copies the build made, keeping JUCE's moduleinfo.json:
    # the installed plugins read the shared copy.
    for b in "$out/${PRODUCT_NAME}.vst3" "$out/${PRODUCT_NAME}.component" "$out/${PRODUCT_NAME}.clap" "$out/${PRODUCT_NAME}.app"; do
        [ -d "$b/Contents/Resources" ] || continue
        for d in BodyIRs CabIRs Examples Fonts Guitars Parts Practice Presets Tunes; do
            rm -rf "$b/Contents/Resources/$d"
        done
        # Our icons came along with the content copy; a bundle's own icon
        # (macOS) is the .icns JUCE generates, which is kept.
        rm -f "$b/Contents/Resources/icon.png" "$b/Contents/Resources/icon_small.png" \
              "$b/Contents/Resources/luthier.ico"
        # Removing files from a signed bundle breaks its seal: re-sign ad hoc.
        # scripts/package_macos.sh signs again with the Developer ID.
        [ "$PLATFORM" = macos ] && codesign --force --deep --sign - "$b"
    done
    echo "$CONFIG" > "$out/BUILD_CONFIG"
}

#------------------------------------------------------------------------------
steps=("$@")
[ ${#steps[@]} -eq 0 ] && steps=(all)

for s in "${steps[@]}"; do
    case "$s" in
        deps)      do_deps ;;
        configure) do_configure ;;
        build)     do_build ;;
        test)      do_test ;;
        validate)  do_validate ;;
        stage)     do_stage ;;
        all)       do_deps; do_configure; do_build; do_test; do_validate; do_stage ;;
        *) echo "ci_build.sh: unknown step '$s'" >&2; exit 2 ;;
    esac
done
