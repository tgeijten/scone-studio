cmake_minimum_required(VERSION 3.10.0)

project(scone-studio)

# To create a folder hierarchy within Visual Studio.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# compilation database for completion on Linux
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Various settings
# ----------------
# Place build products (libraries, executables) in root
# binary (build) directory. Otherwise, they get scattered around
# the build directory and so the dll's aren't next to the executables.
#get_filename_component(CONFIG_NAME "${CMAKE_BINARY_DIR}" NAME)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CONFIG_NAME}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Silence CMake warning on OSX.
set(CMAKE_MACOSX_RPATH ON)

#
# Add targets
#
add_subdirectory(scone)

find_package(Qt5 COMPONENTS Widgets OpenGL PrintSupport REQUIRED)
add_subdirectory(submodules/vis)
add_subdirectory(src/sconestudio)

enable_testing()