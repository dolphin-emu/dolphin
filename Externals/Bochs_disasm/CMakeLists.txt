add_library(bdisasm STATIC
  dis_decode.cc
  dis_groups.cc
  resolve.cc
  syntax.cc
)

if (WIN32)
  target_sources(bdisasm
  PRIVATE
    stdafx.cc
  )
endif()

target_include_directories(bdisasm
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
