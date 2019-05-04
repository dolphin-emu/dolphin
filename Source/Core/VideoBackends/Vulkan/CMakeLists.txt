add_library(videovulkan
  BoundingBox.h
  BoundingBox.cpp
  CommandBufferManager.h
  CommandBufferManager.cpp
  Constants.h
  ObjectCache.h
  ObjectCache.cpp
  PerfQuery.h
  PerfQuery.cpp
  Renderer.h
  Renderer.cpp
  ShaderCompiler.h
  ShaderCompiler.cpp
  StateTracker.h
  StateTracker.cpp
  StagingBuffer.h
  StagingBuffer.cpp
  StreamBuffer.h
  StreamBuffer.cpp
  SwapChain.h
  SwapChain.cpp
  VertexFormat.h
  VertexFormat.cpp
  VertexManager.h
  VertexManager.cpp
  VideoBackend.h
  VKPipeline.h
  VKPipeline.cpp
  VKShader.h
  VKShader.cpp
  VKTexture.h
  VKTexture.cpp
  VulkanContext.h
  VulkanContext.cpp
  VulkanLoader.h
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

set_target_properties(videovulkan PROPERTIES
  FOLDER "Dolphin Core/Video Backends"
  PROJECT_LABEL "Vulkan"
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
