# SPEC-SWEEP (README RM-17, USER_MANUAL UM-53, spec SP-137): a smoke test for
# luthier-render, run by CTest (test LuthierRenderCli) and by ci_build.sh's test step.
#
#   cmake -DRENDER=<path to LuthierRender> -DMIDI=<a .mid> -DOUT_DIR=<scratch dir>
#         -P scripts/render_cli_smoke.cmake
#
# Every documented flag is exercised: --help, --list-presets, --list-guitars,
# --list-phrases, --audition/--guitar/--verbose, --midi/--preset/--out, and an
# unknown option must fail.

foreach(var RENDER MIDI OUT_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "render_cli_smoke: -D${var}=... is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUT_DIR}")
file(MAKE_DIRECTORY "${OUT_DIR}")

function(run_ok name)
    execute_process(COMMAND "${RENDER}" ${ARGN}
                    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "luthier-render ${name} failed (${rc}):\n${out}\n${err}")
    endif()
    set(LAST_OUT "${out}" PARENT_SCOPE)
endfunction()

function(expect_wav file min_bytes)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "no file was written at ${file}")
    endif()
    file(SIZE "${file}" size)
    if(size LESS ${min_bytes})
        message(FATAL_ERROR "${file} is only ${size} bytes")
    endif()
    file(READ "${file}" magic LIMIT 4 HEX)
    if(NOT magic STREQUAL "52494646")   # "RIFF"
        message(FATAL_ERROR "${file} is not a WAV file")
    endif()
endfunction()

run_ok(--help --help)
if(NOT LAST_OUT MATCHES "--midi" OR NOT LAST_OUT MATCHES "--list-presets")
    message(FATAL_ERROR "--help does not describe the documented flags:\n${LAST_OUT}")
endif()

run_ok(--list-presets --list-presets)
if(NOT LAST_OUT MATCHES "Modern Metal Chug")
    message(FATAL_ERROR "--list-presets did not list the factory bank:\n${LAST_OUT}")
endif()

run_ok(--list-guitars --list-guitars)
run_ok(--list-phrases --list-phrases)

run_ok(--audition --audition "Single Note" --guitar "Vintage Single-Cut" --tail 0.5 --verbose
       --out "${OUT_DIR}/audition.wav")
expect_wav("${OUT_DIR}/audition.wav" 20000)

# Two bars at 120 bpm plus a one-second tail: about 5 s of 48 kHz stereo 24-bit.
run_ok(--midi --midi "${MIDI}" --preset "Modern Metal Chug" --tail 1 --out "${OUT_DIR}/riff.wav")
expect_wav("${OUT_DIR}/riff.wav" 1000000)

execute_process(COMMAND "${RENDER}" --no-such-flag RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(rc EQUAL 0)
    message(FATAL_ERROR "an unknown option was accepted")
endif()

file(REMOVE_RECURSE "${OUT_DIR}")
message(STATUS "luthier-render smoke test passed")
