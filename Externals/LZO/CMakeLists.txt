add_library(lzo2 STATIC
  minilzo.c
)

set_target_properties(lzo2 PROPERTIES FOLDER Externals)

target_include_directories(lzo2
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

disable_all_warnings(lzo2)
