set(SRCS	../OGL/GLExtensions/GLExtensions.cpp	
			BPMemLoader.cpp
			Clipper.cpp
			SWCommandProcessor.cpp
			CPMemLoader.cpp
			DebugUtil.cpp
			EfbCopy.cpp
			EfbInterface.cpp
			HwRasterizer.cpp
			SWmain.cpp
			OpcodeDecoder.cpp
			SWPixelEngine.cpp
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

if(wxWidgets_FOUND)
	set(SRCS ${SRCS} VideoConfigDialog.cpp)
endif(wxWidgets_FOUND)

set(LIBS	videocommon
			SOIL
			common
			${X11_LIBRARIES}
			${wxWidgets_LIBRARIES})
if(USE_EGL)
	set(LIBS ${LIBS}
		EGL)
endif()

if(USE_GLES)
	set(SRCS ${SRCS} ../OGL/GLUtil.cpp)
	set(LIBS ${LIBS}
		GLESv2)
else()
	set(SRCS ${SRCS} RasterFont.cpp)
	set(LIBS ${LIBS}
		${OPENGL_LIBRARIES})
endif()

add_dolphin_library(videosoftware "${SRCS}" "${LIBS}")
