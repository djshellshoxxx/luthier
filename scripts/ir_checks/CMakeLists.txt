cmake_minimum_required(VERSION 3.22)
project(LuthierIrChecks LANGUAGES CXX)
# Build the real prepared convolution and native tests without GUI/audio-device dependencies.
# This focused Linux target supplements, and does not replace, native product CI.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MODULES "${ROOT}/ThirdParty/JUCE/modules")
set(SANITIZE "" CACHE STRING "Optional address,undefined or thread sanitizer")
add_executable(ir_checks ../recorder_checks/main.cpp
    "${ROOT}/Source/DSP/Common/LiveConvolution.cpp"
    "${ROOT}/Source/DSP/Common/IrVariants.cpp"
    "${ROOT}/Source/Tests/LiveConvolutionTests.cpp"
    "${ROOT}/Source/Tests/IrSwapContinuityTests.cpp"
    "${ROOT}/Source/DSP/Body/BodyEngine.cpp"
    "${ROOT}/Source/Model/Guitar/BodyModels.cpp"
    "${ROOT}/Source/DSP/Amp/CabinetEngine.cpp"
    "${ROOT}/Source/DSP/Amp/MicPlacement.cpp"
    "${ROOT}/Source/ToneMatch/ToneMatch.cpp")
foreach(module juce_core juce_events juce_data_structures juce_audio_basics juce_audio_formats juce_dsp)
    target_sources(ir_checks PRIVATE "${MODULES}/${module}/${module}.cpp")
endforeach()
target_sources(ir_checks PRIVATE "${MODULES}/juce_core/juce_core_CompilationTime.cpp")
target_include_directories(ir_checks PRIVATE "${MODULES}" "${ROOT}/Source")
target_compile_definitions(ir_checks PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0 JUCE_USE_MP3AUDIOFORMAT=0 JUCE_ALSA=0 JUCE_JACK=0)
target_compile_options(ir_checks PRIVATE -ffunction-sections -fdata-sections)
target_link_options(ir_checks PRIVATE -Wl,--gc-sections)
target_link_libraries(ir_checks PRIVATE pthread dl rt)
if(SANITIZE)
    target_compile_options(ir_checks PRIVATE "-fsanitize=${SANITIZE}" -fno-omit-frame-pointer)
    target_link_options(ir_checks PRIVATE "-fsanitize=${SANITIZE}")
endif()
