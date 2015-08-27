add_executable(${BINARY_NAME}.elf ${GUI_SRC} ${CMAKE_BINARY_DIR}/font.c)
target_link_libraries(${BINARY_NAME}.elf ${BINARY_NAME} ${OS_LIB})
add_custom_command(TARGET ${BINARY_NAME}.elf POST_BUILD COMMAND ${ELF2DOL} ${BINARY_NAME}.elf ${BINARY_NAME}.dol)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta.xml.in ${CMAKE_CURRENT_BINARY_DIR}/meta.xml)
add_custom_target(${BINARY_NAME}-wii.zip ALL
	COMMAND ${CMAKE_COMMAND} -E remove ${BINARY_NAME}-wii.zip
	COMMAND ${CMAKE_COMMAND} -E make_directory ${BINARY_NAME}-wii
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/meta.xml ${BINARY_NAME}-wii/
	COMMAND ${CMAKE_COMMAND} -E copy ${BINARY_NAME}.dol ${BINARY_NAME}-wii/boot.dol
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/icon.png ${BINARY_NAME}-wii/
	COMMAND zip -r ${BINARY_NAME}-wii.zip ${BINARY_NAME}-wii
	DEPENDS ${BINARY_NAME}.elf
	SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/meta.xml.in
	${CMAKE_CURRENT_SOURCE_DIR}/icon.png)
