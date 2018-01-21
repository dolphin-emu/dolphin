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
  VROGL.cpp
)

set(LIBS ${LIBS}
  videocommon
  SOIL
  common
  ${X11_LIBRARIES}
)

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD|NetBSD")
  set(LIBS ${LIBS} usbhid)
endif()

add_dolphin_library(videoogl "${SRCS}" "${LIBS}")
