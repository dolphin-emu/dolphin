set(SRCS
	NullBackend.cpp
	Render.cpp
	VertexManager.cpp
	ShaderCache.cpp
)

set(LIBS
	videocommon
	common
)

add_dolphin_library(videonull "${SRCS}" "${LIBS}")

