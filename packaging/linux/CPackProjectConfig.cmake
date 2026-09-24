# Per-generator CPack settings (installer.md 3). CPack runs this once per
# generator: the .deb installs under /usr and leaves out the tarball's
# install.sh / uninstall.sh; the tarball is relocatable and carries them.
if (CPACK_GENERATOR STREQUAL "DEB")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_COMPONENTS_ALL main)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY OFF)
elseif (CPACK_GENERATOR STREQUAL "TGZ")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
    set(CPACK_COMPONENTS_ALL main tarball)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON)
endif()
