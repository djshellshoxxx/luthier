# The processor's bounded standalone MIDI output contract depends on this
# off-audio reservation in our pinned JUCE8.0.10 player. Refuse unknown sources.
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
set(_luthier_midi_patch "${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch_juce_midi_capacity.py")
set(_luthier_midi_source "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE/modules/juce_audio_utils/players/juce_AudioProcessorPlayer.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_luthier_midi_patch}" "${_luthier_midi_source}")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${_luthier_midi_patch}" "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE"
    RESULT_VARIABLE _luthier_midi_result
    OUTPUT_VARIABLE _luthier_midi_output
    ERROR_VARIABLE _luthier_midi_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
if (NOT _luthier_midi_result EQUAL 0)
    message(FATAL_ERROR "Standalone MIDI reservation verification failed: ${_luthier_midi_error}")
endif()
message(STATUS "${_luthier_midi_output}")
unset(_luthier_midi_patch)
unset(_luthier_midi_source)
unset(_luthier_midi_result)
unset(_luthier_midi_output)
unset(_luthier_midi_error)
