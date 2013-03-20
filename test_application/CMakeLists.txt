# Library selection
# ----
# find_library (LIB_SDL SDL)
# find_library (LIB_SDL_gfx SDL_gfx)
# find_library (LIB_SDL_ttf SDL_ttf)
find_library (LIB_pthread pthread)
# ----

# Source specification
# ----
set (SRCS main.cpp)
# ----

# Target specification
# ----
add_executable(interpreterplatformtest ${SRCS})
target_link_libraries(interpreterplatformtest ${LIBRARIES} ${LIB_pthread} interpreterplatform)
# ----
