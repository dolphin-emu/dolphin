set(SRCS	Src/VideoConfigDiag.cpp)

add_library(videouicommon STATIC ${SRCS})
if(UNIX)
	add_definitions(-fPIC)
endif(UNIX)
