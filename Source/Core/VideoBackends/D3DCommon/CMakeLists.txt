add_library(videod3dcommon
  D3DCommon.cpp
  D3DCommon.h
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
