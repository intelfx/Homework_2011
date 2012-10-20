# Library selection
# ----
find_assert_library ("uXray" LIBRARIES)
find_assert_library ("rt" LIBRARIES)
find_assert_library ("pthread" LIBRARIES)

find_assert_library ("SDL" LIBRARIES)
find_assert_library ("SDL_gfx" LIBRARIES)
find_assert_library ("SDL_ttf" LIBRARIES)
# ----

# Source specification
# ----
set (SRCS main.cpp ssb.c)
# ----

# Target specification
# ----
add_executable(interpreterplatformtest ${SRCS})
add_dependencies(interpreterplatformtest interpreterplatform)
target_link_libraries(interpreterplatformtest ${LIBRARIES} interpreterplatform)
# ----

