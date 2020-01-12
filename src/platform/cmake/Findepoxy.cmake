find_path(EPOXY_INCLUDE_DIRS NAMES epoxy/gl.h PATH_SUFFIX include)
find_library(EPOXY_LIBRARIES NAMES epoxy_0 PATH_SUFFIX lib)

if (NOT EPOXY_LIBRARIES)
	find_library(EPOXY_LIBRARIES NAMES epoxy PATH_SUFFIX lib)
endif()

if(EPOXY_LIBRARIES AND EPOXY_INCLUDE_DIRS)
    set(epoxy_FOUND TRUE)
endif()
