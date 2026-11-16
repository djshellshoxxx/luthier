cmake_minimum_required(VERSION 3.22)
project(LuthierLooperPublicationChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MODULES "${ROOT}/ThirdParty/JUCE/modules")
set(SANITIZE "" CACHE STRING "Optional thread or address,undefined sanitizer")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
set(STORAGE "${ROOT}/Source/ReferencePedals/Common/LooperStorage.cpp")
set(INSTRUMENTED "${CMAKE_CURRENT_BINARY_DIR}/LooperStorage.cpp")
add_custom_command(OUTPUT "${INSTRUMENTED}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/instrument.py" "${STORAGE}" "${INSTRUMENTED}"
    DEPENDS "${STORAGE}" "${CMAKE_CURRENT_LIST_DIR}/instrument.py" VERBATIM)
add_executable(looper_publication_checks ../recorder_checks/main.cpp "${INSTRUMENTED}"
    "${ROOT}/Source/ReferencePedals/Services/AssetStore.cpp"
    "${ROOT}/Source/ReferencePedals/Common/PagedLooperCore.cpp"
    "${ROOT}/Source/Tests/RefPedalLooperManifestPublicationTests.cpp"
    "${ROOT}/Source/Tests/RefPedalLooperStorageTests.cpp"
    "${ROOT}/Source/Tests/RefPedalLooperStorageDirectionTests.cpp"
    "${ROOT}/Source/Tests/RefPedalLooperStorageWrapTests.cpp"
    "${MODULES}/juce_core/juce_core.cpp"
    "${MODULES}/juce_core/juce_core_CompilationTime.cpp"
    "${MODULES}/juce_cryptography/juce_cryptography.cpp")
target_include_directories(looper_publication_checks PRIVATE "${MODULES}" "${ROOT}/Source"
    "${ROOT}/Source/ReferencePedals/Common")
target_compile_definitions(looper_publication_checks PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 LUTHIER_MANIFEST_PUBLICATION_PROBE=1)
target_link_libraries(looper_publication_checks PRIVATE Threads::Threads "${CMAKE_DL_LIBS}")
if(SANITIZE)
    target_compile_options(looper_publication_checks PRIVATE "-fsanitize=${SANITIZE}" -fno-omit-frame-pointer)
    target_link_options(looper_publication_checks PRIVATE "-fsanitize=${SANITIZE}")
endif()
