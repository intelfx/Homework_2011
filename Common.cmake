# ----
# Main common CMake configuration file for Ifx Labs workspace
# Ifx Labs Development Environment
# ----

# CMake Modules
# ----
include (FindPkgConfig)
# ----

# Macros
# ----

# Helper macro to find a library in the default paths.
macro(find_assert_library _libname _liblist)
	find_library(${_libname}_LIBRARY ${_libname})

	if(NOT ${_libname}_LIBRARY)
		message(FATAL_ERROR "*** ${_libname} not found!")
	endif()

	SET(${_liblist} ${${_liblist}} ${${_libname}_LIBRARY})
endmacro()

macro(_add_include_directory _dir)
	if (EXISTS ${_dir})
		message(STATUS "Adding an include directory: ${_dir}")

		# For find_*
		list (APPEND CMAKE_INCLUDE_PATH ${_dir})
		# For native path resolution
		include_directories (${_dir})
	endif()
endmacro()

# Adds a custom library directory
macro(add_library_group _group)
	_add_include_directory (${FX_PROJECT_BASE}/${_group})
endmacro()

# Adds a custom library (include) direcory rel. to project tree
macro(add_fixed_library_group _group)
	_add_include_directory (${CMAKE_CURRENT_SOURCE_DIR}/${_group})
endmacro()

# Adds a custom SDK (include/lib) directory
macro(add_external_sdk _sdkdir)
	if (EXISTS ${FX_PROJECT_BASE}/SDK/${_sdkdir})
		set (SDK_${_sdkdir} TRUE)

		# For find_*
		list (APPEND CMAKE_PREFIX_PATH ${FX_PROJECT_BASE}/SDK/${_sdkdir})
		# For native relative path resolution
		include_directories (${FX_PROJECT_BASE}/SDK/${_sdkdir}/include)
		link_directories (${FX_PROJECT_BASE}/SDK/${_sdkdir}/lib)
	endif()
endmacro()

# Reconfigures all paths based on development tree modesetting
macro (reconfigure_devel_tree)
	# Default pathes
	SET(FX_STATIC_DIR ${FX_OUTPUT_BASE}/__lib)
	SET(FX_EXECUTABLE_DIR ${FX_OUTPUT_BASE}/__bin)

	# Let find_library() search in "__lib" and "__bin"
	LIST(APPEND CMAKE_LIBRARY_PATH ${FX_EXECUTABLE_DIR} ${FX_STATIC_DIR})

	# Let executable files be placed in "__bin"
	# Let shared libraries be placed in "__bin"
	# Let static libraries be placed in "__lib"
	SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${FX_EXECUTABLE_DIR})
	SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${FX_EXECUTABLE_DIR})
	SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${FX_STATIC_DIR})

	# For outdated or buggy CMake parsers like KDev's one
	SET(EXECUTABLE_OUTPUT_PATH ${FX_EXECUTABLE_DIR})
	SET(LIBRARY_OUTPUT_PATH ${FX_EXECUTABLE_DIR})

	# Allow compiler to search library files in output directories
	link_directories (${FX_EXECUTABLE_DIR} ${FX_STATIC_DIR})
endmacro()

# Sets normal development tree mode
macro (set_normal_devel_tree)
	SET(FX_PROJECT_BASE ${PROJECT_SOURCE_DIR}/../..)
	SET(FX_OUTPUT_BASE ${FX_PROJECT_BASE})

	add_library_group (Libs)
	reconfigure_devel_tree()
endmacro()

# Sets local development tree mode
macro (set_local_devel_tree)
	SET(FX_PROJECT_BASE ${PROJECT_SOURCE_DIR})
	SET(FX_OUTPUT_BASE ${FX_PROJECT_BASE})

	add_library_group (Libs)
	reconfigure_devel_tree()
endmacro()

macro (set_production_devel_tree)
	SET(FX_PROJECT_BASE ${CMAKE_CURRENT_SOURCE_DIR})

	add_library_group (Libs)
	include (CMakeConfigureProduction.cmake OPTIONAL) # This file shall contain calls to add_production_build_library[_notarget] for all used libs
endmacro()

