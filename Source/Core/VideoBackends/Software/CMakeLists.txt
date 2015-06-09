set(SRCS BPMemLoader.cpp
	   Clipper.cpp
	   SWCommandProcessor.cpp
	   CPMemLoader.cpp
	   DebugUtil.cpp
	   EfbCopy.cpp
	   EfbInterface.cpp
	   HwRasterizer.cpp
	   SWmain.cpp
	   OpcodeDecoder.cpp
	   RasterFont.cpp
	   Rasterizer.cpp
	   SWRenderer.cpp
	   SetupUnit.cpp
	   SWStatistics.cpp
	   Tev.cpp
	   TextureEncoder.cpp
	   TextureSampler.cpp
	   TransformUnit.cpp
	   SWVertexLoader.cpp
	   SWVideoConfig.cpp
	   XFMemLoader.cpp)

set(LIBS videocommon
         SOIL
         common
         ${X11_LIBRARIES})

add_dolphin_library(videosoftware "${SRCS}" "${LIBS}")
