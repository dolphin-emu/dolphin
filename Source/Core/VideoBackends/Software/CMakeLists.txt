set(SRCS BPMemLoader.cpp
	   CPMemLoader.cpp
	   Clipper.cpp
	   DebugUtil.cpp
	   EfbCopy.cpp
	   EfbInterface.cpp
	   OpcodeDecoder.cpp
	   Rasterizer.cpp
	   SWCommandProcessor.cpp
	   SWOGLWindow.cpp
	   SWRenderer.cpp
	   SWStatistics.cpp
	   SWVertexLoader.cpp
	   SWVideoConfig.cpp
	   SWmain.cpp
	   SetupUnit.cpp
	   Tev.cpp
	   TextureEncoder.cpp
	   TextureSampler.cpp
	   TransformUnit.cpp
	   XFMemLoader.cpp)

set(LIBS videocommon
         SOIL
         common
         ${X11_LIBRARIES})

add_dolphin_library(videosoftware "${SRCS}" "${LIBS}")
