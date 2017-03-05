add_subdirectory(Null)

if(USE_OPENGL)
  add_subdirectory(OGL)
  add_subdirectory(Software)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_subdirectory(D3D)
  add_subdirectory(D3D12)
endif()

if(NOT APPLE)
  add_subdirectory(Vulkan)
endif()
