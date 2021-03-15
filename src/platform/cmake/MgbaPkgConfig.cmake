include(CMakeParseArguments)
include(CMakePackageConfigHelpers)

# pkgcfg_configure - write a configured pkg-config.pc file
#	Required parameters:
#		- FILE_IN: pkg-config template (*.pc.in file)
#		- FILE_OUT: path to write the configured pkg-config file to (*.pc file)
#	Arguments:
#		- NAME: package name
#		- DESC: package description
#		- VERSION: package version
#		- DEFINES: C macros
#		- CFLAGS: compiler flags
#		- LDFLAGS_PRIV: link flags (PRIVATE)
#		- LDFLAGS_PUB: link flags (PUBLIC)
#		- PKGS_PRIV: required pkg-config packages (PRIVATE)
#		- PKGS_PUB: required pkg-config packages (PUBLIC)
function(pkgcfg_configure FILE_IN FILE_OUT)
	set(args_single NAME DESC VERSION)
	set(args_multi DEFINES CFLAGS LDFLAGS_PRIV LDFLAGS_PUB PKGS_PRIV PKGS_PUB)
	cmake_parse_arguments(PKGCFG "" "${args_single}" "${args_multi}" ${ARGN})
	
	foreach(DEF ${PKGCFG_DEFINES})
		string(REPLACE " " "" DEF "${DEF}")	# remove spaces from defines
		list(APPEND PKGCFG_CFLAGS "-D${DEF}")
	endforeach(DEF)
	
	# CMake uses ; to separate values, in pkg-config we use spaces.
	string(REPLACE ";" " " PKGCFG_CFLAGS "${PKGCFG_CFLAGS}")
	string(REPLACE ";" " " PKGCFG_LDFLAGS_PRIV "${PKGCFG_LDFLAGS_PRIV}")
	string(REPLACE ";" " " PKGCFG_LDFLAGS_PUB "${PKGCFG_LDFLAGS_PUB}")
	string(REPLACE ";" " " PKGCFG_PKGS_PRIV "${PKGCFG_PKGS_PRIV}")
	string(REPLACE ";" " " PKGCFG_PKGS_PUB "${PKGCFG_PKGS_PUB}")

	set(libdir_for_pc_file "\${exec_prefix}/lib")
	set(includedir_for_pc_file "\${prefix}/include")

	configure_file("${FILE_IN}" "${FILE_OUT}" @ONLY)
endfunction()

