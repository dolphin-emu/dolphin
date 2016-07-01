set(SRCS
	CommandBufferManager.cpp
	FramebufferManager.cpp
	Helpers.cpp
	ObjectCache.cpp
	PaletteTextureConverter.cpp
	RasterFont.cpp
	Renderer.cpp
	ShaderCache.cpp
	StateTracker.cpp
	StaticShaderCache.cpp
	StreamBuffer.cpp
	SwapChain.cpp
	Texture2D.cpp
	TextureCache.cpp
	TextureEncoder.cpp
	Util.cpp
	VertexFormat.cpp
	VertexManager.cpp
	VulkanImports.cpp
	main.cpp
)

set(LIBS
	videocommon
	common
)

# Flagged as a system include to silence warnings
# TODO: Clean this up and make consistent with rest of build system
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/Externals/Vulkan/Include)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/Externals/glslang)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/Externals/glslang/glslang/Include)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/Externals/glslang/SPIRV)

add_dolphin_library(videovulkan "${SRCS}" "${LIBS}")

target_link_libraries(videovulkan glslang X11-xcb)
