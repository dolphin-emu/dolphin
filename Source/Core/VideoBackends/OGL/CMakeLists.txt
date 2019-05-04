add_library(videoogl
  BoundingBox.h
  BoundingBox.cpp
  main.cpp
  NativeVertexFormat.cpp
  OGLPipeline.h
  OGLPipeline.cpp
  OGLShader.h
  OGLShader.cpp
  OGLTexture.h
  OGLTexture.cpp
  PerfQuery.h
  PerfQuery.cpp
  ProgramShaderCache.h
  ProgramShaderCache.cpp
  Render.h
  Render.cpp
  SamplerCache.h
  SamplerCache.cpp
  StreamBuffer.h
  StreamBuffer.cpp
  VertexManager.h
  VertexManager.cpp
  VideoBackend.h
)

target_link_libraries(videoogl
PUBLIC
  common
  videocommon

PRIVATE
  ${X11_LIBRARIES}
)

set_target_properties(videoogl PROPERTIES
  FOLDER "Dolphin Core/Video Backends"
  PROJECT_LABEL "OGL"
)
