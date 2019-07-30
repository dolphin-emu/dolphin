add_subdirectory(OGL)
add_subdirectory(Vulkan)

if(NOT ANDROID)
  add_subdirectory(Null)
  add_subdirectory(Software)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_subdirectory(D3DCommon)
  add_subdirectory(D3D)
  add_subdirectory(D3D12)
endif()

