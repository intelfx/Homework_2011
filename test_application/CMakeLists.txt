# Library selection
# ----
find_library (LIB_SDL SDL)
find_library (LIB_SDL_gfx SDL_gfx)
find_library (LIB_SDL_ttf SDL_ttf)
# ----

if (LIB_SDL AND LIB_SDL_gfx AND LIB_SDL_ttf)

	# Source specification
	# ----
	set (SRCS main.cpp ssb.c)
	# ----

	# Target specification
	# ----
	add_executable(interpreterplatformtest ${SRCS})
	target_link_libraries(interpreterplatformtest ${LIBRARIES} ${LIB_SDL} ${LIB_SDL_gfx} ${LIB_SDL_ttf} interpreterplatform)
	# ----

endif (LIB_SDL AND LIB_SDL_gfx AND LIB_SDL_ttf)
