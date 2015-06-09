project(xxhash C)

set(SRCS
	xxhash.c
)

add_library(xxhash STATIC ${SRCS})
