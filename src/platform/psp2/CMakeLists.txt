include("${VITASDK}/share/vita.cmake" REQUIRED)

find_program(OBJCOPY ${cross_prefix}objcopy)
find_file(NIDDB db.json PATHS ${VITASDK} ${VITASDK}/bin ${VITASDK}/share)

set(OS_DEFINES IOAPI_NO_64 _GNU_SOURCE)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)

file(GLOB OS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/psp2-*.c)
set(OS_SRC ${OS_SRC} PARENT_SCOPE)
source_group("PS Vita-specific code" FILES ${OS_SRC})

list(APPEND CORE_VFS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/sce-vfs.c)
set(CORE_VFS_SRC ${CORE_VFS_SRC} PARENT_SCOPE)

set(OS_LIB -lvita2d -l${M_LIBRARY}
    -lSceAppMgr_stub
    -lSceAppUtil_stub
    -lSceAudio_stub
    -lSceCamera_stub
    -lSceCommonDialog_stub
    -lSceCtrl_stub
    -lSceDisplay_stub
    -lSceGxm_stub
    -lSceIme_stub
    -lSceMotion_stub
    -lSceNetCtl_stub
    -lScePgf_stub
    -lScePhotoExport_stub
    -lScePower_stub
    -lSceRtc_stub
    -lSceSysmodule_stub
    -lSceTouch_stub)
set(OS_LIB ${OS_LIB} PARENT_SCOPE)
set(OBJCOPY_CMD ${OBJCOPY} -I binary -O elf32-littlearm -B arm)

list(APPEND GUI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/icons.o
                            ${CMAKE_CURRENT_BINARY_DIR}/backdrop.o
                            PROPERTIES GENERATED ON)
add_executable(${BINARY_NAME}.elf ${PLATFORM_SRC} ${GUI_SRC} main.c
               ${CMAKE_CURRENT_BINARY_DIR}/icons.o
               ${CMAKE_CURRENT_BINARY_DIR}/backdrop.o)
set_target_properties(${BINARY_NAME}.elf PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES};${FEATURE_DEFINES};${FUNCTION_DEFINES}")
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${OS_LIB})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icons.o
                   COMMAND ${OBJCOPY_CMD} icons2x.png ${CMAKE_CURRENT_BINARY_DIR}/icons.o
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/res)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/backdrop.o
                   COMMAND ${OBJCOPY_CMD} backdrop.png ${CMAKE_CURRENT_BINARY_DIR}/backdrop.o
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

vita_create_self(${BINARY_NAME}.self ${BINARY_NAME}.elf)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/template.xml.in ${CMAKE_CURRENT_BINARY_DIR}/template.xml)

vita_create_vpk(${BINARY_NAME}.vpk MGBA00001 ${BINARY_NAME}.self
                NAME ${PROJECT_NAME}
                FILE ${CMAKE_CURRENT_SOURCE_DIR}/icon0.png sce_sys/icon0.png
                FILE ${CMAKE_CURRENT_SOURCE_DIR}/pic0.png sce_sys/pic0.png
                FILE ${CMAKE_CURRENT_SOURCE_DIR}/bg.png sce_sys/livearea/contents/bg.png
                FILE ${CMAKE_CURRENT_SOURCE_DIR}/startup.png sce_sys/livearea/contents/startup.png
                FILE ${CMAKE_CURRENT_BINARY_DIR}/template.xml sce_sys/livearea/contents/template.xml)

if(BUILD_PERF)
	add_executable(perf-main.elf perf.c)
	set_target_properties(perf-main.elf PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES};${FEATURE_DEFINES};${FUNCTION_DEFINES}")
	target_link_libraries(perf-main.elf ${OS_LIB})

	vita_create_self(perf.bin ../${BINARY_NAME}-perf)
	vita_create_self(perf-main.self perf-main.elf)

	vita_create_vpk(${BINARY_NAME}-perf.vpk MGBA00002 perf-main.self
	                NAME "${PROJECT_NAME} Perf Tester"
	                FILE ${CMAKE_CURRENT_BINARY_DIR}/perf.bin perf.bin
	                FILE ${CMAKE_CURRENT_SOURCE_DIR}/icon0.png sce_sys/icon0.png
	                FILE ${CMAKE_CURRENT_SOURCE_DIR}/pic0.png sce_sys/pic0.png
	                FILE ${CMAKE_CURRENT_SOURCE_DIR}/bg.png sce_sys/livearea/contents/bg.png
	                FILE ${CMAKE_CURRENT_SOURCE_DIR}/startup.png sce_sys/livearea/contents/startup.png
	                FILE ${CMAKE_CURRENT_BINARY_DIR}/template.xml sce_sys/livearea/contents/template.xml)
	add_dependencies(${BINARY_NAME}-perf.vpk perf.bin)

	install(FILES
	        ${CMAKE_CURRENT_BINARY_DIR}/perf.bin
	        ${CMAKE_CURRENT_SOURCE_DIR}/perf-start.sh
	        ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}-perf.vpk
	        DESTINATION . COMPONENT ${BINARY_NAME}-perf)
endif()

install(TARGETS ${BINARY_NAME}.elf DESTINATION . COMPONENT ${BINARY_NAME}-dbg)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.vpk DESTINATION . COMPONENT ${BINARY_NAME}-psp2)
