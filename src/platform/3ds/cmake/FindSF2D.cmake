# - Try to find sf2d
# Once done this will define
#  LIBSF2D_FOUND - System has sf2d
#  LIBSF2D_INCLUDE_DIRS - The sf2d include directories
#  LIBSF2D_LIBRARIES - The libraries needed to use sf2d
#
# It also adds an imported target named `3ds::sf2d`.
# Linking it is the same as target_link_libraries(target ${LIBSF2D_LIBRARIES}) and target_include_directories(target ${LIBSF2D_INCLUDE_DIRS})


# DevkitPro paths are broken on windows, so we have to fix those
macro(msys_to_cmake_path MsysPath ResultingPath)
    string(REGEX REPLACE "^/([a-zA-Z])/" "\\1:/" ${ResultingPath} "${MsysPath}")
endmacro()

if(NOT DEVKITPRO)
    msys_to_cmake_path("$ENV{DEVKITPRO}" DEVKITPRO)
endif()

find_path(LIBSF2D_INCLUDE_DIR sf2d.h
          PATH_SUFFIXES include )

find_library(LIBSF2D_LIBRARY NAMES sf2d libsf2d.a
          PATH_SUFFIXES lib)

set(LIBSF2D_LIBRARIES ${LIBSF2D_LIBRARY} )
set(LIBSF2D_INCLUDE_DIRS ${LIBSF2D_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBSF2D_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(SF2D  DEFAULT_MSG
                                  LIBSF2D_LIBRARY LIBSF2D_INCLUDE_DIR)

mark_as_advanced(LIBSF2D_INCLUDE_DIR LIBSF2D_LIBRARY )
if(SF2D_FOUND)
    set(SF2D ${LIBSF2D_INCLUDE_DIR}/..)
    message(STATUS "setting SF2D to ${SF2D}")

    add_library(3ds::sf2d STATIC IMPORTED GLOBAL)
    set_target_properties(3ds::sf2d PROPERTIES
        IMPORTED_LOCATION "${LIBSF2D_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBSF2D_INCLUDE_DIR}"
    )
endif()
