project(minizip C)

set(SRCS ioapi.c unzip.c)

add_definitions(-DNOUNCRYPT)
if(UNIX)
  add_definitions(-D__USE_FILE_OFFSET64)
  add_definitions(-D__USE_LARGEFILE64)
  add_definitions(-D_LARGEFILE64_SOURCE)
  add_definitions(-D_FILE_OFFSET_BIT=64)
endif()

add_library(minizip STATIC ${SRCS})
target_include_directories(minizip PUBLIC .)

target_link_libraries(minizip PUBLIC z)

add_library(MiniZip::minizip ALIAS minizip)
