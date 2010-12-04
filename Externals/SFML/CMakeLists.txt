include_directories(include)

set(SRCS	src/SFML/Network/Ftp.cpp
			src/SFML/Network/Http.cpp
			src/SFML/Network/IPAddress.cpp
			src/SFML/Network/Packet.cpp
			src/SFML/Network/SelectorBase.cpp
			src/SFML/Network/SocketTCP.cpp
			src/SFML/Network/SocketUDP.cpp)

if(UNIX)
	set(SRCS ${SRCS} src/SFML/Network/Unix/SocketHelper.cpp)
elseif(WIN32)
	set(SRCS ${SRCS} src/SFML/Network/Win32/SocketHelper.cpp)
endif()

add_library(sfml-network ${SRCS})
