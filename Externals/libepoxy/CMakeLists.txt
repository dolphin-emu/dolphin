set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
include_directories(include)

set(SRCS src/dispatch_common.c src/gl_generated_dispatch.c)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(SRCS ${SRCS} src/dispatch_egl.c src/egl_generated_dispatch.c)
	if(NOT ANDROID)
		set(SRCS ${SRCS} src/dispatch_glx.c src/glx_generated_dispatch.c)
	endif()
endif()

add_library(epoxy STATIC ${SRCS})
