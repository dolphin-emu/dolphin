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
