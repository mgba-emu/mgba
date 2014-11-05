cmake_minimum_required(VERSION 2.8.8)
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
	set(PLATFORM_SRC ${PLATFORM_SRC} ${CMAKE_SOURCE_DIR}/src/platform/sdl/sdl-events.c)
	include_directories(${SDL_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src/platform/sdl)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Multimedia)
find_package(Qt5OpenGL)
find_package(Qt5Widgets)
find_package(OpenGL)

if(NOT Qt5Multimedia_FOUND OR NOT Qt5OpenGL_FOUND OR NOT Qt5Widgets_FOUND OR NOT OPENGL_FOUND)
	set(BUILD_QT OFF PARENT_SCOPE)
	return()
endif()

set(SOURCE_FILES
	AudioDevice.cpp
	AudioProcessor.cpp
	ConfigController.cpp
	Display.cpp
	GBAApp.cpp
	GameController.cpp
	LoadSaveState.cpp
	LogView.cpp
	SavestateButton.cpp
	Window.cpp
	VFileDevice.cpp
	VideoView.cpp)

qt5_wrap_ui(UI_FILES
	LoadSaveState.ui
	LogView.ui
	VideoView.ui)

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
add_executable(mGBA WIN32 MACOSX_BUNDLE main.cpp ${CMAKE_SOURCE_DIR}/res/mgba.icns ${SOURCE_FILES} ${UI_FILES} ${RESOURCES})
set_target_properties(mGBA PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/res/info.plist.in)

qt5_use_modules(mGBA Widgets Multimedia OpenGL)
target_link_libraries(mGBA ${PLATFORM_LIBRARY} ${OPENGL_LIBRARY} ${BINARY_NAME} Qt5::Widgets)
