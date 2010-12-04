include_directories(include)

set(SRCS	src/glew.c
		src/glewinfo.c
		src/visualinfo.c)

add_library(GLEW STATIC ${SRCS})
