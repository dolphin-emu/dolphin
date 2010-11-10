set(SRCS	clrun/clrun.c
			clrun/dynamiclib.c
			clrun/gencl.c
			clrun/genclgl.c)

add_library(clrun STATIC ${SRCS})
if(UNIX)
	add_definitions(-fPIC)
endif(UNIX)
