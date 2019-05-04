add_library(videonull
  NullBackend.cpp
  NullTexture.h
  NullTexture.cpp
  PerfQuery.h
  Render.h
  Render.cpp
  TextureCache.h
  VertexManager.h
  VertexManager.cpp
  VideoBackend.h
)

target_link_libraries(videonull
PUBLIC
  common
  videocommon
)

set_target_properties(videonull PROPERTIES FOLDER "Dolphin Core/Video Backends")

set_target_properties(videonull PROPERTIES
  FOLDER "Dolphin Core/Video Backends"
  PROJECT_LABEL "Null"
)
