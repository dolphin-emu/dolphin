add_library(lzo2 STATIC
  minilzo.c
)

target_include_directories(lzo2
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
