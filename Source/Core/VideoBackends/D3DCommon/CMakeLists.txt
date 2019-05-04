add_library(videod3dcommon
  Common.cpp
  Common.h
  Shader.cpp
  Shader.h
  SwapChain.cpp
  SwapChain.h
)

target_link_libraries(videod3dcommon
PUBLIC
  common
  videocommon
)

set_target_properties(videod3dcommon PROPERTIES
  FOLDER "Dolphin Core/Video Backends"
  PROJECT_LABEL "D3DCommon"
)
