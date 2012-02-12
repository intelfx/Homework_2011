cmake_minimum_required(VERSION 2.6)
project(uXray)

INCLUDE(${PROJECT_SOURCE_DIR}/../../Common.cmake)
INCLUDE(${PROJECT_SOURCE_DIR}/../../Precompiled.cmake)

# Target specification
# ----
find_assert_library ("dl" LIBRARIES)

aux_source_directory("." SOURCES)
# MAIN_ADD_GCH(stdafx.h ${SOURCES})

add_library(uXray ${SOURCES})
target_link_libraries(uXray ${LIBRARIES})
# ----
