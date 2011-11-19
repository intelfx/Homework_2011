set(QPLUGIN_COMMON_SRC SharedServerClientProtocol.h InputServerApp.h)
main_add_gch(../stdafx.h QPLUGIN_COMMON_SRC)

set(QKEYBR_SRC KeyboardHookReader.cpp)
main_add_gch(../stdafx.h QKEYBR_SRC)

set(QTRANS_SRC ScrollLockTransmitter.cpp)
main_add_gch(../stdafx.h QTRANS_SRC)

add_library(keyboard_io ${QPLUGIN_COMMON_SRC} ${QKEYBR_SRC})
target_link_libraries(keyboard_io ${LIBRARIES})

add_library(scroll_flasher ${QPLUGIN_COMMON_SRC} ${QTRANS_SRC})
target_link_libraries(scroll_flasher ${LIBRARIES})
