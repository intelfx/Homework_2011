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

	if(NOT EXISTS ${${_libname}_LIBRARY})
		message(FATAL_ERROR ${_libname} " not found!")
	endif()

	SET(${_liblist} ${${_liblist}} ${${_libname}_LIBRARY})
endmacro()

# Adds a custom library directory
macro(add_library_group _group)
	if (EXISTS ${FX_PROJECT_BASE}/${_group})
		include_directories (${FX_PROJECT_BASE}/${_group})
	endif()
endmacro()

macro(add_external_sdk _sdkdir)
	if (EXISTS ${FX_PROJECT_BASE}/SDK/${_sdkdir}/include)
		include_directories(${FX_PROJECT_BASE}/SDK/${_sdkdir}/include)
	endif()

	if (EXISTS ${FX_PROJECT_BASE}/SDK/${_sdkdir}/lib)
		link_directories(${FX_PROJECT_BASE}/SDK/${_sdkdir}/lib)
	endif()
endmacro()

# Reconfigures all paths based on development tree modesetting
macro (reconfigure_devel_tree)
	# CMake pathes
	SET(FX_STATIC_DIR ${FX_PROJECT_BASE}/__lib)
	SET(FX_EXECUTABLE_DIR ${FX_PROJECT_BASE}/__bin)

	SET(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${FX_EXECUTABLE_DIR} ${FX_STATIC_DIR})

	SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${FX_EXECUTABLE_DIR})
	SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${FX_EXECUTABLE_DIR})
	SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${FX_EXECUTABLE_DIR})

# For outdated or buggy CMake parsers like KDev's one
	SET(EXECUTABLE_OUTPUT_PATH ${FX_EXECUTABLE_DIR})
	SET(LIBRARY_OUTPUT_PATH ${FX_EXECUTABLE_DIR})

# Compiler pathes
	include_directories(${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR})
	link_directories(${FX_EXECUTABLE_DIR})
endmacro()

# Sets normal development tree mode
macro (set_normal_devel_tree)
	SET(FX_PROJECT_BASE ${PROJECT_SOURCE_DIR}/../..)

	add_library_group (Common)
	add_library_group (Libs)
	reconfigure_devel_tree()
endmacro()

# Sets local development tree mode
macro (set_local_devel_tree dirname)
	SET(FX_PROJECT_BASE ${PROJECT_SOURCE_DIR})

	add_library_group (${dirname})
	reconfigure_devel_tree()
endmacro()


# ----

# Directories and setup
# ----
SET(FX_C_ARGS "-std=c99 -Wold-style-definition -Wstrict-prototypes")
SET(FX_CXX_ARGS "-std=c++0x -Wold-style-cast -Woverloaded-virtual -fvisibility-inlines-hidden -fno-pretty-templates")
SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Wall -Wextra -Wmain -Wswitch-enum -Wswitch-default")
SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Wmissing-include-dirs -Wstrict-overflow=4 -Wpointer-arith")
SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Wunreachable-code -Wundef")
SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Wcast-align -Wwrite-strings -Wcast-qual")
SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Wredundant-decls -Winit-self -Wshadow -Wabi -Wstrict-aliasing")
SET(FX_WARNING_ARGS "${FX_WARNING_ARGS} -Winvalid-pch")

SET(FX_TUNE_ARGS	"-fabi-version=5 -fvisibility=hidden")
SET(FX_TUNE_ARGS	"${FX_TUNE_ARGS} -ftrapv -fuse-linker-plugin -pipe")
SET(FX_OPT_ARGS		"-O3 -flto -fno-enforce-eh-specs -fstrict-aliasing -fipa-struct-reorg")
SET(FX_OPT_ARGS		"${FX_OPT_ARGS} -fipa-pta -fipa-matrix-reorg -floop-interchange -floop-flatten")
SET(FX_OPT_ARGS		"${FX_OPT_ARGS} -floop-block -floop-strip-mine -ftree-loop-distribution")
SET(FX_OPT_ARGS		"${FX_OPT_ARGS} -funsafe-loop-optimizations -ftree-parallelize-loops=2")
SET(FX_INSTR_ARGS	"-mfpmath=both -march=native")
SET(FX_LD_OPT_ARGS	"-Wl,-O1")
SET(FX_DBG_ARGS		"")
SET(FX_LD_FLAGS		"-Wl,--sort-common")
# ----

# CMake flags
# ----
SET(BUILD_SHARED_LIBS ON)
SET(CMAKE_STRIP ON)
SET(CMAKE_USE_RELATIVE_PATHS ON)
# ----


# Compiler configuration
# ----
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FX_CXX_ARGS} ${FX_WARNING_ARGS} ${FX_TUNE_ARGS}")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${FX_OPT_ARGS} ${FX_INSTR_ARGS}")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${FX_DBG_ARGS}")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${FX_OPT_ARGS}")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FX_C_ARGS} ${FX_WARNING_ARGS} ${FX_TUNE_ARGS}")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${FX_OPT_ARGS}")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${FX_DBG_ARGS}")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${FX_OPT_ARGS}")

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FX_LD_FLAGS}")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${FX_OPT_ARGS} ${FX_INSTR_ARGS} ${FX_LD_OPT_ARGS}")
SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${FX_OPT_ARGS} ${FX_LD_OPT_ARGS}")

SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${FX_LD_FLAGS}")
SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${FX_OPT_ARGS} ${FX_INSTR_ARGS} ${FX_LD_OPT_ARGS}")
SET(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${FX_OPT_ARGS} ${FX_LD_OPT_ARGS}")

SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${FX_LD_FLAGS}")
SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${FX_OPT_ARGS}  ${FX_LD_OPT_ARGS}")
SET(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "${FX_OPT_ARGS} ${FX_LD_OPT_ARGS}")

set_directory_properties(PROPERTIES COMPILE_DEFINITIONS_RELEASE NDEBUG)
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO NDEBUG)
# ----

# Default build tree configuration
# ----
set_normal_devel_tree()
# ----
