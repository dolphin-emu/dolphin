set(SRCS
	callbacks.c
	compress.c
	host.c
	list.c
	packet.c
	peer.c
	protocol.c)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(SRCS ${SRCS} win32.c)
else()
    set(SRCS ${SRCS} unix.c)
endif()
add_definitions(-Wno-parentheses-equality -DHAS_SOCKLEN_T)
add_library(enet STATIC ${SRCS})
