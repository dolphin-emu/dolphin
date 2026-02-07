add_library(libigl INTERFACE IMPORTED GLOBAL)
set_target_properties(libigl PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}/libigl/include
)
