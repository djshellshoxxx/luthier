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

add_subdirectory(ThirdParty/JUCE)

juce_add_plugin(Luthier
    COMPANY_NAME                "Luthier Audio"
    BUNDLE_ID                   "com.luthieraudio.luthier"
    PLUGIN_MANUFACTURER_CODE    Ltha
    PLUGIN_CODE                 Lthr
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "Luthier"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Instrument" "Synth"
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    ICON_SMALL                  "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_small.png")

file(GLOB_RECURSE LUTHIER_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")
list(FILTER LUTHIER_SOURCES EXCLUDE REGEX "/Tests/")

target_sources(Luthier PRIVATE ${LUTHIER_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" FILES ${LUTHIER_SOURCES})

target_compile_definitions(Luthier PUBLIC
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
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

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
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${destination}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                    "${LUTHIER_RESOURCES}" "${destination}"
            COMMENT "Copying Luthier resources to ${destination}"
            VERBATIM)
    endif()
endfunction()

luthier_copy_resources(Luthier_VST3
    "$<TARGET_FILE_DIR:Luthier_VST3>/../../Resources")

luthier_copy_resources(Luthier_Standalone
    "$<TARGET_FILE_DIR:Luthier_Standalone>/Resources")

#==============================================================================
# Test runner and offline renderer
#
# Both are console apps built from the engine sources - everything except the
# plugin wrapper and the UI. That keeps them free of the plugin-client macros
# while still exercising the real DSP, model, preset and parameter code.
#==============================================================================
set(LUTHIER_ENGINE_SOURCES ${LUTHIER_SOURCES})
list(FILTER LUTHIER_ENGINE_SOURCES EXCLUDE REGEX "Source/UI/")
list(FILTER LUTHIER_ENGINE_SOURCES EXCLUDE REGEX "PluginProcessor\.(cpp|h)$")
list(FILTER LUTHIER_ENGINE_SOURCES EXCLUDE REGEX "PluginEditor\.(cpp|h)$")

set(LUTHIER_CONSOLE_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Luthier"
    JucePlugin_VersionString="${PROJECT_VERSION}")

if (LUTHIER_BUILD_TESTS)
    file(GLOB LUTHIER_TEST_SOURCES CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tests/*.cpp"
         "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tests/*.h")

    juce_add_console_app(LuthierTests PRODUCT_NAME "LuthierTests")

    target_sources(LuthierTests PRIVATE ${LUTHIER_ENGINE_SOURCES} ${LUTHIER_TEST_SOURCES})
    target_include_directories(LuthierTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_compile_definitions(LuthierTests PRIVATE ${LUTHIER_CONSOLE_DEFS})

    target_link_libraries(LuthierTests PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_cryptography
        PUBLIC
        juce::juce_recommended_config_flags)

    luthier_copy_resources(LuthierTests "$<TARGET_FILE_DIR:LuthierTests>/Resources")

    enable_testing()
    add_test(NAME LuthierUnitTests COMMAND LuthierTests)
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
endif()
