add_library(videovulkan
  CommandBufferManager.cpp
  CommandBufferManager.h
  Constants.h
  ObjectCache.cpp
  ObjectCache.h
  ShaderCompiler.cpp
  ShaderCompiler.h
  StagingBuffer.cpp
  StagingBuffer.h
  StateTracker.cpp
  StateTracker.h
  VKBoundingBox.cpp
  VKBoundingBox.h
  VKMain.cpp
  VKMemAllocator.cpp
  VKPerfQuery.cpp
  VKPerfQuery.h
  VKPipeline.cpp
  VKPipeline.h
  VKRenderer.cpp
  VKRenderer.h
  VKShader.cpp
  VKShader.h
  VKStreamBuffer.cpp
  VKStreamBuffer.h
  VKSwapChain.cpp
  VKSwapChain.h
  VKTexture.cpp
  VKTexture.h
  VKVertexFormat.cpp
  VKVertexFormat.h
  VKVertexManager.cpp
  VKVertexManager.h
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
  VulkanMemoryAllocator::VulkanMemoryAllocator
  xxhash
)

# Only include the Vulkan headers when building the Vulkan backend
target_include_directories(videovulkan
PRIVATE
  ${CMAKE_SOURCE_DIR}/Externals/Vulkan/Include
)

target_compile_definitions(videovulkan PRIVATE VMA_STATIC_VULKAN_FUNCTIONS=1)

# Silence warnings on glslang by flagging it as a system include
target_include_directories(videovulkan
SYSTEM PRIVATE
  ${CMAKE_SOURCE_DIR}/Externals/glslang/StandAlone
  ${CMAKE_SOURCE_DIR}/Externals/glslang/glslang/Public
  ${CMAKE_SOURCE_DIR}/Externals/glslang/SPIRV
  ${CMAKE_SOURCE_DIR}/Externals/glslang
)
