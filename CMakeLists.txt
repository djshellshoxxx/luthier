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
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
