add_library(videosoftware
  Clipper.h
  Clipper.cpp
  DebugUtil.h
  DebugUtil.cpp
  EfbCopy.h
  EfbCopy.cpp
  EfbInterface.h
  EfbInterface.cpp
  NativeVertexFormat.h
  Rasterizer.h
  Rasterizer.cpp
  SWOGLWindow.h
  SWOGLWindow.cpp
  SWRenderer.h
  SWRenderer.cpp
  SWTexture.h
  SWTexture.cpp
  SWVertexLoader.h
  SWVertexLoader.cpp
  SWmain.cpp
  SetupUnit.h
  SetupUnit.cpp
  Tev.h
  Tev.cpp
  TextureEncoder.h
  TextureEncoder.cpp
  TextureSampler.h
  TextureSampler.cpp
  TransformUnit.h
  TransformUnit.cpp
  Vec3.h
  VideoBackend.h
)

target_link_libraries(videosoftware
PUBLIC
  common
  videocommon
)

set_target_properties(videosoftware PROPERTIES
  FOLDER "Dolphin Core/Video Backends"
  PROJECT_LABEL "Software"
)
