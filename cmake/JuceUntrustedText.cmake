# Host state, presets, tunes and imported files are untrusted text. Two narrow
# JUCE 8.0.10 fixes found by LuthierFuzz (scripts/patch_juce_untrusted_text.py):
# malformed UTF-8 overrunning its terminator, and JSON integer overflow.
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
set(_luthier_text_patch "${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch_juce_untrusted_text.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_luthier_text_patch}"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE/modules/juce_core/text/juce_CharPointer_UTF8.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE/modules/juce_core/json/juce_JSON.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${_luthier_text_patch}" "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE"
    RESULT_VARIABLE _luthier_text_result
    OUTPUT_VARIABLE _luthier_text_output
    ERROR_VARIABLE _luthier_text_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
if (NOT _luthier_text_result EQUAL 0)
    message(FATAL_ERROR "JUCE untrusted-text patch failed: ${_luthier_text_error}")
endif()
message(STATUS "${_luthier_text_output}")
unset(_luthier_text_patch)
unset(_luthier_text_result)
unset(_luthier_text_output)
unset(_luthier_text_error)
