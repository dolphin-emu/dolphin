set(SRCS
  Clipper.cpp
  DebugUtil.cpp
  EfbCopy.cpp
  EfbInterface.cpp
  Rasterizer.cpp
  SWOGLWindow.cpp
  SWRenderer.cpp
  SWVertexLoader.cpp
  SWmain.cpp
  SetupUnit.cpp
  Tev.cpp
  TextureEncoder.cpp
  TextureSampler.cpp
  TransformUnit.cpp
)

set(LIBS
  videocommon
  SOIL
  common
)

if(USE_X11)
 set(LIBS  ${LIBS} ${X11_LIBRARIES})
endif()

add_dolphin_library(videosoftware "${SRCS}" "${LIBS}")
