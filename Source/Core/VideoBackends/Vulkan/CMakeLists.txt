set(SRCS
	VulkanBackend.cpp
	Render.cpp
	VertexManager.cpp
	ShaderCache.cpp
)

set(LIBS
	videocommon
	common
)

add_dolphin_library(videovulkan "${SRCS}" "${LIBS}")

