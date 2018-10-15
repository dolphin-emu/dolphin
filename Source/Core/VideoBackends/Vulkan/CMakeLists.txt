# We require libX11-xcb when building for X11. This isn't required for any other backend, so test for it here.
if(USE_X11)
  find_package(X11_XCB)
  if (NOT X11_XCB_FOUND)
    message(FATAL_ERROR "libX11-xcb is required to build the Vulkan backend with X11.")
  endif()
endif()

message(STATUS "Enabling Vulkan video backend")

add_library(videovulkan
  BoundingBox.cpp
  CommandBufferManager.cpp
  FramebufferManager.cpp
  ObjectCache.cpp
  PerfQuery.cpp
  PostProcessing.cpp
  RasterFont.cpp
  Renderer.cpp
  ShaderCache.cpp
  ShaderCompiler.cpp
  StateTracker.cpp
  StagingBuffer.cpp
  StreamBuffer.cpp
  SwapChain.cpp
  Texture2D.cpp
  TextureCache.cpp
  TextureConverter.cpp
  Util.cpp
  VertexFormat.cpp
  VertexManager.cpp
  VKPipeline.cpp
  VKShader.cpp
  VKTexture.cpp
  VulkanContext.cpp
  VulkanLoader.cpp
  main.cpp
)

target_link_libraries(videovulkan
PUBLIC
  common
  videocommon

PRIVATE
  # Link against glslang, the other necessary libraries are referenced by the executable.
  glslang
  xxhash
)

# Only include the Vulkan headers when building the Vulkan backend
target_include_directories(videovulkan
PRIVATE
  ${CMAKE_SOURCE_DIR}/Externals/Vulkan/Include
)

# Silence warnings on glslang by flagging it as a system include
target_include_directories(videovulkan
SYSTEM PRIVATE
  ${CMAKE_SOURCE_DIR}/Externals/glslang/glslang/Public
  ${CMAKE_SOURCE_DIR}/Externals/glslang/SPIRV
)

# When building for X11, we need to link to libX11-xcb.
# The main executable doesn't need this library directly, so we have to add it here.
if(USE_X11)
  target_link_libraries(videovulkan PRIVATE X11::XCB)
endif()
