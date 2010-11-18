set(SRCS	clrun/clrun.c
			clrun/dynamiclib.c
			clrun/gencl.c
			clrun/genclgl.c)

add_library(clrun STATIC ${SRCS})
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-fPIC)
	target_link_libraries(clrun ${CMAKE_DL_LIBS})
endif()
