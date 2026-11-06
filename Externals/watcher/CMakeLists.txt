add_library(watcher INTERFACE IMPORTED GLOBAL)

if(WIN32)
  check_cxx_compiler_flag(-fexceptions HAS_FEXCEPTIONS)
  if(HAS_FEXCEPTIONS)
    target_compile_options(watcher INTERFACE -fexceptions)
  endif()
endif()

set_target_properties(watcher PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}/watcher/include
)
