
# Development tree selection
# ----
if(NOT CMAKE_PROJECT_NAME)
	project(Homework_2011)
	cmake_minimum_required(VERSION 2.6)

	if(EXISTS ${PROJECT_SOURCE_DIR}/../../Common.cmake)

		# Development environment
		message ("Using normal development environment")
		INCLUDE(${PROJECT_SOURCE_DIR}/../../Common.cmake)
		INCLUDE(${PROJECT_SOURCE_DIR}/../../Precompiled.cmake)
		set_normal_devel_tree()

		find_assert_library ("uXray" LIBRARIES)

	else(EXISTS ${PROJECT_SOURCE_DIR}/../../Common.cmake)

		# Foreign environment
		message ("Using local copies")
		INCLUDE(Common.cmake)
		INCLUDE(Precompiled.cmake)
		set_local_devel_tree (Libs)

		find_assert_library ("uXray" LIBRARIES)

	endif(EXISTS ${PROJECT_SOURCE_DIR}/../../Common.cmake)

else(NOT CMAKE_PROJECT_NAME)

	message ("Building for production")
	INCLUDE(Common.cmake)
	INCLUDE(Precompiled.cmake)

	add_subdirectory (Libs/uXray)
	list (APPEND LIBRARIES uXray)

endif(NOT CMAKE_PROJECT_NAME)
# ----

# Library selection
# ----
# ----

# Source specification
# ----
set (INTERPRETER_SRC Utility.h Interfaces.cpp Interfaces.h)
set (INTERPRETER_SRC ${INTERPRETER_SRC} Miscellaneous.cpp APIImplementation.cpp)
set (INTERPRETER_SRC ${INTERPRETER_SRC} MMU.h MMU.cpp Linker.cpp Linker.h AssemblyIO.cpp AssemblyIO.h)
set (INTERPRETER_SRC ${INTERPRETER_SRC} BytecodeIO.cpp BytecodeIO.h)
set (INTERPRETER_SRC ${INTERPRETER_SRC} Logic.h Logic.cpp CommandSet_original.h CommandSet_original.cpp)
set (INTERPRETER_SRC ${INTERPRETER_SRC} Executor.h Executor.cpp Executor_int.h Executor_int.cpp)
set (INTERPRETER_SRC ${INTERPRETER_SRC} Executor_service.h Executor_service.cpp)

MAIN_ADD_GCH(stdafx.h ${INTERPRETER_SRC})
# ----

# Target specification
# ----
# add_subdirectory(language_parser)

# TODO: determine the backend dynamically.
set(BACKEND "" CACHE STRING "JIT backend name to build. Currently suppored: x86.")

if(BACKEND)
	set(BACKEND_DIR backend_${BACKEND})
	add_subdirectory(${BACKEND_DIR})

	set(BACKEND_TARGET interpreter_${BACKEND_DIR})
	list(APPEND LIBRARIES ${BACKEND_TARGET})
else(BACKEND)
	add_definitions(-DNEED_STUB_BACKEND)
endif(BACKEND)

add_library(interpreterplatform ${INTERPRETER_SRC})
target_link_libraries(interpreterplatform ${LIBRARIES})
# ----

add_subdirectory(test_application EXCLUDE_FROM_ALL)
