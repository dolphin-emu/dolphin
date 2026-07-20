add_library(bdisasm STATIC
  bochs/dis_decode.cc
  bochs/dis_groups.cc
  bochs/resolve.cc
  bochs/syntax.cc
)
dolphin_disable_warnings(bdisasm)

if (WIN32)
  target_sources(bdisasm
  PRIVATE
    bochs/stdafx.cc
  )
endif()

target_include_directories(bdisasm
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
