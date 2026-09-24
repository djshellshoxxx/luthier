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

    cmake "${args[@]}"
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
    cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$JOBS" --target "${targets[@]}"

    if [ "$PLATFORM" = macos ]; then
        # Resources are copied into each bundle after JUCE has signed it, which
        # breaks the seal. An ad-hoc signature keeps the bundles loadable on
        # Apple Silicon for the validators; release packaging re-signs them
        # with the Developer ID (scripts/package_macos.sh).
        for b in "$ARTEFACTS/VST3/Luthier.vst3" "$ARTEFACTS/AU/Luthier.component" \
                 "$ARTEFACTS/CLAP/Luthier.clap" "$ARTEFACTS/Standalone/Luthier.app"; do
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
    "${wrap[@]}" "$runner" 2>&1 | tee "$LOG_DIR/unit-tests.log"
    local rc=${PIPESTATUS[0]}
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

    local plugins=("$ARTEFACTS/VST3/Luthier.vst3")
    [ "$PLATFORM" = macos ] && plugins+=("$ARTEFACTS/AU/Luthier.component")

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
            auval -strict -v aumu Lthr Ltha 2>&1 | tee "$LOG_DIR/auval.log" || failed=1
        fi
        set +e
        "${wrap[@]}" "$pluginval" --strictness-level "$PLUGINVAL_STRICTNESS" \
            --validate-in-process --timeout-ms 600000 \
            --output-dir "$LOG_DIR" --validate "$p" 2>&1 | tee "$LOG_DIR/pluginval-$name.log"
        [ "${PIPESTATUS[0]}" -eq 0 ] || failed=1
        set -e
    done

    local clap="$ARTEFACTS/CLAP/Luthier.clap"
    if [ -e "$clap" ]; then
        local validator; validator="$(fetch_clap_validator)"
        step "clap-validator: $clap"
        set +e
        "${wrap[@]}" "$validator" validate "$clap" 2>&1 | tee "$LOG_DIR/clap-validator.log"
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
    for f in VST3/Luthier.vst3 CLAP/Luthier.clap AU/Luthier.component; do
        [ -e "$ARTEFACTS/$f" ] && cp -R "$ARTEFACTS/$f" "$out/"
    done
    if [ "$PLATFORM" = macos ]; then
        cp -R "$ARTEFACTS/Standalone/Luthier.app" "$out/"
    else
        cp "$ARTEFACTS/Standalone/Luthier" "$out/luthier"
    fi
    local render="$BUILD_DIR/LuthierRender_artefacts/$CONFIG/luthier-render"
    [ -d "$render.app" ] && render="$render.app/Contents/MacOS/luthier-render"
    [ -e "$render" ] && cp "$render" "$out/luthier-render"

    # One copy of the factory content, shared by every format; the installers
    # put it where IrLibrary::searchForResources looks.
    cp -R Resources "$out/Resources"
    rm -f "$out"/Resources/icon*.png "$out"/Resources/*.ico

    # Strip the per-bundle copies the build made, keeping JUCE's moduleinfo.json:
    # the installed plugins read the shared copy.
    for b in "$out"/Luthier.vst3 "$out"/Luthier.component "$out"/Luthier.clap "$out"/Luthier.app; do
        [ -d "$b/Contents/Resources" ] || continue
        for d in BodyIRs CabIRs Fonts Guitars Parts Presets Tunes; do
            rm -rf "$b/Contents/Resources/$d"
        done
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
