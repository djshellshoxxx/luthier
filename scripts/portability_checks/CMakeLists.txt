cmake_minimum_required(VERSION 3.22)
project(LuthierPortabilityChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MODULES "${ROOT}/ThirdParty/JUCE/modules")
set(SANITIZE "" CACHE STRING "Optional address,undefined sanitizer")
option(USE_LIBCXX "Use Clang's libc++ to exercise macOS ownership semantics on Linux" OFF)
option(EXERCISE_FMA "Allow multiply/add contraction on an FMA-capable x86 CPU" OFF)
add_executable(portability_checks ../recorder_checks/main.cpp
    "${ROOT}/Source/ReferencePedals/Services/AssetStore.cpp"
    "${ROOT}/Source/Tests/RefPedalAssetLifetimeTests.cpp"
    "${ROOT}/Source/Tests/RefPedalHostCapturePortabilityTests.cpp"
    "${MODULES}/juce_core/juce_core.cpp"
    "${MODULES}/juce_core/juce_core_CompilationTime.cpp"
    "${MODULES}/juce_cryptography/juce_cryptography.cpp")
target_include_directories(portability_checks PRIVATE "${MODULES}" "${ROOT}/Source")
target_compile_definitions(portability_checks PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(portability_checks PRIVATE pthread dl)
if(USE_LIBCXX)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "USE_LIBCXX requires Clang")
    endif()
    target_compile_options(portability_checks PRIVATE -stdlib=libc++)
    target_link_options(portability_checks PRIVATE -stdlib=libc++)
endif()
if(EXERCISE_FMA)
    target_compile_options(portability_checks PRIVATE -mfma -ffp-contract=fast)
endif()
if(SANITIZE)
    target_compile_options(portability_checks PRIVATE "-fsanitize=${SANITIZE}" -fno-omit-frame-pointer)
    target_link_options(portability_checks PRIVATE "-fsanitize=${SANITIZE}")
endif()
