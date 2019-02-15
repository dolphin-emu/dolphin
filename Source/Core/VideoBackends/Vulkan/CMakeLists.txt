add_library(videovulkan
  BoundingBox.cpp
  CommandBufferManager.cpp
  ObjectCache.cpp
  PerfQuery.cpp
  Renderer.cpp
  ShaderCompiler.cpp
  StateTracker.cpp
  StagingBuffer.cpp
  StreamBuffer.cpp
  SwapChain.cpp
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
