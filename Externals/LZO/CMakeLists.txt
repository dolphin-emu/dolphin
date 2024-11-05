add_library(lzo2 STATIC
  minilzo.c
)
dolphin_disable_warnings(lzo2)

target_include_directories(lzo2
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
add_library(LZO::LZO ALIAS lzo2)
