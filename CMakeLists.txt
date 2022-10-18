cmake_minimum_required(VERSION 3.10.0)

project(scone-studio VERSION 2.1.0)

# Set LINUX variable
if(UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()

# scone-studio options
option(SCONE_STUDIO_CPACK "Build SCONE Studio installer using CPack" OFF)
if (LINUX)
	option(SCONE_STUDIO_CPACK_DEBIAN "Build debian installer using CPack" OFF)
endif()

if(SCONE_STUDIO_CPACK)
	file (STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/scone/.version" SCONE_REPOSITORY_REVISION)	
	set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}.${SCONE_REPOSITORY_REVISION}")
endif()

# To create a folder hierarchy within Visual Studio.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# compilation database for completion on Linux
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Place build products (libraries, executables) in root
# binary (build) directory. Otherwise, they get scattered around
# the build directory and so the dll's aren't next to the executables.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Set INSTALL directories
if(LINUX)
	set(CMAKE_INSTALL_RPATH "\$ORIGIN")
endif()

# Silence CMake warning on OSX.
set(CMAKE_MACOSX_RPATH ON)

#
# Add targets
#
add_subdirectory(scone)

# Required packages
find_package(Qt5 COMPONENTS Widgets OpenGL PrintSupport REQUIRED)
add_subdirectory(submodules/vis)
add_subdirectory(src/sconestudio)

enable_testing()

#
# Installation / packaging (CPack)
#
if (SCONE_STUDIO_CPACK)
	# package any required system libraries
	include(InstallRequiredSystemLibraries)

	# set necessary CPack variables
	set(CPACK_PACKAGE_NAME "scone-studio")
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
	set(CPACK_PACKAGE_VENDOR "Goatstream")
	set(CPACK_PACKAGE_CONTACT "info@goatstream.com")
	set(CPACK_PACKAGE_HOMEPAGE_URL "https://goatstream.com")
	set(CPACK_PACKAGE_DESCRIPTION "A tool for predictive musculoskeletal simulations")
	set(CPACK_PACKAGE_EXECUTABLES "sconestudio;SCONE Command Line Interface")

	# install scenarios
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scone/scenarios/Examples2" DESTINATION "./scone/scenarios")
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scone/scenarios/Tutorials2" DESTINATION "./scone/scenarios")
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scone/scenarios/SconePy" DESTINATION "./scone/scenarios")
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources" DESTINATION "./")

	# Windows install/packaging:
	if (WIN32)
		# packaging: use NSIS to package everything into a self-extracting installer
		set(CPACK_PACKAGE_INSTALL_DIRECTORY "scone-studio")
		set(CPACK_GENERATOR NSIS)
		set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/scone/resources/package/scone_icon_24.ico")

		# set(CPACK_NSIS_HELP_LINK ${CPACK_PACKAGE_HOMEPAGE_URL})
		set(CPACK_NSIS_CONTACT "info@goatstream.com")
		set(CPACK_NSIS_MODIFY_PATH OFF)
		set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
		set(CPACK_NSIS_CREATE_ICONS "CreateShortCut '\$SMPROGRAMS\\\\scone-studio\\\\scone-studio.lnk' '\$INSTDIR\\\\bin\\\\sconestudio.exe'")
	endif()

	if (UNIX AND NOT APPLE)
		if (SCONE_STUDIO_CPACK_DEBIAN)
			# packaging: package installation as a DEB
			set(CPACK_GENERATOR DEB)
			set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/scone-studio")
			set(CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6")
			set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
			set(CPACK_STRIP_FILES YES)

			# packaging: configure a script that creates a symlink /usr/local/bin/sconecmd --> /opt/osc/bin/sconecmd
			# configure_file("${PROJECT_SOURCE_DIR}/resources/package/postinst.in" "postinst" @ONLY)

			# packaging: configure a script that destroys the above symlink on uninstall
			# configure_file("${PROJECT_SOURCE_DIR}/resources/package/postrm.in" "postrm" @ONLY)

			# packaging: tell debian packager to use the scripts for postinst and postrm actions
			# set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_BINARY_DIR}/postinst;${CMAKE_BINARY_DIR}/postrm")
		else()
			# packaging: package installation as archive
			set(CPACK_GENERATOR TGZ ZIP)
			set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
			set(CPACK_PACKAGING_INSTALL_PREFIX "/scone-studio")
			set(CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6")
			set(CPACK_STRIP_FILES YES)
		endif()
	endif()

	# CPack vars etc. now fully configured, so include it
	include(CPack)
	
endif()
