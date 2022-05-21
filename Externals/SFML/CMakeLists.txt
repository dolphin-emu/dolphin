include_directories(BEFORE include src)

set(SRC_NETWORK
	src/SFML/Network/Http.cpp
	src/SFML/Network/IPAddress.cpp
	src/SFML/Network/Packet.cpp
	src/SFML/Network/Socket.cpp
	src/SFML/Network/SocketSelector.cpp
	src/SFML/Network/TcpListener.cpp
	src/SFML/Network/TcpSocket.cpp
	src/SFML/Network/UdpSocket.cpp
)

if(WIN32)
	list(APPEND SRC_NETWORK src/SFML/Network/Win32/SocketImpl.cpp)
else()
	list(APPEND SRC_NETWORK src/SFML/Network/Unix/SocketImpl.cpp)
endif()

set(SRC_SYSTEM
	src/SFML/System/Err.cpp
	src/SFML/System/String.cpp
	src/SFML/System/Time.cpp
)

add_library(sfml-network ${SRC_NETWORK})
add_library(sfml-system ${SRC_SYSTEM})
dolphin_disable_warnings_msvc(sfml-network)
dolphin_disable_warnings_msvc(sfml-system)
