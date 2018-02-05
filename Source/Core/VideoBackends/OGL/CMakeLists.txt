set(SRCS
  BoundingBox.cpp
  FramebufferManager.cpp
  main.cpp
  NativeVertexFormat.cpp
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

set(LIBS ${LIBS}
  videocommon
  SOIL
  common
  ${X11_LIBRARIES}
)

add_dolphin_library(videoogl "${SRCS}" "${LIBS}")
