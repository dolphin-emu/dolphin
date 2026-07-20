add_library(FatFs STATIC
  FatFs/ff.c
  FatFs/ffunicode.c
  FatFs/diskio.h
  FatFs/ff.h
  FatFs/ffconf.h
)

target_include_directories(FatFs
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
