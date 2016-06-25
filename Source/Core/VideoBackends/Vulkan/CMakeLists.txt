set(SRCS
	CommandBufferManager.cpp
	FramebufferManager.cpp
	Helpers.cpp
	ObjectCache.cpp
	Renderer.cpp
	ShaderCache.cpp
	StateTracker.cpp
	StaticShaderCache.cpp
	StreamBuffer.cpp
	SwapChain.cpp
	Texture2D.cpp
	TextureCache.cpp
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

add_dolphin_library(videovulkan "${SRCS}" "${LIBS}")

