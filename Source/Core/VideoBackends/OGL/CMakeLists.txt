add_library(videoogl
  OGLBoundingBox.cpp
  OGLBoundingBox.h
  OGLConfig.cpp
  OGLConfig.h
  OGLGfx.cpp
  OGLGfx.h
  OGLMain.cpp
  OGLNativeVertexFormat.cpp
  OGLPerfQuery.cpp
  OGLPerfQuery.h
  OGLPipeline.cpp
  OGLPipeline.h
  OGLShader.cpp
  OGLShader.h
  OGLStreamBuffer.cpp
  OGLStreamBuffer.h
  OGLTexture.cpp
  OGLTexture.h
  OGLVertexManager.cpp
  OGLVertexManager.h
  ProgramShaderCache.cpp
  ProgramShaderCache.h
  SamplerCache.cpp
  SamplerCache.h
  VideoBackend.h
)

target_link_libraries(videoogl PUBLIC common videocommon)
if(X11_FOUND)
  target_link_libraries(videoogl PRIVATE PkgConfig::X11)
endif()

dolphin_shared_pch(videoogl)
