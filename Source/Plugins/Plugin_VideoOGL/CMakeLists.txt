set(SRCS	Src/FramebufferManager.cpp
			Src/GLUtil.cpp
			Src/main.cpp
			Src/NativeVertexFormat.cpp
			Src/PerfQuery.cpp
			Src/PixelShaderCache.cpp
			Src/PostProcessing.cpp
			Src/ProgramShaderCache.cpp
			Src/RasterFont.cpp
			Src/Render.cpp
			Src/SamplerCache.cpp
			Src/StreamBuffer.cpp
			Src/TextureCache.cpp
			Src/TextureConverter.cpp
			Src/VertexShaderCache.cpp
			Src/VertexManager.cpp)

set(LIBS	videocommon
			SOIL
			common
			${X11_LIBRARIES})
if(USE_EGL)
	set(LIBS ${LIBS}
		EGL)
endif()

if(USE_GLES3)
	set(LIBS ${LIBS}
		GLESv2)
	set(SRCS ${SRCS} Src/GLFunctions.cpp)
else()
	set(LIBS ${LIBS}
		GLEW
		${OPENGL_LIBRARIES})
endif()

if(wxWidgets_FOUND)
	set(LIBS	${LIBS} ${wxWidgets_LIBRARIES})
endif(wxWidgets_FOUND)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(LIBS	${LIBS} clrun)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR
	${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	set(LIBS	${LIBS} usbhid)
endif()

add_library(videoogl STATIC ${SRCS})
target_link_libraries(videoogl ${LIBS})
