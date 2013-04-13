set(SRCS	Src/BPMemLoader.cpp
			Src/Clipper.cpp
			Src/SWCommandProcessor.cpp
			Src/CPMemLoader.cpp
			Src/DebugUtil.cpp
			Src/EfbCopy.cpp
			Src/EfbInterface.cpp
			Src/HwRasterizer.cpp
			Src/SWmain.cpp
			Src/OpcodeDecoder.cpp
			Src/SWPixelEngine.cpp
			Src/Rasterizer.cpp
			Src/SWRenderer.cpp
			Src/SetupUnit.cpp
			Src/SWStatistics.cpp
			Src/Tev.cpp
			Src/TextureEncoder.cpp
			Src/TextureSampler.cpp
			Src/TransformUnit.cpp
			Src/SWVertexLoader.cpp
			Src/SWVideoConfig.cpp
			Src/XFMemLoader.cpp)

if(wxWidgets_FOUND)
	set(SRCS ${SRCS} Src/VideoConfigDialog.cpp)
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
	set(SRCS ${SRCS} ../Plugin_VideoOGL/Src/GLUtil.cpp)
	set(LIBS ${LIBS}
		GLESv2)
else()
	set(SRCS ${SRCS} Src/RasterFont.cpp)
	set(LIBS ${LIBS}
		GLEW
		${OPENGL_LIBRARIES})
endif()
if(NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
	set(LIBS	${LIBS} clrun)
endif()

add_library(videosoftware STATIC ${SRCS})
target_link_libraries(videosoftware ${LIBS})
