# SPEC-SWEEP (TROUBLESHOOTING TS-1/TS-2): the install locations
# docs/TROUBLESHOOTING.md gives must be the ones the installers write, so the
# table and the installers cannot drift apart. Run as the CTest test
# PackagingPaths or directly: cmake -P scripts/check_packaging_paths.cmake

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

file(READ "${ROOT}/docs/TROUBLESHOOTING.md" doc)
file(READ "${ROOT}/packaging/windows/Luthier.iss" iss)
file(READ "${ROOT}/packaging/linux/install.sh" linux)
file(READ "${ROOT}/scripts/package_macos.sh" mac)
file(READ "${ROOT}/packaging/macos/Uninstall.command" macUninstall)

set(failures "")

# <text the doc shows> | <file variable> | <text that installer must contain>
# The docs show the Pro names; the Free edition's differ only in "Luthier Free".
# The installers take the product name from a variable (ISPP ProductName,
# PRODUCT / PRODUCT_NAME in the shell scripts), so they are matched on that.
set(pairs
    "C:\\Program Files\\Common Files\\VST3\\Luthier Pro.vst3|iss|{commoncf64}\\VST3\\{#ProductName}.vst3"
    "C:\\Program Files\\Common Files\\CLAP\\Luthier Pro.clap|iss|{commoncf64}\\CLAP\\{#ProductName}.clap"
    "/Library/Audio/Plug-Ins/VST3/Luthier Pro.vst3|mac|/Library/Audio/Plug-Ins/VST3"
    "/Library/Audio/Plug-Ins/Components/Luthier Pro.component|mac|/Library/Audio/Plug-Ins/Components"
    "/Library/Audio/Plug-Ins/CLAP/Luthier Pro.clap|macUninstall|/Library/Audio/Plug-Ins/CLAP/$PRODUCT.clap"
    "/Applications/Luthier Pro/Uninstall.command|mac|/Applications/$PRODUCT_NAME/Uninstall.command"
    "~/.vst3/Luthier Pro.vst3|linux|$HOME/.vst3"
    "~/.clap/Luthier Pro.clap|linux|$HOME/.clap"
)

foreach(pair IN LISTS pairs)
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 docText)
    list(GET parts 1 var)
    list(GET parts 2 installerText)

    string(FIND "${doc}" "${docText}" inDoc)
    string(FIND "${${var}}" "${installerText}" inInstaller)

    if(inDoc EQUAL -1)
        string(APPEND failures "  docs/TROUBLESHOOTING.md does not list ${docText}\n")
    endif()
    if(inInstaller EQUAL -1)
        string(APPEND failures "  the ${var} installer does not write ${installerText}\n")
    endif()
endforeach()

if(failures)
    message(FATAL_ERROR "Install locations have drifted:\n${failures}")
endif()

message(STATUS "Install locations in TROUBLESHOOTING.md match the installers")
