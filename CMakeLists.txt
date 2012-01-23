cmake_minimum_required(VERSION 2.6)
project(Homework)

INCLUDE(${PROJECT_SOURCE_DIR}/Common.cmake)
INCLUDE(${PROJECT_SOURCE_DIR}/Precompiled.cmake)

set_local_devel_tree (Libs)

find_assert_library ("uXray" LIBRARIES)
find_assert_library ("pthread" THREADLIBS)
find_assert_library ("rt" LIBRARIES)

# Target specification
# ----
set (OVERALL_SRC Verifier.h Verifier.cpp)
set (STACK_SRC Stack.h Stack.cpp ProcessorTest.cpp Processor.cpp)
set (QUEUE_SRC Queue.cpp Queue.h MTServerClientApp/SharedServerClientProtocol.h)
set (QSERVER_SRC MTServerClientApp/InputServerApp.h MTQueueServer.cpp)
set (LIST_SRC LinkedList.h LinkedList.cpp DoubleLinkedList.cpp DoubleLinkedList.h)
set (LISTDEMO_SRC ListDemo.cpp)
set (HTABLE_SRC Hashtable.h Hashtable.cpp)
set (HASHTEST_SRC Hashtest.cpp)
set (HASHDICT_SRC Dictionary.cpp DictOpImpl.h DictOpImpl.cpp DictReader.h DictOperations.h DictStructures.h)

# set (HUFFMAN_SRC Huffman.cpp Huffman.h)

add_subdirectory(MTServerClientApp)

main_add_gch(stdafx.h "${OVERALL_SRC} ${STACK_SRC} ${QUEUE_SRC} ${QSERVER_SRC} ${LIST_SRC} ${HTABLE_SRC} ${HUFFMAN_SRC}")

# add_executable(homework_huffman ${OVERALL_SRC} ${HUFFMAN_SRC})
# target_link_libraries(homework_huffman ${THREADLIBS} ${LIBRARIES})

add_executable(homework_queue_server ${OVERALL_SRC} ${QUEUE_SRC} ${QSERVER_SRC})
target_link_libraries(homework_queue_server ${THREADLIBS} ${LIBRARIES})

add_executable(homework_stack ${STACK_SRC} ${OVERALL_SRC})
target_link_libraries(homework_stack ${LIBRARIES})

add_executable(homework_list ${LIST_SRC} ${LISTDEMO_SRC})
target_link_libraries(homework_list ${LIBRARIES})

add_executable(homework_hash ${HTABLE_SRC} ${LIST_SRC} ${HASHTEST_SRC})
target_link_libraries(homework_hash ${LIBRARIES})

add_executable(homework_dict ${HTABLE_SRC} ${LIST_SRC} ${HASHDICT_SRC})
target_link_libraries(homework_dict ${LIBRARIES})

#file (GLOB VM_SRC mi_vm_*.cpp)
#MAIN_ADD_GCH(stdafx.h VM_SRC)

# add_executable(homework_vm ${VM_SRC})
# target_link_libraries(homework_vm ${LIBRARIES})
# ----
