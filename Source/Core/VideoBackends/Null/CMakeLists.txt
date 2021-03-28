add_library(videonull
  NullBackend.cpp
  NullRender.cpp
  NullRender.h
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
