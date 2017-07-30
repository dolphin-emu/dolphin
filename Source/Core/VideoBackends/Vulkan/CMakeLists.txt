set(SRCS
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
  StagingTexture2D.cpp
  StreamBuffer.cpp
  SwapChain.cpp
  Texture2D.cpp
  TextureCache.cpp
  TextureConverter.cpp
  Util.cpp
  VertexFormat.cpp
  VertexManager.cpp
  VKTexture.cpp
  VulkanContext.cpp
  VulkanLoader.cpp
  main.cpp
)

set(LIBS
  videocommon
  common
)

# Only include the Vulkan headers when building the Vulkan backend
include_directories(${CMAKE_SOURCE_DIR}/Externals/Vulkan/Include)

# Silence warnings on glslang by flagging it as a system include
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/Externals/glslang/glslang/Public)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/Externals/glslang/SPIRV)

# Link against glslang, the other necessary libraries are referenced by the executable.
add_dolphin_library(videovulkan "${SRCS}" "${LIBS}")
target_link_libraries(videovulkan PRIVATE glslang)

