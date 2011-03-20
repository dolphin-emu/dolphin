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
			GLEW
			SOIL
			common
			${OPENGL_LIBRARIES}
			${X11_LIBRARIES}
			${wxWidgets_LIBRARIES})

if(NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
	set(LIBS	${LIBS} clrun)
endif()

add_library(videosoftware STATIC ${SRCS})
target_link_libraries(videosoftware ${LIBS})
