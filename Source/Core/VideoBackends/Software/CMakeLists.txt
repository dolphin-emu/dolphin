add_library(videosoftware
  Clipper.cpp
  Clipper.h
  CopyRegion.h
  EfbCopy.cpp
  EfbCopy.h
  NativeVertexFormat.h
  Rasterizer.cpp
  Rasterizer.h
  SetupUnit.cpp
  SetupUnit.h
  SWmain.cpp
  SWBoundingBox.cpp
  SWBoundingBox.h
  SWEfbInterface.cpp
  SWEfbInterface.h
  SWGfx.cpp
  SWGfx.h
  SWOGLWindow.cpp
  SWOGLWindow.h
  SWTexture.cpp
  SWTexture.h
  SWVertexLoader.cpp
  SWVertexLoader.h
  Tev.cpp
  Tev.h
  TextureEncoder.cpp
  TextureEncoder.h
  TextureSampler.cpp
  TextureSampler.h
  TransformUnit.cpp
  TransformUnit.h
  VideoBackend.h
)

target_link_libraries(videosoftware
PUBLIC
  common
  videocommon
)

if(MSVC)
  # Add precompiled header
  target_link_libraries(videosoftware PRIVATE use_pch)
endif()
