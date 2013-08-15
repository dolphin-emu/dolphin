set(SRCS	Src/BPFunctions.cpp
			Src/BPMemory.cpp
			Src/BPStructs.cpp
			Src/CPMemory.cpp
			Src/CommandProcessor.cpp
			Src/Debugger.cpp
			Src/DriverDetails.cpp
			Src/Fifo.cpp
			Src/FPSCounter.cpp
			Src/FramebufferManagerBase.cpp
			Src/HiresTextures.cpp
			Src/ImageWrite.cpp
			Src/IndexGenerator.cpp
			Src/MainBase.cpp
			Src/OnScreenDisplay.cpp
			Src/OpcodeDecoding.cpp
			Src/OpenCL.cpp
			Src/OpenCL/OCLTextureDecoder.cpp
			Src/PerfQueryBase.cpp
			Src/PixelEngine.cpp
			Src/PixelShaderGen.cpp
			Src/PixelShaderManager.cpp
			Src/RenderBase.cpp
			Src/Statistics.cpp
			Src/TextureCacheBase.cpp
			Src/TextureConversionShader.cpp
			Src/VertexLoader.cpp
			Src/VertexLoaderManager.cpp
			Src/VertexLoader_Color.cpp
			Src/VertexLoader_Normal.cpp
			Src/VertexLoader_Position.cpp
			Src/VertexLoader_TextCoord.cpp
			Src/VertexManagerBase.cpp
			Src/VertexShaderGen.cpp
			Src/VertexShaderManager.cpp
			Src/VideoBackendBase.cpp
			Src/VideoConfig.cpp
			Src/VideoState.cpp
			Src/XFMemory.cpp
			Src/XFStructs.cpp
			Src/memcpy_amd.cpp)

set(LIBS core)

if(NOT _M_GENERIC)
	set(SRCS ${SRCS}	Src/x64TextureDecoder.cpp
						Src/x64DLCache.cpp)
else()
	set(SRCS ${SRCS}	Src/GenericTextureDecoder.cpp
						Src/GenericDLCache.cpp)
endif()
if(NOT ${CL} STREQUAL CL-NOTFOUND)
	list(APPEND LIBS ${CL})
endif()

if(wxWidgets_FOUND AND WIN32)
	set(SRCS ${SRCS} Src/EmuWindow.cpp)
endif()

if(LIBAV_FOUND OR WIN32)
	set(SRCS ${SRCS} Src/AVIDump.cpp)
endif()

add_library(videocommon STATIC ${SRCS})
target_link_libraries(videocommon ${LIBS})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	if(LIBAV_FOUND)
		target_link_libraries(videocommon ${LIBS} ${LIBAV_LIBRARIES})
		add_definitions(-D__STDC_CONSTANT_MACROS)
	endif()
endif()
