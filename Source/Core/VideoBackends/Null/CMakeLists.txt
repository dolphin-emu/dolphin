set(SRCS
  NullBackend.cpp
  NullTexture.cpp
  Render.cpp
  VertexManager.cpp
)

set(LIBS
  videocommon
  common
)

add_dolphin_library(videonull "${SRCS}" "${LIBS}")

