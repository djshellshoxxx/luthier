#==============================================================================
# Linux packaging for Luthier (spec/installer.md 3, 4, 6, 10).
#
# Included from the top-level CMakeLists.txt when LUTHIER_BUILD_PACKAGES is ON.
# Produces, through CPack:
#
#   luthier_<version>_amd64.deb           system install (installer.md 3.1 layout)
#   Luthier-<version>-linux-x64.tar.gz    relocatable tree with install.sh
#   Luthier-<version>-linux-x64-standalone.tar.gz   the same without the VST3
#                                         (installer.md 4; cpack -D LUTHIER_STANDALONE_ONLY=ON)
#
# Two sets of install rules exist because the two packages want different
# shapes. The "sys_*" components lay files out for the .deb; the "tgz_*"
# components (EXCLUDE_FROM_ALL, so a plain `cmake --install` ignores them) lay
# the same files out flat for the tarball. CPackProjectConfig.cmake picks the
# set for the generator that is running.
#
# Where things go, and why (see docs/INSTALL_LINUX.md and DECISIONS.md):
#
#   /usr/lib/vst3/Luthier.vst3/       the VST3 bundle. Its top-level Resources
#                                     is a symlink to the shared content, which
#                                     is where the build tree puts it too
#                                     (luthier_copy_resources), so IrLibrary's
#                                     walk finds it at the same depth.
#   /opt/Luthier/Luthier              the standalone; /usr/bin/luthier -> it.
#   /opt/Luthier/Resources/           factory content (IRs, parts, guitars,
#                                     tunes, fonts), once. IrLibrary::
#                                     searchForResources looks in
#                                     commonApplicationDataDirectory/Luthier/
#                                     Resources, which JUCE maps to /opt on
#                                     Linux, and beside the executable.
#   /opt/Luthier/.installed_version   written by postinst (installer.md 6).
#   /usr/share/applications, /usr/share/mime/packages, /usr/share/icons,
#   /usr/share/doc/luthier            desktop file, MIME types, icons, licences.
#==============================================================================

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Luthier packaging: scripted for Linux only; nothing to do on ${CMAKE_SYSTEM_NAME}")
    return()
endif()

if (NOT TARGET Luthier_VST3 OR NOT TARGET Luthier_Standalone)
    message(FATAL_ERROR "Luthier packaging needs the Luthier_VST3 and Luthier_Standalone targets")
endif()

set(LUTHIER_PKG_SRC "${CMAKE_CURRENT_LIST_DIR}")
set(LUTHIER_PKG_GEN "${CMAKE_BINARY_DIR}/packaging")
file(MAKE_DIRECTORY "${LUTHIER_PKG_GEN}")

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(LUTHIER_PKG_ARCH_TAG "x64")
else()
    set(LUTHIER_PKG_ARCH_TAG "${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(LUTHIER_PKG_BASENAME "Luthier-${PROJECT_VERSION}-linux-${LUTHIER_PKG_ARCH_TAG}")

# The system-install roots. postinst, postrm and the scripts read the same
# values through configure_file, so a change here changes them all.
set(LUTHIER_SYS_OPT      "opt/Luthier")
set(LUTHIER_SYS_VST3_DIR "usr/lib/vst3")

#------------------------------------------------------------------------------
# THIRD_PARTY_LICENCES.txt: JUCE, the VST3 SDK it wraps, and the two OFL fonts.
# Generated at configure time; re-runs when any source text changes.
#------------------------------------------------------------------------------
set(LUTHIER_LICENCE_SOURCES
    "JUCE 8 framework|${CMAKE_SOURCE_DIR}/ThirdParty/JUCE/LICENSE.md"
    "Steinberg VST 3 SDK (via JUCE)|${CMAKE_SOURCE_DIR}/ThirdParty/JUCE/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt"
    "Lato (SIL Open Font Licence 1.1)|${LUTHIER_RESOURCES}/Fonts/Lato-OFL.txt"
    "Bebas Neue (SIL Open Font Licence 1.1)|${LUTHIER_RESOURCES}/Fonts/BebasNeue-OFL.txt")

set(LUTHIER_LICENCES_FILE "${LUTHIER_PKG_GEN}/THIRD_PARTY_LICENCES.txt")

set(_licences "THIRD-PARTY LICENCES FOR LUTHIER ${PROJECT_VERSION}\n")
string(APPEND _licences "==========================================\n\n")
string(APPEND _licences "Luthier links the JUCE framework (which embeds the Steinberg VST 3 SDK)\n")
string(APPEND _licences "and ships the Lato and Bebas Neue typefaces in Resources/Fonts. Their\n")
string(APPEND _licences "licence texts follow, verbatim.\n\n")

foreach (entry IN LISTS LUTHIER_LICENCE_SOURCES)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 title)
    list(GET parts 1 path)

    if (NOT EXISTS "${path}")
        message(FATAL_ERROR "Luthier packaging: licence text missing: ${path}")
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    file(READ "${path}" text)
    string(APPEND _licences "------------------------------------------------------------------------\n")
    string(APPEND _licences "${title}\n")
    string(APPEND _licences "------------------------------------------------------------------------\n\n")
    string(APPEND _licences "${text}\n\n")
