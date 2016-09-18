find_program(FIXUP vita-elf-create)
find_program(MAKE_FSELF vita-make-fself)
find_program(MAKE_SFO vita-mksfoex)
find_program(OBJCOPY ${cross_prefix}objcopy)
find_file(NIDDB db.json PATHS ${VITASDK} ${VITASDK}/bin ${VITASDK}/share)
find_program(STRIP ${cross_prefix}strip)

set(OS_DEFINES IOAPI_NO_64)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)

file(GLOB OS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/psp2-*.c)
set(OS_SRC ${OS_SRC} PARENT_SCOPE)
source_group("PS Vita-specific code" FILES ${OS_SRC})

list(APPEND CORE_VFS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/sce-vfs.c)
set(CORE_VFS_SRC ${CORE_VFS_SRC} PARENT_SCOPE)

set(OS_LIB -lvita2d -lSceAppMgr_stub -lSceCtrl_stub -lScePgf_stub -lSceGxm_stub -lSceDisplay_stub -lSceAudio_stub -lSceCommonDialog_stub -lSceMotion_stub -lScePhotoExport_stub -lScePower_stub -lSceSysmodule_stub -lSceTouch_stub -l${M_LIBRARY})
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

add_custom_target(${BINARY_NAME}.velf ALL
                  ${STRIP} --strip-unneeded -go ${BINARY_NAME}-stripped.elf ${BINARY_NAME}.elf
                  COMMAND ${FIXUP} ${BINARY_NAME}-stripped.elf ${BINARY_NAME}.velf ${NIDDB}
                  DEPENDS ${BINARY_NAME}.elf)

add_custom_target(sce_sys ${CMAKE_COMMAND} -E make_directory sce_sys)

add_custom_target(param.sfo
                  ${MAKE_SFO} ${PROJECT_NAME} -s TITLE_ID=MGBA00001 sce_sys/param.sfo
                  DEPENDS sce_sys)

add_custom_target(eboot.bin
                  ${MAKE_FSELF} -s ${BINARY_NAME}.velf eboot.bin
                  DEPENDS ${BINARY_NAME}.velf)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/template.xml.in ${CMAKE_CURRENT_BINARY_DIR}/template.xml)

add_custom_target(livearea
                  ${CMAKE_COMMAND} -E make_directory sce_sys/livearea/contents
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/icon0.png sce_sys
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/pic0.png sce_sys
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/template.xml sce_sys/livearea/contents
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/bg.png sce_sys/livearea/contents
                  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/startup.png sce_sys/livearea/contents
                  DEPENDS sce_sys)

add_custom_target(${BINARY_NAME}.vpk ALL
                  zip -qr ${BINARY_NAME}.vpk sce_sys eboot.bin
                  DEPENDS livearea eboot.bin param.sfo)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.vpk DESTINATION . COMPONENT ${BINARY_NAME}-psp2)
