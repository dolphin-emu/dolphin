add_library(videonull
  NullBackend.cpp
  NullTexture.cpp
  Render.cpp
  VertexManager.cpp
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
