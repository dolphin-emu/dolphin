add_library(lzo2 STATIC
  minilzo.c
)
dolphin_disable_warnings_msvc(lzo2)

target_include_directories(lzo2
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
