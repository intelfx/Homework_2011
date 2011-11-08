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

set(QS_SRC Queue.h Queue.cpp Server.cpp SharedServerClientProtocol.h InputServerApp.h)
main_add_gch(stdafx.h QS_SRC)

set(QKEYBR_SRC KeyboardHookReader.cpp)
main_add_gch(stdafx.h QKEYBR_SRC)

set(QTRANS_SRC ScrollLockTransmitter.cpp)
main_add_gch(stdafx.h QTRANS_SRC)

add_executable(antided_stack ${STACK_SRC} ${OVERALL_SRC})
target_link_libraries(antided_stack ${LIBRARIES})

add_executable(antided_queue_server ${QS_SRC} ${OVERALL_SRC})
target_link_libraries(antided_queue_server ${LIBRARIES} ${THREADLIBS})

add_library(keyboard_io ${QKEYBR_SRC} ${OVERALL_SRC})
target_link_libraries(keyboard_io ${LIBRARIES})

add_library(scroll_flasher ${QTRANS_SRC} ${OVERALL_SRC})
target_link_libraries(scroll_flasher ${LIBRARIES})

#file (GLOB VM_SRC mi_vm_*.cpp)
#MAIN_ADD_GCH(stdafx.h VM_SRC)

# add_executable(antided_vm ${VM_SRC})
# target_link_libraries(antided_vm ${LIBRARIES})
# ----
