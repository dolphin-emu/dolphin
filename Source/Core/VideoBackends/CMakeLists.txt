add_subdirectory(OGL)
add_subdirectory(Null)
add_subdirectory(Software)
add_subdirectory(Vulkan)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_subdirectory(D3D)
endif()

