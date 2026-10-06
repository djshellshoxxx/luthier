cmake_minimum_required(VERSION 3.22)

project(Luthier VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/MP1 /bigobj /Zc:lambda /wd4244 /wd4267 /wd4305 /wd4996)
    # Keep per-TU memory low: this machine has 4 GB RAM.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(LUTHIER_BUILD_TESTS "Build the Luthier unit-test runner" ON)
option(LUTHIER_BUILD_CLI   "Build the offline batch-render CLI"  ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Editions.cmake")
# Link-time optimisation for the plugin targets. On for shipped builds; CI turns
# it off on ordinary pushes, where it doubles the link time and defeats the
# compiler cache, and keeps it on for nightly and release builds.
option(LUTHIER_LTO         "Link-time optimise the plugin targets" ON)
# Optional crash reporting via sentry-native. OFF by default: when OFF, nothing
# below fires and Source/Support/CrashReporting.cpp compiles to no-ops (see that
# file and docs/infra/SETTINGS_CHECKLIST.md). Turn on with:
#   -DLUTHIER_ENABLE_SENTRY=ON -DSENTRY_DSN=$SENTRY_DSN
# The DSN is never hardcoded; it is read here from the SENTRY_DSN cache/env var.
option(LUTHIER_ENABLE_SENTRY "Compile in opt-in sentry-native crash reporting" OFF)
if (LUTHIER_ENABLE_SENTRY AND (NOT DEFINED SENTRY_DSN OR SENTRY_DSN STREQUAL ""))
    set(SENTRY_DSN "$ENV{SENTRY_DSN}")
endif()

add_subdirectory(ThirdParty/JUCE)

if (LUTHIER_IS_PRO)
    add_library(luthier_monocypher STATIC
        ThirdParty/monocypher/monocypher.c
        ThirdParty/monocypher/monocypher-ed25519.c)
    target_include_directories(luthier_monocypher PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/monocypher")
    set_target_properties(luthier_monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# CLAP comes from clap-juce-extensions when it has been fetched beside JUCE
# (scripts/setup_linux.sh and the CI workflows fetch it); without it the build
# simply has no CLAP target.
set(LUTHIER_CLAP_EXTENSIONS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/clap-juce-extensions")
if (EXISTS "${LUTHIER_CLAP_EXTENSIONS}/CMakeLists.txt")
    add_subdirectory("${LUTHIER_CLAP_EXTENSIONS}" EXCLUDE_FROM_ALL)
    set(LUTHIER_HAS_CLAP TRUE)
else()
    set(LUTHIER_HAS_CLAP FALSE)
endif()

# host-integration HI-8: LUTHIER_VERSION major.minor.patch plus a build string,
# so a bug report names the exact build rather than just "1.0.0". CI sets
# LUTHIER_BUILD_STRING itself (e.g. a run number); locally it falls back to the
# git short SHA, or "dev" outside a git checkout.
if(NOT DEFINED LUTHIER_BUILD_STRING OR LUTHIER_BUILD_STRING STREQUAL "")
    if(DEFINED ENV{LUTHIER_BUILD_STRING} AND NOT "$ENV{LUTHIER_BUILD_STRING}" STREQUAL "")
        set(LUTHIER_BUILD_STRING "$ENV{LUTHIER_BUILD_STRING}")
    else()
        find_package(Git QUIET)
        set(LUTHIER_BUILD_STRING "dev")
        if(GIT_EXECUTABLE)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse --short=8 HEAD
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE LUTHIER_GIT_SHA
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE LUTHIER_GIT_RESULT)
            if(LUTHIER_GIT_RESULT EQUAL 0 AND NOT LUTHIER_GIT_SHA STREQUAL "")
                set(LUTHIER_BUILD_STRING "${LUTHIER_GIT_SHA}")
            endif()
        endif()
    endif()
endif()

set(LUTHIER_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND LUTHIER_FORMATS AU)
endif()

juce_add_plugin(Luthier
    COMPANY_NAME                "Luthier Audio"
    BUNDLE_ID                   "${LUTHIER_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE    Ltha
    PLUGIN_CODE                 ${LUTHIER_PLUGIN_CODE}
    FORMATS                     ${LUTHIER_FORMATS}
    PRODUCT_NAME                "${LUTHIER_PRODUCT_NAME}"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE    # SPEC-SWEEP PR-13 / HI-2 / HI-31: matches producesMidi(), so VST3/AU declare the event output
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Instrument" "Synth"
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    # macOS notarisation needs the hardened runtime; the sidechain input needs
    # the audio-input entitlement under it and a usage string in the standalone.
    # (scripts/package_macos.sh applies the same entitlements when it re-signs.)
    HARDENED_RUNTIME_ENABLED    TRUE
    HARDENED_RUNTIME_OPTIONS    com.apple.security.device.audio-input
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "Luthier uses audio input for its sidechain."
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    ICON_SMALL                  "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_small.png")

file(GLOB_RECURSE LUTHIER_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")
list(FILTER LUTHIER_SOURCES EXCLUDE REGEX "/Tests/")
if (NOT LUTHIER_IS_PRO)
    list(FILTER LUTHIER_SOURCES EXCLUDE REGEX "/Licence/")
endif()

# Work in progress by the assistant agents (spec/DECISIONS.md): written without
# a compiler, so kept out of the build until the lead integrates it. Remove a
# line here when its feature is integrated.
set(LUTHIER_WIP_REGEX "Source/WIP/")
list(FILTER LUTHIER_SOURCES EXCLUDE REGEX "${LUTHIER_WIP_REGEX}")

target_sources(Luthier PRIVATE ${LUTHIER_SOURCES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" FILES ${LUTHIER_SOURCES})

target_compile_definitions(Luthier PUBLIC
    LUTHIER_PRO=${LUTHIER_IS_PRO}
    LUTHIER_BUILD_STRING="${LUTHIER_BUILD_STRING}"
    LUTHIER_HEADLESS=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
    # installer.md 1.3 / 13.6: Source/StandaloneApp.cpp opens files it is given.
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

target_include_directories(Luthier PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_link_libraries(Luthier PRIVATE
    $<$<BOOL:${LUTHIER_IS_PRO}>:luthier_monocypher>
    juce::juce_audio_utils
    juce::juce_dsp
    # SHA-256, for the license activation proof (updates-telemetry 5).
    juce::juce_cryptography
    PUBLIC
    juce::juce_recommended_config_flags)

if (LUTHIER_LTO)
    target_link_libraries(Luthier PUBLIC juce::juce_recommended_lto_flags)
endif()

# Opt-in crash reporting (scaffold). Only fires when LUTHIER_ENABLE_SENTRY=ON;
# the default build never enters this block, so CrashReporting.cpp stays a no-op.
if (LUTHIER_ENABLE_SENTRY)
    target_compile_definitions(Luthier PRIVATE
        LUTHIER_ENABLE_SENTRY=1
        LUTHIER_SENTRY_DSN="${SENTRY_DSN}")
    # To finish wiring sentry-native, fetch/find it and link, e.g.:
    #   find_package(sentry CONFIG REQUIRED)
    #   target_link_libraries(Luthier PRIVATE sentry::sentry)
    if (SENTRY_DSN STREQUAL "")
        message(WARNING "LUTHIER_ENABLE_SENTRY is ON but SENTRY_DSN is empty; reporting stays disabled at runtime.")
    else()
        message(STATUS "Luthier: sentry crash reporting compiled in (DSN provided via SENTRY_DSN).")
    endif()
endif()

if (LUTHIER_HAS_CLAP)
    clap_juce_extensions_plugin(TARGET Luthier
        CLAP_ID "${LUTHIER_CLAP_ID}"
        CLAP_FEATURES instrument synthesizer stereo)
endif()

#==============================================================================
# Resources
#
# The body and cabinet IRs and the factory preset bank ship beside the binary.
# A VST3 wants them inside its bundle; the standalone wants them next to the exe.
# copy_directory_if_different keeps incremental builds from re-copying 25 MB of
# WAV files every time.
#==============================================================================
set(LUTHIER_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Resources")

function(luthier_copy_resources target destination)
    if (TARGET ${target})
        # Our folders are cleared first: copy_directory_if_different never deletes,
        # and a part or guitar removed from Resources/ must not linger beside its
        # replacement. Only ours - a VST3 bundle's Resources also holds JUCE's
        # moduleinfo.json.
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${destination}/Parts" "${destination}/Guitars"
                    "${destination}/Presets" "${destination}/Tunes" "${destination}/Fonts"
                    "${destination}/Riffs"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${destination}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                    "${LUTHIER_RESOURCES}" "${destination}"
            COMMENT "Copying Luthier resources to ${destination}"
            VERBATIM)
    endif()
endfunction()

# A bundle's resources belong in Contents/Resources, one level up from the
# binary on every platform (Contents/MacOS on macOS, Contents/<arch> for a VST3
# elsewhere); anything written to a macOS bundle root breaks its code signature.
luthier_copy_resources(Luthier_VST3 "$<TARGET_FILE_DIR:Luthier_VST3>/../Resources")

if (APPLE)
    luthier_copy_resources(Luthier_AU   "$<TARGET_FILE_DIR:Luthier_AU>/../Resources")
    luthier_copy_resources(Luthier_CLAP "$<TARGET_FILE_DIR:Luthier_CLAP>/../Resources")
else()
    # A CLAP on Windows and Linux is a single file: its resources go beside it,
    # as they do for the standalone.
    luthier_copy_resources(Luthier_CLAP "$<TARGET_FILE_DIR:Luthier_CLAP>/Resources")
endif()

# The standalone wants them next to the exe, except that on macOS it is an app
# bundle too, and files beside the binary in Contents/MacOS count as unsigned code.
if (APPLE)
    luthier_copy_resources(Luthier_Standalone
        "$<TARGET_FILE_DIR:Luthier_Standalone>/../Resources")
else()
    luthier_copy_resources(Luthier_Standalone
        "$<TARGET_FILE_DIR:Luthier_Standalone>/Resources")
endif()

#==============================================================================
# Test runner and offline renderer
#
# Both are console apps. The renderer is built from the engine sources -
# everything except the UI; the test runner is built from those and the UI too.
#==============================================================================
#
# PluginProcessor is included. It owns the undo stack, uiState, the A/B slots and
# the snapshot and setlist flows, and state-model.md specifies all of them, so
# leaving it out meant those contracts could only be inspected rather than tested.
# The renderer compiles it with LUTHIER_HEADLESS=1, which removes its one
# reference to the editor.
set(LUTHIER_ENGINE_SOURCES ${LUTHIER_SOURCES})
list(FILTER LUTHIER_ENGINE_SOURCES EXCLUDE REGEX "Source/UI/")
# SPEC-SWEEP: UiPreferences (juce_core only) is engine code too - PresetManager reads it (REALISM-C).
list(APPEND LUTHIER_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/UiPreferences.cpp")
list(FILTER LUTHIER_ENGINE_SOURCES EXCLUDE REGEX "PluginEditor[A-Za-z]*\.(cpp|h)$")   # SPEC-SWEEP: + PluginEditorTune; FEAT-NORMALIZE: + PluginEditorOnboarding

# The UI, as its own list. The renderer does not want it - it renders offline and
# has no window - but the test runner does. With Source/UI out of every target
# that runs, nothing in this repository ever constructed a
# LuthierAudioProcessorEditor, and the panels could be verified only by opening
# the plugin in a host by hand.
set(LUTHIER_UI_SOURCES ${LUTHIER_SOURCES})
list(FILTER LUTHIER_UI_SOURCES INCLUDE REGEX "Source/UI/|PluginEditor[A-Za-z]*\.(cpp|h)$")   # FEAT-NORMALIZE: PluginEditorOnboarding.cpp too
list(FILTER LUTHIER_UI_SOURCES EXCLUDE REGEX "Source/UI/UiPreferences\.cpp$")

set(LUTHIER_CONSOLE_DEFS
    LUTHIER_PRO=${LUTHIER_IS_PRO}
    LUTHIER_BUILD_STRING="${LUTHIER_BUILD_STRING}"
    LUTHIER_HEADLESS=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Luthier"
    JucePlugin_VersionString="${PROJECT_VERSION}"
    # host-integration HI-2/HI-31: mirrors the real plugin targets' NEEDS_MIDI_OUTPUT,
    # so PluginBuses::midiOutputIsAnnounced can see the same macro they build with.
    JucePlugin_ProducesMidiOutput=1)

# SPEC-SWEEP PT-24 (practice-tools 3): the TRACK tab offers MP3, so JUCE's own
# decoder (part of juce_audio_formats, no LGPL code) is switched on.
set(LUTHIER_FORMAT_DEFS JUCE_USE_MP3AUDIOFORMAT=1)
list(APPEND LUTHIER_CONSOLE_DEFS ${LUTHIER_FORMAT_DEFS})
target_compile_definitions(Luthier PUBLIC ${LUTHIER_FORMAT_DEFS})

# The test runner is the one console target that is not headless: it builds the
# editor, so LUTHIER_HEADLESS has to be 0 or PluginProcessor compiles out the
# createEditor body these tests exist to call.
set(LUTHIER_TEST_DEFS ${LUTHIER_CONSOLE_DEFS})
list(REMOVE_ITEM LUTHIER_TEST_DEFS LUTHIER_HEADLESS=1)
list(APPEND LUTHIER_TEST_DEFS LUTHIER_HEADLESS=0)
# notation-export 7.1 (MODEL-GAPS): CircuitTests.cpp counts allocations; the other suites' checks compile in.
list(APPEND LUTHIER_TEST_DEFS LUTHIER_ALLOCATION_COUNTER=1)

if (LUTHIER_BUILD_TESTS)
    file(GLOB LUTHIER_TEST_SOURCES CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tests/*.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tests/*.h")
    list(FILTER LUTHIER_TEST_SOURCES EXCLUDE REGEX "${LUTHIER_WIP_REGEX}")
    if (NOT LUTHIER_IS_PRO)
        list(FILTER LUTHIER_TEST_SOURCES EXCLUDE REGEX "Licence.*Tests\\.cpp$|MachineFingerprintTests\\.cpp$")
    endif()

    juce_add_console_app(LuthierTests PRODUCT_NAME "LuthierTests")

    target_sources(LuthierTests PRIVATE ${LUTHIER_ENGINE_SOURCES}
                                        ${LUTHIER_UI_SOURCES}
                                        ${LUTHIER_TEST_SOURCES})
    target_include_directories(LuthierTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_compile_definitions(LuthierTests PRIVATE ${LUTHIER_TEST_DEFS})

    target_link_libraries(LuthierTests PRIVATE
        $<$<BOOL:${LUTHIER_IS_PRO}>:luthier_monocypher>
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_cryptography
        PUBLIC
        juce::juce_recommended_config_flags)

    # Tests hold processors as locals, and a processor is large: three in one
    # frame overflowed the 1 MB default and the runner died without a word.
    if(MSVC)
        target_link_options(LuthierTests PRIVATE "/STACK:8388608")
    endif()

    luthier_copy_resources(LuthierTests "$<TARGET_FILE_DIR:LuthierTests>/Resources")

    enable_testing()
    add_test(NAME LuthierUnitTests COMMAND LuthierTests)
    # SPEC-SWEEP (TROUBLESHOOTING TS-1): the documented install paths match the installers.
    add_test(NAME PackagingPaths COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_packaging_paths.cmake)
endif()

if (LUTHIER_BUILD_CLI)
    juce_add_console_app(LuthierRender PRODUCT_NAME "luthier-render")

    target_sources(LuthierRender PRIVATE
                   ${LUTHIER_ENGINE_SOURCES}
                   "${CMAKE_CURRENT_SOURCE_DIR}/Tools/RenderCli.cpp"
                   # PresetManager reads UiPreferences (plain file I/O, no UI);
                   # FEAT-NORMALIZE restored the renderer's link after that landed.
                   "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/UiPreferences.cpp")

    target_include_directories(LuthierRender PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_compile_definitions(LuthierRender PRIVATE ${LUTHIER_CONSOLE_DEFS})

    target_link_libraries(LuthierRender PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_cryptography
        PUBLIC
        juce::juce_recommended_config_flags)

    luthier_copy_resources(LuthierRender "$<TARGET_FILE_DIR:LuthierRender>/Resources")

    # SPEC-SWEEP (README RM-17): every documented luthier-render flag, end to end.
    enable_testing()
    add_test(NAME LuthierRenderCli
             COMMAND ${CMAKE_COMMAND}
                     -DRENDER=$<TARGET_FILE:LuthierRender>
                     -DMIDI=${CMAKE_CURRENT_SOURCE_DIR}/Tools/testdata/two_bars.mid
                     -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/render-cli-smoke
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/render_cli_smoke.cmake)
endif()

#==============================================================================
# Install and packaging (installer.md 3, 4, 10)
#
# Linux: `cpack` in the build tree makes Luthier-<version>-linux-x64.tar.gz and
# luthier_<version>_amd64.deb. The layout is installer.md 3.1's:
#   VST3        <prefix>/lib/vst3/Luthier.vst3
#   standalone  <prefix>/bin/luthier
#   content     <prefix>/share/luthier        (IrLibrary looks there)
#   desktop     <prefix>/share/applications/luthier.desktop
#   MIME        <prefix>/share/mime/packages/luthier.xml (the six associations)
#   icons       <prefix>/share/icons/hicolor/{128x128,512x512}/apps/luthier.png
# SOURCE_DATE_EPOCH, when set, makes the archives reproducible (installer.md 10).
#==============================================================================
if (UNIX AND NOT APPLE)
    set(LUTHIER_ARTEFACTS "${CMAKE_CURRENT_BINARY_DIR}/Luthier_artefacts/$<CONFIG>")

    # The bundle's own copy of Resources (the build's convenience) is left out:
    # the package ships the content once, in share/luthier, where IrLibrary
    # finds it from the plugin too.
    install(DIRECTORY "${LUTHIER_ARTEFACTS}/VST3/Luthier.vst3"
            DESTINATION lib/vst3 USE_SOURCE_PERMISSIONS COMPONENT main
            REGEX "/Luthier.vst3/Resources(/|$)" EXCLUDE)
    install(PROGRAMS "${LUTHIER_ARTEFACTS}/Standalone/Luthier"
            DESTINATION bin RENAME luthier COMPONENT main)
    install(DIRECTORY "${LUTHIER_RESOURCES}/"
            DESTINATION share/luthier COMPONENT main
            PATTERN "*.ico" EXCLUDE)
    install(FILES packaging/linux/luthier.desktop DESTINATION share/applications COMPONENT main)
    install(FILES packaging/linux/luthier.xml DESTINATION share/mime/packages COMPONENT main)
    install(FILES Resources/icon.png DESTINATION share/icons/hicolor/512x512/apps RENAME luthier.png COMPONENT main)
    install(FILES Resources/icon_small.png DESTINATION share/icons/hicolor/128x128/apps RENAME luthier.png COMPONENT main)
    install(FILES THIRD_PARTY_LICENCES.txt DESTINATION share/doc/luthier COMPONENT main)
    install(PROGRAMS packaging/linux/install.sh packaging/linux/uninstall.sh
            DESTINATION . COMPONENT tarball)

    set(CPACK_PACKAGE_NAME "luthier")
    set(CPACK_PACKAGE_VENDOR "Luthier Audio")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Physically modelled guitar and bass instrument (VST3 and standalone)")
    set(CPACK_PACKAGE_CONTACT "Luthier Audio <support@luthier.audio>")
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/CPackProjectConfig.cmake")

    # One package per generator, whatever the components.
    set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)

    # installer.md 3 names.
    set(CPACK_ARCHIVE_FILE_NAME "Luthier-${PROJECT_VERSION}-linux-x64")
    set(CPACK_PACKAGE_FILE_NAME "Luthier-${PROJECT_VERSION}-linux-x64")
    set(CPACK_DEBIAN_FILE_NAME "luthier_${PROJECT_VERSION}_amd64.deb")
    # The top-level folder (tarball only) is set per generator in CPackProjectConfig.cmake.

    # installer.md 3.3: ALSA always; JACK or PipeWire for low latency; X11
    # (JUCE opens libX11 and friends at run time, so they are named here).
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libasound2 | libasound2t64, libfreetype6, libfontconfig1, libx11-6, libxext6, libxrandr2, libxinerama1, libxcursor1, libatomic1, libstdc++6, libc6")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "pipewire-jack | libjack-jackd2-0 | libjack0")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/postrm")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)

    include(CPack)
endif()
