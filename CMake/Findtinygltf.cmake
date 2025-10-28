find_path(tinygltf_INCLUDE_DIR tiny_gltf.h
  PATH_SUFFIXES tinygltf
)
mark_as_advanced(tinygltf_INCLUDE_DIR)

# STB is required by tinygltf
find_path(STB_INCLUDE_DIR stb_image.h
  PATH_SUFFIXES stb
)
mark_as_advanced(STB_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(tinygltf REQUIRED_VARS tinygltf_INCLUDE_DIR)

if(tinygltf_FOUND AND NOT TARGET tinygltf)
  add_library(tinygltf INTERFACE IMPORTED)

  set(tinygltf_includes "${tinygltf_INCLUDE_DIR}")
  if(STB_INCLUDE_DIR)
    list(APPEND tinygltf_includes "${STB_INCLUDE_DIR}")
  endif()

  set_target_properties(tinygltf PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${tinygltf_includes}"
  )
endif()
