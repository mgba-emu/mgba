cmake_minimum_required(VERSION 2.8.8)
project(mGBA-Qt)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wall -Wextra --std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Wall -Wextra --std=c++11")

if(BUILD_SDL)
	find_package(SDL 1.2 REQUIRED)
	set(PLATFORM_LIBRARY "${PLATFORM_LIBRARY};${SDL_LIBRARY};${SDLMAIN_LIBRARY}")
	set(PLATFORM_SRC ${PLATFORM_SRC} ${CMAKE_SOURCE_DIR}/src/platform/sdl/sdl-events.c)
	include_directories(${SDL_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src/platform/sdl)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets)
find_package(OpenGL)

if(NOT Qt5Widgets_FOUND OR NOT OPENGL_FOUND)
	set(BUILD_QT OFF PARENT_SCOPE)
	return()
endif()

set(SOURCE_FILES
	AudioDevice.cpp
	AudioProcessor.cpp
	Display.cpp
	GameController.cpp
	LoadSaveState.cpp
	LogView.cpp
	SavestateButton.cpp
	Window.cpp
	VFileDevice.cpp)

qt5_wrap_ui(UI_FILES
	LoadSaveState.ui
	LogView.ui)

if(USE_GDB_STUB)
	set(SOURCE_FILES ${PLATFORM_SRC} ${SOURCE_FILES} GDBController.cpp GDBWindow.cpp)
endif()
set(MACOSX_BUNDLE_ICON_FILE mgba.icns)
set(MACOSX_BUNDLE_BUNDLE_VERSION ${LIB_VERSION_STRING})
set_source_files_properties(${CMAKE_SOURCE_DIR}/res/mgba.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

qt5_add_resources(RESOURCES resources.qrc)
message(STATUS ${RESOURCES})
add_executable(mGBA WIN32 MACOSX_BUNDLE main.cpp ${CMAKE_SOURCE_DIR}/res/mgba.icns ${SOURCE_FILES} ${UI_FILES} ${RESOURCES})

qt5_use_modules(mGBA Widgets Multimedia OpenGL)
target_link_libraries(mGBA ${PLATFORM_LIBRARY} ${OPENGL_LIBRARY} ${BINARY_NAME} Qt5::Widgets)
