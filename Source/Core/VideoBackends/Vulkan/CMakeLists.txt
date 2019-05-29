add_library(videovulkan
  BoundingBox.cpp
  BoundingBox.h
  CommandBufferManager.cpp
  CommandBufferManager.h
  Constants.h
  main.cpp
  ObjectCache.cpp
  ObjectCache.h
  PerfQuery.cpp
  PerfQuery.h
  Renderer.cpp
  Renderer.h
  ShaderCompiler.cpp
  ShaderCompiler.h
  StateTracker.cpp
  StateTracker.h
  StagingBuffer.cpp
  StagingBuffer.h
  StreamBuffer.cpp
  StreamBuffer.h
  SwapChain.cpp
  SwapChain.h
  VertexFormat.cpp
  VertexFormat.h
  VertexManager.cpp
  VertexManager.h
  VKPipeline.cpp
  VKPipeline.h
  VKShader.cpp
  VKShader.h
  VKTexture.cpp
  VKTexture.h
  VulkanContext.cpp
  VulkanContext.h
  VulkanLoader.cpp
  VulkanLoader.h
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
