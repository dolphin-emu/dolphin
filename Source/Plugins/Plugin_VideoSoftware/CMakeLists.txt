set(SRCS	Src/BPMemLoader.cpp
			Src/Clipper.cpp
			Src/CommandProcessor.cpp
			Src/CPMemLoader.cpp
			Src/DebugUtil.cpp
			Src/EfbCopy.cpp
			Src/EfbInterface.cpp
			Src/GLUtil.cpp
			Src/HwRasterizer.cpp
			Src/main.cpp
			Src/OpcodeDecoder.cpp
			Src/PixelEngine.cpp
			Src/Rasterizer.cpp
			Src/RasterFont.cpp
			Src/Renderer.cpp
			Src/SetupUnit.cpp
			Src/Statistics.cpp
			Src/Tev.cpp
			Src/TextureEncoder.cpp
			Src/TextureSampler.cpp
			Src/TransformUnit.cpp
			Src/VertexLoader.cpp
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

if((${CMAKE_SYSTEM_NAME} MATCHES "Darwin") AND NOT wxWidgets_FOUND)
	set(SRCS	${SRCS} Src/cocoaGL.m)
elseif(WIN32)
	set(SRCS ${SRCS} Src/Win32.cpp)
elseif(NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
	set(LIBS	${LIBS} clrun)
endif()

add_library(videosoftware STATIC ${SRCS})
target_link_libraries(videosoftware ${LIBS})
