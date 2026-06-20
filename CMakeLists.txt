cmake_minimum_required(VERSION 3.24)

# Set the triplet to static for any remaining vcpkg dependencies
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "" FORCE)

project(
	SimultaneousInput
	VERSION 1.5.2
	LANGUAGES CXX
)

# Enforce Dynamic MSVC Runtime (/MD)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# C++ Standard to 23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Read Starfield path from registry
cmake_host_system_information(
	RESULT Starfield_PATH
	QUERY WINDOWS_REGISTRY "HKLM/SOFTWARE/Microsoft/Windows/CurrentVersion/Uninstall/Steam App 1716740"
	VALUE "InstallLocation"
)

set(Starfield_PATH ${Starfield_PATH} CACHE PATH "Installed path of Starfield.")
set(GAME_DIR ${Starfield_PATH})

# Main Target
add_library(${PROJECT_NAME} SHARED)

# Collect source files
file(
	GLOB_RECURSE
	SOURCE_FILES
	LIST_DIRECTORIES false
	CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})

# Locate static libraries from CommonLibSF
find_library(COMMONLIBSF_LIB 
    NAMES CommonLibSF 
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/external/CommonLibSF/lib"
    REQUIRED
)

find_library(COMMONLIB_SHARED_LIB 
    NAMES commonlib-shared 
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/external/CommonLibSF/lib"
    REQUIRED
)

# Strict warnings for our code, but SYSTEM for external headers to ignore C4201 etc.
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/W4" "/WX")
endif()

# Verified Include Directories
# Marking external libraries as SYSTEM suppresses their warnings in MSVC.
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/external/CommonLibSF/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/CommonLibSF/lib/commonlib-shared/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/CommonLibSF/lib/commonlib-shared/src"
    "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include" # Explicit header-only access
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Linking against CommonLibSF and Windows System Libraries
# Note: spdlog is NOT linked here to avoid conflicts.
target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
	${COMMONLIBSF_LIB}
	${COMMONLIB_SHARED_LIB}
	advapi32
	bcrypt
	dbghelp
	dxgi
	d3d11
	d3dcompiler
	ole32
	shell32
	user32
	version
	ws2_32
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS
    SPDLOG_USE_STD_FORMAT
)

# MSVC Specific Options
if(MSVC)
	target_compile_options(
		${PROJECT_NAME}
		PRIVATE
		"/sdl"
		"/utf-8"
		"/Zi"
		"/permissive-"
		"/Zc:preprocessor"
		"/wd4200"
		"/FI\"REX/PCH.h\""           # Injected from commonlib-shared/src
		"/FI\"SFSE/Impl/PCH.h\""     # Injected from CommonLibSF/include
		"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)

	target_link_options(
		${PROJECT_NAME}
		PRIVATE
		"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
		"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif()

# Installation
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${GAME_DIR}/Data" CACHE PATH "Install path prefix" FORCE)
endif()

install(
	FILES
	"$<TARGET_FILE:${PROJECT_NAME}>"
	"$<TARGET_PDB_FILE:${PROJECT_NAME}>"
	DESTINATION "SFSE/Plugins"
	COMPONENT SFSEPlugin
)

include(CPack)
