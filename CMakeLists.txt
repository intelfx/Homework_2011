cmake_minimum_required(VERSION 2.6)
project(Antided)

INCLUDE(${PROJECT_SOURCE_DIR}/../../Common.cmake)
INCLUDE(${PROJECT_SOURCE_DIR}/../../Precompiled.cmake)

find_assert_library ("uXray" LIBRARIES)
find_assert_library ("pthread" THREADLIBS)

# Target specification
# ----
set (OVERALL_SRC Verifier.h Verifier.cpp)
main_add_gch(stdafx.h OVERALL_SRC)

set (STACK_SRC Stack.h Stack.cpp ProcessorTest.cpp Processor.cpp)
main_add_gch(stdafx.h STACK_SRC)

set(QUEUE_SRC Queue.cpp Queue.h MTServerClientApp/SharedServerClientProtocol.h)
main_add_gch(stdafx.h QUEUE_SRC)

set(QSERVER_SRC MTServerClientApp/InputServerApp.h MTQueueServer.cpp)
main_add_gch(stdafx.h QUEUE_SRC)

add_subdirectory(MTServerClientApp)

add_executable(antided_queue_server ${OVERALL_SRC} ${QUEUE_SRC} ${QSERVER_SRC})
target_link_libraries(antided_queue_server ${THREADLIBS} ${LIBRARIES})

add_executable(antided_stack ${STACK_SRC} ${OVERALL_SRC})
target_link_libraries(antided_stack ${LIBRARIES})

#file (GLOB VM_SRC mi_vm_*.cpp)
#MAIN_ADD_GCH(stdafx.h VM_SRC)

# add_executable(antided_vm ${VM_SRC})
# target_link_libraries(antided_vm ${LIBRARIES})
# ----