# Add a library in $1 and recursively handle its dependencies.
# Do not add any targets.
macro (add_production_build_library_custom _lib)
	# Recursively handle the subdir's dependencies.
	_add_include_directory (${CMAKE_CURRENT_LIST_DIR}/${_lib}/Libs)
	message(STATUS "Going to walk: ${CMAKE_CURRENT_LIST_DIR}/${_lib}/CMakeConfigureProduction.cmake")
	include (${CMAKE_CURRENT_LIST_DIR}/${_lib}/CMakeConfigureProduction.cmake OPTIONAL) # This will change CMAKE_CURRENT_LIST_DIR

	if (NOT IN_add_production_build_library)
		message(STATUS "Going to add subdir: ${CMAKE_CURRENT_LIST_DIR}/${_lib}")
		set (IN_add_production_build_library TRUE)
		add_subdirectory (${CMAKE_CURRENT_LIST_DIR}/${_lib})
		unset (IN_add_production_build_library)
		message(STATUS "Completed adding subdir: ${CMAKE_CURRENT_LIST_DIR}/${_lib}")
	endif (NOT IN_add_production_build_library)
endmacro()

# Wrapper: add a library in Libs/$1, recursively handle its dependencies
# and add the target called $1 into current project's library list.
macro (add_production_build_library _lib)
	add_production_build_library_custom (Libs/${_lib})
	list (APPEND LIBRARIES ${_lib})
endmacro()
# ----

# Determine compiler and system
# ----
if (NOT ${CMAKE_C_COMPILER_ID} STREQUAL ${CMAKE_CXX_COMPILER_ID})
	message(FATAL_ERROR "C and CXX compiler identifications do not match, cannot continue for now")
endif(NOT ${CMAKE_C_COMPILER_ID} STREQUAL ${CMAKE_CXX_COMPILER_ID})

SET(FX_COMPILER_ID ${CMAKE_C_COMPILER_ID})
# ----


# Compiler arguments
# ----

# Common things
if ("${FX_COMPILER_ID}" STREQUAL "Clang" OR "${FX_COMPILER_ID}" STREQUAL "GNU")

	SET(FX_C_ARGS "-std=c99 -Wold-style-definition -Wstrict-prototypes")
	SET(FX_CXX_ARGS "-std=c++11 -Wold-style-cast -Woverloaded-virtual -fvisibility-inlines-hidden -Wnoexcept -Wzero-as-null-pointer-constant")
	SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Wall -Wextra -Wmain -Wswitch-enum -Wswitch-default")
	SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Wmissing-include-dirs -Wstrict-overflow=4 -Wpointer-arith")
	SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Wunreachable-code -Wundef -Wunsafe-loop-optimizations")
	SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Wcast-align -Wwrite-strings -Wcast-qual")
	SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Wredundant-decls -Winit-self -Wshadow -Wabi -Wstrict-aliasing")
	SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Winvalid-pch -pedantic -Wno-variadic-macros -Wno-format")

	SET(FX_TUNE_ARGS	"${FX_TUNE_ARGS} -fvisibility=hidden")

	SET(FX_LD_OPT_ARGS	"-Wl,-O1")

endif ("${FX_COMPILER_ID}" STREQUAL "Clang" OR "${FX_COMPILER_ID}" STREQUAL "GNU")

# GCC-specific things
if ("${FX_COMPILER_ID}" STREQUAL "GNU")

	# Already enabled on MinGW
	if (NOT WIN32)
		SET(FX_TUNE_ARGS "-fPIC")
	endif (NOT WIN32)

	SET(FX_TUNE_ARGS	"${FX_TUNE_ARGS} -fabi-version=6")
	SET(FX_TUNE_ARGS	"${FX_TUNE_ARGS} -ftrapv -pipe")
	SET(FX_OPT_ARGS		"-O3 -fno-enforce-eh-specs -fnothrow-opt -fstrict-aliasing -fipa-struct-reorg")
	SET(FX_OPT_ARGS		"${FX_OPT_ARGS} -fipa-pta -fipa-matrix-reorg -funsafe-loop-optimizations")

	# Unsupported by MinGW
	if (NOT WIN32)
		SET(FX_OPT_ARGS		"${FX_OPT_ARGS} -floop-block -floop-strip-mine -ftree-loop-distribution")
		SET(FX_OPT_ARGS		"${FX_OPT_ARGS} -floop-interchange -floop-flatten -ftree-parallelize-loops=2")
		SET(FX_OPT_ARGS		"${FX_OPT_ARGS} -frepo -flto")
		SET(FX_LD_OPT_ARGS	"${FX_LD_OPT_ARGS} -frepo -flto")
	endif (NOT WIN32)

	if (NOT ANDROID)
		SET(FX_TUNE_ARGS "${FX_TUNE_ARGS} -fuse-linker-plugin")
	endif (NOT ANDROID)

	SET(FX_INSTR_ARGS	"-mfpmath=both -march=native")
	SET(FX_DBG_ARGS		"")
	SET(FX_LD_FLAGS		"")

	# Settings for MinGW
	if (WIN32)
