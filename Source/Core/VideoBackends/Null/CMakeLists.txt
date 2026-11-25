add_library(videonull
  NullBackend.cpp
  NullBoundingBox.h
  NullGfx.cpp
  NullGfx.h
  NullTexture.cpp
  NullTexture.h
  NullVertexManager.cpp
  NullVertexManager.h
  PerfQuery.h
  TextureCache.h
  VideoBackend.h
)

target_link_libraries(videonull
PUBLIC
  common
  videocommon
)

dolphin_shared_pch(videonull)
