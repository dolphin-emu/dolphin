cmake_minimum_required(VERSION 3.10)

project(miniupnpc C)
set(MINIUPNPC_VERSION 1.9)
set(MINIUPNPC_API_VERSION 14)

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
dolphin_disable_warnings(miniupnpc)
target_include_directories(miniupnpc PUBLIC src)

if(UNIX)
	target_compile_definitions(miniupnpc PRIVATE MINIUPNPC_SET_SOCKET_TIMEOUT)
	target_compile_definitions(miniupnpc PRIVATE _BSD_SOURCE _POSIX_C_SOURCE=1)
elseif(WIN32)
	target_compile_definitions(miniupnpc PRIVATE -D_WIN32_WINNT=0x0501)
	target_link_libraries(miniupnpc PUBLIC ws2_32 iphlpapi)
endif()

if(APPLE)
	target_compile_definitions(miniupnpc PRIVATE MACOSX _DARWIN_C_SOURCE)
endif()

add_library(Miniupnpc::miniupnpc ALIAS miniupnpc)
