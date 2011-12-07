cmake_minimum_required(VERSION 2.6)
project(Homework)

INCLUDE(${PROJECT_SOURCE_DIR}/Common.cmake)
INCLUDE(${PROJECT_SOURCE_DIR}/Precompiled.cmake)

set_local_devel_tree (Libs)

find_assert_library ("uXray" LIBRARIES)
find_assert_library ("pthread" THREADLIBS)

# Target specification
# ----
set (OVERALL_SRC Verifier.h Verifier.cpp)
set (STACK_SRC Stack.h Stack.cpp ProcessorTest.cpp Processor.cpp)
set (QUEUE_SRC Queue.cpp Queue.h MTServerClientApp/SharedServerClientProtocol.h)
set (QSERVER_SRC MTServerClientApp/InputServerApp.h MTQueueServer.cpp)
set (LIST_SRC LinkedList.h LinkedList.cpp DoubleLinkedList.cpp DoubleLinkedList.h ListDemo.cpp)


main_add_gch(stdafx.h ${OVERALL_SRC} ${STACK_SRC} ${QUEUE_SRC} ${QSERVER_SRC} ${LIST_SRC})

add_subdirectory(MTServerClientApp)

add_executable(homework_queue_server ${OVERALL_SRC} ${QUEUE_SRC} ${QSERVER_SRC})
target_link_libraries(homework_queue_server ${THREADLIBS} ${LIBRARIES})

add_executable(homework_stack ${STACK_SRC} ${OVERALL_SRC})
target_link_libraries(homework_stack ${LIBRARIES})

add_executable(homework_list ${LIST_SRC})
target_link_libraries(homework_list ${LIBRARIES})

#file (GLOB VM_SRC mi_vm_*.cpp)
#MAIN_ADD_GCH(stdafx.h VM_SRC)

# add_executable(homework_vm ${VM_SRC})
# target_link_libraries(homework_vm ${LIBRARIES})
# ----
