cmake_minimum_required(VERSION 2.6)
project(Antided)

INCLUDE(${PROJECT_SOURCE_DIR}/../../Common.cmake)
INCLUDE(${PROJECT_SOURCE_DIR}/../../Precompiled.cmake)

find_assert_library ("uXray" LIBRARIES)

# Target specification
# ----
set (STACK_NL_SRC Verifier.h Verifier.cpp Stack.h Stack.cpp Utility.h Interfaces.cpp Interfaces.h)
set (STACK_NL_SRC ${STACK_NL_SRC} MMU.h)
MAIN_ADD_GCH(stdafx.h STACK_NL_SRC)

add_executable(antided_stack_nl ${STACK_NL_SRC})
target_link_libraries(antided_stack_nl ${LIBRARIES})

set (STACK_SRC Verifier.h Verifier.cpp Stack.h Stack.cpp StackTest.cpp Processor.cpp)
MAIN_ADD_GCH(stdafx.h STACK_SRC)

add_executable(antided_stack ${STACK_SRC})
target_link_libraries(antided_stack ${LIBRARIES})

#file (GLOB VM_SRC mi_vm_*.cpp)
#MAIN_ADD_GCH(stdafx.h VM_SRC)

# add_executable(antided_vm ${VM_SRC})
# target_link_libraries(antided_vm ${LIBRARIES})
# ----
