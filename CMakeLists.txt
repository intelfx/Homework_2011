cmake_minimum_required(VERSION 2.6)
project(Homework)

INCLUDE(${PROJECT_SOURCE_DIR}/Common.cmake)
INCLUDE(${PROJECT_SOURCE_DIR}/Precompiled.cmake)

set_local_devel_tree (Libs)

find_assert_library ("uXray" LIBRARIES)

# Target specification
# ----
set (STACK_SRC Verifier.h Verifier.cpp Stack.h Stack.cpp ProcessorTest.cpp Processor.cpp)

set (STACK_NL_SRC Verifier.h Verifier.cpp Utility.h Interfaces.cpp Interfaces.h)
set (STACK_NL_SRC ${STACK_NL_SRC} Miscellaneous.cpp APIImplementation.cpp)
set (STACK_NL_SRC ${STACK_NL_SRC} MMU.h MMU.cpp Linker.cpp Linker.h AssemblyIO.cpp AssemblyIO.h)
set (STACK_NL_SRC ${STACK_NL_SRC} Logic.h Logic.cpp CommandSet_original.h CommandSet_original.cpp)
set (STACK_NL_SRC ${STACK_NL_SRC} Executor.h Executor.cpp Executor_int.h Executor_int.cpp)
set (STACK_NL_SRC ${STACK_NL_SRC} Executor_service.h Executor_service.cpp)
set (STACK_NL_SRC ${STACK_NL_SRC} NLUnitTests.cpp)

# MAIN_ADD_GCH(stdafx.h "${STACK_NL_SRC} ${STACK_SRC}")

add_executable(homework_stack_nl ${STACK_NL_SRC})
target_link_libraries(homework_stack_nl ${LIBRARIES})

add_executable(homework_stack ${STACK_SRC})
target_link_libraries(homework_stack ${LIBRARIES})

#file (GLOB VM_SRC mi_vm_*.cpp)
#MAIN_ADD_GCH(stdafx.h VM_SRC)

# add_executable(homework_vm ${VM_SRC})
# target_link_libraries(homework_vm ${LIBRARIES})
# ----