# 		SET(FX_LD_FLAGS "${FX_LD_FLAGS} -static")
	endif (WIN32)

endif ("${FX_COMPILER_ID}" STREQUAL "GNU")

# Clang-specific things
if ("${FX_COMPILER_ID}" STREQUAL "Clang")

	SET(FX_TUNE_ARGS	"${FX_TUNE_ARGS} -fvisibility=hidden")
	SET(FX_TUNE_ARGS	"${FX_TUNE_ARGS} -ftrapv -pipe")
	SET(FX_OPT_ARGS		"-O4 -emit-llvm")
	SET(FX_LD_OPT_ARGS	"-Wl,-O1 -flto")

	if (NOT ANDROID)
		SET(FX_LD_TUNE_ARGS "${FX_LD_TUNE_ARGS} -fuse-linker-plugin")
	endif (NOT ANDROID)

	SET(FX_INSTR_ARGS	"-march=native")
	SET(FX_DBG_ARGS		"")
	SET(FX_LD_FLAGS		"")

endif ("${FX_COMPILER_ID}" STREQUAL "Clang")

# TODO MSVC compiler arguments
# ----


# CMake flags
# ----
SET(BUILD_SHARED_LIBS ON)
SET(CMAKE_STRIP ON)
SET(CMAKE_USE_RELATIVE_PATHS ON)
SET(CMAKE_INCLUDE_CURRENT_DIR ON)

SET(CMAKE_SKIP_BUILD_RPATH TRUE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
SET(CMAKE_INSTALL_RPATH ".")
# ----


# Compiler configuration
# ----
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FX_CXX_ARGS} ${FX_WARNING_ARGS} ${FX_TUNE_ARGS}")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${FX_OPT_ARGS} ${FX_INSTR_ARGS}")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${FX_DBG_ARGS}")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${FX_OPT_ARGS} ${FX_DBG_ARGS}")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FX_C_ARGS} ${FX_WARNING_ARGS} ${FX_TUNE_ARGS}")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${FX_OPT_ARGS} ${FX_INSTR_ARGS}")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${FX_DBG_ARGS}")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${FX_OPT_ARGS} ${FX_DBG_ARGS}")

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FX_LD_FLAGS} ${FX_LD_TUNE_ARGS}")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${FX_LD_OPT_ARGS}")
SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ${FX_LD_OPT_ARGS}")

SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${FX_LD_FLAGS} ${FX_LD_TUNE_ARGS}")
SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} ${FX_LD_OPT_ARGS}")
SET(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} ${FX_LD_OPT_ARGS}")

SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${FX_LD_FLAGS} ${FX_LD_TUNE_ARGS}")
SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${FX_LD_OPT_ARGS}")
SET(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO} ${FX_LD_OPT_ARGS}")

set_directory_properties (PROPERTIES COMPILE_DEFINITIONS_RELEASE NDEBUG)
set_directory_properties (PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO NDEBUG)
# ----


# Build-time macros
# ----

if (UNIX)
	message(STATUS "Project configuration: system is UNIX")
	add_definitions(-DTARGET_POSIX)
endif (UNIX)

if (WIN32)
	message(STATUS "Project configuration: system is Win32")
	add_definitions(-DTARGET_WINDOWS)
endif (WIN32)

if (ANDROID)
	message(STATUS "Project configuration: system is Android")
	add_definitions(-DTARGET_ANDROID)
endif (ANDROID)

if (MSVC)
	message(STATUS "Project configuration: compiler is MSVC")
	add_definitions(-DCOMPILER_MSVC)
endif (MSVC)

if ("${FX_COMPILER_ID}" STREQUAL "GNU")
	message(STATUS "Project configuration: compiler is GNUC")
	add_definitions(-DCOMPILER_GNUC)
endif ("${FX_COMPILER_ID}" STREQUAL "GNU")

if ("${FX_COMPILER_ID}" STREQUAL "Clang")
	message(STATUS "Project configuration: compiler is Clang")
	add_definitions(-DCOMPILER_GNUC)
endif ("${FX_COMPILER_ID}" STREQUAL "Clang")

if (ARMEABI)
	message(STATUS "Project configuration: architecture is ARM/EABI")
	add_definitions(-DARMEABI)
endif (ARMEABI)

if (ARMEABI_V7A)
	message(STATUS "Project configuration: architecture is ARM/EABI v7A")
	add_definitions(-DARMEABI_V7A)
endif (ARMEABI_V7A)

add_definitions(-DSIZEOF_PVOID=${CMAKE_SIZEOF_VOID_P})

# ----
