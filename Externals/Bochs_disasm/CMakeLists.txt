set(SRCS	dis_decode.cpp
			dis_groups.cpp
			resolve.cpp
			syntax.cpp
			PowerPCDisasm.cpp)

if(WIN32)
	set(SRCS ${SRCS} stdafx.cpp)
endif(WIN32)

add_library(bdisasm STATIC ${SRCS})
