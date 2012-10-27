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
	set(LIBS	${LIBS} ${wxWidgets_LIBRARIES})
endif(wxWidgets_FOUND)

if(WIN32 OR ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(LIBS	${LIBS} Cg CgGL)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	list(APPEND LIBS "${CMAKE_SOURCE_DIR}/Externals/Cg/Cg.framework")
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(LIBS	${LIBS} clrun)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR
	${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	set(LIBS	${LIBS} usbhid)
endif()

add_library(videoogl STATIC ${SRCS})
target_link_libraries(videoogl ${LIBS})
