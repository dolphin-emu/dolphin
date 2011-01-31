set(SRCS	Src/BPMemory.cpp
			Src/BPFunctions.cpp
			Src/BPStructs.cpp
			Src/CommandProcessor.cpp
			Src/CPMemory.cpp
			Src/Debugger.cpp
			Src/DLCache.cpp
			Src/Fifo.cpp
			Src/FramebufferManagerBase.cpp
			Src/HiresTextures.cpp
			Src/ImageWrite.cpp
			Src/IndexGenerator.cpp
			Src/MainBase.cpp
			Src/memcpy_amd.cpp
			Src/OnScreenDisplay.cpp
			Src/OpcodeDecoding.cpp
			Src/PixelEngine.cpp
			Src/PixelShaderGen.cpp
			Src/PixelShaderManager.cpp
			Src/RenderBase.cpp
			Src/Statistics.cpp
			Src/TextureCacheBase.cpp
			Src/TextureConversionShader.cpp
			Src/TextureDecoder.cpp
			Src/VertexLoader.cpp
			Src/VertexLoaderManager.cpp
			Src/VertexLoader_Color.cpp
			Src/VertexLoader_Normal.cpp
			Src/VertexLoader_Position.cpp
			Src/VertexLoader_TextCoord.cpp
			Src/VertexManagerBase.cpp
			Src/VertexShaderGen.cpp
			Src/VertexShaderManager.cpp
			Src/VideoConfig.cpp
			Src/VideoState.cpp
			Src/XFBConvert.cpp
			Src/XFMemory.cpp
			Src/XFStructs.cpp
			Src/OpenCL/OCLTextureDecoder.cpp)

if(wxWidgets_FOUND AND WIN32)
	set(SRCS ${SRCS} Src/EmuWindow.cpp)
endif()

if(LIBAV_FOUND OR WIN32)
	set(SRCS ${SRCS} Src/AVIDump.cpp)
endif()

add_library(videocommon STATIC ${SRCS})
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	if(LIBAV_FOUND)
		target_link_libraries(videocommon ${LIBAV_LIBRARIES})
		add_definitions(-D__STDC_CONSTANT_MACROS)
	endif()
endif()
