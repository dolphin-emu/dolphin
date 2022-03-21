project(FatFs C)

add_library(FatFs STATIC ff.c ffunicode.c)
target_include_directories(FatFs
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
