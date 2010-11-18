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
			Src/VertexFormatConverter.cpp
			Src/VertexLoader.cpp
			Src/VideoConfig.cpp
			Src/XFMemLoader.cpp)

set(LIBS	videocommon
			GLEW
			SOIL
			common
			${OPENGL_LIBRARIES}
			${X11_LIBRARIES})

if((${CMAKE_SYSTEM_NAME} MATCHES "Darwin") AND NOT wxWidgets_FOUND)
	set(SRCS	${SRCS} Src/cocoaGL.m)
elseif(WIN32)
	set(SRCS ${SRCS} Src/OS/Win32.cpp)
elseif(NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
	set(LIBS	${LIBS} clrun)
endif()

add_library(Plugin_VideoSoftware SHARED ${SRCS})
target_link_libraries(Plugin_VideoSoftware ${LIBS})
install(TARGETS Plugin_VideoSoftware
	LIBRARY DESTINATION ${plugindir}
	RUNTIME DESTINATION ${plugindir})
