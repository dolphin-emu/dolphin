project(xxhash C)

add_library(xxhash STATIC xxhash.c)
target_include_directories(xxhash
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(xxhash PROPERTIES FOLDER Externals)
disable_all_warnings(xxhash)
