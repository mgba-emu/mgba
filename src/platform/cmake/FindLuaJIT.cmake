# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindLuaJIT
-------

Locate LuaJIT library.

This module defines:

``LUAJIT_FOUND``
  if false, do not try to link to LuaJIT
``LUAJIT_LIBRARIES``
  both luajit and luajitlib
``LUAJIT_INCLUDE_DIR``
  where to find luajit.h
``LUAJIT_VERSION_STRING``
  the version of LuaJIT found
``LUAJIT_VERSION_MAJOR``
  the major version of LuaJIT
``LUAJIT_VERSION_MINOR``
  the minor version of LuaJIT
``LUAJIT_VERSION_PATCH``
  the patch version of LuaJIT
#]=======================================================================]

cmake_policy(PUSH)  # Policies apply to functions at definition-time
cmake_policy(SET CMP0012 NEW)  # For while(TRUE)

unset(_luajit_include_subdirs)
unset(_luajit_library_names)
unset(_luajit_append_versions)

# this is a function only to have all the variables inside go away automatically
function(_luajit_get_versions)
    set(LUAJIT_VERSIONS2 2.1 2.0)

    if (LuaJIT_FIND_VERSION_EXACT)
        if (LuaJIT_FIND_VERSION_COUNT GREATER 1)
            set(_luajit_append_versions ${LuaJIT_FIND_VERSION_MAJOR}.${LuaJIT_FIND_VERSION_MINOR})
        endif ()
    elseif (LuaJIT_FIND_VERSION)
        # once there is a different major version supported this should become a loop
        if (NOT LuaJIT_FIND_VERSION_MAJOR GREATER 2)
            if (LuaJIT_FIND_VERSION_COUNT EQUAL 1)
                set(_luajit_append_versions ${LUAJIT_VERSIONS2})
            else ()
                foreach (subver IN LISTS LUAJIT_VERSIONS2)
                    if (NOT subver VERSION_LESS ${LuaJIT_FIND_VERSION})
                        list(APPEND _luajit_append_versions ${subver})
                    endif ()
                endforeach ()
                # New version -> Search for it (heuristic only! Defines in include might have changed)
                if (NOT _luajit_append_versions)
                    set(_luajit_append_versions ${LuaJIT_FIND_VERSION_MAJOR}.${LuaJIT_FIND_VERSION_MINOR})
                endif()
            endif ()
        endif ()
    else ()
        # once there is a different major version supported this should become a loop
        set(_luajit_append_versions ${LUAJIT_VERSIONS2})
    endif ()

    if (LUAJIT_Debug)
        message(STATUS "Considering following LuaJIT versions: ${_luajit_append_versions}")
    endif()

    set(_luajit_append_versions "${_luajit_append_versions}" PARENT_SCOPE)
endfunction()

function(_luajit_set_version_vars)
  set(_luajit_include_subdirs_raw "luajit")

  foreach (ver IN LISTS _luajit_append_versions)
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)$" _ver "${ver}")
    if (_ver)
      string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)$" "\\1" _version_major "${ver}")
      string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)$" "\\2" _version_minor "${ver}")
      list(APPEND _luajit_include_subdirs_raw
        luajit${_version_major}${_version_minor}
        luajit${_version_major}.${_version_minor}
        luajit-${_version_major}.${_version_minor}
      )
    endif ()
  endforeach ()

  # Prepend "include/" to each path directly after the path
  set(_luajit_include_subdirs "include")
  foreach (dir IN LISTS _luajit_include_subdirs_raw)
    list(APPEND _luajit_include_subdirs "${dir}" "include/${dir}")
  endforeach ()

  set(_luajit_include_subdirs "${_luajit_include_subdirs}" PARENT_SCOPE)
endfunction(_luajit_set_version_vars)

function(_luajit_get_header_version)
  unset(LUAJIT_VERSION_STRING PARENT_SCOPE)
  set(_hdr_file "${LUAJIT_INCLUDE_DIR}/luajit.h")

  if (NOT EXISTS "${_hdr_file}")
    return()
  endif ()

  file(STRINGS "${_hdr_file}" luajit_version_strings
       REGEX "^#define[ \t]+LUAJIT_VERSION[ \t]+\"LuaJIT [0-9].*")

  string(REGEX REPLACE ".*;#define[ \t]+LUAJIT_VERSION_MAJOR[ \t]+\"([0-9])\"[ \t]*;.*" "\\1" LUAJIT_VERSION_MAJOR ";${luajit_version_strings};")
  if (LUAJIT_VERSION_MAJOR MATCHES "^[0-9]+$")
    string(REGEX REPLACE ".*;#define[ \t]+LUAJIT_VERSION_MINOR[ \t]+\"([0-9])\"[ \t]*;.*" "\\1" LUAJIT_VERSION_MINOR ";${luajit_version_strings};")
    string(REGEX REPLACE ".*;#define[ \t]+LUAJIT_VERSION_RELEASE[ \t]+\"([0-9])\"[ \t]*;.*" "\\1" LUAJIT_VERSION_PATCH ";${luajit_version_strings};")
    set(LUAJIT_VERSION_STRING "${LUAJIT_VERSION_MAJOR}.${LUAJIT_VERSION_MINOR}.${LUAJIT_VERSION_PATCH}")
  else ()
    string(REGEX REPLACE ".*;#define[ \t]+LUAJIT_VERSION[ \t]+\"LuaJIT ([0-9.]+[^\"]*)\"[ \t]*;.*" "\\1" LUAJIT_VERSION_STRING ";${luajit_version_strings};")
    string(REGEX REPLACE "^([0-9]+)\\.[^\"]*$" "\\1" LUAJIT_VERSION_MAJOR "${LUAJIT_VERSION_STRING}")
    string(REGEX REPLACE "^[0-9]+\\.([0-9]+)[^\"]*$" "\\1" LUAJIT_VERSION_MINOR "${LUAJIT_VERSION_STRING}")
    string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]).*" "\\1" LUAJIT_VERSION_PATCH "${LUAJIT_VERSION_STRING}")
  endif ()
  foreach (ver IN LISTS _luajit_append_versions)
    if (ver STREQUAL "${LUAJIT_VERSION_MAJOR}.${LUAJIT_VERSION_MINOR}")
      set(LUAJIT_VERSION_MAJOR ${LUAJIT_VERSION_MAJOR} PARENT_SCOPE)
      set(LUAJIT_VERSION_MINOR ${LUAJIT_VERSION_MINOR} PARENT_SCOPE)
      set(LUAJIT_VERSION_PATCH ${LUAJIT_VERSION_PATCH} PARENT_SCOPE)
      set(LUAJIT_VERSION_STRING ${LUAJIT_VERSION_STRING} PARENT_SCOPE)
      return()
    endif ()
  endforeach ()
