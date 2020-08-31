cmake_minimum_required(VERSION 2.6)

project(miniupnpc C)
set(MINIUPNPC_VERSION 1.9)
set(MINIUPNPC_API_VERSION 14)

if(UNIX)
	add_definitions(-DMINIUPNPC_SET_SOCKET_TIMEOUT)
	add_definitions(-D_BSD_SOURCE -D_POSIX_C_SOURCE=1)
elseif(WIN32)
	add_definitions(-D_WIN32_WINNT=0x0501)
	find_library(WINSOCK2_LIBRARY NAMES ws2_32 WS2_32 Ws2_32)
	find_library(IPHLPAPI_LIBRARY NAMES iphlpapi)
	set(LDLIBS ${WINSOCK2_LIBRARY} ${IPHLPAPI_LIBRARY} ${LDLIBS})
endif()

if(APPLE)
	add_definitions(-DMACOSX -D_DARWIN_C_SOURCE)
endif()

set(SRCS	src/igd_desc_parse.c
			src/miniupnpc.c
			src/minixml.c
			src/minisoap.c
			src/minissdpc.c
			src/miniwget.c
			src/upnpcommands.c
			src/upnpdev.c
			src/upnpreplyparse.c
			src/upnperrors.c
			src/connecthostport.c
			src/portlistingparse.c
			src/receivedata.c)

add_library(miniupnpc STATIC ${SRCS})
target_include_directories(miniupnpc PUBLIC src)

add_library(Miniupnpc::miniupnpc ALIAS miniupnpc)
