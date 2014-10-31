set(SRCS GLExtensions/GLExtensions.cpp
	   FramebufferManager.cpp
	   GLUtil.cpp
	   main.cpp
	   NativeVertexFormat.cpp
	   PerfQuery.cpp
	   PostProcessing.cpp
	   ProgramShaderCache.cpp
	   RasterFont.cpp
	   Render.cpp
	   SamplerCache.cpp
	   StreamBuffer.cpp
	   TextureCache.cpp
	   TextureConverter.cpp
	   VertexManager.cpp)

set(LIBS	videocommon
			SOIL
			common
			dl
			${X11_LIBRARIES})
if(USE_EGL)
	set(LIBS ${LIBS}
		EGL)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR
	${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	set(LIBS	${LIBS} usbhid)
endif()

add_dolphin_library(videoogl "${SRCS}" "${LIBS}")
