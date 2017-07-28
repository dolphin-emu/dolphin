set(SRCS
  BoundingBox.cpp
  CommandBufferManager.cpp
  FramebufferManager.cpp
  ObjectCache.cpp
  PerfQuery.cpp
  PostProcessing.cpp
  RasterFont.cpp
  Renderer.cpp
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

# Link against glslang, the other necessary libraries are referenced by the executable.
add_dolphin_library(videovulkan "${SRCS}" "${LIBS}")
target_link_libraries(videovulkan PRIVATE glslang)
