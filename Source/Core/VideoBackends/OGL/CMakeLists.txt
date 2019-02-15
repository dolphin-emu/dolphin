add_library(videoogl
  BoundingBox.cpp
  FramebufferManager.cpp
  main.cpp
  NativeVertexFormat.cpp
  OGLPipeline.cpp
  OGLShader.cpp
  OGLTexture.cpp
  PerfQuery.cpp
  PostProcessing.cpp
  ProgramShaderCache.cpp
  RasterFont.cpp
  Render.cpp
  SamplerCache.cpp
  StreamBuffer.cpp
  TextureCache.cpp
  TextureConverter.cpp
  VertexManager.cpp
)

target_link_libraries(videoogl
PUBLIC
  common
  videocommon

PRIVATE
  ${X11_LIBRARIES}
)