endfunction(_luajit_get_header_version)

function(_luajit_find_header)
  _luajit_set_version_vars()

  # Initialize as local variable
  set(CMAKE_IGNORE_PATH ${CMAKE_IGNORE_PATH})
  while (TRUE)
    # Find the next header to test. Check each possible subdir in order
    # This prefers e.g. higher versions as they are earlier in the list
    # It is also consistent with previous versions of FindLua
    foreach (subdir IN LISTS _luajit_include_subdirs)
      find_path(LUAJIT_INCLUDE_DIR luajit.h
        HINTS ENV LUAJIT_DIR
        PATH_SUFFIXES ${subdir}
        )
      if (LUAJIT_INCLUDE_DIR)
        break()
      endif()
    endforeach()
    # Did not found header -> Fail
    if (NOT LUAJIT_INCLUDE_DIR)
      return()
    endif()
    _luajit_get_header_version()
    # Found accepted version -> Ok
    if (LUAJIT_VERSION_STRING)
      if (LUAJIT_Debug)
        message(STATUS "Found suitable version ${LUAJIT_VERSION_STRING} in ${LUAJIT_INCLUDE_DIR}/lua.h")
      endif()
      return()
    endif()
    # Found wrong version -> Ignore this path and retry
    if (LUAJIT_Debug)
      message(STATUS "Ignoring unsuitable version in ${LUAJIT_INCLUDE_DIR}")
    endif()
    list(APPEND CMAKE_IGNORE_PATH "${LUAJIT_INCLUDE_DIR}")
    unset(LUAJIT_INCLUDE_DIR CACHE)
    unset(LUAJIT_INCLUDE_DIR)
    unset(LUAJIT_INCLUDE_DIR PARENT_SCOPE)
  endwhile ()
endfunction()

_luajit_get_versions()
_luajit_find_header()
_luajit_get_header_version()
unset(_luajit_append_versions)

if (LUAJIT_VERSION_STRING)
  set(_luajit_library_names
    luajit${LUAJIT_VERSION_MAJOR}${LUAJIT_VERSION_MINOR}
    luajit${LUAJIT_VERSION_MAJOR}.${LUAJIT_VERSION_MINOR}
    luajit-${LUAJIT_VERSION_MAJOR}.${LUAJIT_VERSION_MINOR}
    luajit.${LUAJIT_VERSION_MAJOR}.${LUAJIT_VERSION_MINOR}
    luajit51
    luajit5.1
    luajit-5.1
    luajit.5.1
    lua51
    lua5.1
    lua-5.1
    lua.5.1
    )
endif ()

find_library(LUAJIT_LIBRARY
  NAMES ${_luajit_library_names} luajit
  NAMES_PER_DIR
  HINTS
    ENV LUAJIT_DIR
  PATH_SUFFIXES lib
)
unset(_luajit_library_names)

if (LUAJIT_LIBRARY)
  # include the math library for Unix
  if (UNIX AND NOT APPLE AND NOT BEOS)
    find_library(LUAJIT_MATH_LIBRARY m)
    mark_as_advanced(LUAJIT_MATH_LIBRARY)
    set(LUAJIT_LIBRARIES "${LUAJIT_LIBRARY};${LUAJIT_MATH_LIBRARY}")

    # include dl library for statically-linked Lua library
    get_filename_component(LUAJIT_LIB_EXT ${LUAJIT_LIBRARY} EXT)
    if(LUAJIT_LIB_EXT STREQUAL CMAKE_STATIC_LIBRARY_SUFFIX)
      list(APPEND LUAJIT_LIBRARIES ${CMAKE_DL_LIBS})
    endif()

  # For Windows and Mac, don't need to explicitly include the math library
  else ()
    set(LUAJIT_LIBRARIES "${LUAJIT_LIBRARY}")
  endif ()
endif ()

# handle the QUIETLY and REQUIRED arguments and set LUAJIT_FOUND to TRUE if
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LuaJIT
                                  REQUIRED_VARS LUAJIT_LIBRARIES LUAJIT_INCLUDE_DIR
                                  VERSION_VAR LUAJIT_VERSION_STRING)

mark_as_advanced(LUAJIT_INCLUDE_DIR LUAJIT_LIBRARY)

cmake_policy(POP)
