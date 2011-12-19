cmake_minimum_required(VERSION 2.6)
project(Homework)

INCLUDE(${PROJECT_SOURCE_DIR}/Common.cmake)
INCLUDE(${PROJECT_SOURCE_DIR}/Precompiled.cmake)

set_local_devel_tree (Libs)

find_assert_library ("uXray" LIBRARIES)

# Target specification
# ----
set (STACK_NL_SRC Verifier.h Verifier.cpp Stack.h Stack.cpp Utility.h Interfaces.cpp Interfaces.h Miscellaneous.cpp)
set (STACK_NL_SRC ${STACK_NL_SRC} MMU.h MMU.cpp Linker.cpp Linker.h AssemblyIO.cpp AssemblyIO.h APIImplementation.cpp)
set (STACK_NL_SRC ${STACK_NL_SRC} Logic.h Logic.cpp)
set (STACK_NL_SRC ${STACK_NL_SRC} CommandSet_original.h CommandSet_original.cpp NLUnitTests.cpp)
MAIN_ADD_GCH(stdafx.h STACK_NL_SRC)

add_executable(homework_stack_nl ${STACK_NL_SRC})
target_link_libraries(homework_stack_nl ${LIBRARIES})

set (STACK_SRC Verifier.h Verifier.cpp Stack.h Stack.cpp ProcessorTest.cpp Processor.cpp)
MAIN_ADD_GCH(stdafx.h STACK_SRC)

add_executable(homework_stack ${STACK_SRC})
target_link_libraries(homework_stack ${LIBRARIES})

#file (GLOB VM_SRC mi_vm_*.cpp)
#MAIN_ADD_GCH(stdafx.h VM_SRC)

# add_executable(homework_vm ${VM_SRC})
# target_link_libraries(homework_vm ${LIBRARIES})
# ----
