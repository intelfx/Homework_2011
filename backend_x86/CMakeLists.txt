# Library selection
# ----
find_assert_library ("uXray" LIBRARIES)
find_assert_library ("rt" LIBRARIES)
# ----

# Source specification
# ----
set (X86BACKEND_SRC Backend.cpp Codegen.cpp)
# ----

# Target specification
# ----
add_library(interpreter_backend_x86 ${X86BACKEND_SRC})
target_link_libraries(interpreter_backend_x86 ${LIBRARIES})
# ----
