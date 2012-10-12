cmake_minimum_required(VERSION 2.6)
project(Homework)

# Development tree selection
# ----
if(EXISTS ${PROJECT_SOURCE_DIR}/../../Common.cmake)

	# Development environment
	message ("Using normal development environment")
	INCLUDE(${PROJECT_SOURCE_DIR}/../../Common.cmake)
	INCLUDE(${PROJECT_SOURCE_DIR}/../../Precompiled.cmake)

else(EXISTS ${PROJECT_SOURCE_DIR}/../../Common.cmake)

	# Foreign environment
	message ("Using local copies")
	INCLUDE(Common.cmake)
	INCLUDE(Precompiled.cmake)
	set_local_devel_tree (Libs)

endif(EXISTS ${PROJECT_SOURCE_DIR}/../../Common.cmake)
# ----

# Library selection
# ----
find_assert_library ("uXray" LIBRARIES)
find_assert_library ("rt" LIBRARIES)
# ----

# Source specification
# ----
set (INTERPRETER_SRC Utility.h Interfaces.cpp Interfaces.h)
set (INTERPRETER_SRC ${INTERPRETER_SRC} Miscellaneous.cpp APIImplementation.cpp)
set (INTERPRETER_SRC ${INTERPRETER_SRC} MMU.h MMU.cpp Linker.cpp Linker.h AssemblyIO.cpp AssemblyIO.h)
set (INTERPRETER_SRC ${INTERPRETER_SRC} Logic.h Logic.cpp CommandSet_original.h CommandSet_original.cpp)
set (INTERPRETER_SRC ${INTERPRETER_SRC} Executor.h Executor.cpp Executor_int.h Executor_int.cpp)
set (INTERPRETER_SRC ${INTERPRETER_SRC} Executor_service.h Executor_service.cpp)
set (INTERPRETER_SRC ${INTERPRETER_SRC} x86Backend.cpp)
set (STACK_NL_SRC ${INTERPRETER_SRC} NLUnitTests.cpp)

MAIN_ADD_GCH(stdafx.h ${INTERPRETER_SRC})
# ----

# Target specification
# ----
# add_subdirectory(language_parser)
# add_subdirectory(x86_codegen)

add_executable(homework_stack_nl ${STACK_NL_SRC})
target_link_libraries(homework_stack_nl ${LIBRARIES})

add_library(interpreterplatform ${INTERPRETER_SRC})
target_link_libraries(interpreterplatform ${LIBRARIES})
# ----
