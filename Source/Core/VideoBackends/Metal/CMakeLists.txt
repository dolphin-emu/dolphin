message(STATUS "Enabling experimental Metal graphics backend")

set(SRCS
  CommandBufferManager.cpp
  MetalContext.cpp
  MetalFramebuffer.cpp
  MetalPipeline.cpp
  MetalShader.cpp
  MetalTexture.cpp
  MetalVertexFormat.cpp
  Render.cpp
  ShaderCompiler.cpp
  StateTracker.cpp
  StreamBuffer.cpp
  TextureCache.cpp
  Util.cpp
  VertexManager.cpp
  VideoBackend.cpp
  WindowFramebuffer.mm
)

set(LIBS
  mtlpp
  videocommon
  common
)

add_dolphin_library(videometal "${SRCS}" "${LIBS}")
target_link_libraries(videometal PRIVATE glslang spirv-cross)

# We require linking with QuartzCore to access the CoreAnimation Metal Layer.
# TODO: Is there a cmake command for linking with a framework?
target_link_libraries(videometal PRIVATE "-framework QuartzCore")

