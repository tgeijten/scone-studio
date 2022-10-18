cmake_minimum_required(VERSION 3.10.0)

project(scone-studio)

# scone-studio options
option(SCONE_STUDIO_CPACK "Build SCONE Studio installer using CPack" OFF)

# To create a folder hierarchy within Visual Studio.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# compilation database for completion on Linux
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Place build products (libraries, executables) in root
# binary (build) directory. Otherwise, they get scattered around
# the build directory and so the dll's aren't next to the executables.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Silence CMake warning on OSX.
set(CMAKE_MACOSX_RPATH ON)

#
# Add targets
#
add_subdirectory(scone)

# Required packages
find_package(Qt5 COMPONENTS Widgets OpenGL PrintSupport REQUIRED)
include_directories("${CMAKE_SOURCE_DIR}/contrib/qcustomplot")
find_path(QTFX_PATH QOsgViewer.h "${CMAKE_SOURCE_DIR}/submodules/qtfx/qtfx")
include_directories( ${QTFX_PATH} )

find_package( OpenSceneGraph REQUIRED
	osg
	osgWidget
	osgQt
	osgGA
	osgViewer
	osgUtil
	osgDB
	osgShadow
	OpenThreads )
add_subdirectory("${CMAKE_SOURCE_DIR}/submodules/vis")

add_subdirectory(src/sconestudio)

enable_testing()