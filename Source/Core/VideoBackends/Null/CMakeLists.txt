add_library(videonull
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
  VideoBackendNull.cpp
)

target_link_libraries(videonull
PUBLIC
  common
  videocommon
)

if(MSVC)
  # Add precompiled header
  target_link_libraries(videonull PRIVATE use_pch)
endif()
