set(SRCS	Src/BPFunctions.cpp
			Src/FramebufferManager.cpp
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
			Src/VertexManager.cpp
			Src/XFB.cpp)

set(LIBS	videocommon
			GLEW
			SOIL
			common
			Cg
			CgGL)

if(wxWidgets_FOUND)
	set(SRCS	${SRCS}
				Src/GUI/ConfigDlg.cpp
				Src/Debugger/Debugger.cpp)
endif(wxWidgets_FOUND)

if(APPLE AND NOT wxWidgets_FOUND)
	set(SRCS	${SRCS} Src/cocoaGL.m)
elseif(WIN32)
	set(SRCS ${SRCS} Src/OS/Win32.cpp)
elseif(NOT APPLE)
	set(LIBS	${LIBS} clrun)
endif()

add_library(Plugin_VideoOGL SHARED ${SRCS})
target_link_libraries(Plugin_VideoOGL ${LIBS})
install(TARGETS Plugin_VideoOGL
		LIBRARY DESTINATION ${plugindir}
		RUNTIME DESTINATION ${plugindir})
