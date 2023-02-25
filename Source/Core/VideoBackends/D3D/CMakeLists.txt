add_library(videod3d
  D3DBase.cpp
  D3DBase.h
  D3DBoundingBox.cpp
  D3DBoundingBox.h
  D3DMain.cpp
  D3DNativeVertexFormat.cpp
  D3DPerfQuery.cpp
  D3DPerfQuery.h
  D3DGfx.cpp
  D3DGfx.h
  D3DState.cpp
  D3DState.h
  D3DSwapChain.cpp
  D3DVertexManager.cpp
  D3DVertexManager.h
  DXPipeline.cpp
  DXPipeline.h
  DXShader.cpp
  DXShader.h
  DXTexture.cpp
  DXTexture.h
  VideoBackend.h
)

target_link_libraries(videod3d
PUBLIC
  common
  videocommon
  videod3dcommon
)

if(MSVC)
  # Add precompiled header
  target_link_libraries(videod3d PRIVATE use_pch)
endif()
