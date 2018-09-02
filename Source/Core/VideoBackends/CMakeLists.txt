add_subdirectory(OGL)
add_subdirectory(Null)
add_subdirectory(Software)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_subdirectory(D3D)
endif()

if(ENABLE_VULKAN)
  add_subdirectory(Vulkan)
endif()
