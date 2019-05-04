add_library(videoogl
  BoundingBox.cpp
  main.cpp
  NativeVertexFormat.cpp
  OGLPipeline.cpp
  OGLShader.cpp
  OGLTexture.cpp
  PerfQuery.cpp
  ProgramShaderCache.cpp
  Render.cpp
  SamplerCache.cpp
  StreamBuffer.cpp
  VertexManager.cpp
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
