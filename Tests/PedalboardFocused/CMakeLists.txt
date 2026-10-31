cmake_minimum_required(VERSION 3.22)
project(LuthierPedalboardFocused LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(MODULES "${ROOT}/ThirdParty/JUCE/modules")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${ROOT}/scripts/pedalboard_focused_main.py" "${CMAKE_CURRENT_BINARY_DIR}/main.cpp" RESULT_VARIABLE generated)
if(NOT generated EQUAL 0)
  message(FATAL_ERROR "Cannot extract original allocation instrumentation")
endif()
file(GLOB_RECURSE REFERENCE_SOURCES CONFIGURE_DEPENDS "${ROOT}/Source/ReferencePedals/*.cpp")
add_executable(PedalboardFocused "${CMAKE_CURRENT_BINARY_DIR}/main.cpp" ${REFERENCE_SOURCES}
  "${ROOT}/Source/Tests/TestFramework.cpp"
  "${ROOT}/Source/Tests/RefPedalBbdCoreTests.cpp"
  "${ROOT}/Source/Tests/ReferencePedals_boss_ce2Tests.cpp"
  "${ROOT}/Source/Tests/ReferencePedals_boss_ce2wTests.cpp"
  "${ROOT}/Source/Tests/ReferencePedals_boss_dd3tTests.cpp"
  "${ROOT}/Source/Tests/ReferencePedals_mxr_phase90_scriptTests.cpp"
  "${ROOT}/Source/Tests/ReferencePedals_mxr_dynacompTests.cpp"
  "${ROOT}/Source/Tests/ReferencePedals_boss_rc5Tests.cpp"
  "${MODULES}/juce_core/juce_core.cpp"
  "${MODULES}/juce_audio_basics/juce_audio_basics.cpp"
  "${MODULES}/juce_audio_formats/juce_audio_formats.cpp"
  "${MODULES}/juce_dsp/juce_dsp.cpp"
  "${MODULES}/juce_cryptography/juce_cryptography.cpp")
target_include_directories(PedalboardFocused PRIVATE "${ROOT}/Source" "${MODULES}")
target_compile_definitions(PedalboardFocused PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 LUTHIER_ALLOCATION_COUNTER=1 LUTHIER_PRO=1)
if(UNIX AND NOT APPLE)
  target_link_libraries(PedalboardFocused PRIVATE pthread dl rt atomic)
endif()