endforeach()

file(WRITE "${LUTHIER_LICENCES_FILE}" "${_licences}")
unset(_licences)

#------------------------------------------------------------------------------
# Generated control files and scripts.
#------------------------------------------------------------------------------
set(LUTHIER_VERSION "${PROJECT_VERSION}")   # what the .in files substitute

configure_file("${LUTHIER_PKG_SRC}/postinst.in" "${LUTHIER_PKG_GEN}/postinst" @ONLY)
configure_file("${LUTHIER_PKG_SRC}/postrm.in"   "${LUTHIER_PKG_GEN}/postrm"   @ONLY)
configure_file("${LUTHIER_PKG_SRC}/README.txt.in" "${LUTHIER_PKG_GEN}/README.txt" @ONLY)
configure_file("${LUTHIER_PKG_SRC}/CPackProjectConfig.cmake.in"
               "${LUTHIER_PKG_GEN}/CPackProjectConfig.cmake" @ONLY)

# dpkg insists on 0755 for maintainer scripts; configure_file keeps the
# source mode, so set it explicitly.
file(CHMOD "${LUTHIER_PKG_GEN}/postinst" "${LUTHIER_PKG_GEN}/postrm"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# Symlinks are made here and installed as files: install(FILES) keeps a
# symlink a symlink, and this needs no install(CODE) that has to know DESTDIR.
file(MAKE_DIRECTORY "${LUTHIER_PKG_GEN}/links/sys" "${LUTHIER_PKG_GEN}/links/tgz")
file(CREATE_LINK "/${LUTHIER_SYS_OPT}/Luthier"   "${LUTHIER_PKG_GEN}/links/sys/luthier"   SYMBOLIC)
file(CREATE_LINK "/${LUTHIER_SYS_OPT}/Resources" "${LUTHIER_PKG_GEN}/links/sys/Resources" SYMBOLIC)
file(CREATE_LINK "../Resources"                  "${LUTHIER_PKG_GEN}/links/tgz/Resources" SYMBOLIC)

#------------------------------------------------------------------------------
# System layout (the .deb).
#------------------------------------------------------------------------------
set(LUTHIER_VST3_ARCH_DIR "${LUTHIER_SYS_VST3_DIR}/Luthier.vst3/Contents/x86_64-linux")

install(TARGETS Luthier_VST3
        LIBRARY DESTINATION "${LUTHIER_VST3_ARCH_DIR}"
        COMPONENT sys_vst3)

# moduleinfo.json, which JUCE's VST3 helper writes after the link.
install(FILES "$<TARGET_FILE_DIR:Luthier_VST3>/../Resources/moduleinfo.json"
        DESTINATION "${LUTHIER_SYS_VST3_DIR}/Luthier.vst3/Contents/Resources"
        COMPONENT sys_vst3 OPTIONAL)

install(FILES "${LUTHIER_PKG_GEN}/links/sys/Resources"
        DESTINATION "${LUTHIER_SYS_VST3_DIR}/Luthier.vst3"
        COMPONENT sys_vst3)

install(TARGETS Luthier_Standalone
        RUNTIME DESTINATION "${LUTHIER_SYS_OPT}"
        COMPONENT sys_standalone)

install(FILES "${LUTHIER_PKG_GEN}/links/sys/luthier"
        DESTINATION usr/bin
        COMPONENT sys_standalone)

install(DIRECTORY "${LUTHIER_RESOURCES}/"
        DESTINATION "${LUTHIER_SYS_OPT}/Resources"
        COMPONENT sys_content
        PATTERN "*.ico" EXCLUDE)

install(FILES "${LUTHIER_PKG_SRC}/luthier.desktop"
        DESTINATION usr/share/applications COMPONENT sys_desktop)
install(FILES "${LUTHIER_PKG_SRC}/luthier.xml"
        DESTINATION usr/share/mime/packages COMPONENT sys_desktop)
install(FILES "${LUTHIER_RESOURCES}/icon.png"
        DESTINATION usr/share/icons/hicolor/512x512/apps RENAME luthier.png
        COMPONENT sys_desktop)
install(FILES "${LUTHIER_RESOURCES}/icon_small.png"
        DESTINATION usr/share/icons/hicolor/128x128/apps RENAME luthier.png
        COMPONENT sys_desktop)

install(FILES "${LUTHIER_LICENCES_FILE}"
        DESTINATION usr/share/doc/luthier COMPONENT sys_licences)
install(FILES "${LUTHIER_LICENCES_FILE}"
        DESTINATION usr/share/doc/luthier RENAME copyright COMPONENT sys_licences)

#------------------------------------------------------------------------------
# Flat layout (the tarballs). Everything sits in one folder that also runs in
# place: `./Luthier` finds Resources beside it, and Luthier.vst3/Resources is a
# relative symlink to the same folder.
#------------------------------------------------------------------------------
install(TARGETS Luthier_VST3
        LIBRARY DESTINATION "Luthier.vst3/Contents/x86_64-linux"
        COMPONENT tgz_vst3 EXCLUDE_FROM_ALL)
install(FILES "$<TARGET_FILE_DIR:Luthier_VST3>/../Resources/moduleinfo.json"
        DESTINATION "Luthier.vst3/Contents/Resources"
        COMPONENT tgz_vst3 EXCLUDE_FROM_ALL OPTIONAL)
install(FILES "${LUTHIER_PKG_GEN}/links/tgz/Resources"
        DESTINATION "Luthier.vst3"
        COMPONENT tgz_vst3 EXCLUDE_FROM_ALL)

install(TARGETS Luthier_Standalone
        RUNTIME DESTINATION "."
        COMPONENT tgz_standalone EXCLUDE_FROM_ALL)

install(DIRECTORY "${LUTHIER_RESOURCES}/"
        DESTINATION "Resources"
        COMPONENT tgz_content EXCLUDE_FROM_ALL
        PATTERN "*.ico" EXCLUDE)

install(PROGRAMS "${LUTHIER_PKG_SRC}/install.sh" "${LUTHIER_PKG_SRC}/uninstall.sh"
        DESTINATION "." COMPONENT tgz_scripts EXCLUDE_FROM_ALL)
install(FILES "${LUTHIER_PKG_SRC}/luthier.desktop" "${LUTHIER_PKG_SRC}/luthier.xml"
              "${LUTHIER_PKG_GEN}/README.txt" "${LUTHIER_LICENCES_FILE}"
        DESTINATION "." COMPONENT tgz_scripts EXCLUDE_FROM_ALL)
install(FILES "${LUTHIER_RESOURCES}/icon.png"
        DESTINATION "icons" RENAME luthier-512.png COMPONENT tgz_scripts EXCLUDE_FROM_ALL)
install(FILES "${LUTHIER_RESOURCES}/icon_small.png"
        DESTINATION "icons" RENAME luthier-128.png COMPONENT tgz_scripts EXCLUDE_FROM_ALL)

#------------------------------------------------------------------------------
# CPack
#------------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME                "Luthier")
set(CPACK_PACKAGE_VENDOR              "Luthier Audio")
set(CPACK_PACKAGE_VERSION             "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR       "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR       "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH       "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT             "Luthier Audio <support@luthieraudio.example>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Physically modelled guitar instrument (VST3 plugin and standalone)")
set(CPACK_PACKAGE_DESCRIPTION
"Luthier is a physically modelled guitar synthesizer: strings, body, pickups,
amp and pedals, with a workshop of swappable parts, a tune builder and
practice tools. This package holds the VST3 plugin, the standalone
application and the factory content (impulse responses, parts, guitars,
tunes, fonts). Audio: ALSA, or JACK / PipeWire-JACK when present.")
set(CPACK_PACKAGE_FILE_NAME           "${LUTHIER_PKG_BASENAME}")
set(CPACK_PACKAGE_CHECKSUM            SHA256)
set(CPACK_GENERATOR                   "TGZ;DEB")
# Relative install destinations resolve against this prefix. DESTDIR mode is
# off: with it on, a prefix of "/" is dropped and CMAKE_INSTALL_PREFIX
# (/usr/local) sneaks in front of every path.
set(CPACK_SET_DESTDIR                 OFF)
set(CPACK_PACKAGING_INSTALL_PREFIX    "/")
set(CPACK_STRIP_FILES                 ON)
set(CPACK_VERBATIM_VARIABLES          ON)
set(CPACK_PROJECT_CONFIG_FILE         "${LUTHIER_PKG_GEN}/CPackProjectConfig.cmake")
set(CPACK_RESOURCE_FILE_LICENSE       "${LUTHIER_LICENCES_FILE}")
set(CPACK_OUTPUT_FILE_PREFIX          "${CMAKE_BINARY_DIR}/packages")

# One package per generator, whichever components it selects.
set(CPACK_COMPONENTS_GROUPING         ALL_COMPONENTS_IN_ONE)
set(CPACK_ARCHIVE_COMPONENT_INSTALL   ON)
set(CPACK_DEB_COMPONENT_INSTALL       ON)

set(CPACK_DEBIAN_PACKAGE_NAME         "luthier")
set(CPACK_DEBIAN_FILE_NAME            "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_SECTION      "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY     "optional")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER   "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS    ON)
# JUCE loads X11 with dlopen, so dpkg-shlibdeps cannot see it (installer.md 3.3).
set(CPACK_DEBIAN_PACKAGE_DEPENDS      "libx11-6, libxext6, libxrandr2, libxinerama1, libxcursor1, libxrender1")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS   "pipewire-jack | jackd2")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${LUTHIER_PKG_GEN}/postinst;${LUTHIER_PKG_GEN}/postrm")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
set(CPACK_DEBIAN_COMPRESSION_TYPE     "xz")

set(CPACK_ARCHIVE_FILE_NAME           "${LUTHIER_PKG_BASENAME}")
set(CPACK_ARCHIVE_THREADS             0)

include(CPack)

message(STATUS "Luthier packaging: cpack -G DEB / TGZ will write to ${CPACK_OUTPUT_FILE_PREFIX}")
