include_directories(include)

set(SRCS	src/glew.c)

add_library(GLEW STATIC ${SRCS})
