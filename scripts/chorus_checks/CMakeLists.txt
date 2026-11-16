cmake_minimum_required(VERSION 3.22)
project(LuthierChorusChecks LANGUAGES CXX)
# Build the real chorus cores and native tests without GUI/audio-device dependencies.
# This focused Linux target supplements, and does not replace, native product CI.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MODULES "${ROOT}/ThirdParty/JUCE/modules")
set(SANITIZE "" CACHE STRING "Optional address,undefined or thread sanitizer")
add_executable(chorus_checks ../recorder_checks/main.cpp
    "${ROOT}/Source/ReferencePedals/Common/BbdCore.cpp"
    "${ROOT}/Source/ReferencePedals/Models/boss_ce2.cpp"
    "${ROOT}/Source/ReferencePedals/Models/boss_ce2w.cpp"
    "${ROOT}/Source/Tests/ReferencePedals_boss_ce2Tests.cpp"
    "${ROOT}/Source/Tests/RefPedalBbdCoreTests.cpp"
    "${ROOT}/Source/Tests/RefPedalBufferedTimeFxTests.cpp"
    "${ROOT}/Source/Tests/RefPedalBbdCurvatureTests.cpp")
foreach(module juce_core juce_events juce_data_structures juce_audio_basics juce_audio_formats juce_dsp)
    target_sources(chorus_checks PRIVATE "${MODULES}/${module}/${module}.cpp")
endforeach()
target_sources(chorus_checks PRIVATE "${MODULES}/juce_core/juce_core_CompilationTime.cpp")
target_include_directories(chorus_checks PRIVATE "${MODULES}" "${ROOT}/Source")
target_compile_definitions(chorus_checks PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0 JUCE_USE_MP3AUDIOFORMAT=0 JUCE_ALSA=0 JUCE_JACK=0)
target_compile_options(chorus_checks PRIVATE -ffunction-sections -fdata-sections)
target_link_options(chorus_checks PRIVATE -Wl,--gc-sections)
target_link_libraries(chorus_checks PRIVATE pthread dl rt)
if(SANITIZE)
    target_compile_options(chorus_checks PRIVATE "-fsanitize=${SANITIZE}" -fno-omit-frame-pointer)
    target_link_options(chorus_checks PRIVATE "-fsanitize=${SANITIZE}")
endif()
