# Preserve explicit same-value host writes before deferred guitar defaults run.
# Called only when clap-juce-extensions exists, before its add_subdirectory().
# This is an exact, reproducible patch of the pinned upstream wrapper, with no
# target-wide flags or changes to the shared processor binary.
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
set(_luthier_clap_intent_patch "${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch_clap_host_intent.py")
set(_luthier_clap_intent_source "${LUTHIER_CLAP_EXTENSIONS}/src/wrapper/clap-juce-wrapper.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_luthier_clap_intent_patch}" "${_luthier_clap_intent_source}")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${_luthier_clap_intent_patch}" "${LUTHIER_CLAP_EXTENSIONS}"
    RESULT_VARIABLE _luthier_clap_intent_result
    OUTPUT_VARIABLE _luthier_clap_intent_output
    ERROR_VARIABLE _luthier_clap_intent_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
if (NOT _luthier_clap_intent_result EQUAL 0)
    message(FATAL_ERROR "CLAP host-write intent verification failed: ${_luthier_clap_intent_error}")
endif()
message(STATUS "${_luthier_clap_intent_output}")
unset(_luthier_clap_intent_patch)
unset(_luthier_clap_intent_source)
unset(_luthier_clap_intent_result)
unset(_luthier_clap_intent_output)
unset(_luthier_clap_intent_error)
