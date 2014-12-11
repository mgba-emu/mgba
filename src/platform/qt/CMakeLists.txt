cmake_minimum_required(VERSION 2.8.11)
enable_language(CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

if(BUILD_SDL)
	if(NOT SDL_FOUND AND NOT SDL2_FOUND)
		find_package(SDL 1.2 REQUIRED)
	endif()
	if(SDL2_FOUND)
		link_directories(${SDL2_LIBDIR})
	endif()
	set(PLATFORM_LIBRARY "${PLATFORM_LIBRARY};${SDL_LIBRARY};${SDLMAIN_LIBRARY}")
	set(PLATFORM_SRC ${PLATFORM_SRC} ${CMAKE_SOURCE_DIR}/src/platform/sdl/sdl-events.c ${CMAKE_SOURCE_DIR}/src/platform/sdl/sdl-audio.c)
	include_directories(${SDL_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src/platform/sdl)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Multimedia)
find_package(Qt5OpenGL)
find_package(Qt5Widgets)
find_package(OpenGL)

if(NOT Qt5OpenGL_FOUND OR NOT Qt5Widgets_FOUND OR NOT OPENGL_FOUND)
	message(WARNING "Cannot find Qt modules")
	set(BUILD_QT OFF PARENT_SCOPE)
	return()
endif()

set(SOURCE_FILES
	AudioProcessor.cpp
	ConfigController.cpp
	Display.cpp
	GBAApp.cpp
	GBAKeyEditor.cpp
	GIFView.cpp
	GameController.cpp
	InputController.cpp
	KeyEditor.cpp
	LoadSaveState.cpp
	LogView.cpp
	SavestateButton.cpp
	Window.cpp
	VFileDevice.cpp
	VideoView.cpp)

qt5_wrap_ui(UI_FILES
	GIFView.ui
	LoadSaveState.ui
	LogView.ui
	VideoView.ui)

if(BUILD_SDL)
	list(APPEND SOURCE_FILES AudioProcessorSDL.cpp)
elseif(Qt5Multimedia_FOUND)
	list(APPEND SOURCE_FILES
	AudioProcessorQt.cpp
	AudioDevice.cpp)
else()
	message(WARNING "No supported audio modules found")
	set(BUILD_QT OFF PARENT_SCOPE)
	return()
endif()

if(USE_GDB_STUB)
	set(SOURCE_FILES ${PLATFORM_SRC} ${SOURCE_FILES} GDBController.cpp GDBWindow.cpp)
endif()
set(MACOSX_BUNDLE_ICON_FILE mgba.icns)
set(MACOSX_BUNDLE_BUNDLE_VERSION ${LIB_VERSION_STRING})
set(MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
set(MACOSX_BUNDLE_GUI_IDENTIFIER com.endrift.${BINARY_NAME}-qt)
set_source_files_properties(${CMAKE_SOURCE_DIR}/res/mgba.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

qt5_add_resources(RESOURCES resources.qrc)
if(WIN32)
	list(APPEND RESOURCES ${CMAKE_SOURCE_DIR}/res/mgba.rc)
endif()
add_executable(${BINARY_NAME}-qt WIN32 MACOSX_BUNDLE main.cpp ${CMAKE_SOURCE_DIR}/res/mgba.icns ${SOURCE_FILES} ${UI_FILES} ${RESOURCES})
set_target_properties(${BINARY_NAME}-qt PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/res/info.plist.in)

set(QT_LIBRARIES Qt5::Widgets Qt5::OpenGL)
if(Qt5Multimedia_FOUND)
	list(APPEND QT_LIBRARIES Qt5::Multimedia)
endif()
target_link_libraries(${BINARY_NAME}-qt ${PLATFORM_LIBRARY} ${OPENGL_LIBRARY} ${BINARY_NAME} ${QT_LIBRARIES})

install(TARGETS ${BINARY_NAME}-qt RUNTIME DESTINATION bin BUNDLE DESTINATION /Applications)
if(APPLE OR WIN32)
	set_target_properties(${BINARY_NAME}-qt PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
endif()
if(APPLE AND MACDEPLOYQT)
	add_custom_command(TARGET ${BINARY_NAME}-qt POST_BUILD COMMAND ${MACDEPLOYQT} ${PROJECT_NAME}.app)
	add_custom_command(TARGET ${BINARY_NAME}-qt POST_BUILD COMMAND rm -r ${PROJECT_NAME}.app/Contents/Plugins/bearer)
	add_custom_command(TARGET ${BINARY_NAME}-qt POST_BUILD COMMAND rm -r ${PROJECT_NAME}.app/Contents/Plugins/imageformats)
	add_custom_command(TARGET ${BINARY_NAME}-qt POST_BUILD COMMAND rm -r ${PROJECT_NAME}.app/Contents/Plugins/printsupport)
endif()
