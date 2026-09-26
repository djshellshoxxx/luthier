cmake_minimum_required(VERSION 3.22)

project(Luthier VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/MP1 /bigobj /wd4244 /wd4267 /wd4305 /wd4996)
    # Keep per-TU memory low: this machine has 4 GB RAM.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(LUTHIER_BUILD_TESTS "Build the Luthier unit-test runner" ON)
option(LUTHIER_BUILD_CLI   "Build the offline batch-render CLI"  ON)
# Link-time optimisation for the plugin targets. On for shipped builds; CI turns
# it off on ordinary pushes, where it doubles the link time and defeats the
# compiler cache, and keeps it on for nightly and release builds.
option(LUTHIER_LTO         "Link-time optimise the plugin targets" ON)

add_subdirectory(ThirdParty/JUCE)

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

set(LUTHIER_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND LUTHIER_FORMATS AU)
endif()

juce_add_plugin(Luthier
    COMPANY_NAME                "Luthier Audio"
    BUNDLE_ID                   "com.luthieraudio.luthier"
    PLUGIN_MANUFACTURER_CODE    Ltha
    PLUGIN_CODE                 Lthr
    FORMATS                     ${LUTHIER_FORMATS}
    PRODUCT_NAME                "Luthier"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
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

# Work in progress by the assistant agents (spec/DECISIONS.md): written without
# a compiler, so kept out of the build until the lead integrates it. Remove a
# line here when its feature is integrated.
set(LUTHIER_WIP_REGEX "Source/WIP/")
list(FILTER LUTHIER_SOURCES EXCLUDE REGEX "${LUTHIER_WIP_REGEX}")

target_sources(Luthier PRIVATE ${LUTHIER_SOURCES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" FILES ${LUTHIER_SOURCES})

target_compile_definitions(Luthier PUBLIC
    LUTHIER_HEADLESS=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1)

target_include_directories(Luthier PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_link_libraries(Luthier PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    # SHA-256, for the license activation proof (updates-telemetry 5).
    juce::juce_cryptography
    PUBLIC
    juce::juce_recommended_config_flags)

if (LUTHIER_LTO)
    target_link_libraries(Luthier PUBLIC juce::juce_recommended_lto_flags)
endif()

if (LUTHIER_HAS_CLAP)
    clap_juce_extensions_plugin(TARGET Luthier
        CLAP_ID "com.luthieraudio.luthier"
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
list(FILTER LUTHIER_ENGINE_SOURCES EXCLUDE REGEX "PluginEditor[A-Za-z]*\.(cpp|h)$")   # SPEC-SWEEP: + PluginEditorTune/Onboarding

# The UI, as its own list. The renderer does not want it - it renders offline and
# has no window - but the test runner does. With Source/UI out of every target
# that runs, nothing in this repository ever constructed a
# LuthierAudioProcessorEditor, and the panels could be verified only by opening
# the plugin in a host by hand.
set(LUTHIER_UI_SOURCES ${LUTHIER_SOURCES})
list(FILTER LUTHIER_UI_SOURCES INCLUDE REGEX "Source/UI/|PluginEditor[A-Za-z]*\.(cpp|h)$")
list(FILTER LUTHIER_UI_SOURCES EXCLUDE REGEX "Source/UI/UiPreferences\.cpp$")

set(LUTHIER_CONSOLE_DEFS
    LUTHIER_HEADLESS=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Luthier"
    JucePlugin_VersionString="${PROJECT_VERSION}")

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

    juce_add_console_app(LuthierTests PRODUCT_NAME "LuthierTests")

    target_sources(LuthierTests PRIVATE ${LUTHIER_ENGINE_SOURCES}
                                        ${LUTHIER_UI_SOURCES}
                                        ${LUTHIER_TEST_SOURCES})
    target_include_directories(LuthierTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_compile_definitions(LuthierTests PRIVATE ${LUTHIER_TEST_DEFS})

    target_link_libraries(LuthierTests PRIVATE
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
                   "${CMAKE_CURRENT_SOURCE_DIR}/Tools/RenderCli.cpp")

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
