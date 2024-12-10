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
  VKPerfQuery.cpp
  VKPerfQuery.h
  VKPipeline.cpp
  VKPipeline.h
  VKGfx.cpp
  VKGfx.h
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
  VKDebug.cpp
  VKDebug.h
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
  xxhash::xxhash
)

if (ANDROID AND _M_ARM_64)
  target_link_libraries(videovulkan
  PRIVATE
      adrenotools
  )
endif()

# Only include the Vulkan headers when building the Vulkan backend
target_include_directories(videovulkan
PRIVATE
  ${CMAKE_SOURCE_DIR}/Externals/Vulkan-Headers/include
  ${CMAKE_SOURCE_DIR}/Externals/VulkanMemoryAllocator/include
  ${CMAKE_SOURCE_DIR}/Externals/libadrenotools/include
)

if(MSVC)
  # Add precompiled header
  target_link_libraries(videovulkan PRIVATE use_pch)
endif()
