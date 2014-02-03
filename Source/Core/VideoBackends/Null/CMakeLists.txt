set(SRCS
	main.cpp
	Render.cpp
	VertexManager.cpp
	ShaderCache.cpp
)

set(LIBS
	videocommon
	common
	dl
)

add_dolphin_library(videonull "${SRCS}" "${LIBS}")

