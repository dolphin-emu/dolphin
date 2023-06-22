project(xxhash C)

add_library(xxhash STATIC xxhash.c)
dolphin_disable_warnings_msvc(xxhash)
target_include_directories(xxhash
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
