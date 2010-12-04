set(SRCS	Src/FramebufferManager.cpp
			Src/GLUtil.cpp
			Src/main.cpp
			Src/NativeVertexFormat.cpp
			Src/PixelShaderCache.cpp
			Src/PostProcessing.cpp
			Src/RasterFont.cpp
			Src/Render.cpp
			Src/TextureCache.cpp
			Src/TextureConverter.cpp
			Src/VertexShaderCache.cpp
			Src/VertexManager.cpp)

set(LIBS	videocommon
			GLEW
			SOIL
			common
			${OPENGL_LIBRARIES}
			${X11_LIBRARIES})

if(wxWidgets_FOUND)
	set(SRCS	${SRCS}
				Src/Debugger/Debugger.cpp)
	set(LIBS	videouicommon ${LIBS} ${wxWidgets_LIBRARIES})
endif(wxWidgets_FOUND)

if((${CMAKE_SYSTEM_NAME} MATCHES "Darwin") AND NOT wxWidgets_FOUND)
	set(SRCS	${SRCS} Src/cocoaGL.m)
elseif(WIN32)
	set(SRCS ${SRCS} Src/OS/Win32.cpp)
endif()

if(APPLE OR WIN32 OR ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(LIBS	${LIBS} Cg CgGL)
endif()

if(WIN32 OR ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(LIBS	${LIBS} clrun)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR
	${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	set(LIBS	${LIBS} usbhid)
endif()

add_library(Plugin_VideoOGL MODULE ${SRCS})
target_link_libraries(Plugin_VideoOGL ${LIBS})
install(TARGETS Plugin_VideoOGL
		LIBRARY DESTINATION ${plugindir}
		RUNTIME DESTINATION ${plugindir})
