set(SRCS
	BoundingBox.cpp
	CommandBufferManager.cpp
	FramebufferManager.cpp
	ObjectCache.cpp
	PaletteTextureConverter.cpp
	PerfQuery.cpp
	RasterFont.cpp
	Renderer.cpp
	ShaderCompiler.cpp
	StateTracker.cpp
	StagingBuffer.cpp
	StagingTexture2D.cpp
	StreamBuffer.cpp
	SwapChain.cpp
	Texture2D.cpp
	TextureCache.cpp
	TextureEncoder.cpp
	Util.cpp
	VertexFormat.cpp
	VertexManager.cpp
    VulkanContext.cpp
	VulkanLoader.cpp
	main.cpp
)

set(LIBS
	videocommon
	common
)

# Flagged as a system include to silence warnings
# TODO: Clean this up and make consistent with rest of build system
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/Externals/Vulkan/Include)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/Externals/glslang/glslang/Public)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/Externals/glslang/SPIRV)

add_dolphin_library(videovulkan "${SRCS}" "${LIBS}")

# TODO: Better integrate this.
if(USE_X11)
	target_link_libraries(videovulkan glslang X11-xcb)
else()
	target_link_libraries(videovulkan glslang)
endif()

